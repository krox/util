#include "util/series.h"
#include "catch2/catch_test_macros.hpp"

using namespace util;

TEST_CASE("series", "[series]")
{
	using S = Series<float, 3>;
	auto a = S(2);
	auto b = S::generator();

	// yes, the parentheses are not ideal. the whole "Series" class is a bit WIP
	REQURIE(std::format("{}", a * b) == "(2)*ε O(ε^3)");
}