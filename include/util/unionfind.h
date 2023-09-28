#ifndef UTIL_UNIONFIND_H
#define UTIL_UNIONFIND_H

#include "util/span.h"
#include <algorithm>
#include <climits>
#include <vector>

namespace util {
/**
 *  "Disjoint-set data structure".
 *  Quite beautiful, but very special-purpose data structure. So if you don't
 *  know what this is, don't worry, you don't need it.
 */
class UnionFind
{
	std::vector<int> par_, size_;
	int nComp_ = 0;

  public:
	int root(int a)
	{
		assert(0 <= a && a < (int)par_.size());
		while (par_[a] != a)
			a = par_[a] = par_[par_[a]];
		return a;
	}

	int root(int a) const
	{
		assert(0 <= a && a < (int)par_.size());
		while (par_[a] != a)
			a = par_[a];
		return a;
	}

	UnionFind() = default;

	/** constructor that starts with n disjoint components, numbered 0 to n-1 */
	UnionFind(size_t n) : par_(n), size_(n, 1), nComp_((int)n)
	{
		assert(n <= INT_MAX);
		for (int i = 0; i < (int)par_.size(); ++i)
			par_[i] = i;
	}

	/** resets to all-disconnected state */
	void clear()
	{
		for (int i = 0; i < (int)par_.size(); ++i)
		{
			par_[i] = i;
			size_[i] = 1;
		}
		nComp_ = (int)par_.size();
	}

	/** number of nodes */
	size_t size() const { return par_.size(); }

	/** number of components */
	size_t nComp() const { return nComp_; }

	/**
	 * Join the components of elements a and b.
	 * Returns: true if newly joint, false if they already were joined.
	 */
	bool join(int a, int b)
	{
		a = root(a);
		b = root(b);
		if (a == b)
			return false;
		if (size_[a] < size_[b])
			std::swap(a, b);
		par_[b] = a;
		size_[a] += size_[b];
		nComp_ -= 1;
		return true;
	}

	/** Join components of a[0]..a[$-1] into one */
	void join(std::span<const int> a)
	{
		if (a.size() == 0)
			return;
		for (int x : a)
			join(x, a[0]);
	}

	/** returns true if a and b are currently joined */
	bool isJoined(int a, int b) const { return root(a) == root(b); }

	/** size of the component which a belongs to */
	int compSize(int a) const { return size_[root(a)]; }

	/**
	 * Returns: Array of size `.length` such that each connected component
	 *          has a unique number between 0 and .nComps+1
	 *
	 * If `minSize > 1`, all elements in components smaller than minSize
	 * are ignored and indicated as `-1` in the output.
	 */
	std::vector<int> components(int minSize = 1) const
	{
		auto comp = std::vector<int>(par_.size(), -1);

		// label roots
		int count = 0;
		for (int i = 0; i < (int)par_.size(); ++i)
			if (par_[i] == i && size_[i] >= minSize)
				comp[i] = count++;

		// label everything
		for (int i = 0; i < (int)par_.size(); ++i)
			comp[i] = comp[root(i)];

		return comp;
	}

	std::vector<int> compSizes(int minSize = 1) const
	{
		std::vector<int> r;
		r.reserve(nComp_);
		for (int i = 0; i < (int)par_.size(); ++i)
			if (par_[i] == i && size_[i] >= minSize)
				r.push_back(size_[i]);
		std::sort(r.begin(), r.end(), std::greater<>());
		return r;
	}
};

} // namespace util
#endif
