#include "catch2/catch_test_macros.hpp"

#include "util/sorted_vector.h"
#include "util/vector.h"

#include <array>
#include <ranges>

TEST_CASE("is_sorted accepts non-decreasing ranges", "[sorted_vector]")
{
	constexpr auto sorted = std::to_array<int>({1, 2, 2, 4});
	constexpr auto unsorted = std::to_array<int>({1, 3, 2, 4});

	CHECK(util::is_sorted(sorted));
	CHECK_FALSE(util::is_sorted(unsorted));
}

TEST_CASE("set_intersection emits matching elements from arrays",
          "[sorted_vector]")
{
	constexpr auto a = std::to_array<int>({1, 2, 2, 4, 5});
	constexpr auto b = std::to_array<int>({2, 2, 2, 3, 5});

	auto out = util::vector<int>{};
	util::set_intersection(a, b, [&](int x) { out.push_back(x); });

	CHECK(out == util::vector<int>{2, 2, 5});
}

TEST_CASE("set_intersection iterator overload supports heterogeneous types",
          "[sorted_vector]")
{
	constexpr auto a = std::to_array<int>({1, 2, 4, 7});
	constexpr auto b = std::to_array<long>({0, 2, 4, 6, 7});

	auto out = util::vector<long>{};
	util::set_intersection(a.begin(), a.end(), b.begin(), b.end(),
	                       [&](auto x) { out.push_back(x); });

	CHECK(out == util::vector<long>{2, 4, 7});
}

TEST_CASE("set_intersection3 preserves minimum multiplicity", "[sorted_vector]")
{
	constexpr auto a = std::to_array<int>({1, 2, 2, 3, 5, 5});
	constexpr auto b = std::to_array<int>({2, 2, 2, 4, 5});
	constexpr auto c = std::to_array<int>({0, 2, 2, 5, 5, 5});

	auto out = util::vector<int>{};
	util::set_intersection3(a, b, c, [&](int x) { out.push_back(x); });

	CHECK(out == util::vector<int>{2, 2, 5});
}

TEST_CASE("set_intersection3 stops on empty input", "[sorted_vector]")
{
	constexpr auto a = std::to_array<int>({1, 2, 3});
	constexpr auto b = std::to_array<int>({1, 2, 3});
	constexpr auto c = std::array<int, 0>{};

	auto out = util::vector<int>{};
	util::set_intersection3(a, b, c, [&](int x) { out.push_back(x); });

	CHECK(out.empty());
}

TEST_CASE("set_intersection3 can split triple and pair matches",
          "[sorted_vector]")
{
	constexpr auto a = std::to_array<int>({1, 2, 4, 7, 10});
	constexpr auto b = std::to_array<int>({2, 3, 4, 8, 10});
	constexpr auto c = std::to_array<int>({1, 4, 5, 8, 10});

	auto out3 = util::vector<int>{};
	auto out2 = util::vector<int>{};
	util::set_intersection3(
	    a, b, c, [&](int x) { out3.push_back(x); },
	    [&](int x) { out2.push_back(x); });

	CHECK(out3 == util::vector<int>{4, 10});
	CHECK(out2 == util::vector<int>{1, 2, 8});
}

TEST_CASE("set_intersection3 two-callback overload falls back to pairs",
          "[sorted_vector]")
{
	constexpr auto a = std::to_array<int>({1, 3, 5});
	constexpr auto b = std::to_array<int>({1, 2, 5});
	constexpr auto c = std::array<int, 0>{};

	auto out3 = util::vector<int>{};
	auto out2 = util::vector<int>{};
	util::set_intersection3(
	    a, b, c, [&](int x) { out3.push_back(x); },
	    [&](int x) { out2.push_back(x); });

	CHECK(out3.empty());
	CHECK(out2 == util::vector<int>{1, 5});
}

TEST_CASE("set_union accepts arbitrary ranges", "[sorted_vector]")
{
	auto a = util::vector<int>{1, 2, 2, 4, 7};
	auto b = std::to_array<int>({0, 2, 3, 4, 4, 6, 9});
	auto b_mid = b | std::views::drop(1) | std::views::take(5);

	auto out = util::vector<int>{};
	util::set_union(a, b_mid, [&](int x) { out.push_back(x); });

	CHECK(out == util::vector<int>{1, 2, 2, 3, 4, 4, 6, 7});
}

TEST_CASE("callbacks can capture output state", "[sorted_vector]")
{
	constexpr auto a = std::to_array<int>({1, 2, 4});
	constexpr auto b = std::to_array<int>({2, 3, 4});

	int sum = 0;
	util::set_union(a, b, [&](int x) { sum += x; });

	CHECK(sum == 10);
}