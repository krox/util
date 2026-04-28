#pragma once

#include "util/synchronized.h"
#include <atomic>
#include <cassert>
#include <exception>
#include <functional>
#include <future>
#include <mutex>
#include <optional>
#include <ranges>
#include <thread>
#include <tuple>
#include <type_traits>
#include <vector>

namespace util {

// little helper class to process a range of elements in parallel. Just a
// non-owning std::span with an atomic index to distribute chunks.
template <class T> class synchronized_iterator
{
	std::span<T> data_;
	std::atomic<size_t> index_;

  public:
	synchronized_iterator(std::span<T> data) : data_(data), index_(0) {}

	// returns the next chunk of data to process. May return smaller chunks at
	// the end, empty span if all data has been processed.
	std::span<T> next_chunk(size_t chunk_size)
	{
		size_t i = index_.fetch_add(chunk_size);
		if (i >= data_.size())
			return {};
		else
			return data_.subspan(i, std::min(chunk_size, data_.size() - i));
	}

	// call this from any number of threads to process the data in parallel
	void for_each(std::invocable<T &> auto f, size_t chunk_size)
	{
		while (true)
			if (auto chunk = next_chunk(chunk_size); !chunk.empty())
				for (T &x : chunk)
					f(x);
			else
				break;
	}
};

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
//     * TODO: proper separation of low-level job queue and high-level parallel
//     algorithms.
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

	// call 'f' once from each thread, returning a vector of the results.
	//   * implementation: this simply pushes 'num_threads()' jobs to the normal
	//     queue. The expectation is that each one will be picked up by a
	//     different worker thread. Though this is not guaranteed in case some
	//     workers are busy with unrelated work or 'f' finishes very quickly. In
	//     any case, 'worker_index' takes each value from 0 to num_threads()-1
	//     across the calls to 'f'.
	template <class F> auto for_each_thread(F f)
	{
		using result_type = std::invoke_result_t<F &, int>;
		using fun_type = std::decay_t<F>;
		static_assert(std::copy_constructible<fun_type>);

		std::vector<std::future<result_type>> as;
		as.reserve(num_threads());

		for (int worker_index = 0; worker_index < num_threads(); ++worker_index)
		{
			auto job = [f, worker_index]() -> result_type {
				return std::invoke(f, worker_index);
			};
			as.push_back(async(std::move(job)));
		}

		if constexpr (std::is_void_v<result_type>)
		{
			for (auto &a : as)
				a.get();
		}
		else
		{
			std::vector<result_type> result;
			result.reserve(as.size());
			for (auto &a : as)
				result.push_back(a.get());
			return result;
		}
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

	// parallel for_each with worker-local scratch state
	//     * make_state() is called at most once per pool worker that actually
	//       participates in the loop
	//     * each worker reuses its private state across many elements
	//     * chunk_size controls the minimum scheduling granularity for claiming
	//       more work
	//     * this variant uses a bounded number of long-lived jobs rather than
	//       one job per element, so scratch creation stays bounded by the
	//       worker count instead of the range size
	template <std::random_access_iterator I, std::sized_sentinel_for<I> S,
	          class MakeState, class F>
	void for_each_with_state(I first, S last, MakeState make_state, F f,
	                         size_t chunk_size = 1)
	{
		using State = std::remove_cvref_t<std::invoke_result_t<MakeState &>>;
		assert(chunk_size >= 1);

		size_t count = static_cast<size_t>(std::distance(first, last));
		if (count == 0)
			return;

		if (num_threads() == 0)
		{
			auto state = make_state();
			for (size_t index = 0; index < count; index += chunk_size)
			{
				size_t end = std::min(index + chunk_size, count);
				for (size_t i = index; i < end; ++i)
					std::invoke(f, state, *(first + i));
			}
			return;
		}

		std::atomic<size_t> next{0};
		std::atomic<bool> stop{false};
		std::exception_ptr exception;
		std::mutex exception_mutex;
		std::vector<std::future<void>> as;
		as.reserve(num_threads());

		auto worker = [&]() {
			std::optional<State> state;

			while (!stop.load(std::memory_order_relaxed))
			{
				size_t index =
				    next.fetch_add(chunk_size, std::memory_order_relaxed);
				if (index >= count)
					break;
				size_t end = std::min(index + chunk_size, count);

				try
				{
					if (!state)
						state.emplace(make_state());
					for (size_t i = index; i < end; ++i)
						std::invoke(f, *state, *(first + i));
				}
				catch (...)
				{
					stop.store(true, std::memory_order_relaxed);
					std::lock_guard lock(exception_mutex);
					if (!exception)
						exception = std::current_exception();
					break;
				}
			}
		};

		for (int i = 0; i < num_threads(); ++i)
			as.push_back(async(worker));
		for (auto &a : as)
			a.wait();

		for (auto &a : as)
			a.get();

		if (exception)
			std::rethrow_exception(exception);
	}

	template <std::ranges::random_access_range R, class MakeState, class F>
	void for_each_with_state(R &&r, MakeState make_state, F f,
	                         size_t chunk_size = 1)
	{
		for_each_with_state(std::ranges::begin(r), std::ranges::end(r),
		                    std::move(make_state), std::move(f), chunk_size);
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
