#pragma once

// Little helpers for memory allocation and management. Mostly in order to
// make writing custom containers a little less painful.

#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <new>
#include <span>
#include <utility>

namespace util {

// custom deleter for std::unique_ptr that only deallocates memory, but does not
// call any destructors
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

// same as allocate() but highly aligned
//     * optimize cache usage, avoid false sharing
//     * sufficient for optimal SIMD usage
template <typename T> memory_ptr<T> aligned_allocate(size_t n)
{
	// on x86, both of these are 64 bytes. Not sure when they would differ
	constexpr size_t cache_align =
	    std::max(std::hardware_destructive_interference_size,
	             std::hardware_constructive_interference_size);
	constexpr size_t simd_align = 16; // I think this is the value for AVX
	constexpr size_t type_align =
	    std::max(alignof(T), alignof(std::max_align_t));
	constexpr size_t align =
	    std::max(std::max(cache_align, simd_align), type_align);
	constexpr int cache_line = 64; // correct for x86 at least
	if (n == 0)
		return {};
	size_t length = sizeof(T) * n;
	length = (length + align - 1) / align * align;
	void *p = std::aligned_alloc(align, length);
	if (!p)
		throw std::bad_alloc();
	return memory_ptr<T>(p);
}

// default deleter for unique_array that just uses an allocator
template <class Allocator> struct default_array_delete
{
	using value_type = Allocator::value_type;
	using alloc_traits = std::allocator_traits<Allocator>;

	[[no_unique_address]] Allocator alloc_ = {};

	default_array_delete() = default;
	explicit default_array_delete(Allocator const &alloc) : alloc_(alloc) {}

	void operator()(value_type *ptr, size_t n) noexcept
	{
		for (size_t i = 0; i < n; ++i)
			alloc_traits::destroy(alloc_, ptr + i);
		alloc_traits::deallocate(alloc_, ptr, n);
	}

	value_type *create(size_t n, value_type const &value) noexcept
	{
		value_type *ptr = alloc_traits::allocate(alloc_, n);
		for (size_t i = 0; i < n; ++i)
			alloc_traits::construct(alloc_, ptr + i, value);
		return ptr;
	}
};

// alternative to default_array_delete that does not call any destructor, just
// deallocates memory
template <class Allocator> struct uninitialized_array_delete
{
	using value_type = Allocator::value_type;
	using alloc_traits = std::allocator_traits<Allocator>;

	[[no_unique_address]] Allocator alloc_ = {};

	uninitialized_array_delete() = default;
	explicit uninitialized_array_delete(Allocator const &alloc) : alloc_(alloc)
	{}

	void operator()(value_type *ptr, size_t n) noexcept
	{
		alloc_traits::deallocate(alloc_, ptr, n);
	}

	value_type *create(size_t n) noexcept
	{
		return alloc_traits::allocate(alloc_, n);
	}
};

// Alternative to std::unique_ptr<T[]> that stores the size in the object.
//     * 'new[]' and 'delete[]' are not used. Instead, the deleter gets passed
//       (pointer, size), so any allocator can be used.
//     * Can also be used as an alternative to std::vector<T> which can be
//       neither resized nor copied, thus saving on storage for extra capacity.
//       Also note that the unique_ptr-like interface handles const different
//       from the vector-like
//     * This class could (and previously was) implemented as a specialization
//       of std::unique_ptr, storing the size in a custom deleter. But that
//       makes the interface somewhat awkward. Hopefully this is now clean.
template <class T, class Deleter = default_array_delete<std::allocator<T>>>
class unique_array
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
	using const_iterator = T const *;

	// constructors

	unique_array() = default;
	constexpr unique_array(std::nullptr_t) noexcept {}
	explicit constexpr unique_array(T *p, size_t n,
	                                Deleter const &d = {}) noexcept
	    : ptr_(p), size_(n), deleter_(d)
	{}

	// special members (move only)

	unique_array(unique_array &&other) noexcept
	    : ptr_(std::exchange(other.ptr_, nullptr)),
	      size_(std::exchange(other.size_, 0)), deleter_(other.deleter_)
	{}

	unique_array &operator=(unique_array &&other) noexcept
	{
		reset();
		ptr_ = std::exchange(other.ptr_, nullptr);
		size_ = std::exchange(other.size_, 0);
		deleter_ = other.deleter_;
		return *this;
	}

	unique_array &operator=(nullptr_t) noexcept { reset(); }

	void swap(unique_array &other) noexcept
	{
		using std::swap;
		swap(ptr_, other.ptr_);
		swap(size_, other.size_);
		swap(deleter_, other.deleter_);
	}

