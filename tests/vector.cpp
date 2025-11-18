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

TEST_CASE("vector_map", "[vector][vector_map]")
{
	SECTION("basic functionality")
	{
		util::vector_map<int> map;

		// Initially empty
		CHECK(map.size() == 0);
		CHECK(map.empty());

		// Access out-of-bounds should auto-grow
		map[5] = 42;
		CHECK(map.size() == 6);
		CHECK(map[5] == 42);

		// Elements before should be default-constructed (0 for int)
		for (size_t i = 0; i < 5; ++i)
			CHECK(map[i] == 0);
	}

	SECTION("const access")
	{
		util::vector_map<int> map;
		map[2] = 99;

		const auto &cmap = map;
		CHECK(cmap[2] == 99);
		CHECK(cmap[10] == 0);   // Should not grow, just return default
		CHECK(map.size() == 3); // Size should not have changed
	}

	SECTION("iterator support")
	{
		util::vector_map<std::string> map;
		map[1] = "one";
		map[3] = "three";
		map[5] = "five";

		// Test basic access
		CHECK(map[1] == "one");
		CHECK(map[3] == "three");
		CHECK(map[5] == "five");
		CHECK(map.size() == 6);

		// Test values() access
		auto &values = map.values();
		CHECK(values.size() == 6);
		CHECK(values[1] == "one");
		CHECK(values[3] == "three");
		CHECK(values[5] == "five");

		// Test pair iteration (std::map compatible)
		std::vector<std::pair<size_t, std::string>> pairs;
		for (auto it = map.begin(); it != map.end(); ++it)
			pairs.emplace_back(*it);

		CHECK(pairs.size() == 6);
		CHECK(pairs[0].first == 0);
		CHECK(pairs[0].second == "");
		CHECK(pairs[1].first == 1);
		CHECK(pairs[1].second == "one");
		CHECK(pairs[2].first == 2);
		CHECK(pairs[2].second == "");
		CHECK(pairs[3].first == 3);
		CHECK(pairs[3].second == "three");
		CHECK(pairs[4].first == 4);
		CHECK(pairs[4].second == "");
		CHECK(pairs[5].first == 5);
		CHECK(pairs[5].second == "five");

		// Test structured binding
		size_t count = 0;
		for (auto [key, value] : map)
		{
			// the '.get()' is needed because 'value' is a
			// std::reference_wrapper and not a proper reference. Will get
			// proper 'reference_wrapper::operator==' in C++26
			if (key == 1)
				CHECK(value.get() == "one");
			if (key == 3)
				CHECK(value.get() == "three");
			if (key == 5)
				CHECK(value.get() == "five");
			count++;
		}
		CHECK(count == 6);

		// Test values() iteration
		size_t non_empty_count = 0;
		for (const auto &value : map.values())
		{
			if (!value.empty())
				non_empty_count++;
		}
		CHECK(non_empty_count == 3);
	}

	SECTION("values() iteration")
	{
		util::vector_map<int> map;
		map[1] = 10;
		map[3] = 30;

		// Test direct value iteration
		std::vector<int> values;
		for (int value : map.values())
			values.push_back(value);

		CHECK(values.size() == 4);
		CHECK(values[0] == 0);  // default
		CHECK(values[1] == 10); // set
		CHECK(values[2] == 0);  // default
		CHECK(values[3] == 30); // set

		// Test modification through values()
		map.values()[2] = 20;
		CHECK(map[2] == 20);
	}
}

TEST_CASE("vector_multimap basic operations", "[vector_multimap]")
{
	util::vector_multimap<std::string> vm;

	REQUIRE(vm.count_elements() == 0);
	REQUIRE(vm.count_used_keys() == 0);

	// Add some values
	vm.insert(0, "hello");
	vm.insert(0, "world");
	vm.insert(2, "foo");
	vm.insert(2, "bar");
	vm.insert(2, "baz");

	REQUIRE(vm.count_elements() == 5);
	REQUIRE(vm.count_used_keys() == 2); // keys 0 and 2

	// Test element access
	auto span0 = vm[0];
	REQUIRE(span0.size() == 2);
	REQUIRE(span0[0] == "hello");
	REQUIRE(span0[1] == "world");

	auto span1 = vm[1]; // empty key
	REQUIRE(span1.empty());

	auto span2 = vm[2];
	REQUIRE(span2.size() == 3);
	REQUIRE(span2[0] == "foo");
	REQUIRE(span2[1] == "bar");
	REQUIRE(span2[2] == "baz");

	// Test out of bounds access
	auto span10 = vm[10];
	REQUIRE(span10.empty());
}

TEST_CASE("vector_multimap move semantics", "[vector_multimap]")
{
	util::vector_multimap<std::string> vm;

	std::string test_str = "movable";
	vm.insert(0, std::move(test_str));

	REQUIRE(vm[0].size() == 1);
	REQUIRE(vm[0][0] == "movable");
	REQUIRE(test_str.empty()); // moved from
}

TEST_CASE("vector_multimap emplace", "[vector_multimap]")
{
	util::vector_multimap<std::pair<int, std::string>> vm;

	vm.emplace(0, 42, "test");
	vm.emplace(0, 100, "another");

	REQUIRE(vm[0].size() == 2);
	REQUIRE(vm[0][0].first == 42);
	REQUIRE(vm[0][0].second == "test");
	REQUIRE(vm[0][1].first == 100);
	REQUIRE(vm[0][1].second == "another");
}

