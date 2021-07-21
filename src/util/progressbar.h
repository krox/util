#pragma once

#include <chrono>
#include <fmt/chrono.h>
#include <fmt/format.h>

namespace util {

class ProgressBar
{
	using clock = std::chrono::steady_clock;
	size_t total_;
	size_t ticks_ = 0;
	bool finished_ = false;
	const clock::time_point startTime_ = clock::now();

  public:
	ProgressBar(size_t total) : total_(total) {}

	void show() const
	{
		auto p = (double)ticks_ / total_;
		p = std::max(std::min(p, 1.0), 0.0);

		clock::time_point now = clock::now();
		auto elapsed = now - startTime_;
		auto eta = (1.0 - p) / p * elapsed;

		std::string head =
		    fmt::format("{:3}% ({} of {}) |", int(p * 100), ticks_, total_);
		std::string tail =
		    finished_
		        ? fmt::format("| elapsed: {:%T}               \r", elapsed)
		        : fmt::format("| elapsed: {:%T}, ETA: {:%T}\r", elapsed, eta);
		fmt::print(head);
		int width = std::max(10, int(80 - head.size() - tail.size()));
		int pos = int(width * p);
		for (int i = 0; i < pos; ++i)
			fmt::print("#");
		for (int i = pos; i < width; ++i)
			fmt::print(" ");
		fmt::print(tail);
		fflush(stdout);
	}
	void finish()
	{
		finished_ = true;
		show();
		fmt::print("\n");
	}
	void update(size_t ticks) { ticks_ = ticks; }
	void operator++() { ticks_++; }
};

} // namespace util
