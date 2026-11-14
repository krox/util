#include "catch2/catch_test_macros.hpp"

#include "util/jagged_vector.h"

#include <algorithm>
#include <string>
#include <vector>

namespace {

struct Probe
{
	inline static int alive = 0;
	int value = 0;

	Probe() { ++alive; }
	explicit Probe(int v) : value(v) { ++alive; }
	Probe(Probe const &other) : value(other.value) { ++alive; }
	Probe(Probe &&other) noexcept : value(other.value) { ++alive; }
	Probe &operator=(Probe const &other)
	{
		value = other.value;
		return *this;
	}
	Probe &operator=(Probe &&other) noexcept
	{
		value = other.value;
		return *this;
	}
	~Probe() { --alive; }

	bool operator==(Probe const &other) const { return value == other.value; }
};

} // namespace

TEST_CASE("jagged_vector starts empty", "[jagged_vector]")
{
	util::jagged_vector<int> rows;
	CHECK(rows.empty());
	CHECK(rows.size() == 0);
	CHECK(rows.flat_size() == 0);
	CHECK(rows.flat().empty());
	CHECK(rows.begin() == rows.end());
}

TEST_CASE("jagged_vector push_back keeps variable-length rows contiguous",
          "[jagged_vector]")
{
	util::jagged_vector<int> rows;
	rows.reserve(3, 6);

	auto a = std::vector<int>{1, 2};
	auto b = std::vector<int>{};
	auto c = std::vector<int>{3, 4, 5};
	rows.push_back(a);
	rows.push_back(b);
	rows.push_back(c);

	CHECK(rows.size() == 3);
	CHECK(rows.flat_size() == 5);
	CHECK(std::ranges::equal(rows.flat(), std::vector<int>{1, 2, 3, 4, 5}));
	CHECK(std::ranges::equal(rows[0], std::vector<int>{1, 2}));
	CHECK(rows[1].empty());
	CHECK(std::ranges::equal(rows[2], std::vector<int>{3, 4, 5}));
	CHECK(std::ranges::equal(rows.front(), std::vector<int>{1, 2}));
	CHECK(std::ranges::equal(rows.back(), std::vector<int>{3, 4, 5}));
	CHECK(rows.data() == rows.flat().data());

	rows[0][1] = 9;
	CHECK(rows.flat()[1] == 9);
}

TEST_CASE("jagged_vector push_back fills a row from one value", "[jagged_vector]")
{
	util::jagged_vector<std::string> rows;
	rows.push_back(3, "x");
	rows.push_back(0, "y");
	rows.push_back(1, "z");

	CHECK(rows.size() == 3);
	CHECK(std::ranges::equal(rows[0], std::vector<std::string>{"x", "x", "x"}));
	CHECK(rows[1].empty());
	CHECK(std::ranges::equal(rows[2], std::vector<std::string>{"z"}));
}

TEST_CASE("jagged_vector pop_back and clear drop the trailing row",
          "[jagged_vector]")
{
	util::jagged_vector<int> rows;
	rows.push_back(std::vector<int>{1});
	rows.push_back(std::vector<int>{2, 3});
	rows.push_back(std::vector<int>{});

	rows.pop_back();
	CHECK(rows.size() == 2);
	CHECK(std::ranges::equal(rows.flat(), std::vector<int>{1, 2, 3}));

	rows.pop_back();
	CHECK(rows.size() == 1);
	CHECK(std::ranges::equal(rows[0], std::vector<int>{1}));

	rows.pop_back();
	CHECK(rows.empty());
	CHECK(rows.flat_size() == 0);

	rows.push_back(std::vector<int>{4, 5});
	rows.clear();
	CHECK(rows.empty());
	CHECK(rows.flat_size() == 0);
	rows.push_back(std::vector<int>{6});
	CHECK(std::ranges::equal(rows[0], std::vector<int>{6}));
}

TEST_CASE("jagged_vector iterates rows", "[jagged_vector]")
{
	util::jagged_vector<int> rows;
	rows.push_back(std::vector<int>{1});
	rows.push_back(std::vector<int>{2, 3});

	int sum = 0;
	for (auto row : rows)
		for (int x : row)
			sum += x;
	CHECK(sum == 6);

	util::jagged_vector<int> const &frozen = rows;
	CHECK(frozen.begin() + 2 == frozen.end());
	CHECK(std::ranges::equal(*(frozen.begin() + 1), std::vector<int>{2, 3}));
	CHECK(std::ranges::equal(frozen.cbegin()[0], std::vector<int>{1}));
}

TEST_CASE("jagged_vector copy compares by rows", "[jagged_vector]")
{
	util::jagged_vector<int> rows;
	rows.push_back(std::vector<int>{1, 2});
	rows.push_back(std::vector<int>{3});

	auto copy = rows;
	CHECK(copy == rows);
	copy[1][0] = 9;
	CHECK(copy != rows);
	CHECK(std::ranges::equal(rows[1], std::vector<int>{3}));
}

TEST_CASE("jagged_vector destroys elements when rows are removed",
          "[jagged_vector]")
{
	REQUIRE(Probe::alive == 0);
	{
		util::jagged_vector<Probe> rows;
		auto input = std::vector<Probe>{Probe(1), Probe(2)};
		rows.push_back(input);
		rows.push_back(2, Probe(3));
		CHECK(Probe::alive == 6); // 2 in input + 2 copied + 2 filled

		rows.pop_back();
		CHECK(Probe::alive == 4);
		rows.clear();
		CHECK(Probe::alive == 2);
	}
	CHECK(Probe::alive == 0);
}
