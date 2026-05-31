#pragma once

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
#include <thread>
#include <tuple>
#include <type_traits>
#include <variant>
#include <vector>

namespace util {

// exception 'thrown' (stored in the SharedResult/Task), if a job is cancelled
// before it actually started running
class job_cancelled : public std::runtime_error
{
  public:
	job_cancelled() : std::runtime_error("job cancelled") {}
};

// type-erased base class for SharedResult
class SharedResultBase
{
	std::atomic<bool> ready_{false};

  public:
	SharedResultBase() = default;

	// not movable. Doing so would mess with the synchronization.
	SharedResultBase(SharedResultBase const &) = delete;
	SharedResultBase &operator=(SharedResultBase const &) = delete;
	SharedResultBase(SharedResultBase &&) = delete;
	SharedResultBase &operator=(SharedResultBase &&) = delete;

	// check if a value/exception is ready without blocking.
	bool ready() const noexcept
	{
		return ready_.load(std::memory_order_acquire);
	}

	// blocks until value/exception is set
	void wait() const noexcept
	{
		// note: 'atomic::wait' guarantees to only return when the value
		// actually changed away from the expected. No spurious wakeups.
		ready_.wait(false, std::memory_order_acquire);
	}

  protected:
	void set_ready() noexcept
	{
#ifdef NDEBUG
		ready_.store(true, std::memory_order_release);
#else
		bool old = ready_.exchange(true, std::memory_order_release);

		// note: this check does catch race conditions, but this is too late.
		// Competing calls to 'set_value(...)' or the like can already have
		// corrupted memory.
		assert(old == false);
#endif
		ready_.notify_all();
	}
};

// Can hold either a value of type T or an exception.
//   * single-producer: calling 'set_*' multiple times is UB
//   * multi-consumer: any number of threads can call 'ready()', 'wait()', and
//     'get()' concurrently. Though no further synchronization on the contained
//     'T' is provided.
//   * typical use: a shared_ptr<SharedResult<T>> can be used as a single-slot
//     channel, similar to a promise/future pair.
template <class T> class SharedResult;

template <class T>
    requires std::movable<T> && std::is_nothrow_move_constructible_v<T>
class SharedResult<T> : public SharedResultBase
{
	// note: 'exception_ptr' has a natural null state, T might not even have a
	// default constructor. So this is the right order of members.
	std::variant<std::exception_ptr, T> value_;

  public:
	// blocks till ready. Returns value or rethrows exception.
	T &get()
	{
		wait();

		if (std::holds_alternative<std::exception_ptr>(value_))
			std::rethrow_exception(std::get<std::exception_ptr>(value_));
		assert(std::holds_alternative<T>(value_));
		return std::get<T>(value_);
	}

	T const &get() const
	{
		wait();

		if (std::holds_alternative<std::exception_ptr>(value_))
			std::rethrow_exception(std::get<std::exception_ptr>(value_));
		assert(std::holds_alternative<T>(value_));
		return std::get<T>(value_);
	}

	// set a value
	template <class U = T>
	    requires std::constructible_from<T, U>
	void set_value(U &&value)
	{
		value_.template emplace<T>(std::forward<U>(value));
		set_ready();
	}

