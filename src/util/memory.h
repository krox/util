#pragma once

#include <cstdlib>
#include <vector>

namespace util {

// the memory pool behind a MonotoneAllocator
class MonotoneMemoryPool
{
	static constexpr size_t block_size_ = 4096;

	std::vector<void *> blocks_; // list of blocks to be free'd in the end
	size_t head_ = 0, tail_ = 0;

	static size_t align(size_t p, size_t alignment)
	{
		return (p + alignment - 1) & ~(alignment - 1);
	}

	void alloc_new_block(size_t alignment)
	{
		auto p = std::aligned_alloc(alignment, block_size_);
		if (p == nullptr)
			throw std::bad_alloc();
		try
		{
			blocks_.push_back(p);
		}
		catch (...)
		{
			std::free(p);
			throw;
		}
		head_ = (size_t)p;
		tail_ = head_ + block_size_;
	}

  public:
	MonotoneMemoryPool() = default;
	~MonotoneMemoryPool()
	{
		for (auto ptr : blocks_)
			std::free(ptr);
	}

	MonotoneMemoryPool(MonotoneMemoryPool const &) = delete;
	MonotoneMemoryPool &operator=(MonotoneMemoryPool const &) = delete;

	// NOTE: the allocator contains a raw pointer/reference to the pool, so the
	//       pool should not be movable

	// throws std::bad_alloc
	// alignment must be a power of two and a divisor of size
	[[nodiscard]] void *aligned_alloc(size_t alignment, size_t size)
	{
		assert(alignment != 0 && (alignment & (alignment - 1)) == 0);
		assert((size & (alignment - 1)) == 0);

		// returning null is okay, returning a pointer to a location where
		// something else will eventually be placed is not.
		if (size == 0)
			return nullptr;

		// large allocations are just passed on the underlying system
		if (size >= block_size_)
		{
			if (auto p = std::aligned_alloc(alignment, size); p)
				return p;
			else
				throw std::bad_alloc();
		}

		if (align(head_, alignment) + size > tail_)
			alloc_new_block(alignment);

		auto p = align(head_, alignment);
		assert(p + size <= tail_);
		head_ = p + size;
		return (void *)p;
	}
};

// simple allocator that allocates by incraesing a pointer and can only
// deallocate if the whole thing is released
template <typename T> class MonotoneAllocator
{
	MonotoneMemoryPool *pool_; // may not be null

  public:
	using value_type = T;
	// NOTE: Other type aliases ('pointer', 'size_type' and the like), are
	//       defaulted in std::allocator_traits. No need to specify here.

	// TODO: not completely sure about these flags
	using propagate_on_container_copy_assignment = std::false_type;
	using propagate_on_container_move_assignment = std::true_type;
	using propagate_on_container_swap = std::true_type;

	MonotoneAllocator(MonotoneMemoryPool &pool) noexcept : pool_(&pool) {}
	template <typename U>
	MonotoneAllocator(MonotoneAllocator<U> const &other) noexcept
	    : pool_(&other.pool())
	{}

	MonotoneMemoryPool &pool() noexcept { return *pool_; }

	// throws std::bad_alloc if not possible
	// starts lifetime of an array of type T[n], but not of the elements in it
	[[nodiscard]] T *allocate(size_t n)
	{
		return (T *)pool_->aligned_alloc(alignof(T), n * sizeof(T));
	}

	void deallocate(T *, size_t) noexcept {}
};

} // namespace util
