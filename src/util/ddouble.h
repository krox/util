#pragma once

#include "fmt/format.h"
#include <cassert>
#include <cfloat>
#include <cmath>
#include <ostream>
#include <random>
#include <span>
#include <utility>

namespace util {

/**
 * "double double" type which is a (non-IEEE) floating point type implemented
 * as the sum of two doubles and thus can represet about 107 bits of effective
 * mantissa. In practice, this is this fastest possible datatype with more
 * precision than 'double'. (Much faster than libraries like MPFR, which start
 * to shine at much higher precision).
 *
 * Notes on correctness:
 *    - This module will break if compiled with '-ffast-math'. Just dont. Some
 *      weaker flags are okay though, e.g.:
 *      -fno-signed-zeros -fno-math-errno -ffinite-math-only
 *    - It might break if compiled to x87 code due to extended precision for
 *      intermediates. SSE/AVX is always fine.
 *    - It will also probably break if FP rounding mode is changed away
 *      from 'nearest'. But who would ever consider that?
 *    - Correct handling of inf/nan/signed-zero/subnormals is not really tested.
 *      (And probably wont be fixed if broken. Dont want branches.)
 *
 * Notes on performance:
 *    - Make sure you compiler/standard-library emits actual fused multiply-add
 *      instructions for 'std::fma()' (supported since AVX2 on x86 CPUs).
 *      This will speed up all operations quite a lot.
 *    - Only use ddouble where necessary. Mixed operations between double
 *      and ddouble are all supported and should be prefered. In particular,
 *      use static member function like 'ddouble::sum(a,b)' to get ddouble
 *      results from double paramters.
 *    - For the last bit of performance, there are some specialized instructions
 *      that are (very slightly) faster than the naive expressions:
 *            2 * x  ->  times2(x)     (same for times4, divide2, divide4)
 *          512 * x  ->  ldexp(x, 9)
 *            x * x  ->  sqr(x)        (is this true?)
 *            1 / x  ->  inverse(x)    (or this?)
 *
 * Notes on implementation:
 *    - When in doubt, choose speed over precision. Also dont really care about
 *      correct handling of NaN/infinity/signed-zero/subnormals
 *    - As few branches as possible, to make vectorization straightforward.
 */
class ddouble
{
	// invariant / convention:
	//  * if high_ is finite, then low_ must be finite and high_ + low == high_
	//    in double precision.
	//  * if high_ is inf/nan, low_ is undefined.
	double high_ = 0.0 / 0.0;
	double low_ = 0.0 / 0.0;

	constexpr ddouble(double high, double low) noexcept : high_(high), low_(low)
	{
		assert(!std::isfinite(high_) || high_ + low_ == high_);
	}

  public:
	ddouble() = default;
	explicit constexpr ddouble(double a) noexcept : high_(a), low_(0) {}
	constexpr ddouble &operator=(double a) noexcept
	{
		high_ = a;
		low_ = 0;
		return *this;
	}

	static constexpr ddouble unchecked(double high, double low) noexcept
	{
		return ddouble(high, low);
	}

	// direct access to members should not really be needed in user code
	double high() const noexcept { return high_; }
	double low() const noexcept { return low_; }

	explicit operator float() const noexcept { return (float)high(); }
	explicit operator double() const noexcept { return high(); }
	float to_flaot() const noexcept { return high(); }
	double to_double() const noexcept { return high(); }

#define UTIL_DDOUBLE_CONST(name, high, low)                                    \
	static constexpr ddouble name() noexcept                                   \
	{                                                                          \
		return unchecked(high, low);                                           \
	}

