#pragma once

#include "fmt/format.h"
#include "util/simd.h" // for supporting direct complex<simd<T>> <-> T ops
#include <cmath>
#include <utility>

namespace util {

/**
 * Similar to std::complex, but:
 *     - Components are directly accessible as '.re' and '.im' in addition
 *       to the std way '.real()' and '.imag()'.
 *     - No attempt is made to handle edge cases perfectly. For example,
 *       multiplication is just defined as (a+bi)(c+di) = (ac-bd)+(ad+bc)i.
 *       (std::complex by default contains inf/nan checks that can
 *       prohibit vectorization, thus costing quite some performance)
 *     - Allow base types other than float/double/long double. For example
 *         * explicit (horizontal) vectorization: simd<float>, ...
 *         * high-precision types: MpfrFloat, ddouble, ...
 *         * non-floating point types: int, Rational, ...
 *     - Allows more flexible complex<->real operations such as
 *           'complex<float> * double'
 *       NOTE: this is achieved by modifying template deduction rules and then
 *             using the implicit double->float conversion. Therefore the result
 *             always tracks the type of the complex and does NOT perform
 *             promotion to the bigger type.
 *
 *     - TODO: Add explicit 'imaginary<T>' type
 *
 * NOTE: beware of different naming conventions for the norm:
 *                          |.|       |.|^2
 *     complex numbers:
 *         std::complex:   'abs'     'norm'
 *         util::complex:  'abs'     'norm','norm2'
 *     linear algebra:
 *         Eigen:          'norm'    'squaredNorm'
 *         GLSL/GLM:       'length'  -
 *         LQCD:             -       'norm2'
 *         util::Vector:   'length'  'norm2'
 */
template <typename T> struct complex
{
	T re, im;

	complex() = default;
	explicit complex(int r) : re(r), im(0) {}
	complex(T r) : re(std::move(r)), im(0) {}
	complex(T r, T i) : re(std::move(r)), im(std::move(i)) {}
	template <typename U>
	explicit complex(complex<U> const &other) : re(other.re), im(other.im)
	{}

	T const &real() const { return re; }
	T const &imag() const { return im; }
};

template <typename T> T const &real(complex<T> const &a) { return a.re; }
template <typename T> T const &imag(complex<T> const &a) { return a.im; }

using std::exp, std::sin, std::cos, std::sinh, std::cosh, std::abs, std::sqrt;

// unary complex

template <typename T> T norm2(complex<T> const &a)
{
	return a.re * a.re + a.im * a.im;
}
template <typename T> T norm(complex<T> const &a) { return norm2(a); }
template <typename T> T abs(complex<T> const &a) { return sqrt(norm(a)); }
template <typename T> complex<T> conj(complex<T> const &a)
{
	return {a.re, -a.im};
}
template <typename T> complex<T> operator+(complex<T> const &a)
{
	return {+a.re, +a.im};
}
template <typename T> complex<T> operator-(complex<T> const &a)
{
	return {-a.re, -a.im};
}
template <typename T> complex<T> inverse(complex<T> const &a)
{
	return conj(a) / norm(a);
}

// binary complex <-> complex

template <typename T, typename U>
auto operator+(complex<T> const &a, complex<U> const &b)
    -> complex<decltype(a.re + b.re)>
{
	return {a.re + b.re, a.im + b.im};
}
template <typename T, typename U>
auto operator-(complex<T> const &a, complex<U> const &b)
    -> complex<decltype(a.re - b.re)>
{
	return {a.re - b.re, a.im - b.im};
}
template <typename T, typename U>
auto operator*(complex<T> const &a, complex<U> const &b)
    -> complex<decltype(a.re * b.re)>
{
	return {a.re * b.re - a.im * b.im, a.re * b.im + a.im * b.re};
}
template <typename T, typename U>
auto operator/(complex<T> const &a, complex<U> const &b)
    -> complex<decltype(a.re * b.re)>
{
	return (a * conj(b)) / norm(b);
}

// binary complex <-> real

template <typename T>
complex<T> operator+(complex<T> const &a, std::type_identity_t<T> const &b)
{
	return {a.re + b, a.im};
}
template <typename T>
complex<T> operator+(std::type_identity_t<T> const &a, complex<T> const &b)
{
	return {a + b.re, b.im};
}
template <typename T>
complex<T> operator-(complex<T> const &a, std::type_identity_t<T> const &b)
{
	return {a.re - b, a.im};
}
template <typename T>
complex<T> operator-(std::type_identity_t<T> const &a, complex<T> const &b)
{
	return {a - b.re, -b.im};
}
template <typename T>
complex<T> operator*(complex<T> const &a, std::type_identity_t<T> const &b)
{
	return {a.re * b, a.im * b};
}
template <typename T>
complex<T> operator*(std::type_identity_t<T> const &a, complex<T> const &b)
{
	return {a * b.re, a * b.im};
}
template <typename T>
complex<T> operator/(complex<T> const &a, std::type_identity_t<T> const &b)
{
	return {a.re / b, a.im / b};
}
template <typename T>
complex<T> operator/(std::type_identity_t<T> const &a, complex<T> const &b)
{
	return a * inverse(b);
}

// binary complex<simd> <-> scalar real

template <typename T, size_t W>
complex<simd<T, W>> operator+(complex<simd<T, W>> const &a,
                              std::type_identity_t<T> const &b)
{
	return {a.re + b, a.im};
}
template <typename T, size_t W>
complex<simd<T, W>> operator+(std::type_identity_t<T> const &a,
                              complex<simd<T, W>> const &b)
{
	return {a + b.re, b.im};
}
template <typename T, size_t W>
complex<simd<T, W>> operator-(complex<simd<T, W>> const &a,
                              std::type_identity_t<T> const &b)
{
	return {a.re - b, a.im};
}
template <typename T, size_t W>
complex<simd<T, W>> operator-(std::type_identity_t<T> const &a,
                              complex<simd<T, W>> const &b)
{
	return {a - b.re, -b.im};
}
template <typename T, size_t W>
complex<simd<T, W>> operator*(complex<simd<T, W>> const &a,
                              std::type_identity_t<T> const &b)
{
	return {a.re * b, a.im * b};
}
template <typename T, size_t W>
complex<simd<T, W>> operator*(std::type_identity_t<T> const &a,
                              complex<simd<T, W>> const &b)
{
	return {a * b.re, a * b.im};
}
template <typename T, size_t W>
complex<simd<T, W>> operator/(complex<simd<T, W>> const &a,
                              std::type_identity_t<T> const &b)
{
	return {a.re / b, a.im / b};
}
template <typename T, size_t W>
complex<simd<T, W>> operator/(std::type_identity_t<T> const &a,
                              complex<simd<T, W>> const &b)
{
	return a * inverse(b);
}

// op-assigns complex <-> complex

template <typename T, typename U>
complex<T> &operator+=(complex<T> &a, complex<U> const &b)
{
	a.re += b.re;
	a.im += b.im;
	return a;
}
template <typename T, typename U>
complex<T> &operator-=(complex<T> &a, complex<U> const &b)
{
	a.re -= b.re;
	a.im -= b.im;
	return a;
}
template <typename T, typename U>
complex<T> &operator*=(complex<T> &a, complex<U> const &b)
{
	a = a * b;
	return a;
}
template <typename T, typename U>
complex<T> &operator/=(complex<T> &a, complex<U> const &b)
{
	a = a / b;
	return a;
}

// op-assigns complex <-> any

template <typename T, typename U>
complex<T> &operator+=(complex<T> &a, U const &b)
{
	a.re += b;
	return a;
}
template <typename T, typename U>
complex<T> &operator-=(complex<T> &a, U const &b)
{
	a.re -= b;
	return a;
}
template <typename T, typename U> complex<T> &operator*=(complex<T> &a, U b)
{
	// beware of aliasing
	a.re *= b;
	a.im *= b;
	return a;
}
template <typename T, typename U> complex<T> &operator/=(complex<T> &a, U b)
{
	// beware of aliasing
	a.re /= b;
	a.im /= b;
	return a;
}

template <typename T> bool operator==(complex<T> const &a, complex<T> const &b)
{
	return a.re == b.re && a.im == b.im;
}
template <typename T> bool operator==(complex<T> const &a, T const &b)
{
	return a.im == 0 && a.re == b;
}

// exponentials and trigonometry

template <typename T> complex<T> exp(complex<T> const &a)
{
	return exp(a.re) * complex<T>(cos(a.im), sin(a.im));
}
template <typename T> complex<T> sin(complex<T> const &a)
{
	return {sin(a.re) * cosh(a.im), cos(a.re) * sinh(a.im)};
}
template <typename T> complex<T> cos(complex<T> const &a)
{
	return {cos(a.re) * cosh(a.im), -sin(a.re) * sinh(a.im)};
}
template <typename T> complex<T> sinh(complex<T> const &a)
{
	return {sinh(a.re) * cos(a.im), cosh(a.re) * sin(a.im)};
}
template <typename T> complex<T> cosh(complex<T> const &a)
{
	return {cosh(a.re) * cos(a.im), sinh(a.re) * sin(a.im)};
}

// dummy operations to simplify templates that work with either real or complex

inline float conj(float a) { return a; }
inline float norm(float a) { return a * a; }
inline float norm2(float a) { return a * a; }
inline float real(float a) { return a; }
inline float imag([[maybe_unused]] float a) { return 0; }
inline double conj(double a) { return a; }
inline double norm(double a) { return a * a; }
inline double norm2(double a) { return a * a; }
inline double real(double a) { return a; }
inline double imag([[maybe_unused]] double a) { return 0; }

// type traits to convert real <-> complex types
// NOTE: we do NOT define the default cases
//           complex_t<T> = complex<T>    or    real_t<T> = T
//       because these would be wrong in case of deeply nested types

template <typename T> struct RealType;
template <> struct RealType<float>
{
	using type = float;
};
template <> struct RealType<double>
{
	using type = double;
};
template <typename T> struct RealType<complex<T>>
{
	using type = T;
};
template <typename T> struct ComplexType;
template <> struct ComplexType<float>
{
	using type = complex<float>;
};
template <> struct ComplexType<double>
{
	using type = complex<double>;
};
template <typename T> struct ComplexType<complex<T>>
{
	using type = complex<T>;
};
template <typename T> using real_t = typename RealType<T>::type;
template <typename T> using complex_t = typename ComplexType<T>::type;

// overloads for (horizontal) simd

template <typename T> auto vsum(complex<T> const &a)
{
	return complex(vsum(a.re), vsum(a.im));
}
template <typename T> auto vextract(complex<T> const &a, size_t lane)
{
	return complex(vextract(a.re, lane), vextract(a.im, lane));
}
template <typename T, typename U>
void vinsert(complex<T> &a, size_t lane, complex<U> const &b)
{
	vinsert(a.re, lane, b.re);
	vinsert(a.im, lane, b.im);
}

} // namespace util

template <typename T> struct fmt::formatter<util::complex<T>> : formatter<T>
{
	template <typename FormatContext>
	auto format(const util::complex<T> &a, FormatContext &ctx)
	{
		if (a.im == 0)
			return formatter<T>::format(a.re, ctx);
		else if (a.re == 0)
		{
			formatter<T>::format(a.im, ctx);
			return format_to(ctx.out(), "i");
		}
		formatter<T>::format(a.re, ctx);
		format_to(ctx.out(), " + ");
		formatter<T>::format(a.im, ctx);
		return format_to(ctx.out(), "i");
	}
};
