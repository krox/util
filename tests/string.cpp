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

TEST_CASE("display width")
{
	SECTION("codepoint widths")
	{
		CHECK(util::display_width(U'a') == 1);
		CHECK(util::display_width(U'\u00e9') == 1);
		CHECK(util::display_width(U'\u0301') == 0);
		CHECK(util::display_width(U'\u03a9') == 1);
		CHECK(util::display_width(U'\u3042') == 2);
		CHECK(util::display_width(U'\n') == -1);
	}

	SECTION("utf8 string widths")
	{
		CHECK(util::display_width("Cafe\u0301") == 4);
		CHECK(util::display_width("\u03a9") == 1);
		CHECK(util::display_width("\u3042") == 2);
		CHECK(util::display_width("a\u3042\u0301") == 3);
		CHECK(util::display_width("\n") == -1);
	}

	SECTION("truncated utf8")
	{
		CHECK_THROWS_AS(util::display_width(std::string_view{"\xE2\x82", 2}),
		                util::ParseError);
		CHECK_THROWS_AS(
		    util::display_width(std::string_view{"\xF0\x9F\x99", 3}),
		    util::ParseError);
	}
}
