#pragma once

#include <cstddef>
#include <cstdlib>
#include <memory>
#include <utility>

namespace util {

struct free_delete
{
	void operator()(void *p) noexcept { std::free(p); }
};

template <typename T> using memory_ptr = std::unique_ptr<T[], free_delete>;

static_assert(sizeof(memory_ptr<int>) == sizeof(void *));

// allocate uninitialized memory, sized and aligned to store a 'T[n]'
template <typename T> memory_ptr<T> allocate(size_t n)
{
	T *r;
	if constexpr (alignof(T) <= alignof(std::max_align_t))
		r = static_cast<T *>(std::malloc(sizeof(T) * n));
	else
		r = static_cast<T *>(std::aligned_alloc(alignof(T), sizeof(T) * n));
	if (!r && n)
		throw std::bad_alloc();
	return memory_ptr<T>(r);
}

// wrapper around linux's mmap/munmap for allocating memory
void *util_mmap(size_t);
void util_munmap(void *, size_t) noexcept;

struct mmap_delete
{
	size_t length_ = 0;
	void operator()(void *p) noexcept { util_munmap(p, length_); }
};

template <typename T> using lazy_memory_ptr = std::unique_ptr<T[], mmap_delete>;

// "allocates" memory by calling mmap
template <typename T> lazy_memory_ptr<T> lazy_allocate(size_t n)
{
	// NOTE: due to page-size, alignment will never be an issue here
	auto length = sizeof(T) * n;
	return lazy_memory_ptr<T>(static_cast<T *>(util_mmap(length)),
	                          mmap_delete{length});
}

} // namespace util
