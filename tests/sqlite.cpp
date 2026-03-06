#include "catch2/catch_test_macros.hpp"

#include "util/sqlite.h"

#include <tuple>
#include <vector>

namespace {
struct MyRow
{
	int a;
	std::string b;
	double c;
};

template <size_t I> decltype(auto) get(MyRow &row)
{
	if constexpr (I == 0)
		return (row.a);
	else if constexpr (I == 1)
		return (row.b);
	else
		return (row.c);
}

template <size_t I> decltype(auto) get(const MyRow &row)
{
	if constexpr (I == 0)
		return (row.a);
	else if constexpr (I == 1)
		return (row.b);
	else
		return (row.c);
}

template <size_t I> decltype(auto) get(MyRow &&row)
{
	if constexpr (I == 0)
		return std::move(row.a);
	else if constexpr (I == 1)
		return std::move(row.b);
	else
		return std::move(row.c);
}
} // namespace

namespace std {
template <> struct tuple_size<MyRow> : integral_constant<size_t, 3>
{};
template <> struct tuple_element<0, MyRow>
{
	using type = int;
};
template <> struct tuple_element<1, MyRow>
{
	using type = std::string;
};
template <> struct tuple_element<2, MyRow>
{
	using type = double;
};
} // namespace std

TEST_CASE("sqlite in-memory query collection", "[sqlite]")
{
	util::Sqlite db(":memory:");

	db.execute("CREATE TABLE my_table(a INTEGER, b TEXT, c REAL)");
	db.execute("INSERT INTO my_table VALUES (1, 'foo', 1.5)");
	db.execute("INSERT INTO my_table VALUES (2, 'bar', 2.5)");

	auto rows = db.query<std::tuple<int, std::string, double>>(
	    "SELECT a, b, c FROM my_table ORDER BY a");

	REQUIRE(rows.size() == 2);
	CHECK(rows[0] == std::tuple<int, std::string, double>{1, "foo", 1.5});
	CHECK(rows[1] == std::tuple<int, std::string, double>{2, "bar", 2.5});
}

TEST_CASE("sqlite callback query", "[sqlite]")
{
	util::Sqlite db(":memory:");
	db.execute("CREATE TABLE my_table(a INTEGER)");
	db.execute("INSERT INTO my_table VALUES (1)");
	db.execute("INSERT INTO my_table VALUES (2)");

	std::vector<int> rows;
	auto callback = [&](int a) { rows.push_back(a); };
	auto stmt = db.prepare("SELECT a FROM my_table ORDER BY a");
	while (stmt.step())
		callback(stmt.column<int>(0));
	CHECK(rows == std::vector<int>{1, 2});
}

TEST_CASE("sqlite callback query multiple columns", "[sqlite]")
{
	util::Sqlite db(":memory:");
	db.execute("CREATE TABLE my_table(a INTEGER, b TEXT)");
	db.execute("INSERT INTO my_table VALUES (1, 'one')");
	db.execute("INSERT INTO my_table VALUES (2, 'two')");

	std::vector<std::tuple<int, std::string>> rows;
	db.query<int, std::string>(
	    "SELECT a, b FROM my_table ORDER BY a",
	    [&](int a, std::string b) { rows.emplace_back(a, b); });

	REQUIRE(rows.size() == 2);
	CHECK(rows[0] == std::tuple<int, std::string>{1, "one"});
	CHECK(rows[1] == std::tuple<int, std::string>{2, "two"});
}

TEST_CASE("sqlite statement step/row", "[sqlite]")
{
	util::Sqlite db(":memory:");
	db.execute("CREATE TABLE my_table(a INTEGER)");
	db.execute("INSERT INTO my_table VALUES (1)");

	auto stmt = db.prepare("SELECT a FROM my_table");
	REQUIRE(stmt.step());
	CHECK(std::get<0>(stmt.row<std::tuple<int>>()) == 1);
	CHECK(!stmt.step());
}

TEST_CASE("sqlite prepared statements", "[sqlite]")
{
	util::Sqlite db(":memory:");
	db.execute("CREATE TABLE my_table(a INTEGER, b TEXT, c REAL)");

	{
		auto stmt =
		    db.prepare("INSERT INTO my_table(a, b, c) VALUES (?, ?, ?)");
		stmt.bind(1, 10);
		stmt.bind(2, "ten");
		stmt.bind(3, 10.5);
		CHECK(!stmt.step());

		stmt.reset();
		stmt.clear_bindings();
		stmt.bind(1, 20);
		stmt.bind(2, "twenty");
		stmt.bind(3, 20.5);
		CHECK(!stmt.step());
	}

	auto query =
	    db.prepare("SELECT a, b, c FROM my_table WHERE a >= ? ORDER BY a ASC");
	query.bind(1, 10);

	std::vector<std::tuple<int, std::string, double>> rows;
	while (query.step())
		rows.push_back(query.row<std::tuple<int, std::string, double>>());

	REQUIRE(rows.size() == 2);
	CHECK(rows[0] == std::tuple<int, std::string, double>{10, "ten", 10.5});
	CHECK(rows[1] == std::tuple<int, std::string, double>{20, "twenty", 20.5});
}

TEST_CASE("sqlite custom tuple-like row type", "[sqlite]")
{
	util::Sqlite db(":memory:");
	db.execute("CREATE TABLE my_table(a INTEGER, b TEXT, c REAL)");
	db.execute("INSERT INTO my_table VALUES (7, 'seven', 7.25)");

	auto rows = db.query<MyRow>("SELECT a, b, c FROM my_table");
	REQUIRE(rows.size() == 1);
	auto out = rows.front();
	auto [a, b, c] = out;
	CHECK(a == 7);
	CHECK(b == "seven");
	CHECK(c == 7.25);
}

TEST_CASE("sqlite query_one convenience", "[sqlite]")
{
	util::Sqlite db(":memory:");
	db.execute("CREATE TABLE my_table(a INTEGER, b TEXT, c REAL)");
	db.execute("INSERT INTO my_table VALUES (1, 'one', 1.5)");

	auto row = db.query_one<std::tuple<int, std::string, double>>(
	    "SELECT a, b, c FROM my_table");
	CHECK(row == std::tuple<int, std::string, double>{1, "one", 1.5});

	CHECK_THROWS_AS((db.query_one<std::tuple<int, std::string, double>>(
	                    "SELECT a, b, c FROM my_table WHERE a = 2")),
	                util::SqliteError);

	db.execute("INSERT INTO my_table VALUES (2, 'two', 2.5)");
	CHECK_THROWS_AS((db.query_one<std::tuple<int, std::string, double>>(
	                    "SELECT a, b, c FROM my_table")),
	                util::SqliteError);
}