	// constant naming as in std::numbers, though not exactly the same list
	// generated with high precision MPFR, so should be precise in all digits
	// clang-format off
	UTIL_DDOUBLE_CONST(    e,      0x1.5bf0a8b145769p+1,   0x1.4d57ee2b1013ap-53) // 2.7183...
	UTIL_DDOUBLE_CONST(inv_e,      0x1.78b56362cef38p-2,  -0x1.ca8a4270fadf5p-57) // 0.3679...
	UTIL_DDOUBLE_CONST(    egamma, 0x1.2788cfc6fb619p-1,  -0x1.6cb90701fbfabp-58) // 0.5772...
	UTIL_DDOUBLE_CONST(inv_egamma, 0x1.bb8226f502bf8p+0,  -0x1.7abec73926687p-56) // 1.7325...
	UTIL_DDOUBLE_CONST(    pi,     0x1.921fb54442d18p+1,   0x1.1a62633145c07p-53) // 3.1416...
	UTIL_DDOUBLE_CONST(inv_pi,     0x1.45f306dc9c883p-2,  -0x1.6b01ec5417056p-56) // 0.3183...
	UTIL_DDOUBLE_CONST(    sqrt2,  0x1.6a09e667f3bcdp+0,  -0x1.bdd3413b26456p-54) // 1.4142...
	UTIL_DDOUBLE_CONST(inv_sqrt2,  0x1.6a09e667f3bcdp-1,  -0x1.bdd3413b26456p-55) // 0.7071...
	UTIL_DDOUBLE_CONST(    sqrt3,  0x1.bb67ae8584caap+0,   0x1.cec95d0b5c1e3p-54) // 1.7321...
	UTIL_DDOUBLE_CONST(inv_sqrt3,  0x1.279a74590331cp-1,   0x1.34863e0792bedp-55) // 0.5774...
	UTIL_DDOUBLE_CONST(    ln2,    0x1.62e42fefa39efp-1,   0x1.abc9e3b39803fp-56) // 0.6931...
	UTIL_DDOUBLE_CONST(inv_ln2,    0x1.71547652b82fep+0,   0x1.777d0ffda0d24p-56) // 1.4427...
	UTIL_DDOUBLE_CONST(    ln10,   0x1.26bb1bbb55516p+1,  -0x1.f48ad494ea3e9p-53) // 2.3026...
	UTIL_DDOUBLE_CONST(inv_ln10,   0x1.bcb7b1526e50ep-2,   0x1.95355baaafad3p-57) // 0.4343...
	UTIL_DDOUBLE_CONST(    log10e, 0x1.bcb7b1526e50ep-2,   0x1.95355baaafad3p-57) // 0.4343...
	UTIL_DDOUBLE_CONST(inv_log10e, 0x1.26bb1bbb55516p+1,  -0x1.f48ad494ea3e9p-53) // 2.3026...
	UTIL_DDOUBLE_CONST(    log2e,  0x1.71547652b82fep+0,   0x1.777d0ffda0d24p-56) // 1.4427...
	UTIL_DDOUBLE_CONST(inv_log2e,  0x1.62e42fefa39efp-1,   0x1.abc9e3b39803fp-56) // 0.6931...
	UTIL_DDOUBLE_CONST(    phi,    0x1.9e3779b97f4a8p+0,  -0x1.f506319fcfd19p-55) // 1.6180...
	UTIL_DDOUBLE_CONST(inv_phi,    0x1.3c6ef372fe95p-1,   -0x1.f506319fcfd19p-55) // 0.6180...

