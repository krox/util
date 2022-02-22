#include "catch2/catch_approx.hpp"
#include "catch2/catch_test_macros.hpp"

#include "util/random.h"
#include "util/stats.h"

using namespace util;

TEST_CASE("random number generators")
{
	xoshiro256 rng = {};
	std::vector<double> values;
	size_t n = 100000;
	values.reserve(n);

	while (values.size() < n)
		values.push_back(rng.uniform());
	CHECK(mean(values) == Catch::Approx(0.5).epsilon(0.01));
	CHECK(min(values) == Catch::Approx(0.0).margin(0.01));
	CHECK(max(values) == Catch::Approx(1.0).margin(0.01));
	CHECK(variance(values) == Catch::Approx(1. / 12.).epsilon(0.01));

	values.clear();
	while (values.size() < n)
		values.push_back(rng.normal());
	CHECK(mean(values) == Catch::Approx(0.0).margin(0.01));
	CHECK(min(values) > -10.);
	CHECK(max(values) < +10.);
	CHECK(variance(values) == Catch::Approx(1.0).epsilon(0.01));
}
