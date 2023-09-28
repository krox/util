#pragma once

#include <cassert>
#include <chrono>

namespace util {

// Simple Stopwatch class for performance measruements
//     * .stop() must be called once each call of .start() in order to stop
//     * .secs() is valid even while running
class Stopwatch
{
	using Clock = std::chrono::steady_clock;

	int running_ = 0;
	typename Clock::time_point last_; // valid if running
	typename Clock::duration dur_ = Clock::duration::zero();

  public:
	Stopwatch() = default;

	bool running() const { return running_; }

	Stopwatch &start()
	{
		assert(running_ >= 0);
		if (running_++ == 0)
			last_ = Clock::now();
		return *this;
	}

	Stopwatch &stop()
	{
		assert(running_ >= 0);
		if (--running_ == 0)
			dur_ += Clock::now() - last_;
		return *this;
	}

	Stopwatch &reset()
	{
		assert(running_ == 0);
		dur_ = Clock::duration::zero();
		return *this;
	}

	double secs() const
	{
		typename Clock::duration dur = dur_;
		if (running_)
			dur += Clock::now() - last_;
		return std::chrono::duration_cast<std::chrono::microseconds>(dur)
		           .count() *
		       1.0e-6;
	}
};

// RAII-style scope guard for benchmarking blocks of code
class StopwatchGuard
{
	Stopwatch &sw_;

  public:
	StopwatchGuard(Stopwatch &sw) : sw_(sw) { sw_.start(); }
	~StopwatchGuard() { sw_.stop(); }

	StopwatchGuard(const StopwatchGuard &) = delete;
	StopwatchGuard &operator=(const StopwatchGuard &) = delete;
};

} // namespace util
