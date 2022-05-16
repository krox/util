#pragma once

#include <memory>
#include <utility>

namespace util {

struct free_delete
{
	void operator()(void *p) noexcept { std::free(p); }
};

template <typename T>
using unique_memory_ptr = std::unique_ptr<T[], free_delete>;

static_assert(sizeof(unique_memory_ptr<int>) == sizeof(void *));

// allocate uninitialized memory, sized and aligned to store a 'T[n]'
template <typename T> unique_memory_ptr<T> allocate(size_t n)
{
	T *r;
	if constexpr (alignof(T) <= alignof(std::max_align_t))
		r = static_cast<T *>(std::malloc(sizeof(T) * n));
	else
		r = static_cast<T *>(std::aligned_alloc(alignof(T), sizeof(T) * n));
	if (!r && n)
		throw std::bad_alloc();
	return unique_memory_ptr<T>(r);
}

} // namespace util
