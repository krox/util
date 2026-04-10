#pragma once

#include "util/span.h"
#include <algorithm>
#include <cassert>
#include <climits>
#include <vector>

namespace util {

// Disjoint-set data structure, also known as union-find.
// implementation details:
//   * When joining two components, the smaller one is merged into the larger
//     one. This guarantees worst-case logarithmic time complexities.
//   * Additionally, some non-const operations do path compression, which can
//     result in near-constant ("inverse Ackermann") time complexities. But it
//     is not worthwhile to sacrifice const-correctness for this optimization,
//     so this is just done opportunistically without strict guarantees.
class UnionFind
{
	// main data structure: roots are indicated by 'parent_[i] == i', 'size_' is
	// only valid for roots.
	std::vector<int> parent_, size_;

	// track number of components explicitly for convenience
	int singleton_count_ = 0; // components of size 1
	int large_count_ = 0;     // components of size > 1

	// find root-node with and without path compression
	int root(int a);
	int root(int a) const;

  public:
	// default constructor, 0 nodes, 0 components
	UnionFind() = default;

	// Constructor that starts with n disjoint components, numbered 0 to n-1.
	explicit UnionFind(size_t n);

	// Resets to all-disconnected state with the same number of nodes.
	void clear();

	// Number of nodes.
	int size() const noexcept { return parent_.size(); }

	// Number of singleton/non-singleton/all components.
	int singleton_count() const noexcept { return singleton_count_; }
	int large_count() const noexcept { return large_count_; }
	int component_count() const noexcept
	{
		return singleton_count_ + large_count_;
	}

	// Join the components of elements a and b.
	// Returns true if newly joined, false if they already were joined.
	bool join(int a, int b) noexcept;

	// Join components of a[0]..a[$-1] into one.
	void join(std::span<const int> a) noexcept;

	// Returns true if a and b are currently joined.
	bool is_joined(int a, int b) const noexcept { return root(a) == root(b); }

	// Size of the component which a belongs to.
	int component_size(int a) const noexcept { return size_[root(a)]; }

	class Components;

	// explicit build list of nodes in each component.
	// TODO: result is not fully unique yet. Order of components is by
	// decreasing size, but undefined between components of the same size.
	Components components() const;
};

class UnionFind::Components
{
	std::vector<int> nodes_;
	std::vector<int> offsets_;
	int singleton_count_ = 0;

	Components(std::vector<int> nodes, std::vector<int> offsets,
	           int singleton_count);

	friend class UnionFind;

  public:
	// same size metrics as UnionFind
	int size() const noexcept { return (int)nodes_.size(); }
	int singleton_count() const noexcept { return singleton_count_; }
	int large_count() const noexcept
	{
		return component_count() - singleton_count_;
	}
	int component_count() const noexcept { return (int)offsets_.size() - 1; }

	// components in descending order of size, i.e. id=0 is the largest one.
	std::span<const int> component(int id) const noexcept
	{
		assert(0 <= id && id < component_count());
		return std::span<const int>(nodes_.data() + offsets_[id],
		                            offsets_[id + 1] - offsets_[id]);
	}

	std::span<const int> singletons() const noexcept
	{
		return std::span<const int>(nodes_.data() + offsets_[large_count()],
		                            singleton_count_);
	}
};

} // namespace util
