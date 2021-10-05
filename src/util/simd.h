#pragma once

/**
 * Simple 'SIMD wrapper' that should make it easy to write vectorized code.
 * Contrary to many other such wrappers, this one does not actually contain any
 * inline assembly (or compiler builtins/intrinsics), but insead fully relies
 * on auto-vectorization.
 */

#include <cmath>

namespace util {

/**
 * TODO (maybe):
 *   - Compiler-dependent pragmas to ensure vectorization of the inner loops.
 *   - Force inline of basic operations.
 *   - Check if pass by value or by const& is better. Should not matter if
 *     everything is inlined as expected. Also beware of aliasing issues.
 */

using std::sin, std::cos, std::tan, std::exp, std::log, std::sqrt;

#define UTIL_SIMD_FOR(i, w) for (size_t i = 0; i < w; ++i)

template <typename T, size_t N> struct simd
{
	std::array<T, N> elements;

	simd() = default;
	simd(T value) { UTIL_SIMD_FOR(i, N) elements[i] = value; }

	T &operator[](size_t i) { return elements[i]; }
	T operator[](size_t i) const { return elements[i]; }
};

#define UTIL_DEFINE_SIMD_OPERATOR(op)                                          \
	template <typename T, typename U, size_t N>                                \
	auto operator op(simd<T, N> a, simd<U, N> b)                               \
	    ->simd<decltype(a[0] op b[0]), N>                                      \
	{                                                                          \
		simd<decltype(a[0] op b[0]), N> c;                                     \
		UTIL_SIMD_FOR(i, N) c[i] = a[i] op b[i];                               \
		return c;                                                              \
	}                                                                          \
	template <typename T, typename U, size_t N>                                \
	auto operator op(simd<T, N> a, U b)->simd<decltype(a[0] op b), N>          \
	{                                                                          \
		simd<decltype(a[0] op b), N> c;                                        \
		UTIL_SIMD_FOR(i, N) c[i] = a[i] op b;                                  \
		return c;                                                              \
	}                                                                          \
	template <typename T, typename U, size_t N>                                \
	auto operator op(T a, simd<U, N> b)->simd<decltype(a op b[0]), N>          \
	{                                                                          \
		simd<decltype(a op b[0]), N> c;                                        \
		UTIL_SIMD_FOR(i, N) c[i] = a op b[i];                                  \
		return c;                                                              \
	}                                                                          \
	template <typename T, typename U, size_t N>                                \
	simd<T, N> &operator op##=(simd<T, N> &a, simd<U, N> b)                    \
	{                                                                          \
		UTIL_SIMD_FOR(i, N) a[i] op## = b[i];                                  \
		return a;                                                              \
	}                                                                          \
	template <typename T, typename U, size_t N>                                \
	simd<T, N> &operator op##=(simd<T, N> &a, U b)                             \
	{                                                                          \
		UTIL_SIMD_FOR(i, N) a[i] op## = b;                                     \
		return a;                                                              \
	}

#define UTIL_DEFINE_SIMD_FUNCTION(fun)                                         \
	template <typename T, size_t N> simd<T, N> fun(simd<T, N> a)               \
	{                                                                          \
		simd<T, N> b;                                                          \
		UTIL_SIMD_FOR(i, N) b[i] = fun(a[i]);                                  \
		return b;                                                              \
	}

UTIL_DEFINE_SIMD_OPERATOR(+)
UTIL_DEFINE_SIMD_OPERATOR(-)
UTIL_DEFINE_SIMD_OPERATOR(*)
UTIL_DEFINE_SIMD_OPERATOR(/)

UTIL_DEFINE_SIMD_FUNCTION(sin)
UTIL_DEFINE_SIMD_FUNCTION(cos)
UTIL_DEFINE_SIMD_FUNCTION(tan)
UTIL_DEFINE_SIMD_FUNCTION(exp)
UTIL_DEFINE_SIMD_FUNCTION(log)
UTIL_DEFINE_SIMD_FUNCTION(sqrt)

#undef UTIL_DEFINE_SIMD_OPERATOR
#undef UTIL_DEFINE_SIMD_FUNCTION
#undef UTIL_SIMD_FOR

} // namespace util
