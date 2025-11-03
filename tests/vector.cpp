#include "catch2/catch_template_test_macros.hpp"
#include "catch2/catch_test_macros.hpp"

#include "util/vector.h"
#include "util/vector2d.h"

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
                   (util::static_vector<Int, 4>), (util::static_vector<Int, 5>),
                   (util::indirect_vector<Int>))
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

	SECTION("comparison")
	{
		auto a = TestType{};
		auto b = TestType{1};
		auto c = TestType{1, 2};
		auto d = TestType{2};
		CHECK((a == a && b == b && c == c && d == d));
		CHECK((a != b && a != c && a != d && b != c && b != d && c != d));
		CHECK((a < b && a < c && a < d && b < c && b < d && c < d));
	}

	SECTION("resize")
	{
		TestType a;
		CHECK(a.size() == 0);
		a.push_back(1);
		CHECK(a.size() == 1);
		a.resize(3);
		REQUIRE(a.size() == 3);
		CHECK(a[0] == 1);
	}

	SECTION("utility functions")
	{
		TestType a, b;
		a.push_back(1);
		a.push_back(2);
		append(b, std::span<const Int>(a.begin(), a.end()));
		append(b, std::span<const Int>(a.begin(), a.end()));
		trim(b, 1);
		CHECK(b.size() == 3);
		erase(b, 2);
		CHECK(b.size() == 1);
		b.emplace_back(2);
		b.emplace_back(3);
		erase_if(b, [](auto const &x) { return x == 2; });
		CHECK((b.size() == 2 && b[0] == 1 && b[1] == 3));
	}
}

TEST_CASE("vector of string", "[vector]")
{
	util::vector<std::string> a;
	a.push_back("zero");
	a.resize(5);
	REQUIRE(a.size() == 5);
	CHECK(a[0] == "zero");
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

// the whole point of indirect_vector is its tiny size on stack
static_assert(sizeof(util::indirect_vector<int>) == sizeof(int *));

TEST_CASE("tiny_map", "[vector][tiny_map]")
{
	SECTION("misc")
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

	SECTION("comparison")
	{
		util::tiny_map<int, int> a, b, c;
		a[1] = 1;
		a[2] = 2;
		b[2] = 2;
		b[1] = 1;
		c[1] = 2;
		c[2] = 1;
		CHECK(a == b);
		CHECK(a != c);
		CHECK(b != c);
	}
}

TEST_CASE("vector2d", "[vector][vector2d]")
{
	util::vector2d<int> a;
	CHECK(a.empty());
	CHECK(a.height() == 0);
	CHECK(a.width() == 0);
	CHECK(a.size() == 0);

	a.push_back({{1, 2, 3}});
	CHECK(!a.empty());
	CHECK(a.height() == 1);
	CHECK(a.width() == 3);
	CHECK(a.size() == 3);
	CHECK(a[0][0] == 1);
	CHECK(a[0][1] == 2);
	CHECK(a[0][2] == 3);

	a.push_back({{4, 5, 6}});
	CHECK(a.height() == 2);
	CHECK(a.width() == 3);
	CHECK(a.size() == 6);
	CHECK(a[1][0] == 4);
	CHECK(a[1][1] == 5);
	CHECK(a[1][2] == 6);

	a.push_back({{7, 8, 9}});
	CHECK(a.height() == 3);
	CHECK(a.width() == 3);
	CHECK(a.size() == 9);
	CHECK(a[2][0] == 7);
	CHECK(a[2][1] == 8);
	CHECK(a[2][2] == 9);

	a.pop_back();
	CHECK(a.height() == 2);
	CHECK(a.width() == 3);
	CHECK(a.size() == 6);

	a.pop_back();
	a.pop_back();
	CHECK(a.height() == 0);

	a.push_back({{1, 2}});
	CHECK(a.height() == 1);
	CHECK(a.width() == 2);

	a.clear();
	CHECK(a.empty());
	CHECK(a.height() == 0);
	CHECK(a.size() == 0);
}
