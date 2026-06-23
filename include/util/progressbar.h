#pragma once

#include <chrono>
#include <fmt/chrono.h>
#include <fmt/format.h>

namespace util {

// Progress bar utilities.
//
// Usage example:
//
//   // Manual control
//   util::ProgressBar pb(100);
//   for (size_t i = 0; i < 100; ++i) {
//       doWork(i);
//       ++pb;
//       pb.show();
//   }
//   pb.finish();
//
//   // Range-based for (automatically shows and finishes)
//   for (size_t i : util::ProgressRange(100))
//       doWork(i);

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
		using namespace std::chrono;

		auto p = (total_ > 0) ? (double)ticks_ / total_ : 0.0;
		p = std::max(std::min(p, 1.0), 0.0);

		clock::time_point now = clock::now();
		auto elapsed = now - startTime_;

		std::string head =
		    fmt::format("{:3}% ({} of {}) |", int(p * 100), ticks_, total_);
		std::string tail =
		    (finished_ || p == 0)
		        ? fmt::format("| elapsed: {:%T}               \r",
		                      floor<seconds>(elapsed))
		        : fmt::format("| elapsed: {:%T}, ETA: {:%T}\r",
		                      floor<seconds>(elapsed),
		                      floor<seconds>((1.0 - p) / p * elapsed));
		fmt::print(stderr, "{}", head);
		int barSpace = 80 - (int)head.size() - (int)tail.size();
		int width = std::max(10, barSpace);
		int pos = int(width * p);
		for (int i = 0; i < pos; ++i)
			fmt::print(stderr, "#");
		for (int i = pos; i < width; ++i)
			fmt::print(stderr, " ");
		fmt::print(stderr, "{}", tail);
		fflush(stderr);
	}
	void finish()
	{
		finished_ = true;
		show();
		fmt::print(stderr, "\n");
	}
	void update(size_t ticks) { ticks_ = ticks; }
	void operator++() { ++ticks_; }
	size_t total() const { return total_; }
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
