#include "util/fft.h"

#ifdef UTIL_FFTW

#include <cassert>

namespace util {

unique_fftw_plan plan_fft(int n, util::complex<double> *in,
                          util::complex<double> *out, int sign, int flags)
{
	auto plan = fftw_plan_dft_1d(n, (fftw_complex *)in, (fftw_complex *)out,
	                             sign, flags);
	if (!plan)
		throw std::runtime_error("fftw_plan_dft_1d() failed");
	return unique_fftw_plan(plan);
}

unique_fftw_plan plan_fft_all(std::span<const int> const shape,
                              util::complex<double> *in,
                              util::complex<double> *out, int sign, int flags)
{
	auto plan = fftw_plan_dft(shape.size(), shape.data(), (fftw_complex *)in,
	                          (fftw_complex *)out, sign, flags);
	if (!plan)
		throw std::runtime_error("fftw_plan_dft() failed");
	return unique_fftw_plan(plan);
}

unique_fftw_plan plan_fft_last(std::span<const int> shape,
                               util::complex<double> *in,
                               util::complex<double> *out, int sign, int flags)
{
	assert(shape.size() >= 1);
	int howmany = 1; // number of independent FFTs
	for (size_t i = 0; i < shape.size() - 1; ++i)
		howmany *= shape[i];
	int dist = shape.back(); // distance from one FFT to the next
	int stride = 1;          // stride inside one FFT
	auto plan = fftw_plan_many_dft(
	    1, &shape.back(), howmany, (fftw_complex *)in, nullptr, stride, dist,
	    (fftw_complex *)out, nullptr, stride, dist, sign, flags);
	if (!plan)
		throw std::runtime_error("fftw_plan_many_dft() failed");
	return unique_fftw_plan(plan);
}

} // namespace util

#endif