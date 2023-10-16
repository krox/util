#pragma once

// Little helpers for memory allocation and management. Mostly in order to
// make writing custom containers a little less painful.

#include <algorithm>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <new>
#include <span>
#include <utility>

namespace util {

template <class T> struct default_delete;

// Smart pointer that stores an owning span instead of an owning pointer. The
// deleter gets passed (pointer, size) and '.get()' returns a std::span.
//   * compared to std::unique_ptr<T[]>:
//     - offers a '.size()' member
//     - can use any allocator, not just 'new[]' / 'delete[]'
//       (defaults to std::allocator<T> via the default deleter)
//   * compared to std::vector<T>:
//     - no excess capacity, thus slightly smaller overhead
//     - can be neither resized nor copied
//     - semantic of 'const' follows std::unique_ptr, not std::vector. So
//       elements of a 'const unique_span' can be modified.
//   * This class could (and previously was) implemented as a specialization of
//     std::unique_ptr, storing the size in a custom deleter. But that makes the
//     interface somewhat awkward. Hopefully this is now clean.
template <class T, class Deleter = default_delete<T>> class unique_span
{
	T *ptr_ = nullptr;
	size_t size_ = 0;
	[[no_unique_address]] Deleter deleter_ = {};

  public:
	// typedefs

	using element_type = T;
	using value_type = T;
	using deleter_type = Deleter;
	using size_type = size_t;
	using difference_typ = ptrdiff_t;
	using reference = T &;
	using const_reference = T const &;
	using pointer = T *;
	using const_pointer = T const *;
	using iterator = T *;
	using reverse_iterator = std::reverse_iterator<iterator>;

	// constructors

	unique_span() = default;
	constexpr unique_span(std::nullptr_t) noexcept {}
	explicit constexpr unique_span(T *p, size_t n,
	                               Deleter const &d = {}) noexcept
	    : ptr_(p), size_(n), deleter_(d)
	{}
	template <size_t Extent>
	explicit constexpr unique_span(std::span<T, Extent> s,
	                               Deleter const &d = {}) noexcept
	    : ptr_(s.data()), size_(s.size()), deleter_(d)
	{}

	// special members (move only)

	unique_span(unique_span &&other) noexcept
	    : ptr_(std::exchange(other.ptr_, nullptr)),
	      size_(std::exchange(other.size_, 0)), deleter_(other.deleter_)
	{}

	unique_span &operator=(unique_span &&other) noexcept
	{
		reset();
		ptr_ = std::exchange(other.ptr_, nullptr);
		size_ = std::exchange(other.size_, 0);
		deleter_ = other.deleter_;
		return *this;
	}

	unique_span &operator=(std::nullptr_t) noexcept { reset(); }

	friend void swap(unique_span &a, unique_span &b) noexcept
	{
		using std::swap;
		swap(a.ptr_, b.ptr_);
		swap(a.size_, b.size_);
		swap(a.deleter_, b.deleter_);
	}

	constexpr std::span<T> release() noexcept
	{
		auto r = get();
		ptr_ = nullptr;
		size_ = 0;
		return r;
	}

	void reset() noexcept
	{
		if (ptr_)
			deleter_(ptr_, size_);
		ptr_ = nullptr;
		size_ = 0;
	}

	~unique_span()
	{
		if (ptr_)
			deleter_(ptr_, size_);
	}

	// size metrics

	constexpr explicit operator bool() const noexcept { return ptr_; }
	constexpr size_t size() const noexcept { return size_; }
	constexpr size_t size_bytes() const noexcept { return size_ * sizeof(T); }
	constexpr bool empty() const noexcept { return size_; }

	// data access (like std::span)

	constexpr std::span<T> get() const noexcept
	{
		return std::span(ptr_, size_);
	}

	T &operator[](size_t i) const { return ptr_[i]; }

	T &at(size_t i) const
	{
		if (i >= size_)
			throw std::out_of_range("unique_span out of range");
		return ptr_[i];
	}

	constexpr T *data() const noexcept { return ptr_; }

	constexpr iterator begin() const noexcept { return ptr_; }
	constexpr iterator end() const noexcept { return ptr_ + size_; }
	constexpr reverse_iterator rbegin() const noexcept
	{
		return reverse_iterator(end());
	}
	constexpr reverse_iterator rend() const noexcept
	{
		return reverse_iterator(begin());
	}

	// misc

	Deleter &get_deleter() noexcept { return deleter_; }
	Deleter const &get_deleter() const noexcept { return deleter_; }
};

// default deleter for unique_span
template <class T> struct default_delete
{
	void operator()(T *p, size_t n) noexcept
	{
		std::destroy(p, p + n);
		std::free(p);
	}
};

// only deallocates, does not destruct any elements
template <class T> struct free_delete
{
	void operator()(T *p, size_t) { std::free(p); }
};
template <typename T> using unique_memory = unique_span<T, free_delete<T>>;

namespace detail {
void *util_mmap(size_t);
void util_munmap(void *, size_t) noexcept;
} // namespace detail
template <class T> struct mmap_delete
{
	void operator()(T *p, size_t n) { detail::util_munmap(p, n * sizeof(T)); }
};
template <typename T> using lazy_memory = unique_span<T, mmap_delete<T>>;

// make sure the default deleter does not take any space
static_assert(sizeof(unique_span<int>) == sizeof(std::span<int>));
static_assert(sizeof(unique_memory<int>) == sizeof(std::span<int>));
static_assert(sizeof(lazy_memory<int>) == sizeof(std::span<int>));

// allocate memory sized and aligned for T[n], but does not
// initilize the individual objects
template <class T> unique_memory<T> allocate(size_t n)
{
	if (n == 0)
		return {};
	void *p = alignof(T) <= alignof(max_align_t)
	              ? std::aligned_alloc(alignof(T), n * sizeof(T))
	              : std::malloc(n * sizeof(T));
	if (!p)
		throw std::bad_alloc();
	return unique_memory<T>(static_cast<T *>(p), n);
}

// same as alloate(), but highly aligned (for cache and/or SIMD)
template <class T> unique_memory<T> aligned_allocate(size_t n)
{
	// alignment requirement of AVX512 = 64 bytes
	// cache line size on x86 CPUs = 64 bytes
	size_t align = std::max(size_t(64), alignof(T));

#if __cpp_lib_hardware_interference_size >= 201603
	// Some x86 CPUs like to prefetch pairs of cache lines. In that case, the
	// "interference size" might be >64 bytes
	align = std::max(align, std::hardware_destructive_interference_size);
	align = std::max(align, std::hardware_constructive_interference_size);
#endif

	if (n == 0)
		return {};
	// std::aligned_alloc() requires size to be a multiple of alignment.
	// NOTE: this only works because default_delete does not take size into
	//       account, otherwise, the deleter might use the wrong size
	size_t size = (n * sizeof(T) + align - 1) / align * align;
	void *p = std::aligned_alloc(align, size);
	if (!p)
		throw std::bad_alloc();
	return unique_memory<T>(static_cast<T *>(p), n);
}

template <class T> lazy_memory<T> lazy_allocate(size_t n)
{
	// NOTE: due to page size, alignment will never be an issue here
	if (n == 0)
		return {};
	auto p = detail::util_mmap(n * sizeof(T));
	return lazy_memory<T>(static_cast<T *>(p), n);
}

// allocate and initilize memory, returning a unique_span<...>
template <class T>
unique_span<T> make_unique_span(size_t n, T const &value = T{})
{
	auto mem = allocate<T>(n);
	std::uninitialized_fill(mem.begin(), mem.end(), value);
	return unique_span<T>(mem.release().data(), n);
}

template <class T>
unique_span<T> make_aligned_unique_span(size_t n, T const &value = T{})
{
	auto mem = aligned_allocate<T>(n);
	std::uninitialized_fill(mem.begin(), mem.end(), value);
	return unique_span<T>(mem.release().data(), n);
}

// overload this as
//     template<> struct is_trivially_relocatable<MyType> : std::true_type {};
// in order to enable memcpy optimization of relocate. Would need something like
//     https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2020/p1144r5.html
// for reasonable automatic deduction
template <class T> struct is_trivially_relocatable
{
	static constexpr bool value = std::is_trivially_copy_constructible_v<T> &&
	                              std::is_trivially_destructible_v<T>;
};
template <class T>
inline constexpr bool is_trivially_relocatable_v =
    is_trivially_relocatable<T>::value;

// relocate = move + destroy, potentially more efficient and readable
template <typename T> void uninitialized_relocate_at(T *src, T *dest) noexcept
{
	static_assert(std::is_nothrow_move_constructible_v<T>);
	static_assert(std::is_nothrow_destructible_v<T>);

	if constexpr (is_trivially_relocatable_v<T>)
		std::memcpy(dest, src, sizeof(T));
	else
	{
		// NOTE: having the move and destruct next to each other (instead of
		//       two loops), might help the compiler optimize. For example for
		//       RAII types like unique_ptr, the destructor is trivial for
		//       the freshly moved-from objects.
		std::construct_at(dest, std::move(*src));
		std::destroy_at(src);
	}
}

// assumes no overlap
template <typename T>
void uninitialized_relocate_n(T *src, size_t n, T *dest) noexcept
{
	static_assert(std::is_nothrow_move_constructible_v<T>);
	static_assert(std::is_nothrow_destructible_v<T>);

	if constexpr (is_trivially_relocatable_v<T>)
		std::memcpy(static_cast<void *>(dest), src, sizeof(T) * n);
	else
	{
		for (size_t i = 0; i < n; ++i)
			uninitialized_relocate_at(dest + i, src + i);
	}
}

template <class T> T relocate(T *src) noexcept
{
	static_assert(std::is_nothrow_move_constructible_v<T>);
	static_assert(std::is_nothrow_destructible_v<T>);
	auto r = std::move(*src);
	std::destroy_at(src);
	return r;
}

template <class T> void memswap(T *a, T *b) noexcept
{
	char tmp[sizeof(T)];
	std::memcpy(tmp, a, sizeof(T));
	std::memcpy(static_cast<void *>(a), b, sizeof(T));
	std::memcpy(static_cast<void *>(b), tmp, sizeof(T));
}

} // namespace util