TEST_CASE("vector_multimap query operations", "[vector_multimap]")
{
	util::vector_multimap<int> vm;

	vm.insert(0, 10);
	vm.insert(0, 20);
	vm.insert(2, 30);

	REQUIRE(vm[0].size() == 2);
	REQUIRE(vm[1].size() == 0);
	REQUIRE(vm[2].size() == 1);
	REQUIRE(vm[5].size() == 0);
}

TEST_CASE("vector_multimap erase operations", "[vector_multimap]")
{
	util::vector_multimap<int> vm;

	// Set up test data
	vm.insert(0, 10);
	vm.insert(0, 21); // odd number
	vm.insert(0, 10); // duplicate
	vm.insert(0, 33); // odd number
	vm.insert(1, 40);
	vm.insert(1, 50);

	SECTION("erase all occurrences of value")
	{
		size_t erased = vm.erase(0, 10);
		REQUIRE(erased == 2); // removed both 10s
		auto span = vm[0];
		REQUIRE(span.size() == 2);
		REQUIRE(span[0] == 21);
		REQUIRE(span[1] == 33);

		// Erase non-existent value
		REQUIRE(vm.erase(0, 999) == 0);
		// Erase from non-existent key
		REQUIRE(vm.erase(99, 10) == 0);
	}

	SECTION("erase_if with predicate")
	{
		// Remove all even numbers from key 0
		size_t erased = vm.erase_if(0, [](int x) { return x % 2 == 0; });
		REQUIRE(erased == 2); // removed 10, 10
		auto span = vm[0];
		REQUIRE(span.size() == 2);
		REQUIRE(span[0] == 21);
		REQUIRE(span[1] == 33);

		// Key 1 should be unchanged
		REQUIRE(vm[1].size() == 2);

		// Erase from non-existent key
		REQUIRE(vm.erase_if(99, [](int) { return true; }) == 0);
	}

	SECTION("erase_one")
	{
		vm.erase_one(0, 10); // Remove exactly one 10
		auto span = vm[0];
		REQUIRE(span.size() == 3);
		// Should still have one 10, plus 20 and 30
		REQUIRE(std::count(span.begin(), span.end(), 10) == 1);

		// Try to erase non-existent value
		REQUIRE_THROWS_AS(vm.erase_one(0, 999), std::runtime_error);
		// Try to erase from non-existent key
		REQUIRE_THROWS_AS(vm.erase_one(99, 10), std::runtime_error);
	}
}

TEST_CASE("vector_multimap unique_sort", "[vector_multimap]")
{
	util::vector_multimap<int> vm;

	// Add unsorted data with duplicates
	vm.insert(0, 30);
	vm.insert(0, 10);
	vm.insert(0, 20);
	vm.insert(0, 10); // duplicate
	vm.insert(0, 30); // duplicate

	vm.unique_sort(0);

	auto span = vm[0];
	REQUIRE(span.size() == 3); // duplicates removed
	REQUIRE(span[0] == 10);    // sorted
	REQUIRE(span[1] == 20);
	REQUIRE(span[2] == 30);

	// unique_sort on non-existent key should not crash
	vm.unique_sort(99);

	SECTION("unique_sort with custom comparator")
	{
		util::vector_multimap<int> vm2;
		vm2.insert(0, 30);
		vm2.insert(0, 10);
		vm2.insert(0, 20);
		vm2.insert(0, 10);

		// Sort in descending order
		vm2.unique_sort(0, std::greater<int>());

		auto span2 = vm2[0];
		REQUIRE(span2.size() == 3);
		REQUIRE(span2[0] == 30);
		REQUIRE(span2[1] == 20);
		REQUIRE(span2[2] == 10);
	}
}

TEST_CASE("vector_multimap reference stability via spans", "[vector_multimap]")
{
	util::vector_multimap<int> vm;

	// Add initial values
	vm.insert(0, 100);
	vm.insert(0, 200);

	// Get a span to the values
	auto span = vm[0];
	REQUIRE(span.size() == 2);
	REQUIRE(span[0] == 100);
	REQUIRE(span[1] == 200);

	// Add many more values to force reallocation of outer vector
	for (int i = 3; i < 100; ++i)
	{
		vm.insert(i, i * 10);
	}

	// The span should still be valid and contain the same data
	// (this is the key safety feature - spans remain valid)
	REQUIRE(span.size() == 2);
	REQUIRE(span[0] == 100);
	REQUIRE(span[1] == 200);

	// But getting a new span should still work
	auto new_span = vm[0];
	REQUIRE(new_span.size() == 2);
	REQUIRE(new_span[0] == 100);
	REQUIRE(new_span[1] == 200);
}

TEST_CASE("vector_multimap reserve operations", "[vector_multimap]")
{
	util::vector_multimap<int> vm;

	vm.insert(5, 42);

	for (int i = 0; i < 50; ++i)
		vm.insert(5, i);

	REQUIRE(vm[5].size() == 51); // 1 initial + 50 added
}

TEST_CASE("vector_multimap clear", "[vector_multimap]")
{
	util::vector_multimap<std::string> vm;

	vm.insert(0, "test");
	vm.insert(1, "data");

	vm.clear();
	REQUIRE(vm.count_elements() == 0);
	REQUIRE(vm.count_used_keys() == 0);
}
