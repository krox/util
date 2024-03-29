#pragma once

#include "blake3.h"
#include <array>
#include <bit>
#include <cassert>
#include <cstddef>
#include <cstring>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

namespace util {

// SHA2-256
std::array<std::byte, 32> sha256(std::span<const std::byte>,
                                 int rounds = 64) noexcept;

// the Keccak function, that all of SHA3 is based on
void keccakf(std::array<uint64_t, 25> &s) noexcept;

// incremental interface
//     * put data in using 'hasher(ptr, len)'
//     * get hash out using 'hasher()' or 'hasher.generate_bytes(ptr, len)''
//     * calling the retrieving function multiple times advances the infinite
//       output stream
template <int digest_size = 256, uint8_t domain = 0x06> class Sha3
{
	static_assert(digest_size % 8 == 0);
	static_assert(8 <= digest_size && digest_size <= 512);
	static constexpr int bit_rate = 1600 - 2 * digest_size;
	static constexpr int byte_rate = bit_rate / 8;

	union
	{
		std::array<uint64_t, 25> state_;
		std::array<uint8_t, 25 * 8> state_bytes_;
	};
	int pos_ = 0; // byte-position into state. always in [0, byte_rate)
	bool finalized_ = false;

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

	Sha3(void const *data, size_t len) noexcept
	{
		state_.fill(0);
		(*this)(data, len);
	}

	explicit Sha3(std::string_view s) noexcept : Sha3(s.data(), s.size()) {}

	// process some data
	void operator()(void const *data, size_t len) noexcept
	{
		assert(!finalized_);
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

	// generate data
	void generate_bytes(void *data, size_t len) noexcept
	{
		if (!finalized_)
		{
			// last (incomplete) block might be empty apart from the padding
			state_bytes_[pos_] ^= (uint8_t)domain;
			state_bytes_[byte_rate - 1] ^= (uint8_t)0x80U;
			keccakf(state_);
			pos_ = 0;
			finalized_ = true;
		}

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

	std::array<std::byte, digest_size / 8> operator()() noexcept
	{
		std::array<std::byte, digest_size / 8> r;
		generate_bytes(r.data(), r.size());
		return r;
	}
};

// Blake3 cryptographic hash function with the same interface as Sha3 above.
// Calls the official C implementation, which should be really fast.
class Blake3
{
	blake3_hasher hasher_;
	uint64_t pos_ = 0; // position in output

  public:
	Blake3() noexcept { blake3_hasher_init(&hasher_); }
	Blake3(void const *data, size_t len) noexcept
	{
		blake3_hasher_init(&hasher_);
		(*this)(data, len);
	}
	explicit Blake3(std::string_view s) noexcept : Blake3(s.data(), s.size()) {}

	void operator()(void const *data, size_t len) noexcept
	{
		assert(pos_ == 0);
		blake3_hasher_update(&hasher_, data, len);
	}

	void generate_bytes(void *data, size_t len) noexcept
	{
		blake3_hasher_finalize_seek(&hasher_, pos_, (uint8_t *)data, len);
		pos_ += len;
	}

	std::array<std::byte, 32> operator()() noexcept
	{
		std::array<std::byte, 32> r;
		generate_bytes(r.data(), r.size());
		return r;
	}
};

// convenience functions for hashing in a single function call
template <int d>
std::array<std::byte, d / 8> sha3(std::span<const std::byte> data)
{
	return Sha3<d>(data.data(), data.size())();
}

inline std::array<std::byte, 32> blake3(std::span<const std::byte> data)
{
	return Blake3(data.data(), data.size())();
}
template <int d> auto sha3(std::string_view s)
{
	return sha3<d>(std::span((std::byte const *)s.data(), s.size()));
}
inline auto blake3(std::string_view s)
{
	return blake3(std::span((std::byte const *)s.data(), s.size()));
}

// convenience function for pretty printing hash sums
std::string hex_string(std::span<const std::byte>);

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

inline size_t fnv1a(std::string_view s)
{
	Fnv1a h;
	h(s.data(), s.size());
	return static_cast<size_t>(h);
}

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

	constexpr explicit Murmur3(uint64_t seed) noexcept : h_{seed, seed}
	{
		// NOTE: this seeding is suggested by the author of MurmurHash, though
		//       it only allows 32 bit seeds (dont know why, presumably only
		//       for the sake of uniformity with the 32 bit version)
	}

	constexpr void operator()(void const *buf, size_t size) noexcept
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
			h_[0] ^= std::rotl(block_[0] * c1, 31) * c2;
			h_[0] = (std::rotl(h_[0], 27) + h_[1]) * 5 + 0x52dce729;
			h_[1] ^= std::rotl(block_[1] * c2, 33) * c1;
			h_[1] = (std::rotl(h_[1], 31) + h_[0]) * 5 + 0x38495ab5;

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
			h_[0] ^= std::rotl(blocks[2 * i] * c1, 31) * c2;
			h_[0] = (std::rotl(h_[0], 27) + h_[1]) * 5 + 0x52dce729;
			h_[1] ^= std::rotl(blocks[2 * i + 1] * c2, 33) * c1;
			h_[1] = (std::rotl(h_[1], 31) + h_[0]) * 5 + 0x38495ab5;
		}

		// std::array<uint64_t, 2> k = {0, 0};
		assert((size & 15) == (len_ & 15));
		std::memcpy(&block_, tail, size & 15);
	}

