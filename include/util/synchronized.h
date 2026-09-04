#pragma once

#include <atomic>
#include <cassert>
#include <condition_variable>
#include <deque>
#include <memory>
#include <mutex>
#include <new>
#include <optional>
#include <type_traits>
#include <utility>

namespace util {

// Smart pointer that does not own the lifetime of the pointed-to object, but
// does own exclusive access to it via a std::mutex.
//  * This class mainly exists to make to make the API of 'synchronized<T>'
//    convenient. But as it is a clean abstraction on its own, it is defined
//    separately instead of as a nested class.
//  * The API of this class is deliberately modeled after the standard library
//    smart pointers because of the strong similarity between
//        std::unique_ptr = T* + Deleter
//    and
//        util::locked_ptr = T* + Lock
//    But the analogy breaks down due to the fact that the resource owned by
//    unique_ptr is represented by the pointer, while the resource that is owned
//    by locked_ptr is represented by the lock.
template <class T> class locked_ptr
{
	// future: Would be natural to template on different lock/mutex types. For
	// now, this is sufficient. Needs to be a timed mutex so that
	// 'synchronized<T>::lock_for' can be implemented.
	using Lock = std::unique_lock<std::timed_mutex>;

	// invariant:
	// either bool(*this) == false AND ptr_ == nullptr AND !lock_.owns_lock()
	// or     bool(*this) == true  AND ptr_! = nullptr AND  lock_.owns_lock()

	T *ptr_{nullptr};
	Lock lock_;

  public:
	// default constructor creates a null-state
	locked_ptr() noexcept = default;

	// constructor that takes ownership via a moved-in lock.
	//   * If 'lock.owns_lock()==false', then the locked_ptr will be in the null
	//     state and 'ptr' is ignored. Otherwise, ptr_ may not be nullptr.
	locked_ptr(T *ptr, Lock &&lock) noexcept : ptr_(ptr), lock_(std::move(lock))
	{
		if (lock_.owns_lock())
			assert(ptr_ != nullptr);
		else
			ptr_ = nullptr;
	}

	// move-only
	locked_ptr(locked_ptr const &) = delete;
	locked_ptr &operator=(locked_ptr const &) = delete;
	locked_ptr(locked_ptr &&other) noexcept
	    : ptr_(std::exchange(other.ptr_, nullptr)),
	      lock_(std::move(other.lock_))
	{}
	locked_ptr &operator=(locked_ptr &&other) noexcept
	{
		if (this == &other)
			return *this;
		ptr_ = std::exchange(other.ptr_, nullptr);
		lock_ = std::move(other.lock_);
		return *this;
	}

	// check whether this pointer is valid (i.e. owns the lock)
	explicit operator bool() const noexcept { return lock_.owns_lock(); }

	// dereference the pointer. Undefined behaviour if the lock is not owned.
	T &operator*() const noexcept { return *ptr_; }
	T *operator->() const noexcept { return ptr_; }

	// get the raw pointer. returns nullptr if the lock is not owned.
	T *get() const noexcept { return ptr_; }

	// release the lock if held. After this call, the locked_ptr will be in the
	// null state. No-op if the lock was not held to begin with.
	void unlock() noexcept
	{
		// note: std::unique_lock::unlock() throws if the lock is not held, so
		// we need to check, because we want silent no-op behaviour in that
		// case.
		if (!*this)
			return;
		ptr_ = nullptr;
		lock_.unlock();
	}

	// Release the lock and return it. After this call, the locked_ptr will be
	// in the null state. This does not itself unlock the lock, but transfers
	// ownership of it to the caller.
	//   * Note: if the return value is ignored and thus immediately destroyed,
	//     the behaviour is equivalent to calling '.unlock()'. The separate
	//     function is provided mostly for clarity.
	Lock release_lock() noexcept
	{
		ptr_ = nullptr;
		return std::move(lock_);
	}
};

// Simple synchronization primitive, coupling a value with a mutex.
// Intended usage for 'synchronized<T> obj;':
//
// single line critical sections (lock released at end of full expression):
//     obj.lock()->use();
// or:
//     use(*obj.lock());
//
// longer critical sections (lock released when 'locked' goes out of scope):
//     auto locked = obj.lock();
//     locked->use();
//     locked->use_again();
//     locked.unlock(); // optional explicit/early unlock
//
// non-blocking try-lock:
//    if (auto locked = obj.try_lock())
//        locked->use();
template <class T> class synchronized
{
	// could be templated, but this is sufficient for now.
	// NOTE: needs to be a timed mutex to support 'lock_for()'.
	using Mutex = std::timed_mutex;
	using Lock = std::unique_lock<Mutex>;

	mutable Mutex mutex_;
	T value_;

  public:
	// constructor mimics contained value. In particular, synchronized<T> is
	// default-constructible if T is.
	template <class... Args>
	    requires std::constructible_from<T, Args...>
	explicit synchronized(Args &&...args) : value_(std::forward<Args>(args)...)
	{}

	// disable move/copy
	// Note: move is impossible due to the mutex. Copy might be added in the
	// future, but for now we remove it to avoid accidental misues.
	synchronized(synchronized const &) = delete;
	synchronized &operator=(synchronized const &) = delete;

	// Lock the mutex and return a locked_ptr to access the value.
	//   * Currently, always takes an exclusive lock. In the future, the const
	//     variant could take a shared lock instead.
	locked_ptr<T> lock() { return locked_ptr<T>(&value_, Lock(mutex_)); }
	locked_ptr<const T> lock() const
	{
		return locked_ptr<const T>(&value_, Lock(mutex_));
	}

	// Non-blocking variant of 'lock()' that returns a null-state locked_ptr if
	// the lock can not be acquired immediately.
	locked_ptr<T> try_lock()
	{
		return locked_ptr<T>(&value_, Lock(mutex_, std::try_to_lock));
	}
	locked_ptr<const T> try_lock() const
	{
		return locked_ptr<const T>(&value_, Lock(mutex_, std::try_to_lock));
	}

	// timed variants of 'lock()' that return a null-state locked_ptr if the
	// lock can not be acquired within the given timeout.
	template <class Rep, class Period>
	locked_ptr<T> lock_for(std::chrono::duration<Rep, Period> const &timeout)
	{
		return locked_ptr<T>(&value_, Lock(mutex_, timeout));
	}
	template <class Rep, class Period>
	locked_ptr<const T>
	lock_for(std::chrono::duration<Rep, Period> const &timeout) const
	{
		return locked_ptr<const T>(&value_, Lock(mutex_, timeout));
	}
};

// thread-safe queue
// - just a std::deque + std::mutex, nothing fancy
// - based on 'std::unique_ptr' to manage ownership transfer
//   - Side effect: subclasses of 'T' can be stored just as well
// - explicit 'closed' state to manage clean shutdown in typical usage
//   - NOTE: racing destruction with anything is still UB. Calling '.close()' is
//     not sufficient to prevent this, you must ensure any waiting threads
//     actually receive the closing signal (returning null from '.pop()') before
//     destroying the queue.
// - Future ideas:
//   - Make the queue itself lock-free
//   - Make a Handle type that acts as a shared_ptr to a shared queue. This
//     would solve the UB behaviour mentioned above: Each thread has their own
//     handle, the queue is only destroyed when all threads are done.
//   - Advanced version: distinguish between ReaderHandle and WriterHandle. Then
//     call 'close()' automatically when the last writer is gone.
template <class T> class synchronized_queue
{
	std::deque<std::unique_ptr<T>> queue_; // push to back, pop from front
	mutable std::mutex mutex_;
	bool closed_ = false;
	std::condition_variable cv_; // .pop() blocks on this

  public:
	// current number of elements in the queue
	// NOTE: in a multithreaded context this is of limited use because the
	//       size might already have changed by the time this function returns
	size_t size() const noexcept
	{
		auto lock = std::unique_lock(mutex_);
		return queue_.size();
	}

	// returns size() == 0
	bool empty() const noexcept { return size() == 0; }

	bool closed() const noexcept
	{
		auto lock = std::unique_lock(mutex_);
		return closed_;
	}

	// close the queue. After this, no new elements can be added, but existing
	// ones can still be popped.
	void close() noexcept
	{
		auto lock = std::unique_lock(mutex_);
		closed_ = true;
		lock.unlock();
		cv_.notify_all();
	}

	// pop one element
	// - blocks until one is available
	// - returns null only if queue is/becomes closed AND empty
	std::unique_ptr<T> pop() noexcept
	{
		auto lock = std::unique_lock(mutex_);
		cv_.wait(lock, [this] { return !queue_.empty() || closed_; });

		// NOTE: order matters. If an element is available, we want to
		//       return it regardless of the closed state

		if (!queue_.empty())
		{
			auto r = std::move(queue_.front());
			queue_.pop_front();
			return r;
		}
		else
			return nullptr;
	}

	// non-blocking variant of pop() that returns nullptr immediately if the
	// queue is empty
	std::unique_ptr<T> try_pop() noexcept
	{
		auto lock = std::unique_lock(mutex_);
		if (queue_.empty())
			return nullptr;
		auto r = std::move(queue_.front());
		queue_.pop_front();
		return r;
	}

	// pop all currently available elements from the queue. Non-blocking, can
	// return empty.
	std::deque<std::unique_ptr<T>> pop_all() noexcept
	{
		auto lock = std::unique_lock(mutex_);
		std::deque<std::unique_ptr<T>> r;
		swap(r, queue_);
		return r;
	}

	// Add an element to the queue. Returns null on success, returns the value
	// back on failure (for example if the queue is closed)
	std::unique_ptr<T> push(std::unique_ptr<T> value) noexcept
	{
		if (!value)
			return nullptr;
		auto lock = std::unique_lock(mutex_);
		if (closed_)
			return value;
		queue_.push_back(std::move(value));
		lock.unlock();
		cv_.notify_one();
		return nullptr;
	}
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