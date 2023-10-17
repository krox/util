#include "catch2/catch_test_macros.hpp"

#include "fmt/format.h"
#include "fmt/ranges.h"
#include "util/json.h"
#include "util/numpy.h"
#include "util/span.h"
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

	// negative numbers
	auto l = Json::parse("[-1, -2, -3]");
	CHECK(fmt::format("{}", l) == "[-1, -2, -3]");
}

/*
TEST_CASE("numpy", "[numpy]")
{
    {
        auto np = NumpyFile::create("test.npy", {2, 3}, "<f8", true);
        auto view = np.view<double, 2>();
        view(0, 0) = 1;
        view(0, 1) = 2;
        view(0, 2) = 3;
        view(1, 0) = 4;
        view(1, 1) = 5;
        view(1, 2) = 6;
    }
    {
        auto np = NumpyFile::open("test.npy");
        auto view = np.view<double, 2>();
        fmt::print("{}", view);
    }
}
*/