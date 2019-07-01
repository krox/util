#ifndef UTIL_BITSET_H
#define UTIL_BITSET_H

#include "util/span.h"
#include <cstring>

namespace util {

/**
 * Similar to specialized std::vector<bool>, but
 *   - does not pretend to be container (no iterators, no push_back, ...)
 *   - offers (fast) bitwise operations
 *   - resizing is slow, because no spare capacity is kept
 */
class bitset
{
  public:
	/** types and constants */
	using limb_t = size_t;
	static constexpr size_t limb_bits = sizeof(limb_t) * 8;

  private:
	size_t size_ = 0;        // number of used bits
	limb_t *data_ = nullptr; // unused bits of the last limb are zero

  public:
	class reference
	{
		friend class bitset;
		limb_t &limb_;
		limb_t mask_;
		reference(limb_t &limb, size_t pos)
		    : limb_(limb), mask_(limb_t(1) << pos)
		{}

		void operator&() = delete; // prevent misuse

	  public:
		operator bool() const { return limb_ & mask_; }
		bool operator~() const { return (limb_ & mask_) == 0; }

		void set() { limb_ |= mask_; }
		void reset() { limb_ &= ~mask_; }
		void flip() { limb_ ^= mask_; }

		reference &operator=(bool x)
		{
			if (x)
				set();
			else
				reset();
			return *this;
		}

		reference &operator|=(bool x)
		{
			if (x)
				set();
			return *this;
		}
		reference &operator&=(bool x)
		{
			if (!x)
				reset();
			return *this;
		}
		reference &operator^=(bool x)
		{
			if (x)
				flip();
			return *this;
		}
	};

	/** size metrics */
	size_t size() const { return size_; }
	size_t size_limbs() const { return (size_ + limb_bits - 1) / limb_bits; }

	/** raw data access. NOTE: don't write the unused bits */
	limb_t *data() { return data_; }
	span<limb_t> limbs() { return {data_, size_limbs()}; }
	const limb_t *data() const { return data_; }
	span<const limb_t> limbs() const { return {data_, size_limbs()}; }

	/** constructor / destructor*/
	bitset() {}
	bitset(size_t size) : size_(size), data_(new limb_t[size_limbs()])
	{
		std::memset(data(), 0, size_limbs() * sizeof(limb_t));
	}
	~bitset()
	{
		if (data_ != nullptr)
			delete[] data_;
	}

	/** copy-constructor / assignment */
	bitset(const bitset &b) : size_(b.size_), data_(new limb_t[size_limbs()])
	{
		std::memcpy(data(), b.data(), size_limbs() * sizeof(limb_t));
	}
	bitset &operator=(const bitset &b)
	{
		if (size_limbs() != b.size_limbs())
		{
			if (data_ != nullptr)
				delete[] data_;
			size_ = b.size_;
			data_ = new limb_t[size_limbs()];
		}

		std::memcpy(data(), b.data(), size_limbs() * sizeof(limb_t));
		return *this;
	}

	/** move-constructor / assigment */
	bitset(bitset &&b) noexcept : size_(b.size_), data_(b.data_)
	{
		b.size_ = 0;
		b.data_ = nullptr;
	}
	bitset &operator=(bitset &&b) noexcept
	{
		if (data_ != nullptr)
			delete[] data_;
		size_ = b.size_;
		data_ = b.data_;
		b.size_ = 0;
		b.data_ = nullptr;
		return *this;
	}

	/** set all (used) bits to zero */
	void clear() noexcept
	{
		std::memset(data(), 0, size_limbs() * sizeof(limb_t));
	}

	/** set new bits to zero, does not reduce capacity */
	void resize(size_t newsize)
	{
		size_t newsize_limbs = (newsize + limb_bits - 1) / limb_bits;

		if (newsize_limbs == size_limbs())
		{
			size_ = newsize;
		}
		else
		{
			limb_t *newdata = new limb_t[newsize_limbs];

			if (newsize_limbs < size_limbs()) // shrink
			{
				std::memcpy(newdata, data_, newsize_limbs * sizeof(limb_t));
			}
			else // grow
			{
				std::memcpy(newdata, data_, size_limbs() * sizeof(limb_t));
				std::memset(newdata + size_limbs(), 0,
				            (newsize_limbs - size_limbs()) * sizeof(limb_t));
			}

			if (data_ != nullptr)
				delete[] data_;
			size_ = newsize;
			data_ = newdata;
		}

		// clear out unused bits
		size_t tail = size_ % limb_bits;
		if (tail != 0)
			data_[size_limbs() - 1] &= (limb_t(1) << tail) - 1;
	}

	/** elemet access */
	reference operator[](size_t i)
	{
		return reference(data_[i / limb_bits], i % limb_bits);
	}
	bool operator[](size_t i) const
	{
		return data_[i / limb_bits] & (limb_t(1) << (i % limb_bits));
	}

	/** global bitwise operations (inplace) */
	bitset &operator|=(const bitset &b)
	{
		assert(size() == b.size());
		for (size_t k = 0; k < size_limbs(); ++k)
			data_[k] |= b.data_[k];
		return *this;
	}
	bitset &operator&=(const bitset &b)
	{
		assert(size() == b.size());
		for (size_t k = 0; k < size_limbs(); ++k)
			data_[k] &= b.data_[k];
		return *this;
	}
	bitset &operator^=(const bitset &b)
	{
		assert(size() == b.size());
		for (size_t k = 0; k < size_limbs(); ++k)
			data_[k] ^= b.data_[k];
		return *this;
	}

	/** global bitwise operations (creating new objects) */
	bitset operator|(const bitset &b) const
	{
		assert(size() == b.size());
		bitset r;
		r.size_ = size_;
		r.data_ = new limb_t[size_limbs()];
		for (size_t k = 0; k < size_limbs(); ++k)
			r.data_[k] = data_[k] | b.data_[k];
		return r;
	}
	bitset operator&(const bitset &b) const
	{
		assert(size() == b.size());
		bitset r;
		r.size_ = size_;
		r.data_ = new limb_t[size_limbs()];
		for (size_t k = 0; k < size_limbs(); ++k)
			r.data_[k] = data_[k] & b.data_[k];
		return r;
	}
	bitset operator^(const bitset &b) const
	{
		assert(size() == b.size());
		bitset r;
		r.size_ = size_;
		r.data_ = new limb_t[size_limbs()];
		for (size_t k = 0; k < size_limbs(); ++k)
			r.data_[k] = data_[k] ^ b.data_[k];
		return r;
	}

	/** returns true if any bit is set to 1 */
	bool any() const
	{
		for (size_t k = 0; k < size_limbs(); ++k)
			if (data_[k] != limb_t(0))
				return true;
		return false;
	}

	/** returns true if all bits are set to 1 */
	bool all() const
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

	/** number of set bits */
	size_t count() const
	{
		size_t c = 0;
		for (size_t k = 0; k < size_limbs(); ++k)
			c += __builtin_popcountll(data_[k]);
		return c;
	}

	/** number of bits equal to b */
	size_t count(bool b) const
	{
		if (b)
			return count();
		else
			return size() - count();
	}

	/** find first set bit. returns size() if none is set */
	size_t find() const
	{
		for (size_t k = 0; k < size_limbs(); ++k)
			if (data_[k] != limb_t(0))
				return limb_bits * k + __builtin_ctzll(data_[k]);
		return size_;
	}
};

} // namespace util

#endif
