#pragma once

// Container classes with contiguous storage. 'util::vector' is nearly
// equivalent to 'std::vector', but with a few differences:
//   - Some additional convenience:
//     * '.pop_back()' returns the removed element
//     * '.set_size_unsafe()' enables some low-level optimizations
//     * '.reserve()' takes an (optional) second argument controlling growth
//   - Some additional variants of vectors with different memory management.
//     (note that these could not be cleanly implemented using custom allocators
//     inside a 'std::vector')
//     * 'util::small_vector': small-buffer optimization
//     * 'util::static_vector': fixed capacity, no dynamic allocation
//     * 'util::indirect_vector': size and capacity stored inside allocation
//     * 'util::stable_vector': stable pointers and iterators (based on 'mmap')
//   - Only accepts nothrow-moveable types.
//     * Rationale: The overhead required to satisfy vector's strong exception
//       guarantee without nothrow-move probably makes 'vector' undesirable to
//       use in such a case anyway.
//     * Future: While true for '{std,util}::vector', this requirement could be
//       lifted for some of the variants, namely 'util::stable_vector' (and
//       maybe 'util::static_vector')
//   - No allocator support.
//     * Rationale: Many interesting strategies (namely SBO) cannot be
//       implemented using allocators anyway.
//     * Future: Implementing a 'util::pmr_vector' which takes a
//       'std::pmr::memory_resource' (directly, without wrapping it in an
//       allocator) would be quite sensible if the need ever arises.

#include "util/hash.h"
#include "util/iterator.h"
#include "util/memory.h"
#include <algorithm>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <iterator>
#include <type_traits>

