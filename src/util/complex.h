#pragma once

#include <cmath>
#include <utility>

namespace util {

/**
 * Similar to std::complex, but:
 *     - Implemented as POD, i.e. '.real' instead of '.real()'.
 *     - No attempt is made to handle edge cases perfectly. In particular,
 *       multiplication is just defined as (a+bi)(c+di) = (ac-bd)+(ad+bc)i.
 *       (std::complex by default contains inf/nan checks that can
 *       prohibit vectorization, thus costing quite some performance)
 *     - Allow base types other than float/double/long double. For example
 *         * explicit (horizontal) vectorization: simd<float>, ...
 *         * high-precision types: MpfrFloat, ddouble, ...
 *         * non-floating point types: int, Rational, ...
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
	T real, imag;

	complex() = default;
	complex(T r) : real(std::move(r)), imag(0) {}
	explicit complex(T r, T i) : real(std::move(r)), imag(std::move(i)) {}
};

using std::exp, std::sin, std::cos, std::sinh, std::cosh, std::abs, std::sqrt;

template <typename T> complex<T> operator+(complex<T> const &a)
{
	return complex<T>(+a.real, +a.imag);
}
template <typename T> complex<T> operator-(complex<T> const &a)
{
	return complex<T>(-a.real, -a.imag);
}
template <typename T>
complex<T> operator+(complex<T> const &a, complex<T> const &b)
{
	return complex<T>(a.real + b.real, a.imag + b.imag);
}
template <typename T>
complex<T> operator-(complex<T> const &a, complex<T> const &b)
{
	return complex<T>(a.real - b.real, a.imag - b.imag);
}
template <typename T>
complex<T> operator*(complex<T> const &a, complex<T> const &b)
{
	return complex<T>(a.real * b.real - a.imag * b.imag,
	                  a.real * b.imag + a.imag * b.real);
}
template <typename T> complex<T> inverse(complex<T> const &a)
{
	return conj(a) / norm(a);
}
template <typename T>
complex<T> operator/(complex<T> const &a, complex<T> const &b)
{
	return a * inverse(b);
}
template <typename T> T norm2(complex<T> const &a)
{
	return a.real * a.real + a.imag * a.imag;
}
template <typename T> T norm(complex<T> const &a) { return norm2(a); }
template <typename T> T abs(complex<T> const &a) { return sqrt(norm(a)); }
template <typename T> complex<T> conj(complex<T> const &a)
{
	return complex<T>(a.real, -a.imag);
}
template <typename T> T const &real(complex<T> const &a) { return a.real; }
template <typename T> T const &imag(complex<T> const &a) { return a.imag; }

// mixed complex <-> real operations
template <typename T> complex<T> operator+(complex<T> const &a, T const &b)
{
	return complex<T>(a.real + b, a.imag);
}
template <typename T> complex<T> operator+(T const &a, complex<T> const &b)
{
	return complex<T>(a + b.real, b.imag);
}
template <typename T> complex<T> operator-(complex<T> const &a, T const &b)
{
	return complex<T>(a.real - b, a.imag);
}
template <typename T> complex<T> operator-(T const &a, complex<T> const &b)
{
	return complex<T>(a - b.real, -b.imag);
}
template <typename T> complex<T> operator*(complex<T> const &a, T const &b)
{
	return complex<T>(a.real * b, a.imag * b);
}
template <typename T> complex<T> operator*(T const &a, complex<T> const &b)
{
	return complex<T>(a * b.real, a * b.imag);
}
template <typename T> complex<T> operator/(complex<T> const &a, T const &b)
{
	return complex<T>(a.real / b, a.imag / b);
}
template <typename T> complex<T> operator/(T const &a, complex<T> const &b)
{
	return a * inverse(b);
}
template <typename T> complex<T> &operator+=(complex<T> &a, const T &b)
{
	a.real += b;
	return a;
}
template <typename T> complex<T> &operator-=(complex<T> &a, const T &b)
{
	a.real -= b;
	return a;
}
template <typename T> complex<T> &operator*=(complex<T> &a, T b)
{
	// beware of aliasing
	a.real *= b;
	b.imag *= b;
	return a;
}
template <typename T> complex<T> &operator/=(complex<T> &a, T b)
{
	// beware of aliasing
	a.real /= b;
	a.imag /= b;
	return a;
}

// op-assigns (beware of aliasing!)
template <typename T> complex<T> &operator+=(complex<T> &a, complex<T> const &b)
{
	a.real += b.real;
	a.imag += b.imag;
	return a;
}
template <typename T> complex<T> &operator-=(complex<T> &a, complex<T> const &b)
{
	a.real -= b.real;
	a.imag -= b.imag;
	return a;
}
template <typename T> complex<T> &operator*=(complex<T> &a, complex<T> const &b)
{
	a = a * b;
	return a;
}
template <typename T> complex<T> &operator/=(complex<T> &a, complex<T> const &b)
{
	a = a / b;
	return a;
}

// exponentials and trigonometry
template <typename T> complex<T> exp(complex<T> const &a)
{
	return exp(a.real) * complex<T>(cos(a.imag), sin(a.imag));
}
template <typename T> complex<T> sin(complex<T> const &a)
{
	return complex<T>(sin(a.real) * cosh(a.imag), cos(a.real) * sinh(a.imag));
}
template <typename T> complex<T> cos(complex<T> const &a)
{
	return complex<T>(cos(a.real) * cosh(a.imag), -sin(a.real) * sinh(a.imag));
}
template <typename T> complex<T> sinh(complex<T> const &a)
{
	return complex<T>(sinh(a.real) * cos(a.imag), cosh(a.real) * sin(a.imag));
}
template <typename T> complex<T> cosh(complex<T> const &a)
{
	return complex<T>(cosh(a.real) * cos(a.imag), sinh(a.real) * sin(a.imag));
}

// dummy operations to simplify templates that work with either real or complex
float conj(float a) { return a; }
float norm(float a) { return a * a; }
float norm2(float a) { return a * a; }
float real(float a) { return a; }
float imag([[maybe_unused]] float a) { return 0; }
double conj(double a) { return a; }
double norm(double a) { return a * a; }
double norm2(double a) { return a * a; }
double real(double a) { return a; }
double imag([[maybe_unused]] double a) { return 0; }

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

} // namespace util

template <typename T> struct fmt::formatter<util::complex<T>> : formatter<T>
{
	template <typename FormatContext>
	auto format(const util::complex<T> &a, FormatContext &ctx)
	{
		if (a.imag == 0)
			return formatter<T>::format(a.real, ctx);
		else if (a.real == 0)
		{
			formatter<T>::format(a.imag, ctx);
			return format_to(ctx.out(), "i");
		}
		formatter<T>::format(a.real, ctx);
		format_to(ctx.out(), " + ");
		formatter<T>::format(a.imag, ctx);
		return format_to(ctx.out(), "i");
	}
};
