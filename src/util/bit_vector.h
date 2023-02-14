#pragma once

#include "util/memory.h"
#include <bit>
#include <cassert>
#include <cstring>
#include <span>

namespace util {

/**
 * Similar to specialized std::vector<bool>, but
 *   - does not pretend to be container (no iterators),
 *     thus avoiding some common pitfalls of std::vector<bool>
 *   - offers (fast) bitwise operations
 *   - using 'add() and remove()', it can be used as a 'set of integers'
 *   - beware of different semantics. e.g. '.clear()' sets all bits to zero,
 *     whereas std::vector<bool>::clear() resizes the vector
 */
class bit_vector
{
  public:
	/** types and constants */
	using limb_t = size_t;
	static constexpr size_t limb_bits = sizeof(limb_t) * 8;

  private:
	size_t size_ = 0;                 // number of used bits
	unique_memory<limb_t> data_ = {}; // all unused bits are kept at zero

  public:
	class reference
	{
		friend class bit_vector;
		limb_t &limb_;
		limb_t mask_;
		reference(limb_t &limb, size_t pos) noexcept
		    : limb_(limb), mask_(limb_t(1) << pos)
		{}

		void operator&() = delete; // prevent misuse

	  public:
		operator bool() const noexcept { return limb_ & mask_; }
		bool operator~() const noexcept { return (limb_ & mask_) == 0; }

		void set() noexcept { limb_ |= mask_; }
		void reset() noexcept { limb_ &= ~mask_; }
		void flip() noexcept { limb_ ^= mask_; }

		reference &operator=(bool x) noexcept
		{
			if (x)
				set();
			else
				reset();
			return *this;
		}

		reference &operator|=(bool x) noexcept
		{
			if (x)
				set();
			return *this;
		}
		reference &operator&=(bool x) noexcept
		{
			if (!x)
				reset();
			return *this;
		}
		reference &operator^=(bool x) noexcept
		{
			if (x)
				flip();
			return *this;
		}
	};

	/** size metrics */
	size_t size() const noexcept { return size_; }
	size_t size_limbs() const noexcept
	{
		return (size_ + limb_bits - 1) / limb_bits;
	}
	size_t capacity() const noexcept { return data_.size() * limb_bits; }
	size_t capacity_limbs() const noexcept { return data_.size(); }

	/** raw data access. NOTE: don't write the unused bits*/
	limb_t *data() noexcept { return data_.data(); }
	std::span<limb_t> limbs() noexcept { return {data(), size_limbs()}; }
	const limb_t *data() const noexcept { return data_.data(); }
	std::span<const limb_t> limbs() const noexcept
	{
		return {data(), size_limbs()};
	}

	// constructor
	bit_vector() = default;
	explicit bit_vector(size_t size, bool value = false)
	    : size_(size), data_(allocate<limb_t>(size_limbs()))
	{
		clear(value);
	}

	// copy-constructor / assignment
	bit_vector(bit_vector const &b)
	    : size_(b.size()), data_(allocate<limb_t>(size_limbs()))
	{
		std::memcpy(data(), b.data(), size_limbs() * sizeof(limb_t));
	}
	bit_vector &operator=(bit_vector const &b)
	{
		if (capacity_limbs() < b.size_limbs())
			data_ = allocate<limb_t>(b.size_limbs());
		else if (size_limbs() > b.size_limbs())
			std::memset(data() + b.size_limbs(), 0,
			            (size_limbs() - b.size_limbs()) * sizeof(limb_t));

		std::memcpy(data(), b.data(), b.size_limbs() * sizeof(limb_t));
		size_ = b.size_;

		return *this;
	}

	/** move-constructor / assigment */
	bit_vector(bit_vector &&b) noexcept
	    : size_(std::exchange(b.size_, 0)), data_(std::exchange(b.data_, {}))
	{}
	bit_vector &operator=(bit_vector &&b) noexcept
	{
		size_ = std::exchange(b.size_, 0);
		data_ = std::exchange(b.data_, {});
		return *this;
	}

	// set all (used) bits to value
	void clear(bool value = false) noexcept
	{
		if (value)
		{
			std::memset(data(), (char)255, size_limbs() * sizeof(limb_t));
			if (size() % limb_bits)
				data_[size_limbs() - 1] =
				    limb_t(-1) >> (limb_bits - size() % limb_bits);
		}
		else
			std::memset(data(), 0, size_limbs() * sizeof(limb_t));
	}

