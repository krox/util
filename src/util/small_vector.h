#pragma once

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <iterator>
#include <type_traits>

namespace util {

/**
 * Similar to std::vector, but optimized for 'usually small' sizes
 *   - sizes <= N are stored inline without any heap allocation
 *   - size and capacity are stored as uint32_t instead of size_t
 *   - inline-storage shares space with the data pointer. This is in contrast
 *     to many other small-vector implementations like LLVM and Boost.
 *   - currently only implemented for trivially copyable types
 */
template <typename T, size_t N> class small_vector
{
	static_assert(std::is_trivially_copyable_v<T>);

	uint32_t size_ = 0;
	uint32_t capacity_ = (uint32_t)N;
	union
	{
		T *data_;
		T store_[N];
	};

  public:
	/** type aliases */
	using value_type = T;
	using size_type = size_t;
	using difference_type = ptrdiff_t;
	using reference = T &;
	using const_reference = const T &;
	using pointer = T *;
	using const_pointer = const T *;
	using iterator = T *;
	using const_iterator = const T *;

	// constructors
	small_vector() = default;
	explicit small_vector(size_t count) { resize(count); }
	small_vector(size_t count, const T &value) { resize(count, value); }

	// special member functions
	~small_vector()
	{
		if (!is_small())
			delete[] data_;
	}
	small_vector(small_vector const &other)
	{
		resize(other.size());
		std::memcpy(data(), other.data(), sizeof(T) * size_);
	}
	small_vector(small_vector &&other) noexcept
	{
		std::memcpy(this, &other, sizeof(small_vector));
		other.size_ = 0;
		other.capacity_ = (uint32_t)N;
	}
	small_vector &operator=(const small_vector &other)
	{
		resize(other.size());
		std::memcpy(data(), other.data(), sizeof(T) * size_);
		return *this;
	}
	small_vector &operator=(small_vector &&other) noexcept
	{
		swap(*this, other);
		return *this;
	}
	friend void swap(small_vector &a, small_vector &b) noexcept
	{
		constexpr size_t s = sizeof(small_vector);
		uint8_t buf[s];
		std::memcpy(buf, &a, s);
		std::memcpy(&a, &b, s);
		std::memcpy(&b, buf, a);
	}

	// size_metrics
	bool empty() const { return size_ == 0; }
	size_t size() const { return size_; }
	size_t max_size() const { return UINT32_MAX; }
	size_t capacity() const { return capacity_; }
	bool is_small() const { return capacity_ == N; }

	// element access
	T &operator[](size_t i) { return data()[i]; }
	T const &operator[](size_t i) const { return data()[i]; }
	T &at(size_t i)
	{
		if (i >= size_)
			throw std::out_of_range("small_vector index out of range");
		return data()[i];
	}
	T const &at(size_t i) const
	{
		if (i >= size_)
			throw std::out_of_range("small_vector index out of range");
		return data()[i];
	}
	T &front() { return data()[0]; }
	T const &front() const { return data()[0]; }
	T &back() { return data()[size_ - 1]; }
	T const &back() const { return data()[size_ - 1]; }

	/** iterators and raw data access */
	T *begin() { return data(); }
	const T *begin() const { return data(); }
	const T *cbegin() const { return data(); }
	T *end() { return data() + size_; }
	const T *end() const { return data() + size_; }
	const T *cend() const { return data() + size_; }
	T *data()
	{
		if (is_small())
			return store_;
		else
			return data_;
	}
	const T *data() const
	{
		if (is_small())
			return store_;
		else
			return data_;
	}

	// add/remove at the back
	void push_back(T const &value)
	{
		if (size_ == capacity_)
			reserve(capacity_ * 2);
		*end() = value;
		size_ += 1;
	}
	void pop_back() { size_ -= 1; }

	// insert at arbitrary position
	iterator insert(size_t pos, T const &value)
	{
		if (size_ == capacity_)
			reserve(capacity_ * 2);
		std::move_backward(begin() + pos, end(), end() + 1);
		(*this)[pos] = value;
		size_ += 1;
		return begin() + pos;
	}
	template <typename It> iterator insert(size_t pos, It first, It last)
	{
		size_t count = std::distance(first, last);
		if (size_ + count > capacity())
			reserve(std::max(capacity() * 2, size_ + count));
		std::move_backward(begin() + pos, end(), end() + count);
		std::copy(first, last, begin() + pos);
		size_ += count;
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

	// erase at arbitrary position
	void erase(iterator first, iterator last)
	{
		std::move(last, end(), first);
		size_ -= std::distance(last - first);
	}
	void erase(iterator pos) { erase(pos, pos + 1); }

	// other operators
	void reserve(size_t new_cap)
	{
		assert(new_cap < max_size());
		if (new_cap <= capacity_)
			return;
		T *new_data = new T[new_cap];
		std::memcpy(new_data, data(), sizeof(T) * size_);
		if (!is_small())
			delete[] data_;
		capacity_ = new_cap;
		data_ = new_data;
	}
	void clear() noexcept { resize(0); }
	void resize(size_t new_size, const T &value = T())
	{
		if (new_size > size_)
		{
			reserve(new_size);
			for (size_t i = size_; i < new_size; ++i)
				data()[i] = value;
		}
		size_ = new_size;
	}
};

template <typename T, size_t N, typename U>
size_t erase(small_vector<T, N> &c, const U &value)
{
	auto it = std::remove_if(c.begin(), c.end(), value);
	auto r = std::distance(it, c.end());
	c.erase(it, c.end());
	return r;
}

template <typename T, size_t N, typename Pred>
size_t erase_if(small_vector<T, N> &c, Pred pred)
{
	auto it = std::remove_if(c.begin(), c.end(), pred);
	auto r = std::distance(it, c.end());
	c.erase(it, c.end());
	return r;
}

} // namespace util