namespace util {

namespace detail {

template <typename T> class MallocStorage
{
	unique_memory<T> data_ = {};
	size_t size_ = 0;

  public:
	MallocStorage() = default;
	explicit MallocStorage(size_t cap) : data_(allocate<T>(cap)) {}
	~MallocStorage() { std::destroy_n(data_.data(), size_); }

	MallocStorage(MallocStorage const &other) = delete;
	MallocStorage &operator=(MallocStorage const &other) = delete;

	void swap(MallocStorage &other) noexcept
	{
		using std::swap;
		swap(data_, other.data_);
		swap(size_, other.size_);
	}

	size_t size() const noexcept { return size_; }
	void set_size(size_t s) noexcept { size_ = s; }
	size_t capacity() const noexcept { return data_.size(); }
	static constexpr size_t max_capacity() { return SIZE_MAX / sizeof(T); }
	T *data() noexcept { return data_.data(); }
	T const *data() const noexcept { return data_.data(); }
};

template <typename T, size_t N> class StaticStorage
{
	static_assert(1 <= N && N <= UINT8_MAX);

	uint8_t size_ = 0;
	union
	{
		T data_[N];
	};

  public:
	StaticStorage() noexcept {};
	StaticStorage(size_t cap) { assert(cap <= N); }
	~StaticStorage() { std::destroy_n(data_, size_); }

	StaticStorage(StaticStorage const &other) = delete;
	StaticStorage &operator=(StaticStorage const &other) = delete;

	void swap(StaticStorage &other) noexcept
	{
		static_assert(is_trivially_relocatable_v<T>);
		memswap(this, &other);
	}

	size_t size() const noexcept { return size_; }
	void set_size(size_t s) noexcept
	{
		size_ = static_cast<decltype(size_)>(s);
	}
	size_t capacity() const noexcept { return N; }
	static constexpr size_t max_capacity() { return N; }
	T *data() { return data_; }
	T const *data() const { return data_; }
};

template <typename T, size_t N> class alignas(T) alignas(T *) SboStorage
{
	static_assert(1 <= N && N <= UINT8_MAX);

	union
	{
		T buffer_[N];

		struct __attribute__((packed))
		{
			T *data_;
			uint32_t capacity_;
		} alloc_;
	};
	uint32_t size_ : 31;
	bool big_ : 1;

  public:
	SboStorage() noexcept : size_(0), big_(false){};
	SboStorage(size_t cap) : size_(0), big_(false)
	{
		static_assert(alignof(T) <= alignof(std::max_align_t));
		if (cap <= N)
			return;
		alloc_.data_ = static_cast<T *>(std::malloc(cap * sizeof(T)));
		alloc_.capacity_ = cap;
		big_ = true;
	}
	~SboStorage()
	{
		std::destroy_n(data(), size());
		if (big_)
			std::free(alloc_.data_);
	}

	SboStorage(SboStorage const &other) = delete;
	SboStorage &operator=(SboStorage const &other) = delete;

	void swap(SboStorage &other) noexcept
	{
		static_assert(is_trivially_relocatable_v<T>);
		memswap(this, &other);
	}

	size_t size() const noexcept { return size_; }
	void set_size(size_t s) noexcept
	{
		size_ = static_cast<decltype(size_)>(s);
	}
	size_t capacity() const noexcept { return big_ ? alloc_.capacity_ : N; }
	static constexpr size_t max_capacity() { return UINT32_MAX / 2; }
	T *data() { return big_ ? alloc_.data_ : buffer_; }
	T const *data() const { return big_ ? alloc_.data_ : buffer_; }
};

template <typename T, size_t N> class MmapStorage
{
	lazy_memory<T> data_ = {};
	size_t size_ = 0;

  public:
	MmapStorage() = default;
	explicit MmapStorage(size_t cap) : data_(lazy_allocate<T>(max_capacity()))
	{
		assert(cap < max_capacity());
	}
	~MmapStorage() { std::destroy_n(data_.data(), size_); }

	MmapStorage(MmapStorage const &other) = delete;
	MmapStorage &operator=(MmapStorage const &other) = delete;

	void swap(MmapStorage &other) noexcept
	{
		using std::swap;
		swap(data_, other.data_);
		swap(size_, other.size_);
	}

	size_t size() const noexcept { return size_; }
	void set_size(size_t s) noexcept { size_ = s; }
	size_t capacity() const noexcept { return data_ ? max_capacity() : 0; }
	static constexpr size_t max_capacity() { return N; }
	T *data() noexcept { return data_.data(); }
	T const *data() const noexcept { return data_.data(); }
};

template <typename T> class IndirectStorage
{
	struct Header
	{
		size_t capacity_;
		size_t size_;
	};

	static_assert(alignof(T) <= alignof(std::max_align_t));
	static_assert(alignof(T) <= sizeof(Header));

	Header *header_ = nullptr;

  public:
	IndirectStorage() = default;
	explicit IndirectStorage(size_t cap)
	{
		if (cap == 0)
			return;
		header_ = (Header *)std::malloc(sizeof(Header) + cap * sizeof(T));
		header_->capacity_ = cap;
		header_->size_ = 0;
	}

	~IndirectStorage()
	{
		if (header_ == nullptr)
			return;
		std::destroy_n(data(), size());
		std::free(header_);
	}

	IndirectStorage(IndirectStorage const &other) = delete;
	IndirectStorage &operator=(IndirectStorage const &other) = delete;

	void swap(IndirectStorage &other) noexcept
	{
		std::swap(header_, other.header_);
	}

	size_t size() const noexcept
	{
		if (header_ == nullptr)
			return 0;
		else
			return header_->size_;
	}

	void set_size(size_t s) noexcept
	{
		if (header_ == nullptr)
			assert(s == 0);
		else
			header_->size_ = s;
	}

	size_t capacity() const noexcept
	{
		if (header_ == nullptr)
			return 0;
		else
			return header_->capacity_;
	}

	static constexpr size_t max_capacity() { return SIZE_MAX / sizeof(T); }
	T *data() noexcept
	{
		if (header_ == nullptr)
			return nullptr;
		else
			return reinterpret_cast<T *>(header_ + 1);
	}

	T const *data() const noexcept
	{
		if (header_ == nullptr)
			return nullptr;
		else
			return reinterpret_cast<T *>(header_ + 1);
	}
};

template <class T, class Impl> class Vector
{
	// No strange types allowed in current implementation. If necessary, use
	// an additional level of indirection, e.g., something like
	//     vector<unique_ptr<T>>    or    vector<reference_wrapper<T>>
	static_assert(std::is_nothrow_move_constructible_v<T>);
	static_assert(std::is_nothrow_move_assignable_v<T>);
	static_assert(std::is_nothrow_destructible_v<T>);
	static_assert(std::is_same_v<T, std::decay_t<T>>);

	Impl impl_;

  public:
	// type aliases

	using value_type = T;
	using size_type = size_t;
	using difference_type = ptrdiff_t;
	using reference = T &;
	using const_reference = const T &;
	using pointer = T *;
	using const_pointer = const T *;
	using iterator = T *;
	using const_iterator = const T *;

	// default/copy/move constructor

	Vector() = default;

	Vector(Vector const &other) : impl_(other.size())
	{
		std::uninitialized_copy_n(other.data(), other.size(), data());
		set_size_unsafe(other.size());
	}

	Vector(Vector &&other) noexcept : Vector() { swap(other); }

	// constructor from data

	Vector(size_t count, T const &value) : impl_(count)
	{
		std::uninitialized_fill_n(data(), count, value);
		set_size_unsafe(count);
	}

	explicit Vector(size_t count) : Vector(count, T()) {}

	template <class It,
	          class = typename std::iterator_traits<It>::iterator_category>
	Vector(It first, It last) : impl_(std::distance(first, last))
	{
		std::uninitialized_copy(first, last, data());
		set_size_unsafe(std::distance(first, last));
	}

	Vector(std::initializer_list<T> init) : Vector(init.begin(), init.end()) {}

	// assignment operator

	Vector &operator=(Vector const &other)
	{
		if (this != &other)
			assign(other.begin(), other.end());
		return *this;
	}

	Vector &operator=(Vector &&other) noexcept
	{
		clear(); // not obvious what is reasonable here
		swap(other);
		return *this;
	}

	Vector &operator=(std::initializer_list<T> ilist)
	{
		assign(ilist.begin(), ilist.end());
		return *this;
	}

	// assign() replaces the whole content of the vector

	void assign(size_t count, T const &value)
	{
		// design choice if there are existing elements:
		//     1) copy-assign to the existing elements
		//     2) destroy everything and copy-construct new elements
		//     3) do (1) if capacity is sufficient, and (2) if not. This version
		//        avoids moving elements that are about to be overwritten
		// this implementation does (3).

		if (count <= size())
		{
			std::fill_n(data(), count, value);
			std::destroy_n(data() + count, size() - count);
			set_size_unsafe(count);
		}
		else if (count <= capacity())
		{
			std::fill_n(data(), size(), value);
			std::uninitialized_fill_n(data() + size(), count - size(), value);
			set_size_unsafe(count);
		}
		else
		{
			// at this point, clang and gcc reserve exactly matching, while msvs
			// reserves spare capacity. We do the former for now.
			auto new_impl = Impl(count);
			std::uninitialized_fill_n(new_impl.data(), count, value);
			new_impl.set_size(count);
			impl_.swap(new_impl);
		}
	}

	template <class It,
	          class = typename std::iterator_traits<It>::iterator_category>
	void assign(It first, It last)
	{
		// see assign(count, value) for details

		size_t count = std::distance(first, last);
		if (count <= size())
		{
			std::copy_n(first, count, data());
			std::destroy_n(data() + count, size() - count);
			set_size_unsafe(count);
		}
		else if (count <= capacity())
		{
			std::copy_n(first, size(), data());
			std::uninitialized_copy_n(first + size(), count - size(),
			                          data() + size());
			set_size_unsafe(count);
		}
		else
		{
			auto new_impl = Impl(count);
			std::uninitialized_copy_n(first, count, new_impl.data());
			new_impl.set_size(count);
			impl_.swap(new_impl);
		}
	}

	void assign(std::initializer_list<T> ilist)
	{
		assign(ilist.begin(), ilist.end());
	}

	// indexing and iterators

	T &at(size_t i)
	{
		if (i >= size())
			throw std::out_of_range("util::Vector index out of bounds");
		return data()[i];
	}
	T const &at(size_t i) const
	{
		if (i >= size())
			throw std::out_of_range("util::Vector index out of bounds");
		return data()[i];
	}
	T &operator[](size_t i) noexcept { return data()[i]; }
	T const &operator[](size_t i) const noexcept { return data()[i]; }

	T &front() noexcept { return data()[0]; }
	T const &front() const noexcept { return data()[0]; }
	T &back() noexcept { return data()[size() - 1]; }
	T const &back() const noexcept { return data()[size() - 1]; }
	T *data() { return impl_.data(); }
	T const *data() const { return impl_.data(); }

	T *begin() noexcept { return data(); }
	T const *begin() const noexcept { return data(); }
	T const *cbegin() const noexcept { return data(); }
	T *end() noexcept { return data() + size(); }
	T const *end() const noexcept { return data() + size(); }
	T const *cend() const noexcept { return data() + size(); }
	// TODO: reverse iterators

	// size metrics

	bool empty() const noexcept { return size() == 0; }
	size_t size() const noexcept { return impl_.size(); }
	static constexpr size_t max_size() { return Impl::max_capacity(); }
	size_t capacity() const noexcept { return impl_.capacity(); }

	// Change the size without constructing/destrocuting/allocating
	// anything. This circumvents the value semantics of this conainer, so
	// be very careful. In rare cases useful for optimizing some low level
	// routines.
	void set_size_unsafe(size_t s) noexcept { impl_.set_size(s); }

	// Increase capacity to at least new_cap. If spare=true, capacity is
	// increased at least geometrically
	void reserve(size_t new_cap, bool spare = false)
	{
		if (new_cap <= capacity())
			return;
		assert(new_cap <= max_size());

		if (spare)
			new_cap = std::clamp(2 * capacity(), new_cap, max_size());

		auto new_impl = Impl(new_cap);
		uninitialized_relocate_n(data(), size(), new_impl.data());
		new_impl.set_size(size());
		impl_.set_size(0);
		impl_.swap(new_impl);
	}

	// remove all elements, keeping allocated capacity
	void clear() noexcept
	{
		std::destroy_n(data(), size());
		set_size_unsafe(0);
	}

	// change size. either truncating, or filling with value
	void resize(size_t s, T const &value = T())
	{
		if (s <= size())
		{
			std::destroy_n(data() + s, size() - s);
			impl_.set_size(s);
		}
		else
		{
			reserve(s, true);
			std::uninitialized_fill_n(data() + size(), s - size(), value);
			set_size_unsafe(s);
		}
	}

	// insert at arbitrary position
	//     * strong exception guarantee
	//     * aliasing not allowed
	iterator insert(size_t pos, T const &value)
	{
		reserve(size() + 1, true);
		std::construct_at(end(), value);
		set_size_unsafe(size() + 1);
		std::rotate(begin() + pos, end() - 1, end());
		return begin() + pos;
	}
	template <typename It> iterator insert(size_t pos, It first, It last)
	{
		size_t count = std::distance(first, last);
		reserve(size() + count, true);
		std::uninitialized_copy_n(first, count, end());
		set_size_unsafe(size() + count);
		std::rotate(begin() + pos, end() - count, end());
		return begin() + pos;
	}
	iterator insert(size_t pos, std::initializer_list<T> ilist)
	{
		return insert(pos, ilist.begin(), ilist.end());
	}
	iterator insert(const_iterator pos, T const &value)
	{
		return insert(std::distance(cbegin(), pos), value);
	}
	template <typename It>
	iterator insert(const_iterator pos, It first, It last)
	{
		return insert(std::distance(cbegin(), pos), first, last);
	}
	iterator insert(const_iterator pos, std::initializer_list<T> ilist)
	{
		return insert(std::distance(cbegin(), pos), ilist);
	}

	// add element at the back
	//     * strong exception guarantee
	//     * aliasing is allowed as in 'a.push_back(a[0])'
	template <class... Args> void emplace_back(Args &&...args)
	{
		if (size() == capacity())
		{
			// careful order of operations to ensure strong exception guarantee
			// and allow aliasing at the same time
			auto new_impl = Impl(capacity() ? 2 * capacity() : 1);
			std::construct_at(new_impl.data() + size(),
			                  std::forward<Args>(args)...);
			uninitialized_relocate_n(data(), size(), new_impl.data());
			new_impl.set_size(size() + 1);
			set_size_unsafe(0);
			impl_.swap(new_impl);
		}
		else
		{
			std::construct_at(end(), std::forward<Args>(args)...);
			set_size_unsafe(size() + 1);
		}
	}

	void push_back(T const &value) { emplace_back(value); }
	void push_back(T &&value) { emplace_back(std::move(value)); }

	T pop_back()
	{
		T r = relocate(&back());
		set_size_unsafe(size() - 1);
		return r;
	}

	// erase at arbitrary position
	void erase(iterator first, iterator last) noexcept
	{
		size_t count = std::distance(first, last);
		std::move(last, end(), first);
		std::destroy(end() - count, end());
		set_size_unsafe(size() - count);
	}
	void erase(iterator pos) { erase(pos, pos + 1); }

	// swap with another vector

	void swap(Vector &other) noexcept { impl_.swap(other.impl_); }
	friend void swap(Vector &a, Vector &b) noexcept { a.swap(b); }
};

template <typename T, class Impl, typename T2, class Impl2>
bool operator==(Vector<T, Impl> const &a, Vector<T2, Impl2> const &b) noexcept
{
	if (a.size() != b.size())
		return false;
	for (size_t i = 0; i < a.size(); ++i)
		if (!(a[i] == b[i]))
			return false;
	return true;
}

template <typename T, class Impl, typename T2, class Impl2>
auto operator<=>(Vector<T, Impl> const &a, Vector<T2, Impl2> const &b) noexcept
{
	return std::lexicographical_compare_three_way(a.begin(), a.end(), b.begin(),
	                                              b.end());
}

// short-form of erase-remove idiom
template <class T, class Impl, class U>
size_t erase(Vector<T, Impl> &c, const U &value)
{
	auto it = std::remove_if(c.begin(), c.end(), value);
	auto r = std::distance(it, c.end());
	c.erase(it, c.end());
	return r;
}

// short-form of erase-remove idiom
template <class T, class Impl, class Pred>
size_t erase_if(Vector<T, Impl> &c, Pred pred)
{
	auto it = std::remove_if(c.begin(), c.end(), pred);
	auto r = std::distance(it, c.end());
	c.erase(it, c.end());
	return r;
}

// sort and remove duplicates
template <class T, class Impl> void unique_sort(Vector<T, Impl> &c)
{
	std::sort(c.begin(), c.end());
	c.erase(std::unique(c.begin(), c.end()), c.end());
}

// sort and remove duplicates
template <class T, class Impl, class Pred>
void unique_sort(Vector<T, Impl> &c, Pred pred)
{
	auto not_pred = [&pred](T const &a, T const &b) { return !pred(a, b); };
	std::sort(c.begin(), c.end(), pred);
	c.erase(std::unique(c.begin(), c.end(), not_pred), c.end());
}

} // namespace detail

// malloc-based vector, essentially equivalent to std::vector
template <typename T>
using vector = detail::Vector<T, detail::MallocStorage<T>>;

// vector with "small buffer optimization"
//   - sizes <= N are stored inline without any heap allocation
//   - size and capacity are stored as uint32_t instead of size_t
//   - buffer shares space with the data pointer and capacity. This is in
//     contrast to many other small-vector implementations like LLVM and Boost.
template <typename T, size_t N>
using small_vector = detail::Vector<T, detail::SboStorage<T, N>>;

// fixed capacity of N, no dynamic memory allocation at all
template <typename T, size_t N>
using static_vector = detail::Vector<T, detail::StaticStorage<T, N>>;

// vector with stable pointers and iterators (except for end()), implemented
// with lazy/over-commited memory allocation using mmap(). The size (2^36 bytes)
// is somewhat arbitrary. Ideally larger than physical memory, and
// (significantly) smaller than virtual adress space. Note that the latter is
// "only" 2^48 bytes on many 64bit platforms (and not sure how much the OS would
// be willing to give us in a single mmap()).
template <typename T, size_t N = (1LL << 36) / sizeof(T)>
using stable_vector = detail::Vector<T, detail::MmapStorage<T, N>>;

// vector that stores its size and capacity inside the allocation. The struct
// itself contains only a single pointer, thus very space efficient in case the
// vector is typically empty anyway.
// TODO: if T itself is a pointer type, we could do some bit stuffing to get a
//       N=1 small-object-optimization going.
template <typename T>
using indirect_vector = detail::Vector<T, detail::IndirectStorage<T>>;

// Associative container implemented as unsorted vector. For sufficiently small
// datasets this should be the most efficient datastructure. Furthermore:
//     * needs neither a hash nor a total ordering, only equality comparisons
//     * elements are kept in order of insertion
//     * TODO: make it actually work with util::vector and friends
template <class Key, class T,
          class Container = std::vector<std::pair<const Key, T>>>
class tiny_map
{
	Container values_;

