#pragma once

// Set theoretic functions (intersection, union, ...) for sorted ranges. Input
// ranges must be sorted in non-decreasing order. Output is emitted via
// callbacks for maximum flexibility.

#include <concepts>
#include <functional>
#include <ranges>
#include <utility>

namespace util {

// Emits elements present in both sorted iterator ranges.
void set_intersection(std::input_iterator auto it_a, auto end_a,
                      std::input_iterator auto it_b, auto end_b,
                      auto &&callback)
    requires std::sentinel_for<decltype(end_a), decltype(it_a)> &&
             std::sentinel_for<decltype(end_b), decltype(it_b)>
{
	while (it_a != end_a && it_b != end_b)
	{
		if (*it_a < *it_b)
			++it_a;
		else if (*it_b < *it_a)
			++it_b;
		else
		{
			std::invoke(callback, *it_a);
			++it_a;
			++it_b;
		}
	}
}

// Emits elements present in all three sorted iterator ranges.
void set_intersection3(std::input_iterator auto it_a, auto end_a,
                       std::input_iterator auto it_b, auto end_b,
                       std::input_iterator auto it_c, auto end_c,
                       auto &&callback)
    requires std::sentinel_for<decltype(end_a), decltype(it_a)> &&
             std::sentinel_for<decltype(end_b), decltype(it_b)> &&
             std::sentinel_for<decltype(end_c), decltype(it_c)>
{
	while (it_a != end_a && it_b != end_b && it_c != end_c)
	{
		if (*it_a < *it_b || *it_a < *it_c)
			++it_a;
		else if (*it_b < *it_a || *it_b < *it_c)
			++it_b;
		else if (*it_c < *it_a || *it_c < *it_b)
			++it_c;
		else
		{
			std::invoke(callback, *it_a);
			++it_a;
			++it_b;
			++it_c;
		}
	}
}

// Splits three-way matches from exactly-two-way matches across sorted iterator
// ranges.
void set_intersection3(std::input_iterator auto it_a, auto end_a,
                       std::input_iterator auto it_b, auto end_b,
                       std::input_iterator auto it_c, auto end_c,
                       auto &&callback3, auto &&callback2)
    requires std::sentinel_for<decltype(end_a), decltype(it_a)> &&
             std::sentinel_for<decltype(end_b), decltype(it_b)> &&
             std::sentinel_for<decltype(end_c), decltype(it_c)>
{
	while (true)
	{
		if (it_a == end_a)
		{
			set_intersection(it_b, end_b, it_c, end_c,
			                 std::forward<decltype(callback2)>(callback2));
			return;
		}
		if (it_b == end_b)
		{
			set_intersection(it_a, end_a, it_c, end_c,
			                 std::forward<decltype(callback2)>(callback2));
			return;
		}
		if (it_c == end_c)
		{
			set_intersection(it_a, end_a, it_b, end_b,
			                 std::forward<decltype(callback2)>(callback2));
			return;
		}

		if (*it_a == *it_b)
		{
			if (*it_a == *it_c)
			{
				std::invoke(callback3, *it_a);
				++it_a;
				++it_b;
				++it_c;
			}
			else if (*it_a < *it_c)
			{
				std::invoke(callback2, *it_a);
				++it_a;
				++it_b;
			}
			else
				++it_c;
		}
		else if (*it_a == *it_c)
		{
			if (*it_a < *it_b)
			{
				std::invoke(callback2, *it_a);
				++it_a;
				++it_c;
			}
			else
				++it_b;
		}
		else if (*it_b == *it_c)
		{
			if (*it_b < *it_a)
			{
				std::invoke(callback2, *it_b);
				++it_b;
				++it_c;
			}
			else
				++it_a;
		}
		else if (*it_a < *it_b && *it_a < *it_c)
			++it_a;
		else if (*it_b < *it_a && *it_b < *it_c)
			++it_b;
		else
			++it_c;
	}
}

// Emits the sorted union of both sorted iterator ranges.
void set_union(std::input_iterator auto it_a, auto end_a,
               std::input_iterator auto it_b, auto end_b, auto &&callback)
    requires std::sentinel_for<decltype(end_a), decltype(it_a)> &&
             std::sentinel_for<decltype(end_b), decltype(it_b)>
{
	while (it_a != end_a && it_b != end_b)
	{
		if (*it_a < *it_b)
			std::invoke(callback, *it_a++);
		else if (*it_b < *it_a)
			std::invoke(callback, *it_b++);
		else
		{
			std::invoke(callback, *it_a);
			++it_a;
			++it_b;
		}
	}

	while (it_a != end_a)
		std::invoke(callback, *it_a++);
	while (it_b != end_b)
		std::invoke(callback, *it_b++);
}

// Returns true if the range is in non-decreasing order.
bool is_sorted(std::ranges::forward_range auto &&xs)
{
	auto it = std::ranges::begin(xs);
	auto end = std::ranges::end(xs);
	if (it == end)
		return true;

	auto prev = it;
	++it;
	while (it != end)
	{
		if (*it < *prev)
			return false;
		prev = it;
		++it;
	}
	return true;
}

// Emits elements present in both sorted input ranges.
void set_intersection(std::ranges::input_range auto &&a,
                      std::ranges::input_range auto &&b, auto &&callback)
{
	set_intersection(std::ranges::begin(a), std::ranges::end(a),
	                 std::ranges::begin(b), std::ranges::end(b),
	                 std::forward<decltype(callback)>(callback));
}

// Emits elements present in all three sorted input ranges.
void set_intersection3(std::ranges::input_range auto &&a,
                       std::ranges::input_range auto &&b,
                       std::ranges::input_range auto &&c, auto &&callback)
{
	set_intersection3(std::ranges::begin(a), std::ranges::end(a),
	                  std::ranges::begin(b), std::ranges::end(b),
	                  std::ranges::begin(c), std::ranges::end(c),
	                  std::forward<decltype(callback)>(callback));
}

// Splits three-way matches from exactly-two-way matches across sorted ranges.
void set_intersection3(std::ranges::input_range auto &&a,
                       std::ranges::input_range auto &&b,
                       std::ranges::input_range auto &&c, auto &&callback3,
                       auto &&callback2)
{
	set_intersection3(std::ranges::begin(a), std::ranges::end(a),
	                  std::ranges::begin(b), std::ranges::end(b),
	                  std::ranges::begin(c), std::ranges::end(c),
	                  std::forward<decltype(callback3)>(callback3),
	                  std::forward<decltype(callback2)>(callback2));
}

// Emits the sorted union of both input ranges.
void set_union(std::ranges::input_range auto &&a,
               std::ranges::input_range auto &&b, auto &&callback)
{
	set_union(std::ranges::begin(a), std::ranges::end(a), std::ranges::begin(b),
	          std::ranges::end(b), std::forward<decltype(callback)>(callback));
}

} // namespace util