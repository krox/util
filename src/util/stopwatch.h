#ifndef UTIL_STOPWATCH_H
#define UTIL_STOPWATCH_H

#include <cassert>
#include <chrono>

namespace util {

/** Simple Stopwatch class for performance measruements */
class Stopwatch
{
	using Clock = std::chrono::steady_clock;

	bool running_ = false;
	typename Clock::time_point last_; // valid if running
	typename Clock::duration dur_ = Clock::duration::zero();

  public:
	Stopwatch() {}

	bool running() const { return running_; }

	Stopwatch &start()
	{
		if (!running_)
		{
			running_ = true;
			last_ = Clock::now();
		}
		return *this;
	}

	Stopwatch &stop()
	{
		if (running_)
		{
			running_ = false;
			dur_ += Clock::now() - last_;
		}
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

/** RAII-style scope guard for benchmarking blocks of code */
class StopwatchGuard
{
	Stopwatch &sw;

  public:
	StopwatchGuard(Stopwatch &sw) : sw(sw)
	{
		assert(!sw.running());
		sw.start();
	}

	~StopwatchGuard()
	{
		assert(sw.running());
		sw.stop();
	}

	StopwatchGuard(const StopwatchGuard &) = delete;
	StopwatchGuard &operator=(const StopwatchGuard &) = delete;
};

} // namespace util

#endif
