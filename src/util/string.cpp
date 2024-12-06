#include "util/string.h"
#include "fmt/format.h"
#include <charconv>

std::string_view util::trim_white(std::string_view s)
{
	size_t start = 0;
	size_t end = s.size();
	while (start < end && std::isspace(s[start]))
		++start;
	while (end > start && std::isspace(s[end - 1]))
		--end;
	return s.substr(start, end - start);
}

std::vector<std::string_view> util::split(std::string_view s, char delim)
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

std::vector<std::string_view> util::split_white(std::string_view s)
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

template <std::integral T> T util::parse_int(std::string_view s)
{
	// std::from_chars(...) is supposed to be essentially the fastest routine
	// possible (a lot less overhead compared to scanf() and such)
	T value = {};
	auto [ptr, ec] = std::from_chars(s.begin(), s.end(), value);
	if (ec == std::errc() || ptr == s.end())
		return value;
	else
		throw ParseError(fmt::format("cannot parse integer '{}'", s));
}

// instantiate template
template signed char util::parse_int<signed char>(std::string_view);
template short util::parse_int<short>(std::string_view);
template int util::parse_int<int>(std::string_view);
template long util::parse_int<long>(std::string_view);
template long long util::parse_int<long long>(std::string_view);
template unsigned char util::parse_int<unsigned char>(std::string_view);
template unsigned short util::parse_int<unsigned short>(std::string_view);
template unsigned int util::parse_int<unsigned int>(std::string_view);
template unsigned long util::parse_int<unsigned long>(std::string_view);
template unsigned long long
    util::parse_int<unsigned long long>(std::string_view);

util::Parser::Parser(std::string_view src) : src_(src) { skip_white(); }

void util::Parser::skip_white()
{
	while (pos_ < src_.size() && std::isspace(src_[pos_]))
		pos_++;
}

// advance if match, returns false if not
bool util::Parser::match(char ch)
{
	if (pos_ < src_.size() && src_[pos_] == ch)
	{
		pos_++;
		return true;
	}
	return false;
}

bool util::Parser::match(std::string_view word)
{
	if (src_.substr(pos_, word.size()) == word)
	{
		pos_ += word.size();
		return true;
	}
	return false;
}

bool util::Parser::ident(std::string_view word)
{
	if (src_.substr(pos_, word.size()) == word)
	{
		if (pos_ + word.size() < src_.size() &&
		    (std::isalnum(src_[pos_ + word.size()]) ||
		     src_[pos_ + word.size()] == '_'))
			return false;
		pos_ += word.size();
		skip_white();
		return true;
	}
	return false;
}

std::string_view util::Parser::ident()
{
	size_t start = pos_;
	if (pos_ < src_.size() && (std::isalpha(src_[pos_]) || src_[pos_] == '_'))
	{
		pos_++;
		while (pos_ < src_.size() &&
		       (std::isalnum(src_[pos_]) || src_[pos_] == '_'))
			pos_++;
	}
	size_t end = pos_;
	skip_white();
	return src_.substr(start, end - start);
}

std::string_view util::Parser::integer()
{
	size_t start = pos_;
	while (pos_ < src_.size() && std::isdigit(src_[pos_]))
		pos_++;
	size_t end = pos_;
	skip_white();
	return src_.substr(start, end - start);
}

std::string_view util::Parser::string()
{
	if (pos_ >= src_.size() || (src_[pos_] != '"' && src_[pos_] != '\''))
		return {};
	size_t start = pos_;
	char quote = src_[pos_++];
	while (pos_ < src_.size())
	{
		if (src_[pos_] == quote)
		{
			size_t end = ++pos_;
			skip_white();
			return src_.substr(start, end - start);
		}
		else if (src_[pos_] == '\\')
		{
			pos_ += 2;
		}
		else if (src_[pos_] == '\n')
			throw ParseError("unterminated string (reached newline)");
		else
			pos_ += 1;
	}
	throw ParseError("unterminated string (reached end of input)");
}

bool util::Parser::end() const { return pos_ == src_.size(); }