	/** set new bits to zero, does not reduce capacity */
	void resize(size_t newsize)
	{
		size_t newsize_limbs = (newsize + limb_bits - 1) / limb_bits;

		// decreasing size -> set all removed bits to zero
		if (newsize < size())
		{
			std::memset(data() + newsize_limbs, 0,
			            (size_limbs() - newsize_limbs) * sizeof(limb_t));
			if (newsize % limb_bits)
				data_[newsize_limbs - 1] &=
				    limb_t(-1) >> (limb_bits - newsize % limb_bits);
		}

		// increasing size above capacity -> need reallocation
		else if (newsize > capacity())
		{
			auto newdata = allocate<limb_t>(newsize_limbs);
			std::memcpy(newdata.data(), data(), size_limbs() * sizeof(limb_t));
			std::memset(newdata.data() + size_limbs(), 0,
			            (newsize_limbs - size_limbs()) * sizeof(limb_t));
			data_ = std::move(newdata);
		}
		// else: increasing size, but capacity suffices -> do nothing, unused
		// bits are already zero

		size_ = newsize;
	}

	/** add a single element to the back */
	void push_back(bool value)
	{
		if (size() == capacity())
			resize(std::max(size_t(1), capacity() * 2));
		assert(capacity() > size());
		size_ += 1;
		(*this)[size_ - 1] = value;
	}

	/** remove a single element from the back */
	void pop_back() noexcept
	{
		assert(size_);
		(*this)[size_ - 1] = false;
		size_ -= 1;
	}

	/** elemet access */
	reference operator[](size_t i) noexcept
	{
		return reference(data_[i / limb_bits], i % limb_bits);
	}
	bool operator[](size_t i) const noexcept
	{
		return data_[i / limb_bits] & (limb_t(1) << (i % limb_bits));
	}

	/** global bitwise operations (inplace) */
	bit_vector &operator|=(const bit_vector &b) noexcept
	{
		assert(size() == b.size());
		for (size_t k = 0; k < size_limbs(); ++k)
			data_[k] |= b.data_[k];
		return *this;
	}
	bit_vector &operator&=(const bit_vector &b) noexcept
	{
		assert(size() == b.size());
		for (size_t k = 0; k < size_limbs(); ++k)
			data_[k] &= b.data_[k];
		return *this;
	}
	bit_vector &operator^=(const bit_vector &b) noexcept
	{
		assert(size() == b.size());
		for (size_t k = 0; k < size_limbs(); ++k)
			data_[k] ^= b.data_[k];
		return *this;
	}

	/** global bitwise operations (creating new objects) */
	bit_vector operator|(const bit_vector &b) const
	{
		assert(size() == b.size());
		bit_vector r;
		r.size_ = size_;
		r.data_ = allocate<limb_t>(size_limbs());
		for (size_t k = 0; k < size_limbs(); ++k)
			r.data_[k] = data_[k] | b.data_[k];
		return r;
	}
	bit_vector operator&(const bit_vector &b) const
	{
		assert(size() == b.size());
		bit_vector r;
		r.size_ = size_;
		r.data_ = allocate<limb_t>(size_limbs());
		for (size_t k = 0; k < size_limbs(); ++k)
			r.data_[k] = data_[k] & b.data_[k];
		return r;
	}
	bit_vector operator^(const bit_vector &b) const
	{
		assert(size() == b.size());
		bit_vector r;
		r.size_ = size_;
		r.data_ = allocate<limb_t>(size_limbs());
		for (size_t k = 0; k < size_limbs(); ++k)
			r.data_[k] = data_[k] ^ b.data_[k];
		return r;
	}

	/** returns true if any bit is set to 1 */
	bool any() const noexcept
	{
		for (size_t k = 0; k < size_limbs(); ++k)
			if (data_[k] != limb_t(0))
				return true;
		return false;
	}

	/** returns true if all bits are set to 1 */
	bool all() const noexcept
	{
		// check full limbs
		for (size_t k = 0; k < size_ / limb_bits; ++k)
			if (data_[k] != ~limb_t(0))
				return false;
		// check incomplete limb
		size_t tail = size_ % limb_bits;
		if (tail)
			if (data_[size_limbs() - 1] != (limb_t(1) << tail) - 1)
				return false;
		return true;
	}

	// returns number of bits set to value
	size_t count(bool value = true) const noexcept
	{
		size_t c = 0;
		for (size_t k = 0; k < size_limbs(); ++k)
			c += std::popcount(data_[k]);
		if (value)
			return c;
		else
			return size() - c;
		return c;
	}

	/** find first set bit. returns size() if none is set */
	size_t find() const noexcept
	{
		for (size_t k = 0; k < size_limbs(); ++k)
			if (data_[k])
				return limb_bits * k + std::countr_zero(data_[k]);
		return size_;
	}

	/** sets i'th element to true. returns false if it already was */
	bool add(size_t i) noexcept
	{
		if ((*this)[i])
			return false;
		else
		{
			(*this)[i] = true;
			return true;
		}
	}

	/** sets i'th element to false. returns false if it already was */
	bool remove(size_t i) noexcept
	{
		if (!(*this)[i])
			return false;
		else
		{
			(*this)[i] = false;
			return true;
		}
	}
};

} // namespace util
