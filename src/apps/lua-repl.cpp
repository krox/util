#include "fmt/format.h"
#include "util/lua.h"

#include <iostream>
#include <string>

namespace {
bool is_syntax_error(std::string const &msg)
{
	return msg.find("syntax") != std::string::npos ||
	       msg.find("unexpected symbol") != std::string::npos ||
	       msg.find(" near '") != std::string::npos;
}
} // namespace

int main()
{
	util::Lua lua;

	std::string line;
	while (true)
	{
		fmt::print("lua> ");
		if (!std::getline(std::cin, line))
			break;
		if (line == ":quit" || line == ":q" || line == "quit" || line == "exit")
			break;
		if (line.empty())
			continue;

		try
		{
			lua.run(line, "repl");
		}
		catch (util::LuaError const &statement_error)
		{
			if (is_syntax_error(statement_error.what()))
			{
				try
				{
					lua.run(fmt::format("print({})", line), "repl");
					continue;
				}
				catch (util::LuaError const &)
				{}
			}

			fmt::print(stderr, "error: {}\n", statement_error.what());
		}
	}

	return 0;
}
