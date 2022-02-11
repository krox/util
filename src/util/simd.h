#pragma once

/**
 * Simple 'SIMD wrapper' that should make it easy to write vectorized code.
 * Contrary to many other such wrappers, this one does not actually contain any
 * inline assembly (or compiler builtins/intrinsics), but insead fully relies
 * on auto-vectorization.
 */

#include <cmath>

namespace util {

// configuration (no auto-detect yet)

#if defined(__clang__)
// no vector builtin
//     * TODO: actually, clang does support this, but differs slightly from GCC
//     * clang auto-vectorizer seems more powerful than GCC, so might be fine
#elif defined(__GNUC__) || defined(__GNUG__)
#define UTIL_SIMD_GCC_BUILTIN // use __attribute__((vector_size(...)))
#endif

#define UTIL_SIMD_INLINE __attribute__((always_inline)) inline // force inline
//#define UTIL_SIMD_INLINE inline                                // dont force

// static constexpr size_t simd_register_size = 16; // 128 bit (SSE)
static constexpr size_t simd_register_size = 32; // 256 bit (AVX, AVX2)
// static constexpr size_t simd_register_size = 64; // 512 bit (AVX512)

/**
 * TODO (maybe):
 *   - Compiler-dependent pragmas to ensure vectorization of the inner loops.
 *   - Force inline of basic operations.
 *   - Check if pass by value or by const& is better. Should not matter if
 *     everything is inlined as expected. Also beware of aliasing issues.
 */

// Type trait that just returns the type itself. This can be used to establish
// "non-deduced contexts", i.e. make some function arguments not participate
// in template argument deduction. This will be part of C++20.
template <class T> struct type_identity
{
	using type = T;
};
template <class T> using type_identity_t = typename type_identity<T>::type;

using std::sin, std::cos, std::tan, std::exp, std::log, std::sqrt;

#define UTIL_SIMD_FOR(i, w) for (size_t i = 0; i < w; ++i)

template <typename T, size_t W = simd_register_size / sizeof(T)>
struct alignas(sizeof(T) * W) simd;

template <size_t size> struct IntType;
template <> struct IntType<1>
{
	using type = int8_t;
};
template <> struct IntType<2>
{
	using type = int16_t;
};
template <> struct IntType<4>
{
	using type = int32_t;
};
template <> struct IntType<8>
{
	using type = int64_t;
};

#ifdef UTIL_SIMD_GCC_BUILTIN

template <typename T, size_t W> struct alignas(sizeof(T) * W) simd
{
	static_assert(W > 0 && (W & (W - 1)) == 0); // W has to be a power of two

	using value_type = T;
	// simd<integer> type of the same size/width
	using int_type = simd<typename IntType<sizeof(T)>::type, W>;

	typedef T Vec_ __attribute__((vector_size(W * sizeof(T))));
	Vec_ v_;

	simd() = default;

	explicit simd(T v) { UTIL_SIMD_FOR(i, W) v_[i] = v; }

	static constexpr size_t size() { return W; }

	UTIL_SIMD_INLINE T &operator[](size_t i) { return ((T *)this)[i]; }
	UTIL_SIMD_INLINE T operator[](size_t i) const
	{
		return ((T const *)this)[i];
	}
};

template <typename T, size_t W>
UTIL_SIMD_INLINE simd<T, W> operator+(simd<T, W> a)
{
	simd<T, W> b;
	b.v_ = +a.v_;
	return b;
}
template <typename T, size_t W>
UTIL_SIMD_INLINE simd<T, W> operator-(simd<T, W> a)
{
	simd<T, W> b;
	b.v_ = -a.v_;
	return b;
}

#define UTIL_DEFINE_SIMD_OPERATOR(op)                                          \
	template <typename T, typename U, size_t W>                                \
	UTIL_SIMD_INLINE auto operator op(simd<T, W> a, simd<U, W> b)              \
	    ->simd<decltype(a[0] op b[0]), W>                                      \
	{                                                                          \
		simd<decltype(a[0] op b[0]), W> c;                                     \
		c.v_ = a.v_ op b.v_;                                                   \
		return c;                                                              \
	}                                                                          \
	template <typename T, size_t W>                                            \
	UTIL_SIMD_INLINE simd<T, W> operator op(simd<T, W> a,                      \
	                                        type_identity_t<T> b)              \
	{                                                                          \
		simd<T, W> c;                                                          \
		c.v_ = a.v_ op b;                                                      \
		return c;                                                              \
	}                                                                          \
	template <typename T, size_t W>                                            \
	UTIL_SIMD_INLINE simd<T, W> operator op(type_identity_t<T> a,              \
	                                        simd<T, W> b)                      \
	{                                                                          \
		simd<T, W> c;                                                          \
		c.v_ = a op b.v_;                                                      \
		return c;                                                              \
	}                                                                          \
	template <typename T, typename U, size_t W>                                \
	UTIL_SIMD_INLINE simd<T, W> &operator op##=(simd<T, W> &a, simd<U, W> b)   \
	{                                                                          \
		a.v_ op## = b.v_;                                                      \
		return a;                                                              \
	}                                                                          \
	template <typename T, size_t W>                                            \
	UTIL_SIMD_INLINE simd<T, W> &operator op##=(simd<T, W> &a,                 \
	                                            type_identity_t<T> b)          \
	{                                                                          \
		a.v_ op## = b;                                                         \
		return a;                                                              \
	}

