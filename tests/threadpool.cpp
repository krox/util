#include "util/threadpool.h"
#include "catch2/catch_test_macros.hpp"
#include "fmt/format.h"
#include <algorithm>
#include <chrono>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <type_traits>

using namespace std::chrono_literals;

TEST_CASE("threadpool")
{
	SECTION("async")
	{
		util::ThreadPool pool{2};
		auto f = [](int x) {
			std::this_thread::sleep_for(20ms);
			return 2 * x;
		};
		auto e = pool.async([]() { throw std::runtime_error("foo"); });
		auto a = pool.async(f, 1);
		auto b = pool.async(f, 2);
		auto c = pool.async(f, 3);
		static_assert(std::is_same_v<decltype(a), std::future<int>>);
		CHECK(a.get() == 2);
		CHECK(b.get() == 4);
		CHECK(c.get() == 6);
		REQUIRE_THROWS(e.get());

		auto g = pool.async([x = std::make_unique<int>(42)]() { return *x; });
		CHECK(g.get() == 42);
	}

	SECTION("for_each_thread")
	{
		util::ThreadPool pool{4};
		auto result = pool.for_each_thread([](int worker_index) {
			std::this_thread::sleep_for(20ms);
			return worker_index * worker_index;
		});

		REQUIRE(result.size() == (size_t)pool.num_threads());
		std::sort(result.begin(), result.end());
		CHECK(result == std::vector<int>{0, 1, 4, 9});
	}

	SECTION("for_each_thread void")
	{
		util::ThreadPool pool{4};
		std::vector<std::atomic<int>> seen((size_t)pool.num_threads());

		pool.for_each_thread(
		    [&](int worker_index) { seen[(size_t)worker_index].fetch_add(1); });

		for (auto &count : seen)
			CHECK(count.load() == 1);
	}

	SECTION("parallel for_each (mutable)")
	{
		std::vector<int> v = {1, 2, 3, 4, 5};
		auto f = [](int &x) {
			std::this_thread::sleep_for(20ms);
			x *= 2;
		};

		util::ThreadPool pool;
		pool.for_each(v.begin(), v.end(), f);
		CHECK(v == std::vector{2, 4, 6, 8, 10});
	}

	SECTION("parallel for_each (const)")
	{
		const std::vector<int> v = {1, 2, 3, 4, 5};
		std::atomic<int> sum{0};
		auto f = [&sum](int x) {
			std::this_thread::sleep_for(20ms);
			sum += x;
		};

		util::ThreadPool pool;
		pool.for_each(v.begin(), v.end(), f);
		CHECK(sum == 1 + 2 + 3 + 4 + 5);
	}

	SECTION("parallel for_each (range)")
	{
		const std::vector<int> v = {1, 2, 3, 4, 5};
		std::atomic<int> sum{0};
		auto f = [&sum](int x) {
			std::this_thread::sleep_for(20ms);
			sum += x;
		};

		util::ThreadPool pool;
		pool.for_each(v, f);
		CHECK(sum == 1 + 2 + 3 + 4 + 5);
	}

	SECTION("parallel for_each with worker-local state")
	{
		std::vector<int> v(10'000);
		for (int i = 0; i < (int)v.size(); ++i)
			v[(size_t)i] = i;

		util::ThreadPool pool{4};
		std::atomic<int> state_count{0};
		std::atomic<int> sum{0};
		std::mutex states_mutex;
		std::vector<std::shared_ptr<std::atomic<int>>> states;

		pool.for_each_with_state(
		    v,
		    [&]() {
			    auto state = std::make_shared<std::atomic<int>>(0);
			    state_count.fetch_add(1);
			    std::lock_guard lock(states_mutex);
			    states.push_back(state);
			    return state;
		    },
		    [&sum](auto &state, int x) {
			    state->fetch_add(1);
			    sum += x;
		    });

		CHECK(state_count <= pool.num_threads());
		REQUIRE(!states.empty());

		int total_uses = 0;
		bool saw_reuse = false;
		for (auto const &state : states)
		{
			int uses = state->load();
			total_uses += uses;
			saw_reuse = saw_reuse || uses > 1;
		}

		CHECK(total_uses == (int)v.size());
		CHECK(saw_reuse);
		CHECK(sum == 49'995'000);
	}

	SECTION("parallel for_each with worker-local state propagates exceptions")
	{
		std::vector<int> v(128);
		for (int i = 0; i < (int)v.size(); ++i)
			v[(size_t)i] = i;

		util::ThreadPool pool{4};
		REQUIRE_THROWS_AS(pool.for_each_with_state(
		                      v, [] { return 0; },
		                      [](int &, int x) {
			                      if (x == 42)
				                      throw std::runtime_error("foo");
		                      }),
		                  std::runtime_error);
	}

	SECTION("parallel for_each with worker-local state supports chunking")
	{
		std::vector<int> v(257, 0);
		util::ThreadPool pool{4};
		std::atomic<int> state_count{0};

		pool.for_each_with_state(
		    v,
		    [&] {
			    state_count.fetch_add(1);
			    return 0;
		    },
		    [](int &state, int &x) {
			    ++state;
			    x += 1;
		    },
		    17);

		CHECK(state_count <= pool.num_threads());
		CHECK(v == std::vector<int>(257, 1));
	}

	SECTION("parallel filter")
	{
		util::ThreadPool pool;
		std::vector<int> r = pool.filter(std::views::iota(0, 10),
		                                 [](int a) -> bool { return a % 2; });
		CHECK(r == std::vector<int>{1, 3, 5, 7, 9});
	}
}
