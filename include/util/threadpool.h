#pragma once

#include "util/atomic.h"
#include "util/synchronized.h"
#include <atomic>
#include <cassert>
#include <exception>
#include <functional>
#include <future>
#include <limits>
#include <mutex>
#include <optional>
#include <ranges>
#include <stdexcept>
#include <stop_token>
#include <thread>
#include <tuple>
#include <type_traits>
#include <variant>
#include <vector>

namespace util {

// exception 'thrown' (stored in the TaskState/Task), if a job is cancelled
// before it actually started running
class job_cancelled : public std::runtime_error
{
  public:
	job_cancelled() : std::runtime_error("job cancelled") {}
};

// Can hold either a value of type T or an exception.
//   * single-producer: calling 'set_*' multiple times is UB
//   * multi-consumer: any number of threads can call 'ready()', 'wait()', and
//     'get()' concurrently. Though no further synchronization on the contained
//     'T' is provided.
//   * typical use: a shared_ptr<TaskState<T>> can be used as a single-slot
//     channel, similar to a promise/future pair.
template <class T> class TaskState
{
  public:
	// 'void' is not quite a type in C++, so we use 'std::monostate' as a
	// placeholder.
	using value_type = std::conditional_t<std::is_void_v<T>, std::monostate, T>;

  private:
	std::atomic<bool> ready_{false};

	// note: 'exception_ptr' has a natural null state, T might not even have a
	// default constructor. So this is the right order of members.
	std::variant<std::exception_ptr, value_type> value_;

  public:
	bool ready() const noexcept
	{
		return ready_.load(std::memory_order_acquire);
	}

	// block till the task is completed)
	void wait() const noexcept
	{
		ready_.wait(false, std::memory_order_acquire);
	}

	// blocks till ready. Returns value or rethrows exception.
	value_type &get()
	{
		wait();

		if (std::holds_alternative<std::exception_ptr>(value_))
			std::rethrow_exception(std::get<std::exception_ptr>(value_));
		assert(std::holds_alternative<value_type>(value_));
		return std::get<value_type>(value_);
	}

	value_type const &get() const
	{
		wait();

		if (std::holds_alternative<std::exception_ptr>(value_))
			std::rethrow_exception(std::get<std::exception_ptr>(value_));
		assert(std::holds_alternative<value_type>(value_));
		return std::get<value_type>(value_);
	}

	// set a value
	void set_value(value_type value = {})
	{
		value_.template emplace<value_type>(std::move(value));
		ready_.store(true, std::memory_order_release);
		ready_.notify_all();
	}

	// set an exception
	void set_exception(std::exception_ptr exception)
	{
		value_.template emplace<std::exception_ptr>(std::move(exception));
		ready_.store(true, std::memory_order_release);
		ready_.notify_all();
	}
};

template <class T> class Task
{
	std::shared_ptr<TaskState<T>> state_;

  public:
	using value_type = typename TaskState<T>::value_type;

	Task() = default;

	explicit Task(std::shared_ptr<TaskState<T>> state)
	    : state_(std::move(state))
	{}

	Task(const Task &) = delete;
	Task &operator=(const Task &) = delete;

	Task(Task &&) noexcept = default;
	Task &operator=(Task &&) noexcept = default;

	bool valid() const noexcept { return static_cast<bool>(state_); }

	bool ready() const noexcept
	{
		assert(state_);
		return state_->ready();
	}

	void wait() const noexcept
	{
		assert(state_);
		state_->wait();
	}

	value_type &get()
	{
		assert(state_);
		return state_->get();
	}
	value_type const &get() const
	{
		assert(state_);
		return state_->get();
	}
};

// base class to put into the job queue
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

	std::shared_ptr<TaskState<result_type>> state_ =
	    std::make_shared<TaskState<result_type>>(); // never null
	F f_;
	std::tuple<Args...> args_;

  public:
	// no copy/move. Users should only deal with 'unique_ptr<Job>' or
	// similar
	Job(Job const &) = delete;
	Job &operator=(Job const &) = delete;
	Job(Job &&) noexcept = delete;
	Job &operator=(Job &&) noexcept = delete;

	Job(F f, Args... args) noexcept
	    : f_(std::move(f)), args_(std::move(args)...)
	{}

	~Job()
	{
		// sanity check: if the job is destroyed without being completed,
		// someone might still be waiting on its result, which would be UB
		assert(state_->ready());
	}

