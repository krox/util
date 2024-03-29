#ifndef UTIL_GNUPLOT_H
#define UTIL_GNUPLOT_H

#include "util/functional.h"
#include "util/span.h"
#include "util/stats.h"

namespace util {

class Gnuplot
{
	FILE *pipe = nullptr;
	static int nplotsGlobal;
	int nplots = 0;
	const int plotID;
	std::string style_ = "points";
	bool logx_ = false;
	bool logy_ = false;
	bool logz_ = false;

  public:
	/** constructor */
	explicit Gnuplot(bool persist = true);
	~Gnuplot();

	/** non-copyable but movable due to pipe */
	Gnuplot(const Gnuplot &) = delete;
	Gnuplot &operator=(const Gnuplot &) = delete;

	Gnuplot(Gnuplot &&b)
	    : pipe(b.pipe), nplots(b.nplots), plotID(b.plotID),
	      style_(std::move(b.style_)), logx_(b.logx_), logy_(b.logy_),
	      logz_(b.logz_)
	{
		b.pipe = nullptr;
	}
	Gnuplot &operator=(Gnuplot &&) = delete;

	/** plot a function given by a string that gnuplot can understand */
	Gnuplot &plotFunction(const std::string &fun,
	                      const std::string &title = "");
	Gnuplot &plotFunction(const std::string &fun, double min, double max,
	                      const std::string &title = "");

	/** plot a function given as function object */
	Gnuplot &plotFunction(util::function_view<double(double)> fun, double min,
	                      double max, const std::string &title = "");

	/** plot raw data points (i, ys[i]) */
	Gnuplot &plotData(gspan<const double> ys,
	                  const std::string &title = "data");
	Gnuplot &plotError(gspan<const double> ys, gspan<const double> err,
	                   const std::string &title = "data");

	/** plot raw data points (xs[i], ys[i]) */
	Gnuplot &plotData(gspan<const double> xs, gspan<const double> ys,
	                  const std::string &title = "data");
	Gnuplot &plotError(gspan<const double> xs, gspan<const double> ys,
	                   gspan<const double> err,
	                   const std::string &title = "data");

	/** 3D plot of a 2D grid */
	Gnuplot &plotData3D(ndspan<const double, 2> zs,
	                    const std::string &title = "data");

	/** plot a histogram */
	Gnuplot &plotHistogram(const Histogram &hist,
	                       const std::string &title = "hist",
	                       double scale = 1.0);
	Gnuplot &plotHistogram(const Histogram &hist,
	                       util::function_view<double(double)> dist,
	                       const std::string &title = "hist");
	Gnuplot &plotHistogram(const IntHistogram &hist,
	                       const std::string &title = "hist",
	                       double scale = 1.0);

	/** black horizontal line without label */
	Gnuplot &hline(double y);

	/** set style */
	Gnuplot &style(const std::string &s)
	{
		style_ = s;
		return *this;
	}

	/** set range of plot */
	Gnuplot &setRangeX(double min, double max);
	Gnuplot &setRangeY(double min, double max);
	Gnuplot &setRangeZ(double min, double max);

	/** make the plot logarithmic */
	Gnuplot &setLogScaleX();
	Gnuplot &setLogScaleY();
	Gnuplot &setLogScaleZ();

	/** remove all plots (but keep settings) */
	Gnuplot &clear();

	Gnuplot &savefig(const std::string &filename);
}; // namespace util

} // namespace util

#endif
