#pragma once

// Set theoretic functions (intersection, union, ...) for sorted vectors. Input
// spans must be sorted in non-decreasing order. Output is emitted via callbacks
// for maximum flexibility.

#include <concepts>
#include <functional>
#include <span>

namespace util {

// Returns true if the span is in non-decreasing order.
template <std::totally_ordered T> bool is_sorted(std::span<const T> xs)
{
	for (size_t i = 1; i < xs.size(); ++i)
		if (xs[i] < xs[i - 1])
			return false;
	return true;
}

// Emits elements present in both sorted input spans.
template <std::totally_ordered T>
void set_intersection(std::span<const T> a, std::span<const T> b,
                      std::invocable<T const &> auto callback)
{
	auto it_a = a.begin();
	auto it_b = b.begin();
	while (it_a != a.end() && it_b != b.end())
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

// Emits elements present in all three sorted input spans.
template <std::totally_ordered T>
void set_intersection3(std::span<const T> a, std::span<const T> b,
                       std::span<const T> c,
                       std::invocable<T const &> auto callback)
{
	auto it_a = a.begin();
	auto it_b = b.begin();
	auto it_c = c.begin();
	while (it_a != a.end() && it_b != b.end() && it_c != c.end())
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

// Splits three-way matches from exactly-two-way matches across sorted spans.
template <std::totally_ordered T>
void set_intersection3(std::span<const T> a, std::span<const T> b,
                       std::span<const T> c,
                       std::invocable<T const &> auto callback3,
                       std::invocable<T const &> auto callback2)
{
	auto it_a = a.begin();
	auto it_b = b.begin();
	auto it_c = c.begin();
	while (true)
	{
		if (it_a == a.end())
		{
			set_intersection(std::span<const T>(it_b, b.end()),
			                 std::span<const T>(it_c, c.end()), callback2);
			return;
		}
		if (it_b == b.end())
		{
			set_intersection(std::span<const T>(it_a, a.end()),
			                 std::span<const T>(it_c, c.end()), callback2);
			return;
		}
		if (it_c == c.end())
		{
			set_intersection(std::span<const T>(it_a, a.end()),
			                 std::span<const T>(it_b, b.end()), callback2);
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

// Emits the sorted union of both input spans.
template <std::totally_ordered T>
void set_union(std::span<const T> a, std::span<const T> b,
               std::invocable<T const &> auto callback)
{
	auto it_a = a.begin();
	auto it_b = b.begin();
	while (it_a != a.end() && it_b != b.end())
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

	while (it_a != a.end())
		std::invoke(callback, *it_a++);
	while (it_b != b.end())
		std::invoke(callback, *it_b++);
}

} // namespace util