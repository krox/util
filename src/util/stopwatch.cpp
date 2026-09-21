#include "util/stopwatch.h"

#include "fmt/format.h"
#include "fmt/ranges.h"

#include <algorithm>
#include <stdexcept>

namespace util {

void TimeStack::push(std::string_view label)
{
	auto now = Clock::now();
	if (!curr_.empty())
		timings_[curr_] += now - last_;
	last_ = now;
	curr_.push_back(std::string(label));

	// ensure the current scope exists in the timings map
	timings_[curr_] += Clock::duration::zero();
}

void TimeStack::pop()
{
	if (curr_.empty())
		throw std::runtime_error("pop from empty TimeStack");
	auto now = Clock::now();
	timings_[curr_] += now - last_;
	last_ = now;
	curr_.pop_back();
}

std::string TimeStack::summary() const
{
	util::vector<std::pair<Scope, Clock::duration>> entries;
	for (auto &[scope, duration] : timings_)
		entries.emplace_back(scope, duration);

	Scope last;
	util::vector<std::pair<std::string, double>> flat;
	double total_secs = 0.0;
	size_t max_length = 0;
	for (auto const &[scope, duration] : entries)
	{
		size_t indent = 0;
		while (indent < scope.size() && indent < last.size() &&
		       scope[indent] == last[indent])
			++indent;
		last = scope;

		auto section =
		    fmt::format("{:>{}}{}", "", indent * 2,
		                fmt::join(std::span(scope).subspan(indent), "/"));
		auto elapsed = duration;
		if (scope == curr_)
			elapsed += Clock::now() - last_;
		double secs =
		    std::chrono::duration_cast<std::chrono::duration<double>>(elapsed)
		        .count();
		max_length = std::max(max_length, section.size());
		flat.emplace_back(std::move(section), secs);
		total_secs += secs;
	}

	std::string result;
	for (auto const &[section, secs] : flat)
	{
		if (secs >= 0.01)
			result += fmt::format("{:<{}} : {:7.2f} s   ({:5.2f} %)\n", section,
			                      max_length, secs, secs / total_secs * 100.0);
		else
			result += fmt::format("{}\n", section);
	}
	return result;
}

void TimeStack::clear()
{
	timings_.clear();
	curr_.clear();
	last_ = Clock::now();
}

} // namespace util