	void run() noexcept
	{
		try
		{
			if constexpr (std::is_same_v<result_type, void>)
			{
				std::apply(std::move(f_), std::move(args_));
				state_->set_value();
			}
			else
			{
				state_->set_value(std::apply(std::move(f_), std::move(args_)));
			}
		}
		catch (...)
		{
			state_->set_exception(std::current_exception());
		}
	}

	void cancel() noexcept
	{
		state_->set_exception(std::make_exception_ptr(job_cancelled{}));
	}

	auto get_task() noexcept { return Task<result_type>{state_}; }
};

// simple thread pool with central queue of tasks
// - Destructor joins all workers (similar behaviour to std::jthread). No more
//   new work is accepted when the destructor starts, but all pending work is
//   finished as usual before the destructor returns.
//   - Nitpick: It would be even better to still allow new work submitted from
//     within running jobs, while ThreadPool's destructor is already running.
// - The handle returned by ThreadPool::async() does not block on destruction.
//   It can be freely discarded for "fire-and-forget" tasks. This is in contrast
//   to std::async().
// - Submitting jobs is thread-safe, including from within a running job.
class ThreadPool
{
	std::vector<std::jthread> threads_; // worker threads
	synchronized_queue<JobBase> queue_; // pending jobs

  public:
	ThreadPool() : ThreadPool(std::thread::hardware_concurrency()) {}

	explicit ThreadPool(int n)
	{
		threads_.reserve(n);
		for (int i = 0; i < n; ++i)
			threads_.emplace_back([&q = queue_] {
				while (true)
				{
					if (auto job = q.pop())
						job->run();
					else
						break;
				}
			});
	}

	// not movable: workers keep a reference to the pool/queue
	ThreadPool(ThreadPool const &) = delete;
	ThreadPool(ThreadPool &&) = delete;
	ThreadPool &operator=(ThreadPool const &) = delete;
	ThreadPool &operator=(ThreadPool &&) = delete;

	~ThreadPool()
	{
		queue_.close();
		// implicit: join workers after queue is drained
	}

	int num_threads() const noexcept { return (int)threads_.size(); }

	// asynchronously call a function (or anything invoke'able)
	//     * Both f and args must be movable, but no copy is required
	//     * Arguments are captured by value (just like std::async). Use
	//       std::ref/cref for references, but beware of escaping dangling
	//       references, especially because
	//     * The returned handle does not block on destruction (this is
	//       different from std::async). The maximum lifetime of captured
	//       references is determined by the ThreadPool itself, the destructor
	//       of which cancels or waits for all all pending jobs.
	//     * If f throws, the exception is captured and can be retrieved
	//       from the returned future.
	template <class F, class... Args>
	auto async(F f,
	           Args... args) noexcept -> Task<std::invoke_result_t<F, Args...>>
	{
		auto typed_job =
		    std::make_unique<Job<F, Args...>>(std::move(f), std::move(args)...);
		auto task = typed_job->get_task();
		std::unique_ptr<JobBase> job = std::move(typed_job);

		job = queue_.push(std::move(job));
		if (job) // 'push' was rejected (e.g. queue closed) -> cancel job
			job->cancel();

		return task;
	}
};

// Run a function in parallel on each worker thread.
// - Each worker thread gets their own copy of the function object, which must
//   thus be copyable.
// - The function object is invoked with (thread_id, num_threads).
// - If 'f' returns non-void, the results are collected and returned as a
//   vector (Ordered by participant-ID)
// - If any instance of 'f' throws, the exception is propagated. If multiple
//   workers throw, all but one exception are discarded.
// - No guarantee on actual number of participating workers, but it will be at
//   most 'num_threads()'.
// - future: stop other workers early as soon as one throws.
auto parallel(ThreadPool &pool, std::invocable<int, int> auto f)
{
	using Result = std::invoke_result_t<decltype(f), int, int>;
	int n = pool.num_threads();
	if (n == 0)
		throw std::runtime_error("parallel requires at least one worker");

	std::vector<Task<Result>> tasks;
	tasks.reserve(n);
	for (int i = 0; i < n; ++i)
		tasks.push_back(pool.async(f, i, n));

	// wait for all work to finish before collecting results. This ensures
	// no references escape in case of an exception.
	for (auto &t : tasks)
		t.wait();

	if constexpr (std::is_same_v<Result, void>)
	{
		for (auto &t : tasks)
			t.get();
	}
	else
	{
		std::vector<Result> results;
		results.reserve(tasks.size());
		for (auto &t : tasks)
			results.push_back(t.get());
		return results;
	}
}

struct bulk_options
{
	size_t chunk_size = 1;
	//	size_t max_participants = infinity; // not implemented
};

