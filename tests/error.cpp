#include "catch2/catch_test_macros.hpp"

#include "util/error.h"

TEST_CASE("error handling utils", "[error]")
{
	SECTION("check")
	{
		REQUIRE_NOTHROW(util::check(true));
		REQUIRE_THROWS_AS(util::check(false), std::runtime_error);

		REQUIRE_NOTHROW(util::check_non_negative(0));
		REQUIRE_THROWS_AS(util::check_non_negative(-1), std::runtime_error);

		REQUIRE_NOTHROW(util::check_positive(1));
		REQUIRE_THROWS_AS(util::check_positive(0), std::runtime_error);

		REQUIRE_NOTHROW(util::assume(true));
	}

	SECTION("check value forwarding")
	{
		int a = 5;
		auto b = util::check(a);
		b = 6;
		REQUIRE(a == 5);
		auto &c = util::check(a);
		c = 7;
		REQUIRE(a == 7);
		(void)b;
		(void)c;
	}
}
