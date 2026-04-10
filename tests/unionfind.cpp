#include "catch2/catch_test_macros.hpp"

#include "util/unionfind.h"

#include <array>

using namespace util;

namespace {

std::vector<int> to_vector(std::span<const int> values)
{
	return {values.begin(), values.end()};
}

} // namespace

TEST_CASE("union find join and query", "[unionfind]")
{
	auto uf = UnionFind(7);

	CHECK(uf.size() == 7);
	CHECK(uf.component_count() == 7);
	CHECK(uf.singleton_count() == 7);
	CHECK(uf.large_count() == 0);
	CHECK_FALSE(uf.is_joined(1, 4));
	CHECK(uf.join(1, 4));
	CHECK_FALSE(uf.join(4, 1));
	CHECK(uf.is_joined(1, 4));
	CHECK(uf.component_size(1) == 2);
	CHECK(uf.component_size(4) == 2);
	CHECK(uf.component_count() == 6);
	CHECK(uf.singleton_count() == 5);
	CHECK(uf.large_count() == 1);

	std::array<int, 3> group = {2, 5, 6};
	uf.join(group);
	CHECK(uf.is_joined(2, 6));
	CHECK(uf.component_size(5) == 3);
	CHECK(uf.component_count() == 4);
	CHECK(uf.singleton_count() == 2);
	CHECK(uf.large_count() == 2);
}

TEST_CASE("union find components groups isolated nodes separately",
          "[unionfind]")
{
	auto uf = UnionFind(8);
	uf.join(1, 4);
	uf.join(4, 5);
	uf.join(2, 6);

	auto comps = uf.components();
	CHECK(comps.component_count() == 5);
	CHECK(to_vector(comps.singletons()) == std::vector{0, 3, 7});
	CHECK(to_vector(comps.component(0)) == std::vector{1, 4, 5});
	CHECK(to_vector(comps.component(1)) == std::vector{2, 6});
}

TEST_CASE("union find clear resets structure", "[unionfind]")
{
	auto uf = UnionFind(5);
	uf.join(0, 1);
	uf.join(2, 3);

	uf.clear();

	CHECK(uf.component_count() == 5);
	CHECK(uf.singleton_count() == 5);
	CHECK(uf.large_count() == 0);
	for (int i = 0; i < 5; ++i)
		CHECK(uf.component_size(i) == 1);
	CHECK(to_vector(uf.components().singletons()) ==
	      std::vector{0, 1, 2, 3, 4});
}

TEST_CASE("default constructed union find has no components", "[unionfind]")
{
	auto comps = UnionFind().components();
	CHECK(comps.component_count() == 0);
	CHECK(comps.singletons().empty());
}