#include "catch2/catch_test_macros.hpp"

#include "util/indexed_heap.h"

#include <vector>

using namespace util;

TEST_CASE("indexed_heap default priority is zero for int and float",
          "[indexed_heap]")
{
	auto ints = indexed_heap<int>(5);
	auto floats = indexed_heap<float>(5);
	for (uint32_t i = 0; i < 5; ++i)
	{
		CHECK(ints.priority(i) == 0);
		CHECK(floats.priority(i) == 0.0f);
		CHECK_FALSE(ints.contains(i));
		CHECK_FALSE(floats.contains(i));
	}

	ints.resize(8);
	floats.resize(8);
	for (uint32_t i = 5; i < 8; ++i)
	{
		CHECK(ints.priority(i) == 0);
		CHECK(floats.priority(i) == 0.0f);
		CHECK_FALSE(ints.contains(i));
		CHECK_FALSE(floats.contains(i));
	}
}

TEST_CASE("indexed_heap push pop respects less as max-heap", "[indexed_heap]")
{
	indexed_heap<int> heap(4);
	heap.push(0, 10);
	heap.push(1, 30);
	heap.push(2, 20);

	CHECK(heap.size() == 3);
	CHECK(heap.top() == 1);
	CHECK(heap.top_priority() == 30);
	CHECK(heap.pop() == 1);
	CHECK(heap.pop() == 2);
	CHECK(heap.pop() == 0);
	CHECK(heap.empty());
}

TEST_CASE("indexed_heap greater compare yields min-heap", "[indexed_heap]")
{
	indexed_heap<int, std::greater<int>> heap(3);
	heap.push(0, 10);
	heap.push(1, 30);
	heap.push(2, 20);
	CHECK(heap.pop() == 0);
	CHECK(heap.pop() == 2);
	CHECK(heap.pop() == 1);
}

TEST_CASE("indexed_heap equal priorities pop in some order", "[indexed_heap]")
{
	indexed_heap<int> heap(3);
	heap.push(2, 5);
	heap.push(0, 5);
	heap.push(1, 5);
	std::vector<uint32_t> got;
	while (!heap.empty())
		got.push_back(heap.pop());
	CHECK(got.size() == 3);
	CHECK(got[0] + got[1] + got[2] == 0 + 1 + 2);
}

TEST_CASE("indexed_heap set_priority updates present and absent indices",
          "[indexed_heap]")
{
	indexed_heap<int> heap(3);
	heap.set_priority(1, 7);
	CHECK(heap.priority(1) == 7);
	CHECK_FALSE(heap.contains(1));

	heap.push(0, 1);
	heap.push(1);
	heap.push(2, 3);
	CHECK(heap.top() == 1);

	heap.set_priority(2, 10);
	CHECK(heap.top() == 2);

	heap.set_priority(2, 0);
	CHECK(heap.top() == 1);
}

TEST_CASE("indexed_heap add_priority adjusts present and absent indices",
          "[indexed_heap]")
{
	indexed_heap<int> heap(3);
	heap.add_priority(1, 7);
	CHECK(heap.priority(1) == 7);
	CHECK_FALSE(heap.contains(1));

	heap.push(0, 1);
	heap.push(1);
	heap.push(2, 3);
	CHECK(heap.top() == 1);

	heap.add_priority(2, 10);
	CHECK(heap.priority(2) == 13);
	CHECK(heap.top() == 2);

	heap.add_priority(2, -20);
	CHECK(heap.priority(2) == -7);
	CHECK(heap.top() == 1);
}

TEST_CASE("indexed_heap scale_all multiplies priorities without reordering",
          "[indexed_heap]")
{
	indexed_heap<int> heap(3);
	heap.push(0, 1);
	heap.push(1, 3);
	heap.push(2, 2);
	heap.set_priority(0, 5);
	CHECK(heap.top() == 0);

	heap.erase(0);
	heap.scale_all(2);
	CHECK(heap.priority(0) == 10);
	CHECK(heap.priority(1) == 6);
	CHECK(heap.priority(2) == 4);
	CHECK(heap.top() == 1);
	CHECK(heap.pop() == 1);
	CHECK(heap.pop() == 2);
}

TEST_CASE("indexed_heap erase and pop keep priorities", "[indexed_heap]")
{
	indexed_heap<int> heap(4);
	heap.push(0, 1);
	heap.push(1, 4);
	heap.push(2, 3);
	heap.push(3, 2);

	CHECK(heap.pop() == 1);
	CHECK(heap.priority(1) == 4);
	CHECK_FALSE(heap.contains(1));

	heap.erase(2);
	CHECK(heap.priority(2) == 3);
	CHECK_FALSE(heap.contains(2));
	CHECK(heap.size() == 2);

	heap.push(1);
	CHECK(heap.top() == 1);
	CHECK(heap.top_priority() == 4);
}

TEST_CASE("indexed_heap clear drops membership only", "[indexed_heap]")
{
	indexed_heap<int> heap(3);
	heap.push(0, 9);
	heap.push(2, 8);
	heap.clear();

	CHECK(heap.empty());
	CHECK(heap.universe_size() == 3);
	CHECK(heap.priority(0) == 9);
	CHECK(heap.priority(1) == 0);
	CHECK(heap.priority(2) == 8);
	CHECK_FALSE(heap.contains(0));
	CHECK_FALSE(heap.contains(2));

	heap.push(0);
	CHECK(heap.top() == 0);
}

TEST_CASE("indexed_heap erase of top middle and leaf", "[indexed_heap]")
{
	indexed_heap<int> heap(5);
	for (uint32_t i = 0; i < 5; ++i)
		heap.push(i, static_cast<int>(i + 1));

	heap.erase(4); // top
	CHECK(heap.top() == 3);

	heap.erase(1); // interior-ish
	CHECK(heap.size() == 3);
	CHECK(heap.contains(0));
	CHECK(heap.contains(2));
	CHECK(heap.contains(3));

	heap.erase(0); // likely leaf/low
	std::vector<uint32_t> got;
	while (!heap.empty())
		got.push_back(heap.pop());
	CHECK(got == std::vector<uint32_t>{3, 2});
}

TEST_CASE("default constructed indexed_heap is empty", "[indexed_heap]")
{
	indexed_heap<int> heap;
	CHECK(heap.universe_size() == 0);
	CHECK(heap.empty());
	heap.resize(2);
	CHECK(heap.priority(0) == 0);
	CHECK(heap.priority(1) == 0);
}

TEST_CASE("indexed_heap pop_nth removes the n'th in heap order",
          "[indexed_heap]")
{
	indexed_heap<int> heap(4);
	heap.push(0, 10);
	heap.push(1, 40);
	heap.push(2, 30);
	heap.push(3, 20);

	CHECK(heap.pop_nth(0) == 1);
	CHECK(heap.size() == 3);
	CHECK(heap.top() == 2);

	CHECK(heap.pop_nth(2) == 0);
	CHECK(heap.size() == 2);
	CHECK(heap.pop() == 2);
	CHECK(heap.pop() == 3);
	CHECK(heap.empty());
}
