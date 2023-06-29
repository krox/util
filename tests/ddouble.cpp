#include "catch2/catch_test_macros.hpp"

#include "fmt/format.h"
#include "util/ddouble.h"
#include "util/random.h"
#include "util/stopwatch.h"
using util::ddouble;

namespace {

// prevent optimization while benchmarking
// static void escape(void *p) { asm volatile("" : : "g"(p) : "memory"); }
// static void clobber() { asm volatile("" : : : "memory"); }

template <class F> void test_unary(F &&f, double min = -10.0, double max = 10.0)
{
	util::xoshiro256 rng;
	for (int iter = 0; iter < 1000; ++iter)
	{
		auto x = ddouble::random(rng) * (max - min) + min;
		CHECK(abs(f(x)) < 1e-27);
	}
}

template <class F>
void test_binary(F &&f, double min = -10.0, double max = 10.0)
{
	util::xoshiro256 rng;
	for (int iter = 0; iter < 1000; ++iter)
	{
		auto x = ddouble::random(rng) * (max - min) + min;
		auto y = ddouble::random(rng) * (max - min) + min;
		CHECK(abs(f(x, y)) < 1e-25);
	}
}

TEST_CASE("ddouble identites", "[ddouble][numerics]")
{
	CHECK(sqrt(ddouble(4)) == 2);
	CHECK(cbrt(ddouble(-8)) == -2);
	CHECK(abs(sin(13 * ddouble::pi())) < 1e-25);

	test_unary(
	    [](ddouble a) {
		    auto b = sqrt(a);
		    return b * b - a;
	    },
	    0.0, 10.0);
	test_unary([](ddouble a) {
		auto b = cbrt(a);
		return b * b * b - a;
	});
	test_binary([](ddouble a, ddouble b) {
		return sin(a) * cos(b) + cos(a) * sin(b) - sin(a + b);
	});
	test_binary([](ddouble a, ddouble b) {
		return sin(a) * cos(b) - cos(a) * sin(b) - sin(a - b);
	});
	test_binary([](ddouble a, ddouble b) {
		return cos(a) * cos(b) - sin(a) * sin(b) - cos(a + b);
	});
	test_binary([](ddouble a, ddouble b) {
		return cos(a) * cos(b) + sin(a) * sin(b) - cos(a - b);
	});
	test_unary([](ddouble a) { return sin(a) / cos(a) - tan(a); });
	test_unary(
	    [](ddouble a) { return sin(a) * sin(a) + cos(a) * cos(a) - 1.0; });
	test_unary([](ddouble a) { return log(exp(a)) - a; });
}

} // namespace
