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
		using namespace std::chrono;

		auto p = (double)ticks_ / total_;
		p = std::max(std::min(p, 1.0), 0.0);

		clock::time_point now = clock::now();
		auto elapsed = now - startTime_;
		auto eta = (1.0 - p) / p * elapsed;

		std::string head =
		    fmt::format("{:3}% ({} of {}) |", int(p * 100), ticks_, total_);
		std::string tail =
		    finished_
		        ? fmt::format("| elapsed: {:%T}               \r",
		                      floor<seconds>(elapsed))
		        : fmt::format("| elapsed: {:%T}, ETA: {:%T}\r",
		                      floor<seconds>(elapsed), floor<seconds>(eta));
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
	void operator++() { ++ticks_; }
	size_t total() const { return total_; }
};

/** allows iteration as `for(size_t i : ProgressRange(100))` */
class ProgressRange
{
	struct iterator
	{
		ProgressRange &pr;
		size_t current;

		iterator(ProgressRange &p, int c) : pr(p), current(c){};
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
	iterator begin() { return iterator(*this, 0); }
	iterator end() { return iterator(*this, pb.total()); }
};

} // namespace util
