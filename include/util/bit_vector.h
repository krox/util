#pragma once

#include "util/memory.h"
#include <bit>
#include <cassert>
#include <cstring>
#include <span>
#include <vector>

namespace util {

// (non-owning) span of (limb-aligned, read-only) bits. Does not support
// sub-spans because that would break alignment.
class const_bit_span
{
  public:
	using limb_t = size_t;
	static constexpr size_t limb_bits = sizeof(limb_t) * 8;

  private:
	limb_t const *data_ = nullptr; // unused bits of the last limb are zero
	size_t size_ = 0;              // number of used bits

  public:
	// size metrics
	size_t size() const noexcept { return size_; }
	size_t size_limbs() const noexcept
	{
		return (size_ + limb_bits - 1) / limb_bits;
	}

	// raw data access. beware of 'offset' when using this
	limb_t const *data() const noexcept { return data_; }
	std::span<const limb_t> limbs() const noexcept
	{
		return {data(), size_limbs()};
	}

	// constructor
	const_bit_span() = default;
	explicit const_bit_span(limb_t const *data, size_t size) noexcept
	    : data_(data), size_(size)
	{}

	// elemet access
	bool operator[](size_t i) const noexcept
	{
		return data_[i / limb_bits] & (limb_t(1) << (i % limb_bits));
	}
	bool at(size_t i) const
	{
		if (i >= size())
			throw std::out_of_range("const_bit_span::at");
		return (*this)[i];
	}

	// returns true if any bit is set to 1
	bool any() const noexcept
	{
		for (size_t k = 0; k < size_limbs(); ++k)
			if (data_[k] != limb_t(0))
				return true;
		return false;
	}

	// returns true if all bits are set to 1
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
	}

	// find first set bit. returns size() if none is set
	size_t find() const noexcept
	{
		for (size_t k = 0; k < size_limbs(); ++k)
			if (data_[k])
				return limb_bits * k + std::countr_zero(data_[k]);
		return size_;
	}
};

// (non-owning) reference to a single (writeable) bit
class bit_reference
{
  public:
	using limb_t = const_bit_span::limb_t;
	static constexpr size_t limb_bits = const_bit_span::limb_bits;

  private:
	limb_t &limb_;
	limb_t mask_;

	void operator&() = delete; // prevent some misuse

  public:
	explicit bit_reference(limb_t &limb, size_t pos) noexcept
	    : limb_(limb), mask_(limb_t(1) << pos)
	{}

	operator bool() const noexcept { return limb_ & mask_; }
	bool operator~() const noexcept { return (limb_ & mask_) == 0; }

	void set() noexcept { limb_ |= mask_; }
	void reset() noexcept { limb_ &= ~mask_; }
	void flip() noexcept { limb_ ^= mask_; }

	bit_reference &operator=(bool x) noexcept
	{
		if (x)
			set();
		else
			reset();
		return *this;
	}

	bit_reference &operator|=(bool x) noexcept
	{
		if (x)
			set();
		return *this;
	}

	bit_reference &operator&=(bool x) noexcept
	{
		if (!x)
			reset();
		return *this;
	}

	bit_reference &operator^=(bool x) noexcept
	{
		if (x)
			flip();
		return *this;
	}
};

// (non-owning) span of (limb-aligned, writeable) bits
//   * does not support sub-spans because that would break alignment
class bit_span
{
  public:
	using limb_t = bit_reference::limb_t;
	static constexpr size_t limb_bits = bit_reference::limb_bits;
	using reference = bit_reference;

  private:
	limb_t *data_ = nullptr; // unused bits of the last limb are kept at zero
	size_t size_ = 0;        // number of used bits

  public:
	// size metrics
	size_t size() const noexcept { return size_; }
	size_t size_limbs() const noexcept
	{
		return (size_ + limb_bits - 1) / limb_bits;
	}

