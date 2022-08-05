#include "CLI/CLI.hpp"
#include "fmt/format.h"
#include "util/json.h"
#include <cassert>
#include <iostream>
#include <iterator>
#include <string>

int main(int argc, char *argv[])
{
	CLI::App app{
	    "Reads json from stdin and outputs it on stdout, nicely formatted."};
	CLI11_PARSE(app, argc, argv);

	std::istreambuf_iterator<char> begin(std::cin), end;
	auto src = std::string(begin, end);
	util::Json j;
	try
	{
		j = util::Json::parse(src);
	}
	catch (util::ParseError const &ex)
	{
		fmt::print(stderr, "JSON parsing error: {}\n", ex.what());
		return -1;
	}

	fmt::print("{:h}\n", j);
}
