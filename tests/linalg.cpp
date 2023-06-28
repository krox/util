#include "catch2/catch_template_test_macros.hpp"
#include "catch2/catch_test_macros.hpp"

#include "util/linalg.h"
#include "util/random.h"
#include "util/simd.h"

using util::vsum, util::vmax;

template <class T> using Matrix2 = util::Matrix<T, 2>;
template <class T> using Matrix3 = util::Matrix<T, 3>;

#define CHECK_EQ(A, B) CHECK(vmax(norm2((A) - (B))) < 1e-8)
#define CHECK_SLOPPY(A, B) CHECK(vmax(norm2((A) - (B))) < 1e-4)

TEMPLATE_PRODUCT_TEST_CASE("simple matrix identities", "[linalg]",
                           (Matrix2, Matrix3),
                           (float, double, util::complex<float>,
                            util::complex<double>, util::simd<float>))
{
	CHECK_EQ(TestType(1.0), TestType::identity());
	CHECK_EQ(TestType(0.0), TestType::zero());

	using std::exp;
	using util::norm2;
	auto rng = util::xoshiro256(12345);
	auto a = TestType::random_normal(rng);
	auto b = TestType::random_normal(rng);
	auto c = TestType::random_normal(rng);
	auto u = TestType::Vector::random_normal(rng);
	auto v = TestType::Vector::random_normal(rng);
	auto w = TestType::Vector::random_normal(rng);

	CHECK_EQ((u + v) + w, u + (v + w));
	CHECK_EQ((u + v) - u, v);
	CHECK_EQ(a * u + a * v, a * (u + v));
	CHECK_EQ(a * u + b * u, (a + b) * u);
	CHECK_EQ(a + a, 2.0 * a);

	CHECK_EQ((a * b) * c, a * (b * c));
	CHECK_EQ(transpose(a * b), transpose(b) * transpose(a));
	CHECK_EQ(adj(a * b), adj(b) * adj(a));
	CHECK_SLOPPY(inverse(a * b), inverse(b) * inverse(a));
	CHECK_EQ(a * inverse(a), TestType::identity());
	CHECK_SLOPPY(exp(a + a, 32), exp(a, 32) * exp(a, 32));
	CHECK_EQ(determinant(a) * determinant(b), determinant(a * b));
	CHECK_EQ(trace(a * b * c), trace(b * c * a));
	CHECK_SLOPPY(determinant(exp(a, 32)), exp(trace(a)));
}

TEMPLATE_PRODUCT_TEST_CASE("complex matrix (anti)hermitian decomposition",
                           "[linalg]", (Matrix2, Matrix3),
                           (util::complex<float>, util::complex<double>))
{
	using std::exp;
	using util::norm2;
	auto rng = util::xoshiro256(12345);
	auto a = TestType::random_normal(rng);

	auto x = hermitian_traceless(a);
	auto y = antihermitian_traceless(a);
	CHECK_EQ(x + y + TestType::identity() * trace(a) / TestType::dim(), a);
	CHECK_EQ(trace(x), 0.0);
	CHECK_EQ(trace(y), 0.0);
	CHECK_EQ(adj(x), x);
	CHECK_EQ(adj(y), -y);
}
