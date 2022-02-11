#pragma once

#include <cassert>
#include <cstdint>
#include <cstring>
#include <type_traits>

namespace util {

/**
 * Similar to std::vector, but uses fixed capacity with embedded storage
 *   - current implementation is sub-optimal for non-trivial types
 */
template <typename T, size_t N> class static_vector
{
	static_assert(std::is_trivially_copyable_v<T>);

	using size_type_internal = uint8_t; // could adjust according to N
	size_type_internal size_ = 0;
	T data_[N];

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

	/** constructors */
	static_vector() noexcept = default;
	explicit static_vector(size_t count) { resize(count); }
	static_vector(size_t count, T const &value) { resize(count, value); }
	template <typename It> static_vector(It a, It b) { assign(a, b); }
	static_vector(std::initializer_list<T> ilist) { assign(ilist); }

	/** assignments */
	void assign(size_t count, T const &value)
	{
		clear();
		resize(count, value);
	}
	template <class It> void assign(It first, It last)
	{
		clear();
		while (first != last)
			push_back(*first++);
	}
	void assign(std::initializer_list<T> ilist)
	{
		clear();
		reserve(ilist.size());
		for (size_t i = 0; i < ilist.size(); ++i)
			data()[i] = std::data(ilist)[i];
		size_ = (size_type_internal)ilist.size();
	}
	static_vector &operator=(std::initializer_list<T> ilist)
	{
		assign(ilist);
		return *this;
	}

	/** size_metrics */
	bool empty() const { return size_ == 0; }
	size_t size() const { return size_; }
	size_t max_size() const { return N; }
	size_t capacity() const { return N; }

	/** element access */
	T &operator[](size_t i) { return data()[i]; }
	const T &operator[](size_t i) const { return data()[i]; }
	T &at(size_t i)
	{
		if (i >= size())
			throw std::out_of_range("util::static_vector index out of bounds");
		return data()[i];
	}
	T const &at(size_t i) const
	{
		if (i >= size())
			throw std::out_of_range("util::static_vector index out of bounds");
		return data()[i];
	}
	T &front() { return data()[0]; }
	const T &front() const { return data()[0]; }
	T &back() { return data()[size_ - 1]; }
	const T &back() const { return data()[size_ - 1]; }

	/** iterators and raw data access */
	T *begin() { return data(); }
	T const *begin() const { return data(); }
	T const *cbegin() const { return data(); }
	T *end() { return data() + size(); }
	T const *end() const { return data() + size(); }
	T const *cend() const { return data() + size(); }
	T *data() { return data_; }
	const T *data() const { return data_; }

	/** add/remove elements */
	void push_back(const T &value)
	{
		*end() = value;
		size_ += 1;
	}
	void pop_back() { size_ -= 1; }

	void erase(size_t i)
	{
		std::memmove(data() + i, data() + i + 1, (size_ - i - 1) * sizeof(T));
		size_ -= 1;
	}
	void erase(const T *a)
	{
		std::memmove((T *)a, (T *)a + 1, (end() - a - 1) * sizeof(T));
		size_ -= 1;
	}
	void erase(const T *a, const T *b)
	{
		std::memmove((T *)a, b, (end() - b) * sizeof(T));
		size_ -= b - a;
	}

	/** other operations */
	void reserve(size_t new_cap) { assert(new_cap <= N); }
	void clear() { resize(0); }
	void resize(size_t new_size, const T &value = T())
	{
		if (new_size > size_)
		{
			assert(new_size <= capacity());
			for (size_t i = size_; i < new_size; ++i)
				data()[i] = value;
		}
		size_ = (size_type_internal)new_size;
	}
	void shrink_to_fit() {} // do nothing
};

template <typename T, size_t N, typename U, size_t M>
bool operator==(static_vector<T, N> const &a, static_vector<U, M> const &b)
{
	if (a.size() != b.size())
		return false;
	for (size_t i = 0; i < a.size(); ++i)
		if (!(a[i] == b[i]))
			return false;
	return true;
}

template <typename T, size_t N, typename U>
size_t erase(static_vector<T, N> &c, const U &value)
{
	auto it = std::remove_if(c.begin(), c.end(), value);
	auto r = std::distance(it, c.end());
	c.erase(it, c.end());
	return r;
}

template <typename T, size_t N, typename Pred>
size_t erase_if(static_vector<T, N> &c, Pred pred)
{
	auto it = std::remove_if(c.begin(), c.end(), pred);
	auto r = std::distance(it, c.end());
	c.erase(it, c.end());
	return r;
}

} // namespace util
