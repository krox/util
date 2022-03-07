#include "catch2/catch_test_macros.hpp"

#include "util/hash_map.h"
#include "util/random.h"
#include <unordered_map>

TEST_CASE("hash map fuzzer", "[hash_map]")
{
	util::hash_map<int, int> a;
	std::unordered_map<int, int> b;
	auto rng = util::xoshiro256();
	for (int i = 0; i < 10000; ++i)
	{
		REQUIRE(a.size() == b.size());
		switch (rng() % 3)
		{
		case 0: {
			int key = rng() % 100;
			int value = rng() % 1000;
			auto ra = a.insert({key, value});
			auto rb = b.insert({key, value});
			REQUIRE(ra.second == rb.second);
			REQUIRE(ra.first->first == key);
			REQUIRE(rb.first->first == key);
		}
		break;
		case 1: {

			int key = rng() % 100;
			REQUIRE(a.count(key) == b.count(key));
			if (a.count(key))
				REQUIRE(a[key] == b[key]);
		}
		break;
		case 2: {
			int key = rng() % 100;
			auto ra = a.erase(key);
			auto rb = b.erase(key);
			REQUIRE(ra == rb);
		}
		break;
		}
	}

	for (auto [k, v] : a)
	{
		REQUIRE(b.count(k));
		REQUIRE(b[k] == v);
	}
	for (auto [k, v] : b)
	{
		REQUIRE(a.count(k));
		REQUIRE(a[k] == v);
	}
}

TEST_CASE("hash map with pattern in the keys", "[hash_map][hash]")
{
	// the default hash should be non-trivial, thus working even if the entropy
	// is badly distributed in the keys

	util::hash_map<int64_t, int> m;
	std::vector<std::pair<int64_t, int>> v;
	for (size_t i = 0; i < 100; ++i)
		v.push_back({(int64_t)i << 48, i});
	REQUIRE_NOTHROW(m.insert(v.begin(), v.end()));
}

TEST_CASE("hash map with very bad hasher", "[hash_map][hash]")
{
	// util::hash_map provides some protection from completely degenerate
	// hash functions by throwing after some threshold of collisions

	struct BadHash
	{
		void operator()(void const *, size_t) noexcept {}
		operator size_t() noexcept { return 0; }
	};

	util::hash_map<int, int, util::hash<int, BadHash>> m;
	std::vector<std::pair<int, int>> v;
	for (size_t i = 0; i < 100; ++i)
		v.push_back({i, i});
	REQUIRE_THROWS(m.insert(v.begin(), v.end()));
}

TEST_CASE("hash map with seeded hasher", "[hash_map][hash]")
{
	auto m0 = std::unordered_map<int, int>();
	auto m1 = util::hash_map<int, int, util::seeded_hash<int>>(
	    util::seeded_hash<int>(1));
	auto m2 = util::hash_map<int, int, util::seeded_hash<int>>(
	    util::seeded_hash<int>(2));
	util::xoshiro256 rng;

	rng.seed(0);
	for (int i = 0; i < 100; ++i)
		m0[(int)rng() % 100] = (int)rng();
	rng.seed(0);
	for (int i = 0; i < 100; ++i)
		m1[(int)rng() % 100] = (int)rng();
	rng.seed(0);
	for (int i = 0; i < 100; ++i)
		m2[(int)rng() % 100] = (int)rng();

	auto v0 = std::vector<std::pair<int, int>>(m0.begin(), m0.end());
	auto v1 = std::vector<std::pair<int, int>>(m1.begin(), m1.end());
	auto v2 = std::vector<std::pair<int, int>>(m2.begin(), m2.end());
	CHECK(v1 != v2);

	std::sort(v0.begin(), v0.end());
	std::sort(v1.begin(), v1.end());
	std::sort(v2.begin(), v2.end());
	CHECK(v0 == v1);
	CHECK(v1 == v2);
}
