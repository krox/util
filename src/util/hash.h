#pragma once

#include "util/span.h"
#include <array>
#include <cstddef>
#include <string_view>
#include <type_traits>
#include <vector>

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
			state_bytes_[pos_++] ^= *buf++;
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

// convenience function for pretty printing hash sums
std::string hex_string(span<const std::byte>);

// FNV by Fowler, Noll, Vo. This is the "FNV-1a", 64 bit version.
// public domain code adapted from
//     isthe.com/chongo/tech/comp/fnv/index.html
class Fnv1a
{
	uint64_t state_ = 14695981039346656037u;

  public:
	Fnv1a() = default;
	explicit Fnv1a(uint64_t seed) noexcept
	{
		// NOTE: afaik, the authors of FNV do not discuss seeding, but this is
		//       the obvious 'not wrong' way to do it.
		operator()(&seed, sizeof(seed));
	}

	void operator()(void const *buf, size_t size) noexcept
	{
		static constexpr uint64_t m = 1099511628211u;
		auto p = static_cast<uint8_t const *>(buf);
		for (size_t i = 0; i < size; ++i)
			state_ = (state_ ^ p[i]) * m;
	}

	explicit operator uint64_t() noexcept { return state_; }
};

// MurmurHash3 by Austin Appleby. This is the 128-bit, x64 version.
// public domain code adapted from
//     github.com/aappleby/smhasher/blob/master/src/MurmurHash3.cpp
class Murmur3
{
	union // hash state
	{
		std::array<uint64_t, 2> h_ = {0, 0};
		std::array<std::byte, 16> ret_;
	};
	size_t len_ = 0; // bytes taken in so for
	union            // incomplete not-yet-processed data
	{
		std::array<uint64_t, 2> block_ = {0, 0};
		std::array<uint8_t, 16> block_bytes_;
	};

	static constexpr uint64_t c1 = 0x87c37b91114253d5u;
	static constexpr uint64_t c2 = 0x4cf5ad432745937fu;

	static constexpr uint64_t rotl(uint64_t x, int y)
	{
		return (x << y) | (x >> (64 - y));
	}

	static constexpr uint64_t fmix64(uint64_t k)
	{
		k ^= k >> 33;
		k *= 0xff51afd7ed558ccdu;
		k ^= k >> 33;
		k *= 0xc4ceb9fe1a85ec53u;
		k ^= k >> 33;
		return k;
	}

  public:
	Murmur3() = default;

	explicit Murmur3(uint64_t seed) noexcept : h_{seed, seed}
	{
		// NOTE: this seeding is suggested by the author of MurmurHash, though
		//       it only allows 32 bit seeds (dont know why, presumably only
		//       for the sake of uniformity with the 32 bit version)
	}

	void operator()(void const *buf, size_t size) noexcept
	{
		auto data = (uint8_t const *)buf;

		if (len_ & 15) // previous incomplete block
		{
			size_t head = 16 - (len_ & 15); // bytes needed to complete block

			if (size < head)
			{
				for (size_t i = 0; i < size; ++i)
					block_bytes_[(len_ & 15) + i] = data[i];
				len_ += size;
				return;
			}

			// finish previous block
			for (size_t i = 0; i < head; ++i)
				block_bytes_[(len_ & 15) + i] = data[i];
			h_[0] ^= rotl(block_[0] * c1, 31) * c2;
			h_[0] = (rotl(h_[0], 27) + h_[1]) * 5 + 0x52dce729;
			h_[1] ^= rotl(block_[1] * c2, 33) * c1;
			h_[1] = (rotl(h_[1], 31) + h_[0]) * 5 + 0x38495ab5;

			len_ += head;
			size -= head;
			data += head;

			assert((len_ & 15) == 0);
		}

		len_ += size;
		auto blocks = (uint64_t const *)data;
		size_t nblocks = size / 16;
		auto tail = data + nblocks * 16;

		for (size_t i = 0; i < nblocks; i++)
		{
			h_[0] ^= rotl(blocks[2 * i] * c1, 31) * c2;
			h_[0] = (rotl(h_[0], 27) + h_[1]) * 5 + 0x52dce729;
			h_[1] ^= rotl(blocks[2 * i + 1] * c2, 33) * c1;
			h_[1] = (rotl(h_[1], 31) + h_[0]) * 5 + 0x38495ab5;
		}

		// std::array<uint64_t, 2> k = {0, 0};
		assert((size & 15) == (len_ & 15));
		std::memcpy(&block_, tail, size & 15);
	}

