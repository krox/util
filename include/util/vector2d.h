#pragma once

#include "util/span.h"
#include <cassert>

namespace util {

// 2-dimensional vector, where full rows are added at once by '.push_back'. An
// empty vector will deduce the width from the first row added, after which all
// rows must have the same width.
template <class T> class vector2d
{
	// invariant: data_.size() == height_ * width_
	// (still nice to have all three available directly)
	util::vector<T> data_;
	size_t height_ = 0;
	size_t width_ = 0;

  public:
	// default to empty
	vector2d() = default;

	// accesors
	// note: .width() of an empty vector is unspecified
	bool empty() const noexcept { return height_ == 0; }
	size_t height() const noexcept { return height_; }
	size_t width() const noexcept { return width_; }
	size_t size() const noexcept { return height_ * width_; }

	// single-row access
	std::span<T> row(size_t i) noexcept
	{
		return {data_.data() + width_ * i, width_};
	}
	std::span<const T> row(size_t i) const noexcept
	{
		return {data_.data() + width_ * i, width_};
	}
	std::span<T> operator[](size_t i) { return row(i); }
	std::span<const T> operator[](size_t i) const { return row(i); }

	std::span<T> front() { return row(0); }
	std::span<T> back() { return row(height_ - 1); }
	std::span<const T> front() const { return row(0); }
	std::span<const T> back() const { return row(height_ - 1); }

	// column access
	gspan<T> col(size_t j) noexcept
	{
		return gspan<T>(data_.data() + j, height_, width_);
	}
	gspan<const T> col(size_t j) const noexcept
	{
		return gspan<const T>(data_.data() + j, height_, width_);
	}

	// single-element access
	T &operator()(size_t i, size_t j) { return data_[width_ * i + j]; }
	const T &operator()(size_t i, size_t j) const
	{
		return data_[width_ * i + j];
	}

	// access as one-dimensional (row-major) array
	std::span<T> flat() noexcept { return data_; }
	std::span<const T> flat() const noexcept { return data_; }
	T &flat(size_t i) noexcept { return data_[i]; }
	const T &flat(size_t i) const noexcept { return data_[i]; }

	// add one row at bottom. If the vector was non-empty, the width of the new
	// row must match the width of the existing rows.
	// * aliasing is not allowed (UB), e.g., 'v.push_back(v.back())'
	void push_back(std::span<const T> v)
	{
		assert(v.size() > 0); // maybe this could be relaxed consistently
		if (height_ == 0)
			width_ = v.size();
		assert(v.size() == width_);

		data_.reserve_with_spare(data_.size() + width_);
		std::uninitialized_copy(v.begin(), v.end(), data_.end());
		data_.set_size_unsafe(data_.size() + width_);
		height_ += 1;
	}

	// remove one row from bottom
	void pop_back() noexcept
	{
		assert(height_ > 0);
		data_.resize(width_ * height_);
		height_ -= 1;
	}

	// remove all elements, keep capacity
	void clear() noexcept
	{
		data_.clear();
		height_ = 0;
	}
};

} // namespace util
