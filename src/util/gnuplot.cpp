#include "util/gnuplot.h"

#include "util/numerics.h"
#include <fmt/format.h>
#include <fstream>
#include <numeric>

int util::Gnuplot::nplotsGlobal = 0;

util::Gnuplot::Gnuplot(bool persist) : plotID(nplotsGlobal++)
{
	if (persist)
		pipe = popen("gnuplot -p", "w");
	else
		pipe = popen("gnuplot", "w");
	if (pipe == nullptr)
		throw "ERROR: Could not open gnuplot (is it installed?)";

	fmt::print(pipe, "set output\n");
	fmt::print(pipe, "set terminal x11\n");
	fflush(pipe);
}

util::Gnuplot::~Gnuplot()
{
	if (pipe != nullptr)
		pclose(pipe);
}

util::Gnuplot &util::Gnuplot::plot_data_impl(std::span<const double> xs,
                                             std::span<const double> ys,
                                             std::span<const double> es,
                                             std::string_view title,
                                             std::string_view style)
{
	std::string filename = fmt::format("gnuplot_{}_{}.txt", plotID, nplots);
	std::ofstream file(filename);

	std::vector<double> xs_;
	if (xs.empty())
	{
		xs_.resize(ys.size());
		std::iota(xs_.begin(), xs_.end(), 0);
		xs = xs_;
	}
	else
		assert(xs.size() == ys.size());
	if (es.empty())
	{
		for (size_t i = 0; i < ys.size(); ++i)
			file << xs[i] << " " << ys[i] << "\n";
	}
	else
	{
		assert(xs.size() == es.size());
		for (size_t i = 0; i < ys.size(); ++i)
			file << xs[i] << " " << ys[i] << " " << es[i] << "\n";
	}

	if (ys.empty())
	{
		fmt::print("WARNING: tried to plot empty data (title = '{}')\n", title);
		return *this;
	}

	if (style.empty())
		style = es.empty() ? "points" : "errorbars";
	if (title.empty())
		title = "data";

	file.flush();
	file.close();
	fmt::print(pipe, "{} '{}' using {} with {} title \"{}\"\n",
	           (nplots ? "replot" : "plot"), filename,
	           (es.empty() ? "1:2" : "1:2:3"), style, title);
	fflush(pipe);
	++nplots;
	return *this;
}

util::Gnuplot &
util::Gnuplot::plot_function(util::function_view<double(double)> fun, double a,
                             double b, std::string_view title)
{
	std::vector<double> xs, ys;
	for (int i = 0; i <= 100; ++i)
	{
		xs.push_back(a + (b - a) * i / 100);
		ys.push_back(fun(xs.back()));
	}

	return plot_data_impl(xs, ys, {}, title, "lines");
}

util::Gnuplot &util::Gnuplot::plot_data(std::span<const double> ys,
                                        std::string_view title,
                                        std::string_view style)
{
	return plot_data_impl({}, ys, {}, title, style);
}

util::Gnuplot &util::Gnuplot::plot_error(std::span<const double> ys,
                                         std::span<const double> err,
                                         std::string_view title,
                                         std::string_view style)
{
	return plot_data_impl({}, ys, err, title, style);
}

util::Gnuplot &util::Gnuplot::plot_data(std::span<const double> xs,
                                        std::span<const double> ys,
                                        std::string_view title,
                                        std::string_view style)
{
	return plot_data_impl(xs, ys, {}, title, style);
}

util::Gnuplot &util::Gnuplot::plot_error(std::span<const double> xs,
                                         std::span<const double> ys,
                                         std::span<const double> err,
                                         std::string_view title,
                                         std::string_view style)
{
	return plot_data_impl(xs, ys, err, title, style);
}

util::Gnuplot &util::Gnuplot::plot_data_3d(ndspan<const double, 2> zs,
                                           std::string_view title)
{
	std::string filename = fmt::format("gnuplot_{}_{}.txt", plotID, nplots);
	std::ofstream file(filename);
	for (size_t i = 0; i < zs.shape(0); ++i)
	{
		for (size_t j = 0; j < zs.shape(1); ++j)
			file << zs(i, j) << " ";
		file << "\n";
	}
	file.flush();
	file.close();
	fmt::print(pipe, "{} '{}' matrix with {} title \"{}\"\n",
	           (nplots ? "replot" : "splot"), filename, "points", title);
	fflush(pipe);
	++nplots;
	return *this;
}