	constexpr T *release() noexcept
	{
		auto r = ptr_;
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

	~unique_array()
	{
		if (ptr_)
			deleter_(ptr_, size_);
	}

	// size metrics (smart pointer-like and container-like)

	constexpr explicit operator bool() const noexcept { return ptr_; }
	constexpr size_t size() const noexcept { return size_; }
	constexpr bool empty() const noexcept { return size_; }

	// vector-like data access

	T &operator[](size_t i) { return ptr_[i]; }
	T const &operator[](size_t i) const { return ptr_[i]; }

	T &at(size_t i)
	{
		if (i >= size_)
			throw std::out_of_range("unique_array out of range");
		return ptr_[i];
	}
	T const &at(size_t i) const
	{
		if (i >= size_)
			throw std::out_of_range("unique_array out of range");
		return ptr_[i];
	}

	constexpr T *data() noexcept { return ptr_; }
	constexpr T const *data() const noexcept { return ptr_; }

	iterator begin() { return ptr_; }
	const_iterator begin() const { return ptr_; }
	const_iterator cbegin() const { return ptr_; }
	iterator end() { return ptr_ + size_; }
	const_iterator end() const { return ptr_ + size_; }
	const_iterator cend() const { return ptr_ + size_; }

	// smart-pointer-like data access

	constexpr std::span<T> get() noexcept { return std::span(ptr_, size_); }
	constexpr std::span<const T> get() const noexcept
	{
		return std::span(ptr_, size_);
	}

	// misc

	Deleter &get_deleter() noexcept { return deleter_; }
	Deleter const &get_deleter() const noexcept { return deleter_; }
};

template <class T, class D>
void swap(unique_array<T, D> &a, unique_array<T, D> &b) noexcept
{
	a.swap(b);
}

static_assert(sizeof(unique_array<int>) == 2 * sizeof(void *));

// allocate memory using given allocator, returning a unique_array<...> that
// cleans itself up
template <class T, class Allocator = std::allocator<T>>
auto make_unique_array(size_t n, T const &value = T{},
                       Allocator alloc = Allocator{})
{
	using Deleter = default_array_delete<Allocator>;
	auto deleter = Deleter(alloc);
	T *ptr = deleter.create(n, value);
	return unique_array<T, Deleter>(ptr, n, deleter);
}

template <class T, class Allocator = std::allocator<T>>
auto make_uninitialized_unique_array(size_t n, Allocator alloc = Allocator{})
{
	using Deleter = uninitialized_array_delete<Allocator>;
	auto deleter = Deleter(alloc);
	T *ptr = deleter.create(n);
	return unique_array<T, Deleter>(ptr, n, deleter);
}

// wrapper around linux's mmap/munmap for allocating memory
void *util_mmap(size_t);
void util_munmap(void *, size_t) noexcept;

// allocator that calls linux's mmap/munmap, which typically is only backed by
// physical storage after access, not at time of allocation
template <typename T> struct mmap_allocator
{
	// NOTE: due to page-size, alignment will never be an issue here
	using value_type = T;
	T *allocate(size_t n) { return static_cast<T *>(util_mmap(n * sizeof(T))); }
	void deallocate(T *p, size_t n) noexcept { util_munmap(p, n * sizeof(T)); }
};

template <typename T>
using lazy_uninitialized_unique_array =
    unique_array<T, uninitialized_array_delete<mmap_allocator<T>>>;

// "allocates" memory by calling mmap
template <typename T> lazy_uninitialized_unique_array<T> lazy_allocate(size_t n)
{
	return make_uninitialized_unique_array<T>(n, mmap_allocator<T>{});
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
		construct_at(dest, std::move(*src));
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

// little wrapper for fopen() / close() that does RAII and throws on errors
struct fclose_delete
{
	void operator()(FILE *p);
};
using FilePointer = std::unique_ptr<FILE, fclose_delete>;
FilePointer open_file(std::string const &filename, char const *mode);

class MappedFile
{
	void *ptr_ = nullptr;
	size_t size_ = 0;
	MappedFile(char const *, char const *, bool);

  public:
	// constructors
	MappedFile() = default;

	static MappedFile open(std::string const &file, bool writeable = false);
	static MappedFile create(std::string const &file, size_t size,
	                         bool overwrite = false);
	void close() noexcept;

	// special members (move-only type)
	~MappedFile() { close(); };
	MappedFile(MappedFile &&other) noexcept
	    : ptr_(std::exchange(other.ptr_, nullptr)),
	      size_(std::exchange(other.size_, 0))
	{}
	MappedFile &operator=(MappedFile &&other) noexcept
	{
		close();
		ptr_ = std::exchange(other.ptr_, nullptr);
		size_ = std::exchange(other.size_, 0);
		return *this;
	}

	// data access
	// NOTE: if the file is opened as read-only, writing should be considered
	//       undefined behaviour, not sure about platform specifics
	void *data() { return ptr_; }
	void const *data() const { return ptr_; }
	size_t size() const { return size_; }
	explicit operator bool() const { return ptr_; }
};

} // namespace util
