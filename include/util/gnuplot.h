#pragma once

#include "util/functional.h"
#include "util/stats.h"
#include <concepts>
#include <ranges>
#include <span>

namespace util {

// Handle to gnuplot process, typically corresponds to an open window. Example:
//     Gnuplot().plot_data(std::vector<double>(...))
//              .plot_function("sin(x)")
//              .range_x(0, 10);
class Gnuplot
{
	FILE *pipe = nullptr;
	static int nplotsGlobal;
	int nplots = 0;
	const int plotID;
	bool logx_ = false;
	bool logy_ = false;
	bool logz_ = false;

  public:
	explicit Gnuplot(bool persist = true);
	~Gnuplot();

	// move-only due to contained pipe
	Gnuplot(const Gnuplot &) = delete;
	Gnuplot &operator=(const Gnuplot &) = delete;

	Gnuplot(Gnuplot &&b)
	    : pipe(b.pipe), nplots(b.nplots), plotID(b.plotID), logx_(b.logx_),
	      logy_(b.logy_), logz_(b.logz_)
	{
		b.pipe = nullptr;
	}
	Gnuplot &operator=(Gnuplot &&) = delete;

	// backend implementation.
	//   * If 'es' is empty, no error bars are shown.
	//   * If 'title' is empty, no title is shown.
	//   * If 'style' is empty, 'errorbars' or 'points' is used.
	//   * If 'xs' is empty, the x-values are assumed to be 0, 1, 2, ...
	Gnuplot &plot_data_impl(std::span<const double> xs,
	                        std::span<const double> ys,
	                        std::span<const double> es, std::string_view title,
	                        std::string_view style);

	// plot a function given as function object
	Gnuplot &plot_function(util::function_view<double(double)> fun, double min,
	                       double max, std::string_view title = "");

	// plot raw data points (i, ys[i])
	Gnuplot &plot_data(std::span<const double> ys, std::string_view title = "",
	                   std::string_view style = "");
	Gnuplot &plot_error(std::span<const double> ys, std::span<const double> err,
	                    std::string_view title = "",
	                    std::string_view style = "");

	// plot raw data points (xs[i], ys[i])
	Gnuplot &plot_data(std::span<const double> xs, std::span<const double> ys,
	                   std::string_view title = "data",
	                   std::string_view style = "");
	Gnuplot &plot_error(std::span<const double> xs, std::span<const double> ys,
	                    std::span<const double> err,
	                    std::string_view title = "data",
	                    std::string_view style = "");

	// templated version for ranges
	template <std::ranges::range R>
	    requires std::convertible_to<std::ranges::range_value_t<R>, double>
	Gnuplot &plot_range_data(const R &ys, std::string_view title = "",
	                         std::string_view style = "")
	{
		std::vector<double> v(std::ranges::begin(ys), std::ranges::end(ys));
		return plot_data_impl({}, std::span(v), {}, title, style);
	}

	// 3D plot of a 2D grid
	Gnuplot &plot_data_3d(ndspan<const double, 2> zs,
	                      std::string_view title = "");

	// plot a histogram
	Gnuplot &plot_histogram(const Histogram &hist, std::string_view title = "",
	                        double scale = 1.0);
	Gnuplot &plot_histogram(const Histogram &hist,
	                        util::function_view<double(double)> dist,
	                        std::string_view title = "");
	Gnuplot &plot_histogram(const IntHistogram &hist,
	                        std::string_view title = "", double scale = 1.0);

	// black horizontal line without label
	Gnuplot &hline(double y);

	// set range of plot
	Gnuplot &range_x(double min, double max);
	Gnuplot &range_y(double min, double max);
	Gnuplot &range_z(double min, double max);

	// make the plot logarithmic
	Gnuplot &log_scale_x();
	Gnuplot &log_scale_y();
	Gnuplot &log_scale_z();

	// remove all plots (but keep some settings)
	Gnuplot &clear();

	// save figure to file
	Gnuplot &savefig(std::string_view filename);
};

} // namespace util
