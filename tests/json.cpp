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
	Json k = Json::array(3);
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

TEST_CASE("json objects", "[json]")
{
	auto j = Json::parse("{a:5, b:7}");
	CHECK(j.value("a", 1) == 5);
	CHECK(j.value("c", 1) == 1);
}

namespace my_lib {
struct Foo
{
	int a;
	float b;
	std::string c;

	bool operator==(Foo const &) const = default;
};

Json to_json(Foo const &f)
{
	Json j;
	j["a"] = f.a;
	j["b"] = f.b;
	j["c"] = f.c;
	return j;
}

void from_json(Foo &f, Json const &j)
{
	f.a = j.value<int>("a", 0);
	f.b = j.value<float>("b", 0.0f);
	f.c = j.value<std::string>("c", "");
}
} // namespace my_lib

TEST_CASE("json type", "[json]")
{
	Json j = 1.0;
	CHECK(!j.is_integer());
	CHECK(j.is_floating());
	CHECK(j.as_floating() == 1.0);

	j = 2;
	CHECK(j.is_integer());
	CHECK(!j.is_floating());
	CHECK(j.as_integer() == 2);
	CHECK(j.get<int>() == 2);
	CHECK(j.get<float>() == 2.0);
}

TEST_CASE("json custom type", "[json]")
{
	// struct -> json -> string -> json -> struct
	my_lib::Foo f{1, 2.5f, "foo"};
	Json j = f;
	CHECK(j["a"].as_integer() == 1);
	CHECK(j["b"].as_floating() == 2.5f);
	CHECK(j["c"].as_string() == "foo");

	auto j2 = Json::parse(fmt::format("{}", j));
	CHECK(j == j2);
	CHECK(j2.get<my_lib::Foo>() == f);
}

TEST_CASE("json comments", "[json]")
{
	auto j = Json::parse(R"(
		{
			// comment
			"a": 1,
			"b": 2, // comment
			// comment
			"c": 3
			"d": /*4
			"e" :*/ 5
		}
	)");
	CHECK(j["a"].as_integer() == 1);
	CHECK(j["b"].as_integer() == 2);
	CHECK(j["c"].as_integer() == 3);
	CHECK(j["d"].as_integer() == 5);
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