#pragma once

#include <string>
#include <string_view>

namespace util {

class Linenoise
{
	std::string history_file_;

  public:
	Linenoise() : Linenoise("") {}
	explicit Linenoise(std::string_view name);

	// Returns an empty string only when linenoise signals EOF (e.g. Ctrl-D).
	std::string operator()(std::string_view prompt);
};

} // namespace util