#ifndef UTIL_SMALL_VECTOR_H
#define UTIL_SMALL_VECTOR_H

#include <cstdint>
#include <cstring>
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

	/** constructor/destructor */
	small_vector(){};
	explicit small_vector(size_t count) { resize(count); }
	small_vector(size_t count, const T &value) { resize(count, value); }
	~small_vector()
	{
		if (!is_small())
			delete[] data_;
	}

	/** copy/move constructor/assignment */
	small_vector(const small_vector &other)
	{
		resize(other.size());
		std::memcpy(data(), other.data(), sizeof(T) * size_);
	}
	small_vector(small_vector &&other)
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
	small_vector &operator=(small_vector &&other)
	{
		if (!is_small())
			delete[] data_;
		std::memcpy(this, &other, sizeof(small_vector));
		other.size_ = 0;
		other.capacity_ = (uint32_t)N;
	}

	/** size_metrics */
	bool empty() const { return size_ == 0; }
	size_t size() const { return size_; }
	size_t max_size() const { return UINT32_MAX; }
	size_t capacity() const { return capacity_; }
	bool is_small() const { return capacity_ == N; }

	/** element access */
	T &operator[](size_t i) { return data()[i]; }
	const T &operator[](size_t i) const { return data()[i]; }
	T &front() { return data()[0]; }
	const T &front() const { return data()[0]; }
	T &back() { return data()[size_ - 1]; }
	const T &back() const { return data()[size_ - 1]; }

	/** iterators and raw data access */
	T *begin() { return data(); }
	const T *begin() const { return data(); }
	T *end() { return data() + size_; }
	const T *end() const { return data() + size_; }
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

	/** add/remove elements */
	void push_back(const T &value)
	{
		if (size_ == capacity_)
			reserve(capacity_ * 2);
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
	void clear() { resize(0); }
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

namespace std {
template <typename T, size_t N>
inline void swap(util::small_vector<T, N> &a, util::small_vector<T, N> &b)
{
	a.swap(b);
	constexpr size_t s = sizeof(small_vector<T, N>);
	uint8_t buf[s];
	std::memcpy(buf, &a, s);
	std::memcpy(&a, &b, s);
	std::memcpy(&b, buf, a);
}

} // namespace std

#endif
