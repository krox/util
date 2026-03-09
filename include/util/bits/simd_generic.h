#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <type_traits>

namespace util {

template <class T, int N>
struct simd_generic
{
	static_assert(sizeof(T) == 4 || sizeof(T) == 8);
	static_assert(N >= 2);
	static_assert((N & (N - 1)) == 0);

	using integer = std::conditional_t<sizeof(T) == 4, uint32_t, uint64_t>;
	using scalar = T;
	using vector = std::array<T, N>;
	using mask = std::array<integer, N>;

	static_assert(sizeof(vector) == sizeof(mask));
	static_assert(sizeof(vector) == N * sizeof(scalar));

	static UTIL_SIMD_INLINE vector make(scalar a) noexcept
	{
		vector r{};
		for (int i = 0; i < N; ++i)
			r[i] = a;
		return r;
	}

	static UTIL_SIMD_INLINE vector make(scalar a, scalar b) noexcept
	    requires(N >= 2)
	{
		vector r{};
		for (int i = 0; i < N; ++i)
			r[i] = (i & 1) ? b : a;
		return r;
	}

	static UTIL_SIMD_INLINE vector make(scalar a, scalar b, scalar c, scalar d) noexcept
	    requires(N >= 4)
	{
		vector r{};
		for (int i = 0; i < N; ++i)
			r[i] = (i & 2) ? ((i & 1) ? d : c) : ((i & 1) ? b : a);
		return r;
	}

	static UTIL_SIMD_INLINE vector make(scalar a, scalar b, scalar c, scalar d, scalar e, scalar f, scalar g, scalar h) noexcept
	    requires(N >= 8)
	{
		vector r{};
		for (int i = 0; i < N; ++i)
			r[i] = (i & 4) ? ((i & 2) ? ((i & 1) ? h : g) : ((i & 1) ? f : e)) : ((i & 2) ? ((i & 1) ? d : c) : ((i & 1) ? b : a));
		return r;
	}

	static UTIL_SIMD_INLINE mask make_mask(bool a) noexcept
	{
		mask r{};
		const integer value = a ? static_cast<integer>(-1) : 0;
		for (int i = 0; i < N; ++i)
			r[i] = value;
		return r;
	}

	static UTIL_SIMD_INLINE mask make_mask(bool a, bool b) noexcept
	    requires(N >= 2)
	{
		mask r{};
		const integer value_a = a ? static_cast<integer>(-1) : 0;
		const integer value_b = b ? static_cast<integer>(-1) : 0;
		for (int i = 0; i < N; ++i)
			r[i] = (i & 1) ? value_b : value_a;
		return r;
	}

	static UTIL_SIMD_INLINE mask make_mask(bool a, bool b, bool c, bool d) noexcept
	    requires(N >= 4)
	{
		mask r{};
		const integer value_a = a ? static_cast<integer>(-1) : 0;
		const integer value_b = b ? static_cast<integer>(-1) : 0;
		const integer value_c = c ? static_cast<integer>(-1) : 0;
		const integer value_d = d ? static_cast<integer>(-1) : 0;
		for (int i = 0; i < N; ++i)
			r[i] = (i & 2) ? ((i & 1) ? value_d : value_c) : ((i & 1) ? value_b : value_a);
		return r;
	}

	static UTIL_SIMD_INLINE mask make_mask(bool a, bool b, bool c, bool d, bool e, bool f, bool g, bool h) noexcept
	    requires(N >= 8)
	{
		mask r{};
		const integer value_a = a ? static_cast<integer>(-1) : 0;
		const integer value_b = b ? static_cast<integer>(-1) : 0;
		const integer value_c = c ? static_cast<integer>(-1) : 0;
		const integer value_d = d ? static_cast<integer>(-1) : 0;
		const integer value_e = e ? static_cast<integer>(-1) : 0;
		const integer value_f = f ? static_cast<integer>(-1) : 0;
		const integer value_g = g ? static_cast<integer>(-1) : 0;
		const integer value_h = h ? static_cast<integer>(-1) : 0;
		for (int i = 0; i < N; ++i)
			r[i] = (i & 4) ? ((i & 2) ? ((i & 1) ? value_h : value_g) : ((i & 1) ? value_f : value_e)) : ((i & 2) ? ((i & 1) ? value_d : value_c) : ((i & 1) ? value_b : value_a));
		return r;
	}

