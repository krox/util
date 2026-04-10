#include "util/unionfind.h"

#include <utility>

using namespace util;

int UnionFind::root(int a)
{
	assert(0 <= a && a < (int)parent_.size());
	while (parent_[a] != a)
		a = parent_[a] = parent_[parent_[a]];
	return a;
}

int UnionFind::root(int a) const
{
	assert(0 <= a && a < (int)parent_.size());
	while (parent_[a] != a)
		a = parent_[a];
	return a;
}

UnionFind::UnionFind(size_t n)
    : parent_(n), size_(n, 1), singleton_count_((int)n)
{
	assert(n <= INT_MAX);
	for (int i = 0; i < (int)parent_.size(); ++i)
		parent_[i] = i;
}

void UnionFind::clear()
{
	for (int i = 0; i < (int)parent_.size(); ++i)
	{
		parent_[i] = i;
		size_[i] = 1;
	}
	singleton_count_ = (int)parent_.size();
	large_count_ = 0;
}

bool UnionFind::join(int a, int b) noexcept
{
	a = root(a);
	b = root(b);
	if (a == b)
		return false;

	if (size_[a] < size_[b])
		std::swap(a, b);

	if (size_[a] == 1) // join two singletons
	{
		singleton_count_ -= 2;
		large_count_ += 1;
	}
	else if (size_[b] == 1) // join singleton to large
		singleton_count_ -= 1;
	else // join two large components
		large_count_ -= 1;

	parent_[b] = a;
	size_[a] += size_[b];
	return true;
}

void UnionFind::join(std::span<const int> a) noexcept
{
	if (a.size() == 0)
		return;
	for (int x : a)
		join(x, a[0]);
}

UnionFind::Components::Components(std::vector<int> nodes,
                                  std::vector<int> offsets, int singleton_count)
    : nodes_(std::move(nodes)), offsets_(std::move(offsets)),
      singleton_count_(singleton_count)
{
	assert(singleton_count_ >= 0 &&
	       singleton_count_ <= (int)offsets_.size() - 1);
	assert(!offsets_.empty());
	assert(std::is_sorted(offsets_.begin(), offsets_.end()));
	assert(offsets_.front() == 0);
	assert(offsets_.back() == (int)nodes_.size());
}

UnionFind::Components UnionFind::components() const
{
	std::vector<std::pair<int, int>> roots; // (size, root)
	roots.reserve(large_count_);
	for (int i = 0; i < (int)parent_.size(); ++i)
		if (parent_[i] == i)
			roots.emplace_back(size_[i], i);
	std::ranges::sort(roots, std::greater<>(), {});

	auto map = std::vector<int>(parent_.size(), -1);
	for (int i = 0; i < (int)roots.size(); ++i)
		map[roots[i].second] = i;

	auto offsets = std::vector<int>(roots.size() + 1);
	offsets[0] = 0;
	for (int i = 0; i < (int)roots.size() - 1; ++i)
		offsets[i + 2] = offsets[i + 1] + roots[i].first;

	auto nodes = std::vector<int>(parent_.size());
	for (int i = 0; i < (int)parent_.size(); ++i)
		nodes[offsets[map[root(i)] + 1]++] = i;

	// small fix: sort the singletone components
	std::ranges::sort(nodes.begin() + nodes.size() - singleton_count_,
	                  nodes.end());

	return Components(std::move(nodes), std::move(offsets), singleton_count_);
}
