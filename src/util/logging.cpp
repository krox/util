#include "util/logging.h"

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

Logger::Component::Component(Logger &output, std::string name)
    : output_(output), name_(std::move(name))
{}

std::string_view Logger::Component::name() const noexcept { return name_; }

Logger::Level Logger::Component::level() const noexcept
{
	return level_.load();
}

void Logger::Component::set_level(Level l) noexcept { level_.store(l); }

Logger::Clock::duration Logger::Component::elapsed() const noexcept
{
	return total_time_.load();
}

void Logger::Component::do_log(Level l, std::string_view msg)
{
	if (l <= level())
		output_.do_log(this, l, msg);
}

Logger::Component &Logger::operator[](std::string_view component_name)
{
	auto lock = std::unique_lock(mutex_);
	for (auto &c : components_)
		if (c->name() == component_name)
			return *c;

	auto component = std::unique_ptr<Component>(
	    new Component(*this, std::string(component_name)));
	component->set_level(default_level_);
	auto &ref = *component;
	components_.push_back(std::move(component));
	return ref;
}

void Logger::set_level(Level level)
{
	auto lock = std::unique_lock(mutex_);
	default_level_ = level;
	for (auto &component : components_)
		component->set_level(level);
}

Logger::Scope Logger::scope(std::string_view name)
{
	return Scope(&(*this)[name]);
}

Logger::Scope::Scope() = default;

Logger::Scope::Scope(Component *component) noexcept : component_(component) {}

Logger::Scope::~Scope() noexcept { finish(); }

Logger::Component *Logger::Scope::component() const noexcept
{
	return component_.load();
}

void Logger::Scope::finish() noexcept
{
	auto *comp = component_.exchange(nullptr);
	if (comp)
		comp->total_time_ += Clock::now() - start_time_;
}

std::chrono::steady_clock::duration Logger::Scope::elapsed() const noexcept
{
	return Clock::now() - start_time_;
}

double Logger::Scope::secs() const noexcept
{
	return std::chrono::duration<double>(elapsed()).count();
}

Logger::Bar::Bar() noexcept = default;

Logger::Bar::Bar(BarState *state) noexcept : state_(state) {}

void Logger::Bar::finish() noexcept
{
	auto *state = state_.exchange(nullptr);
	if (state)
		state->finished.store(true);
}

Logger::Bar::~Bar() noexcept { finish(); }

Logger::Bar::Bar(Bar &&other) noexcept : state_(other.state_.exchange(nullptr))
{}

Logger::Bar &Logger::Bar::operator=(Bar &&other) noexcept
{
	if (this == &other)
		return *this;
	finish();
	state_.store(other.state_.exchange(nullptr));
	return *this;
}

void Logger::Bar::set_total(uint64_t total) noexcept
{
	if (auto *state = state_.load())
		state->total.store(total);
}

void Logger::Bar::set_ticks(uint64_t ticks) noexcept
{
	if (auto *state = state_.load())
		state->ticks.store(ticks);
}

void Logger::Bar::increment(uint64_t ticks) noexcept
{
	if (auto *state = state_.load())
		state->ticks += ticks;
}

uint64_t Logger::Bar::ticks() const noexcept
{
	if (auto *state = state_.load())
		return state->ticks.load();
	return 0;
}

uint64_t Logger::Bar::total() const noexcept
{
	if (auto *state = state_.load())
		return state->total.load();
	return 0;
}

Logger::BarState::BarState(uint64_t total_, std::string label_)
    : label(std::move(label_)), total(total_)
{}

std::string Logger::BarState::format(int line_width) const
{
	auto tcks = ticks.load();
	auto ttl = total.load();
	double progress = (ttl > 0) ? static_cast<double>(tcks) / ttl : 0.0;
	auto elapsed = Clock::now() - start_time;
	auto elapsed_secs = std::chrono::duration<double>(elapsed).count();
	auto eta = (progress > 0.0)
	               ? std::chrono::duration<double>(elapsed_secs *
	                                               (1.0 - progress) / progress)
	               : std::chrono::duration<double>(0.0);

	auto line1 =
	    pad_string(label, fmt::format("{} of {}", tcks, ttl), line_width);
	auto line2 =
	    fmt::format("] {:6.2f}% elapsed: {:%T} ETA: {:%T}", progress * 100.0,
	                std::chrono::duration_cast<std::chrono::seconds>(elapsed),
	                std::chrono::duration_cast<std::chrono::seconds>(eta));
	int pbar_width = std::max(0, line_width - display_width(line2) - 1);

	return fmt::format("{}\n[{}{}", line1, progress_bar(progress, pbar_width),
	                   line2);
}

Logger::Logger(std::string_view log_file)
    : default_level_(Level::info),
      log_file_(log_file.empty()
                    ? File{}
                    : File::create(log_file, /* overwrite = */ true)),
      thread_(&Logger::thread_main, this)
{
	// note: 'thread_' is already running at this point. So be wary of races
	// when putting code here.
}

Logger::~Logger()
{
	thread_.request_stop();
	cv_.notify_all();
	thread_.join();
	if (rendered_lines_ > 0)
		fmt::print(stdout, "\n");
}

