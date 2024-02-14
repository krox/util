#include "util/fft.h"

#ifdef UTIL_FFTW

#include <cassert>

namespace util {

namespace {
template <class T> struct Stash
{
	T *ptr_;
	util::unique_memory<T> stash_;
	explicit Stash(T *ptr, size_t n) : ptr_(ptr), stash_(util::allocate<T>(n))
	{
		std::copy_n(ptr_, stash_.size(), stash_.data());
	}
	~Stash() { std::copy_n(stash_.data(), stash_.size(), ptr_); };
};
} // namespace

unique_fftw_plan plan_fft_1d(int n, util::complex<double> *in,
                             util::complex<double> *out, int sign, int flags)
{
	auto plan = fftw_plan_dft_1d(n, (fftw_complex *)in, (fftw_complex *)out,
	                             sign, flags | FFTW_WISDOM_ONLY);
	if (!plan)
	{
		auto stash = Stash(in, n);
		plan = fftw_plan_dft_1d(n, (fftw_complex *)in, (fftw_complex *)out,
		                        sign, flags);
	}
	if (!plan)
		throw std::runtime_error("fftw_plan_dft() failed");
	return unique_fftw_plan(plan);
}

unique_fftw_plan plan_fft_r2c_1d(int n, double *in, util::complex<double> *out,
                                 int flags)
{
	auto plan = fftw_plan_dft_r2c_1d(n, in, (fftw_complex *)out,
	                                 flags | FFTW_WISDOM_ONLY);
	if (!plan)
	{
		auto stash = Stash(in, n);
		plan = fftw_plan_dft_r2c_1d(n, in, (fftw_complex *)out, flags);
	}
	if (!plan)
		throw std::runtime_error("fftw_plan_dft_r2c() failed");
	return unique_fftw_plan(plan);
}

unique_fftw_plan plan_fft_c2r_1d(int n, util::complex<double> *in, double *out,
                                 int flags)
{
	auto plan = fftw_plan_dft_c2r_1d(n, (fftw_complex *)in, out,
	                                 flags | FFTW_WISDOM_ONLY);
	if (!plan)
	{
		auto stash = Stash(in, n / 2 + 1);
		plan = fftw_plan_dft_c2r_1d(n, (fftw_complex *)in, out, flags);
	}
	if (!plan)
		throw std::runtime_error("fftw_plan_dft_c2r() failed");
	return unique_fftw_plan(plan);
}

unique_fftw_plan plan_fft_all(std::span<const int> shape,
                              util::complex<double> *in,
                              util::complex<double> *out, int sign, int flags)
{
	auto plan =
	    fftw_plan_dft((int)shape.size(), shape.data(), (fftw_complex *)in,
	                  (fftw_complex *)out, sign, flags | FFTW_WISDOM_ONLY);
	if (!plan)
	{
		size_t n = std::accumulate(shape.begin(), shape.end(), size_t(1),
		                           std::multiplies<size_t>());
		auto stash = Stash(in, n);
		plan =
		    fftw_plan_dft((int)shape.size(), shape.data(), (fftw_complex *)in,
		                  (fftw_complex *)out, sign, flags);
	}
	if (!plan)
		throw std::runtime_error("fftw_plan_dft() failed");
	return unique_fftw_plan(plan);
}

void fft_1d(std::span<util::complex<double>> in,
            std::span<util::complex<double>> out, int sign, int flags)
{
	assert(in.size() == out.size());
	assert(in.size() <= INT_MAX);
	auto p = plan_fft_1d((int)in.size(), in.data(), out.data(), sign, flags);
	fftw_execute(p.get());
}
void fft_1d(std::span<util::complex<double>> inout, int sign, int flags)
{
	fft_1d(inout, inout, sign, flags);
}

void fft_r2c_1d(std::span<double> in, std::span<util::complex<double>> out,
                int flags)
{
	assert(in.size() / 2 + 1 == out.size());
	assert(in.size() <= INT_MAX);
	auto p = plan_fft_r2c_1d((int)in.size(), in.data(), out.data(), flags);
	fftw_execute(p.get());
}

void fft_c2r_1d(std::span<util::complex<double>> in, std::span<double> out,
                int flags)
{
	assert(in.size() == out.size() / 2 + 1);
	assert(out.size() <= INT_MAX);
	auto p = plan_fft_c2r_1d((int)out.size(), in.data(), out.data(), flags);
	fftw_execute(p.get());
}

void fft_all(std::span<const int> shape, std::span<util::complex<double>> in,
             std::span<util::complex<double>> out, int sign, int flags)
{
	size_t n = std::accumulate(shape.begin(), shape.end(), size_t(1),
	                           std::multiplies<size_t>());
	assert(in.size() == n);
	assert(out.size() == n);
	auto p = plan_fft_all(shape, in.data(), out.data(), sign, flags);
	fftw_execute(p.get());
}
void fft_all(std::span<const int> shape, std::span<util::complex<double>> inout,
             int sign, int flags)
{
	fft_all(shape, inout, inout, sign, flags);
}

void periodic_convolution_1d(std::span<double> a, std::span<double> b,
                             std::span<double> out, bool reverse_left)
{
	// TODO (maybe):
	//   1) using some real-to-real transform in FFTW, this can probably be made
	//      properly inplace
	//   2) the (common) case of a==b and reverse_left==true has an additional
	//      factor 2 of symmetry to be exploited.
	auto n = a.size();
	double scale = 1.0 / n;
	assert(b.size() == n);
	assert(out.size() == n);

	auto tmpa = util::aligned_allocate<util::complex<double>>(n / 2 + 1);
	fft_r2c_1d(a, tmpa);

	if (a.data() == b.data())
	{
		if (reverse_left)
			for (size_t i = 0; i < n / 2 + 1; ++i)
				tmpa[i] = util::complex<double>(norm2(tmpa[i]) * scale);
		else
			for (size_t i = 0; i < n / 2 + 1; ++i)
				tmpa[i] = tmpa[i] * tmpa[i] * scale;
	}
	else
	{
		auto tmpb = util::aligned_allocate<util::complex<double>>(n / 2 + 1);
		fft_r2c_1d(b, tmpb);
		if (reverse_left)
			for (size_t i = 0; i < n / 2 + 1; ++i)
				tmpa[i] = conj(tmpa[i]) * tmpb[i] * scale;
		else
			for (size_t i = 0; i < n / 2 + 1; ++i)
				tmpa[i] = tmpa[i] * tmpb[i] * scale;
	}
	fft_c2r_1d(tmpa, out);
}

} // namespace util

#endif