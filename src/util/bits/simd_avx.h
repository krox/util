#pragma once

#include <bit>
#include <tuple>
#include <x86intrin.h>

namespace util {

// helpers for bitmasks
#define UTIL_SELECT_2(a, b) ((a << 0) | (b << 1))
#define UTIL_SELECT_4(a, b, c, d) ((a << 0) | (b << 2) | (c << 4) | (d << 6))

struct sse_float
{
	// typedefs
	using scalar = float;
	using vector = __m128;
	using mask = __m128i;

	// 'constructors'
	static UTIL_SIMD_INLINE vector make(scalar a) noexcept { return _mm_set1_ps(a); }
	static UTIL_SIMD_INLINE vector make(scalar a, scalar b) noexcept { return _mm_setr_ps(a, b, a, b); }
	static UTIL_SIMD_INLINE vector make(scalar a, scalar b, scalar c, scalar d) noexcept { return _mm_setr_ps(a, b, c, d); }
	static UTIL_SIMD_INLINE mask make_mask(bool a) noexcept { return _mm_set1_epi32(a ? -1 : 0); }
	static UTIL_SIMD_INLINE mask make_mask(bool a, bool b) noexcept { return _mm_setr_epi32(a ? -1 : 0, b ? -1 : 0, a ? -1 : 0, b ? -1 : 0); }
	static UTIL_SIMD_INLINE mask make_mask(bool a, bool b, bool c, bool d) noexcept { return _mm_setr_epi32(a ? -1 : 0, b ? -1 : 0, c ? -1 : 0, d ? -1 : 0); }

	// insert/extract single elements
	static UTIL_SIMD_INLINE vector insert(vector a, int i, scalar b) noexcept
	{
		__m128i m = _mm_cmpeq_epi32(_mm_set1_epi32(i), _mm_setr_epi32(0, 1, 2, 3));
		return _mm_blendv_ps(a, _mm_set1_ps(b), _mm_castsi128_ps(m));
	}
	static UTIL_SIMD_INLINE scalar extract(vector a, int i) noexcept
	{
		return _mm_cvtss_f32(_mm_permutevar_ps(a, _mm_cvtsi32_si128(i)));
	}

	// arithmetic
	static UTIL_SIMD_INLINE vector sqrt(vector a) noexcept { return _mm_sqrt_ps(a); }
	static UTIL_SIMD_INLINE vector add(vector a, vector b) noexcept { return _mm_add_ps(a, b); }
	static UTIL_SIMD_INLINE vector sub(vector a, vector b) noexcept { return _mm_sub_ps(a, b); }
	static UTIL_SIMD_INLINE vector mul(vector a, vector b) noexcept { return _mm_mul_ps(a, b); }
	static UTIL_SIMD_INLINE vector div(vector a, vector b) noexcept { return _mm_div_ps(a, b); }
	static UTIL_SIMD_INLINE vector min(vector a, vector b) noexcept { return _mm_min_ps(a, b); }
	static UTIL_SIMD_INLINE vector max(vector a, vector b) noexcept { return _mm_max_ps(a, b); }
	static UTIL_SIMD_INLINE vector fma(vector a, vector b, vector c) noexcept { return _mm_fmadd_ps(a, b, c); }
	static UTIL_SIMD_INLINE vector fms(vector a, vector b, vector c) noexcept { return _mm_fmsub_ps(a, b, c); }