	void finalize() noexcept
	{
		// last block can be partially/completely empty, then this is a no-op
		for (size_t i = (len_ & 15); i < 16; ++i)
			block_bytes_[i] = 0;
		h_[0] ^= rotl(block_[0] * c1, 31) * c2;
		h_[1] ^= rotl(block_[1] * c2, 33) * c1;

		h_[0] ^= len_;
		h_[1] ^= len_;

		h_[0] += h_[1];
		h_[1] += h_[0];

		h_[0] = fmix64(h_[0]);
		h_[1] = fmix64(h_[1]);

		h_[0] += h_[1];
		h_[1] += h_[0];
	}

	explicit operator uint64_t() noexcept
	{
		finalize();
		return h_[0];
	}

	explicit operator std::array<std::byte, 16>() noexcept
	{
		finalize();
		return ret_;
	}
};

inline std::array<std::byte, 16> murmur3_128(span<const std::byte> data)
{
	Murmur3 m;
	m(data.data(), data.size());
	return (std ::array<std::byte, 16>)m;
}

// convenience overload for strings
inline auto murmur3_128(std::string_view s)
{
	return murmur3_128(span<const std::byte>((std::byte *)s.data(), s.size()));
}

// alternative to std::hash:
//     * is non-trivial by default even for basic integer types, so
//           unordered_map<int, hash<int>>
//       is okay to use
//     * provides specializations for more types such as std::pair, std::vector
//     * allows to switch the underlying hashing algorithm, thus decoupling
//       hashable types from hashing algorithms. This is inspired by
//           http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2014/n3980.html
//       though in contrast to that proposal, this hash is still strongly typed
//       and not 'universal'. I think the latter is unsafe when used for
//       heterogeneous lookups.
//     * to make a new type hashable, you can either define
//         template<> util::is_contiguously_hashable<MyType> : std::true_type{};
//       or implement
//         template<class HashAlgorithm>
//         void hash_append(HashAlgorithm& h, MyType a) noexcept { ... }
//       which should be findable by ADL

template <class T>
struct is_contiguously_hashable
    : std::integral_constant<bool, std::is_integral_v<T> || std::is_enum_v<T> ||
                                       std::is_pointer_v<T>>
{};
template <class T>
static constexpr bool is_contiguously_hashable_v =
    is_contiguously_hashable<T>::value;

template <class T1, class T2>
struct is_contiguously_hashable<std::pair<T1, T2>>
    : std::integral_constant<bool, is_contiguously_hashable_v<T1> &&
                                       is_contiguously_hashable_v<T2> &&
                                       sizeof(T1) + sizeof(T2) ==
                                           sizeof(std::pair<T1, T2>)>
{};

template <class... Ts>
struct is_contiguously_hashable<std::tuple<Ts...>>
    : std::integral_constant<bool, (is_contiguously_hashable_v<Ts> && ...) &&
                                       ((sizeof(Ts) + ...) ==
                                        sizeof(std::tuple<Ts...>))>
{};

template <class HashAlgorithm, class T>
std::enable_if_t<is_contiguously_hashable_v<T>> hash_append(HashAlgorithm &h,
                                                            T const &x) noexcept
{
	h(&x, sizeof(T));
}

template <class HashAlgorithm, class T, class A>
void hash_append(HashAlgorithm &h, std::vector<T, A> const &a) noexcept
{
	hash_append(h, a.size());

	if constexpr (is_contiguously_hashable_v<T>)
		h(a.data(), a.size() * sizeof(T));
	else
		for (auto &x : a)
			hash_append(h, x);
}

template <class HashAlgorithm, class T1, class T2>
std::enable_if_t<!is_contiguously_hashable_v<std::pair<T1, T2>>>
hash_append(HashAlgorithm &h, std::pair<T1, T2> const &a) noexcept
{
	hash_append(h, a.first);
	hash_append(h, a.second);
}

template <class HashAlgorithm, class T, size_t N>
std::enable_if_t<!is_contiguously_hashable_v<std::array<T, N>>>
hash_append(HashAlgorithm &h, std::array<T, N> const &a) noexcept
{
	for (auto &x : a)
		hash_append(h, x);
}

template <class HashAlgorithm>
void hash_append(HashAlgorithm &h, std::string_view x) noexcept
{
	hash_append(h, x.size());
	h(x.data(), x.size() * sizeof(x[0]));
}

template <class T, class HashAlgorithm = Fnv1a> struct hash
{
	size_t operator()(T const &x) const noexcept
	{
		HashAlgorithm h;
		hash_append(h, x);
		return static_cast<size_t>(h);
	}
};

template <class T, class HashAlgorithm = Fnv1a> class seeded_hash
{
	size_t seed_;

  public:
	seeded_hash(size_t seed) : seed_(seed) {}
	size_t operator()(T const &x) const noexcept
	{
		HashAlgorithm h(seed_);
		hash_append(h, x);
		return static_cast<size_t>(h);
	}
};

} // namespace util
