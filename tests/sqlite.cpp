#include "catch2/catch_test_macros.hpp"

#include "util/sqlite.h"

#include <cstdint>
#include <optional>
#include <tuple>
#include <vector>

TEST_CASE("sqlite in-memory query collection", "[sqlite]")
{
	util::Sqlite db(":memory:");

	db.execute("CREATE TABLE my_table(a INTEGER, b TEXT, c REAL)");
	db.execute("INSERT INTO my_table VALUES (1, 'foo', 1.5)");
	db.execute("INSERT INTO my_table VALUES (2, 'bar', 2.5)");

	std::vector<std::tuple<int, std::string, double>> rows;
	db.query<int, std::string, double>(
	    "SELECT a, b, c FROM my_table ORDER BY a",
	    [&](int a, std::string b, double c) { rows.emplace_back(a, b, c); });

	REQUIRE(rows.size() == 2);
	CHECK(rows[0] == std::tuple<int, std::string, double>{1, "foo", 1.5});
	CHECK(rows[1] == std::tuple<int, std::string, double>{2, "bar", 2.5});
	auto [a, b, c] = rows[0];
	CHECK(a == 1);
	CHECK(b == "foo");
	CHECK(c == 1.5);
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
	CHECK(stmt.column<int>(0) == 1);
	CHECK(stmt.row<int>() == std::tuple<int>{1});
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
		rows.push_back(query.row<int, std::string, double>());

	REQUIRE(rows.size() == 2);
	CHECK(rows[0] == std::tuple<int, std::string, double>{10, "ten", 10.5});
	CHECK(rows[1] == std::tuple<int, std::string, double>{20, "twenty", 20.5});
}

TEST_CASE("sqlite query_one convenience", "[sqlite]")
{
	util::Sqlite db(":memory:");
	db.execute("CREATE TABLE my_table(a INTEGER, b TEXT, c REAL)");
	db.execute("INSERT INTO my_table VALUES (1, 'one', 1.5)");

	auto row =
	    db.query_one<int, std::string, double>("SELECT a, b, c FROM my_table");
	CHECK(row == std::tuple<int, std::string, double>{1, "one", 1.5});

	auto missing = db.query_one<int>("SELECT a FROM my_table WHERE a = 2");
	CHECK(missing == std::nullopt);

	db.execute("INSERT INTO my_table VALUES (2, 'two', 2.5)");
	auto first = db.query_one<int, std::string, double>(
	    "SELECT a, b, c FROM my_table ORDER BY a");
	CHECK(first == std::tuple<int, std::string, double>{1, "one", 1.5});
}

TEST_CASE("sqlite one-shot statements with bound parameters", "[sqlite]")
{
	util::Sqlite db(":memory:");
	db.execute("CREATE TABLE my_table(a INTEGER, b TEXT, c REAL)");
	db.execute("INSERT INTO my_table(a, b, c) VALUES (?, ?, ?)", 1, "one", 1.5);
	db.execute("INSERT INTO my_table(a, b, c) VALUES (?, ?, ?)", 2, "two", 2.5);
	db.execute("INSERT INTO my_table(a, b, c) VALUES (?, ?, ?)", 3, "three",
	           3.5);
	db.execute("INSERT INTO my_table(a, b, c) VALUES (?, ?, ?)",
	           std::int64_t{1} << 40, "big", 0.0);
	db.execute("INSERT INTO my_table(a, b, c) VALUES (?, ?, ?)", 4, nullptr,
	           nullptr);

	std::vector<std::tuple<int, std::string, double>> rows;
	db.query<int, std::string, double>(
	    "SELECT a, b, c FROM my_table WHERE a >= ? AND a <= ? ORDER BY a", 2, 3,
	    [&](int a, std::string b, double c) { rows.emplace_back(a, b, c); });
	REQUIRE(rows.size() == 2);
	CHECK(rows[0] == std::tuple<int, std::string, double>{2, "two", 2.5});
	CHECK(rows[1] == std::tuple<int, std::string, double>{3, "three", 3.5});

	std::vector<std::tuple<int, std::string>> cols;
	db.query<int, std::string>(
	    "SELECT a, b FROM my_table WHERE c > ? ORDER BY a", 2.0,
	    [&](int a, std::string b) { cols.emplace_back(a, std::move(b)); });
	REQUIRE(cols.size() == 2);
	CHECK(cols[0] == std::tuple<int, std::string>{2, "two"});
	CHECK(cols[1] == std::tuple<int, std::string>{3, "three"});

	std::vector<int> ids;
	db.query<int>("SELECT a FROM my_table WHERE b = ?", "two",
	              [&](int a) { ids.push_back(a); });
	CHECK(ids == std::vector<int>{2});

	int seen = 0;
	db.query<int, std::string, double>(
	    "SELECT a, b, c FROM my_table WHERE a = ?", 1,
	    [&](int a, std::string b, double c) {
		    ++seen;
		    CHECK(a == 1);
		    CHECK(b == "one");
		    CHECK(c == 1.5);
	    });
	CHECK(seen == 1);

	db.query<int, std::string, double>(
	    "SELECT a, b, c FROM my_table WHERE a = ?", 3,
	    [&](int a, std::string b, double c) {
		    ++seen;
		    CHECK(std::tuple{a, b, c} ==
		          std::tuple<int, std::string, double>{3, "three", 3.5});
	    });
	CHECK(seen == 2);

	auto one = db.query_one<int, std::string, double>(
	    "SELECT a, b, c FROM my_table WHERE b = ? AND c = ?", "two", 2.5);
	CHECK(one == std::tuple<int, std::string, double>{2, "two", 2.5});

	auto big =
	    db.query_one<std::int64_t>("SELECT a FROM my_table WHERE b = ?", "big");
	CHECK(big == (std::int64_t{1} << 40));

	auto is_null = db.query_one<int>(
	    "SELECT b IS NULL AND c IS NULL FROM my_table WHERE a = ?", 4);
	CHECK(is_null == 1);

	CHECK(db.query_one<int>("SELECT a FROM my_table WHERE a = ?", 9) ==
	      std::nullopt);
	auto first_of_many = db.query_one<int>(
	    "SELECT a FROM my_table WHERE a >= ? AND a <= ? ORDER BY a", 1, 3);
	CHECK(first_of_many == 1);
}
