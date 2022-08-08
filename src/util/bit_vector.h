#pragma once

#include "util/bits.h"
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
	size_t size_ = 0;           // number of used bits
	limb_t *data_ = nullptr;    // all unused bits are kept at zero
	size_t capacity_limbs_ = 0; // size of allocation in limbs

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
	size_t capacity() const noexcept { return capacity_limbs_ * limb_bits; }
	size_t capacity_limbs() const noexcept { return capacity_limbs_; }

	/** raw data access. NOTE: don't write the unused bits*/
	limb_t *data() noexcept { return data_; }
	std::span<limb_t> limbs() noexcept { return {data_, size_limbs()}; }
	const limb_t *data() const noexcept { return data_; }
	std::span<const limb_t> limbs() const noexcept
	{
		return {data_, size_limbs()};
	}

	/** constructor / destructor*/
	bit_vector() = default;
	explicit bit_vector(size_t size, bool value = false)
	    : size_(size), data_(new limb_t[size_limbs()]),
	      capacity_limbs_(size_limbs())
	{
		clear(value);
	}
	~bit_vector()
	{
		if (data_)
			delete[] data_;
	}

	/** copy-constructor / assignment */
	bit_vector(bit_vector const &b)
	    : size_(b.size()), data_(new limb_t[size_limbs()]),
	      capacity_limbs_(size_limbs())
	{
		std::memcpy(data(), b.data(), size_limbs() * sizeof(limb_t));
	}
	bit_vector &operator=(bit_vector const &b)
	{
		if (capacity_limbs() < b.size_limbs())
		{
			if (data_ != nullptr)
				delete[] data_;
			data_ = new limb_t[b.size_limbs()];
			capacity_limbs_ = b.size_limbs();
		}
		else if (size_limbs() > b.size_limbs())
			std::memset(data() + b.size_limbs(), 0,
			            (size_limbs() - b.size_limbs()) * sizeof(limb_t));

		std::memcpy(data(), b.data(), b.size_limbs() * sizeof(limb_t));
		size_ = b.size_;

		return *this;
	}

	/** move-constructor / assigment */
	bit_vector(bit_vector &&b) noexcept
	    : size_(b.size_), data_(b.data_), capacity_limbs_(b.capacity_limbs_)
	{
		// just steal the buffer of b
		b.size_ = 0;
		b.data_ = nullptr;
		b.capacity_limbs_ = 0;
	}
	bit_vector &operator=(bit_vector &&b) noexcept
	{
		// delete old storage (if any) and then just steal buffer of b
		if (data_ != nullptr)
			delete[] data_;
		size_ = b.size_;
		data_ = b.data_;
		capacity_limbs_ = b.capacity_limbs_;
		b.size_ = 0;
		b.data_ = nullptr;
		b.capacity_limbs_ = 0;
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
			// whole limbs
			std::memset(data() + newsize_limbs, 0,
			            (size_limbs() - newsize_limbs) * sizeof(limb_t));
			// last partial limb
			size_t tail = newsize % limb_bits;
			if (tail != 0)
				data_[newsize_limbs - 1] &= (limb_t(1) << tail) - 1;
			size_ = newsize;
		}

		// increasing size but still enough capacity -> do nothing
		else if (newsize <= capacity())
		{
			size_ = newsize; // unused bits should already be zero
		}

		// else, need new capacity
		else
		{
			limb_t *newdata = new limb_t[newsize_limbs];
			std::memcpy(newdata, data_, size_limbs() * sizeof(limb_t));
			std::memset(newdata + size_limbs(), 0,
			            (newsize_limbs - size_limbs()) * sizeof(limb_t));
			if (data_ != nullptr)
				delete[] data_;
			size_ = newsize;
			data_ = newdata;
			capacity_limbs_ = newsize_limbs;
		}
	}

	/** add a single element to the back */
	void push_back(bool value)
	{
		if (size() == capacity())
		{
			size_t newsize_limbs = std::max((size_t)1, capacity_limbs() * 2);
			limb_t *newdata = new limb_t[newsize_limbs];
			std::memcpy(newdata, data_, size_limbs() * sizeof(limb_t));
			std::memset(newdata + size_limbs(), 0,
			            (newsize_limbs - size_limbs()) * sizeof(limb_t));
			if (data_ != nullptr)
				delete[] data_;
			data_ = newdata;
			capacity_limbs_ = newsize_limbs;
		}
		assert(capacity() > size());
		size_ += 1;
		(*this)[size_ - 1] = value;
	}

	/** remove a single element from the back */
	void pop_back() noexcept
	{
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
		r.data_ = new limb_t[size_limbs()];
		r.capacity_limbs_ = size_limbs();
		for (size_t k = 0; k < size_limbs(); ++k)
			r.data_[k] = data_[k] | b.data_[k];
		return r;
	}
	bit_vector operator&(const bit_vector &b) const
	{
		assert(size() == b.size());
		bit_vector r;
		r.size_ = size_;
		r.data_ = new limb_t[size_limbs()];
		r.capacity_limbs_ = size_limbs();
		for (size_t k = 0; k < size_limbs(); ++k)
			r.data_[k] = data_[k] & b.data_[k];
		return r;
	}
	bit_vector operator^(const bit_vector &b) const
	{
		assert(size() == b.size());
		bit_vector r;
		r.size_ = size_;
		r.data_ = new limb_t[size_limbs()];
		r.capacity_limbs_ = size_limbs();
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
			c += popcount(data_[k]);
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
				return limb_bits * k + ctz(data_[k]);
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
