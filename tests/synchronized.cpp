#include "util/synchronized.h"

#include "catch2/catch_test_macros.hpp"

#include <algorithm>
#include <atomic>
#include <memory>
#include <thread>
#include <vector>

TEST_CASE("mpsc queue")
{
	SECTION("pop returns nullopt when empty")
	{
		util::MpscQueue<int> queue;
		CHECK_FALSE(queue.pop().has_value());
	}

	SECTION("preserves fifo order for a single producer")
	{
		util::MpscQueue<int> queue;
		queue.push(1);
		queue.push(2);
		queue.push(3);

		CHECK(queue.pop() == std::optional<int>{1});
		CHECK(queue.pop() == std::optional<int>{2});
		CHECK(queue.pop() == std::optional<int>{3});
		CHECK_FALSE(queue.pop().has_value());
	}

	SECTION("supports move-only payloads")
	{
		util::MpscQueue<std::unique_ptr<int>> queue;
		queue.push(std::make_unique<int>(42));

		auto value = queue.pop();
		REQUIRE(value.has_value());
		REQUIRE(*value);
		CHECK(**value == 42);
		CHECK_FALSE(queue.pop().has_value());
	}

	SECTION("accepts concurrent producers while preserving per-producer order")
	{
		util::MpscQueue<std::pair<int, int>> queue;
		constexpr int producer_count = 4;
		constexpr int values_per_producer = 256;
		std::vector<std::thread> producers;
		producers.reserve(producer_count);

		for (int producer = 0; producer < producer_count; ++producer)
		{
			producers.emplace_back([&, producer] {
				for (int index = 0; index < values_per_producer; ++index)
					queue.push({producer, index});
			});
		}

		for (auto &producer : producers)
			producer.join();

		std::vector<std::pair<int, int>> values;
		values.reserve(producer_count * values_per_producer);
		for (int attempts = 0;
		     attempts < producer_count * values_per_producer * 8; ++attempts)
		{
			if (auto value = queue.pop())
			{
				values.push_back(*value);
				if ((int)values.size() == producer_count * values_per_producer)
					break;
			}
			else
				std::this_thread::yield();
		}

		REQUIRE(values.size() ==
		        (size_t)(producer_count * values_per_producer));

		CHECK_FALSE(queue.pop().has_value());

		std::vector<int> counts(producer_count, 0);
		for (auto [producer, index] : values)
		{
			REQUIRE(producer >= 0);
			REQUIRE(producer < producer_count);
			CHECK(index == counts[(size_t)producer]);
			++counts[(size_t)producer];
		}

		CHECK(std::all_of(counts.begin(), counts.end(), [&](int count) {
			return count == values_per_producer;
		}));
	}
}
