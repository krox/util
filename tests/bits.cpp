#include "catch2/catch_template_test_macros.hpp"
#include "catch2/catch_test_macros.hpp"

#include "util/bit_vector.h"

TEST_CASE("bit_vector", "[bits]")
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
