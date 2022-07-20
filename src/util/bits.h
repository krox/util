#pragma once

// bit twiddling stuff. hopefully nicer than using __builtin_XXX directly

namespace util {

// wrap builtin bit functions into overload sets
#define UTIL_DEFINE_BIT_FUN(fun)                                               \
	inline int fun(int x) { return __builtin_##fun(x); }                       \
	inline int fun(unsigned int x) { return __builtin_##fun(x); }              \
	inline int fun(long x) { return __builtin_##fun##l(x); }                   \
	inline int fun(unsigned long x) { return __builtin_##fun##l(x); }          \
	inline int fun(long long x) { return __builtin_##fun##ll(x); }             \
	inline int fun(unsigned long long x) { return __builtin_##fun##ll(x); }
UTIL_DEFINE_BIT_FUN(ffs)      // 1 + index of least significant set bit
UTIL_DEFINE_BIT_FUN(clz)      // leading zeros, undefined on 0
UTIL_DEFINE_BIT_FUN(ctz)      // trailing zeros, undefined on 0
UTIL_DEFINE_BIT_FUN(popcount) // number of set bits
UTIL_DEFINE_BIT_FUN(parity)   // popcount mod 2
#undef UTIL_DEFINE_BIT_FUN

inline constexpr bool is_pow2(uint64_t x) { return x && (x & (x - 1)) == 0; }

inline constexpr uint64_t round_down_pow2(uint64_t x)
{
	return x ? uint64_t(1) << (63 - clz(x)) : 0;
}

inline constexpr uint64_t round_up_pow2(uint64_t x)
{
	return x == 1 ? 1 : 1 << (64 - clz(x - 1));
}

} // namespace util