	constexpr void finalize() noexcept
	{
		// last block can be partially/completely empty, then this is a no-op
		for (size_t i = (len_ & 15); i < 16; ++i)
			block_bytes_[i] = 0;
		h_[0] ^= std::rotl(block_[0] * c1, 31) * c2;
		h_[1] ^= std::rotl(block_[1] * c2, 33) * c1;

		h_[0] ^= len_;
		h_[1] ^= len_;

		h_[0] += h_[1];
		h_[1] += h_[0];

		h_[0] = fmix64(h_[0]);
		h_[1] = fmix64(h_[1]);

		h_[0] += h_[1];
		h_[1] += h_[0];
	}

	constexpr explicit operator uint64_t() noexcept
	{
		finalize();
		return h_[0];
	}

	constexpr explicit operator std::array<std::byte, 16>() noexcept
	{
		finalize();
		return ret_;
	}
};

constexpr inline std::array<std::byte, 16>
murmur3_128(std::span<const std::byte> data, uint64_t seed = 0) noexcept
{
	auto m = Murmur3(seed);
	m(data.data(), data.size());
	return static_cast<std ::array<std::byte, 16>>(m);
}

// convenience overload for strings
inline auto murmur3_128(std::string_view s, uint64_t seed = 0) noexcept
{
	return murmur3_128(std::span((std::byte const *)s.data(), s.size()), seed);
}

// helper type trait for 'in' parameters in generic code. For example:
// T           -> T const&
// int         -> int
// std::string -> std::string_view
template <class T> struct in_param_type
{
  private:
	// NOTE: 'trivial=true' should roughly mean that it can be passed in
	//       register(s). Though making this precise depends on platform and
	//       calling conventions of course.
	static constexpr bool trivial = std::is_trivially_copyable_v<T> &&
	                                std::is_trivially_destructible_v<T> &&
	                                sizeof(T) <= 2 * sizeof(size_t);

  public:
	using type = std::conditional_t<trivial, T, T const &>;
};

template <> struct in_param_type<std::string>
{
	using type = std::string_view;
};

template <class T> using in_param = typename in_param_type<T>::type;

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
	size_t operator()(in_param<T> x) const noexcept
	{
		HashAlgorithm h;
		hash_append(h, x);
		return static_cast<size_t>(h);
	}

	bool operator==(hash const &) const noexcept { return true; }
};

template <class T, class HashAlgorithm = Fnv1a> class seeded_hash
{
	size_t seed_;

  public:
	seeded_hash() noexcept : seed_(0) {}
	seeded_hash(size_t seed) noexcept : seed_(seed) {}
	size_t operator()(in_param<T> x) const noexcept
	{
		HashAlgorithm h(seed_);
		hash_append(h, x);
		return static_cast<size_t>(h);
	}

	bool operator==(seeded_hash const &other) const noexcept
	{
		return seed_ == other.seed_;
	}
};

} // namespace util
