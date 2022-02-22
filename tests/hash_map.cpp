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
