#include "catch2/catch_template_test_macros.hpp"
#include "catch2/catch_test_macros.hpp"

#include "util/vector.h"

#include <unordered_map>

// wrapper around int that tracks all special members
struct Int
{
	uint64_t id;

	int data = 0;

	static inline uint64_t global = 0;
	static inline std::unordered_map<uint64_t, int> registry;

	Int() noexcept : id(global++)
	{
		auto [it, ok] = registry.insert({id, data});
		REQUIRE(ok);
	}
	Int(int x) : id(global++), data(x)
	{
		auto [it, ok] = registry.insert({id, data});
		REQUIRE(ok);
	}
	operator int() const { return data; }
	~Int()
	{
		REQUIRE(registry.count(id));
		REQUIRE(registry[id] == data);
		auto ok = registry.erase(id);
		REQUIRE(ok);
	}
	Int(Int const &other) : id(global++), data(other.data)
	{
		auto [it, ok] = registry.insert({id, data});
		REQUIRE(ok);
	}
	Int(Int &&other) noexcept : id(global++), data(other.data)
	{
		auto [it, ok] = registry.insert({id, data});
		REQUIRE(ok);
	}
	Int &operator=(Int const &other)
	{
		REQUIRE(registry.count(id));
		REQUIRE(registry[id] == data);
		data = other.data;
		registry[id] = data;
		return *this;
	}
	Int &operator=(Int &&other) noexcept
	{
		REQUIRE(registry.count(id));
		REQUIRE(registry[id] == data);
		data = other.data;
		registry[id] = data;
		return *this;
	}
};

template <> struct util::is_trivially_relocatable<Int> : std::true_type
{};

TEMPLATE_TEST_CASE("vectors", "[vector]", (util::vector<Int>),
                   (util::stable_vector<Int>), (util::small_vector<Int, 1>),
                   (util::small_vector<Int, 2>), (util::small_vector<Int, 3>),
                   (util::small_vector<Int, 4>), (util::small_vector<Int, 5>),
                   (util::static_vector<Int, 4>), (util::static_vector<Int, 5>))
{
	SECTION("constructors")
	{
		REQUIRE(Int::registry.empty());
		TestType a;
		REQUIRE(a.size() == 0);
		REQUIRE(a.empty());

		auto b = TestType{3, 5, 7};
		REQUIRE(b.size() == 3);
		REQUIRE(b[0] == 3);
		REQUIRE(b[1] == 5);
		REQUIRE(b[2] == 7);

		REQUIRE(TestType(2, Int(9)) == TestType{9, 9});
		REQUIRE(TestType(4) == TestType{0, 0, 0, 0});

		auto c = b;
		REQUIRE(c == b);
		c.push_back(9);
		REQUIRE(c != b);
		REQUIRE(c == TestType{3, 5, 7, 9});

		c.assign(b.begin(), b.end());
		REQUIRE(b == c);
		c.assign(3, 1);
		REQUIRE(c == TestType{1, 1, 1});
	}

	REQUIRE(Int::registry.empty());

	SECTION("insertion")
	{
		REQUIRE(Int::registry.empty());
		TestType a;
		a.push_back(Int(1));
		REQUIRE(a.size() == 1u);
		a.insert(a.end(), {2, 3});
		REQUIRE(a.size() == 3u);
		a.insert(a.begin(), Int(0));
		REQUIRE(a.size() == 4u);
		for (int i = 0; i < 4; ++i)
			REQUIRE(a[i] == i);
		if (TestType::max_size() >= 20)
		{
			a.reserve(20);
			REQUIRE(a.capacity() >= 20u);
		}
		a.clear();
		REQUIRE(Int::registry.empty());
	}
}

// small_vector should optimally only store buffer + 4 bytes, but not screw up
// alignment.
static_assert(sizeof(util::small_vector<int32_t, 3>) == 4 * 4);
static_assert(sizeof(util::small_vector<int32_t, 7>) == 8 * 4);
static_assert(alignof(util::small_vector<int *, 1>) == alignof(int *));
static_assert(sizeof(util::small_vector<int *, 1>) % alignof(int *) == 0);
static_assert(alignof(util::small_vector<char, 1>) == alignof(char *));
static_assert(sizeof(util::small_vector<char, 1>) % alignof(char *) == 0);
struct alignas(32) Foo
{};
static_assert(sizeof(util::small_vector<Foo, 1>) == 2 * 32);
static_assert(alignof(util::small_vector<Foo, 1>) == 32);

TEST_CASE("tiny_map", "[vector][tiny_map]")
{
	util::tiny_map<std::string, int> a;
	CHECK(a.size() == 0);
	CHECK(a.empty());
	a["one"] = 1;
	CHECK(a.size() == 1);
	a["two"] = 2;
	a["one"] = 3;
	CHECK(a.size() == 2);
	CHECK(a["two"] == 2);
	CHECK(a["foo"] == 0);
	CHECK(a.size() == 3);
}