	// comparisons
	static UTIL_SIMD_INLINE mask cmplt(vector a, vector b) noexcept { return _mm_castps_si128(_mm_cmplt_ps(a, b)); }
	static UTIL_SIMD_INLINE mask cmple(vector a, vector b) noexcept { return _mm_castps_si128(_mm_cmple_ps(a, b)); }
	static UTIL_SIMD_INLINE mask cmpeq(vector a, vector b) noexcept { return _mm_castps_si128(_mm_cmpeq_ps(a, b)); }
	static UTIL_SIMD_INLINE mask cmpge(vector a, vector b) noexcept { return _mm_castps_si128(_mm_cmpge_ps(a, b)); }
	static UTIL_SIMD_INLINE mask cmpgt(vector a, vector b) noexcept { return _mm_castps_si128(_mm_cmpgt_ps(a, b)); }
	static UTIL_SIMD_INLINE mask cmpnlt(vector a, vector b) noexcept { return _mm_castps_si128(_mm_cmpnlt_ps(a, b)); }
	static UTIL_SIMD_INLINE mask cmpnle(vector a, vector b) noexcept { return _mm_castps_si128(_mm_cmpnle_ps(a, b)); }
	static UTIL_SIMD_INLINE mask cmpneq(vector a, vector b) noexcept { return _mm_castps_si128(_mm_cmpneq_ps(a, b)); }
	static UTIL_SIMD_INLINE mask cmpnge(vector a, vector b) noexcept { return _mm_castps_si128(_mm_cmpnge_ps(a, b)); }
	static UTIL_SIMD_INLINE mask cmpngt(vector a, vector b) noexcept { return _mm_castps_si128(_mm_cmpngt_ps(a, b)); }

	// mask logic
	static UTIL_SIMD_INLINE bool all_of(mask a) noexcept { return _mm_testc_si128(a, _mm_set1_epi32(-1)); }
	static UTIL_SIMD_INLINE bool none_of(mask a) noexcept { return _mm_testz_si128(a, a); }

	// shuffles
	static UTIL_SIMD_INLINE vector permute0(vector a) noexcept { return _mm_shuffle_ps(a, a, UTIL_SELECT_4(1, 0, 3, 2)); }
	static UTIL_SIMD_INLINE vector permute1(vector a) noexcept { return _mm_shuffle_ps(a, a, UTIL_SELECT_4(2, 3, 0, 1)); }

	// m ? b : a
	static UTIL_SIMD_INLINE vector blend(vector a, vector b, mask m) { return _mm_blendv_ps(a, b, _mm_castsi128_ps(m)); }
};

struct sse_double
{
	// typedefs
	using scalar = double;
	using vector = __m128d;
	using mask = __m128i;

	// 'constructors'
	static UTIL_SIMD_INLINE vector make(scalar a) noexcept { return _mm_set1_pd(a); }
	static UTIL_SIMD_INLINE vector make(scalar a, scalar b) noexcept { return _mm_setr_pd(a, b); }
	static UTIL_SIMD_INLINE mask make_mask(bool a) noexcept { return _mm_set1_epi64x(a ? -1 : 0); }
	static UTIL_SIMD_INLINE mask make_mask(bool a, bool b) noexcept { return _mm_set_epi64x(b ? -1 : 0, a ? -1 : 0); }

	// insert/extract single elements
	static UTIL_SIMD_INLINE vector insert(vector a, int i, scalar b) noexcept
	{
		__m128i m = _mm_cmpeq_epi64(_mm_set1_epi64x(i), _mm_set_epi64x(1, 0));
		return _mm_blendv_pd(a, _mm_set1_pd(b), _mm_castsi128_pd(m));
	}
	static UTIL_SIMD_INLINE scalar extract(vector a, int i) noexcept
	{
		// _mm_permutevar_pd is controlled by bits 1 and 65 and not 0 and 64 as I would have assumed. really weird (to me). thats why there is the '*2'
		return _mm_cvtsd_f64(_mm_permutevar_pd(a, _mm_cvtsi32_si128(i * 2)));
	}

