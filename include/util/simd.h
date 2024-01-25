#pragma once

/**
 * Simple 'SIMD wrapper' that should make it easy to write vectorized code.
 *
 * Backend is in util/bits/simd_*.h. This file only has some
 * platform-independent high-level functions.
 */

#include "fmt/format.h"
#include <cmath>

// force inline of low level SIMD functions
#define UTIL_SIMD_INLINE __attribute__((always_inline)) inline

#if __AVX__

#include "util/bits/simd_avx.h"

namespace util {

static constexpr size_t simd_native_bytes = 32; // 256 bit (AVX, AVX2)

template <class T, int W> struct simd_impl;
template <> struct simd_impl<float, 4>
{
	using impl = sse_float;
};
template <> struct simd_impl<double, 2>
{
	using impl = sse_double;
};
template <> struct simd_impl<float, 8>
{
	using impl = avx_float;
};
template <> struct simd_impl<double, 4>
{
	using impl = avx_double;
};
} // namespace util

#elif _OPENMP

#define UTIL_SIMD_FOR(i, n) _Pragma("omp simd") for (int i = 0; i < n; ++i)
#include "util/bits/simd_generic.h"
#undef UTIL_SIMD_FOR
namespace util {
static constexpr size_t simd_native_bytes = 32; // 256 bit, arbitrary
template <class T, int W> struct simd_impl
{
	using impl = simd_generic<T, W>;
};
} // namespace util

#else
#error "no SIMD support found (maybe just missing '-march=native' ?)"
#endif

namespace util {

template <class T, int W = simd_native_bytes / sizeof(T)>
struct alignas(sizeof(T) * W) simd
{
	using impl = typename simd_impl<T, W>::impl;

	using value_type = T;
	static constexpr size_t size() { return W; }

	static_assert(std::has_single_bit(size_t(W)));
	static_assert(std::is_same_v<value_type, typename impl::scalar>);
	static_assert(size() * sizeof(T) == sizeof(typename impl::vector));

	typename impl::vector v_;

	simd() = default;
	simd(T a) noexcept : v_(impl::make(a)) {}
	simd(T a, T b) noexcept : v_(impl::make(a, b)) {}
	simd(T a, T b, T c, T d) noexcept : v_(impl::make(a, b, c, d)) {}
	simd(T a, T b, T c, T d, T e, T f, T g, T h) noexcept
	    : v_(impl::make(a, b, c, d, e, f, g, h))
	{}

	// TODO: this should be removed in favor of actual SIMD-PRNG
	template <class Rng> static simd random_uniform(Rng &rng)
	{
		simd r;
		for (int i = 0; i < W; ++i)
			r = vinsert(r, i, rng.template uniform<T>());
		return r;
	}
	template <class Rng> static simd random_normal(Rng &rng)
	{
		simd r;
		for (int i = 0; i < W; ++i)
			r = vinsert(r, i, rng.template normal<T>());
		return r;
	}
};

template <class T, int W = simd_native_bytes / sizeof(T)>
struct alignas(sizeof(T) * W) simd_mask
{
	using impl = typename simd_impl<T, W>::impl;

	using value_type = bool;
	static constexpr size_t size() { return W; }

	static_assert(std::has_single_bit(size_t(W)));
	static_assert(sizeof(typename impl::mask) == sizeof(typename impl::vector));

	typename impl::mask v_;

