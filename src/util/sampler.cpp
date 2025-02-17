#include "util/sampler.h"

#include <cmath>
#include <queue>
#include <vector>

#include <fmt/format.h>

#include "util/gnuplot.h"
#include "util/numerics.h"
#include "util/random.h"
#include "util/stats.h"

namespace util {
/** area of upper bound */
double LogSampler::Region::areaUpper() const
{
	if (beta == 0)
		return (b - a) * std::exp(alpha);
	return (b - a) / beta * std::exp(alpha) * std::expm1(beta);
}

/** area of lower bound */
double LogSampler::Region::areaLower() const
{
	if (delta == 0)
		return (b - a) * std::exp(gamma);
	return (b - a) / delta * std::exp(gamma) * std::expm1(delta);
}

/** lower area / upper area */
double LogSampler::Region::frac() const { return areaLower() / areaUpper(); }

/** sort regions with bad bounds to the front */
bool LogSampler::Region::operator<(const Region &r) const
{
	return areaUpper() - areaLower() < r.areaUpper() - r.areaLower();
}

LogSampler::Region LogSampler::makeRegion(double a, double b)
{
	assert(a < b);
	Region r;
	r.a = a;
	r.b = b;

	// secant through endpoints
	r.alpha = f(a);
	r.beta = f(b) - f(a);
	if (r.alpha == -std::numeric_limits<double>::infinity())
		r.beta = 0;

	// tangent to midpoint
	r.delta = fd(0.5 * (a + b)) * (b - a);
	r.gamma = f(0.5 * (a + b)) - 0.5 * r.delta;

	if (r.alpha < r.gamma)
	{
		std::swap(r.alpha, r.gamma);
		std::swap(r.beta, r.delta);
	}

	assert(r.alpha >= r.gamma);
	assert(r.alpha + r.beta >= r.gamma + r.delta);
	return r;
}

LogSampler::LogSampler(std::function<double(double)> f,
                       std::function<double(double)> fd,
                       std::function<double(double)> fdd, double min,
                       double max, size_t nRegs)
    : f(f), fd(fd), fdd(fdd), min(min), max(max), uni_dist(0, 1)
{
	assert(min < max);

	// Idea:
	//    Split the interval [a,b] into N regions, and compute linear
	//    lower and upper bounds of f in each region. The upper bound is
	//    then used to produce candidates with a truncated exponential
	//    distribution. The lower bound is used for an early-out on the
	//    rejection-sampling. This means that when drawing random samples,
	//    f itself is evaluated very rarely.
	//
	//    To find good regions:
	//    1) split the interval at all inflection points of f, so that
	//       f is either concave or convex in each region. (makes finding
	//       the linear bounds very easy)
	//    2) repeatedly split the 'worst' region into two, where 'worst' is
	//       quantified by the difference between upper and lower bound.
	//
	// TODO / future ideas:
	//    * Implement unbounded intervals
	//      (exponential proposal-distribution for the tails)
	//    * Make all sub-intervals have the same 'area'/probability
	//      (makes sampling faster, but complicates construction, especially
	//      when there are inflection points)
	//    * Maybe switch from exponential proposal-distribution to something
	//      simpler like uniform or triangular ( = max(uniform,uniform)).
	//      The bounds will probably be worse, but the sampling gets rid of
	//      expensive logarithms.

	// sample f'' at 100 points
	std::vector<double> xs;
	xs.push_back(min);
	for (int i = 1; i < 99; ++i)
		xs.push_back(min + (max - min) * i / 99);
	xs.push_back(max);
	std::vector<double> fddxs;
	for (size_t i = 0; i < xs.size(); ++i)
		fddxs.push_back(fdd(xs[i]));

	// find inflection points to use as initial region bounds
	std::vector<double> ys;
	ys.push_back(xs.front());
	for (size_t i = 1; i < xs.size(); ++i)
		if (fddxs[i - 1] * fddxs[i] < 0)
			ys.push_back(util::solve(fdd, xs[i - 1], xs[i]));

	// create initial regions
	std::priority_queue<Region> heap;
	ys.push_back(xs.back());
	for (size_t i = 1; i < ys.size(); ++i)
		heap.push(makeRegion(ys[i - 1], ys[i]));

	// split regions that have bad bounds
	while (heap.size() < nRegs)
	{
		auto r = heap.top();
		heap.pop();
		double m = 0.5 * (r.a + r.b);
		heap.push(makeRegion(r.a, m));
		heap.push(makeRegion(m, r.b));
	}

	while (!heap.empty())
	{
		regs.push_back(heap.top());
		heap.pop();
	}

	// sort regions (not actually neccessary, but looks nicer for debugging)
	std::sort(regs.begin(), regs.end(),
	          [](const Region &a, const Region &b) { return a.a < b.a; });

	// create distribution of regions
	std::vector<double> areas;
	for (auto &r : regs)
		areas.push_back(r.areaUpper());
	disc_dist = std::discrete_distribution<int>(areas.begin(), areas.end());
}

double LogSampler::quality() const
{
	double areaUpper = 0;
	double areaLower = 0;
	for (auto &r : regs)
	{
		areaUpper += r.areaUpper();
		areaLower += r.areaLower();
	}
	return areaLower / areaUpper;
}

void LogSampler::test()
{
	{
		std::vector<double> plotX, plotY, plotY2;
		for (auto &r : regs)
		{
			plotX.push_back(r.a);
			plotX.push_back(r.b);
			plotY.push_back(r.gamma);
			plotY.push_back(r.gamma + r.delta);
			plotY2.push_back(r.alpha);
			plotY2.push_back(r.alpha + r.beta);
		}

		util::Gnuplot plot;
		plot.plot_function(f, min, max, "log-prob");
		plot.plot_data(plotX, plotY, "min", "lines");
		plot.plot_data(plotX, plotY2, "max", "lines");
	}

	{
		util::xoshiro256 rng{std::random_device()()};
		size_t count = 10000000;
		size_t binCount = 50;

		// create a histogram
		util::Histogram hist(min, max, binCount);
		for (size_t i = 0; i < count; ++i)
			hist.add((*this)(rng));
		fmt::print("nRegs    = {}\n", regs.size());
		fmt::print("quality  = {}\n", quality());
		fmt::print("accProb  = {}\n", accProb());
		fmt::print("evalProb = {}\n", evalProb());

		// match scaling of histogram/distribution
		double c =
		    util::integrate([&](double x) { return std::exp(f(x)); }, min, max);
		c = (double)count * (max - min) / binCount / c;

		util::Gnuplot p;
		p.plot_histogram(hist);
		p.plot_function([&](double x) { return c * std::exp(f(x)); }, min, max);
	}
}

} // namespace util
