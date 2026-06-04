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

	SECTION("bit_matrix")
	{
		constexpr size_t width = util::bit_matrix::limb_bits + 3;
		util::bit_matrix m(2, width);

		m[0].clear();
		m[1].clear(true);

		CHECK(m.height() == 2);
		CHECK(m.width() == width);
		CHECK(m[1].all());
		CHECK(m[1].count() == width);

		m[0][0] = true;
		m[0][util::bit_matrix::limb_bits] = true;
		CHECK(m[0].count() == 2);
		CHECK(m.count() == width + 2);
	}
}
