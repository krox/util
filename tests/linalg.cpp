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

TEST_CASE("rsqrt function implementation", "[linalg]")
{
	using util::Vector;
	using util::rsqrt;
	using util::normalize;
	using util::norm2;
	
	// Test rsqrt accuracy for float (relaxed tolerance due to fast approximation)
	{
		float x = 4.0f;
		float expected = 1.0f / std::sqrt(x);  // Should be 0.5
		float actual = rsqrt(x);
		CHECK(std::abs(actual - expected) < 1e-5f);  // Fast rsqrt has lower precision
	}
	
	// Test rsqrt accuracy for double (using approximation algorithm)
	{
		double x = 9.0;
		double expected = 1.0 / std::sqrt(x);  // Should be 1/3
		double actual = rsqrt(x);
		CHECK(std::abs(actual - expected) < 1e-10);  // Relaxed tolerance for approximation
	}
	
	// Test that normalize function using rsqrt produces unit vectors
	{
		Vector<float, 3> v{3.0f, 4.0f, 0.0f};  // Length should be 5
		auto normalized = normalize(v);
		float length_squared = norm2(normalized);
		CHECK(std::abs(length_squared - 1.0f) < 1e-5f);  // Relaxed tolerance
		
		// Check that normalized vector components are approximately correct
		CHECK(std::abs(normalized[0] - 0.6f) < 1e-5f);  // 3/5
		CHECK(std::abs(normalized[1] - 0.8f) < 1e-5f);  // 4/5
		CHECK(std::abs(normalized[2] - 0.0f) < 1e-5f);  // 0/5
	}
	
	// Test that rsqrt works for various values (with appropriate tolerance)
	{
		std::array<float, 5> test_values = {1.0f, 0.25f, 16.0f, 100.0f, 0.01f};
		for (float x : test_values) {
			float expected = 1.0f / std::sqrt(x);
			float actual = rsqrt(x);
			// Use relative error for better accuracy across different scales
			float relative_error = std::abs((actual - expected) / expected);
			CHECK(relative_error < 1e-4f);  // 0.01% relative error
		}
	}
}
