#include "catch2/catch_test_macros.hpp"

#include "util/atomic.h"

#include <chrono>
#include <future>
#include <string>
#include <thread>
#include <type_traits>

namespace {

struct non_lock_free_payload
{
	long long data[4];
};

struct move_only_payload
{
	int value;
	std::string label;

	move_only_payload(int value_, std::string label_) noexcept
	    : value(value_), label(std::move(label_))
	{}

	move_only_payload(move_only_payload const &) = delete;
	move_only_payload &operator=(move_only_payload const &) = delete;
	move_only_payload(move_only_payload &&) noexcept = default;
	move_only_payload &operator=(move_only_payload &&) noexcept = default;
};

template <class T>
concept has_relaxed_atomic = requires { typename util::relaxed_atomic<T>; };

static_assert(has_relaxed_atomic<int>);
static_assert(has_relaxed_atomic<std::chrono::nanoseconds>);
static_assert(
    has_relaxed_atomic<std::chrono::time_point<std::chrono::steady_clock>>);
static_assert(!has_relaxed_atomic<non_lock_free_payload>);

} // namespace

TEST_CASE("relaxed_atomic supports basic scalar operations")
{
	util::relaxed_atomic<int> value{3};

	CHECK(value.load() == 3);
	CHECK(value.exchange(7) == 3);
	CHECK(value.load() == 7);
	CHECK(value.fetch_add(5) == 7);
	CHECK(value.load() == 12);
	CHECK((value += 2) == 14);

	int expected = 14;
	CHECK(value.compare_exchange_strong(expected, 20));
	CHECK(value.load() == 20);

	expected = 14;
	CHECK_FALSE(value.compare_exchange_weak(expected, 30));
	CHECK(expected == 20);
}

TEST_CASE("relaxed_atomic duration specialization supports arithmetic")
{
	using namespace std::chrono_literals;

	util::relaxed_atomic<std::chrono::nanoseconds> value{5ns};

	CHECK(value.load() == 5ns);
	CHECK(value.fetch_add(7ns) == 5ns);
	CHECK(value.load() == 12ns);
	CHECK((value += 8ns) == 20ns);
	CHECK(value.fetch_sub(6ns) == 20ns);
	CHECK(value.load() == 14ns);

	auto expected = 14ns;
	CHECK(value.compare_exchange_strong(expected, 30ns));
	CHECK(value.load() == 30ns);
}

TEST_CASE(
    "relaxed_atomic time_point specialization supports duration increments")
{
	using namespace std::chrono_literals;
	using time_point = std::chrono::time_point<std::chrono::steady_clock>;

	util::relaxed_atomic<time_point> value{time_point{10ns}};

	CHECK(value.load() == time_point{10ns});
	CHECK(value.fetch_add(15ns) == time_point{10ns});
	CHECK(value.load() == time_point{25ns});
	CHECK((value += 5ns) == time_point{30ns});
	CHECK(value.fetch_sub(12ns) == time_point{30ns});
	CHECK(value.load() == time_point{18ns});

	auto expected = time_point{18ns};
	CHECK(value.compare_exchange_weak(expected, time_point{42ns}));
	CHECK(value.load() == time_point{42ns});
}
