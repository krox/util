#pragma once

#include <atomic>
#include <chrono>
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
	static constexpr auto interval_ = std::chrono::milliseconds(100);
	static constexpr int line_width_ = 80;

	class Bar
	{
		friend class AsyncOutput;
		using Clock = std::chrono::steady_clock;

		std::string label_;
		std::atomic<uint64_t> ticks_{0};
		std::atomic<uint64_t> total_{0};
		Clock::time_point start_time_ = Clock::now();

		std::string format(int line_width) const;

	  public:
		Bar(uint64_t total, std::string label);

		void set_total(uint64_t total)
		{
			total_.store(total, std::memory_order_relaxed);
		}
		void set_ticks(uint64_t ticks)
		{
			ticks_.store(ticks, std::memory_order_relaxed);
		}
		void increment(uint64_t ticks = 1)
		{
			ticks_.fetch_add(ticks, std::memory_order_relaxed);
		}

		uint64_t ticks() const
		{
			return ticks_.load(std::memory_order_relaxed);
		}
		uint64_t total() const
		{
			return total_.load(std::memory_order_relaxed);
		}
	};

	AsyncOutput();
	~AsyncOutput();

	AsyncOutput(AsyncOutput const &) = delete;
	AsyncOutput &operator=(AsyncOutput const &) = delete;
	AsyncOutput(AsyncOutput &&) = delete;
	AsyncOutput &operator=(AsyncOutput &&) = delete;

	std::shared_ptr<Bar> add_bar(uint64_t total, std::string label);
	void log(std::string msg);

	// Add a log message
	template <typename... Args>
	void operator()(fmt::format_string<Args...> format, Args &&...args)
	{
		log(fmt::format(format, std::forward<Args>(args)...));
	}

  private:
	std::mutex mutex_;
	std::condition_variable cv_;
	std::deque<std::string> msg_queue_;
	std::vector<std::weak_ptr<Bar>> bars_;
	std::jthread thread_;
	size_t rendered_lines_ = 0;

	void thread_main(std::stop_token stop);
	std::vector<std::shared_ptr<Bar>> snapshot_bars();
	void redraw(std::deque<std::string> messages,
	            std::vector<std::shared_ptr<Bar>> const &bars);
};

} // namespace util