  public:
	// types
	using container_type = Container;
	using key_type = Key;
	using mapped_type = T;
	using value_type = std::pair<const Key, T>;
	using size_type = size_t;
	using difference_type = ptrdiff_t;
	using reference = value_type &;
	using const_reference = value_type const &;
	using pointer = value_type *;
	using const_pointer = value_type const *;
	using iterator = typename container_type::iterator;
	using const_iterator = typename container_type::const_iterator;
	static_assert(
	    std::is_same_v<typename container_type::value_type, value_type>);

	// metrics
	bool empty() const noexcept { return values_.empty(); }
	size_t size() const noexcept { return values_.size(); }
	size_t capacity() const noexcept { return values_.capacity(); }
	size_t max_size() const noexcept { return values_.max_size(); }
	void reserve(size_t cap) { values_.reserve(cap); }

	// iterators
	iterator begin() noexcept { return values_.begin(); }
	const_iterator begin() const noexcept { return values_.begin(); }
	const_iterator cbegin() const noexcept { return values_.begin(); }
	iterator end() noexcept { return values_.end(); }
	const_iterator end() const noexcept { return values_.end(); }
	const_iterator cend() const noexcept { return values_.end(); }

	// element access
	T &at(in_param<Key> key)
	{
		if (auto it = find(key); it != end())
			return it->second;
		throw std::out_of_range("key not found in util::hash_map");
	}
	T const &at(in_param<Key> key) const
	{
		if (auto it = find(key); it != end())
			return it->second;
		throw std::out_of_range("key not found in util::hash_map");
	}
	T &operator[](in_param<Key> key)
	{
		if (auto it = find(key); it != end())
			return it->second;
		values_.emplace_back(key, T());
		return values_.back().second;
	}
	T const &operator[](in_param<Key> key) const noexcept
	{
		return find(key)->second;
	}

	// lookup
	iterator find(in_param<Key> key)
	{
		return std::find_if(begin(), end(),
		                    [&](auto &a) { return a.first == key; });
	}
	const_iterator find(in_param<Key> key) const
	{
		return std::find_if(begin(), end(),
		                    [&](auto &a) { return a.first == key; });
	}
	bool contains(in_param<Key> key) const noexcept
	{
		return find(key) != end();
	}
	size_t count(in_param<Key> key) const noexcept { return contains(key); }

	// misc
	void clear() noexcept { values_.clear(); }
	void swap(tiny_map &other) noexcept { values_.swap(other.values_); };
	friend void swap(tiny_map &a, tiny_map &b) { a.swap(b); }
};

} // namespace util
