#include "util/stats.h"

#include "util/error.h"
#include <algorithm>
#include <cmath>
#include <fmt/format.h>

namespace util {

double covariance(gspan<const double> xs, gspan<const double> ys)
{
	assert(xs.size() == ys.size());
	double mx = mean(xs);
	double my = mean(ys);
	double sum = 0;
	for (size_t i = 0; i < xs.size(); ++i)
		sum += (xs[i] - mx) * (ys[i] - my);
	return sum / xs.size();
}

double correlation(gspan<const double> xs, gspan<const double> ys)
{
	return covariance(xs, ys) / std::sqrt(variance(xs) * variance(ys));
}

ConstantFit::ConstantFit(std::span<const double> ys)
{
	a = 0;
	for (double y : ys)
		a += y;
	a /= ys.size();
	a_err = 0.0 / 0.0;
}

ConstantFit::ConstantFit(std::span<const double> ys,
                         std::span<const double> ys_err)
{
	assert(ys.size() == ys_err.size());
	a = 0;
	a_err = 0;
	for (size_t i = 0; i < ys.size(); ++i)
	{
		a += ys[i] / (ys_err[i] * ys_err[i]);
		a_err += 1 / (ys_err[i] * ys_err[i]);
	}
	a /= a_err;
	a_err = 1 / sqrt(a_err);
}

double ConstantFit::operator()() const { return a; }
double ConstantFit::operator()([[maybe_unused]] double x) const { return a; }

LinearFit::LinearFit(std::span<const double> xs, std::span<const double> ys)
{
	assert(xs.size() == ys.size());
	Estimator<2> est;
	for (size_t i = 0; i < xs.size(); ++i)
		est.add({xs[i], ys[i]});
	b = est.cov(0, 1) / est.var(0);
	a = est.mean(1) - est.mean(0) * b;
}

LinearFit::LinearFit(std::span<const double> xs, std::span<const double> ys,
                     std::span<const double> err)
{
	assert(xs.size() == ys.size());
	Estimator<2> est;
	for (size_t i = 0; i < xs.size(); ++i)
		est.add({xs[i], ys[i]}, 1.0 / (err[i] * err[i]));
	b = est.cov(0, 1) / est.var(0);
	a = est.mean(1) - est.mean(0) * b;
}

double LinearFit::operator()(double x) const { return a + b * x; }

ExponentialFit::ExponentialFit(std::span<const double> xs,
                               std::span<const double> ys,
                               std::span<const double> es)
{
	assert(ys.size() == es.size());
	assert(xs.empty() || xs.size() == ys.size());

	Estimator<2> est;
	for (size_t i = 0; i < ys.size(); ++i)
	{
		if (ys[i] < 2 * es[i])
			continue;
		if (xs.empty())
			est.add({double(i), std::log(ys[i])},
			        ys[i] * ys[i] / (es[i] * es[i]));
		else
			est.add({xs[i], std::log(ys[i])}, ys[i] * ys[i] / (es[i] * es[i]));
	}

	b = est.cov(0, 1) / est.var(0);
	a = std::exp(est.mean(1) - est.mean(0) * b);
}

double ExponentialFit::operator()(double x) const
{
	return a * std::exp(b * x);
}

void Histogram::init(double min, double max, size_t n)
{
	mins.resize(n);
	maxs.resize(n);
	bins.resize(n);
	for (size_t i = 0; i < n; ++i)
	{
		mins[i] = min + (max - min) * i / n;
		maxs[i] = min + (max - min) * (i + 1) / n;
		bins[i] = 0;
	}
}

Histogram::Histogram(double min, double max, size_t n) { init(min, max, n); }

Histogram::Histogram(std::span<const double> xs, size_t n)
{
	double lo = 1.0 / 0.0;
	double hi = -1.0 / 0.0;
	for (double x : xs)
	{
		lo = std::min(lo, x);
		hi = std::max(hi, x);
	}
	init(lo, hi, n);
	for (double x : xs)
		add(x);
}

void Histogram::add(double x)
{
	if (!(mins.front() <= x && x <= maxs.back()))
	{
		ignored += 1;
		return;
	}

	auto it = std::upper_bound(maxs.begin(), maxs.end(), x);
	auto i = std::distance(maxs.begin(), it);
	bins[i] += 1;
	total += 1;
}

void IntHistogram::add(int x, int64_t weight)
{
	assert(x >= 0);
	assert(x < (1 << 20));
	// NOTE: std::vector::resize() does (typically) not over-allocate, so we
	//       have to do the growth-factor ourselves
	if (x >= (int)bins_.size())
		bins_.resize(std::max(x + 1, (int)bins_.size() * 2));

	max_ = std::max(max_, x);
	count_ += weight;
	sum_ += x * weight;
	bins_[x] += weight;
}

void IntHistogram::add(std::span<const int> xs)
{
	for (auto x : xs)
		add(x);
}

IntHistogram &IntHistogram::operator+=(const IntHistogram &b)
{
	if (max_ < b.max_)
	{
		bins_.resize(b.max_ + 1);
		max_ = b.max_;
	}
	for (size_t i = 0; i < b.bins_.size(); ++i)
		bins_[i] += b.bins_[i];
	count_ += b.count_;
	sum_ += b.sum_;
	return *this;
}

// returns the k-th smallest element in the histogram
//   * if k >= count(), returns max()+1
int IntHistogram::find_nth(int64_t n) const
{
	assert(n >= 0);
	if (n >= count_)
		return max_ + 1;
	int64_t c = 0;
	for (size_t i = 0; i < bins_.size(); ++i)
	{
		c += bins_[i];
		if (c > n)
			return i;
	}
	unreachable();
}

template <size_t dim> Estimator<dim>::Estimator() { clear(); }

template <size_t dim> void Estimator<dim>::add(std::array<double, dim> x)
{
	return add(x, 1.0);
}

template <size_t dim>
void Estimator<dim>::add(std::array<double, dim> x, double w)
{
	n += w;
	double dx[dim];
	for (size_t i = 0; i < dim; ++i)
	{
		dx[i] = x[i] - avg[i];
		avg[i] += dx[i] * (w / n);
	}

	for (size_t i = 0; i < dim; ++i)
		for (size_t j = 0; j < dim; ++j)
			sum2[i][j] += w * dx[i] * (x[j] - avg[j]);
}

template <size_t dim> double Estimator<dim>::mean(size_t i) const
{
	return avg[i];
}

template <size_t dim> double Estimator<dim>::var(size_t i) const
{
	return sum2[i][i] / (n - 1);
}

template <size_t dim> double Estimator<dim>::cov(size_t i, size_t j) const
{
	return sum2[i][j] / (n - 1);
}

template <size_t dim> double Estimator<dim>::corr(size_t i, size_t j) const
{
	return cov(i, j) / sqrt(var(i) * var(j));
}

template <size_t dim> void Estimator<dim>::clear()
{
	n = 0;
	for (size_t i = 0; i < dim; ++i)
		avg[i] = 0;
	for (size_t i = 0; i < dim; ++i)
		for (size_t j = 0; j < dim; ++j)
			sum2[i][j] = 0;
}

// template class Estimator<1>; // explicitly specialized
template class Estimator<2>;
template class Estimator<3>;
template class Estimator<4>;

void BinnedSeries::add(double x)
{
	if (binsize_ == 1)
	{
		bins_.push_back(x);
		est_.add(x);
	}
	else
	{
		buffer_.push_back(x);
		if (buffer_.size() == binsize_)
		{
			auto s = util::mean(buffer_);
			buffer_.clear();

			bins_.push_back(s);
			est_.add(s);
		}
	}

	if (bins_.size() >= 2 * (size_t)min_nbins_ && bins_.size() % 2 == 0)
	{
		binsize_ *= 2;
		for (size_t i = 0; i < bins_.size() / 2; ++i)
			bins_[i] = 0.5 * (bins_[2 * i] + bins_[2 * i + 1]);
		bins_.resize(bins_.size() / 2);

		// re-compute the mean/variance/... estimations
		// (mean is only affected by numerical errors, variance is critical
		// in case of autocorrelation)
		est_.clear();
		for (auto x : bins_)
			est_.add(x);
	}
}

std::vector<double> autocorrelation(std::span<const double> xs, size_t m)
{
	m = std::min(m, xs.size() - 1);
	std::vector<double> r(m, 0.0 / 0.0);
	r[0] = 1.0;
	for (size_t k = 1; k < m; ++k)
	{
		Estimator<2> est;
		for (size_t i = 0; i < xs.size() - k; ++i)
			est.add({xs[i], xs[i + k]});
		r[k] = est.corr();
	}
	return r;
}

double correlationTime(std::span<const double> xs)
{
	auto mx = mean(xs);
	auto vx = variance(xs);

	double time = 0.5;
	for (size_t lag = 1; lag < xs.size() / 20; ++lag)
	{
		double sum = 0;
		for (size_t i = 0; i < xs.size() - lag; ++i)
			sum += (xs[i] - mx) * (xs[i + lag] - mx);
		time += sum / (xs.size() - lag) / vx;
		if (lag >= 5 * time)
			return time;
	}
	return 1.0 / 0.0; // no reliable estimation -> infinity
}

// format a value with error, e.g. "1.23(45)".
//   * The error is shown to (typically) two digits.
//   * Always uses 'f' style, i.e. no scientific notation. Therefore, the result
//     will look bad if the value is very large or very small.
std::string format_error(double val, double err)
{
	if (err == 0)
		return fmt::format("{}", val);
	if (err < 0 || err != err || err == 1.0 / 0.0)
		return fmt::format("{}(?)", val);

	int digits = 0;
	while (int(err) < 10 && digits < 30)
	{
		err *= 10;
		digits += 1;
	}

	auto ierr = int(err);

	// special case: decimal point in the middle of the error
	if (digits == 1)
	{
		assert(10 <= ierr && int(err) <= 99);
		return fmt::format("{:.{}f}({}.{})", val, digits, ierr / 10, ierr % 10);
	}

	if (digits >= 2)
		assert(ierr < 100);
	else
		assert(digits == 0);
	return fmt::format("{:.{}f}({})", val, digits, int(err));
}

} // namespace util
