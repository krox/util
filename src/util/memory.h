#pragma once

// Little helpers for memory allocation and management. Mostly in order to
// make writing custom containers a little less painful.

#include <cstddef>
#include <cstdlib>
#include <cstring>
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

// part of C++20 (in a slightly better version)
template <class T, class... Args>
constexpr T *construct_at(T *p, Args &&...args)
{
	return ::new (const_cast<void *>(static_cast<const volatile void *>(p)))
	    T(std::forward<Args>(args)...);
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

class MappedFile
{
	void *ptr_ = nullptr;
	size_t size_ = 0;
	MappedFile(char const *, char const *, bool);

  public:
	// constructors
	MappedFile() = default;

	static MappedFile open(std::string const &file, bool writeable = false);
	static MappedFile create(std::string const &file, bool overwrite = false);
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
	explicit operator bool() { return ptr_; }
};

} // namespace util