	// raw data access. beware of 'offset' when using this
	limb_t *data() const noexcept { return data_; }
	std::span<limb_t> limbs() const noexcept { return {data(), size_limbs()}; }

	// constructor
	bit_span() = default;
	explicit bit_span(limb_t *data, size_t size) noexcept
	    : data_(data), size_(size)
	{}

	// const-cast
	operator const_bit_span() const noexcept
	{
		return const_bit_span(data_, size_);
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

	// elemet access
	reference operator[](size_t i) const noexcept
	{
		return reference(data_[i / limb_bits], i % limb_bits);
	}
	reference at(size_t i) const
	{
		if (i >= size())
			throw std::out_of_range("bit_span::at");
		return (*this)[i];
	}

	// returns true if any bit is set to 1
	bool any() const noexcept { return const_bit_span(*this).any(); }

	// returns true if all bits are set to 1
	bool all() const noexcept { return const_bit_span(*this).all(); }

	// returns number of bits set to value
	size_t count(bool value = true) const noexcept
	{
		return const_bit_span(*this).count(value);
	}

	// find first set bit. returns size() if none is set
	size_t find() const noexcept { return const_bit_span(*this).find(); }

	// sets i'th element to true. returns false if it already was
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

	// sets i'th element to false. returns false if it already was
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

// global bitwise operations (out parameter, aliasing allowed)
inline void bitwise_or(bit_span r, const_bit_span a, const_bit_span b) noexcept
{
	assert(r.size() == a.size() && r.size() == b.size());
	for (size_t k = 0; k < r.size_limbs(); ++k)
		r.data()[k] = a.data()[k] | b.data()[k];
}
inline void bitwise_and(bit_span r, const_bit_span a, const_bit_span b) noexcept
{
	assert(r.size() == a.size() && r.size() == b.size());
	for (size_t k = 0; k < r.size_limbs(); ++k)
		r.data()[k] = a.data()[k] & b.data()[k];
}
inline void bitwise_xor(bit_span r, const_bit_span a, const_bit_span b) noexcept
{
	assert(r.size() == a.size() && r.size() == b.size());
	for (size_t k = 0; k < r.size_limbs(); ++k)
		r.data()[k] = a.data()[k] ^ b.data()[k];
}

// global bitwise operations (inplace)
inline void operator|=(bit_span a, const_bit_span b) noexcept
{
	bitwise_or(a, a, b);
}
inline void operator&=(bit_span a, const_bit_span b) noexcept
{
	bitwise_and(a, a, b);
}
inline void operator^=(bit_span a, const_bit_span b) noexcept
{
	bitwise_xor(a, a, b);
}

/**
 * Similar to specialized std::vector<bool>, but
 *   - does not pretend to be container (no iterators),
 *     thus avoiding some common pitfalls of std::vector<bool>
 *   - offers (fast) bitwise operations
 *   - using 'add() and remove()', it can be used as a 'set of integers'
 *   - beware of different semantics. e.g. '.clear()' sets all bits to zero,
 *     whereas std::vector<bool>::clear() resizes the vector
 */
template <int features = 0> class bit_vector_impl
{
  public:
	enum feature_flags
	{
		none = 0,
		auto_resize = 1,
	};

	/** types and constants */
	using limb_t = size_t;
	static constexpr size_t limb_bits = sizeof(limb_t) * 8;

  private:
	size_t size_ = 0;                 // number of used bits
	unique_memory<limb_t> data_ = {}; // all unused bits are kept at zero

  public:
	class reference
	{
		friend class bit_vector_impl;
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
	bit_vector_impl() = default;
	explicit bit_vector_impl(size_t size, bool value = false)
	    : size_(size), data_(allocate<limb_t>(size_limbs()))
	{
		clear(value);
	}

	operator bit_span() noexcept { return bit_span(data(), size()); }
	operator const_bit_span() const noexcept
	{
		return const_bit_span(data(), size());
	}

	// copy-constructor / assignment
	bit_vector_impl(bit_vector_impl const &b)
	    : size_(b.size()), data_(allocate<limb_t>(size_limbs()))
	{
		std::memcpy(data(), b.data(), size_limbs() * sizeof(limb_t));
	}
	bit_vector_impl &operator=(bit_vector_impl const &b)
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
	bit_vector_impl(bit_vector_impl &&b) noexcept
	    : size_(std::exchange(b.size_, 0)), data_(std::exchange(b.data_, {}))
	{}
	bit_vector_impl &operator=(bit_vector_impl &&b) noexcept
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

	// make sure capacity is at least newcap
	//     * does nothing if newcap <= capacity()
	//     * if spare=true, any reallocation will at least double the capacity
	void reserve(size_t newcap, bool spare = false)
	{
		if (newcap <= capacity())
			return;
		if (spare)
			newcap = std::max(newcap, 2 * capacity());
		size_t newcap_limbs = (newcap + limb_bits - 1) / limb_bits;
		auto newdata = allocate<limb_t>(newcap_limbs);
		std::memcpy(newdata.data(), data(), size_limbs() * sizeof(limb_t));
		std::memset(newdata.data() + size_limbs(), 0,
		            (newcap_limbs - size_limbs()) * sizeof(limb_t));
		data_ = std::move(newdata);
	}

	// set new bits to zero, does not reduce capacity
	void resize(size_t newsize)
	{
		reserve(newsize, false);
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

		size_ = newsize;
	}

	// add a single element to the back
	void push_back(bool value)
	{
		if (size() == capacity())
			reserve(size() + 1, true);
		size_ += 1;
		(*this)[size_ - 1] = value;
	}

	/** remove and return a single element from the back */
	bool pop_back() noexcept
	{
		assert(size_);
		bool r = (*this)[size_ - 1];
		(*this)[size_ - 1] = false;
		size_ -= 1;
		return r;
	}

	/** elemet access */
	reference operator[](size_t i) noexcept
	{
		if constexpr (features & auto_resize)
			if (i >= size())
			{
				reserve(i + 1, true);
				resize(i + 1);
			}
		return reference(data_[i / limb_bits], i % limb_bits);
	}
	bool operator[](size_t i) const noexcept
	{
		if constexpr (features & auto_resize)
			if (i >= size())
				return false;
		return data_[i / limb_bits] & (limb_t(1) << (i % limb_bits));
	}

	/** global bitwise operations (creating new objects) */
	bit_vector_impl operator|(const bit_vector_impl &b) const
	{
		assert(size() == b.size());
		bit_vector_impl r;
		r.size_ = size_;
		r.data_ = allocate<limb_t>(size_limbs());
		bitwise_or(r, *this, b);
		return r;
	}
	bit_vector_impl operator&(const bit_vector_impl &b) const
	{
		assert(size() == b.size());
		bit_vector_impl r;
		r.size_ = size_;
		r.data_ = allocate<limb_t>(size_limbs());
		bitwise_and(r, *this, b);
		return r;
	}
	bit_vector_impl operator^(const bit_vector_impl &b) const
	{
		assert(size() == b.size());
		bit_vector_impl r;
		r.size_ = size_;
		r.data_ = allocate<limb_t>(size_limbs());
		bitwise_xor(r, *this, b);
		return r;
	}

	// returns true if all/any bits are set to 1
	bool any() const noexcept { return const_bit_span(*this).any(); }
	bool all() const noexcept { return const_bit_span(*this).all(); }

	// returns number of bits set to value
	size_t count(bool value = true) const noexcept
	{
		return const_bit_span(*this).count(value);
	}

	// find first set bit. returns size() if none is set
	size_t find() const noexcept { return const_bit_span(*this).find(); }

	// sets i'th element to true. returns false if it already was
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

	// sets i'th element to false. returns false if it already was
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

using bit_vector = bit_vector_impl<>;
using bit_map = bit_vector_impl<bit_vector_impl<>::auto_resize>;

// alternative to bit_vector with fast '.clear()' implemented by keeping a list
// of non-zero limbs
class sparse_bit_vector
{
  public:
	// types and constants
	using limb_t = size_t;
	static constexpr size_t limb_bits = sizeof(limb_t) * 8;

  private:
	size_t size_ = 0;                 // number of used bits
	unique_memory<limb_t> data_ = {}; // all unused bits are kept at zero
	std::vector<uint32_t> dirty_;     // list of non-zero limb indices

  public:
	// size metrics
	size_t size() const noexcept { return size_; }
	size_t size_limbs() const noexcept
	{
		return (size_ + limb_bits - 1) / limb_bits;
	}
	size_t capacity() const noexcept { return data_.size() * limb_bits; }
	size_t capacity_limbs() const noexcept { return data_.size(); }

	// raw data access. read-only to make sure the dirty list is consistent
	const limb_t *data() const noexcept { return data_.data(); }
	std::span<const limb_t> limbs() const noexcept
	{
		return {data(), size_limbs()};
	}

	// constructor
	sparse_bit_vector() = default;
	explicit sparse_bit_vector(size_t size)
	    : size_(size), data_(allocate<limb_t>(size_limbs()))
	{
		std::memset(data_.data(), 0, size_limbs() * sizeof(limb_t));
	}

	// copy-constructor / assignment
	sparse_bit_vector(sparse_bit_vector const &b)
	    : size_(b.size()), data_(allocate<limb_t>(size_limbs())),
	      dirty_(b.dirty_)
	{
		std::memcpy(data_.data(), b.data(), size_limbs() * sizeof(limb_t));
	}
	sparse_bit_vector &operator=(sparse_bit_vector const &b)
	{
		if (capacity_limbs() < b.size_limbs())
			data_ = allocate<limb_t>(b.size_limbs());
		else if (size_limbs() > b.size_limbs())
			std::memset(data_.data() + b.size_limbs(), 0,
			            (size_limbs() - b.size_limbs()) * sizeof(limb_t));

		std::memcpy(data_.data(), b.data(), b.size_limbs() * sizeof(limb_t));
		size_ = b.size_;
		dirty_ = b.dirty_;

		return *this;
	}

	// move-constructor / assigment
	sparse_bit_vector(sparse_bit_vector &&b) noexcept
	    : size_(std::exchange(b.size_, 0)), data_(std::exchange(b.data_, {})),
	      dirty_(std::exchange(b.dirty_, {}))
	{}
	sparse_bit_vector &operator=(sparse_bit_vector &&b) noexcept
	{
		size_ = std::exchange(b.size_, 0);
		data_ = std::exchange(b.data_, {});
		dirty_ = std::exchange(b.dirty_, {});
		return *this;
	}

	// set all (used) bits to zero
	void clear() noexcept
	{
		for (auto k : dirty_)
			data_[k] = 0;
		dirty_.clear();
	}

	// returns number of bits set to value
	size_t count() const noexcept
	{
		size_t c = 0;
		for (auto k : dirty_)
			c += std::popcount(data_[k]);
		return c;
	}

	// sets i'th element to true. returns false if it already was
	bool add(size_t i) noexcept
	{
		size_t k = i / limb_bits;
		size_t pos = i % limb_bits;
		auto mask = limb_t(1) << pos;

		if (data_[k] == 0)
		{
			dirty_.push_back(k);
			data_[k] = mask;
			return true;
		}
		else if ((data_[k] & mask) == 0)
		{
			data_[k] |= mask;
			return true;
		}
		else
			return false;
	}

	bool operator[](size_t i) const
	{
		size_t k = i / limb_bits;
		size_t pos = i % limb_bits;
		auto mask = limb_t(1) << pos;
		return (data_[k] & mask) != 0;
	}
};

} // namespace util
