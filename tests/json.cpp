#include "catch2/catch_test_macros.hpp"

#include "util/json.h"

using namespace util;

TEST_CASE("json parser", "[json]")
{
	auto j = Json::parse("[[4,5,6],{},{\"a\":null, b:\"foo\"},1,2]");
	CHECK(fmt::format("{}", j) ==
	      "[[4, 5, 6], {}, {\"a\": null, \"b\": \"foo\"}, 1, 2]");
	Json k;
	k[0].push_back(4);
	k[0].push_back(Json(5));
	k[0].push_back(Json(6));
	k[2]["b"] = "foo";
	k[2]["a"] = Json{};
	k.push_back(Json::integer(1));
	k.push_back(2);
	CHECK(fmt::format("{}", k) ==
	      "[[4, 5, 6], null, {\"b\": \"foo\", \"a\": null}, 1, 2]");
}