	static UTIL_SIMD_INLINE vector insert(vector a, int i, scalar b) noexcept
	{
		a[i] = b;
		return a;
	}

	static UTIL_SIMD_INLINE scalar extract(const vector &a, int i) noexcept
	{
		return a[i];
	}

#define UTIL_DEFINE_SIMD_UNARY(fun, expr)                        \
	static UTIL_SIMD_INLINE vector fun(const vector &a) noexcept \
	{                                                            \
		vector r{};                                              \
		for (int i = 0; i < N; ++i)                              \
			r[i] = (expr);                                       \
		return r;                                                \
	}

#define UTIL_DEFINE_SIMD_BINARY(fun, expr)                                        \
	static UTIL_SIMD_INLINE vector fun(const vector &a, const vector &b) noexcept \
	{                                                                             \
		vector r{};                                                               \
		for (int i = 0; i < N; ++i)                                               \
			r[i] = (expr);                                                        \
		return r;                                                                 \
	}

#define UTIL_DEFINE_SIMD_TERNARY(fun, expr)                                                        \
	static UTIL_SIMD_INLINE vector fun(const vector &a, const vector &b, const vector &c) noexcept \
	{                                                                                              \
		vector r{};                                                                                \
		for (int i = 0; i < N; ++i)                                                                \
			r[i] = (expr);                                                                         \
		return r;                                                                                  \
	}

#define UTIL_DEFINE_SIMD_COMPARE(fun, expr)                                     \
	static UTIL_SIMD_INLINE mask fun(const vector &a, const vector &b) noexcept \
	{                                                                           \
		mask r{};                                                               \
		for (int i = 0; i < N; ++i)                                             \
			r[i] = (expr) ? static_cast<integer>(-1) : 0;                       \
		return r;                                                               \
	}

	UTIL_DEFINE_SIMD_UNARY(sqrt, std::sqrt(a[i]))
	UTIL_DEFINE_SIMD_BINARY(add, a[i] + b[i])
	UTIL_DEFINE_SIMD_BINARY(sub, a[i] - b[i])
	UTIL_DEFINE_SIMD_BINARY(mul, a[i] * b[i])
	UTIL_DEFINE_SIMD_BINARY(div, a[i] / b[i])
	UTIL_DEFINE_SIMD_BINARY(min, std::min(a[i], b[i]))
	UTIL_DEFINE_SIMD_BINARY(max, std::max(a[i], b[i]))
	UTIL_DEFINE_SIMD_TERNARY(fma, std::fma(a[i], b[i], c[i]))
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

	UTIL_DEFINE_SIMD_UNARY(permute0, a[(i ^ 1) & (N - 1)])
	UTIL_DEFINE_SIMD_UNARY(permute1, a[(i ^ 2) & (N - 1)])
	UTIL_DEFINE_SIMD_UNARY(permute2, a[(i ^ (N / 2)) & (N - 1)])

#undef UTIL_DEFINE_SIMD_UNARY
#undef UTIL_DEFINE_SIMD_BINARY
#undef UTIL_DEFINE_SIMD_TERNARY
#undef UTIL_DEFINE_SIMD_COMPARE

	static UTIL_SIMD_INLINE bool all_of(const mask &a) noexcept
	{
		for (int i = 0; i < N; ++i)
			if (!a[i])
				return false;
		return true;
	}

	static UTIL_SIMD_INLINE bool none_of(const mask &a) noexcept
	{
		for (int i = 0; i < N; ++i)
			if (a[i])
				return false;
		return true;
	}

	static UTIL_SIMD_INLINE vector blend(const vector &a, const vector &b, const mask &m)
	{
		vector r{};
		for (int i = 0; i < N; ++i)
			r[i] = m[i] ? b[i] : a[i];
		return r;
	}
};

} // namespace util
