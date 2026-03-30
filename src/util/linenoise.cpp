#include "util/linenoise.h"

#include "linenoise.h"

namespace util {

Linenoise::Linenoise(std::string_view name)
{
	linenoiseHistorySetMaxLen(1000);

	if (!name.empty())
	{
		history_file_ = "." + std::string(name) + "_history";
		linenoiseHistoryLoad(history_file_.c_str());
	}
}

std::string Linenoise::operator()(std::string_view prompt)
{
	std::string prompt_string(prompt);

	while (true)
	{
		char *input = linenoise(prompt_string.c_str());
		if (input == nullptr)
			return {};

		std::string line(input);
		linenoiseFree(input);

		if (line.empty())
			continue;

		linenoiseHistoryAdd(line.c_str());

		if (!history_file_.empty())
			linenoiseHistorySave(history_file_.c_str());

		return line;
	}
}

} // namespace util