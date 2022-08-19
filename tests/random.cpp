#include "catch2/catch_approx.hpp"
#include "catch2/catch_test_macros.hpp"

#include "util/random.h"
#include "util/stats.h"

//#include "util/gnuplot.h"

using namespace util;

template <typename Dist> void test_distribution(Dist dist, int l = 4)
{
	xoshiro256 rng = {};
	size_t n = 1'000'000;
	Estimator<1> est;

	for (size_t i = 0; i < n; ++i)
		est.add(dist(rng));
	if (l >= 1)
		CHECK(est.mean() ==
		      Catch::Approx(dist.mean()).epsilon(0.01).margin(0.01));
	if (l >= 2)
		CHECK(est.variance() ==
		      Catch::Approx(dist.variance()).epsilon(0.01).margin(0.01));
	if (l >= 3)
		CHECK(est.skewness() ==
		      Catch::Approx(dist.skewness()).epsilon(0.01).margin(0.01));
	if (l >= 4)
		CHECK(est.kurtosis() ==
		      Catch::Approx(dist.exkurtosis()).epsilon(0.01).margin(0.01));

	// CHECK(min(values) >= dist.min());
	// CHECK(max(values) <= dist.max());

	// Gnuplot().plotHistogram(Histogram(values, 50));
}

TEST_CASE("random number distributions", "[random]")
{
	test_distribution(uniform_distribution(1.5, 4.8));
	test_distribution(normal_distribution(-2.1, 0.8));
	test_distribution(exponential_distribution(1.7));
	test_distribution(bernoulli_distribution(0.15));
	test_distribution(binomial_distribution(20, 0.3));

	test_distribution(Autoregressive({0.8}, {0.1, 0.8}));
	test_distribution(Autoregressive({0.5, 0.3}, {0.1, 0.8}));
	test_distribution(Autoregressive({0.5, -0.3}, {10., 3}));
	test_distribution(Autoregressive({-0.1, 0.2}, {-6, 1}));
	// test_distribution(canonical_quartic_exponential_distribution(1.0, 2.0));
}

template <typename T> void test_int_dist(T a, T b)
{
	xoshiro256 rng = {};
	T min = b;
	T max = a;
	for (size_t i = 0; i < 50 * size_t(b - a + 1); ++i)
	{
		T x = rng.uniform<T>(a, b);
		CHECK(a <= x);
		CHECK(x <= b);
		min = std::min(min, x);
		max = std::max(max, x);
	}

	CHECK(min == a);
	CHECK(max == b);
}

TEST_CASE("integer unifom distribution", "[random]")
{
	test_int_dist<int>(-19, -1);
	test_int_dist<int>(-3, 5);
	test_int_dist<int>(2, 17);
	test_int_dist<int64_t>(-19, -1);
	test_int_dist<int64_t>(-3, 5);
	test_int_dist<int64_t>(2, 17);
	test_int_dist<uint8_t>(0, 255);
	test_int_dist<int8_t>(-128, 127);
}
