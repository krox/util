#include "catch2/catch_test_macros.hpp"

#include "util/functional.h"

namespace {

void recurse_with_view(int depth, util::function_view<void(int)> callback = {})
{
	if (callback)
		callback(depth);
	if (depth > 0)
		recurse_with_view(depth - 1, callback);
}

} // namespace

TEST_CASE("function_view copies preserve emptiness")
{
	util::function_view<void(int)> callback;
	auto copy = callback;

	CHECK(!callback);
	CHECK(!copy);

	recurse_with_view(3, copy);
	SUCCEED();
}

TEST_CASE("function_view copies preserve bound callback")
{
	int sum = 0;
	auto lambda = [&](int x) { sum += x; };
	util::function_view<void(int)> callback = lambda;
	auto copy = callback;

	REQUIRE(callback);
	REQUIRE(copy);

	copy(5);
	CHECK(sum == 5);
}

TEST_CASE("function_view is safe to pass by value recursively")
{
	int sum = 0;
	auto lambda = [&](int x) { sum += x; };

	recurse_with_view(3, lambda);

	CHECK(sum == 6);
}