	// arithmetic
	static UTIL_SIMD_INLINE vector sqrt(vector a) noexcept { return _mm_sqrt_pd(a); }
	static UTIL_SIMD_INLINE vector add(vector a, vector b) noexcept { return _mm_add_pd(a, b); }
	static UTIL_SIMD_INLINE vector sub(vector a, vector b) noexcept { return _mm_sub_pd(a, b); }
	static UTIL_SIMD_INLINE vector mul(vector a, vector b) noexcept { return _mm_mul_pd(a, b); }
	static UTIL_SIMD_INLINE vector div(vector a, vector b) noexcept { return _mm_div_pd(a, b); }
	static UTIL_SIMD_INLINE vector min(vector a, vector b) noexcept { return _mm_min_pd(a, b); }
	static UTIL_SIMD_INLINE vector max(vector a, vector b) noexcept { return _mm_max_pd(a, b); }
	static UTIL_SIMD_INLINE vector fma(vector a, vector b, vector c) noexcept { return _mm_fmadd_pd(a, b, c); }
	static UTIL_SIMD_INLINE vector fms(vector a, vector b, vector c) noexcept { return _mm_fmsub_pd(a, b, c); }

	// comparisons
	static UTIL_SIMD_INLINE mask cmplt(vector a, vector b) noexcept { return _mm_castpd_si128(_mm_cmplt_pd(a, b)); }
	static UTIL_SIMD_INLINE mask cmple(vector a, vector b) noexcept { return _mm_castpd_si128(_mm_cmple_pd(a, b)); }
	static UTIL_SIMD_INLINE mask cmpeq(vector a, vector b) noexcept { return _mm_castpd_si128(_mm_cmpeq_pd(a, b)); }
	static UTIL_SIMD_INLINE mask cmpge(vector a, vector b) noexcept { return _mm_castpd_si128(_mm_cmpge_pd(a, b)); }
	static UTIL_SIMD_INLINE mask cmpgt(vector a, vector b) noexcept { return _mm_castpd_si128(_mm_cmpgt_pd(a, b)); }
	static UTIL_SIMD_INLINE mask cmpnlt(vector a, vector b) noexcept { return _mm_castpd_si128(_mm_cmpnlt_pd(a, b)); }
	static UTIL_SIMD_INLINE mask cmpnle(vector a, vector b) noexcept { return _mm_castpd_si128(_mm_cmpnle_pd(a, b)); }
	static UTIL_SIMD_INLINE mask cmpneq(vector a, vector b) noexcept { return _mm_castpd_si128(_mm_cmpneq_pd(a, b)); }
	static UTIL_SIMD_INLINE mask cmpnge(vector a, vector b) noexcept { return _mm_castpd_si128(_mm_cmpnge_pd(a, b)); }
	static UTIL_SIMD_INLINE mask cmpngt(vector a, vector b) noexcept { return _mm_castpd_si128(_mm_cmpngt_pd(a, b)); }

	// mask logic
	static UTIL_SIMD_INLINE bool all_of(mask a) noexcept { return _mm_testc_si128(a, _mm_set1_epi64x(-1)); }
	static UTIL_SIMD_INLINE bool none_of(mask a) noexcept { return _mm_testz_si128(a, a); }

	// shuffles
	static UTIL_SIMD_INLINE vector permute0(vector a) noexcept { return _mm_shuffle_pd(a, a, UTIL_SELECT_2(1, 0)); }

	// m ? b : a
	static UTIL_SIMD_INLINE vector blend(vector a, vector b, mask m) { return _mm_blendv_pd(a, b, _mm_castsi128_pd(m)); }
};

struct avx_float
{
	// typedefs
	using scalar = float;
	using vector = __m256;
	using mask = __m256i;

