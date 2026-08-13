#include "util/synchronized.h"

#include "catch2/catch_test_macros.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <memory>
#include <string>
#include <thread>
#include <vector>

TEST_CASE("locked_ptr")
{
	SECTION("default constructed is null")
	{
		util::locked_ptr<int> p;
		CHECK_FALSE(p);
		CHECK(p.get() == nullptr);
	}

	SECTION("wraps a locked mutex and dereferences the value")
	{
		util::synchronized<int> obj(42);
		auto p = obj.lock();
		CHECK(p);
		CHECK(*p == 42);
		*p = 43;
		CHECK(*p == 43);
	}

	SECTION("move construction transfers ownership")
	{
		util::synchronized<int> obj(1);
		auto a = obj.lock();
		auto b = std::move(a);
		CHECK_FALSE(a);
		CHECK(a.get() == nullptr);
		CHECK(b);
		CHECK(*b == 1);
	}

	SECTION("move assignment releases the previously held lock")
	{
		util::synchronized<int> obj_a(1);
		util::synchronized<int> obj_b(2);
		auto a = obj_a.lock();
		auto b = obj_b.lock();
		a = std::move(b);
		CHECK(*a == 2);
		CHECK_FALSE(b);

		// obj_a's mutex was released by the assignment, so it can be locked
		// again immediately.
		CHECK(obj_a.try_lock());
	}

	SECTION("unlock releases the mutex and resets to null state")
	{
		util::synchronized<int> obj(1);
		auto p = obj.lock();
		p.unlock();
		CHECK_FALSE(p);
		CHECK(p.get() == nullptr);

		// unlocking twice is a no-op
		p.unlock();
		CHECK_FALSE(p);

		CHECK(obj.try_lock());
	}

	SECTION("release_lock transfers the lock out without unlocking")
	{
		util::synchronized<int> obj(1);
		auto p = obj.lock();
		auto lock = p.release_lock();
		CHECK_FALSE(p);
		CHECK(lock.owns_lock());

		// mutex is still held via 'lock', so try_lock must fail
		CHECK_FALSE(obj.try_lock());
		lock.unlock();
		CHECK(obj.try_lock());
	}
}

TEST_CASE("synchronized")
{
	SECTION("default construction requires default-constructible value")
	{
		util::synchronized<std::string> obj;
		CHECK(obj.lock()->empty());
	}

	SECTION("constructor forwards arguments to the value")
	{
		util::synchronized<std::string> obj(size_t(3), 'x');
		CHECK(*obj.lock() == "xxx");
	}

	SECTION("try_lock fails while the lock is already held")
	{
		util::synchronized<int> obj(0);
		auto held = obj.lock();
		CHECK_FALSE(obj.try_lock());
	}

	SECTION("try_lock succeeds once the previous lock is released")
	{
		util::synchronized<int> obj(0);
		{
			auto held = obj.lock();
		}
		auto p = obj.try_lock();
		CHECK(p);
		CHECK(*p == 0);
	}

	SECTION("lock_for times out while the lock is held elsewhere")
	{
		util::synchronized<int> obj(0);
		auto held = obj.lock();
		auto p = obj.lock_for(std::chrono::milliseconds(10));
		CHECK_FALSE(p);
	}

	SECTION("const object yields a locked_ptr to const")
	{
		const util::synchronized<int> obj(5);
		auto p = obj.lock();
		CHECK(*p == 5);
		static_assert(
		    std::is_same_v<decltype(*p), const int &>,
		    "locking a const synchronized<T> should yield locked_ptr<const T>");
	}

	SECTION("serializes concurrent increments")
	{
		util::synchronized<int> counter(0);
		constexpr int thread_count = 8;
		constexpr int increments_per_thread = 1000;

		std::vector<std::thread> threads;
		threads.reserve(thread_count);
		for (int i = 0; i < thread_count; ++i)
			threads.emplace_back([&] {
				for (int j = 0; j < increments_per_thread; ++j)
					++*counter.lock();
			});

		for (auto &t : threads)
			t.join();

		CHECK(*counter.lock() == thread_count * increments_per_thread);
	}
}

TEST_CASE("synchronized_queue")
{
	SECTION("try_pop returns nullopt when empty")
	{
		util::synchronized_queue<int> queue;
		CHECK_FALSE(queue.try_pop().has_value());
		CHECK(queue.size() == 0);
		CHECK(queue.empty());
	}

	SECTION("empty() reflects the current size")
	{
		util::synchronized_queue<int> queue;
		CHECK(queue.empty());
		queue.push(1);
		CHECK_FALSE(queue.empty());
	}

	SECTION("preserves fifo order")
	{
		util::synchronized_queue<int> queue;
		queue.push(1);
		queue.push(2);
		queue.push(3);
		CHECK(queue.size() == 3);

		CHECK(queue.try_pop() == std::optional<int>{1});
		CHECK(queue.try_pop() == std::optional<int>{2});
		CHECK(queue.try_pop() == std::optional<int>{3});
		CHECK_FALSE(queue.try_pop().has_value());
	}

	SECTION("pop_all drains the queue")
	{
		util::synchronized_queue<int> queue;
		queue.push(1);
		queue.push(2);

		auto all = queue.pop_all();
		CHECK(all.size() == 2);
		CHECK(queue.size() == 0);
	}

	SECTION("pop blocks until an element is pushed")
	{
		util::synchronized_queue<int> queue;
		std::thread producer([&] {
			std::this_thread::sleep_for(std::chrono::milliseconds(20));
			queue.push(7);
		});

		int value = queue.pop();
		CHECK(value == 7);
		producer.join();
	}

	SECTION("pop with stop_waiting predicate returns nullopt on notify")
	{
		util::synchronized_queue<int> queue;
		std::atomic<bool> stop{false};

		std::thread notifier([&] {
			std::this_thread::sleep_for(std::chrono::milliseconds(20));
			stop = true;
			queue.notify();
		});

		auto result = queue.pop([&] { return stop.load(); });
		CHECK_FALSE(result.has_value());
		notifier.join();
	}

	SECTION("multiple producers, single consumer preserves all elements")
	{
		util::synchronized_queue<int> queue;
		constexpr int producer_count = 4;
		constexpr int values_per_producer = 256;

		std::vector<std::thread> producers;
		producers.reserve(producer_count);
		for (int p = 0; p < producer_count; ++p)
			producers.emplace_back([&] {
				for (int i = 0; i < values_per_producer; ++i)
					queue.push(i);
			});

		int received = 0;
		while (received < producer_count * values_per_producer)
			if (queue.pop([] { return false; }))
				++received;

		for (auto &t : producers)
			t.join();

		CHECK(received == producer_count * values_per_producer);
	}
}

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
