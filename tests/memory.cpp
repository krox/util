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
	auto a = make_unique_array<int>(3, 7);
	CHECK(a.size() == 3);
	CHECK(a[1] == 7);
}
