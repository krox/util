#include "util/error.h"

#include <version>

#if defined(__cpp_lib_stacktrace) && __cpp_lib_stacktrace >= 202011L
#include <stacktrace>
void util::print_stacktrace()
{
	auto st = std::stacktrace::current();
	fmt::print(stderr, "Stack trace:\n");
	int cnt = 0;
	for (auto const &frame : st)
	{
		auto fun = frame.description();
		auto file = frame.source_file();
		auto line = frame.source_line();

		// skip frames that will not be useful, including this function itself
		if (fun.contains("util::unreachable") || fun.contains("util::assume") ||
		    fun.contains("util::print_stacktrace") || fun == "_start" ||
		    fun.starts_with("__libc_start") || (fun.empty() && file.empty()))
			continue;

		// abbreviate the function signature, but dont abbreviate the filename
		// to keep it parsable by an IDE
		std::string_view anon = "(anonymous namespace)::";
		for (size_t pos = 0;
		     (pos = fun.find(anon, pos)) != std::string_view::npos;)
			fun.erase(pos, anon.size());
		if (fun.size() > 70)
			fun = fun.substr(0, 67) + "...";
		fmt::print(stderr, "  #{:02}: {:80}\n       at {}:{}\n", cnt++, fun,
		           file, line);
	}
	if (cnt == 0)
		fmt::print(stderr, "  (no stack frames found)\n");
}

#else
void util::print_stacktrace()
{
	fmt::print(stderr, "(no stack trace available)\n");
}
#endif