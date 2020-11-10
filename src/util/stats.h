#ifndef UTIL_STATS_H
#define UTIL_STATS_H

/** statistics utilities */

#include <array>
#include <vector>

#include "util/span.h"

namespace util {

/** simple population mean/variance */
double mean(gspan<const double> xs);
double variance(gspan<const double> xs);
double covariance(gspan<const double> xs, gspan<const double> ys);
double correlation(gspan<const double> xs, gspan<const double> ys);

double mean_abs(gspan<const double> xs);
double variance_abs(gspan<const double> xs);

/** fit constant function f(x) = a */
struct ConstantFit
{
	/** constructors */
	ConstantFit() = default;
	ConstantFit(span<const double> ys);
	ConstantFit(span<const double> ys, span<const double> ys_err);

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
	LinearFit(span<const double> xs, span<const double> ys);
	LinearFit(span<const double> xs, span<const double> ys,
	          span<const double> err);

	/** fit result */
	double a = 0.0 / 0.0;
	double b = 0.0 / 0.0;

	/** evaluate the fitted function */
	double operator()(double x) const;
};

struct Histogram
{
	std::vector<double> mins;
	std::vector<double> maxs;
	std::vector<size_t> bins;

	void init(double min, double max, size_t n);
	Histogram(double min, double max, size_t n);
	Histogram(span<const double> xs, size_t n);

	void add(double x);
};

class IntHistogram
{
	int min_ = 0, max_ = 0;
	std::vector<size_t> bins_;
	size_t underflow_ = 0, overflow_ = 0;
	size_t count_ = 0;
	double sum_ = 0.0;

  public:
	IntHistogram() = default;
	IntHistogram(int min, int max) : min_(min), max_(max), bins_(max - min, 0)
	{}

	void add(int x)
	{
		if (x < min_)
			underflow_ += 1;
		else if (x >= max_)
			overflow_ += 1;
		else
			bins_[x - min_] += 1;
		count_ += 1;
		sum_ += x;
	}

	int min() const { return min_; }
	int max() const { return max_; }

	size_t overflow() const { return overflow_; }
	size_t underflow() const { return underflow_; }
	size_t count() const { return count_; }
	size_t count(int x) const
	{
		assert(min_ <= x && x < max_);
		return bins_[x - min_];
	}
	double sum() const { return sum_; }
	double mean() const { return (double)sum_ / count_; }

	void clear()
	{
		for (auto &c : bins_)
			c = 0;
		underflow_ = 0;
		overflow_ = 0;
		count_ = 0;
		sum_ = 0;
	}
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

/** autocorrelation coefficients */
std::vector<double> autocorrelation(span<const double> xs, size_t m);

/** estimate auto-correlation time */
double correlationTime(span<const double> xs);

} // namespace util

#endif
