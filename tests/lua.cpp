#include "catch2/catch_test_macros.hpp"
#include "catch2/matchers/catch_matchers_string.hpp"

#include "util/lua.h"

#include <functional>
#include <span>
#include <string>
#include <vector>

using namespace util;

namespace {

struct SolverOptions
{
	double tol = 1.0e-10;
	int steps = 1000;
	bool verbose = false;
	std::string label = "default";
};

} // namespace

template <> struct util::lua_struct_traits<SolverOptions>
{
	using Self = SolverOptions;
	static constexpr auto fields =
	    std::make_tuple(util::lua_field("tol", &Self::tol),
	                    util::lua_field("steps", &Self::steps),
	                    util::lua_field("verbose", &Self::verbose),
	                    util::lua_field("label", &Self::label));
};

TEST_CASE("lua scripting", "[lua]")
{
	Lua lua;

	lua.run("function fib(n) if n <= 1 then return n else return "
	        "fib(n-1) + fib(n-2) end end");
	lua.set("x", 10);
	lua.run("x = fib(x)");
	CHECK(lua.get<int>("x") == 55);

	lua.set("c_function", [](int x) { return x * 2; });
	lua.run("y = c_function(21)");
	CHECK(lua.get<int>("y") == 42);

	lua.set("name", std::string("foo"));
	lua.run("name = name .. '-bar'");
	CHECK(lua.get<std::string>("name") == "foo-bar");

	std::vector<int> numbers{1, 2, 3};
	lua.set("numbers", std::span<int>(numbers));
	lua.run("for i=1,#numbers do numbers[i] = numbers[i] * 3 end");
	CHECK(lua.get<std::vector<int>>("numbers") == std::vector<int>{3, 6, 9});

	lua.set("sum", [](std::vector<int> values) {
		int out = 0;
		for (int v : values)
			out += v;
		return out;
	});
	lua.run("total = sum({10, 20, 30})");
	CHECK(lua.get<int>("total") == 60);
}

TEST_CASE("lua errors propagate as LuaError", "[lua]")
{
	Lua lua;

	CHECK_THROWS_AS(lua.run("function bad("), LuaError);
	CHECK_THROWS_AS(lua.run("error('boom')"), LuaError);
}

TEST_CASE("lua bound function type and arity errors", "[lua]")
{
	Lua lua;
	lua.set("double", [](int x) { return x * 2; });

	CHECK_THROWS_AS(lua.run("double('x')"), LuaError);
	CHECK_THROWS_AS(lua.run("double(1, 2)"), LuaError);
}

TEST_CASE("lua closed state operations throw", "[lua]")
{
	Lua lua;
	lua.close();

	CHECK_THROWS_AS(lua.run("x = 1"), LuaError);
	CHECK_THROWS_AS(lua.set("x", 1), LuaError);
	CHECK_THROWS_AS((lua.get<int>("x")), LuaError);
	CHECK_THROWS_AS(lua.set("f", [](int x) { return x + 1; }), LuaError);
}

TEST_CASE("lua rebinding keeps old closure valid", "[lua]")
{
	Lua lua;
	lua.set("f", [](int x) { return x + 1; });
	lua.run("old = f");
	lua.set("f", [](int x) { return x + 2; });
	lua.run("a = old(5); b = f(5)");

	CHECK(lua.get<int>("a") == 6);
	CHECK(lua.get<int>("b") == 7);
}

TEST_CASE("lua conversion failures keep state usable", "[lua]")
{
	Lua lua;
	lua.set("bad", std::vector<std::vector<int>>{{1, 2}, {3, 4}});
	lua.run("bad[2] = 'oops'");

	CHECK_THROWS_AS((lua.get<std::vector<std::vector<int>>>("bad")), LuaError);

	lua.run("x = 123");
	CHECK(lua.get<int>("x") == 123);
}

TEST_CASE("lua registered structs decode named option tables", "[lua]")
{
	Lua lua;
	SolverOptions seen;

	lua.set("solve", [&](SolverOptions const &opts) { seen = opts; });

	lua.run("solve{}");
	CHECK(seen.tol == 1.0e-10);
	CHECK(seen.steps == 1000);
	CHECK(seen.verbose == false);
	CHECK(seen.label == "default");

	lua.run("solve{tol=1.0e-8, steps=42, verbose=true, label='fast'}");
	CHECK(seen.tol == 1.0e-8);
	CHECK(seen.steps == 42);
	CHECK(seen.verbose == true);
	CHECK(seen.label == "fast");
}

TEST_CASE("lua registered structs reject unknown keys and bad field types",
          "[lua]")
{
	Lua lua;
	lua.set("solve", [](SolverOptions const &) {});

	CHECK_THROWS_WITH(lua.run("solve{stpes=10}"),
	                  Catch::Matchers::ContainsSubstring("stpes"));
	CHECK_THROWS_WITH(lua.run("solve{steps='foo'}"),
	                  Catch::Matchers::ContainsSubstring("steps"));
	CHECK_THROWS_WITH(
	    lua.run("solve{steps='foo'}"),
	    Catch::Matchers::ContainsSubstring("expected Lua integer"));
}

TEST_CASE("lua registered structs return to Lua as named tables", "[lua]")
{
	Lua lua;

	lua.set("defaults", [] {
		return SolverOptions{
		    .tol = 1.0e-7, .steps = 77, .verbose = true, .label = "from-cpp"};
	});
	lua.set("capture", [](SolverOptions const &opts) { return opts; });

	lua.run(
	    "returned = defaults(); tol = returned.tol; steps = returned.steps; "
	    "verbose = returned.verbose; label = returned.label; "
	    "echoed = capture(returned)");

	CHECK(lua.get<double>("tol") == 1.0e-7);
	CHECK(lua.get<int>("steps") == 77);
	CHECK(lua.get<bool>("verbose") == true);
	CHECK(lua.get<std::string>("label") == "from-cpp");

	SolverOptions echoed = lua.get<SolverOptions>("echoed");
	CHECK(echoed.tol == 1.0e-7);
	CHECK(echoed.steps == 77);
	CHECK(echoed.verbose == true);
	CHECK(echoed.label == "from-cpp");
}