#define UTIL_DEFINE_SIMD_FUNCTION(fun)                                         \
	template <typename T, size_t W>                                            \
	UTIL_SIMD_INLINE simd<T, W> fun(simd<T, W> a)                              \
	{                                                                          \
		simd<T, W> b;                                                          \
		UTIL_SIMD_FOR(i, W) b[i] = fun(a[i]);                                  \
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

#else

template <typename T, size_t W> struct alignas(sizeof(T) * W) simd
{
	static_assert(W > 0 && (W & (W - 1)) == 0); // W has to be a power of two

	using value_type = T;
	// simd<integer> type of the same size/width
	using int_type = simd<typename IntType<sizeof(T)>::type, W>;

	T elements[W];

	simd() = default;
	explicit simd(T value) { UTIL_SIMD_FOR(i, W) elements[i] = value; }

	static constexpr size_t size() { return W; }

	UTIL_SIMD_INLINE T &operator[](size_t i) { return elements[i]; }
	UTIL_SIMD_INLINE T operator[](size_t i) const { return elements[i]; }
};

template <typename T, size_t W>
UTIL_SIMD_INLINE simd<T, W> operator+(simd<T, W> a)
{
	simd<T, W> b;
	UTIL_SIMD_FOR(i, W) b[i] = +a[i];
	return b;
}
template <typename T, size_t W>
UTIL_SIMD_INLINE simd<T, W> operator-(simd<T, W> a)
{
	simd<T, W> b;
	UTIL_SIMD_FOR(i, W) b[i] = -a[i];
	return b;
}

#define UTIL_DEFINE_SIMD_OPERATOR(op)                                          \
	template <typename T, typename U, size_t W>                                \
	UTIL_SIMD_INLINE auto operator op(simd<T, W> a, simd<U, W> b)              \
	    ->simd<decltype(a[0] op b[0]), W>                                      \
	{                                                                          \
		simd<decltype(a[0] op b[0]), W> c;                                     \
		UTIL_SIMD_FOR(i, W) c[i] = a[i] op b[i];                               \
		return c;                                                              \
	}                                                                          \
	template <typename T, size_t W>                                            \
	UTIL_SIMD_INLINE simd<T, W> operator op(simd<T, W> a,                      \
	                                        type_identity_t<T> b)              \
	{                                                                          \
		simd<T, W> c;                                                          \
		UTIL_SIMD_FOR(i, W) c[i] = a[i] op b;                                  \
		return c;                                                              \
	}                                                                          \
	template <typename T, size_t W>                                            \
	UTIL_SIMD_INLINE simd<T, W> operator op(type_identity_t<T> a,              \
	                                        simd<T, W> b)                      \
	{                                                                          \
		simd<T, W> c;                                                          \
		UTIL_SIMD_FOR(i, W) c[i] = a op b[i];                                  \
		return c;                                                              \
	}                                                                          \
	template <typename T, typename U, size_t W>                                \
	UTIL_SIMD_INLINE simd<T, W> &operator op##=(simd<T, W> &a, simd<U, W> b)   \
	{                                                                          \
		UTIL_SIMD_FOR(i, W) a[i] op## = b[i];                                  \
		return a;                                                              \
	}                                                                          \
	template <typename T, typename U, size_t W>                                \
	UTIL_SIMD_INLINE simd<T, W> &operator op##=(simd<T, W> &a, U b)            \
	{                                                                          \
		UTIL_SIMD_FOR(i, W) a[i] op## = b;                                     \
		return a;                                                              \
	}

#define UTIL_DEFINE_SIMD_FUNCTION(fun)                                         \
	template <typename T, size_t W>                                            \
	UTIL_SIMD_INLINE simd<T, W> fun(simd<T, W> a)                              \
	{                                                                          \
		simd<T, W> b;                                                          \
		UTIL_SIMD_FOR(i, W) b[i] = fun(a[i]);                                  \
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

#endif

// operations that dont act independently accross SIMD lanes
//     * prefixed with 'v' in order not to clash with parallel instructions
//     * can be overloaded for horizontal SIMD i.e. things like
//         vsum(Matrix<simd<double>>) -> Matrix<double>

#ifdef UTIL_SIMD_GCC_BUILTIN

template <typename T, size_t W>
UTIL_SIMD_INLINE simd<T, W> vshuffle(simd<T, W> a,
                                     typename simd<T, W>::int_type mask)
{
	simd<T, W> r;
	r.v_ = __builtin_shuffle(a.v_, mask.v_);
	return r;
}

#else

template <typename T, size_t W>
UTIL_SIMD_INLINE simd<T, W> vshuffle(simd<T, W> a,
                                     typename simd<T, W>::int_type mask)
{
	simd<T, W> r;
	for (size_t i = 0; i < W; ++i)
		r[i] = a[mask[i]];
	return r;
}

#endif

template <typename T, size_t W> UTIL_SIMD_INLINE T vsum(simd<T, W> a)
{
	// TODO: replace with builtin. as it stands, its not vectorizable
	T r = a[0];
	for (size_t i = 1; i < W; ++i)
		r += a[i];
	return r;
}
template <typename T, size_t W>
UTIL_SIMD_INLINE T vextract(simd<T, W> a, size_t lane)
{
	// assert(i < W);
	return a[lane];
}
template <typename T, size_t W>
UTIL_SIMD_INLINE void vinsert(simd<T, W> &a, size_t lane, T b)
{
	a[lane] = b;
}

///////
UTIL_SIMD_INLINE float vshuffle(float a, int32_t) { return a; }
UTIL_SIMD_INLINE float vsum(float a) { return a; }
UTIL_SIMD_INLINE float vextract(float a, size_t) { return a; }
UTIL_SIMD_INLINE void vinsert(float &a, size_t, float b) { a = b; }
UTIL_SIMD_INLINE double vshuffle(double a, int64_t) { return a; }
UTIL_SIMD_INLINE double vsum(double a) { return a; }
UTIL_SIMD_INLINE double vextract(double a, size_t) { return a; }
UTIL_SIMD_INLINE void vinsert(double &a, size_t, double b) { a = b; }

} // namespace util