	// set an exception
	void set_exception(std::exception_ptr exception)
	{
		value_.template emplace<std::exception_ptr>(std::move(exception));
		set_ready();
	}
};

// void specialization of SharedResult.
template <> class SharedResult<void> : public SharedResultBase
{
	std::exception_ptr exception_ = nullptr;

  public:
	// blocks till ready. rethrows any exception.
	void get() const
	{
		wait();
		if (exception_)
			std::rethrow_exception(exception_);
	}

	// set a value
	void set_value() { set_ready(); }

	// set an exception
	void set_exception(std::exception_ptr exception)
	{
		exception_ = exception;
		set_ready();
	}
};

template <class T> class Task
{
	std::shared_ptr<SharedResult<T>> state_;

  public:
	Task() = default;

	explicit Task(std::shared_ptr<SharedResult<T>> state)
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

	T get()
	{
		assert(state_);
		if constexpr (std::is_same_v<T, void>)
		{
			state_->get();
			state_.reset();
			return;
		}
		else
		{
			T result = std::move(state_->get());
			state_.reset();
			return result;
		}
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

	std::shared_ptr<SharedResult<result_type>> promise_ =
	    std::make_shared<SharedResult<result_type>>();
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

		assert(promise_->ready());
	}

	void run() noexcept
	{
		try
		{
			if constexpr (std::is_same_v<result_type, void>)
			{
				std::apply(std::move(f_), std::move(args_));
				promise_->set_value();
			}
			else
				promise_->set_value(
				    std::apply(std::move(f_), std::move(args_)));
		}
		catch (...)
		{
			// NOTE: 'promise_.set_value()' cannot throw in our usecase,
			//       so we know any exception came from invoking f_ itself
			promise_->set_exception(std::current_exception());
		}
	}

	void cancel() noexcept
	{
		promise_->set_exception(std::make_exception_ptr(job_cancelled{}));
	}

	// call at most once
	auto get_future() noexcept { return Task<result_type>{promise_}; }
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
	template <class F, class... Args>
	auto async(F f,
	           Args... args) noexcept -> Task<std::invoke_result_t<F, Args...>>
	{
		auto job =
		    std::make_unique<Job<F, Args...>>(std::move(f), std::move(args)...);
		auto future = job->get_future();
		add_job(std::move(job));
		return future;
	}
};

struct bulk_options
{
	size_t chunk_size = 1;
	size_t max_participants = std::numeric_limits<size_t>::max();
};

// flexible backend for parallel algorithms
template <class MakeState, class RunChunk, class FinishState>
auto bulk_execute(ThreadPool &pool, size_t count, MakeState make_state,
                  RunChunk run_chunk, FinishState finish_state,
                  bulk_options options = {})
{
	using State = std::remove_cvref_t<std::invoke_result_t<MakeState &>>;
	using Result =
	    std::remove_cvref_t<std::invoke_result_t<FinishState &, State>>;

	assert(options.chunk_size >= 1);
	assert(options.max_participants >= 1);

	if (count == 0)
		return std::vector<Result>{};

	// empty pool -> run everything in the current thread
	if (pool.num_threads() == 0)
	{
		State state = std::invoke(make_state);
		for (size_t begin = 0; begin < count; begin += options.chunk_size)
		{
			size_t end = std::min(begin + options.chunk_size, count);
			std::invoke(run_chunk, state, begin, end);
		}

		std::vector<Result> results;
		results.reserve(1);
		results.push_back(std::invoke(finish_state, std::move(state)));
		return results;
	}

	// number of participants is limited by (1) available threads,
	// (2) available chunks, and (3) user-provided 'max_participants'
	size_t chunk_count = (count + options.chunk_size - 1) / options.chunk_size;
	size_t participants = std::min(chunk_count, options.max_participants);
	participants = std::min(participants, (size_t)pool.num_threads());

	// These atomics are used to distribute work between participants and signal
	// early stopping in case of an exception.
	std::atomic<size_t> next{0};
	std::atomic<bool> stop{false};

	auto worker = [&]() -> std::optional<Result> {
		// check if any work is left before even starting. If there is nothing
		// to do (e.g. if this worker started late), we skip the potentially
		// expensive state creation. This is a best-effort optimization, no
		// particular guaranee is made.
		if (stop.load(std::memory_order_relaxed))
			return std::nullopt;
		if (next.load(std::memory_order_relaxed) >= count)
			return std::nullopt;

		try
		{
			State state = std::invoke(make_state);
			while (!stop.load(std::memory_order_relaxed))
			{
				size_t begin = next.fetch_add(options.chunk_size,
				                              std::memory_order_relaxed);
				if (begin >= count)
					break;
				size_t end = std::min(begin + options.chunk_size, count);

				std::invoke(run_chunk, state, begin, end);
			}

			return std::optional<Result>(
			    std::in_place, std::invoke(finish_state, std::move(state)));
		}
		catch (...)
		{
			stop.store(true, std::memory_order_relaxed);
			throw;
		}
	};

	// enqueue one job per participant. Ideally, each one would be picked up by
	// a different worker thread, but this is not strictly guaranteed.
	std::vector<Task<std::optional<Result>>> as;
	as.reserve(participants);

	// note: the workers contain references local variables, so an exception in
	// here would be bad. Dont think its actually possible, but just to be safe
	// we wrap it in 'noexcept'.
	[&]() noexcept {
		for (size_t i = 0; i < participants; ++i)
			as.push_back(pool.async(worker));
		for (auto &a : as)
			a.wait();
	}();

	// collect results. (the '.get()' might rethrow an exception from the
	// worker)
	std::vector<Result> results;
	results.reserve(participants);
	for (auto &a : as)
		if (auto result = a.get(); result)
			results.push_back(std::move(*result));
	return results;
}

// parallel loop: execute 'f(x)' for each element 'x' in the range 'r'
void for_each(ThreadPool &pool, std::ranges::random_access_range auto &&r,
              auto f, bulk_options options)
{
	using State = std::tuple<>;
	auto make_state = []() { return State{}; };
	auto finalize_state = [](State &&) { return State{}; };

	auto first = std::ranges::begin(r);
	size_t count = static_cast<size_t>(std::ranges::distance(r));

	auto run_chunk = [&f, first](State &, size_t begin, size_t end) {
		for (size_t i = begin; i < end; ++i)
			std::invoke(f, *(first + i));
	};

	bulk_execute(pool, count, make_state, run_chunk, finalize_state, options);
}

// parallel loop with worker-local scratch state: execute 'f(state, x)' for
// each element 'x' in the range 'r'
void for_each(ThreadPool &pool, std::ranges::random_access_range auto &&r,
              auto make_state, auto f, bulk_options options)
{
	using State =
	    std::remove_cvref_t<std::invoke_result_t<decltype(make_state) &>>;
	auto finalize_state = [](State &&) { return std::tuple<>{}; };

	auto first = std::ranges::begin(r);
	size_t count = static_cast<size_t>(std::ranges::distance(r));

	auto run_chunk = [&f, first](State &state, size_t begin, size_t end) {
		for (size_t i = begin; i < end; ++i)
			std::invoke(f, state, *(first + i));
	};

	bulk_execute(pool, count, make_state, run_chunk, finalize_state, options);
}

// parallel filter: collect all elements 'x' for which 'f(x)' returns true.
// Order of the returned elements is unspecified.
auto filter_unordered(ThreadPool &pool,
                      std::ranges::random_access_range auto &&r, auto f,
                      bulk_options options)
{
	using T = std::ranges::range_value_t<decltype(r)>;
	using State = std::vector<T>;
	auto make_state = []() { return State{}; };
	auto finalize_state = [](State &&state) { return std::move(state); };

	auto first = std::ranges::begin(r);
	size_t count = static_cast<size_t>(std::ranges::distance(r));

	auto run_chunk = [&f, first](State &state, size_t begin, size_t end) {
		for (size_t i = begin; i < end; ++i)
			if (std::invoke(f, *(first + i)))
				state.push_back(*(first + i));
	};

	auto result = bulk_execute(pool, count, make_state, run_chunk,
	                           finalize_state, options);

	std::vector<T> merged;
	for (auto &part : result)
		merged.insert(merged.end(), std::make_move_iterator(part.begin()),
		              std::make_move_iterator(part.end()));
	return merged;
}

// parallel filter with worker-local scratch state: collect all elements 'x'
// for which 'f(state, x)' returns true. Order of the returned elements is
// unspecified.
auto filter_unordered(ThreadPool &pool,
                      std::ranges::random_access_range auto &&r,
                      auto make_state, auto f, bulk_options options)
{
	using T = std::ranges::range_value_t<decltype(r)>;
	using Scratch =
	    std::remove_cvref_t<std::invoke_result_t<decltype(make_state) &>>;
	struct State
	{
		Scratch scratch;
		std::vector<T> output;
	};

	auto finalize_state = [](State &&state) { return std::move(state.output); };

	auto first = std::ranges::begin(r);
	size_t count = static_cast<size_t>(std::ranges::distance(r));

	auto run_chunk = [&f, first](State &state, size_t begin, size_t end) {
		for (size_t i = begin; i < end; ++i)
			if (std::invoke(f, state.scratch, *(first + i)))
				state.output.push_back(*(first + i));
	};

	auto result = bulk_execute(
	    pool, count,
	    [&make_state] { return State{std::invoke(make_state), {}}; }, run_chunk,
	    finalize_state, options);

	std::vector<T> merged;
	for (auto &part : result)
		merged.insert(merged.end(), std::make_move_iterator(part.begin()),
		              std::make_move_iterator(part.end()));
	return merged;
}

} // namespace util
