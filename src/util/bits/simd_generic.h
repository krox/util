#include <array>

namespace util {

template <class T, int N>
struct simd_generic
{
	static_assert(sizeof(T) == 4 || sizeof(T) == 8);
	using integer = std::conditional_t<sizeof(T) == 4, uint32_t, uint64_t>;

	// typedefs
	using scalar = T;
	using vector = std::array<T, N>;
	using mask = std::array<integer, N>;

	static_assert(sizeof(vector) == sizeof(mask) && sizeof(vector) == N * sizeof(scalar));

	// 'constructors'
	static UTIL_SIMD_INLINE vector make(scalar a) noexcept
	{
		vector r;
		UTIL_SIMD_FOR(i, N)
		r[i] = a;
		return r;
	}
	static UTIL_SIMD_INLINE vector make(scalar a, scalar b) noexcept
	{
		vector r;
		UTIL_SIMD_FOR(i, N)
		r[i] = (i & 1) ? b : a;
		return r;
	}

	static UTIL_SIMD_INLINE vector make_mask(bool a) noexcept
	{
		vector r;
		UTIL_SIMD_FOR(i, N)
		r[i] = a ? -1 : 0;
		return r;
	}
	static UTIL_SIMD_INLINE vector make_mask(bool a, bool b) noexcept
	{
		vector r;
		UTIL_SIMD_FOR(i, N)
		r[i] = (i & 1) ? (b ? -1 : 0) : (a ? -1 : 0);
		return r;
	}

	// insert/extract single elements
	static UTIL_SIMD_INLINE vector insert(vector a, int i, scalar b) noexcept
	{
		a[i] = b;
		return a;
	}
	static UTIL_SIMD_INLINE scalar extract(vector a, int i) noexcept
	{
		return a[i];
	}

#define UTIL_DEFINE_SIMD_UNARY(fun, expr)                 \
	static UTIL_SIMD_INLINE vector fun(vector a) noexcept \
	{                                                     \
		vector r;                                         \
		UTIL_SIMD_FOR(i, N)                               \
		r[i] = expr;                                      \
		return r;                                         \
	}
#define UTIL_DEFINE_SIMD_BINARY(fun, expr)                          \
	static UTIL_SIMD_INLINE vector fun(vector a, vector b) noexcept \
	{                                                               \
		vector r;                                                   \
		UTIL_SIMD_FOR(i, N)                                         \
		r[i] = expr;                                                \
		return r;                                                   \
	}
#define UTIL_DEFINE_SIMD_TERNARY(fun, expr)                                   \
	static UTIL_SIMD_INLINE vector fun(vector a, vector b, vector c) noexcept \
	{                                                                         \
		vector r;                                                             \
		UTIL_SIMD_FOR(i, N)                                                   \
		r[i] = expr;                                                          \
		return r;                                                             \
	}
#define UTIL_DEFINE_SIMD_COMPARE(fun, expr)                       \
	static UTIL_SIMD_INLINE mask fun(vector a, vector b) noexcept \
	{                                                             \
		mask r;                                                   \
		UTIL_SIMD_FOR(i, N)                                       \
		r[i] = expr;                                              \
		return r;                                                 \
	}

	UTIL_DEFINE_SIMD_UNARY(sqrt, std::sqrt(a[i]))
	UTIL_DEFINE_SIMD_BINARY(add, a[i] + b[i])
	UTIL_DEFINE_SIMD_BINARY(sub, a[i] - b[i])
	UTIL_DEFINE_SIMD_BINARY(mul, a[i] * b[i])
	UTIL_DEFINE_SIMD_BINARY(div, a[i] / b[i])
	UTIL_DEFINE_SIMD_BINARY(min, std::min(a[i], b[i]))
	UTIL_DEFINE_SIMD_BINARY(max, std::max(a[i], b[i]))
	UTIL_DEFINE_SIMD_TERNARY(fma, std::fma(a[i], b[i], c[i]))
	// UTIL_DEFINE_SIMD_TERNARY(add, std::fms(a[i], b[i], c[i]))
	UTIL_DEFINE_SIMD_COMPARE(cmplt, a[i] < b[i])
	UTIL_DEFINE_SIMD_COMPARE(cmple, a[i] <= b[i])
	UTIL_DEFINE_SIMD_COMPARE(cmpeq, a[i] == b[i])
	UTIL_DEFINE_SIMD_COMPARE(cmpge, a[i] >= b[i])
	UTIL_DEFINE_SIMD_COMPARE(cmpgt, a[i] > b[i])
	UTIL_DEFINE_SIMD_COMPARE(cmpnlt, !(a[i] < b[i]))
	UTIL_DEFINE_SIMD_COMPARE(cmpnle, !(a[i] <= b[i]))
	UTIL_DEFINE_SIMD_COMPARE(cmpneq, !(a[i] == b[i]))
	UTIL_DEFINE_SIMD_COMPARE(cmpnge, !(a[i] >= b[i]))
	UTIL_DEFINE_SIMD_COMPARE(cmpngt, !(a[i] > b[i]))

	// mask logic
	static UTIL_SIMD_INLINE bool all_of(mask a) noexcept
	{
		for (int i = 0; i < N; ++i)
			if (!a[i])
				return false;
		return true;
	}
	static UTIL_SIMD_INLINE bool none_of(mask a) noexcept
	{
		for (int i = 0; i < N; ++i)
			if (a[i])
				return false;
		return true;
	}

	// shuffles
	UTIL_DEFINE_SIMD_UNARY(permute0, a[i ^ 1])
	UTIL_DEFINE_SIMD_UNARY(permute1, a[i ^ 2])

	// m ? b : a
	static UTIL_SIMD_INLINE vector blend(vector a, vector b, mask m)
	{
		vector r;
		UTIL_SIMD_FOR(i, N)
		r[i] = m[i] ? b[i] : a[i];
		return r;
	}

#undef UTIL_DEFINE_SIMD_UNARY
#undef UTIL_DEFINE_SIMD_BINARY
#undef UTIL_DEFINE_SIMD_TERNARY
#undef UTIL_DEFINE_SIMD_COMPARE
};

} // namespace util