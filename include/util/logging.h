#pragma once

#include "util/atomic.h"
#include "util/fixed_map.h"
#include "util/functional.h"
#include "util/io.h"
#include "util/stopwatch.h"
#include "util/vector.h"
#include <atomic>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <fmt/format.h>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <optional>
#include <stop_token>
#include <string>
#include <string_view>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

namespace util {

// Little terminal renderer to re-draw dynamic lines on the bottom of the
// terminal. Intended for progress bars and the like.
class Terminal
{
  public:
	using line_sink = function_view<void(std::string_view)>;
	using print_t = std::function<void(line_sink, int)>;

  private:
	static constexpr std::chrono::milliseconds interval_{50};

	std::mutex mutex_;
	std::condition_variable cv_;
	std::deque<std::string> msg_queue_;
	util::vector<std::unique_ptr<print_t>> sections_;
	int rendered_lines_ = 0;
	std::jthread thread_;
	util::vector<char> frame_;
	std::optional<std::promise<std::string>> input_promise_;

	// must be called with locked mutex
	void prepare_frame();

	void thread_main(std::stop_token stop);

  public:
	class Section
	{
		Terminal *terminal_ = nullptr;
		print_t *print_ = nullptr;

	  public:
		Section() = default;
		explicit Section(Terminal *terminal, print_t *print)
		    : terminal_(terminal), print_(print)
		{
			assert(terminal);
			assert(print);
		}

		Section(const Section &) = delete;
		Section &operator=(const Section &) = delete;
		Section(Section &&other) noexcept
		    : terminal_(std::exchange(other.terminal_, nullptr)),
		      print_(std::exchange(other.print_, nullptr))
		{}
		Section &operator=(Section &&other) noexcept
		{
			close();
			terminal_ = std::exchange(other.terminal_, nullptr);
			print_ = std::exchange(other.print_, nullptr);
			return *this;
		}

		~Section() { close(); }

		void close() noexcept;
	};

	friend class Section;

	Terminal() : thread_(&Terminal::thread_main, this) {}

	// enqueue message
	void print(std::string msg)
	{
		{
			auto lock = std::unique_lock(mutex_);
			msg_queue_.push_back(std::move(msg));
		}
		cv_.notify_one();
	}

	// block until all enqueued messages are processed
	void flush()
	{
		auto lock = std::unique_lock(mutex_);
		cv_.wait(lock, [this] { return msg_queue_.empty(); });
	}

	// create a new dynamic section
	Section section(print_t print)
	{
		assert(print);
		auto section_ptr = std::make_unique<print_t>(std::move(print));
		auto r = Section(this, section_ptr.get());
		{
			auto lock = std::unique_lock(mutex_);
			sections_.push_back(std::move(section_ptr));
		}
		cv_.notify_one();
		return r;
	}
};

enum class LogLevel
{
	off,
	critical,
	error,
	warning,
	info,
	debug,
	trace
};

// Handles output to stdout
//   * Thread-safe: Any thread can use operator() to print a log message
//   * Asynchronous: Actual printing is done in a dedicated thread
//   * Keeps progress bars on the bottom of the terminal, log messages above
class Logger
{

  public:
	using Clock = std::chrono::steady_clock;

	// user-facing types
	using Level = LogLevel;
	class Scope;
	class ProgressBar;

	Logger(Level default_level = Level::info)
	    : default_level_(default_level), time_stack_()
	{}

	// not copyable or movable. Typicallly there should only be a single
	// instance for the entire program anyway.
	Logger(Logger const &) = delete;
	Logger &operator=(Logger const &) = delete;
	Logger(Logger &&) = delete;
	Logger &operator=(Logger &&) = delete;

	// open a scope for logging and terminal status reporting
	Scope scope(std::string_view name);
	Scope scope(std::string_view name, Level l);

	ProgressBar bar(int64_t total);

	// non-templated logging backend function
	void do_log(std::string msg) { terminal_.print(std::move(msg)); }

	// default level of the logger. Affects (1) all subsequent scopes that are
	// created without explicit level and (2) any subsequent top-level messages.
	void set_default_level(Level level) { default_level_.store(level); }
	Level default_level() const noexcept { return default_level_.load(); }

	// print timing summary for all components via normal top-level info logs
	void print_summary() { do_log(time_stack_.lock()->summary()); }

	// reset accumulated component timings and summary baseline timer
	void reset_summary() { time_stack_.lock()->clear(); }

	// top-level logging functions
	template <class... Args>
	void log(Level level, fmt::format_string<Args...> format, Args &&...args)
	{
		if (level <= default_level())
			do_log(fmt::format(format, std::forward<Args>(args)...));
	}
	template <class... Args>
	void trace(fmt::format_string<Args...> format, Args &&...args)
	{
		log(Level::trace, format, std::forward<Args>(args)...);
	}
	template <class... Args>
	void debug(fmt::format_string<Args...> format, Args &&...args)
	{
		log(Level::debug, format, std::forward<Args>(args)...);
	}
	template <class... Args>
	void info(fmt::format_string<Args...> format, Args &&...args)
	{
		log(Level::info, format, std::forward<Args>(args)...);
	}
	template <class... Args>
	void warning(fmt::format_string<Args...> format, Args &&...args)
	{
		log(Level::warning, format, std::forward<Args>(args)...);
	}
	template <class... Args>
	void error(fmt::format_string<Args...> format, Args &&...args)
	{
		log(Level::error, format, std::forward<Args>(args)...);
	}
	template <class... Args>
	void critical(fmt::format_string<Args...> format, Args &&...args)
	{
		log(Level::critical, format, std::forward<Args>(args)...);
	}

