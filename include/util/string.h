#pragma once

// utilities for string parsing

#include <string>
#include <string_view>
#include <vector>

namespace util {

// trim leading/trailing whitespace from a string_view
inline std::string_view trim_white(std::string_view s)
{
	size_t start = 0;
	size_t end = s.size();
	while (start < end && std::isspace(s[start]))
		++start;
	while (end > start && std::isspace(s[end - 1]))
		--end;
	return s.substr(start, end - start);
}

// split a string by a delimiter
inline std::vector<std::string_view> split(std::string_view s, char delim)
{
	std::vector<std::string_view> result;

	size_t start = 0;
	for (size_t i = 0; i < s.size(); ++i)
		if (s[i] == delim)
		{
			result.push_back(s.substr(start, i - start));
			start = i + 1;
		}
	result.push_back(s.substr(start));

	return result;
}

// split a string by whitespace (std::isspace).
//   * Leading/trailing whitespace is trimmed
//   * multiple whitespace is collapsed. Thus the resulting
//     vector will not contain empty strings.
//   * resulting string_views point into the original string
inline std::vector<std::string_view> split_white(std::string_view s)
{
	std::vector<std::string_view> result;

	size_t start = 0;
	for (size_t i = 0; i < s.size(); ++i)
		if (std::isspace(s[i]))
		{
			if (i > start)
				result.push_back(s.substr(start, i - start));
			start = i + 1;
		}
	if (start < s.size())
		result.push_back(s.substr(start));

	return result;
}
} // namespace util