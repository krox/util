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

util::Parser::Parser(std::string_view src, Options const &opt)
    : src_(src), opt_(opt)
{
	skip_white();
}

void util::Parser::skip_white()
{
	while (true)
	{
		while (pos_ < src_.size() && std::isspace(src_[pos_]))
			pos_++;
		if (!opt_.comment_start.empty() &&
		    src_.substr(pos_, opt_.comment_start.size()) == opt_.comment_start)
		{
			pos_ += opt_.comment_start.size();
			while (pos_ < src_.size() && src_[pos_] != '\n')
				pos_++;
		}
		else
			break;
	}
}

char util::Parser::peek() const { return pos_ < src_.size() ? src_[pos_] : 0; }

bool util::Parser::match(char ch)
{
	if (pos_ < src_.size() && src_[pos_] == ch)
	{
		pos_++;
		skip_white();
		return true;
	}
	return false;
}

bool util::Parser::match(std::string_view word)
{
	if (src_.substr(pos_, word.size()) == word)
	{
		pos_ += word.size();
		skip_white();
		return true;
	}
	return false;
}

std::string_view util::Parser::word()
{
	if (pos_ >= src_.size())
		return {};

	size_t start = pos_;
	while (pos_ < src_.size() && !std::isspace(src_[pos_]))
		pos_++;
	size_t end = pos_;
	skip_white();
	return src_.substr(start, end - start);
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
	if (pos_ >= src_.size() || (!std::isalpha(src_[pos_]) && src_[pos_] != '_'))
		return {};

	size_t start = pos_;

	while (pos_ < src_.size() &&
	       (std::isalnum(src_[pos_]) || src_[pos_] == '_'))
		pos_++;

	size_t end = pos_;
	skip_white();
	return src_.substr(start, end - start);
}

std::string_view util::Parser::integer()
{
	size_t start = pos_;
	if (pos_ < src_.size() && src_[pos_] == '-')
		pos_++;
	if (pos_ >= src_.size() || !std::isdigit(src_[pos_]))
		return {};
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
			raise("unterminated string (reached newline)");
		else
			pos_ += 1;
	}
	raise("unterminated string (reached end of input)");
}

bool util::Parser::end() const { return pos_ == src_.size(); }

void util::Parser::expect(char ch)
{
	if (!match(ch))
		raise(fmt::format("expected '{}'", ch));
}

void util::Parser::expect(std::string_view word)
{
	if (!match(word))
		raise(fmt::format("expected '{}'", word));
}

void util::Parser::expect_ident(std::string_view word)
{
	if (!ident(word))
		raise(fmt::format("expected '{}'", word));
}

std::string_view util::Parser::expect_ident()
{
	std::string_view result = ident();
	if (result.empty())
		raise("expected identifier");
	return result;
}

std::string_view util::Parser::expect_integer()
{
	std::string_view result = integer();
	if (result.empty())
		raise("expected integer");
	return result;
}

std::string_view util::Parser::expect_string()
{
	std::string_view result = string();
	if (result.empty())
		raise("expected string");
	return result;
}

void util::Parser::expect_end()
{
	if (!end())
		raise("expected end of input");
}

[[noreturn]] void util::Parser::raise(std::string_view msg)
{
	size_t line = 1;
	size_t col = 1;
	size_t line_start = 0;
	for (size_t i = 0; i < pos_; ++i)
	{
		if (src_[i] == '\n')
		{
			line++;
			col = 1;
			line_start = i + 1;
		}
		else
			col++;
	}
	std::string_view source_line;
	size_t line_end = src_.find('\n', line_start);

	if (line_end == std::string_view::npos)
		source_line = src_.substr(line_start);
	else
		source_line = src_.substr(line_start, line_end - line_start);
	throw ParseError(
	    fmt::format("input:{}:{}: error: {}\n{:>5} | {}\n{:>5} | {:>{}}^\n",
	                line, col, msg, line, source_line, "", "", col - 1));
}

