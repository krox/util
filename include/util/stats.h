#pragma once

// statistics utilities

#include "util/span.h"
#include <array>
#include <cmath>
#include <vector>

namespace util {

// simple population statistics

// TODO: do something like pair-summation for mean/variance to further improve
//       numerical stability (and possibly get some vectorization on the way?)

// mean of f(x_i)
template <typename F> double mean(gspan<const double> xs, F f)
{
	double sum = 0;
	for (auto x : xs)
		sum += f(x);
	return sum / xs.size();
}

// variance of f(x_i)
template <typename F> double variance(gspan<const double> xs, F f)
{
	// there are surprisingly many ways to compute the variance:
	//    * <(x-<x>)^2>:
	//      numerically stable, but requires two passes over the data
	//    * <x^2> - <x>^2:
	//      instable, but only requires one pass
	//    * "Welford's online algorithm":
	//      stable, only one pass but somewhat inefficient (division in loop)
	//    * approximate mean and then run Welford on shifted data:
	//      slowest version, but numerically essentially perfect. Would be
	//      cool to encounter an actual usecase.
	//
	// Here, we choose Welford's algorithm, because we want to guarantee that
	// f is only evaluated once for every element, without allocating any
	// temporary memory. Other than that, stability is more important than
	// performance.
	double mean = 0, sum2 = 0;
	for (size_t i = 0; i < xs.size(); ++i)
	{
		double fx = f(xs[i]);
		double dx = fx - mean;
		mean += dx / (i + 1);
		sum2 += dx * (fx - mean);
	}
	return sum2 / xs.size();
}

// mean of x_i
inline double mean(gspan<const double> xs)
{
	return mean(xs, [](double x) { return x; });
}

// variance of x_i
inline double variance(gspan<const double> xs)
{
	return variance(xs, [](double x) { return x; });
}

// mean of |x_i|
inline double mean_abs(gspan<const double> xs)
{
	return mean(xs, [](double x) { return std::abs(x); });
}

// variance of |x_i|
inline double variance_abs(gspan<const double> xs)
{
	return variance(xs, [](double x) { return std::abs(x); });
}

double covariance(gspan<const double> xs, gspan<const double> ys);
double correlation(gspan<const double> xs, gspan<const double> ys);

inline double min(gspan<const double> xs)
{
	double r = std::numeric_limits<double>::infinity();
	for (auto x : xs)
		r = std::min(r, x);
	return r;
}

inline double max(gspan<const double> xs)
{
	double r = -std::numeric_limits<double>::infinity();
	for (auto x : xs)
		r = std::max(r, x);
	return r;
}

/** fit constant function f(x) = a */
struct ConstantFit
{
	/** constructors */
	ConstantFit() = default;
	ConstantFit(std::span<const double> ys);
	ConstantFit(std::span<const double> ys, std::span<const double> ys_err);

	/** fit result */
	double a = 0.0 / 0.0;
	double a_err = 0.0 / 0.0;

	/** evaluate the fitted function */
	double operator()() const;
	double operator()(double x) const;
};

/** fit linear function f(x) = a + b*x */
struct LinearFit
{
	/** constructors */
	LinearFit() = default;
	LinearFit(std::span<const double> xs, std::span<const double> ys);
	LinearFit(std::span<const double> xs, std::span<const double> ys,
	          std::span<const double> err);

	/** fit result */
	double a = 0.0 / 0.0;
	double b = 0.0 / 0.0;

	/** evaluate the fitted function */
	double operator()(double x) const;
};

// fit exponential function f(x) = a * exp(b*x)
//   * implemented as linear fit to log(y).
//     (Fast and simple but not super accurate)
//   * values that are negative within 2*err are ignored
struct ExponentialFit
{
	ExponentialFit() = default;
	ExponentialFit(std::span<const double> xs, std::span<const double> ys,
	               std::span<const double> err);

	double a = 0.0 / 0.0;
	double b = 0.0 / 0.0;

