#pragma once

#include "util/span.h"
#include <cassert>

namespace util {
template <typename T> class vector2d
{
	std::vector<T> data_;
	size_t height_ = 0;
	size_t width_ = -1;

  public:
	/** constructor */
	vector2d() {}

	/** size metrics */
	bool empty() const { return height_ == 0; }
	size_t height() const { return height_; }
	size_t width() const { return width_; }
	size_t size() const { return height_ * width_; }

	/** row access */
	std::span<T> row(size_t i)
	{
		return std::span<T>(data_.data() + width_ * i, width_);
	}
	std::span<const T> row(size_t i) const
	{
		return std::span<const T>(data_.data() + width_ * i, width_);
	}
	std::span<T> operator[](size_t i) { return row(i); }
	std::span<const T> operator[](size_t i) const { return row(i); }
	std::span<T> operator()(size_t i) { return row(i); }
	std::span<const T> operator()(size_t i) const { return row(i); }

	std::span<T> front() { return row(0); }
	std::span<T> back() { return row(height_ - 1); }
	std::span<const T> front() const { return row(0); }
	std::span<const T> back() const { return row(height_ - 1); }

	/** column access */
	gspan<T> col(size_t j)
	{
		return gspan<T>(data_.data() + j, height_, width_);
	}
	gspan<const T> col(size_t j) const
	{
		return gspan<const T>(data_.data() + j, height_, width_);
	}

	/** element access */
	T &operator()(size_t i, size_t j) { return data_[width_ * i + j]; }
	const T &operator()(size_t i, size_t j) const
	{
		return data_[width_ * i + j];
	}

	/** access as one-dimensional span */
	std::span<T> flat() { return data_; }
	std::span<const T> flat() const { return data_; }
	T &flat(size_t i) { return data_[i]; }
	const T &flat(size_t i) const { return data_[i]; }

	/** add one row at bottom */
	void push_back(std::span<const T> v)
	{
		if (width_ == (size_t)-1)
		{
			assert(v.size() > 0);
			assert(data_.size() == 0);
			width_ = v.size();
		}

		assert(v.size() == width_);
		for (size_t i = 0; i < width_; ++i)
			data_.push_back(v[i]);
		height_ += 1;
	}

	/** remove one row from bottom */
	void pop_back()
	{
		assert(height_ > 0);
		height_ -= 1;
		data_.resize(width_ * height_);
	}
};

} // namespace util
