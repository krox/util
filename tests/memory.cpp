#include "catch2/catch_approx.hpp"
#include "catch2/catch_test_macros.hpp"

#include "util/memory.h"
#include "util/random.h"
#include "util/string_id.h"

using namespace util;

TEST_CASE("string_id")
{
	StringPool pool;
	CHECK(pool("foo").id() == 1);
	CHECK(pool("bar").id() == 2);
	CHECK(pool("foo").id() == 1);
	CHECK(pool("").id() == 0);
	CHECK((pool(pool("foobar")) == "foobar"));
}

TEST_CASE("lazy allocation", "[memory]")
{
	// allocate huge amount of memory and do some random read/write
	size_t length = 1LL << 40;
	auto mem = lazy_allocate<int>(length);
	xoshiro256 rng;

	rng.seed(0);
	for (int i = 0; i < 1000; ++i)
	{
		auto pos = rng() % length;
		mem[pos] = (int)rng();
	}

	rng.seed(0);
	for (int i = 0; i < 1000; ++i)
	{
		auto pos = rng() % length;
		CHECK(mem[pos] == (int)rng());
	}
}

struct Foo
{
	Foo(Foo const &){};
};

static_assert(is_trivially_relocatable_v<int>);
static_assert(is_trivially_relocatable_v<int *>);
static_assert(is_trivially_relocatable_v<std::string_view>);
static_assert(!is_trivially_relocatable_v<std::string>);
static_assert(!is_trivially_relocatable_v<Foo>);

TEST_CASE("unique_array", "[memory]")
{
	auto a = make_unique_span<int>(3, 7);
	a[0] = 5;
	CHECK(a.size() == 3);
	CHECK(a.at(0) == 5);
	CHECK(a[1] == 7);
	auto vec = std::vector(a.rbegin(), a.rend());
	CHECK(vec == std::vector{7, 7, 5});
}

TEST_CASE("Default constructed value_ptr is null")
{
	value_ptr<int> p;
	REQUIRE_FALSE(p);
	REQUIRE(p.get() == nullptr);
}

TEST_CASE("Construction with make_value")
{
	auto p = make_value<std::string>("hello");
	REQUIRE(*p == "hello");
	REQUIRE(p);
}

TEST_CASE("Copy constructs a deep copy")
{
	auto p1 = make_value<std::string>("world");
	auto p2 = p1; // deep copy
	REQUIRE(*p1 == *p2);
	REQUIRE(p1.get() != p2.get()); // different objects
}

TEST_CASE("Move transfers ownership")
{
	auto p1 = make_value<int>(42);
	auto p2 = std::move(p1);
	REQUIRE(p2);
	REQUIRE_FALSE(p1);
	REQUIRE(*p2 == 42);
}

TEST_CASE("Copy assignment replaces contents with deep copy")
{
	auto p1 = make_value<int>(10);
	auto p2 = make_value<int>(20);
	p2 = p1; // deep copy
	REQUIRE(*p1 == *p2);
	REQUIRE(p1.get() != p2.get());
}

TEST_CASE("Move assignment replaces contents with transfer")
{
	auto p1 = make_value<int>(99);
	auto p2 = make_value<int>(100);
	p2 = std::move(p1);
	REQUIRE(*p2 == 99);
	REQUIRE_FALSE(p1);
}

TEST_CASE("Reset and swap work as expected")
{
	auto p1 = make_value<int>(5);
	auto p2 = make_value<int>(10);

	p1.swap(p2);
	REQUIRE(*p1 == 10);
	REQUIRE(*p2 == 5);

	p1.reset();
	REQUIRE_FALSE(p1);
}

TEST_CASE("Pointer comparison semantics")
{
	auto p1 = make_value<int>(123);
	auto p2 = make_value<int>(123);

	REQUIRE_FALSE(p1 == p2); // different addresses
	REQUIRE(*p1 == *p2);     // same values
}

// test that value_ptr works with incomplete types (as long as its special
// members are not actually instantiated)
struct Incomplete;
struct Holder
{
	value_ptr<Incomplete> p;
	~Holder();
};
