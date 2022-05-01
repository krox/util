#pragma once

#include <atomic>
#include <cassert>
#include <condition_variable>
#include <deque>
#include <functional>
#include <future>
#include <mutex>
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
//     * Nothing fancy inside (no look-free structures, no work stealing, etc)
//     * The future returned by ThreadPool::async() does not block on
//       destruction, so it can simply be discarded if the return value is not
//       needed. This is in contrast to std::async().
//     * TODO: cancel_all() should request cooperative stopping from running
//             jobs using some std::stop_token and such.
class ThreadPool
{
	class JobBase
	{
	  public:
		// exactly one of these should be called once
		virtual void run() noexcept = 0;
		virtual void cancel() noexcept = 0;
		virtual ~JobBase(){};
	};

	template <class F, class... Args> class Job : public JobBase
	{
		using result_type = std::invoke_result_t<F, Args...>;

		std::promise<result_type> promise_;
		F f_;
		std::tuple<Args...> args_;

	  public:
		Job(F f, Args... args) : f_(std::move(f)), args_(std::move(args)...) {}

		~Job() final
		{
			// At this point, we assume promise_ to be fulfilled (either by a
			// value or by an exception), otherwise anyone waiting on its
			// future will be stuck indefinitely.

			// assert(promise_.has_value()); // this method doesnt exist :(
		}

		void run() noexcept final
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

		void cancel() noexcept final
		{
			promise_.set_exception(std::make_exception_ptr(job_cancelled{}));
		}

		// call at most once
		auto get_future() noexcept { return promise_.get_future(); }
	};

	std::vector<std::thread> threads_;

	// job queue, usually FIFO
	std::mutex mutex_;
	std::deque<std::unique_ptr<JobBase>> queue_;

	// terminate workers after draining queue
	std::atomic<bool> terminate_{false};

	// idle threads are waiting on this condition variable
	std::condition_variable condition_;

	void loop_function()
	{
		while (true)
		{
			auto lock = std::unique_lock(mutex_);
			condition_.wait(lock,
			                [this]() { return !queue_.empty() || terminate_; });
			if (queue_.empty())
			{
				assert(terminate_);
				return;
			}
			auto job = std::move(queue_.front());
			queue_.pop_front();

			lock.unlock();
			job->run();
		}
	}

  public:
	ThreadPool() : ThreadPool(std::thread::hardware_concurrency()) {}

	explicit ThreadPool(int n)
	{
		threads_.reserve(n);
		for (int i = 0; i < n; ++i)
			threads_.push_back(std::thread(&ThreadPool::loop_function, this));
	}

	// not movable because the workers have to keep a reference to the pool
	ThreadPool(ThreadPool const &) = delete;
	ThreadPool(ThreadPool &&) = delete;
	ThreadPool &operator=(ThreadPool const &) = delete;
	ThreadPool &operator=(ThreadPool &&) = delete;

	~ThreadPool()
	{
		terminate_ = true;
		cancel_pending();
		condition_.notify_all();

		// with C++20 and std::jthread, this would not be necessary
		for (auto &t : threads_)
			if (t.joinable())
				t.join();
	}

	int num_threads() const { return (int)threads_.size(); }

	// cancel all pending jobs (already running jobs are left running)
	void cancel_pending()
	{
		auto lock = std::unique_lock(mutex_);
		auto q = std::move(queue_);
		lock.unlock();
		for (auto &job : q)
			job->cancel();
	}

	// Wait for all pending jobs to finish and joins the worker threads.
	// Adding jobs after calling join() is undefined.
	void join()
	{
		terminate_ = true;
		condition_.notify_all();
		for (auto &t : threads_)
			if (t.joinable())
				t.join();
	}

	// asynchronously call a function (or anything invoke'able)
	//     * Both f and args must be movable, but no copy is required
	//     * Arguments are captured by value (just like std::async). Use
	//       std::ref/cref for references, but beware of escaping dangling
	//       references, especially because
	//     * The returned future does not block on destruction (this is
	//       different from std::async). The ThreadPool destructor does join
	//       all workers (cancelling or finishing all pending work), so this
	//       is the longest that captured references might live.
	//     * If async itself throws, the job will is not enqueued (Though
	//       currently, some problems do std::terminate() instead)
	template <class F, class... Args> auto async(F &&f, Args &&...args)
	{
		using JobType = Job<std::decay_t<F>, std::decay_t<Args>...>;

		auto job = std::make_unique<JobType>(std::forward<F>(f),
		                                     std::forward<Args>(args)...);
		auto future = job->get_future();

		// Exceptions in here would need careful cleanup. we dont bother...
		[&]() noexcept {
			auto lock = std::scoped_lock(mutex_);
			queue_.push_back(std::move(job));
		}();

		condition_.notify_one(); // noexcept

		return future;
	}

	// parallely call f on each element in [first, last)
	//     * elements are captured by reference, thus allowing inplace
	//       modifications. This is safe because for_each() waits for everything
	//       to finish
	//     * If any invocation of f throws an exception, one of them is rethrown
	//       and any additional ones are lost.
	template <class It, class F> void for_each(It first, It last, F f)
	{
		// NOTE: because we capture the elements by reference, we need to be
		//       carful to .wait() on all futures before leaving this function

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
};

} // namespace util
