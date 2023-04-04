#include "catch2/catch_template_test_macros.hpp"
#include "catch2/catch_test_macros.hpp"

#include "util/complex.h"
#include <array>
#include <cmath>

TEST_CASE("complex operations", "[complex]")
{
	using C = util::complex<double>;
	auto i = C(0, 1);

	CHECK(i * i == C(-1));
	CHECK(C(1) * C(1) == C(1));
	CHECK(C(1) * i == i);
	CHECK(i + i == 2 * i);
}

TEST_CASE("quaternion operations", "[quaternions]")
{
	using Q = util::quaternion<double>;
	auto i = Q(0, 1, 0, 0);
	auto j = Q(0, 0, 1, 0);
	auto k = Q(0, 0, 0, 1);
	CHECK(i * i == -1);
	CHECK(j * j == -1);
	CHECK(i * j == k);
	CHECK(j * i == -k);
	CHECK(j * k == i);
	CHECK(k * j == -i);
	CHECK(i * k == -j);
	CHECK(k * i == j);
}