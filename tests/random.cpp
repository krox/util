#include "catch2/catch_approx.hpp"
#include "catch2/catch_test_macros.hpp"

#include "util/random.h"
#include "util/stats.h"

using namespace util;

TEST_CASE("random number generators")
{
	xoshiro256 rng = {};
	std::vector<double> values;
	size_t n = 1000000;
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

	values.clear();
	while (values.size() < n)
		values.push_back(rng.bernoulli());
	CHECK(mean(values) == Catch::Approx(0.5).epsilon(0.01));
	CHECK(variance(values) == Catch::Approx(0.25).epsilon(0.01));

	values.clear();
	while (values.size() < n)
		values.push_back(rng.bernoulli(0.3));
	CHECK(mean(values) == Catch::Approx(0.3).epsilon(0.01));
	CHECK(variance(values) == Catch::Approx(0.3 * (1 - 0.3)).epsilon(0.01));

	values.clear();
	while (values.size() < n)
		values.push_back(rng.exponential(1.5));
	CHECK(mean(values) == Catch::Approx(1 / 1.5).epsilon(0.01));
	CHECK(variance(values) == Catch::Approx(1 / (1.5 * 1.5)).epsilon(0.01));

	values.clear();
	while (values.size() < n)
		values.push_back(rng.poisson(1.5));
	CHECK(mean(values) == Catch::Approx(1.5).epsilon(0.01));
	CHECK(variance(values) == Catch::Approx(1.5).epsilon(0.01));

	values.clear();
	while (values.size() < n)
		values.push_back(rng.binomial(5, 0.3));
	CHECK(mean(values) == Catch::Approx(5 * 0.3).epsilon(0.01));
	CHECK(min(values) == 0);
	CHECK(max(values) == 5);
	CHECK(variance(values) == Catch::Approx(5 * 0.3 * (1 - 0.3)).epsilon(0.01));
}
