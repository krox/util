#pragma once

// a couple tiny helpers to make error handling just a little bit nicer.

#include "fmt/format.h"
#include <source_location>
#include <stdexcept>
#include <utility>

namespace util {

// Print current stack trace in human readable form. Requires C++23 with
// 'std::stacktrace' support. Actual usefulness depends on platform of course.
void print_stacktrace();

// Essentially equivalent to classic 'assert', but with some modern amenities
// - Message is formatted using libfmt.
// - In release mode, failure is explicit UB, potentially helping the optimizer.
// - In debug mode, also prints a stacktrace if available.
template <class... Args>
void assume(bool cond, fmt::format_string<Args...> str = "", Args &&...args,
            std::source_location loc = std::source_location::current()) noexcept
{
	(void)str;
	((void)args, ...);
	if (cond) [[likely]]
		return;

#ifdef NDEBUG
#if defined(__cpp_lib_unreachable) && __cpp_lib_unreachable >= 202202L
	std::unreachable();
#elif defined(__GNUC__)
	__builtin_unreachable();
#endif
#else
	fmt::print(stderr, "Assumption failed: {}\n  at {}:{}\n",
	           fmt::format(str, std::forward<Args>(args)...), loc.file_name(),
	           loc.line());
	print_stacktrace();
	std::terminate();
#endif
}

// Same as 'assume(false)', but with [[noreturn]] attribute to make the compiler
// happy in typical usages.
template <class... Args>
[[noreturn]] void
unreachable(fmt::format_string<Args...> str = "", Args &&...args,
            std::source_location loc = std::source_location::current()) noexcept
{
	(void)str;
	((void)args, ...);

#ifdef NDEBUG
#if defined(__cpp_lib_unreachable) && __cpp_lib_unreachable >= 202202L
	std::unreachable();
#elif defined(__GNUC__)
	__builtin_unreachable();
#endif
#else
	fmt::print(stderr, "Assumption failed: {}\n  at {}:{}\n",
	           fmt::format(str, std::forward<Args>(args)...), loc.file_name(),
	           loc.line());
	print_stacktrace();
	std::terminate();
#endif
}

// throws an exception, formatting the message using libfmt
template <class Ex = std::runtime_error, class... Args>
[[noreturn]] void raise(fmt::format_string<Args...> str, Args &&...args)
{
	throw Ex(fmt::format(str, std::forward<Args>(args)...));
}

// Check a value, throwing an exception if it fails, returning the value if it
// succeeds. Intended to be used like this:
//   void* ptr = util::check<std::bad_alloc>(malloc(10));
template <class Ex = std::runtime_error, class T, class... Args>
T &&check(T &&value, fmt::format_string<Args...> msg = "util::check failed",
          Args &&...args)
{
	if (!!value) [[likely]]
		return std::forward<T>(value);

	raise<Ex>(msg, std::forward<Args>(args)...);
}
template <class Ex = std::runtime_error, class T, class... Args>
T &&check_non_negative(T &&value,
                       fmt::format_string<Args...> msg = "util::check failed",
                       auto &&...args)
{
	if (value >= 0) [[likely]]
		return std::forward<T>(value);

	raise<Ex>(msg, std::forward<Args>(args)...);
}
template <class Ex = std::runtime_error, class T, class... Args>
T &&check_positive(T &&value,
                   fmt::format_string<Args...> msg = "util::check failed",
                   Args &&...args)
{
	if (value > 0) [[likely]]
		return std::forward<T>(value);

	raise<Ex>(msg, std::forward<Args>(args)...);
}

} // namespace util