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
