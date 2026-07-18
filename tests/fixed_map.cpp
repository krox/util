#include "catch2/catch_test_macros.hpp"

#include "util/fixed_map.h"
#include <barrier>
#include <string>
#include <thread>
#include <type_traits>
#include <vector>

static_assert(std::is_same_v<decltype(std::declval<util::fixed_map<int, int> const &>().items()),
							 std::span<util::fixed_map<int, int>::const_pointer const>>);

TEST_CASE("fixed_map basic operations", "[fixed_map]")
{
	util::fixed_map<std::string, int> map;
	CHECK(map.empty());
	CHECK(map.items().empty());

	auto [value, inserted] = map.try_emplace("alpha", 7);
	CHECK(inserted);
	CHECK(value == 7);
	CHECK(map.size() == 1);
	CHECK(map.contains("alpha"));
	CHECK(map.at("alpha") == 7);

	map["beta"] = 11;
	CHECK(map.size() == 2);
	CHECK(map.at("beta") == 11);

	auto items = map.items();
	REQUIRE(items.size() == 2);
	CHECK(items[0]->first == "alpha");
	CHECK(items[0]->second == 7);
	CHECK(items[1]->first == "beta");
	CHECK(items[1]->second == 11);
}

TEST_CASE("fixed_map duplicate insert returns existing value", "[fixed_map]")
{
	util::fixed_map<std::string, int> map;
	auto [first, inserted_first] = map.try_emplace("alpha", 3);
	auto [second, inserted_second] = map.try_emplace("alpha", 9);

	CHECK(inserted_first);
	CHECK_FALSE(inserted_second);
	CHECK(&first == &second);
	CHECK(second == 3);
	CHECK(map.size() == 1);
}

TEST_CASE("fixed_map throws when full", "[fixed_map]")
{
	util::fixed_map<int, int> map;
	for (int i = 0; i < (int)map.capacity(); ++i)
	{
		auto [value, inserted] = map.try_emplace(i, i * 2);
		CHECK(inserted);
		CHECK(value == i * 2);
	}

	CHECK(map.size() == map.capacity());
	CHECK_THROWS_AS(map.try_emplace(-1, 1), std::runtime_error);
}

TEST_CASE("fixed_map supports concurrent prefix growth", "[fixed_map]")
{
	util::fixed_map<int, int> map;
	constexpr int threads = 4;
	constexpr int per_thread = 24;
	std::barrier start(threads);
	std::vector<std::jthread> workers;

	for (int t = 0; t < threads; ++t)
	{
		workers.emplace_back([&, t]() {
			start.arrive_and_wait();
			for (int i = 0; i < per_thread; ++i)
			{
				int key = t * per_thread + i;
				(void)map.try_emplace(key, key + 1);
			}
		});
	}
	workers.clear();

	for (int key = 0; key < threads * per_thread; ++key)
	{
		REQUIRE(map.contains(key));
		CHECK(map.at(key) == key + 1);
	}

	auto items = map.items();
	REQUIRE(items.size() == threads * per_thread);
	for (auto item : items)
		REQUIRE(item != nullptr);
}

TEST_CASE("fixed_map coalesces concurrent duplicate keys", "[fixed_map]")
{
	util::fixed_map<int, int> map;
	constexpr int threads = 8;
	std::barrier start(threads);
	std::vector<std::jthread> workers;

	for (int t = 0; t < threads; ++t)
	{
		workers.emplace_back([&, t]() {
			start.arrive_and_wait();
			(void)map.try_emplace(42, t);
		});
	}
	workers.clear();

	REQUIRE(map.size() == 1);
	CHECK(map.contains(42));
	REQUIRE(map.items().size() == 1);
	CHECK(map.at(42) >= 0);
	CHECK(map.at(42) < threads);
}