#include "util/random.h"
#include "CLI/CLI.hpp"
#include "fmt/format.h"
#include "util/hash.h"
#include <string>

int main(int argc, char *argv[])
{
	std::string seed;
	uint64_t count = UINT64_MAX;
	CLI::App app{"Generate random numbers (unsigned 64 bit integers)."};
	app.add_option("--count", count, "how many numbers to generate");
	app.add_option("--seed", seed,
	               "seed for the random number generator (string)");
	CLI11_PARSE(app, argc, argv);

	util::xoshiro256 rng;
	if (app.count("--seed") == 0)
	{
		std::random_device rd;
		rng.seed(rd);
	}
	else
		rng.seed(seed);

	for (uint64_t i = 0; i < count; ++i)
		fmt::print("{}\n", rng());
}
