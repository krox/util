#include "catch2/catch_approx.hpp"
#include "catch2/catch_test_macros.hpp"

#include "util/numerics.h"

#include "util/random.h"
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

TEST_CASE("fsum")
{
	CHECK(fsum(std::array{1e30, 1e-30, -1e30}) == 1e-30);

	util::xoshiro256 rng;

	auto seed = 12094;
	FSum f(1.23456);

	rng.seed(seed);
	for (int i = 0; i < 1000; ++i)
		f += rng.uniform() * exp(rng.uniform() * 10);

	rng.seed(seed);
	for (int i = 0; i < 1000; ++i)
		f -= rng.uniform() * exp(rng.uniform() * 10);

	CHECK(double(f) == 1.23456);
}