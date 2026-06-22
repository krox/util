#include "util/threadpool.h"
#include "catch2/catch_test_macros.hpp"
#include "fmt/format.h"
#include <algorithm>
#include <chrono>
#include <memory>
#include <mutex>
#include <numeric>
#include <stdexcept>
#include <thread>
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
		static_assert(std::is_same_v<decltype(a), util::Task<int>>);
		CHECK(a.get() == 2);
		CHECK(b.get() == 4);
		CHECK(c.get() == 6);
		REQUIRE_THROWS(e.get());

		auto g = pool.async([x = std::make_unique<int>(42)]() { return *x; });
		CHECK(g.get() == 42);
	}

	SECTION("async passes stop handle when supported")
	{
		util::ThreadPool pool{1};
		std::promise<void> blocker_started;
		std::promise<void> blocker_release;
		auto blocker_release_future = blocker_release.get_future();

		auto blocker = pool.async([&] {
			blocker_started.set_value();
			blocker_release_future.wait();
		});

		blocker_started.get_future().wait();

		struct overloaded_job
		{
			int operator()(util::stop_handle stop, int value) const
			{
				while (!stop)
					std::this_thread::yield();
				return value * 10;
			}

			int operator()(int value) const { return value * -1; }
		};

		auto task = pool.async(overloaded_job{}, 7);
		task.request_stop();
		blocker_release.set_value();

		blocker.get();
		CHECK(task.get() == 70);
	}

	SECTION("parallel member function collects ordered results")
	{
		util::ThreadPool pool{4};

		auto results =
		    pool.parallel([](int worker_id) { return worker_id * worker_id; });

		CHECK(results == std::vector<int>{0, 1, 4, 9});
	}

	SECTION("parallel member function waits for all workers before rethrowing")
	{
		util::ThreadPool pool{2};
		std::promise<void> thrower_started;
		std::promise<void> blocker_started;
		std::promise<void> release_thrower;
		std::promise<void> release_blocker;
		auto release_thrower_future = release_thrower.get_future().share();
		auto release_blocker_future = release_blocker.get_future().share();
		std::atomic<bool> blocker_finished = false;

		auto call = std::async(std::launch::async, [&] {
			REQUIRE_THROWS_AS(pool.parallel([&](int worker_id) {
				if (worker_id == 0)
				{
					thrower_started.set_value();
					release_thrower_future.wait();
					throw std::runtime_error("foo");
				}

				blocker_started.set_value();
				release_blocker_future.wait();
				blocker_finished.store(true);
			}),
			                  std::runtime_error);
		});

		thrower_started.get_future().wait();
		blocker_started.get_future().wait();
		release_thrower.set_value();

		CHECK(call.wait_for(20ms) == std::future_status::timeout);
		CHECK_FALSE(blocker_finished.load());

		release_blocker.set_value();
		call.get();
		CHECK(blocker_finished.load());
	}

	SECTION("parallel for_each free function")
	{
		std::vector<int> v = {1, 2, 3, 4, 5};

		util::ThreadPool pool{4};
		util::bulk_options options;
		options.chunk_size = 2;

		util::for_each(pool, v, [](int &x) { x *= 3; }, options);
		CHECK(v == std::vector{3, 6, 9, 12, 15});
	}

	SECTION("parallel for_each free function with worker-local state")
	{
		std::vector<int> v(10'000);
		for (int i = 0; i < (int)v.size(); ++i)
			v[(size_t)i] = i;

		util::ThreadPool pool{4};
		util::bulk_options options;
		options.chunk_size = 113;
		std::atomic<int> state_count{0};
		std::atomic<int> sum{0};
		std::mutex states_mutex;
		std::vector<std::shared_ptr<std::atomic<int>>> states;

		util::for_each(
		    pool, v,
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
		    },
		    options);

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

	SECTION("parallel for_each free function with worker-local state "
	        "propagates exceptions")
	{
		std::vector<int> v(128);
		for (int i = 0; i < (int)v.size(); ++i)
			v[(size_t)i] = i;

		util::ThreadPool pool{4};
		util::bulk_options options;
		options.chunk_size = 9;

		REQUIRE_THROWS_AS(util::for_each(
		                      pool, v, [] { return 0; },
		                      [](int &, int x) {
			                      if (x == 42)
				                      throw std::runtime_error("foo");
		                      },
		                      options),
		                  std::runtime_error);
	}

	SECTION("parallel for_each free function with worker-local state supports "
	        "chunking")
	{
		std::vector<int> v(257, 0);
		util::ThreadPool pool{4};
		util::bulk_options options;
		options.chunk_size = 17;
		std::atomic<int> state_count{0};

		util::for_each(
		    pool, v,
		    [&] {
			    state_count.fetch_add(1);
			    return 0;
		    },
		    [](int &state, int &x) {
			    ++state;
			    x += 1;
		    },
		    options);

		CHECK(state_count <= pool.num_threads());
		CHECK(v == std::vector<int>(257, 1));
	}

	SECTION("parallel filter unordered free function")
	{
		std::vector<int> v = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};

		util::ThreadPool pool{4};
		util::bulk_options options;
		options.chunk_size = 2;

		auto r = util::filter_unordered(
		    pool, v, [](int x) -> bool { return x % 2 == 1; }, options);

		std::sort(r.begin(), r.end());
		CHECK(r == std::vector<int>{1, 3, 5, 7, 9});
	}

	SECTION("parallel filter unordered free function with worker-local state")
	{
		std::vector<int> v(10'000);
		for (int i = 0; i < (int)v.size(); ++i)
			v[(size_t)i] = i;

		util::ThreadPool pool{4};
		util::bulk_options options;
		options.chunk_size = 113;
		std::atomic<int> state_count{0};
		std::mutex states_mutex;
		std::vector<std::shared_ptr<std::atomic<int>>> states;

		auto r = util::filter_unordered(
		    pool, v,
		    [&]() {
			    auto state = std::make_shared<std::atomic<int>>(0);
			    state_count.fetch_add(1);
			    std::lock_guard lock(states_mutex);
			    states.push_back(state);
			    return state;
		    },
		    [](auto &state, int x) -> bool {
			    state->fetch_add(1);
			    return x % 2 == 1;
		    },
		    options);

		std::sort(r.begin(), r.end());
		CHECK(r.size() == v.size() / 2);
		CHECK(r.front() == 1);
		CHECK(r.back() == 9'999);
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
	}

	SECTION("bulk_execute collects per-participant results")
	{
		std::vector<int> v(10'000);
		for (int i = 0; i < (int)v.size(); ++i)
			v[(size_t)i] = i;

		util::ThreadPool pool{4};
		util::bulk_options options;
		options.chunk_size = 113;

		auto partials = util::bulk_execute(
		    pool, v.size(), [] { return 0; },
		    [&v](int &state, size_t begin, size_t end) {
			    for (size_t i = begin; i < end; ++i)
				    state += v[i];
		    },
		    [](int state) { return state; }, options);

		REQUIRE(!partials.empty());
		CHECK(partials.size() <= (size_t)pool.num_threads());
		CHECK(std::accumulate(partials.begin(), partials.end(), 0) ==
		      49'995'000);
	}

	SECTION("bulk_execute finalizes results and destroys scratch on helpers")
	{
		struct Scratch
		{
			std::thread::id owner{};
			std::atomic<int> *destroyed = nullptr;
			std::atomic<int> *destroyed_on_owner = nullptr;
			int processed = 0;

			Scratch() = default;
			Scratch(Scratch const &) = delete;
			Scratch &operator=(Scratch const &) = delete;
			Scratch(Scratch &&other) noexcept
			    : owner(other.owner), destroyed(other.destroyed),
			      destroyed_on_owner(other.destroyed_on_owner),
			      processed(other.processed)
			{
				other.owner = {};
				other.destroyed = nullptr;
				other.destroyed_on_owner = nullptr;
				other.processed = 0;
			}
			Scratch &operator=(Scratch &&other) noexcept
			{
				if (this == &other)
					return *this;
				owner = other.owner;
				destroyed = other.destroyed;
				destroyed_on_owner = other.destroyed_on_owner;
				processed = other.processed;
				other.owner = {};
				other.destroyed = nullptr;
				other.destroyed_on_owner = nullptr;
				other.processed = 0;
				return *this;
			}
			~Scratch()
			{
				if (!destroyed)
					return;
				destroyed->fetch_add(1);
				if (owner != std::thread::id{} &&
				    std::this_thread::get_id() == owner)
					destroyed_on_owner->fetch_add(1);
			}
		};

		util::ThreadPool pool{4};
		std::atomic<int> state_count{0};
		std::atomic<int> destroyed{0};
		std::atomic<int> destroyed_on_owner{0};
		util::bulk_options options;
		options.chunk_size = 17;

		auto results = util::bulk_execute(
		    pool, 257,
		    [&] {
			    Scratch state;
			    state.destroyed = &destroyed;
			    state.destroyed_on_owner = &destroyed_on_owner;
			    state_count.fetch_add(1);
			    return state;
		    },
		    [](Scratch &state, size_t begin, size_t end) {
			    state.owner = std::this_thread::get_id();
			    state.processed += (int)(end - begin);
		    },
		    [](Scratch &&state) { return state.processed; }, options);

		CHECK(results.size() == (size_t)state_count.load());
		CHECK(std::accumulate(results.begin(), results.end(), 0) == 257);
		CHECK(destroyed.load() == state_count.load());
		CHECK(destroyed_on_owner.load() == destroyed.load());
	}

	SECTION("bulk_execute propagates exceptions")
	{
		util::ThreadPool pool{4};
		util::bulk_options options;
		options.chunk_size = 9;

		REQUIRE_THROWS_AS(util::bulk_execute(
		                      pool, 128, [] { return 0; },
		                      [](int &, size_t begin, size_t end) {
			                      for (size_t i = begin; i < end; ++i)
				                      if (i == 42)
					                      throw std::runtime_error("foo");
		                      },
		                      [](int state) { return state; }, options),
		                  std::runtime_error);
	}
}