	// block until all pending messages have been processed and written
	void flush() noexcept { terminal_.flush(); }

  private:
	relaxed_atomic<Level> default_level_;
	synchronized<TimeStack> time_stack_;
	Terminal terminal_; // threadsafe itself
};

class Logger::Scope
{
	// NOTE: 'Scope' does have a null-state. Thats useful as:
	//   * Time-accounting is otherwise tied to Scope's lifetime, setting a
	//     scope to null-state will stop timing explicitly.
	//   * In the null-state, all logging is a silent no-op.

	Logger *logger_ = nullptr;
	std::string name_;
	Terminal::Section terminal_section_;
	using Clock = TimeStack::Clock;
	Clock::time_point start_ = Clock::now();
	relaxed_atomic<Logger::Level> level_ = Logger::Level::info;

	friend class Logger;

	void print(Terminal::line_sink sink, int width);

  public:
	Scope() = default;
	Scope(Logger *logger, std::string name, Level level);
	~Scope() noexcept { close(); }
	void close() noexcept;

	std::string const &name() const noexcept { return name_; }

	Level level() const noexcept { return level_.load(); }
	void set_level(Level l) noexcept { level_.store(l); }

	// dont move. The printing callback has references to ticks/total.
	Scope(Scope const &) = delete;
	Scope &operator=(Scope const &) = delete;
	Scope(Scope &&other) noexcept = delete;
	Scope &operator=(Scope &&other) noexcept = delete;

	// Returns the elapsed time since the scope was created.
	Clock::duration elapsed() const noexcept { return Clock::now() - start_; }

	// log a message at specified level
	//   * no-op if level is lower than current logging level.
	//   * also no-op if the scope is in null-state.
	template <typename... Args>
	void log(Level level, fmt::format_string<Args...> format,
	         Args &&...args) const
	{
		if (!logger_ || level > this->level())
			return;
		std::string buf = fmt::format("[{}] ", name_);
		fmt::format_to(std::back_inserter(buf), format,
		               std::forward<Args>(args)...);
		logger_->do_log(std::move(buf));
	}

	template <typename... Args>
	void trace(fmt::format_string<Args...> format, Args &&...args) const
	{
		log(Level::trace, format, std::forward<Args>(args)...);
	}
	template <typename... Args>
	void debug(fmt::format_string<Args...> format, Args &&...args) const
	{
		log(Level::debug, format, std::forward<Args>(args)...);
	}

	template <typename... Args>
	void info(fmt::format_string<Args...> format, Args &&...args) const
	{
		log(Level::info, format, std::forward<Args>(args)...);
	}

	template <typename... Args>
	void warning(fmt::format_string<Args...> format, Args &&...args) const
	{
		log(Level::warning, format, std::forward<Args>(args)...);
	}

	template <typename... Args>
	void error(fmt::format_string<Args...> format, Args &&...args) const
	{
		log(Level::error, format, std::forward<Args>(args)...);
	}

	template <typename... Args>
	void critical(fmt::format_string<Args...> format, Args &&...args) const
	{
		log(Level::critical, format, std::forward<Args>(args)...);
	}
};

class Logger::ProgressBar
{

	Logger *logger_ = nullptr;
	Terminal::Section terminal_section_;
	using Clock = TimeStack::Clock;
	Clock::time_point start_ = Clock::now();
	relaxed_atomic<int64_t> ticks_ = 0;
	relaxed_atomic<int64_t> total_ = 0;

	friend class Logger;

	void print(Terminal::line_sink sink, int width);

  public:
	ProgressBar() = default;
	ProgressBar(Logger *logger, int64_t total);
	~ProgressBar() noexcept { close(); }
	void close() noexcept;

	int64_t ticks() const noexcept { return ticks_.load(); }
	int64_t total() const noexcept { return total_.load(); }
	void set_ticks(int64_t ticks) noexcept { ticks_.store(ticks); }
	void set_total(int64_t total) noexcept { total_.store(total); }
	void increment(int64_t ticks = 1) noexcept { ticks_.fetch_add(ticks); }

	// dont move. The printing callback has references to ticks/total.
	ProgressBar(ProgressBar const &) = delete;
	ProgressBar &operator=(ProgressBar const &) = delete;
	ProgressBar(ProgressBar &&other) noexcept = delete;
	ProgressBar &operator=(ProgressBar &&other) noexcept = delete;

	// Returns the elapsed time since the scope was created.
	Clock::duration elapsed() const noexcept { return Clock::now() - start_; }
};

inline Logger::Scope Logger::scope(std::string_view name)
{
	return scope(name, default_level());
}

inline Logger::Scope Logger::scope(std::string_view name, Level l)
{
	return Scope(this, std::string(name), l);
}

inline Logger::ProgressBar Logger::bar(int64_t total)
{
	return ProgressBar(this, total);
}

} // namespace util
