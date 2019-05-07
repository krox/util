// SHA3 implementation based on (heavily stripped down version of)
// https://github.com/brainhub/SHA3IUF by Andrey Jivsov (crypto@brainhub.org)
// public domain

#include "util/hash.h"

#include <array>
#include <cassert>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

namespace util {

// will usually be compiled into a single statement
static inline uint64_t rotl(uint64_t x, int y)
{
	return (x << y) | (x >> (64 - y));
}

static const uint64_t keccakf_rndc[24] = {
    0x0000000000000001ULL, 0x0000000000008082ULL, 0x800000000000808aULL,
    0x8000000080008000ULL, 0x000000000000808bULL, 0x0000000080000001ULL,
    0x8000000080008081ULL, 0x8000000000008009ULL, 0x000000000000008aULL,
    0x0000000000000088ULL, 0x0000000080008009ULL, 0x000000008000000aULL,
    0x000000008000808bULL, 0x800000000000008bULL, 0x8000000000008089ULL,
    0x8000000000008003ULL, 0x8000000000008002ULL, 0x8000000000000080ULL,
    0x000000000000800aULL, 0x800000008000000aULL, 0x8000000080008081ULL,
    0x8000000000008080ULL, 0x0000000080000001ULL, 0x8000000080008008ULL};

static const int keccakf_rotc[24] = {1,  3,  6,  10, 15, 21, 28, 36,
                                     45, 55, 2,  14, 27, 41, 56, 8,
                                     25, 43, 62, 18, 39, 61, 20, 44};

static const int keccakf_piln[24] = {10, 7,  11, 17, 18, 3,  5,  16,
                                     8,  21, 24, 4,  15, 23, 19, 13,
                                     12, 2,  20, 14, 22, 9,  6,  1};

static void keccakf(std::array<uint64_t, 25> &s)
{
	for (int r = 0; r < 24; r++)
	{
		uint64_t bc[5];

		// Theta
		for (int i = 0; i < 5; i++)
			bc[i] = s[i] ^ s[i + 5] ^ s[i + 10] ^ s[i + 15] ^ s[i + 20];
		for (int i = 0; i < 5; i++)
		{
			uint64_t t = bc[(i + 4) % 5] ^ rotl(bc[(i + 1) % 5], 1);
			for (int j = 0; j < 25; j += 5)
				s[j + i] ^= t;
		}

		// Rho Pi
		uint64_t t = s[1];
		for (int i = 0; i < 24; i++)
		{
			int j = keccakf_piln[i];
			bc[0] = s[j];
			s[j] = rotl(t, keccakf_rotc[i]);
			t = bc[0];
		}

		// Chi
		for (int j = 0; j < 25; j += 5)
		{
			for (int i = 0; i < 5; i++)
				bc[i] = s[j + i];
			for (int i = 0; i < 5; i++)
				s[j + i] ^= (~bc[(i + 1) % 5]) & bc[(i + 2) % 5];
		}

		// Iota
		s[0] ^= keccakf_rndc[r];
	}
}

std::array<std::byte, 32> sha3_256(span<const std::byte> data)
{
	union
	{
		std::array<uint64_t, 25> state;          // full state
		std::array<uint8_t, 25 * 8> state_bytes; // full state
		std::array<std::byte, 32> ret;           // truncated state for return
	};

	state.fill(0);

	const uint8_t *buf = (uint8_t *)data.data();

	size_t blocks = data.size() / (8 * 17);
	size_t tail = data.size() % (8 * 17);

	// process whole blocks
	for (size_t i = 0; i < blocks; ++i)
	{
		for (int k = 0; k < 17; ++k, buf += 8)
			state[k] ^= *(uint64_t *)buf;
		keccakf(state);
	}

	// process last (incomplete) block (which might be empty apart from padding)
	for (size_t j = 0; j < tail; ++j, buf += 1)
		state_bytes[j] ^= *buf;
	state_bytes[tail] ^= (uint8_t)6;
	state_bytes[17 * 8 - 1] ^= (uint8_t)0x80U;
	keccakf(state);

	// result = truncated state
	return ret;
}

std::string hex_string(span<const std::byte> h)
{
	static const char digits[] = "0123456789abcdef";
	std::string r;
	r.resize(2 * h.size());
	for (size_t i = 0; i < h.size(); ++i)
	{
		r[2 * i] = digits[(int)h[i] >> 4];
		r[2 * i + 1] = digits[(int)h[i] & 15];
	}
	return r;
}

} // namespace util
