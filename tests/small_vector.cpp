#include "catch2/catch_test_macros.hpp"

#include "util/small_vector.h"

using namespace util;

template <size_t N> void test_small_vector()
{
	small_vector<int, N> a;
	REQUIRE(a.size() == 0);
	REQUIRE(a.empty());

	a.push_back(1);
	REQUIRE(a.size() == 1u);
	a.insert(a.end(), {2, 3});
	REQUIRE(a.size() == 3u);
	a.insert(a.begin(), 0);
	REQUIRE(a.size() == 4u);
	for (int i = 0; i < 4; ++i)
		REQUIRE(a[i] == i);
	a.reserve(20);
	REQUIRE(a.capacity() >= 20u);
}

TEST_CASE("small_vector")
{
	test_small_vector<0>();
	test_small_vector<1>();
	test_small_vector<2>();
	test_small_vector<3>();
	test_small_vector<4>();
	test_small_vector<5>();
}
