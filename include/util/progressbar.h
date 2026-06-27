#pragma once

#include <chrono>
#include <fmt/chrono.h>
#include <fmt/format.h>
#include <string>

namespace util {

// Progress bar utilities. Renders a two-line progress bar to stderr using
// ANSI escape sequences (requires a modern terminal).
//
// Layout:
//   [##########          ] 42% (420 of 1000)
//   elapsed: 00:01:23, ETA: 00:02:10  optional status message
//
// Usage example:
//
//   // Manual control
//   util::ProgressBar pb(100);
//   pb.show();
//   for (size_t i = 0; i < 100; ++i) {
//       doWork(i);
//       pb.set_message(fmt::format("item {}", i));
//       ++pb;
//       pb.show();
//   }
//   // pb.finish() is called automatically by the destructor if omitted
//
//   // Range-based for (automatically shows and finishes)
//   for (size_t i : util::ProgressRange(100))
//       doWork(i);

class ProgressBar
{
	using clock = std::chrono::steady_clock;
	using time_point = clock::time_point;
	using duration = clock::duration;

	size_t total_;
	size_t ticks_ = 0;
	bool finished_ = false;
	bool firstDraw_ = true;
	std::string message_;
	const clock::time_point startTime_ = clock::now();

	void drawLines(bool final)
	{
		using namespace std::chrono;

		auto p = (total_ > 0) ? (double)ticks_ / total_ : 0.0;
		p = std::max(std::min(p, 1.0), 0.0);

		auto elapsed = clock::now() - startTime_;

		// Line 1: [bar] pct% (ticks of total)
		static constexpr int kBarWidth = 40;
		int pos = int(kBarWidth * p);
		std::string bar(kBarWidth, ' ');
		for (int i = 0; i < pos; ++i)
			bar[i] = '#';
		std::string line1 = fmt::format("[{}] {:3}% ({} of {})", bar,
		                                int(p * 100), ticks_, total_);

		// Line 2: elapsed / ETA + optional message
		std::string line2 =
		    (final || p == 0)
		        ? fmt::format("elapsed: {:%T}", floor<seconds>(elapsed))
		        : fmt::format("elapsed: {:%T}, ETA: {:%T}",
		                      floor<seconds>(elapsed),
		                      floor<seconds>((1.0 - p) / p * elapsed));
		if (!message_.empty())
			line2 += "  " + message_;

		// Move cursor back to line 1 on all redraws after the first
		if (!firstDraw_)
			fmt::print(stderr, "\x1b[1A\r");

		// Erase each line before writing. End with \r on line 2 (not \n) so
		// the next redraw can navigate back up with a single \x1b[1A.
		fmt::print(stderr, "\x1b[2K{}\n\x1b[2K{}{}", line1, line2,
		           final ? "\n" : "\r");
		fflush(stderr);
		firstDraw_ = false;
	}

  public:
	ProgressBar(size_t total) : total_(total) {}
	~ProgressBar()
	{
		if (!finished_)
			finish();
	}

	// Non-copyable: the bar owns terminal cursor state
	ProgressBar(const ProgressBar &) = delete;
	ProgressBar &operator=(const ProgressBar &) = delete;

	void show() { drawLines(false); }
	void finish()
	{
		finished_ = true;
		drawLines(true);
	}
	void set_message(std::string msg) { message_ = std::move(msg); }
	void update(size_t ticks) { ticks_ = ticks; }
	void operator++() { ++ticks_; }
	size_t total() const { return total_; }

	// estimate for average time per item in seconds
	// (not a core feature, but nice to have as we are measuring time anyway)
	float secs_per_item() const
	{
		using namespace std::chrono;
		auto elapsed = clock::now() - startTime_;
		return (float)duration_cast<milliseconds>(elapsed).count() / 1000.0f /
		       (float)ticks_;
	}
};

// allows iteration as `for(size_t i : ProgressRange(100))`
class ProgressRange
{
	struct iterator
	{
		ProgressRange &pr;
		size_t current;

		iterator(ProgressRange &p, size_t c) : pr(p), current(c){};
		size_t operator*() const { return current; }
		void operator++()
		{
			++current;
			++pr.pb;
			pr.pb.show();
		}
		bool operator!=(iterator const &other)
		{
			assert(&pr == &other.pr);
			return current != other.current;
		}
	};
	ProgressBar pb;

  public:
	ProgressRange(size_t total) : pb(total) { pb.show(); }
	~ProgressRange() { pb.finish(); }
	iterator begin() { return iterator(*this, size_t(0)); }
	iterator end() { return iterator(*this, pb.total()); }
};

} // namespace util
