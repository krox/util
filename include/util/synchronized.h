#pragma once

#include <atomic>
#include <condition_variable>
#include <deque>
#include <memory>
#include <mutex>
#include <new>
#include <optional>
#include <type_traits>
#include <utility>

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

// thread-safe multi-producer single-consumer queue.
// * Classical algorithm by Vyukov which is lock-free (no mutexes) and wait-free
//  (no CAS loops) for both producers and consumer
// * Subtle caveat: 'push' is still not exactly atomic: It can happen that a
//   pushed element only becomes visible to the consumer after some other push
//   concludes. In practice this usually does not matter because:
//     1. for each producer, relative order of pushes is still preserved
//     2. A typical usecase will do "push(..); notify_consumer();". It is
//       possible that consumer wakes up and does not yet see the pushed
//       element. But in that case, another push is in flight, and the consumer
//       will see both elements after that one notifies. Thus as long as the
//       consumer proecsses all elements on each wakeup, it will eventually see
//       everything.
// * TODO: tighter memory ordering. Currently all atomic operations are
//         sequentially consistent, which is overly conservative.
template <class T>
    requires(std::is_nothrow_move_constructible_v<T> &&
             std::is_nothrow_move_assignable_v<T> &&
             std::is_nothrow_destructible_v<T>)
class MpscQueue
{
	struct Node
	{
		union Storage
		{
			char dummy;
			T value;

			Storage() noexcept : dummy() {}
			~Storage() noexcept {}
		};

		std::atomic<Node *> next{nullptr};
		Storage storage;
	};

	std::atomic<Node *> head_{nullptr}; // consumer pops here
	std::atomic<Node *> tail_{nullptr}; // producers push here

  public:
	MpscQueue(const MpscQueue &) = delete;
	MpscQueue &operator=(const MpscQueue &) = delete;
	MpscQueue(MpscQueue &&) = delete;
	MpscQueue &operator=(MpscQueue &&) = delete;

	MpscQueue() noexcept
	{
		Node *dummy = new Node;
		head_ = dummy;
		tail_ = dummy;
	}

	~MpscQueue() noexcept
	{
		while (pop())
			;
		delete tail_;
	}

	// push an element to the queue
	//   - never blocks. New element might not be visible immediately if other
	//     producers are racing. But once all producers are finished, everything
	//     will be visible.
	void push(const T &value) { emplace(value); }
	void push(T &&value) { emplace(std::move(value)); }

	// same as push, but constructs the element in-place from the given
	// arguments
	template <class... Args> void emplace(Args &&...args)
	{
		Node *node = new Node;
		std::construct_at(&node->storage.value, std::forward<Args>(args)...);
		Node *old_tail = tail_.exchange(node);
		old_tail->next.store(node);
	}

	// pop an element from the queue
	//   - returns std::nullopt if nothing is available
	std::optional<T> pop() noexcept
	{
		Node *old_head = head_.load();
		Node *next = old_head->next.load();
		if (!next)
			return std::nullopt;
		std::optional<T> r = std::move(next->storage.value);
		std::destroy_at(&next->storage.value);
		head_.store(next);
		delete old_head;
		return r;
	}
};

} // namespace util