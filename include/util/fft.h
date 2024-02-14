#pragma once

// simple wrapper around the FFTW3 library
// general notes on FFTW:
//     * For best performance, arrays should be highly aligned
//       (e.g., using util::aligned_allocate<util::complex<double>>(...)).
//     * Performance is best for sizes that are products of small primes,
//       especially pure powers of 2. Though FFTW uses O(n log n) algorithms
//       even for large prime sizes.
//     * 'sign' is the sign in the 'exp(...)' part of the transform. The
//       constants FFTW_FORWARD = -1 and FFTW_BACKWARD = 1 are just convention.
//     * By default, some types of transform destroy the input array. This can
//       be controlled using FFTW_{PRESERVE,DESTROY}_INPUT.
//     * FFTW computes 'unnormalized' transforms, i.e., a forward transform
//       followed by a backward transform will multiply the input by n.
#ifdef UTIL_FFTW

#include "util/complex.h"
#include "util/memory.h"
#include <algorithm>
#include <fftw3.h>
#include <numeric>
#include <span>
#include <vector>

namespace util {

// 'fftw_plan' is indeed a pointer (though the pointed-to type is unnamed),
// so we can use std::unique_ptr for a nice RAII wrapper
static_assert(std::is_pointer_v<fftw_plan>);
struct fftw_plan_delete
{
	void operator()(fftw_plan p) { fftw_destroy_plan(p); }
};
using unique_fftw_plan =
    std::unique_ptr<std::remove_pointer_t<fftw_plan>, fftw_plan_delete>;

// basic wrappers around fftw_plan_*
//     * throws on errors instad of returning nullptr
//     * In contrast to the raw FFTW routines, these planning functions do not
//       destroy the input array (more precisely, they restore it after
//       planning).The output array is not saved though.

// 1d complex-to-complex
unique_fftw_plan plan_fft_1d(int n, util::complex<double> *in,
                             util::complex<double> *out, int sign,
                             int flags = FFTW_MEASURE);

// 1d complex-to-real-to-complex. the real array should be of size n/2+1
unique_fftw_plan plan_fft_r2c_1d(int n, double *in, util::complex<double> *out,
                                 int flags = FFTW_MEASURE);
unique_fftw_plan plan_fft_c2r_1d(int n, util::complex<double> *in, double *out,
                                 int flags = FFTW_MEASURE);

// transforms all axes of multi-dimensional array (assumed to be row-major)
unique_fftw_plan plan_fft_all(std::span<const int> shape,
                              util::complex<double> *in,
                              util::complex<double> *out, int sign,
                              int flags = FFTW_MEASURE);

// higher-level interface for convenience that hides FFTW's 'planning' stage
//     * thanks to FFTW's internal wisdom collection, planning should only be
//       expensive at most once for any given shape/type/flags/alignment
//       combination, but the overhead might still be a bit higher than creating
//       and (re-)using a plan explicitly.

void fft_1d(std::span<util::complex<double>> in,
            std::span<util::complex<double>> out, int sign,
            int flags = FFTW_MEASURE);
void fft_1d(std::span<util::complex<double>> inout, int sign,
            int flags = FFTW_MEASURE);
void fft_r2c_1d(std::span<double> in, std::span<util::complex<double>> out,
                int flags = FFTW_MEASURE);
void fft_c2r_1d(std::span<util::complex<double>> in, std::span<double> out,
                int flags = FFTW_MEASURE);
void fft_all(std::span<const int> shape, std::span<util::complex<double>> in,
             std::span<util::complex<double>> out, int sign,
             int flags = FFTW_MEASURE);
void fft_all(std::span<const int> shape, std::span<util::complex<double>> inout,
             int sign, int flags = FFTW_MEASURE);

} // namespace util

#endif
