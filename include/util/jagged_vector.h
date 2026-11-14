#pragma once

// Vector of variable-length rows. Elements of every row live in one contiguous
// buffer; a second array stores where each row starts. size() is the number of
// rows, as in vector<vector<T>>. The total number of elements is flat_size().

#include "util/vector.h"
#include <cassert>
#include <compare>
#include <cstddef>
#include <iterator>
#include <memory>
#include <span>
#include <type_traits>

namespace util {

template <class T> class jagged_vector
{
	util::vector<T> data_;
	util::vector<size_t> pos_{0}; // pos_[i] starts row i

  public:
	using value_type = T;
	using size_type = size_t;
	using difference_type = std::ptrdiff_t;
	using reference = std::span<T>;
	using const_reference = std::span<T const>;

	template <bool IsConst> class row_iterator
	{
		using parent_type =
		    std::conditional_t<IsConst, jagged_vector const, jagged_vector>;

		parent_type *self_ = nullptr;
		size_t index_ = 0;

		template <bool> friend class row_iterator;

	  public:
		using iterator_concept = std::random_access_iterator_tag;
		using iterator_category = std::random_access_iterator_tag;
		using value_type =
		    std::conditional_t<IsConst, std::span<T const>, std::span<T>>;
		using difference_type = std::ptrdiff_t;
		using reference = value_type;

		row_iterator() = default;
		row_iterator(parent_type *self, size_t index) noexcept
		    : self_(self), index_(index)
		{}

		template <bool Other>
		    requires(IsConst && !Other)
		row_iterator(row_iterator<Other> const &other) noexcept
		    : self_(other.self_), index_(other.index_)
		{}

		reference operator*() const { return (*self_)[index_]; }
		reference operator[](difference_type n) const
		{
			return (*self_)[index_ + static_cast<size_t>(n)];
		}

		row_iterator &operator++() noexcept
		{
			++index_;
			return *this;
		}
		row_iterator operator++(int) noexcept
		{
			auto tmp = *this;
			++*this;
			return tmp;
		}
		row_iterator &operator--() noexcept
		{
			--index_;
			return *this;
		}
		row_iterator operator--(int) noexcept
		{
			auto tmp = *this;
			--*this;
			return tmp;
		}

		row_iterator &operator+=(difference_type n) noexcept
		{
			index_ =
			    static_cast<size_t>(static_cast<difference_type>(index_) + n);
			return *this;
		}
		row_iterator &operator-=(difference_type n) noexcept
		{
			return *this += -n;
		}

		friend row_iterator operator+(row_iterator it,
		                              difference_type n) noexcept
		{
			it += n;
			return it;
		}
		friend row_iterator operator+(difference_type n,
		                              row_iterator it) noexcept
		{
			return it + n;
		}
		friend row_iterator operator-(row_iterator it,
		                              difference_type n) noexcept
		{
			it -= n;
			return it;
		}
		friend difference_type operator-(row_iterator a,
		                                 row_iterator b) noexcept
		{
			return static_cast<difference_type>(a.index_) -
			       static_cast<difference_type>(b.index_);
		}

		friend bool operator==(row_iterator a, row_iterator b) noexcept
		{
			return a.index_ == b.index_;
		}
		friend std::strong_ordering operator<=>(row_iterator a,
		                                        row_iterator b) noexcept
		{
			return a.index_ <=> b.index_;
		}
	};

	using iterator = row_iterator<false>;
	using const_iterator = row_iterator<true>;

	jagged_vector() = default;

	size_t size() const noexcept { return pos_.size() - 1; }
	bool empty() const noexcept { return pos_.size() == 1; }
	size_t flat_size() const noexcept { return data_.size(); }

	void reserve(size_t rows, size_t elements)
	{
		pos_.reserve(rows + 1);
		data_.reserve(elements);
	}

	void clear() noexcept
	{
		data_.clear();
		pos_.resize(1);
	}

	T *data() noexcept { return data_.data(); }
	T const *data() const noexcept { return data_.data(); }

	std::span<T> flat() noexcept { return {data_.data(), data_.size()}; }
	std::span<T const> flat() const noexcept
	{
		return {data_.data(), data_.size()};
	}

	std::span<T> operator[](size_t i) noexcept
	{
		assert(i < size());
		return {data_.data() + pos_[i], pos_[i + 1] - pos_[i]};
	}
	std::span<T const> operator[](size_t i) const noexcept
	{
		assert(i < size());
		return {data_.data() + pos_[i], pos_[i + 1] - pos_[i]};
	}

	std::span<T> front() noexcept { return (*this)[0]; }
	std::span<T const> front() const noexcept { return (*this)[0]; }
	std::span<T> back() noexcept { return (*this)[size() - 1]; }
	std::span<T const> back() const noexcept { return (*this)[size() - 1]; }

	iterator begin() noexcept { return {this, 0}; }
	iterator end() noexcept { return {this, size()}; }
	const_iterator begin() const noexcept { return {this, 0}; }
	const_iterator end() const noexcept { return {this, size()}; }
	const_iterator cbegin() const noexcept { return begin(); }
	const_iterator cend() const noexcept { return end(); }

	// Copies the elements. The source must not alias this container.
	void push_back(std::span<T const> row)
	{
		data_.reserve_with_spare(data_.size() + row.size());
		pos_.reserve_with_spare(pos_.size() + 1);
		std::uninitialized_copy(row.begin(), row.end(), data_.end());
		data_.set_size_unsafe(data_.size() + row.size());
		pos_.push_back(data_.size());
	}

	// New row of 'n' copies of 'val'. 'n' may be zero. 'val' must not alias
	// this container.
	void push_back(size_t n, T const &val)
	{
		data_.reserve_with_spare(data_.size() + n);
		pos_.reserve_with_spare(pos_.size() + 1);
		std::uninitialized_fill_n(data_.end(), n, val);
		data_.set_size_unsafe(data_.size() + n);
		pos_.push_back(data_.size());
	}

	void pop_back()
	{
		assert(!empty());
		size_t new_end = pos_[pos_.size() - 2];
		std::destroy(data_.data() + new_end, data_.data() + data_.size());
		data_.set_size_unsafe(new_end);
		pos_.pop_back();
	}

	friend bool operator==(jagged_vector const &a, jagged_vector const &b)
	{
		return a.data_ == b.data_ && a.pos_ == b.pos_;
	}
};

} // namespace util