	double operator()(double x) const;
};

struct Histogram
{
	std::vector<double> mins;
	std::vector<double> maxs;
	std::vector<size_t> bins;
	size_t total = 0;
	size_t ignored = 0;

	double min() const { return mins.front(); }
	double max() const { return maxs.back(); }
	size_t binCount() const { return bins.size(); }

	void init(double min, double max, size_t n);
	Histogram(double min, double max, size_t n);
	Histogram(std::span<const double> xs, size_t n);

	void add(double x);
};

class IntHistogram
{
	std::vector<int64_t> bins_;
	int max_ = INT_MIN;
	int64_t count_ = 0;
	double sum_ = 0;

  public:
	IntHistogram() = default;
	IntHistogram(std::span<const int> xs) { add(xs); }

	void add(int x, int64_t weight = 1);
	void add(std::span<const int> xs);

	int max() const { return max_; }

	int64_t bin(int i) const
	{
		assert(i >= 0);
		if (i >= (int)bins_.size())
			return 0;
		return bins_[i];
	}
	double sum() const { return sum_; }
	double mean() const { return (double)sum_ / count_; }
};

/**
 * Estimate mean/variance/covariance of a population as samples are coming in.
 * This is the same as the standard formula "Var(x) = n/(n-1) (E(x^2) - E(x)^2)"
 * but numerically more stable.
 */
template <size_t dim> class Estimator
{
	double n = 0;
	double avg[dim];       // = 1/n ∑ x_i
	double sum2[dim][dim]; //= ∑ (x_i - meanX)*(y_i - meanY)

  public:
	/** default constructor */
	Estimator();

	/** add a new data point */
	void add(std::array<double, dim> x);
	void add(std::array<double, dim> x, double w);

	/** mean/variance in dimension i */
	double mean(size_t i = 0) const;
	double var(size_t i = 0) const;

	/** covariance/correlation between dimensions i and j */
	double cov(size_t i = 0, size_t j = 1) const;
	double corr(size_t i = 0, size_t j = 1) const;

	/** reset everything */
	void clear();
};

/** special case of 1D estimator that also tracks skewness and kurtosis */
template <> class Estimator<1>
{
	double n = 0;
	double m = 0;                  // mean
	double m2 = 0, m3 = 0, m4 = 0; // sum of (x_i - m)^k

  public:
	Estimator() = default;

	void add(double x)
	{
		n += 1;

		auto d = x - m;
		auto d_n = d / n;
		auto d_n2 = d_n * d_n;
		auto tmp = d_n * d * (n - 1);

		m += d_n;
		m4 += d_n2 * tmp * (n * n - 3 * n + 3) + 6 * d_n2 * m2 - 4 * d_n * m3;
		m3 += d_n * tmp * (n - 2) - 3 * d_n * m2;
		m2 += tmp;
	}

	// TODO: not really sure about the bias-correction factors of skew/kurtosis

	double mean() const { return m; }

	double variance() const { return m2 / (n - 1); }

	double skewness() const { return m3 / n / std::pow(variance(), 1.5); }

	double kurtosis() const
	{
		double k4 = n * (n + 1) / (n - 1) * m4 / (m2 * m2) - 3;
		k4 *= (n - 1) * (n - 1) / ((n - 2) * (n - 3));
		return k4;
	}

	// reset everything
	void clear() { n = m = m2 = m3 = m4 = 0; }
};

// records a time-series, automatically increasing the bin size to keep the
// number of stored samples bounded.
class BinnedSeries
{
	std::vector<double> samples_;
	size_t binsize_ = 1;
	std::vector<double> buffer_;
	Estimator<1> est_;

  public:
	BinnedSeries() = default;

	void add(double x);

	double mean() const { return est_.mean(); }
	double mean_error() const
	{
		return std::sqrt(est_.variance() / (samples_.size() - 1.5));
	}
};

/** autocorrelation coefficients */
std::vector<double> autocorrelation(std::span<const double> xs, size_t m);

/** estimate auto-correlation time */
double correlationTime(std::span<const double> xs);

// format a number with error, e.g. "1.23(45)"
std::string format_error(double val, double err);

} // namespace util