	// inverse factorials. used in Taylor expansions of sin/cos/exp/...
	UTIL_DDOUBLE_CONST(inv_fac3,   0x1.5555555555555p-3,   0x1.5555555555555p-57) // 0.1667...
	UTIL_DDOUBLE_CONST(inv_fac4,   0x1.5555555555555p-5,   0x1.5555555555555p-59) // 0.0417...
	UTIL_DDOUBLE_CONST(inv_fac5,   0x1.1111111111111p-7,   0x1.1111111111111p-63) // 0.0083...
	UTIL_DDOUBLE_CONST(inv_fac6,   0x1.6c16c16c16c17p-10, -0x1.f49f49f49f49fp-65) // 0.0014...
	UTIL_DDOUBLE_CONST(inv_fac7,   0x1.a01a01a01a01ap-13,  0x1.a01a01a01a01ap-73) // 0.0002...
	UTIL_DDOUBLE_CONST(inv_fac8,   0x1.a01a01a01a01ap-16,  0x1.a01a01a01a01ap-76) // 0.0000...
	UTIL_DDOUBLE_CONST(inv_fac9,   0x1.71de3a556c734p-19, -0x1.c154f8ddc6c00p-73) // 0.0000...
	UTIL_DDOUBLE_CONST(inv_fac10,  0x1.27e4fb7789f5cp-22,  0x1.cbbc05b4fa99ap-76) // 0.0000...
	UTIL_DDOUBLE_CONST(inv_fac11,  0x1.ae64567f544e4p-26, -0x1.c062e06d1f209p-80) // 0.0000...
	UTIL_DDOUBLE_CONST(inv_fac12,  0x1.1eed8eff8d898p-29, -0x1.2aec959e14c06p-83) // 0.0000...
	UTIL_DDOUBLE_CONST(inv_fac13,  0x1.6124613a86d09p-33,  0x1.f28e0cc748ebep-87) // 0.0000...
	UTIL_DDOUBLE_CONST(inv_fac14,  0x1.93974a8c07c9dp-37,  0x1.05d6f8a2efd1fp-92) // 0.0000...

	// special values
	UTIL_DDOUBLE_CONST(nan,      0.0/0.0, 0.0/0.0)
	UTIL_DDOUBLE_CONST(infinity, 1.0/0.0, 0.0    )
	UTIL_DDOUBLE_CONST(highest,  DBL_MAX, 0.0    )
	UTIL_DDOUBLE_CONST(lowest,  -DBL_MAX, 0.0    )
	// clang-format on

	template <typename RNG> static ddouble random(RNG &rng)
	{
		auto dist = std::uniform_real_distribution<double>(0.0, 1.0);
		return sum_quick(dist(rng), ldexp(dist(rng), -53));
	}

	// Compute a+b exactly. No rounding, overflow still possible.
	static ddouble sum(double a, double b)
	{
		// version 1: one branch, 3 adds
		// if (abs(a) >= abs(b))
		//	return sum_quick(a, b);
		// else
		//	return sum_quick(b, a);

		// version 2: no branch, 6 adds
		double high = a + b;
		double v = high - a;
		double low = (a - (high - v)) + (b - v);
		return ddouble(high, low);
	}

	// Compute a+b, assuming abs(a) >= abs(b)
	static ddouble sum_quick(double a, double b)
	{
		double high = a + b;
		double low = b - (high - a);
		return ddouble(high, low);
	}

	// Compute a*b exactly. No rounding, overflow still possible.
	static ddouble mul(double a, double b)
	{
		double high = a * b;
		double low = fma(a, b, -high);
		return ddouble(high, low);
	}

