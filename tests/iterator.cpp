#include "catch2/catch_test_macros.hpp"

#include "util/iterator.h"
#include <vector>

#include <iostream>

TEST_CASE("filter_iterator", "[iterator]")
{
	auto is_even = [](auto x) -> bool { return x % 2 == 0; };
	auto is_positive = [](auto x) -> bool { return x > 0; };

	SECTION("empty range")
	{
		auto v = std::vector<int>{};
		auto begin = util::filter_iterator(is_even, v.begin(), v.end());
		CHECK(std::vector(begin, begin.end()) == std::vector<int>{});
	}

	SECTION("none true")
	{
		auto v = std::vector{1, 3, 5};
		auto begin = util::filter_iterator(is_even, v.begin(), v.end());
		CHECK(std::vector(begin, begin.end()) == std::vector<int>{});
	}

	SECTION("all true")
	{
		auto v = std::vector{2, 4, 6, 8};
		auto begin = util::filter_iterator(is_even, v.begin(), v.end());
		CHECK(std::vector(begin, begin.end()) == std::vector{2, 4, 6, 8});
	}

	SECTION("first/last false")
	{
		auto v = std::vector{1, 2, 3, 4, 5, 6, 7};
		auto begin = util::filter_iterator(is_even, v.begin(), v.end());
		CHECK(std::vector(begin, begin.end()) == std::vector{2, 4, 6});
	}

	SECTION("cascade")
	{
		auto v = std::vector{1, -1, 2, -2, 3, -3, 4, -4, 5, -5};
		auto even = util::filter_iterator(is_even, v.begin(), v.end());
		auto begin = util::filter_iterator(is_positive, even, even.end());
		CHECK(std::vector(begin, begin.end()) == std::vector{2, 4});
	}

	SECTION("still writable")
	{
		auto v = std::vector{1, -1, 2, -2, 3, -3, 4, -4, 5, -5};
		auto even = util::filter_iterator(is_even, v.begin(), v.end());
		auto begin = util::filter_iterator(is_positive, even, even.end());
		for (auto it = begin; it != begin.end(); ++it)
			*it = 0;
		CHECK(v == std::vector{1, -1, 0, -2, 3, -3, 0, -4, 5, -5});
	}
}