// parallel loop: execute 'f(x)' for each element 'x' in the range 'r'
void parallel_for_each(ThreadPool &pool,
                       std::ranges::random_access_range auto &&r, auto f,
                       bulk_options options)
{
	auto first = std::ranges::begin(r);
	size_t count = static_cast<size_t>(std::ranges::distance(r));
	if (count == 0)
		return;
	assert(options.chunk_size >= 1);

	relaxed_atomic<size_t> next{0};
	parallel(pool, [&](int, int) {
		while (true)
		{
			size_t begin = next.fetch_add(options.chunk_size);
			if (begin >= count)
				break;
			size_t end = std::min(begin + options.chunk_size, count);
			for (size_t i = begin; i < end; ++i)
				std::invoke(f, *(first + i));
		}
	});
}

// parallel loop with worker-local scratch state: execute 'f(state, x)' for
// each element 'x' in the range 'r'
void parallel_for_each(ThreadPool &pool,
                       std::ranges::random_access_range auto &&r,
                       auto make_state, auto f, bulk_options options)
{
	using State =
	    std::remove_cvref_t<std::invoke_result_t<decltype(make_state) &>>;

	auto first = std::ranges::begin(r);
	size_t count = static_cast<size_t>(std::ranges::distance(r));
	if (count == 0)
		return;
	assert(options.chunk_size >= 1);

	relaxed_atomic<size_t> next{0};

	parallel(pool, [&](int, int) {
		// early-out for late-arriving workers: If no work is left, dont create
		// any state.
		if (next.load() >= count)
			return;

		State state = std::invoke(make_state);
		while (true)
		{
			size_t begin = next.fetch_add(options.chunk_size);
			if (begin >= count)
				break;
			size_t end = std::min(begin + options.chunk_size, count);
			for (size_t i = begin; i < end; ++i)
				std::invoke(f, state, *(first + i));
		}
	});
}

// parallel filter: collect all elements 'x' for which 'f(x)' returns true.
// Order of the returned elements is unspecified.
auto parallel_filter_unordered(ThreadPool &pool,
                               std::ranges::random_access_range auto &&r,
                               auto f, bulk_options options)
{
	using T = std::ranges::range_value_t<decltype(r)>;
	auto first = std::ranges::begin(r);
	size_t count = static_cast<size_t>(std::ranges::distance(r));
	if (count == 0)
		return std::vector<T>{};
	assert(options.chunk_size >= 1);

	relaxed_atomic<size_t> next{0};

	auto result = parallel(pool, [&](int, int) {
		std::vector<T> output;

		while (true)
		{
			size_t begin = next.fetch_add(options.chunk_size);
			if (begin >= count)
				break;
			size_t end = std::min(begin + options.chunk_size, count);
			for (size_t i = begin; i < end; ++i)
				if (std::invoke(f, *(first + i)))
					output.push_back(*(first + i));
		}

		return output;
	});

	std::vector<T> merged;
	for (auto &part : result)
		merged.insert(merged.end(), std::make_move_iterator(part.begin()),
		              std::make_move_iterator(part.end()));
	return merged;
}

// parallel filter with worker-local scratch state: collect all elements 'x'
// for which 'f(state, x)' returns true. Order of the returned elements is
// unspecified.
auto parallel_filter_unordered(ThreadPool &pool,
                               std::ranges::random_access_range auto &&r,
                               auto make_state, auto f, bulk_options options)
{
	using T = std::ranges::range_value_t<decltype(r)>;
	using State =
	    std::remove_cvref_t<std::invoke_result_t<decltype(make_state) &>>;
	auto first = std::ranges::begin(r);
	size_t count = static_cast<size_t>(std::ranges::distance(r));
	if (count == 0)
		return std::vector<T>{};

	assert(options.chunk_size >= 1);

	relaxed_atomic<size_t> next{0};

	auto result = parallel(pool, [&](int, int) {
		std::vector<T> output;

		if (next.load() >= count)
			return output;

		State state = std::invoke(make_state);
		while (true)
		{
			size_t begin = next.fetch_add(options.chunk_size);
			if (begin >= count)
				break;
			size_t end = std::min(begin + options.chunk_size, count);
			for (size_t i = begin; i < end; ++i)
				if (std::invoke(f, state, *(first + i)))
					output.push_back(*(first + i));
		}

		return output;
	});

	std::vector<T> merged;
	for (auto &part : result)
		merged.insert(merged.end(), std::make_move_iterator(part.begin()),
		              std::make_move_iterator(part.end()));
	return merged;
}

} // namespace util
