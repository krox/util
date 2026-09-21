#include "util/logging.h"
#include "util/functional.h"
#include "util/string.h"
#include "util/vector.h"
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <fmt/chrono.h>
#include <functional>
#include <memory>
#include <thread>
#include <unistd.h>

namespace util {

namespace {
std::string pad_string(std::string_view left, std::string_view right, int width)
{
	int pad = width - display_width(left) - display_width(right);
	return fmt::format("{}{:>{}}{}", left, "", std::max(pad, 0), right);
}

std::string progress_bar(double progress, int width)
{
	if (width <= 0)
		return "";

	static constexpr std::string_view blocks[] = {" ", "▏", "▎", "▍", "▌",
	                                              "▋", "▊", "▉", "█"};
	int eighths =
	    std::clamp(static_cast<int>(progress * width * 8.0), 0, 8 * width);
	int full = eighths / 8;
	int frac = eighths % 8;

	std::string result;
	result.reserve(width * 3);
	for (int i = 0; i < full; ++i)
		result += blocks[8];
	if (frac != 0)
		result += blocks[frac];
	for (int i = full + (frac != 0 ? 1 : 0); i < width; ++i)
		result += blocks[0];
	return result;
}

} // namespace

void Terminal::prepare_frame()
{
	frame_.clear();
	auto it = std::back_inserter(frame_);

	// early out to avoid thrashing with useless control sequences
	if (msg_queue_.empty() && sections_.empty() && rendered_lines_ == 0)
		return;

	append(frame_, "\x1b[?25l"); // hide cursor
	if (rendered_lines_)         // move cursor up
		fmt::format_to(it, "\x1b[{}F", rendered_lines_);
	append(frame_, "\x1b[J"); // clear from cursor

	// messages
	for (auto &msg : msg_queue_)
	{
		append(frame_, msg);
		if (msg.empty() || msg.back() != '\n')
			frame_.push_back('\n');
	}
	msg_queue_.clear();

	// dynamic lines
	rendered_lines_ = 0;
	auto sink = [&](std::string_view line) {
		append(frame_, line);
		frame_.push_back('\n');
		++rendered_lines_;
	};
	int width = 80;
	for (auto &section : sections_)
		(*section)(sink, width);

	append(frame_, "\x1b[?25h"); // show cursor
}

void Terminal::thread_main(std::stop_token stop)
{
	std::stop_callback on_stop(stop, [this] { cv_.notify_all(); });
	auto lock = std::unique_lock(mutex_);

	while (true)
	{
		prepare_frame();
		// note: actual write should be done while holding the lock.
		// Otherwise '.flush()' might return too early.
		if (!frame_.empty())
		{
			auto written = ::write(STDOUT_FILENO, frame_.data(), frame_.size());
			(void)written;
		}
		cv_.notify_all(); // <- wakes up anyone who is waiting in '.flush'
		if (stop.stop_requested())
			break;

		cv_.wait_for(lock, interval_);
	}
}

void Terminal::Section::close() noexcept
{
	auto *terminal = std::exchange(terminal_, nullptr);
	auto *print = std::exchange(print_, nullptr);
	if (terminal == nullptr)
		return;
	{
		auto lock = std::unique_lock(terminal->mutex_);
		erase_if(terminal->sections_, [&](std::unique_ptr<print_t> const &p) {
			return p.get() == print;
		});
	}
	terminal->cv_.notify_one();
}

Logger::Scope::Scope(Logger *logger, std::string name, Level level)
    : logger_(logger), name_(std::move(name)), level_(level)
{
	assert(logger_ != nullptr);
	logger_->time_stack_.lock()->push(name_);
	auto printer = [this](auto sink, int width) { print(sink, width); };
	terminal_section_ = logger_->terminal_.section(printer);
}

void Logger::Scope::close() noexcept
{
	auto *logger = logger_;
	if (!logger)
		return;

	terminal_section_.close();

	auto ts = logger->time_stack_.lock();
	if (ts->stack().empty() || ts->stack().back() != name_)
	{
		ts.unlock();
		logger->do_log(
		    fmt::format("[{}] Time stack corrupted: expected top to be '{}'",
		                name_, name_));
		logger_ = nullptr;
		return;
	}

	ts->pop();
	logger_ = nullptr;
}

void Logger::Scope::print(Terminal::line_sink sink, int width)
{
	using std::chrono::duration_cast;
	using std::chrono::seconds;

	auto elap = elapsed();
	sink(pad_string(name(),
	                fmt::format("elapsed: {:%T}", duration_cast<seconds>(elap)),
	                width));

	if (total() == 0)
		return;
	double progress = static_cast<double>(ticks()) / total();
	std::string suffix;
	if (0.0 < progress && progress <= 1.0)
	{
		auto eta = elap * ((1.0 - progress) / progress);
		suffix = fmt::format("] {:6.2f}% {}/{} ETA: {:%T}", progress * 100.0,
		                     ticks(), total(), duration_cast<seconds>(eta));
	}
	else
	{
		suffix = fmt::format("] {:6.2f}% {}/{} ETA: --:--:--", progress * 100.0,
		                     ticks(), total());
	}
	int pbar_width = std::max(0, width - display_width(suffix) - 1);

	sink(fmt::format("[{}{}", progress_bar(progress, pbar_width), suffix));
}

} // namespace util
