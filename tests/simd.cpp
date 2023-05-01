#include "catch2/catch_template_test_macros.hpp"
#include "catch2/catch_test_macros.hpp"

#include "util/simd.h"

#include "util/random.h"
#include <array>
#include <cmath>

TEMPLATE_TEST_CASE("simd basic arithmetic", "[simd]", util::vfloat4,
                   util::vfloat8, util::vdouble2, util::vdouble4)
{
	using Vec = TestType;
	util::xoshiro256 rng;

	auto test_binary = [&](auto f) {
		std::array<typename Vec::value_type, Vec::size()> A, B, C;
		Vec a, b, c;
		for (size_t i = 0; i < (int)Vec::size(); ++i)
		{
			A[i] = rng.uniform();
			B[i] = rng.uniform();
			C[i] = f(A[i], B[i]);
			a = vinsert(a, i, A[i]);
			b = vinsert(b, i, B[i]);
			c = vinsert(c, i, C[i]);
		}
		CHECK(all_of(f(a, b) == c));
		for (int i = 0; i < (int)Vec::size(); ++i)
			CHECK(f(A[i], B[i]) == vextract(c, i));
	};

	auto test_unary = [&](auto f) {
		std::array<typename Vec::value_type, Vec::size()> A, C;
		Vec a, c;
		for (size_t i = 0; i < (int)Vec::size(); ++i)
		{
			A[i] = rng.uniform();
			C[i] = f(A[i]);
			a = vinsert(a, i, A[i]);
			c = vinsert(c, i, C[i]);
		}
		CHECK(all_of(f(a) == c));
		for (int i = 0; i < (int)Vec::size(); ++i)
			CHECK(f(A[i]) == vextract(c, i));
	};

	using std::sqrt, std::min, std::max;
	test_binary([](auto a, auto b) { return a + b; });
	test_binary([](auto a, auto b) { return a - b; });
	test_binary([](auto a, auto b) { return a * b; });
	test_binary([](auto a, auto b) { return a / b; });
	test_binary([](auto a, auto b) { return min(a, b); });
	test_binary([](auto a, auto b) { return max(a, b); });
	test_unary([](auto a) { return sqrt(a); });
}

TEST_CASE("simd operations", "[simd]")
{
	SECTION("permutations (sse)")
	{
		CHECK(fmt::format("{}", vpermute0(util::vfloat4(0.1, 0.2, 0.3, 0.4))) ==
		      "{0.2, 0.1, 0.4, 0.3}");
		CHECK(fmt::format("{}", vpermute1(util::vfloat4(0.1, 0.2, 0.3, 0.4))) ==
		      "{0.3, 0.4, 0.1, 0.2}");
		CHECK(fmt::format("{}", vpermute0(util::vdouble2(0.1, 0.2))) ==
		      "{0.2, 0.1}");
	}

	SECTION("permutations (avx)")
	{
		CHECK(fmt::format("{}", vpermute0(util::vfloat8(0.1, 0.2, 0.3, 0.4, 0.5,
		                                                0.6, 0.7, 0.8))) ==
		      "{0.2, 0.1, 0.4, 0.3, 0.6, 0.5, 0.8, 0.7}");
		CHECK(fmt::format("{}", vpermute1(util::vfloat8(0.1, 0.2, 0.3, 0.4, 0.5,
		                                                0.6, 0.7, 0.8))) ==
		      "{0.3, 0.4, 0.1, 0.2, 0.7, 0.8, 0.5, 0.6}");
		CHECK(fmt::format("{}", vpermute2(util::vfloat8(0.1, 0.2, 0.3, 0.4, 0.5,
		                                                0.6, 0.7, 0.8))) ==
		      "{0.5, 0.6, 0.7, 0.8, 0.1, 0.2, 0.3, 0.4}");
		CHECK(
		    fmt::format("{}", vpermute0(util::vdouble4(0.1, 0.2, 0.3, 0.4))) ==
		    "{0.2, 0.1, 0.4, 0.3}");
		CHECK(
		    fmt::format("{}", vpermute1(util::vdouble4(0.1, 0.2, 0.3, 0.4))) ==
		    "{0.3, 0.4, 0.1, 0.2}");
	}
}
