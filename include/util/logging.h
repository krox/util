#pragma once

#include "fmt/format.h"
#include "util/stopwatch.h"
#include <functional>
#include <mutex>
#include <vector>

namespace util {

// simple logging facitily built upon fmtlib and some performance monitoring
//     * log level can be set globally or per component, both are stored in
//       global variables
//     * logger construction is (slightly) slow due to memory allocation and
//       mutexes, so reusing is good for performance
//     * logging itself should never throw (so it can be used in destructors and
//       any other precarious situation), so it is marked 'noexcept'.
//       TODO: actually handle any errors (from formatting or filesystem or
//       such) more gracefully than calling std::terminate
//     * TODO: obviously too many globals. hierarchically structured loggers
//       might be great. Any application would be free to declare a global one
//       if it so chooses.
//
// Intended usage:
//    // at the beginning of main()
//    Logger::set_level(Logger::Level::info);
//    Logger::set_level("my class", Logger::Level::debug);
//
//    // in a class constructor or similar
//    log = Logger("my class");
//
//    // during actual work
//    log.info("some message {}", 42);
//    log.debug("some more verbose message {}", 21+21);
//
//    // at the end of the program (optional)
//    Logger::print_summary();

class Logger
{
  public:
	enum class Level
	{
		// same levels as the 'spdlog' library (and probably many others)
		off,
		critical,
		error,
		warning,
		info,
		debug,
		trace
	};

  private:
	static void default_sink(std::string_view msg) { fmt::print("{}\n", msg); }
	inline static std::function<void(std::string_view)> g_sink_ = default_sink;

	inline static std::mutex g_mutex_;
	inline static Level g_default_level_ = Level::info;

	inline static Stopwatch g_total_ = Stopwatch().start();
	struct Component
	{
		std::string name;
		Level level = Level::info;
		double total_secs = 0;
	};
	inline static std::vector<Component> g_components_;

	// only call with lock held
	static Component &lookup(std::string_view name)
	{
		for (auto &comp : g_components_)
			if (comp.name == name)
				return comp;
		g_components_.push_back({std::string(name), g_default_level_, 0});
		return g_components_.back();
	}

  public:
	// set level for all components
	static void set_level(Level level)
	{
		auto lock = std::unique_lock(g_mutex_);
		g_default_level_ = level;
		for (auto &comp : g_components_)
			comp.level = level;
	}

	// set level for a specific component
	static void set_level(std::string_view name, Level level)
	{
		auto lock = std::unique_lock(g_mutex_);
		lookup(name).level = level;
	}

	// set global sink for log messages
	// IMPORTANT: not thread-safe. Call only once at the start of the program.
	static void set_sink(std::function<void(std::string_view)> sink)
	{
		g_sink_ = sink;
	}

  private:
	std::string name_;
	Level level_;
	util::Stopwatch sw;

	void do_log(std::string_view str, fmt::format_args &&args) const noexcept
	{
		std::string msg = fmt::vformat(str, std::move(args));
		msg = fmt::format("[{:12} {:#6.2f}] {}", name_, sw.secs(), msg);
		g_sink_(msg);
	}

  public:
	explicit Logger(std::string name) : name_(std::move(name))
	{
		sw.start();
		auto lock = std::unique_lock(g_mutex_);
		level_ = lookup(name_).level;
	}

	~Logger() noexcept
	{
		sw.stop();
		auto lock = std::unique_lock(g_mutex_);
		lookup(name_).total_secs += sw.secs();
	}

	static void print_summary()
	{
		auto lock = std::unique_lock(g_mutex_);
		g_sink_("============================ time stats "
		        "=============================");
		double tot = g_total_.secs();

		for (auto &comp : g_components_)
		{
			auto msg =
			    fmt::format("{:12}: {:#6.2f} s ({:#4.1f} %)", comp.name,
			                comp.total_secs, 100. * comp.total_secs / tot);
			g_sink_(msg);
		}
	}

	template <class... Args>
	void trace(std::string_view str, Args &&...args) const noexcept
	{
		if (level_ >= Level::trace)
			do_log(str, fmt::make_format_args(args...));
	}

	template <class... Args>
	void debug(std::string_view str, Args &&...args) const noexcept
	{
		if (level_ >= Level::debug)
			do_log(str, fmt::make_format_args(args...));
	}

	template <class... Args>
	void info(std::string_view str, Args &&...args) const noexcept
	{
		if (level_ >= Level::info)
			do_log(str, fmt::make_format_args(args...));
	}

	template <class... Args>
	void warning(std::string_view str, Args &&...args) const noexcept
	{
		if (level_ >= Level::warning)
			do_log(str, fmt::make_format_args(args...));
	}

	template <class... Args>
	void error(std::string_view str, Args &&...args) const noexcept
	{
		if (level_ >= Level::error)
			do_log(str, fmt::make_format_args(args...));
	}

	template <class... Args>
	void critical(std::string_view str, Args &&...args) const noexcept
	{
		if (level_ >= Level::critical)
			do_log(str, fmt::make_format_args(args...));
	}

	double secs() const { return sw.secs(); }
};

} // namespace util