#include "catch2/catch_approx.hpp"
#include "catch2/catch_test_macros.hpp"

#include "util/memory.h"
#include "util/string_id.h"
#include <unordered_set>
#include <vector>

using namespace util;

// testing class that tracks all special member functions
struct Foo
{
	uint64_t id;

	static inline uint64_t global = 0;
	static inline std::unordered_set<uint64_t> registry;

	Foo() : id(global++)
	{
		auto [it, ok] = registry.insert(id);
		assert(ok);
	}
	~Foo()
	{
		auto ok = registry.erase(id);
		assert(ok);
	}
	Foo(Foo const &) : id(global++)
	{
		auto [it, ok] = registry.insert(id);
		assert(ok);
	}
	Foo(Foo &&) noexcept : id(global++)
	{
		auto [it, ok] = registry.insert(id);
		assert(ok);
	}
	Foo &operator=(Foo const &)
	{
		auto ok = registry.erase(id);
		assert(ok);
		id = global++;
		auto [it, ok2] = registry.insert(id);
		assert(ok2);
		return *this;
	}
	Foo &operator=(Foo &&) noexcept
	{
		auto ok = registry.erase(id);
		assert(ok);
		id = global++;
		auto [it, ok2] = registry.insert(id);
		assert(ok2);
		return *this;
	}
};

TEST_CASE("memory")
{
	{
		MonotoneMemoryPool pool;
		auto a = std::vector<Foo, MonotoneAllocator<Foo>>(
		    MonotoneAllocator<Foo>(pool));
		a.resize(1);
		a.resize(0);

		for (int i = 1; i <= 20; ++i)
		{
			a.push_back(Foo());
			REQUIRE(Foo::registry.size() == a.size());
		}
	}
	REQUIRE(Foo::registry.size() == 0);
}

TEST_CASE("string_id")
{
	StringPool pool;
	CHECK(pool("foo").id() == 1);
	CHECK(pool("bar").id() == 2);
	CHECK(pool("foo").id() == 1);
	CHECK(pool("").id() == 0);
	CHECK((pool(pool("foobar")) == "foobar"));
}