Logger::Bar Logger::bar(uint64_t total, std::string label)
{
	auto state = std::make_unique<BarState>(total, std::move(label));
	auto result = Bar(state.get());

	{
		auto lock = std::unique_lock(mutex_);
		bars_.push_back(std::move(state));
	}

	cv_.notify_one();
	return result;
}

void Logger::print_summary()
{
	std::vector<Component const *> components;
	{
		auto lock = std::unique_lock(mutex_);
		components.reserve(components_.size());
		for (auto const &component : components_)
			components.push_back(component.get());
	}

	double total_secs =
	    std::chrono::duration<double>(Clock::now() - summary_start_).count();

	do_log(nullptr, Level::info,
	       "============================ time stats "
	       "=============================");
	for (auto const *component : components)
	{
		auto secs = std::chrono::duration<double>(component->elapsed()).count();
		do_log(nullptr, Level::info,
		       fmt::format("{:12}: {:#6.2f} s ({:5.1f} %)", component->name(),
		                   secs, (secs / total_secs) * 100.0));
	}
	do_log(nullptr, Level::info,
	       fmt::format("{:12}: {:#6.2f} s (100.0 %)", "total", total_secs));
}

void Logger::reset_summary()
{
	auto lock = std::unique_lock(mutex_);
	for (auto &component : components_)
		component->total_time_.store(Clock::duration::zero());
	summary_start_ = Clock::now();
}

void Logger::do_log(Component const *component, Level, std::string_view msg)
{
	// note: construct the message outside the lock, keeping the critical
	// section minimal
	Message message{component, std::string(msg)};
	{
		auto lock = std::unique_lock(mutex_);
		msg_queue_.push_back(std::move(message));
	}

	cv_.notify_one();
}

void Logger::flush() noexcept
{
	auto msg = Message{};
	msg.completion = std::promise<void>{};
	auto future = msg.completion->get_future();
	{
		auto lock = std::unique_lock(mutex_);
		msg_queue_.push_back(std::move(msg));
	}
	cv_.notify_one();
	future.wait();
}

void Logger::thread_main(std::stop_token stop)
{
	auto callback = std::stop_callback(stop, [this] { cv_.notify_all(); });

	std::deque<Message> messages;
	std::vector<BarState const *> bars;
	while (true)
	{
		messages.clear();
		bars.clear();

		// note: critical section only drains the messages and observe the bars.
		// Printing is done outside the lock
		{
			auto lock = std::unique_lock(mutex_);
			cv_.wait_for(lock, interval_, [&] {
				return stop.stop_requested() || !msg_queue_.empty();
			});

			std::swap(messages, msg_queue_);

			std::erase_if(bars_,
			              [](auto const &bar) { return bar->finished.load(); });

			bars.reserve(bars_.size());
			for (auto const &bar : bars_)
				bars.push_back(bar.get());
		}

		if (!messages.empty() || !bars.empty() || rendered_lines_ != 0)
			write_terminal(messages, bars);
		if (log_file_ && !messages.empty())
			write_file(messages);

		for (auto &msg : messages)
			if (msg.completion)
				msg.completion->set_value();

		if (stop.stop_requested())
			break;
	}
}

void Logger::write_terminal(std::deque<Message> const &messages,
                            std::vector<BarState const *> const &bars) noexcept
{
	fmt::memory_buffer frame;
	fmt::format_to(std::back_inserter(frame), "\x1b[?25l");

	if (rendered_lines_ > 0)
		fmt::format_to(std::back_inserter(frame), "\x1b[{}F", rendered_lines_);
	fmt::format_to(std::back_inserter(frame), "\x1b[J");

	for (auto const &message : messages)
	{
		if (message.message.empty())
			continue;
		if (message.component == nullptr)
			fmt::format_to(std::back_inserter(frame), "{}\n", message.message);
		else
			fmt::format_to(std::back_inserter(frame), "[{}] {}\n",
			               message.component->name(), message.message);
	}

	size_t lines = 0;
	for (auto const *bar : bars)
	{
		fmt::format_to(std::back_inserter(frame), "{}\n",
		               bar->format(line_width_));
		lines += 2;
	}
	fmt::format_to(std::back_inserter(frame), "\x1b[?25h");

	if (frame.size() != 0)
		std::fwrite(frame.data(), 1, frame.size(), stdout);
	std::fflush(stdout);
	rendered_lines_ = lines;
}

void Logger::write_file(std::deque<Message> const &messages) noexcept
{
	fmt::memory_buffer log;

	for (auto const &message : messages)
	{
		if (message.message.empty())
			continue;
		if (message.component == nullptr)
			fmt::format_to(std::back_inserter(log), "{}\n", message.message);
		else
			fmt::format_to(std::back_inserter(log), "[{}] {}\n",
			               message.component->name(), message.message);
	}

	try
	{
		log_file_.write_raw(log.data(), log.size());
		log_file_.flush();
	}
	catch (...)
	{
		// swallow errors writing to the log file: this runs on the
		// background thread and there is nothing meaningful we could do with
		// an exception here anyway.
	}
}

} // namespace util
