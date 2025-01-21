#include "catch2/catch_test_macros.hpp"

#include "fmt/format.h"
#include "util/string.h"

TEST_CASE("basic string parsing")
{
	CHECK(util::trim_white("  foo  ") == "foo");
	CHECK(util::trim_white("  ") == "");
	CHECK(util::trim_white("foo") == "foo");
	CHECK(util::trim_white("") == "");

	CHECK(util::split("foo,bar", ',') ==
	      std::vector<std::string_view>{"foo", "bar"});
	CHECK(util::split("foo,bar,", ',') ==
	      std::vector<std::string_view>{"foo", "bar", ""});
	CHECK(util::split("foo", ',') == std::vector<std::string_view>{"foo"});
	CHECK(util::split("", ',') == std::vector<std::string_view>{""});

	CHECK(util::split_white("  foo  bar  ") ==
	      std::vector<std::string_view>{"foo", "bar"});
	CHECK(util::split_white("  ") == std::vector<std::string_view>{});
	CHECK(util::split_white("  foo  ") == std::vector<std::string_view>{"foo"});
	CHECK(util::split_white("foo") == std::vector<std::string_view>{"foo"});
	CHECK(util::split_white("") == std::vector<std::string_view>{});
}

TEST_CASE("simple parser")
{
	SECTION("match")
	{
		util::Parser p(" foo bar");
		CHECK(!p.ident("fo"));
		CHECK(p.match("fo"));
		CHECK(p.match('o'));
		CHECK(!p.match(" "));
		CHECK(p.match("bar"));
		CHECK(!p.match("baz"));
		CHECK(p.end());
	}

	SECTION("ident")
	{
		util::Parser p("  foo  bar ");

		CHECK(p.ident("foo"));
		CHECK(p.ident("bar"));
		CHECK(!p.ident("baz"));
		CHECK(p.end());
	}

	SECTION("integer")
	{
		util::Parser p("123 456");
		CHECK(p.integer() == "123");
		CHECK(p.integer() == "456");
		CHECK(p.integer() == "");
		CHECK(p.end());
	}

	SECTION("string")
	{
		util::Parser p("\"foo\" 'ba\"\\\\\\'r' baz");
		CHECK(p.string() == "\"foo\"");
		CHECK(p.string() == "'ba\"\\\\\\'r'");
		CHECK(p.string() == "");
		CHECK(p.ident() == "baz");
		CHECK(p.end());
	}
}
