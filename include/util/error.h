#pragma once

// a couple tiny helpers to make error handling just a little bit nicer.

#include "fmt/format.h"
#include <stdexcept>

namespace util {

// calls std::terminate, printing a message using libfmt to stderr first
template <class... Args>
[[noreturn]] void terminate(fmt::format_string<Args...> str,
                            Args &&...args) noexcept
{
	fmt::print(stderr, str, std::forward<Args>(args)...);
	std::terminate();
}

[[noreturn]] inline void terminate() noexcept { std::terminate(); }

// throws an exception, formatting the message using libfmt
template <class Ex = std::runtime_error, class... Args>
[[noreturn]] void raise(fmt::format_string<Args...> str, Args &&...args)
{
	throw Ex(fmt::format(str, std::forward<Args>(args)...));
}

// Invokes undefined bahaviour. Will be part of std in C++23.
#ifdef NDEBUG
#ifdef __GNUC__
[[noreturn]] inline void unreachable() noexcept { __builtin_unreachable(); }
#elif defined(_MSC_VER)
[[noreturn]] inline void unreachable() noexcept { __assume(0); }
#else
[[noreturn]] inline void unreachable() noexcept
{ // returning from a [[noreturn]] function is UB
}
#endif
#else
[[noreturn]] inline void unreachable() noexcept
{
	terminate("util::unreachable called");
}
#endif

// assume is like assert, but creates a builtin_assume in release mode
//   * only use for simple stuff (null-checks, simple bounds-checks, etc.),
//     because otherwise, something might be evaluated even in release mode.
template <class... Args>
void assume(bool cond, fmt::format_string<Args...> str = "util::assume failed",
            Args &&...args) noexcept
{
	if (cond) [[likely]]
		return;

#ifdef NDEBUG
	unreachable();
#else
	terminate(str, std::forward<Args>(args)...);
#endif
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