util::Gnuplot &util::Gnuplot::plot_histogram(Histogram const &hist,
                                             std::string_view title,
                                             double scale)
{
	std::string filename = fmt::format("gnuplot_{}_{}.txt", plotID, nplots);
	std::ofstream file(filename);

	for (size_t i = 0; i < hist.bins.size(); ++i)
		file << 0.5 * (hist.mins[i] + hist.maxs[i]) << " "
		     << scale * hist.bins[i] << "\n";
	file.flush();
	file.close();
	fmt::print(pipe, "{} '{}' using 1:2 with histeps title \"{}\"\n",
	           (nplots ? "replot" : "plot"), filename, title);
	fflush(pipe);
	nplots++;
	return *this;
}

util::Gnuplot &
util::Gnuplot::plot_histogram(Histogram const &hist,
                              util::function_view<double(double)> dist,
                              std::string_view title)
{
	double c = 1.0 / util::integrate(dist, hist.min(), hist.max());
	plot_histogram(hist, title,
	               hist.binCount() / (hist.max() - hist.min()) / hist.total);
	plot_function([&](double x) { return c * dist(x); }, hist.min(),
	              hist.max());

	return *this;
}

util::Gnuplot &util::Gnuplot::plot_histogram(const IntHistogram &hist,
                                             std::string_view title,
                                             double scale)
{
	std::string filename = fmt::format("gnuplot_{}_{}.txt", plotID, nplots);
	std::ofstream file(filename);

	for (int i = 0; i <= hist.max(); ++i)
		file << i << " " << scale * hist.bin(i) << "\n";
	file.flush();
	file.close();
	fmt::print(pipe, "{} '{}' using 1:2 with histeps title \"{}\"\n",
	           (nplots ? "replot" : "plot"), filename, title);
	fflush(pipe);
	nplots++;
	return *this;
}

util::Gnuplot &util::Gnuplot::hline(double y)
{
	fmt::print(pipe, "{} {} lt -1 title \"\"\n", nplots ? "replot" : "plot", y);
	fflush(pipe);
	++nplots;
	return *this;
}

util::Gnuplot &util::Gnuplot::range_x(double min, double max)
{
	fmt::print(pipe, "set xrange[{} : {}]\n", min, max);
	fflush(pipe);
	return *this;
}

util::Gnuplot &util::Gnuplot::range_y(double min, double max)
{
	fmt::print(pipe, "set yrange[{} : {}]\n", min, max);
	fflush(pipe);
	return *this;
}

util::Gnuplot &util::Gnuplot::range_z(double min, double max)
{
	fmt::print(pipe, "set zrange[{} : {}]\n", min, max);
	fflush(pipe);
	return *this;
}

util::Gnuplot &util::Gnuplot::log_scale_x()
{
	fmt::print(pipe, "set logscale x\n");
	logx_ = true;
	fflush(pipe);
	return *this;
}

util::Gnuplot &util::Gnuplot::log_scale_y()
{
	fmt::print(pipe, "set logscale y\n");
	logy_ = true;
	fflush(pipe);
	return *this;
}

util::Gnuplot &util::Gnuplot::log_scale_z()
{
	fmt::print(pipe, "set logscale z\n");
	logz_ = true;
	fflush(pipe);
	return *this;
}

util::Gnuplot &util::Gnuplot::clear()
{
	fmt::print(pipe, "clear\n");
	fflush(pipe);
	nplots = 0;
	return *this;
}

util::Gnuplot &util::Gnuplot::savefig(std::string_view filename)
{
	fmt::print(pipe, "set terminal pdf\n");
	fmt::print(pipe, "set output \"{}\"\n", filename);
	fmt::print(pipe, "replot\n");
	fflush(pipe);
	return *this;
}