	simd_mask() = default;
	simd_mask(bool a) noexcept : v_(impl::make_mask(a)) {}
	simd_mask(bool a, bool b) noexcept : v_(impl::make_mask(a, b)) {}
	simd_mask(bool a, bool b, bool c, bool d) noexcept
	    : v_(impl::make_mask(a, b, c, d))
	{}
};

#define UTIL_DEFINE_UNARY(op, fun)                                             \
	template <class T, int W>                                                  \
	UTIL_SIMD_INLINE simd<T, W> op(simd<T, W> a) noexcept                      \
	{                                                                          \
		simd<T, W> r;                                                          \
		r.v_ = simd<T, W>::impl::fun(a.v_);                                    \
		return r;                                                              \
	}

#define UTIL_DEFINE_BINARY(op, fun)                                            \
	template <class T, int W>                                                  \
	UTIL_SIMD_INLINE simd<T, W> op(simd<T, W> a, simd<T, W> b) noexcept        \
	{                                                                          \
		simd<T, W> r;                                                          \
		r.v_ = simd<T, W>::impl::fun(a.v_, b.v_);                              \
		return r;                                                              \
	}                                                                          \
	template <class T, int W>                                                  \
	UTIL_SIMD_INLINE simd<T, W> op(simd<T, W> a,                               \
	                               std::type_identity_t<T> b) noexcept         \
	{                                                                          \
		simd<T, W> r;                                                          \
		r.v_ = simd<T, W>::impl::fun(a.v_, simd<T, W>::impl::make(b));         \
		return r;                                                              \
	}                                                                          \
	template <class T, int W>                                                  \
	UTIL_SIMD_INLINE simd<T, W> op(std::type_identity_t<T> a,                  \
	                               simd<T, W> b) noexcept                      \
	{                                                                          \
		simd<T, W> r;                                                          \
		r.v_ = simd<T, W>::impl::fun(simd<T, W>::impl::make(a), b.v_);         \
		return r;                                                              \
	}

#define UTIL_DEFINE_REDUCTION(op, fun)                                         \
	template <class T> UTIL_SIMD_INLINE T op(simd<T, 2> a) noexcept            \
	{                                                                          \
		using impl = typename simd<T, 2>::impl;                                \
		auto r = impl::fun(a.v_, impl::permute0(a.v_));                        \
		return impl::extract(r, 0);                                            \
	}                                                                          \
	template <class T> UTIL_SIMD_INLINE T op(simd<T, 4> a) noexcept            \
	{                                                                          \
		using impl = typename simd<T, 4>::impl;                                \
		auto r = impl::fun(a.v_, impl::permute0(a.v_));                        \
		r = impl::fun(r, impl::permute1(r));                                   \
		return impl::extract(r, 0);                                            \
	}                                                                          \
	template <class T> UTIL_SIMD_INLINE T op(simd<T, 8> a) noexcept            \
	{                                                                          \
		using impl = typename simd<T, 8>::impl;                                \
		auto r = impl::fun(a.v_, impl::permute0(a.v_));                        \
		r = impl::fun(r, impl::permute1(r));                                   \
		r = impl::fun(r, impl::permute2(r));                                   \
		return impl::extract(r, 0);                                            \
	}

#define UTIL_DEFINE_COMPARISON(op, fun)                                        \
	template <class T, int W>                                                  \
	UTIL_SIMD_INLINE simd_mask<T, W> op(simd<T, W> a, simd<T, W> b) noexcept   \
	{                                                                          \
		simd_mask<T, W> r;                                                     \
		r.v_ = simd<T, W>::impl::fun(a.v_, b.v_);                              \
		return r;                                                              \
	}                                                                          \
	template <class T, int W>                                                  \
	UTIL_SIMD_INLINE simd_mask<T, W> op(simd<T, W> a,                          \
	                                    std::type_identity_t<T> b) noexcept    \
	{                                                                          \
		simd_mask<T, W> r;                                                     \
		r.v_ = simd<T, W>::impl::fun(a.v_, simd<T, W>::impl::make(b));         \
		return r;                                                              \
	}                                                                          \
	template <class T, int W>                                                  \
	UTIL_SIMD_INLINE simd_mask<T, W> op(std::type_identity_t<T> a,             \
	                                    simd<T, W> b) noexcept                 \
	{                                                                          \
		simd_mask<T, W> r;                                                     \
		r.v_ = simd<T, W>::impl::fun(simd<T, W>::impl::make(a), b.v_);         \
		return r;                                                              \
	}

#define UTIL_DEFINE_COMPONENT_WISE_UNARY(fun)                                  \
	template <class T, int W>                                                  \
	UTIL_SIMD_INLINE simd<T, W> fun(simd<T, W> a) noexcept                     \
	{                                                                          \
		simd<T, W> r;                                                          \
		for (int i = 0; i < W; ++i)                                            \
			r = vinsert(r, i, std::fun(vextract(a, i)));                       \
		return r;                                                              \
	}

UTIL_DEFINE_BINARY(operator+, add)
UTIL_DEFINE_BINARY(operator-, sub)
UTIL_DEFINE_BINARY(operator*, mul)
UTIL_DEFINE_BINARY(operator/, div)
UTIL_DEFINE_BINARY(min, min)
UTIL_DEFINE_BINARY(max, max)
UTIL_DEFINE_UNARY(sqrt, sqrt)
UTIL_DEFINE_UNARY(vpermute0, permute0)
UTIL_DEFINE_UNARY(vpermute1, permute1)
UTIL_DEFINE_UNARY(vpermute2, permute2)
UTIL_DEFINE_REDUCTION(vsum, add)
UTIL_DEFINE_REDUCTION(vmin, min)
UTIL_DEFINE_REDUCTION(vmax, max)
UTIL_DEFINE_COMPARISON(operator==, cmpeq)
UTIL_DEFINE_COMPARISON(operator!=, cmpneq)
UTIL_DEFINE_COMPARISON(operator<, cmplt)
UTIL_DEFINE_COMPARISON(operator<=, cmple)
UTIL_DEFINE_COMPARISON(operator>, cmpgt)
UTIL_DEFINE_COMPARISON(operator>=, cmpge)
UTIL_DEFINE_COMPONENT_WISE_UNARY(sin)
UTIL_DEFINE_COMPONENT_WISE_UNARY(cos)
UTIL_DEFINE_COMPONENT_WISE_UNARY(tan)
UTIL_DEFINE_COMPONENT_WISE_UNARY(exp)
UTIL_DEFINE_COMPONENT_WISE_UNARY(log)

#undef UTIL_DEFINE_UNARY
#undef UTIL_DEFINE_BINARY
#undef UTIL_DEFINE_REDUCTION
#undef UTIL_DEFINE_COMPARISON
#undef UTIL_DEFINE_COMPONENT_WISE_UNARY

template <std::floating_point T, int W> simd<T, W> norm2(simd<T, W> a) noexcept
{
	return a * a;
}

template <std::floating_point T, int W> simd<T, W> conj(simd<T, W> a) noexcept
{
	return a;
}

template <std::floating_point T> T vsum(T a) noexcept { return a; }
template <std::floating_point T> T vmin(T a) noexcept { return a; }
template <std::floating_point T> T vmax(T a) noexcept { return a; }

template <class T, int W>
UTIL_SIMD_INLINE simd<T, W> operator-(simd<T, W> a) noexcept
{
	return T(0) - a;
}

template <class T, int W, class U>
UTIL_SIMD_INLINE simd<T, W> &operator+=(simd<T, W> &a, U b) noexcept
{
	a = a + b;
	return a;
}

template <class T, int W, class U>
UTIL_SIMD_INLINE simd<T, W> &operator-=(simd<T, W> &a, U b) noexcept
{
	a = a - b;
	return a;
}

template <class T, int W, class U>
UTIL_SIMD_INLINE simd<T, W> &operator*=(simd<T, W> &a, U b) noexcept
{
	a = a * b;
	return a;
}

template <class T, int W, class U>
UTIL_SIMD_INLINE simd<T, W> &operator/=(simd<T, W> &a, U b) noexcept
{
	a = a / b;
	return a;
}

template <class T, int W>
UTIL_SIMD_INLINE T vextract(simd<T, W> a, int i) noexcept
{
	return simd<T, W>::impl::extract(a.v_, i);
}

template <class T, int W>
UTIL_SIMD_INLINE simd<T, W> vinsert(simd<T, W> a, int i,
                                    std::type_identity_t<T> b) noexcept
{
	simd<T, W> r;
	r.v_ = simd<T, W>::impl::insert(a.v_, i, b);
	return r;
}

template <class T, int W>
UTIL_SIMD_INLINE bool all_of(simd_mask<T, W> a) noexcept
{
	return simd_mask<T, W>::impl::all_of(a.v_);
}

// TODO: remove this. should do proper simd-prng
template <class T, int W, class Rng> void gaussian(simd<T, W> x, Rng &rng)
{
	for (int i = 0; i < W; ++i)
		x = vinsert(x, i, rng.normal());
}

using vfloat4 = simd<float, 4>;
using vfloat4_mask = simd_mask<float, 4>;
using vfloat8 = simd<float, 8>;
using vfloat8_mask = simd_mask<float, 8>;

using vdouble2 = simd<double, 2>;
using vdouble2_mask = simd_mask<double, 2>;
using vdouble4 = simd<double, 4>;
using vdouble4_mask = simd_mask<double, 4>;

} // namespace util

template <class T, int W>
struct fmt::formatter<util::simd<T, W>> : fmt::formatter<T>
{
	// NOTE: parse() is inherited from formatter<T>

	template <class FormatContext>
	auto format(util::simd<T, W> a, FormatContext &ctx) -> decltype(ctx.out())
	{
		static_assert(W == 2 || W == 4 || W == 8 || W == 16 || W == 32);

		auto it = ctx.out();
		*it++ = '{';

		for (int i = 0; i < W; ++i)
		{
			if (i)
				it = fmt::format_to(it, ", ");
			formatter<T>::format(vextract(a, i), ctx);
		}
		*it++ = '}';
		return it;
	}
};
