#include "catch2/catch_approx.hpp"
#include "catch2/catch_test_macros.hpp"

#include "util/numerics.h"

#include <cmath>

using namespace util;

TEST_CASE("numerics")
{
	double pi = 3.141592653589793;
	CHECK(solve([](double x) { return std::sin(x); }, 3.0, 4.0) ==
	      Catch::Approx(pi));
	CHECK(integrate([](double x) { return std::sin(x); }, pi, 2 * pi) ==
	      Catch::Approx(-2.0));

	CHECK(integrate_hermite_15([](double x) { return std::exp(-x * x); }) ==
	      Catch::Approx(std::sqrt(pi)));
	CHECK(integrate_hermite_31([](double x) {
		      return x * x * std::exp(-x * x);
	      }) == Catch::Approx(std::sqrt(pi) / 2));
	CHECK(integrate_hermite_63([](double x) {
		      return std::exp(-x * x * x * x);
	      }) == Catch::Approx(1.812804954110954));
}
