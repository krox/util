#include "util/threadpool.h"
#include "catch2/catch_test_macros.hpp"
#include "fmt/format.h"
#include <chrono>
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
}
