#include "catch2/catch_template_test_macros.hpp"
#include "catch2/catch_test_macros.hpp"

#include "util/bit_vector.h"

TEST_CASE("bit_vector", "[bits]")
{
	SECTION("misc")
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

	SECTION("operators")
	{
		util::bit_vector a(17, true);
		util::bit_vector b(17, false);
		CHECK((a | b).count() == 17);
		CHECK((a & b).count() == 0);
		CHECK((a ^ b).count() == 17);
	}

	SECTION("push_back")
	{
		util::bit_vector a;
		a.push_back(true);
		a.push_back(false);
		a.push_back(true);
		a.push_back(true);
		a.push_back(true);
		a.push_back(false);
		CHECK(a.size() == 6);
		CHECK(a.count() == 4);
	}

	SECTION("auto_resize")
	{
		util::bit_map a;
		a[17] = true;
		CHECK(a.size() == 18);
		CHECK(a.count() == 1);
	}
}
