#include "catch2/catch_test_macros.hpp"

#include "util/json.h"
namespace util {}
using namespace util;
/*
TEST_CASE("json parser", "[json]")
{
    auto j = Json::parse("[[4,5,6],{},{\"a\":null, b:\"foo\"},1,2]");
    Json k;
    k.push_back(Json{});
    k[0].push_back(4);
    k[0].push_back(Json(5));
    k[0].push_back(Json(6));
    k.push_back(Json::object());
    k.push_back(Json::object());
    k[2]["a"] = Json{};
    k[2]["b"] = "foo";
    k.push_back(Json::number(1));
    k.push_back(2);
    fmt::print("{}\n", j);
    fmt::print("{}\n", k);
}*/
