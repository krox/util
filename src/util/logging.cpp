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
	auto &component = (*this)[name];
	auto state = std::make_shared<ScopeState>(&component, std::string(name),
	                                          component.level());
	{
		auto lock = std::unique_lock(mutex_);
		scopes_.push_back(state);
	}

	cv_.notify_one();
	return Scope(std::move(state));
}

Logger::Scope::Scope() = default;

Logger::Scope::Scope(std::shared_ptr<ScopeState> state) noexcept
    : state_(std::move(state))
{}

Logger::Scope::~Scope() noexcept { finish(); }

Logger::Scope::Scope(Scope &&other) noexcept : state_(std::move(other.state_))
{}

Logger::Scope &Logger::Scope::operator=(Scope &&other) noexcept
{
	if (this == &other)
		return *this;
	finish();
	state_ = std::move(other.state_);
	return *this;
}

Logger::Component *Logger::Scope::component() const noexcept
{
	if (!state_ || state_->finished.load())
		return nullptr;
	return state_->component;
}

Logger::Level Logger::Scope::level() const noexcept
{
	if (state_)
		return state_->level.load();
	else
		return Level::info;
}

void Logger::Scope::set_level(Level l) noexcept
{
	if (state_)
		state_->level.store(l);
}

uint64_t Logger::Scope::ticks() const noexcept
{
	if (state_)
		return state_->ticks.load();
	else
		return 0;
}

uint64_t Logger::Scope::total() const noexcept
{
	if (state_)
		return state_->total.load();
	else
		return 0;
}

void Logger::Scope::set_ticks(uint64_t ticks_value) noexcept
{
	if (state_)
		state_->ticks.store(ticks_value);
}

void Logger::Scope::set_total(uint64_t total_value) noexcept
{
	if (state_)
		state_->total.store(total_value);
}

void Logger::Scope::increment(uint64_t tick_count) noexcept
{
	if (state_)
		state_->ticks.fetch_add(tick_count);
}

void Logger::Scope::finish() noexcept
{
	if (!state_)
		return;

	auto elapsed_time = Clock::now() - state_->start_time;
	if (!state_->finished.exchange(true))
	{
		state_->finished_elapsed.store(elapsed_time);
		if (state_->component)
			state_->component->total_time_ += elapsed_time;
	}
}

std::chrono::steady_clock::duration Logger::Scope::elapsed() const noexcept
{
	if (!state_)
		return Clock::duration::zero();
	return state_->elapsed();
}

double Logger::Scope::secs() const noexcept
{
	return std::chrono::duration<double>(elapsed()).count();
}

Logger::ScopeState::ScopeState(Component *component_, std::string label_,
                               Level level_) noexcept
    : component(component_), label(std::move(label_)), level(level_)
{}

Logger::Clock::duration Logger::ScopeState::elapsed() const noexcept
{
	if (finished.load())
		return finished_elapsed.load();
	return Clock::now() - start_time;
}

size_t Logger::ScopeState::line_count() const noexcept
{
	return total.load() == 0 ? 1 : 2;
}

std::string Logger::ScopeState::format(int line_width) const
{
	auto tcks = ticks.load();
	auto ttl = total.load();
	double progress = (ttl > 0) ? static_cast<double>(tcks) / ttl : 0.0;
	auto elapsed_time = elapsed();
	auto elapsed_secs = std::chrono::duration<double>(elapsed_time).count();
	auto eta = (progress > 0.0)
	               ? std::chrono::duration<double>(elapsed_secs *
	                                               (1.0 - progress) / progress)
	               : std::chrono::duration<double>(0.0);

	auto line1 =
	    pad_string(label,
	               fmt::format("elapsed: {:%T}",
	                           std::chrono::duration_cast<std::chrono::seconds>(
	                               elapsed_time)),
	               line_width);
	if (ttl == 0)
		return line1;

	auto line2 =
	    fmt::format("] {:6.2f}% {}/{} ETA: {:%T}", progress * 100.0, tcks, ttl,
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
	Message message{component, std::string(msg), {}};
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
	std::vector<std::shared_ptr<ScopeState>> scopes;
	while (true)
	{
		messages.clear();
		scopes.clear();

		// note: critical section only drains the messages and observe live
		// scopes. Printing is done outside the lock.
		// Printing is done outside the lock
		{
			auto lock = std::unique_lock(mutex_);
			cv_.wait_for(lock, interval_, [&] {
				return stop.stop_requested() || !msg_queue_.empty();
			});

			std::swap(messages, msg_queue_);

			std::erase_if(scopes_, [](auto const &scope) {
				return scope == nullptr || scope->finished.load();
			});

			scopes.reserve(scopes_.size());
			for (auto const &scope : scopes_)
				scopes.push_back(scope);
		}

		if (!messages.empty() || !scopes.empty() || rendered_lines_ != 0)
			write_terminal(messages, scopes);
		if (log_file_ && !messages.empty())
			write_file(messages);

		for (auto &msg : messages)
			if (msg.completion)
				msg.completion->set_value();

		if (stop.stop_requested())
			break;
	}
}

void Logger::write_terminal(
    std::deque<Message> const &messages,
    std::vector<std::shared_ptr<ScopeState>> const &scopes) noexcept
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
	for (auto const &scope : scopes)
	{
		fmt::format_to(std::back_inserter(frame), "{}\n",
		               scope->format(line_width_));
		lines += scope->line_count();
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
