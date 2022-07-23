#include "catch2/catch_template_test_macros.hpp"
#include "catch2/catch_test_macros.hpp"

#include "util/bit_vector.h"
#include "util/bits.h"

TEST_CASE("bit functions", "[bits]")
{
	CHECK(util::popcount(17) == 2);
	CHECK(util::parity(17) == 0);
	CHECK(util::parity(1024) == 1);
	CHECK(!util::is_pow2(0));
	CHECK(util::is_pow2(1));
	CHECK(util::is_pow2(2));
	CHECK(!util::is_pow2(3));
	CHECK(util::is_pow2(4));
	CHECK(!util::is_pow2(5));
	CHECK(util::round_up_pow2(0) == 1);
	CHECK(util::round_up_pow2(1) == 1);
	CHECK(util::round_up_pow2(2) == 2);
	CHECK(util::round_up_pow2(3) == 4);
	CHECK(util::round_up_pow2(4) == 4);
	CHECK(util::round_up_pow2(5) == 8);
	CHECK(util::round_down_pow2(1) == 1);
	CHECK(util::round_down_pow2(2) == 2);
	CHECK(util::round_down_pow2(3) == 2);
	CHECK(util::round_down_pow2(4) == 4);
	CHECK(util::round_down_pow2(5) == 4);
	CHECK(util::round_down_pow2(7) == 4);
	CHECK(util::round_down_pow2(8) == 8);
}

TEST_CASE("bit_vecto", "[bits]")
{
	CHECK(util::bit_vector(17).count() == 0);
	CHECK(util::bit_vector(17, true).count() == 17);

	util::bit_vector a;
	a.push_back(true);
	a.resize(13);
	CHECK(a.count(false) == 12);
	a.clear();
	CHECK(!a.any());
	a.clear(true);
	CHECK(a.all());
}
