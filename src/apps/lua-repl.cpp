#include "fmt/format.h"
#include "util/linenoise.h"
#include "util/lua.h"

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
	util::Linenoise linenoise("lua-repl");

	while (true)
	{
		auto line = linenoise("lua> ");
		if (line.empty() || line == ":quit" || line == ":q" || line == "quit" ||
		    line == "exit")
			break;

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
