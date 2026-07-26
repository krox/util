#pragma once

#include "util/atomic.h"
#include "util/fixed_map.h"
#include "util/io.h"
#include <atomic>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <fmt/format.h>
#include <memory>
#include <mutex>
#include <stop_token>
#include <string>
#include <string_view>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

namespace util {

// Handles output to stdout
//   * Thread-safe: Any thread can use operator() to print a log message
//   * Asynchronous: Actual printing is done in a dedicated thread
//   * Keeps progress bars on the bottom of the terminal, log messages above
class AsyncOutput
{
  public:
	using Clock = std::chrono::steady_clock;

	// user-facing types
	enum class Level;
	class Component;
	class Scope;
	class Bar;

	// constructor creates the background thread, does not print anything yet.
	//   * if 'log_file' is non-empty, all log messages (but not progress bars)
	//     are additionally written there as plain text.
	explicit AsyncOutput(std::string_view log_file = {});

	// destructor processes any remaining messages, then joins background thread
	~AsyncOutput();

	// not copyable or movable. Typicallly there should only be a single
	// instance for the entire program anyway.
	AsyncOutput(AsyncOutput const &) = delete;
	AsyncOutput &operator=(AsyncOutput const &) = delete;
	AsyncOutput(AsyncOutput &&) = delete;
	AsyncOutput &operator=(AsyncOutput &&) = delete;

	// look up a component by name, creating it if it does not exist yet.
	Component &operator[](std::string_view component_name);

	// open a scope for logging
	Scope scope(std::string_view name);

	// add a progress bar (label is optional, no uniqueness requirement)
	Bar bar(uint64_t total, std::string label);

	// non-template backend for logging.
	//   * level is not checked at this point anymore
	//   * msg is already formatted
	//   * comonent can be empty/null for top-level messages
	void do_log(Component const *component, Level level, std::string_view msg);

  private:
	// configuration
	static constexpr auto interval_ = std::chrono::milliseconds(50);
	static constexpr int line_width_ = 80;

	// internal types
	struct Message;
	class BarState;

	std::mutex mutex_;
	std::condition_variable cv_;
	std::deque<Message> msg_queue_; // Pending messages, protected by mutex_
	std::vector<std::unique_ptr<Component>> components_;
	std::vector<std::unique_ptr<BarState>> bars_;
	File log_file_; // optional, only opened if a filename was given
	size_t rendered_lines_ = 0;

	// NOTE: must be the last member. Its constructor starts the background
	// thread immediately, which may access any of the members above, so all
	// of them need to be fully constructed first.
	std::jthread thread_;

	void thread_main(std::stop_token stop);
	// formats+writes to stdout (with ANSI cursor control, redrawing bars in
	// place); tracks rendered_lines_.
	void write_terminal(std::deque<Message> const &messages,
	                    std::vector<BarState const *> const &bars) noexcept;
	// formats+writes plain text to log_file_ (no bars, no ANSI codes)
	void write_file(std::deque<Message> const &messages) noexcept;
};

struct AsyncOutput::Message
{
	Component const *component = nullptr;
	std::string message;
};

enum class AsyncOutput::Level
{
	off,
	critical,
	error,
	warning,
	info,
	debug,
	trace
};

class AsyncOutput::Component
{
	// NOTE: 'Component' does not have a null-state. It is only created by
	// AsyncOutput and lives as long as that parent object.

	AsyncOutput &output_;
	std::string name_;
	relaxed_atomic<Level> level_{Level::info};
	relaxed_atomic<Clock::duration> total_time_;

	explicit Component(AsyncOutput &output, std::string name);
	friend class AsyncOutput;

  public:
	std::string_view name() const noexcept;
	Level level() const noexcept;
	Clock::duration elapsed() const noexcept;

	// log a message prefixed by this components name. No-op if level is below
	// configured.
	void do_log(Level l, std::string_view msg);
};

class AsyncOutput::Scope
{
	// NOTE: 'Scope' does have a null-state, indicated by 'component_' being
	// nullptr. Thats useful as:
	//   * Time-accounting is otherwise tied to Scope's lifetime, setting a
	//     scope to null-state will stop timing explicitly.
	//   * In the null-state, all logging is a silent no-op.

	relaxed_atomic<Component *> component_ = nullptr;
	Clock::time_point const start_time_ = Clock::now();

	explicit Scope(Component *component) noexcept;

	friend class AsyncOutput;

  public:
	Scope();
	~Scope() noexcept;

	// Returns the component associated with this scope, or nullptr if the scope
	// is in the null-state.
	// note: component lifetime is tied to AsyncOutput, so this pointer remains
	// valid independent of the scope.
	Component *component() const noexcept;

	void finish() noexcept;

	// dont copy (would mess with time accounting). No need for move.
	Scope(Scope const &) = delete;
	Scope &operator=(Scope const &) = delete;
	Scope(Scope &&other) = delete;
	Scope &operator=(Scope &&other) = delete;

	// Returns the elapsed time since the scope was created.
	std::chrono::steady_clock::duration elapsed() const noexcept;

	// same as 'elapsed()' but in seconds.
	double secs() const noexcept;

	// log a message at specified level. This is a no-op if the configured log
	// level is lower than the specified level.
	template <typename... Args>
	void log(Level level, fmt::format_string<Args...> format,
	         Args &&...args) const
	{
		auto comp = component();
		if (!comp || level > comp->level())
			return;
		comp->do_log(level, fmt::format(format, std::forward<Args>(args)...));
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

class AsyncOutput::Bar
{
	relaxed_atomic<BarState *> state_ = nullptr;

	explicit Bar(BarState *state) noexcept;

	friend class AsyncOutput;

  public:
	Bar() noexcept;
	~Bar() noexcept;
	void finish() noexcept;

	Bar(Bar const &) = delete;
	Bar &operator=(Bar const &) = delete;
	Bar(Bar &&other) noexcept;
	Bar &operator=(Bar &&other) noexcept;

	void set_total(uint64_t total) noexcept;
	void set_ticks(uint64_t ticks) noexcept;
	void increment(uint64_t ticks = 1) noexcept;
	uint64_t ticks() const noexcept;
	uint64_t total() const noexcept;
};

struct AsyncOutput::BarState
{
	std::string label;
	relaxed_atomic<uint64_t> ticks{0};
	relaxed_atomic<uint64_t> total{0};
	relaxed_atomic<bool> finished{false};
	Clock::time_point start_time = Clock::now();

	BarState(uint64_t total_, std::string label_);

	std::string format(int line_width) const;
};

} // namespace util