	// 'constructors'
	static UTIL_SIMD_INLINE vector make(scalar a) noexcept { return _mm256_set1_ps(a); }
	static UTIL_SIMD_INLINE vector make(scalar a, scalar b) noexcept { return _mm256_set_ps(b, a, b, a, b, a, b, a); }
	static UTIL_SIMD_INLINE vector make(scalar a, scalar b, scalar c, scalar d) noexcept { return _mm256_set_ps(d, c, b, a, d, c, b, a); }
	static UTIL_SIMD_INLINE vector make(scalar a, scalar b, scalar c, scalar d, scalar e, scalar f, scalar g, scalar h) noexcept { return _mm256_set_ps(h, g, f, e, d, c, b, a); }
	static UTIL_SIMD_INLINE mask make_mask(bool a) noexcept { return _mm256_set1_epi32(a ? -1 : 0); }
	static UTIL_SIMD_INLINE mask make_mask(bool a, bool b) noexcept
	{
		int32_t aa = a ? -1 : 0;
		int32_t bb = b ? -1 : 0;
		return _mm256_set_epi32(bb, aa, bb, aa, bb, aa, bb, aa);
	}
	static UTIL_SIMD_INLINE mask make_mask(bool a, bool b, bool c, bool d) noexcept
	{
		int32_t aa = a ? -1 : 0;
		int32_t bb = b ? -1 : 0;
		int32_t cc = c ? -1 : 0;
		int32_t dd = d ? -1 : 0;
		return _mm256_set_epi32(dd, cc, bb, aa, dd, cc, bb, aa);
	}

	// insert/extract single elements
	static UTIL_SIMD_INLINE vector insert(vector a, int i, scalar b) noexcept
	{
		__m256i m = _mm256_cmpeq_epi32(_mm256_set1_epi32(i), _mm256_set_epi32(7, 6, 5, 4, 3, 2, 1, 0));
		return _mm256_blendv_ps(a, _mm256_set1_ps(b), _mm256_castsi256_ps(m));
	}
	static UTIL_SIMD_INLINE scalar extract(vector a, int i) noexcept
	{
		return a[i];
	}

	// arithmetic
	static UTIL_SIMD_INLINE vector sqrt(vector a) noexcept { return _mm256_sqrt_ps(a); }
	static UTIL_SIMD_INLINE vector add(vector a, vector b) noexcept { return _mm256_add_ps(a, b); }
	static UTIL_SIMD_INLINE vector sub(vector a, vector b) noexcept { return _mm256_sub_ps(a, b); }
	static UTIL_SIMD_INLINE vector mul(vector a, vector b) noexcept { return _mm256_mul_ps(a, b); }
	static UTIL_SIMD_INLINE vector div(vector a, vector b) noexcept { return _mm256_div_ps(a, b); }
	static UTIL_SIMD_INLINE vector min(vector a, vector b) noexcept { return _mm256_min_ps(a, b); }
	static UTIL_SIMD_INLINE vector max(vector a, vector b) noexcept { return _mm256_max_ps(a, b); }
	static UTIL_SIMD_INLINE vector fma(vector a, vector b, vector c) noexcept { return _mm256_fmadd_ps(a, b, c); }
	static UTIL_SIMD_INLINE vector fms(vector a, vector b, vector c) noexcept { return _mm256_fmsub_ps(a, b, c); }

	// comparisons
	static UTIL_SIMD_INLINE mask cmplt(vector a, vector b) noexcept { return _mm256_castps_si256(_mm256_cmp_ps(a, b, _CMP_LT_OQ)); }
	static UTIL_SIMD_INLINE mask cmple(vector a, vector b) noexcept { return _mm256_castps_si256(_mm256_cmp_ps(a, b, _CMP_LE_OQ)); }
	static UTIL_SIMD_INLINE mask cmpeq(vector a, vector b) noexcept { return _mm256_castps_si256(_mm256_cmp_ps(a, b, _CMP_EQ_OQ)); }
	static UTIL_SIMD_INLINE mask cmpge(vector a, vector b) noexcept { return _mm256_castps_si256(_mm256_cmp_ps(a, b, _CMP_GE_OQ)); }
	static UTIL_SIMD_INLINE mask cmpgt(vector a, vector b) noexcept { return _mm256_castps_si256(_mm256_cmp_ps(a, b, _CMP_GT_OQ)); }
	static UTIL_SIMD_INLINE mask cmpnlt(vector a, vector b) noexcept { return _mm256_castps_si256(_mm256_cmp_ps(a, b, _CMP_NLT_UQ)); }
	static UTIL_SIMD_INLINE mask cmpnle(vector a, vector b) noexcept { return _mm256_castps_si256(_mm256_cmp_ps(a, b, _CMP_NLE_UQ)); }
	static UTIL_SIMD_INLINE mask cmpneq(vector a, vector b) noexcept { return _mm256_castps_si256(_mm256_cmp_ps(a, b, _CMP_NEQ_UQ)); }
	static UTIL_SIMD_INLINE mask cmpnge(vector a, vector b) noexcept { return _mm256_castps_si256(_mm256_cmp_ps(a, b, _CMP_NGE_UQ)); }
	static UTIL_SIMD_INLINE mask cmpngt(vector a, vector b) noexcept { return _mm256_castps_si256(_mm256_cmp_ps(a, b, _CMP_NGT_UQ)); }

