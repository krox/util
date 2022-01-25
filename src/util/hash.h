#pragma once

#include "util/span.h"
#include <array>
#include <cstddef>

namespace util {

// SHA2-256
std::array<std::byte, 32> sha256(span<const std::byte>) noexcept;

// the Keccak function, that all of SHA3 is based on
void keccakf(std::array<uint64_t, 25> &s) noexcept;

// This implements the sponge construction based on the Keccak function.
//     * For most usecases you should call call .finish() exactly once,
//       .process() only before and .generate() only afterwards
//     * would be nice to provide an interface that works for all usecases
//       at the same time (i.e. producing fixed-size hashes, extendable output
//       functions, and cryptographic prng), but that seems tricky to design.
//       So for the time being we use this backend class with a separate wrapper
//       for different usecases
template <int bit_rate, uint8_t domain> class Sha3
{
	static_assert(bit_rate >= 64 && bit_rate <= 1600 && bit_rate % 64 == 0);
	static constexpr int byte_rate = bit_rate / 8;

	int pos_ = 0; // byte-position into state. always in [0, byte_rate)
	union
	{
		std::array<uint64_t, 25> state_;
		std::array<uint8_t, 25 * 8> state_bytes_;
	};

	// Notes on Keccak based SHA3:
	//     * internal state is always 1600 bits = 25 x uint64
	//     * input is padded by 10....01 bits and some explicit domain
	//       separation between SHA3, SHAKE, etc
	//     * each round consumes/produces r bits and then runs the Keccak
	//       function, leaving c = 1600 - r bits of capacity for security
	//     * for the standard SHA-3 functions, output size is always d = c / 2
	//       because preimage resistance is at most c / 2 anyway (due to
	//       invertibility of the Keccak function). As a side effect, this means
	//       that a single round of output is sufficient to produce the result.
	//     * for extendable output, use the entire r bits, then run Keccak again

  public:
	Sha3() noexcept { state_.fill(0); }

	// process some data
	void process(void const *data, size_t len) noexcept
	{
		auto buf = (uint8_t const *)data;
		for (size_t i = 0; i < len; ++i)
		{
			state_bytes_[pos_++] = *buf++;
			if (pos_ == byte_rate)
			{
				keccakf(state_);
				pos_ = 0;
			}
		}
	}

	// add padding and complete final block
	void finish() noexcept
	{
		// last (incomplete) block might be empty apart from the padding
		state_bytes_[pos_] ^= (uint8_t)domain;
		state_bytes_[byte_rate - 1] ^= (uint8_t)0x80U;
		keccakf(state_);
		pos_ = 0;
	}

	// generate data
	void generate(void *data, size_t len) noexcept
	{
		auto buf = (uint8_t *)data;
		for (size_t i = 0; i < len; ++i)
		{
			*buf++ = state_bytes_[pos_++];
			if (pos_ == byte_rate)
			{
				keccakf(state_);
				pos_ = 0;
			}
		}
	}
};

// Use the sponge-construction as single-call hash function
//     * The standard only defines sha3<224/256/384/512>
//     * Note that different parameters here are implicitly domain separated
//       due to different rates and the applied padding
template <int d> std::array<std::byte, d / 8> sha3(span<const std::byte> data)
{
	static_assert(d > 0 && d <= 800 && d % 8 == 0);
	static constexpr int bit_rate = 1600 - 2 * d;
	Sha3<bit_rate, 0x06> sha = {};

	sha.process(data.data(), data.size());
	sha.finish();

	std::array<std::byte, d / 8> r;
	sha.generate(r.data(), r.size());
	return r;
}

// convenience overload for strings
template <int d> auto sha3(std::string_view s)
{
	return sha3<d>(span<const std::byte>((std::byte *)s.data(), s.size()));
}

// non-cryptographic hash function "FNV-1a" (for use in hash-tables and such)
uint32_t fnv1a_32(span<const std::byte>) noexcept;
uint64_t fnv1a_64(span<const std::byte>) noexcept;

// convenience function for pretty printing hash sums
std::string hex_string(span<const std::byte>);

} // namespace util
