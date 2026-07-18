#include "util/progressbar.h"

#include "util/string.h"
#include <algorithm>
#include <cstdio>
#include <fmt/chrono.h>

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

AsyncOutput::Bar::Bar(uint64_t total, std::string label)
    : label_(std::move(label)), total_(total)
{}

std::string AsyncOutput::Bar::format(int line_width) const
{
	auto tcks = ticks();
	auto ttl = total();
	double progress = (ttl > 0) ? static_cast<double>(tcks) / ttl : 0.0;
	auto elapsed = Clock::now() - start_time_;
	auto elapsed_secs = std::chrono::duration<double>(elapsed).count();
	auto eta = (progress > 0.0)
	               ? std::chrono::duration<double>(elapsed_secs *
	                                               (1.0 - progress) / progress)
	               : std::chrono::duration<double>(0.0);

	auto line1 =
	    pad_string(label_, fmt::format("{} of {}", tcks, ttl), line_width);
	auto line2 =
	    fmt::format("] {:6.2f}% elapsed: {:%T} ETA: {:%T}", progress * 100.0,
	                std::chrono::duration_cast<std::chrono::seconds>(elapsed),
	                std::chrono::duration_cast<std::chrono::seconds>(eta));
	int pbar_width = std::max(0, line_width - display_width(line2) - 1);

	return fmt::format("{}\n[{}{}", line1, progress_bar(progress, pbar_width),
	                   line2);
}

AsyncOutput::AsyncOutput() : thread_(&AsyncOutput::thread_main, this) {}

AsyncOutput::~AsyncOutput()
{
	thread_.request_stop();
	cv_.notify_all();
	thread_.join();
	if (rendered_lines_ > 0)
		fmt::print(stdout, "\n");
}

std::shared_ptr<AsyncOutput::Bar> AsyncOutput::add_bar(uint64_t total,
                                                       std::string label)
{
	auto bar = std::make_shared<Bar>(total, std::move(label));
	{
		auto lock = std::unique_lock(mutex_);
		bars_.push_back(bar);
	}
	cv_.notify_one();
	return bar;
}

void AsyncOutput::log(std::string msg)
{
	{
		auto lock = std::unique_lock(mutex_);
		msg_queue_.push_back(std::move(msg));
	}
	cv_.notify_one();
}

void AsyncOutput::thread_main(std::stop_token stop)
{
	auto callback = std::stop_callback(stop, [this] { cv_.notify_all(); });

	while (true)
	{
		std::deque<std::string> messages;
		std::vector<std::shared_ptr<Bar>> bars;
		{
			auto lock = std::unique_lock(mutex_);
			cv_.wait_for(lock, interval_, [&] {
				return stop.stop_requested() || !msg_queue_.empty();
			});
			messages.swap(msg_queue_);
			bars = snapshot_bars();
		}

		if (!messages.empty() || !bars.empty() || rendered_lines_ != 0)
			redraw(std::move(messages), bars);

		if (stop.stop_requested())
			break;
	}
}

std::vector<std::shared_ptr<AsyncOutput::Bar>> AsyncOutput::snapshot_bars()
{
	std::vector<std::shared_ptr<Bar>> live_bars;
	live_bars.reserve(bars_.size());

	auto out = bars_.begin();
	for (auto it = bars_.begin(); it != bars_.end(); ++it)
	{
		if (auto bar = it->lock())
		{
			live_bars.push_back(std::move(bar));
			*out++ = *it;
		}
	}
	bars_.erase(out, bars_.end());
	return live_bars;
}

void AsyncOutput::redraw(std::deque<std::string> messages,
                         std::vector<std::shared_ptr<Bar>> const &bars)
{
	if (rendered_lines_ > 0)
		fmt::print(stdout, "\x1b[{}F", rendered_lines_);
	fmt::print(stdout, "\x1b[J");

	for (auto const &message : messages)
		fmt::print(stdout, "{}\n", message);

	size_t lines = 0;
	for (auto const &bar : bars)
	{
		fmt::print(stdout, "{}\n", bar->format(line_width_));
		lines += 2;
	}

	std::fflush(stdout);
	rendered_lines_ = lines;
}

} // namespace util