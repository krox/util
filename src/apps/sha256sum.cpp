#include "CLI/CLI.hpp"
#include "fmt/format.h"
#include "util/hash.h"
#include "util/io.h"
#include "util/random.h"
#include <string>

int main(int argc, char *argv[])
{

	int rounds = 64;
	std::string filename;
	CLI::App app{"compute sha256 of a file (way less efficient than "
	             "'sha256sum' from GNU coreutils)"};
	app.add_option("--rounds", rounds,
	               "number of rounds (default = 64 = full sha256)");
	app.add_option("filename", filename, "file to hash")->required();
	CLI11_PARSE(app, argc, argv);

	auto data = util::read_binary_file(filename);
	auto hash = util::sha256(data, rounds);
	fmt::print("{}\n", util::hex_string(hash));
}
