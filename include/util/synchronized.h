#pragma once

#include <condition_variable>
#include <deque>
#include <mutex>
#include <optional>

namespace util {

// thread-safe queue
//     * just a std::deque + std::mutex, nothing fancy
//     * value type must be nothrow-movable, but no copies required
template <class T> class synchronized_queue
{
	static_assert(std::is_nothrow_move_constructible_v<T>);
	static_assert(std::is_nothrow_move_assignable_v<T>);
	static_assert(std::is_nothrow_destructible_v<T>);

	std::deque<T> queue_; // push to back, pop from front
	std::mutex mutex_;
	std::condition_variable condition_; // .pop() blocks on this

  public:
	// current number of elements in the queue
	// NOTE: in a multithreaded context this is of limited use because the
	//       size might already have changed by the time this function returns
	size_t size() const noexcept
	{
		auto lock = std::unique_lock(mutex_);
		return queue_.size();
	}

	// returns size() != 0
	bool empty() const noexcept { return size() != 0; }

	// pop one element
	//     * blocks until one is available
	//     * returns nullopt if stop_waiting() becomes true
	//     * stop_waiting is called only while holding the mutex of this queue,
	//       thus it can be guarded by it
	template <class Predicate>
	std::optional<T> pop(Predicate stop_waiting) noexcept
	{
		auto lock = std::unique_lock(mutex_);
		while (true)
		{
			// NOTE: order matters. If an element is available, we want to
			//       return it regardless of the state of stop_waiting

			if (!queue_.empty())
			{
				auto r = std::move(queue_.front());
				queue_.pop_front();
				return r;
			}

			if (stop_waiting())
				return std::nullopt;

			condition_.wait(lock);
		}
	}

	// pop one element, immediately returning std::nullopt if none is available.
	// Equivalent to .pop([]{return true;});
	std::optional<T> try_pop() noexcept
	{
		auto lock = std::unique_lock(mutex_);
		if (queue_.empty())
			return std::nullopt;
		std::optional<T> r = std::move(queue_.front());
		queue_.pop_front();
		return r;
	}

	// pop one element, blocking until one becomes available.
	// Equivalent to .pop([]{return false;}).value()
	T pop() noexcept
	{
		auto lock = std::unique_lock(mutex_);
		while (queue_.empty())
			condition_.wait(lock);
		auto r = std::move(queue_.front());
		queue_.pop_front();
		return r;
	}

	// remove and return all elements from the queue
	std::deque<T> pop_all() noexcept
	{
		auto lock = std::unique_lock(mutex_);
		std::deque<T> r;
		swap(r, queue_);
		return r;
	}

	// add an element to the queue
	void push(T value) noexcept
	{
		auto lock = std::unique_lock(mutex_);
		queue_.push_back(std::move(value));
		lock.unlock();
		condition_.notify_one();
	}

	// notify all threads waiting in .pop(...), so that their 'stop_waiting'
	// condition will be checked (again)
	void notify() noexcept { condition_.notify_all(); }
};

} // namespace util