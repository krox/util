// the SHA256 and SHA3 implementations here are originally based on
// https://github.com/B-Con/crypto-algorithms by Brad Conte (brad@bradconte.com)
// https://github.com/brainhub/SHA3IUF by Andrey Jivsov (crypto@brainhub.org)
// respectively. Both public domain, and modified beyond recognition by now.

#include "util/hash.h"

#include <array>
#include <cassert>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <utility>

#if __BYTE_ORDER__ != __ORDER_LITTLE_ENDIAN__
#error "keccakf() not implemented for big-endian platforms"
#endif

namespace util {

namespace {

// will usually be compiled into a single statement
constexpr uint64_t rotl(uint64_t x, int y)
{
	return (x << y) | (x >> (64 - y));
}
constexpr uint32_t rotr(uint32_t x, int y)
{
	return (x >> y) | (x << (32 - y));
}

constexpr uint64_t keccakf_rndc[24] = {
    0x0000000000000001ULL, 0x0000000000008082ULL, 0x800000000000808aULL,
    0x8000000080008000ULL, 0x000000000000808bULL, 0x0000000080000001ULL,
    0x8000000080008081ULL, 0x8000000000008009ULL, 0x000000000000008aULL,
    0x0000000000000088ULL, 0x0000000080008009ULL, 0x000000008000000aULL,
    0x000000008000808bULL, 0x800000000000008bULL, 0x8000000000008089ULL,
    0x8000000000008003ULL, 0x8000000000008002ULL, 0x8000000000000080ULL,
    0x000000000000800aULL, 0x800000008000000aULL, 0x8000000080008081ULL,
    0x8000000000008080ULL, 0x0000000080000001ULL, 0x8000000080008008ULL};

constexpr int keccakf_rotc[24] = {1,  3,  6,  10, 15, 21, 28, 36,
                                  45, 55, 2,  14, 27, 41, 56, 8,
                                  25, 43, 62, 18, 39, 61, 20, 44};

constexpr int keccakf_piln[24] = {10, 7,  11, 17, 18, 3, 5,  16, 8,  21, 24, 4,
                                  15, 23, 19, 13, 12, 2, 20, 14, 22, 9,  6,  1};
} // namespace

void keccakf(std::array<uint64_t, 25> &s) noexcept
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

namespace {
constexpr uint32_t ch(uint32_t x, uint32_t y, uint32_t z)
{
	return (x & y) ^ (~x & z);
}
constexpr uint32_t maj(uint32_t x, uint32_t y, uint32_t z)
{
	return (x & y) ^ (x & z) ^ (y & z);
}
constexpr uint32_t ep0(uint32_t x)
{
	return rotr(x, 2) ^ rotr(x, 13) ^ rotr(x, 22);
}
constexpr uint32_t ep1(uint32_t x)
{
	return rotr(x, 6) ^ rotr(x, 11) ^ rotr(x, 25);
}
constexpr uint32_t sig0(uint32_t x)
{
	return rotr(x, 7) ^ rotr(x, 18) ^ (x >> 3);
}
constexpr uint32_t sig1(uint32_t x)
{
	return rotr(x, 17) ^ rotr(x, 19) ^ (x >> 10);
}
constexpr uint32_t bswap(uint32_t x)
{
	return ((x & 0xFF000000) >> 24) | ((x & 0x00FF0000) >> 8) |
	       ((x & 0x0000FF00) << 8) | ((x & 0x000000FF) << 24);
}
constexpr uint32_t sha256_k[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1,
    0x923f82a4, 0xab1c5ed5, 0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3,
    0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174, 0xe49b69c1, 0xefbe4786,
    0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147,
    0x06ca6351, 0x14292967, 0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13,
    0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85, 0xa2bfe8a1, 0xa81a664b,
    0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a,
    0x5b9cca4f, 0x682e6ff3, 0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208,
    0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2};
} // namespace

void sha256_transform(std::array<uint32_t, 8> &state,
                      std::array<uint32_t, 16> const &data, int rounds) noexcept
{
	assert(0 <= rounds && rounds <= 64);

	uint32_t m[64];

	for (size_t i = 0; i < 16; ++i)
		m[i] = bswap(data[i]);
	for (size_t i = 16; i < 64; ++i)
		m[i] = sig1(m[i - 2]) + m[i - 7] + sig0(m[i - 15]) + m[i - 16];

	uint32_t a = state[0];
	uint32_t b = state[1];
	uint32_t c = state[2];
	uint32_t d = state[3];
	uint32_t e = state[4];
	uint32_t f = state[5];
	uint32_t g = state[6];
	uint32_t h = state[7];

	for (int i = 0; i < rounds; ++i)
	{
		uint32_t t1 = h + ep1(e) + ch(e, f, g) + sha256_k[i] + m[i];
		uint32_t t2 = ep0(a) + maj(a, b, c);
		h = g;
		g = f;
		f = e;
		e = d + t1;
		d = c;
		c = b;
		b = a;
		a = t1 + t2;
	}

	state[0] += a;
	state[1] += b;
	state[2] += c;
	state[3] += d;
	state[4] += e;
	state[5] += f;
	state[6] += g;
	state[7] += h;
}

std::array<std::byte, 32> sha256(std::span<const std::byte> data,
                                 int rounds) noexcept
{
	assert(0 <= rounds && rounds <= 64);
	union
	{
		std::array<uint32_t, 8> state;
		std::array<std::byte, 32> ret;
	};

	state[0] = 0x6a09e667;
	state[1] = 0xbb67ae85;
	state[2] = 0x3c6ef372;
	state[3] = 0xa54ff53a;
	state[4] = 0x510e527f;
	state[5] = 0x9b05688c;
	state[6] = 0x1f83d9ab;
	state[7] = 0x5be0cd19;

	const uint8_t *buf = (uint8_t *)data.data();

	size_t blocks = data.size() / (4 * 16);
	size_t tail = data.size() % (4 * 16);

	// process whole blocks
	for (size_t i = 0; i < blocks; ++i)
	{
		sha256_transform(state, *(std::array<uint32_t, 16> *)buf, rounds);
		buf += 4 * 16;
	}

	// process last (incomplete) block (which might be empty apart from padding)
	union
	{
		std::array<uint32_t, 16> tmp = {0, 0, 0, 0, 0, 0, 0, 0,
		                                0, 0, 0, 0, 0, 0, 0, 0};
		std::array<uint8_t, 4 * 16> tmp_bytes;
	};
	memcpy(tmp.data(), buf, tail);

	// NOTE: the trailing 0x80 always fits in the last (incomplete) block,
	//       but the trailing size might not
	tmp_bytes[tail] = 0x80; // this always fits in last block

	if (tail >= 56)
	{
		sha256_transform(state, tmp, rounds);
		memset(&tmp, 0, 4 * 16);
	}

	// append size (in bits!)
	size_t bitlen = 8 * data.size();
	tmp_bytes[63] = (uint8_t)(bitlen);
	tmp_bytes[62] = (uint8_t)(bitlen >> 8);
	tmp_bytes[61] = (uint8_t)(bitlen >> 16);
	tmp_bytes[60] = (uint8_t)(bitlen >> 24);
	tmp_bytes[59] = (uint8_t)(bitlen >> 32);
	tmp_bytes[58] = (uint8_t)(bitlen >> 40);
	tmp_bytes[57] = (uint8_t)(bitlen >> 48);
	tmp_bytes[56] = (uint8_t)(bitlen >> 56);
	sha256_transform(state, tmp, rounds);

	for (size_t i = 0; i < 8; ++i)
		state[i] = bswap(state[i]);

	// result = state
	return ret;
}

std::string hex_string(std::span<const std::byte> h)
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

static_assert(std::is_same_v<in_param<int>, int>);
static_assert(std::is_same_v<in_param<int *>, int *>);
static_assert(std::is_same_v<in_param<std::string>, std::string_view>);
static_assert(
    std::is_same_v<in_param<std::vector<int>>, std::vector<int> const &>);
static_assert(std::is_same_v<in_param<std::string_view>, std::string_view>);

// counterintuitivley, std::pair<int,int> is not std::trivially_copyable
// static_assert(
//  std::is_same_v<in_param<std::pair<int, int>>, std::pair<int, int>>);

} // namespace util
