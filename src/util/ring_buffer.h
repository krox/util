#pragma once

#include "util/memory.h"

namespace util {

template <class T> class ring_iterator
{
	// Storing both 'head_' and 'index_' might seem redundant, but there is an
	// annoying edge case (differentiating between the begin() and end() of a
	// completely full ring buffer) that causes problems otherwise.
	std::span<T> data_;
	size_t head_;
	size_t index_; // counted from 'head_', not from data_.begin()

  public:
	using iterator_category = std::random_access_iterator_tag;
	using value_type = T;
	using difference_type = std::ptrdiff_t;
	using pointer = value_type *;
	using reference = value_type &;

	ring_iterator(std::span<T> data, size_t head, size_t index) noexcept
	    : data_(data), head_(head), index_(index)
	{}

	value_type &operator*() const noexcept
	{
		return data_[(head_ + index_) % data_.size()];
	}
	value_type *operator->() const noexcept
	{
		return data_ + (head_ + index_) % data_.size();
	}

	bool operator==(ring_iterator other) const noexcept
	{
		// assumes both refer to the same ring buffer...
		return index_ == other.index_;
	}
	ring_iterator &operator++() noexcept
	{
		++index_;
		return *this;
	}
	ring_iterator operator++(int) noexcept
	{
		auto r = *this;
		++*this;
		return r;
	}
};

// ring buffer with fixed maximum capacity.
//   * when pushing beyond capacity, an element is dropped from the front.
//   * "ring" refers to the implementation, indexing is not periodic.
template <class T> class fixed_ring_buffer
{
	util::unique_memory<T> data_;
	size_t size_ = 0;
	size_t head_ = 0;

  public:
	// nested types
	using value_type = T;
	using size_type = size_t;
	using difference_type = ptrdiff_t;
	using reference = T &;
	using const_reference = const T &;
	using pointer = T *;
	using const_pointer = const T *;
	using iterator = ring_iterator<T>;
	using const_iterator = ring_iterator<const T>;

	fixed_ring_buffer() = default;
	explicit fixed_ring_buffer(size_t cap) : data_(util::allocate<T>(cap)) {}
	~fixed_ring_buffer() { clear(); }

	// copy constructor/assigmnet. TODO
	fixed_ring_buffer(fixed_ring_buffer const &other) = delete;
	fixed_ring_buffer &operator=(fixed_ring_buffer const &other) = delete;

	// move constructor/assignment
	fixed_ring_buffer(fixed_ring_buffer &&other) noexcept
	    : data_(std::exchange(other.data_, {})),
	      size_(std::exchange(other.size_, 0)),
	      head_(std::exchange(other.head_, 0))
	{}
	fixed_ring_buffer &operator=(fixed_ring_buffer &&other) noexcept
	{
		if (this != &other)
		{
			clear();
			data_ = std::exchange(other.data_, {});
			size_ = std::exchange(other.size_, 0);
			head_ = std::exchange(other.head_, 0);
		}
		return *this;
	}

	// destroy contents, keep capacity
	void clear() noexcept
	{
		size_t n = std::min(size_, capacity() - head_);
		std::destroy_n(data_.data() + head_, n);
		std::destroy_n(data_.data(), size_ - n);
		size_ = 0;
		head_ = 0;
	}

	// metrics
	bool empty() const noexcept { return size_ == 0; }
	size_t size() const noexcept { return size_; }
	size_t capacity() const noexcept { return data_.size(); }

	// element access
	T &operator[](size_t i) noexcept { return data_[(head_ + i) % capacity()]; }
	T const &operator[](size_t i) const noexcept
	{
		return data_[(head_ + i) % capacity()];
	}
	T &front() noexcept { return data_[head_]; }
	T const &front() const noexcept { return data_[head_]; }
	T &back() noexcept { return data_[(head_ + size_ - 1) % capacity()]; }
	T const &back() const noexcept
	{
		return data_[(head_ + size_ - 1) % capacity()];
	}

	// iterators
	iterator begin() noexcept { return {data_.get(), head_, 0}; }
	iterator end() noexcept { return {data_.get(), head_, size_}; }
	const_iterator begin() const noexcept { return {data_.get(), head_, 0}; }
	const_iterator end() const noexcept { return {data_.get(), head_, size_}; }
	const_iterator cbegin() const noexcept { return {data_.get(), head_, 0}; }
	const_iterator cend() const noexcept { return {data_.get(), head_, size_}; }

	// push an element to the end
	template <class... Args> void emplace_back(Args &&...args)
	{
		if (size() == capacity())
		{
			// only throwing operation is the constructor of T
			static_assert(std::is_nothrow_move_assignable_v<T>);
			front() = T(std::forward<Args>(args)...);
			head_ = (head_ + 1) % capacity();
		}
		else
		{
			std::construct_at(&data_[(head_ + size_) % capacity()],
			                  std::forward<Args>(args)...);
			++size_;
		}
	}
	void push_back(T const &value) { emplace_back(value); }
	void push_back(T &&value) { emplace_back(std::move(value)); }
};
} // namespace util