	// mask logic
	static UTIL_SIMD_INLINE bool all_of(mask a) noexcept { return _mm256_testc_si256(a, _mm256_set1_epi32(-1)); }
	static UTIL_SIMD_INLINE bool none_of(mask a) noexcept { return _mm256_testz_si256(a, a); }

	// shuffles
	static UTIL_SIMD_INLINE vector permute0(vector a) noexcept { return _mm256_shuffle_ps(a, a, UTIL_SELECT_4(1, 0, 3, 2)); }
	static UTIL_SIMD_INLINE vector permute1(vector a) noexcept { return _mm256_shuffle_ps(a, a, UTIL_SELECT_4(2, 3, 0, 1)); }

	// m ? b : a
	static UTIL_SIMD_INLINE vector blend(vector a, vector b, mask m) { return _mm256_blendv_ps(a, b, _mm256_castsi256_ps(m)); }
};

struct avx_double
{
	// typedefs
	using scalar = double;
	using vector = __m256d;
	using mask = __m256i;

	// 'constructors'
	static UTIL_SIMD_INLINE vector make(scalar a) noexcept { return _mm256_set1_pd(a); }
	static UTIL_SIMD_INLINE vector make(scalar a, scalar b) noexcept { return _mm256_set_pd(b, a, b, a); }
	static UTIL_SIMD_INLINE vector make(scalar a, scalar b, scalar c, scalar d) noexcept { return _mm256_set_pd(d, c, b, a); }
	static UTIL_SIMD_INLINE mask make_mask(bool a) noexcept { return _mm256_set1_epi64x(a ? -1 : 0); }
	static UTIL_SIMD_INLINE mask make_mask(bool a, bool b) noexcept
	{
		int64_t aa = a ? -1 : 0;
		int64_t bb = b ? -1 : 0;
		return _mm256_set_epi64x(bb, aa, bb, aa);
	}
	static UTIL_SIMD_INLINE mask make_mask(bool a, bool b, bool c, bool d) noexcept
	{
		int64_t aa = a ? -1 : 0;
		int64_t bb = b ? -1 : 0;
		int64_t cc = c ? -1 : 0;
		int64_t dd = d ? -1 : 0;
		return _mm256_set_epi64x(dd, cc, bb, aa);
	}

	// insert/extract single elements
	static UTIL_SIMD_INLINE vector insert(vector a, int i, scalar b) noexcept
	{
		__m256i m = _mm256_cmpeq_epi64(_mm256_set1_epi64x(i), _mm256_set_epi64x(3, 2, 1, 0));
		return _mm256_blendv_pd(a, _mm256_set1_pd(b), _mm256_castsi256_pd(m));
	}
	static UTIL_SIMD_INLINE scalar extract(vector a, int i) noexcept
	{
		return a[i];
	}