	// Compute a/b in ddouble precision.
	static ddouble div(double a, double b)
	{
		double high = a / b;
		double low = fma(-high, b, a) / b;
		return ddouble(high, low);
	}
};

template <size_t i> double get(ddouble const &a)
{
	if constexpr (i == 0)
		return a.high();
	else if constexpr (i == 1)
		return a.low();
	else
		assert(false);
}

// returns negative of a
inline ddouble operator-(ddouble a)
{
	return ddouble::unchecked(-a.high(), -a.low());
}

// returns absolute value of a
inline ddouble abs(ddouble a) { return a.high() < 0.0 ? -a : a; }

// returns 2*a
inline ddouble times2(ddouble a)
{
	return ddouble::unchecked(a.high() * 2, a.low() * 2);
}

// returns 4*a
inline ddouble times4(ddouble a)
{
	return ddouble::unchecked(a.high() * 4, a.low() * 4);
}

// returns a/2
inline ddouble divide2(ddouble a)
{
	return ddouble::unchecked(a.high() / 2, a.low() / 2);
}

// returns a/4
inline ddouble divide4(ddouble a)
{
	return ddouble::unchecked(a.high() / 4, a.low() / 4);
}

// returns a * 2^e
inline ddouble ldexp(ddouble a, int e)
{
	return ddouble::unchecked(std::ldexp(a.high(), e), std::ldexp(a.low(), e));
}

// decompose a = r * 2^e with 0.5 <= |r| < 1
inline ddouble frexp(ddouble a, int *e)
{
	double high = std::frexp(a.high(), e);
	double low = std::ldexp(a.low(), -*e);
	return ddouble::unchecked(high, low);
}

inline int ilogb(ddouble a) { return std::ilogb(a.high()); }

// binary double <-> ddouble operators
inline ddouble operator+(ddouble a, double b)
{
	ddouble tmp = ddouble::sum(a.high(), b);
	return ddouble::sum_quick(tmp.high(), tmp.low() + a.low());
}
inline ddouble operator+(double a, ddouble b) { return b + a; }
inline ddouble operator-(ddouble a, double b) { return a + (-b); }
inline ddouble operator-(double a, ddouble b) { return (-b) + a; }
inline ddouble operator*(ddouble a, double b)
{
	auto tmp = ddouble::mul(a.high(), b);
	return ddouble::sum_quick(tmp.high(), tmp.low() + a.low() * b);
}
inline ddouble operator*(double a, ddouble b) { return b * a; }
inline ddouble operator/(ddouble a, double b)
{
	double high = a.high() / b;
	double low = (fma(-high, b, a.high()) + a.low()) / b;
	return ddouble::sum_quick(high, low);
}
inline ddouble operator/(double a, ddouble b)
{
	double high = a / b.high();
	double low = (fma(-high, b.high(), a) - high * b.low()) / b.high();
	return ddouble::sum_quick(high, low);
}

/** binary ddouble <-> ddouble operators */
inline ddouble operator+(ddouble a, ddouble b)
{
	ddouble tmp = ddouble::sum(a.high(), b.high());
	return ddouble::sum_quick(tmp.high(), tmp.low() + a.low() + b.low());
}
inline ddouble operator-(ddouble a, ddouble b) { return a + (-b); }
inline ddouble operator*(ddouble a, ddouble b)
{
	auto tmp = ddouble::mul(a.high(), b.high());
	return ddouble::sum_quick(tmp.high(), tmp.low() + a.high() * b.low() +
	                                          a.low() * b.high());
}
inline ddouble operator/(ddouble a, ddouble b)
{
	double high = a.high() / b.high();
	double low =
	    (fma(-high, b.high(), a.high()) + a.low() - high * b.low()) / b.high();
	return ddouble::sum_quick(high, low);
}

// convenience operators
inline ddouble &operator+=(ddouble &a, double b) { return a = a + b; }
inline ddouble &operator-=(ddouble &a, double b) { return a = a - b; }
inline ddouble &operator*=(ddouble &a, double b) { return a = a * b; }
inline ddouble &operator/=(ddouble &a, double b) { return a = a / b; }
inline ddouble &operator+=(ddouble &a, ddouble b) { return a = a + b; }
inline ddouble &operator-=(ddouble &a, ddouble b) { return a = a - b; }
inline ddouble &operator*=(ddouble &a, ddouble b) { return a = a * b; }
inline ddouble &operator/=(ddouble &a, ddouble b) { return a = a / b; }

namespace ddouble_detail {
inline ddouble taylor(std::span<const ddouble> c, ddouble x)
{
	ddouble r = c[0] + x * c[1];
	ddouble xi = x;

	for (size_t i = 2; i < c.size(); ++i)
	{
		xi *= x;
		r += c[i] * xi;
	}
	return r;
}

// taylor coefficients of exp
static constexpr ddouble coeffs_exp[] = {
    ddouble::unchecked(0x1.0000000000000p+0, 0x0.0000000000000p+0),
    ddouble::unchecked(0x1.0000000000000p+0, 0x0.0000000000000p+0),
    ddouble::unchecked(0x1.0000000000000p-1, 0x0.0000000000000p+0),
    ddouble::unchecked(0x1.5555555555555p-3, 0x1.5555555555555p-57),
    ddouble::unchecked(0x1.5555555555555p-5, 0x1.5555555555555p-59),
    ddouble::unchecked(0x1.1111111111111p-7, 0x1.1111111111111p-63),
    ddouble::unchecked(0x1.6c16c16c16c17p-10, -0x1.f49f49f49f49fp-65),
    ddouble::unchecked(0x1.a01a01a01a01ap-13, 0x1.a01a01a01a01ap-73),
    ddouble::unchecked(0x1.a01a01a01a01ap-16, 0x1.a01a01a01a01ap-76),
    ddouble::unchecked(0x1.71de3a556c734p-19, -0x1.c154f8ddc6c00p-73),
    ddouble::unchecked(0x1.27e4fb7789f5cp-22, 0x1.cbbc05b4fa99ap-76),
    ddouble::unchecked(0x1.ae64567f544e4p-26, -0x1.c062e06d1f209p-80),
    ddouble::unchecked(0x1.1eed8eff8d898p-29, -0x1.2aec959e14c06p-83),
    // ddouble::unchecked(0x1.6124613a86d09p-33, 0x1.f28e0cc748ebep-87),
    // ddouble::unchecked(0x1.93974a8c07c9dp-37, 0x1.05d6f8a2efd1fp-92),
    // ddouble::unchecked(0x1.ae7f3e733b81fp-41, 0x1.1d8656b0ee8cbp-097),
    // ddouble::unchecked(0x1.ae7f3e733b81fp-45, 0x1.1d8656b0ee8cbp-101),
};

// taylor coefficients of sin (odd only)
static constexpr ddouble coeffs_sin[] = {
    ddouble::unchecked(0x1.0000000000000p+0, 0x0.0000000000000p+0),
    ddouble::unchecked(-0x1.5555555555555p-3, -0x1.5555555555555p-57),
    ddouble::unchecked(0x1.1111111111111p-7, 0x1.1111111111111p-63),
    ddouble::unchecked(-0x1.a01a01a01a01ap-13, -0x1.a01a01a01a01ap-73),
    ddouble::unchecked(0x1.71de3a556c734p-19, -0x1.c154f8ddc6c00p-73),
    ddouble::unchecked(-0x1.ae64567f544e4p-26, 0x1.c062e06d1f209p-80),
    ddouble::unchecked(0x1.6124613a86d09p-33, 0x1.f28e0cc748ebep-87),
    ddouble::unchecked(-0x1.ae7f3e733b81fp-41, -0x1.1d8656b0ee8cbp-97),
    ddouble::unchecked(0x1.952c77030ad4ap-49, 0x1.ac981465ddc6cp-103),
    ddouble::unchecked(-0x1.2f49b46814157p-57, -0x1.2650f61dbdcb4p-112),
    ddouble::unchecked(0x1.71b8ef6dcf572p-66, -0x1.d043ae40c4647p-120),
    ddouble::unchecked(-0x1.761b41316381ap-75, 0x1.3423c7d91404fp-130),
    ddouble::unchecked(0x1.3f3ccdd165fa9p-84, -0x1.58ddadf344487p-139),
    ddouble::unchecked(-0x1.d1ab1c2dccea3p-94, -0x1.054d0c78aea14p-149),
    // ddouble::unchecked(0x1.259f98b4358adp-103, 0x1.eaf8c39dd9bc5p-157),
    // ddouble::unchecked(-0x1.434d2e783f5bcp-113, -0x1.0b87b91be9affp-167),
    // ddouble::unchecked(0x1.3981254dd0d52p-123, -0x1.2b1f4c8015a2fp-177),
    // ddouble::unchecked(-0x1.0dc59c716d91fp-133, -0x1.419e3fad3f031p-188),
};

// taylor coefficients of cos (even only)
static constexpr ddouble coeffs_cos[] = {
    ddouble::unchecked(0x1.0000000000000p+0, 0x0.0000000000000p+0),
    ddouble::unchecked(-0x1.0000000000000p-1, 0x0.0000000000000p+0),
    ddouble::unchecked(0x1.5555555555555p-5, 0x1.5555555555555p-59),
    ddouble::unchecked(-0x1.6c16c16c16c17p-10, 0x1.f49f49f49f49fp-65),
    ddouble::unchecked(0x1.a01a01a01a01ap-16, 0x1.a01a01a01a01ap-76),
    ddouble::unchecked(-0x1.27e4fb7789f5cp-22, -0x1.cbbc05b4fa99ap-76),
    ddouble::unchecked(0x1.1eed8eff8d898p-29, -0x1.2aec959e14c06p-83),
    ddouble::unchecked(-0x1.93974a8c07c9dp-37, -0x1.05d6f8a2efd1fp-92),
    ddouble::unchecked(0x1.ae7f3e733b81fp-45, 0x1.1d8656b0ee8cbp-101),
    ddouble::unchecked(-0x1.6827863b97d97p-53, -0x1.eec01221a8b0bp-107),
    ddouble::unchecked(0x1.e542ba4020225p-62, 0x1.ea72b4afe3c2fp-120),
    ddouble::unchecked(-0x1.0ce396db7f853p-70, 0x1.aebcdbd20331cp-124),
    ddouble::unchecked(0x1.f2cf01972f578p-80, -0x1.9ada5fcc1ab14p-135),
    ddouble::unchecked(-0x1.88e85fc6a4e5ap-89, 0x1.71c37ebd16540p-143),
    // ddouble::unchecked(0x1.0a18a2635085dp-98, 0x1.b9e2e28e1aa54p-153),
    // ddouble::unchecked(-0x1.3932c5047d60ep-108, -0x1.832b7b530a627p-162),
    // ddouble::unchecked(0x1.434d2e783f5bcp-118, 0x1.0b87b91be9affp-172),
    // ddouble::unchecked(-0x1.2710231c0fd7ap-128, -0x1.3f8a2b4af9d6bp-184),
};

} // namespace ddouble_detail

// returns square of a. slightly faster than 'a * a'
inline ddouble sqr(ddouble a)
{
	auto tmp = ddouble::mul(a.high(), a.high());
	return ddouble::sum_quick(tmp.high(),
	                          tmp.low() + std::ldexp(a.high() * a.low(), 1));
}

// returns inverse of a. Slightly faster than '1.0 / a'
inline ddouble inverse(ddouble a)
{
	double high = 1 / a.high();
	double low = (fma(-high, a.high(), 1.0) - high * a.low()) / a.high();
	return ddouble::sum_quick(high, low);
}

// returns square root of a
inline ddouble sqrt(ddouble a)
{
	// double-precision + 1 newton step
	auto high = std::sqrt(a.high());
	return divide2(high + a / high);
}

// inverse square root '1/sqrt(a)'
inline ddouble rec_sqrt(ddouble a)
{
	// double-precision + 1 newton step
	// TODO: would be nice to get the SSE instruction 'rsqrt' as a first
	//       approximation (only gives ~11 correct bits, but very fast)
	double r = 1.0 / std::sqrt(a.high());
	return (0.5 * r) * (3 - a * ddouble::mul(r, r));
}

// cube root
inline ddouble cbrt(ddouble a)
{
	auto high = std::cbrt(a.high());
	ddouble r = (2 * high + a / (ddouble::mul(high, high))) / 3;
	return r;
}

inline ddouble pow(ddouble a, int b)
{
	if (b < 0)
	{
		a = inverse(a);
		b = -b;
	}
	ddouble r{1};
	for (; b != 0; b >>= 1, a = sqr(a))
		if (b & 1)
			r *= a;
	return r;
}

inline ddouble exp(ddouble a)
{
	using namespace ddouble_detail;

	// Idea: exp(k*log(2) + r) = exp(r/16)^16 * 2^k
	// (the repeated half/square-trick looses a bit of accuracy, which is fine)
	int k = (int)std::round(a.high() * (1.0 / M_LN2));
	a -= k * ddouble::ln2();
	a = ldexp(a, -4);
	// now it is |a| <= ln(2)/32 = 0.02166..., so the series converges quickly
	assert(std::abs(a.high()) < 0.022);
	ddouble r = taylor(coeffs_exp, a);

	// restore full answer
	r = sqr(sqr(sqr(sqr(r))));
	return ldexp(r, k);
}

inline ddouble log(ddouble a)
{
	double high = std::log(a.high());
	return high + a * exp(-ddouble(high)) - 1.0;
}

inline ddouble sin(ddouble a)
{
	using namespace ddouble_detail;

	// reduce mod pi/2, so that resulting |a| <= pi/4 = 0.785...
	int k = (int)std::round(a.high() * (2 / M_PI));
	a -= k * divide2(ddouble::pi());
	assert(std::abs(a.high()) < 0.786);

	// taylor series of sin/cos
	switch (k & 3)
	{
	case 0:
		return a * taylor(coeffs_sin, a * a);
	case 1:
		return taylor(coeffs_cos, a * a);
	case 2:
		return -a * taylor(coeffs_sin, a * a);
	case 3:
		return -taylor(coeffs_cos, a * a);
	}
	assert(false);
}

inline ddouble cos(ddouble a)
{
	using namespace ddouble_detail;

	// reduce mod pi/2, so that resulting |a| <= pi/4 = 0.785...
	int k = (int)std::round(a.high() * (2 / M_PI));
	a -= k * divide2(ddouble::pi());
	assert(std::abs(a.high()) < 0.786);

	// taylor series of sin/cos
	switch (k & 3)
	{
	case 0:
		return taylor(coeffs_cos, a * a);
	case 1:
		return -a * taylor(coeffs_sin, a * a);
	case 2:
		return -taylor(coeffs_cos, a * a);
	case 3:
		return a * taylor(coeffs_sin, a * a);
	}
	assert(false);
}

inline ddouble tan(ddouble a) { return sin(a) / cos(a); }
inline ddouble cot(ddouble a) { return cos(a) / sin(a); }
inline ddouble sec(ddouble a) { return inverse(cos(a)); }
inline ddouble csc(ddouble a) { return inverse(sin(a)); }

inline bool operator<(ddouble a, ddouble b)
{
	return a.high() < b.high() || (a.high() == b.high() && a.low() < b.low());
}
inline bool operator<=(ddouble a, ddouble b)
{
	return a.high() < b.high() || (a.high() == b.high() && a.low() <= b.low());
}
inline bool operator==(ddouble a, ddouble b)
{
	return a.high() == b.high() && a.low() == b.low();
}

inline bool operator<(ddouble a, double b)
{
	return a.high() < b || (a.high() == b && a.low() < 0);
}
inline bool operator<=(ddouble a, double b)
{
	return a.high() < b || (a.high() == b && a.low() <= 0);
}
inline bool operator==(ddouble a, double b)
{
	return a.high() == b && a.low() == 0;
}
inline bool operator<(double a, ddouble b)
{
	return a < b.high() || (a == b.high() && 0 < b.low());
}
inline bool operator<=(double a, ddouble b)
{
	return a < b.high() || (a == b.high() && 0 <= b.low());
}
inline bool operator==(double a, ddouble b) { return b == a; }

inline bool operator>(ddouble a, ddouble b) { return b < a; }
inline bool operator>=(ddouble a, ddouble b) { return b <= a; }
inline bool operator!=(ddouble a, ddouble b) { return !(a == b); }
inline bool operator>(ddouble a, double b) { return b < a; }
inline bool operator>=(ddouble a, double b) { return b <= a; }
inline bool operator!=(ddouble a, double b) { return !(a == b); }
inline bool operator>(double a, ddouble b) { return b < a; }
inline bool operator>=(double a, ddouble b) { return b <= a; }
inline bool operator!=(double a, ddouble b) { return !(a == b); }

template <class T> struct RingTraits;

template <> struct RingTraits<ddouble>
{
	/** potentially faster than full comparison */
	static bool is_zero(ddouble const &value) { return value.high() == 0.0; }
	static bool is_one(ddouble const &value) { return value == 1; }

	/** just set to false if signs dont make sense for T */
	static bool is_negative(ddouble const &value) { return value.high() < 0; }

	/** do we need parentheses if T occurs in a product/power? */
	static bool need_parens_product(ddouble const &) { return false; }
	static bool need_parens_power(ddouble const &value)
	{
		return is_negative(value);
	}
};

} // namespace util

