#include "catch2/catch_approx.hpp"
#include "catch2/catch_test_macros.hpp"

#include "util/random.h"
#include "util/stats.h"

//#include "util/gnuplot.h"

using namespace util;

template <typename Dist> void test_distribution(Dist dist)
{
	xoshiro256 rng = {};
	std::vector<double> values;
	size_t n = 1000000;
	values.reserve(n);
	while (values.size() < n)
		values.push_back(dist(rng));
	CHECK(mean(values) == Catch::Approx(dist.mean()).epsilon(0.01));
	CHECK(variance(values) == Catch::Approx(dist.variance()).epsilon(0.01));
	CHECK(min(values) >= dist.min());
	CHECK(max(values) <= dist.max());

	// Gnuplot().plotHistogram(Histogram(values, 50));
}

TEST_CASE("random number generators")
{
	test_distribution(uniform_distribution(1.5, 4.8));
	test_distribution(normal_distribution(-2.1, 0.8));
	test_distribution(exponential_distribution(1.7));
	test_distribution(bernoulli_distribution(0.15));
	test_distribution(binomial_distribution(20, 0.3));
	// test_distribution(canonical_quartic_exponential_distribution(1.0, 2.0));
}
