#pragma once

#include "util/vector.h"

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
#include <type_traits>
#include <utility>
#include <vector>

namespace util {

// Priority queue over a dense index universe with reverse lookup.
// Priorities are owned for every index in [0, universe_size()), including
// indices that are not currently members of the heap. Membership and
// priorities are independent: erase/pop/clear remove membership only.
// Equal priorities may appear in either order.
template <class Priority, class Compare = std::less<Priority>,
          class Index = std::uint32_t>
class indexed_heap
{
	static_assert(std::is_unsigned_v<Index>, "Index must be an unsigned type");

	static constexpr Index npos = std::numeric_limits<Index>::max();

	std::vector<Priority> priority_;
	std::vector<Index> heap_;
	std::vector<Index> location_;
	[[no_unique_address]] Compare comp_{};

	bool better(Index a, Index b) const
	{
		return comp_(priority_[b], priority_[a]);
	}

	void swap_nodes(size_t a, size_t b)
	{
		std::swap(heap_[a], heap_[b]);
		location_[heap_[a]] = static_cast<Index>(a);
		location_[heap_[b]] = static_cast<Index>(b);
	}

	void sift_up(size_t at)
	{
		while (at > 0)
		{
			size_t parent = (at - 1) / 2;
			if (!better(heap_[at], heap_[parent]))
				break;
			swap_nodes(at, parent);
			at = parent;
		}
	}

	void sift_down(size_t at)
	{
		// while two children exist
		while (2 * at + 2 < heap_.size())
		{
			size_t child = 2 * at + 1;
			if (better(heap_[child + 1], heap_[child]))
				++child;
			if (!better(heap_[child], heap_[at]))
				return;
			swap_nodes(at, child);
			at = child;
		}

		// at most one lone child remains
		size_t child = 2 * at + 1;
		if (child < heap_.size() && better(heap_[child], heap_[at]))
			swap_nodes(at, child);
	}

	void sift(Index i)
	{
		assert(location_[i] != npos);
		sift_up(location_[i]);
		sift_down(location_[i]);
	}

	void check_index(Index i) const
	{
		assert(static_cast<size_t>(i) < priority_.size());
		assert(i != npos);
	}

  public:
	using priority_type = Priority;
	using index_type = Index;
	using compare_type = Compare;

	indexed_heap() = default;

	explicit indexed_heap(size_t n, Compare comp = {})
	    : priority_(n), location_(n, npos), comp_(std::move(comp))
	{
		heap_.reserve(n);
	}

	size_t universe_size() const noexcept { return priority_.size(); }
	size_t size() const noexcept { return heap_.size(); }
	bool empty() const noexcept { return heap_.empty(); }

	bool contains(Index i) const
	{
		check_index(i);
		return location_[i] != npos;
	}

	Priority const &priority(Index i) const
	{
		check_index(i);
		return priority_[i];
	}

	Index top() const
	{
		assert(!empty());
		return heap_.front();
	}

	Priority const &top_priority() const
	{
		assert(!empty());
		return priority_[heap_.front()];
	}

	Compare const &compare() const noexcept { return comp_; }

	// Grow the universe. New indices start with Priority{} and are absent.
	void resize(size_t n)
	{
		assert(n >= priority_.size());
		priority_.resize(n);
		location_.resize(n, npos);
	}

	void push(Index i)
	{
		check_index(i);
		assert(!contains(i));
		location_[i] = static_cast<Index>(heap_.size());
		heap_.push_back(i);
		sift_up(heap_.size() - 1);
	}

	void push(Index i, Priority p)
	{
		set_priority(i, std::move(p));
		push(i);
	}

	void erase(Index i)
	{
		check_index(i);
		Index at = location_[i];
		if (at == npos)
			return;
		location_[i] = npos;
		Index last = heap_.back();
		heap_.pop_back();
		if (static_cast<size_t>(at) == heap_.size())
			return;
		heap_[at] = last;
		location_[last] = at;
		sift(last);
	}

	Index pop()
	{
		assert(!empty());
		Index i = heap_.front();
		erase(i);
		return i;
	}

	// Pop the n'th element in heap order (pop_nth(0) == pop()).
	Index pop_nth(size_t n)
	{
		assert(n < size());
		util::small_vector<Index, 8> skipped;
		for (size_t i = 0; i < n; ++i)
			skipped.push_back(pop());
		Index r = pop();
		for (Index i : skipped)
			push(i);
		return r;
	}

	// Drop membership only. Universe size and priorities are unchanged.
	void clear() noexcept
	{
		for (Index i : heap_)
			location_[i] = npos;
		heap_.clear();
	}

	void set_priority(Index i, Priority p)
	{
		check_index(i);
		priority_[i] = std::move(p);
		if (location_[i] != npos)
			sift(i);
	}

	void add_priority(Index i, Priority delta)
	{
		check_index(i);
		priority_[i] += delta;
		if (location_[i] != npos)
			sift(i);
	}

	// Multiply every stored priority by p. Does not re-heapify; p must
	// preserve the Compare order (e.g. p >= 0).
	void scale_all(Priority p)
	{
		for (Priority &x : priority_)
			x *= p;
	}
};

} // namespace util
