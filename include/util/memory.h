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

// Owns a contiguous block of memory suitable for storing an array of T.
//   * Does not construct/destruct individual elements
template <class T, class Deleter = default_delete<T>> class array_storage
{
	T *data_ = nullptr;
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

	array_storage() = default;

	explicit constexpr array_storage(T *p, size_t n,
	                                 Deleter const &d = {}) noexcept
	    : data_(p), size_(n), deleter_(d)
	{}
	template <size_t Extent>
	explicit constexpr array_storage(std::span<T, Extent> s,
	                                 Deleter const &d = {}) noexcept
	    : data_(s.data()), size_(s.size()), deleter_(d)
	{}

	// special members (move only)

	array_storage(array_storage &&other) noexcept
	    : data_(std::exchange(other.data_, nullptr)),
	      size_(std::exchange(other.size_, 0)), deleter_(other.deleter_)
	{}

	array_storage &operator=(array_storage &&other) noexcept
	{
		reset();
		data_ = std::exchange(other.data_, nullptr);
		size_ = std::exchange(other.size_, 0);
		deleter_ = other.deleter_;
		return *this;
	}

	friend void swap(array_storage &a, array_storage &b) noexcept
	{
		using std::swap;
		swap(a.data_, b.data_);
		swap(a.size_, b.size_);
		swap(a.deleter_, b.deleter_);
	}

	constexpr std::span<T> release() noexcept
	{
		T *r = data();
		data_ = nullptr;
		size_ = 0;
		return r;
	}

	void reset() noexcept
	{
		if (data())
			deleter_(data(), size());
		data_ = nullptr;
		size_ = 0;
	}

	~array_storage() { reset(); }

	// size metrics
	constexpr size_t size() const noexcept { return size_; }
	constexpr size_t size_bytes() const noexcept { return size() * sizeof(T); }
	constexpr bool empty() const noexcept { return size() == 0; }

	// data access (no guarantee which elements have been constructed)
	constexpr T *data() noexcept { return data_; }
	constexpr T const *data() const noexcept { return data_; }
	constexpr T &operator[](size_t i) noexcept { return data()[i]; }
	constexpr T const &operator[](size_t i) const noexcept { return data()[i]; }

	// convenience wrappers for std::construct_at/std::destroy_at. It is the
	// users responsibility to know which elements have been constructed and
	// destroying them before destroying the array_storage.
	template <class... Args> T *construct_at(size_t i, Args &&...args)
	{
		assert(i < size());
		return std::construct_at(data() + i, std::forward<Args>(args)...);
	}
	void destroy_at(size_t i) noexcept
	{
		assert(i < size());
		std::destroy_at(data() + i);
	}

	// misc
	Deleter &get_deleter() noexcept { return deleter_; }
	Deleter const &get_deleter() const noexcept { return deleter_; }
};

// default deleter for array_storage
template <class T> struct default_delete
{
	void operator()(T *p, size_t) noexcept { std::free(p); }
};

namespace detail {
void *util_mmap(size_t);
void util_munmap(void *, size_t) noexcept;
} // namespace detail
template <class T> struct mmap_delete
{
	void operator()(T *p, size_t n) { detail::util_munmap(p, n * sizeof(T)); }
};
template <typename T>
using lazy_array_storage = array_storage<T, mmap_delete<T>>;

// make sure the default deleter does not take any space
static_assert(sizeof(array_storage<int>) == sizeof(std::span<int>));
static_assert(sizeof(lazy_array_storage<int>) == sizeof(std::span<int>));

// allocate memory sized and aligned for T[n], but does not
// initilize the individual objects
template <class T> array_storage<T> allocate(size_t n)
{
	if (n == 0)
		return {};
	void *p = alignof(T) <= alignof(max_align_t)
	              ? std::aligned_alloc(alignof(T), n * sizeof(T))
	              : std::malloc(n * sizeof(T));
	if (!p)
		throw std::bad_alloc();
	return array_storage<T>(static_cast<T *>(p), n);
}

// same as alloate(), but highly aligned (for cache and/or SIMD)
template <class T> array_storage<T> aligned_allocate(size_t n)
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
	return array_storage<T>(static_cast<T *>(p), n);
}

template <class T> lazy_array_storage<T> lazy_allocate(size_t n)
{
	// NOTE: due to page size, alignment will never be an issue here
	if (n == 0)
		return {};
	auto p = detail::util_mmap(n * sizeof(T));
	return lazy_array_storage<T>(static_cast<T *>(p), n);
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
			uninitialized_relocate_at(src + i, dest + i);
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

// smart pointer that deep-copies the object on copy
template <class T> class value_ptr
{
	static_assert(!std::is_reference_v<T>);
	static_assert(!std::is_void_v<T>);

	std::unique_ptr<T> ptr_;

  public:
	using element_type = T;

	// default = nullptr
	constexpr value_ptr() noexcept = default;
	constexpr value_ptr(std::nullptr_t) noexcept : ptr_(nullptr) {}

	explicit value_ptr(T *ptr) noexcept : ptr_(ptr) {}

	// construct in-place
	explicit value_ptr(std::in_place_t, auto &&...args)
	    : ptr_(std::make_unique<T>(std::forward<decltype(args)>(args)...))
	{}

	// special members
	// implementation note: important to define these out-of-line, in order to
	// make value_ptr work with incomplete types.
	value_ptr(const value_ptr &other);
	value_ptr &operator=(const value_ptr &other);
	value_ptr(value_ptr &&other) noexcept = default;
	value_ptr &operator=(value_ptr &&other) noexcept = default;
	~value_ptr() noexcept;

	// observers
	T *get() noexcept { return ptr_.get(); }
	const T *get() const noexcept { return ptr_.get(); }

	T &operator*() noexcept { return *ptr_; }
	const T &operator*() const noexcept { return *ptr_; }

	T *operator->() noexcept { return ptr_.get(); }
	const T *operator->() const noexcept { return ptr_.get(); }

	explicit operator bool() const noexcept { return static_cast<bool>(ptr_); }

	// modifiers
	void reset(T *p = nullptr) noexcept { ptr_.reset(p); }
	void swap(value_ptr &other) noexcept { ptr_.swap(other.ptr_); }
};

template <class T> value_ptr<T>::~value_ptr() noexcept = default;

template <class T>
value_ptr<T>::value_ptr(const value_ptr<T> &other)
    : ptr_(other ? std::make_unique<T>(*other.ptr_) : nullptr)
{}

template <class T>
value_ptr<T> &value_ptr<T>::operator=(const value_ptr<T> &other)
{
	if (this != &other)
		ptr_ = other ? std::make_unique<T>(*other.ptr_) : nullptr;
	return *this;
}

template <class T> void swap(value_ptr<T> &a, value_ptr<T> &b) noexcept
{
	a.swap(b);
}

template <class T>
bool operator==(value_ptr<T> const &a, value_ptr<T> const &b) noexcept
{
	return a.get() == b.get();
}

template <class T> value_ptr<T> make_value(auto &&...args)
{
	return value_ptr<T>(std::in_place, std::forward<decltype(args)>(args)...);
}

} // namespace util
