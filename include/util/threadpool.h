#pragma once

#include "util/synchronized.h"
#include <atomic>
#include <cassert>
#include <functional>
#include <future>
#include <optional>
#include <ranges>
#include <thread>

namespace util {

// exception 'thrown' (stored in the std::future), if a job is cancelled before
// it actually started running
class job_cancelled : public std::runtime_error
{
  public:
	job_cancelled() : std::runtime_error("job cancelled") {}
};

// simple thread pool with central queue of tasks
//     * Destructor joins all workers (similar behaviour to std::jthread).
//       Pending jobs are cancelled and already running ones are waited for.
//     * Cancelled jobs recieve a 'job_cancelled' exception in their associated
//       promise/future
//     * The future returned by ThreadPool::async() does not block on
//       destruction, so it can simply be discarded if the return value is not
//       needed. This is in contrast to std::async().
//     * Nothing fancy inside (no look-free structures, no work stealing, etc)
//     * Submitting jobs is thread-safe, including from within a running job.
//     * TODO: some co-operative stoping, maybe using std::stop_token
class ThreadPool
{
	class JobBase
	{
	  public:
		// exactly one of run() or cancel() should be called exactly once
		virtual void run() noexcept = 0;
		virtual void cancel() noexcept = 0;
		virtual ~JobBase(){};
	};

	template <class F, class... Args> class Job final : public JobBase
	{
		using result_type = std::invoke_result_t<F, Args...>;

		std::promise<result_type> promise_;
		F f_;
		std::tuple<Args...> args_;

	  public:
		Job(Job const &) = delete;
		Job &operator=(Job const &) = delete;

		Job(F f, Args... args) noexcept
		    : f_(std::move(f)), args_(std::move(args)...)
		{}

		~Job()
		{
			// At this point, we assume promise_ to be fulfilled (either by a
			// value or by an exception). Otherwise, anyone waiting on its
			// future will be stuck indefinitely.

			// assert(promise_.has_value()); // this method doesnt exist :(
		}

		void run() noexcept
		{
			try
			{
				if constexpr (std::is_same_v<result_type, void>)
				{
					std::apply(std::move(f_), std::move(args_));
					promise_.set_value();
				}
				else
					promise_.set_value(
					    std::apply(std::move(f_), std::move(args_)));
			}
			catch (...)
			{
				// NOTE: 'promise_.set_value()' cannot throw in our usecase,
				//       so we know any exception came from invoking f_ itself
				promise_.set_exception(std::current_exception());
			}
		}

		void cancel() noexcept
		{
			promise_.set_exception(std::make_exception_ptr(job_cancelled{}));
		}

		// call at most once
		auto get_future() noexcept { return promise_.get_future(); }
	};

	// worker threads
	std::vector<std::jthread> threads_;

	// job queue, usually FIFO
	synchronized_queue<std::unique_ptr<JobBase>> queue_;

	// controls behaviour of the worker threads
	std::atomic<bool> terminate_{false};

	// "main function" of the worker threads
	void loop_function() noexcept
	{
		auto should_terminate = [this]() -> bool { return terminate_; };
		while (true)
		{
			auto job = queue_.pop(should_terminate);
			if (job)
				(*job)->run();
			else
				break;
		}
	}

  public:
	ThreadPool() : ThreadPool(std::thread::hardware_concurrency()) {}

	explicit ThreadPool(int n)
	{
		threads_.reserve(n);
		for (int i = 0; i < n; ++i)
			threads_.push_back(std::jthread(&ThreadPool::loop_function, this));
	}

	// not movable because the workers have to keep a reference to the pool
	ThreadPool(ThreadPool const &) = delete;
	ThreadPool(ThreadPool &&) = delete;
	ThreadPool &operator=(ThreadPool const &) = delete;
	ThreadPool &operator=(ThreadPool &&) = delete;

	~ThreadPool()
	{
		terminate_ = true;
		auto q = queue_.pop_all();
		queue_.notify();
		for (auto &job : q)
			job->cancel();
	}

	int num_threads() const noexcept { return (int)threads_.size(); }

	void add_job(std::unique_ptr<JobBase> job) noexcept
	{
		if (terminate_)
			job->cancel();
		else
			queue_.push(std::move(job));
	}

	// asynchronously call a function (or anything invoke'able)
	//     * Both f and args must be movable, but no copy is required
	//     * Arguments are captured by value (just like std::async). Use
	//       std::ref/cref for references, but beware of escaping dangling
	//       references, especially because
	//     * The returned future does not block on destruction (this is
	//       different from std::async). The maximum lifetime of captured
	//       references is determined by the ThreadPool itself, the destructor
	//       of which cancels or waits for all all pending jobs.
	//     * If f throws, the exception is captured and can be retrieved
	//       from the returned future.
	template <class F, class... Args> auto async(F f, Args... args) noexcept
	{
		auto job =
		    std::make_unique<Job<F, Args...>>(std::move(f), std::move(args)...);
		auto future = job->get_future();
		add_job(std::move(job));
		return future;
	}

	// parallely call f on each element in [first, last)
	//     * elements are captured by reference, thus allowing inplace
	//       modifications. This is safe because for_each() waits for everything
	//       to finish
	//     * If any invocation of f throws an exception, one of them is rethrown
	//       and any additional ones are lost.
	//     * TODO: if one invocation of f throws, it would be reasonable to
	//             cancel all pending invocations to get out faster.
	template <std::forward_iterator I, std::sentinel_for<I> S, class F>
	void for_each(I first, S last, F f)
	{
		// NOTE: because we capture the elements by reference, we need to be
		//       careful to .wait() on all futures before leaving this function

		std::vector<decltype(async(f, std::ref(*first)))> as;
		as.reserve(std::distance(first, last));

		// Exceptions in here would need careful cleanup to avoid escaping
		// references. So we just std::terminate instead.
		[&]() noexcept {
			for (; first != last; ++first)
				as.push_back(async(f, std::ref(*first)));
			for (auto &a : as)
				a.wait();
		}();

		for (auto &a : as)
			a.get(); // rethrows any exception from the job
	}

	template <std::ranges::forward_range R, class F> void for_each(R &&r, F f)
	{
		for_each(std::ranges::begin(r), std::ranges::end(r), std::move(f));
	}

	// parallel filter with optional chunking
	//     * order of elements is preserved
	template <std ::ranges::random_access_range R, class F>
	std::vector<std::ranges::range_value_t<R>> filter(R &&r, F f,
	                                                  size_t chunk_size = 1)
	{
		assert(chunk_size >= 1);
		using T = std::ranges::range_value_t<R>;
		using It = std::ranges::iterator_t<R>;

		std::vector<std::future<std::vector<T>>> as;
		std::vector<T> result;
		size_t n = std::ranges::distance(r);
		size_t chunk_count = (n + chunk_size - 1) / chunk_size;
		as.reserve(chunk_count);

		auto fun = [&f](It first, size_t cnt) -> std::vector<T> {
			std::vector<T> r;
			while (cnt-- > 0)
			{
				if (f(*first))
					r.push_back(*first);
				++first;
			}
			return r;
		};

		// didnt implement proper exeption safety / cleanup yet, so just wrap it
		// in noexcept to terminate on any error...
		[&]() noexcept {
			for (size_t i = 0; i < chunk_count; ++i)
				as.push_back(async(fun, std::ranges::begin(r) + chunk_size * i,
				                   std::min(chunk_size, n - chunk_size * i)));
			for (auto &a : as)
			{
				std::vector<T> tmp = a.get();
				result.insert(result.end(), tmp.begin(), tmp.end());
			}
		}();

		return result;
	}
};

} // namespace util