template <>
struct std::tuple_size<util::ddouble> : std::integral_constant<size_t, 2>
{};
template <size_t N>
struct std::tuple_element<N, util::ddouble> : std::type_identity<double>
{};

template <> struct fmt::formatter<util::ddouble>
{
	int precision = 30; // digits after decimal point

	constexpr auto parse(format_parse_context &ctx)
	{
		auto it = ctx.begin(), end = ctx.end();

		auto parseInt = [&]() -> int {
			int r = 0;
			while (it != end && '0' <= *it && *it <= '9')
				r = 10 * r + *it++ - '0';
			return r;
		};

		if (it != end && *it == '.')
		{
			++it;
			precision = parseInt();
		}

		// currently, we only support 'e' format
		if (it != end && *it == 'e')
			++it;

		// Check if reached the end of the range:
		if (it != end && *it != '}')
			throw format_error(
			    fmt::format("invalid format: unexpected character '{}'", *it));

		// Return an iterator past the end of the parsed range:
		return it;
	}

	template <typename FormatContext>
	auto format(util::ddouble a, FormatContext &ctx)
	{
		auto out = ctx.out();

		// special cases and minus sign
		if (std::isnan(a.high()))
			return format_to(out, "nan");
		if (std::signbit(a.high()))
		{
			*out++ = '-';
			a = -a;
		}
		if (std::isinf(a.high()))
			return format_to(out, "inf");
		if (a.high() == 0.0)
			return format_to(ctx.out(), "0.0");
		assert(std::isfinite(a.high()));
		assert(a > 0);

		// find exponent
		int e = (int)std::log10(a.high());
		a *= pow(util::ddouble(10), -e);
		if (a.high() < 1.0)
		{
			e++;
			a *= 10;
		}
		else if (a.high() >= 10.0)
		{
			e--;
			a /= 10;
		}
		assert(1.0 <= a && a < 10.0);

		// output the mantissa
		for (int i = 0; i < precision + 1; ++i)
		{
			int d = (int)a.high();
			assert(0 <= d && d <= 9);
			a -= d;
			a *= 10;
			*out++ = '0' + d;
			if (i == 0)
				*out++ = '.';
		}

		out = format_to(out, "e{:+03}", e);

		return out;
	}
};

namespace Eigen {

template <typename T> class NumTraits;

template <> struct NumTraits<util::ddouble>
{
	using Real = util::ddouble;
	using NonInteger = util::ddouble;
	using Nested = util::ddouble;
	using Literal = double; // is this correct?

	static inline Real epsilon() { return Real(ldexp(1.0, -107)); }
	static inline Real dummy_precision() { return Real(ldexp(1.0, -99)); }
	static inline int digits10() { return 29; }
	static inline Real highest() { return Real::highest(); }
	static inline Real lowest() { return Real::lowest(); }

	enum
	{
		IsComplex = 0,
		IsInteger = 0,
		IsSigned = 1,
		RequireInitialization = 1,

		// not tuned at all
		ReadCost = 1,
		AddCost = 3,
		MulCost = 10
	};
};

} // namespace Eigen