	// arithmetic
	static UTIL_SIMD_INLINE vector sqrt(vector a) noexcept { return _mm256_sqrt_pd(a); }
	static UTIL_SIMD_INLINE vector add(vector a, vector b) noexcept { return _mm256_add_pd(a, b); }
	static UTIL_SIMD_INLINE vector sub(vector a, vector b) noexcept { return _mm256_sub_pd(a, b); }
	static UTIL_SIMD_INLINE vector mul(vector a, vector b) noexcept { return _mm256_mul_pd(a, b); }
	static UTIL_SIMD_INLINE vector div(vector a, vector b) noexcept { return _mm256_div_pd(a, b); }
	static UTIL_SIMD_INLINE vector min(vector a, vector b) noexcept { return _mm256_min_pd(a, b); }
	static UTIL_SIMD_INLINE vector max(vector a, vector b) noexcept { return _mm256_max_pd(a, b); }
	static UTIL_SIMD_INLINE vector fma(vector a, vector b, vector c) noexcept { return _mm256_fmadd_pd(a, b, c); }
	static UTIL_SIMD_INLINE vector fms(vector a, vector b, vector c) noexcept { return _mm256_fmsub_pd(a, b, c); }

	// comparisons
	static UTIL_SIMD_INLINE mask cmplt(vector a, vector b) noexcept { return _mm256_castpd_si256(_mm256_cmp_pd(a, b, _CMP_LT_OQ)); }
	static UTIL_SIMD_INLINE mask cmple(vector a, vector b) noexcept { return _mm256_castpd_si256(_mm256_cmp_pd(a, b, _CMP_LE_OQ)); }
	static UTIL_SIMD_INLINE mask cmpeq(vector a, vector b) noexcept { return _mm256_castpd_si256(_mm256_cmp_pd(a, b, _CMP_EQ_OQ)); }
	static UTIL_SIMD_INLINE mask cmpge(vector a, vector b) noexcept { return _mm256_castpd_si256(_mm256_cmp_pd(a, b, _CMP_GE_OQ)); }
	static UTIL_SIMD_INLINE mask cmpgt(vector a, vector b) noexcept { return _mm256_castpd_si256(_mm256_cmp_pd(a, b, _CMP_GT_OQ)); }
	static UTIL_SIMD_INLINE mask cmpnlt(vector a, vector b) noexcept { return _mm256_castpd_si256(_mm256_cmp_pd(a, b, _CMP_NLT_UQ)); }
	static UTIL_SIMD_INLINE mask cmpnle(vector a, vector b) noexcept { return _mm256_castpd_si256(_mm256_cmp_pd(a, b, _CMP_NLE_UQ)); }
	static UTIL_SIMD_INLINE mask cmpneq(vector a, vector b) noexcept { return _mm256_castpd_si256(_mm256_cmp_pd(a, b, _CMP_NEQ_UQ)); }
	static UTIL_SIMD_INLINE mask cmpnge(vector a, vector b) noexcept { return _mm256_castpd_si256(_mm256_cmp_pd(a, b, _CMP_NGE_UQ)); }
	static UTIL_SIMD_INLINE mask cmpngt(vector a, vector b) noexcept { return _mm256_castpd_si256(_mm256_cmp_pd(a, b, _CMP_NGT_UQ)); }

	// mask logic
	static UTIL_SIMD_INLINE bool all_of(mask a) noexcept { return _mm256_testc_si256(a, _mm256_set1_epi64x(-1)); }
	static UTIL_SIMD_INLINE bool none_of(mask a) noexcept { return _mm256_testz_si256(a, a); }

	// shuffles
	static UTIL_SIMD_INLINE vector permute0(vector a) noexcept { return _mm256_shuffle_pd(a, a, 5); }
	static UTIL_SIMD_INLINE vector permute1(vector a) noexcept { return _mm256_permute2f128_pd(a, a, 1); }

	// m ? b : a
	static UTIL_SIMD_INLINE vector blend(vector a, vector b, mask m) { return _mm256_blendv_pd(a, b, _mm256_castsi256_pd(m)); }
};

#undef UTIL_SELECT_2
#undef UTIL_SELECT_4

} // namespace util