char32_t util::parse_utf8(std::string_view &s)
{
	// classify bytes in utf8 sequence
	auto is_1byte = [](unsigned char c) { return (c & 0x80) == 0; };
	auto is_2byte = [](unsigned char c) { return (c & 0xE0) == 0xC0; };
	auto is_3byte = [](unsigned char c) { return (c & 0xF0) == 0xE0; };
	auto is_4byte = [](unsigned char c) { return (c & 0xF8) == 0xF0; };
	auto is_cont = [](unsigned char c) { return (c & 0xC0) == 0x80; };

	if (s.empty())
		throw ParseError("unexpected end of input while parsing utf8");

	if (is_1byte(s[0]))
	{
		char32_t cp = s[0];
		s.remove_prefix(1);
		return cp;
	}
	else if (is_2byte(s[0]))
	{
		if (s.size() < 2 || !is_cont(s[1]))
			throw ParseError("invalid utf8 (incomplete 2-byte sequence)");
		char32_t cp = ((s[0] & 0x1F) << 6) | (s[1] & 0x3F);
		if (cp < 0x80)
			throw ParseError("invalid utf8 (overlong encoding)");
		s.remove_prefix(2);
		return cp;
	}
	else if (is_3byte(s[0]))
	{
		if (s.size() < 3 || !is_cont(s[1]) || !is_cont(s[2]))
			throw ParseError("invalid utf8 (incomplete 3-byte sequence)");
		char32_t cp =
		    ((s[0] & 0x0F) << 12) | ((s[1] & 0x3F) << 6) | (s[2] & 0x3F);
		if (cp < 0x800)
			throw ParseError("invalid utf8 (overlong encoding)");
		if (cp >= 0xD800 && cp <= 0xDFFF)
			throw ParseError("invalid utf8 (surrogate half)");
		s.remove_prefix(3);
		return cp;
	}
	else if (is_4byte(s[0]))
	{
		if (s.size() < 4 || !is_cont(s[1]) || !is_cont(s[2]) || !is_cont(s[3]))
			throw ParseError("invalid utf8 (incomplete 4-byte sequence)");
		char32_t cp = ((s[0] & 0x07) << 18) | ((s[1] & 0x3F) << 12) |
		              ((s[2] & 0x3F) << 6) | (s[3] & 0x3F);
		if (cp < 0x10000)
			throw ParseError("invalid utf8 (overlong encoding)");
		if (cp > 0x10FFFF)
			throw ParseError("invalid utf8 (codepoint out of range)");
		s.remove_prefix(4);
		return cp;
	}
	else
		throw ParseError("invalid utf8 sequence");
}

int util::display_width(char32_t ucs)
{
	if (ucs < 32) // control characters
		return -1;
	if (ucs < 0x7f) // printable ASCII
		return 1;
	if (ucs < 0xa0) // more control characters
		return -1;

	// combining characters
	if ((ucs >= 0x300 && ucs <= 0x36F) || (ucs >= 0x1AB0 && ucs <= 0x1AFF) ||
	    (ucs >= 0x1DC0 && ucs <= 0x1DFF) || (ucs >= 0x20D0 && ucs <= 0x20FF) ||
	    (ucs >= 0xFE20 && ucs <= 0xFE2F))
		return 0;

	// wide characters (East Asian, emojis, "full width ascii", ...)
	if ((ucs >= 0x1100 && ucs <= 0x115f) || (ucs >= 0x2329 && ucs <= 0x232a) ||
	    (ucs >= 0x2e80 && ucs <= 0xa4cf) || (ucs >= 0xac00 && ucs <= 0xd7a3) ||
	    (ucs >= 0xf900 && ucs <= 0xfaff) || (ucs >= 0xfe10 && ucs <= 0xfe19) ||
	    (ucs >= 0xfe30 && ucs <= 0xfe6f) || (ucs >= 0xff00 && ucs <= 0xff60) ||
	    (ucs >= 0xffe0 && ucs <= 0xffe6) ||
	    (ucs >= 0x20000 && ucs <= 0x2fffd) ||
	    (ucs >= 0x30000 && ucs <= 0x3fffd))
		return 2;

	return 1;
}

int util::display_width(std::string_view s)
{
	int width = 0;
	while (!s.empty())
	{
		char32_t cp = parse_utf8(s);
		int w = display_width(cp);
		if (w < 0)
			return -1;
		width += w;
	}
	return width;
}
