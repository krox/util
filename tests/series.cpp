#include "util/series.h"
#include "catch2/catch_test_macros.hpp"

using namespace util;

TEST_CASE("series", "[series]")
{
	using S = Series<float, 3>;

	auto a = S(2);
	auto b = S::generator();
	CHECK(fmt::format("{}", a * b) == "[0, 2, 0]");

	a[0] = 1;
	a[1] = 2;
	a[2] = 3;
	b[0] = 4;
	b[1] = 5;
	b[2] = 6;
	CHECK(fmt::format("{}", a * b) == "[4, 13, 28]");
}