#pragma once

/**
 * A simple tokenizer based on std::string_view.
 * Right now used for simple math expressions and JSON, though might be general
 * enough for some programmling languages.
 */

#include "util/hash_map.h"
#include <cassert>
#include <cctype>
#include <charconv>
#include <climits>
#include <optional>
#include <stdexcept>
#include <string_view>

namespace util {

class ParseError : public std::runtime_error
{
  public:
	ParseError(std::string const &what) : std::runtime_error(what) {}
};

// Parse an integer literal, throwing ParseError if not the whole s was matched.
// Currently only parses decimal with optional minus sign. Might add hex/bin
// in the future.
template <typename T = int> T parse_int(std::string_view s)
{
	// std::from_chars(...) is supposed to be essentially the fastest routine
	// possible (a lot less overhead compared to scanf() and such)

	static_assert(std::is_integral_v<T>);
	T value = {};
	auto [ptr, ec] = std::from_chars(s.begin(), s.end(), value);
	if (ec == std::errc() || ptr == s.end())
		return value;
	else
		throw ParseError(fmt::format("cannot parse integer '{}'", s));
}

// Parse an integer literal, throwing ParseError if not the whole s was matched.
// Currently only parses decimal with optional minus sign. Might add hex/bin
// in the future.
template <typename T = double> T parse_float(std::string_view s)
{
	static_assert(std::is_floating_point_v<T>);
	T value = {};
	auto [ptr, ec] = std::from_chars(s.begin(), s.end(), value);
	if (ec == std::errc() || ptr == s.end())
		return value;
	else
		throw ParseError(fmt::format("cannot parse float '{}'", s));
}

// parse a single- or double-quoted string, understands some escapes
std::string parse_string(std::string_view s)
{
	// NOTE: this parser is not totally strict. For example non-escaped quotes
	//       and trailing backslashes are accepted. Dont really care for now.
	if (!(s.size() >= 2 && ((s.front() == '"' && s.back() == '"') ||
	                        (s.front() == '\'' && s.back() == '\''))))
		throw ParseError("string literal not surrounded by quotes");

	std::string r;
	r.reserve(s.size() - 2);
	for (size_t i = 1; i < s.size() - 1; ++i)
	{
		if (s[i] == '\\')
		{
			++i;
			switch (s[i])
			{
			case '\\':
				r.push_back('\\');
				break;
			case 'n':
				r.push_back('\n');
				break;
			case 'r':
				r.push_back('\r');
				break;
			case 't':
				r.push_back('\t');
				break;
			case '\'':
				r.push_back('\'');
				break;
			case '"':
				r.push_back('"');
				break;
			default:
				throw ParseError(
				    fmt::format("unknown escape character '{}'", s[i]));
			}
		}
		else
			r.push_back(s[i]);
	}
	return r;
}

struct Tok
{
	uint32_t value_ = 0;

	constexpr Tok() = default;
	constexpr Tok(uint32_t val) : value_(val) {}
	constexpr Tok(char const *s) // TODO: should be consteval I think
	{
		assert(s && s[0]);
		if (!s[1])
			value_ = s[0];
		else if (!s[2])
			value_ = s[0] | (s[1] << 8);
		else if (!s[3])
			value_ = s[0] | (s[1] << 8) | (s[2] << 16);
		else
			assert(false);
	}

	static constexpr Tok none() { return (1 - 1); }
	static constexpr Tok ident() { return 1; }
	static constexpr Tok integer() { return 2; }
	static constexpr Tok floating() { return 3; }
	static constexpr Tok string() { return 4; }

	constexpr operator uint32_t() const { return value_; }

	constexpr bool operator==(Tok const &b) const { return value_ == b.value_; }
	constexpr bool operator!=(Tok const &b) const { return value_ != b.value_; }
};

inline const hash_map<char, Tok> string_to_tok = {
    {'(', "("}, {')', ")"}, {'[', "["}, {']', "]"}, {'{', "{"}, {'}', "}"},

    {'+', "+"}, {'-', "-"}, {'*', "*"}, {'/', "/"}, {'%', "%"},

    {'<', "<"}, {'>', ">"}, {'&', "&"}, {'|', "|"}, {'^', "^"}, {'!', "!"},

    {'=', "="}, {'.', "."}, {',', ","}, {';', ";"}, {':', ":"},
};

inline const hash_map<std::array<char, 2>, Tok> string2_to_tok = {
    {{'+', '+'}, "++"}, {{'-', '-'}, "--"}, {{'*', '*'}, "**"},
    {{'!', '!'}, "!!"},

    {{'&', '&'}, "&&"}, {{'|', '|'}, "||"}, {{'^', '^'}, "^^"},

    {{'=', '='}, "=="}, {{'!', '='}, "!="}, {{'<', '='}, "<="},
    {{'>', '='}, ">="},
};

struct Token
{
	Tok tok = Tok::none();       // "type" of token
	std::string_view value = {}; // slice into source or ""
};

/**
 * split source string into tokens
 *   - tokens always use a string_view into original source
 *     ( -> no copying of strings, but beware of dangling references )
 *   - tokens are parsed one at a time
 *     ( -> no allocation of token array, but lookahead is somewhat ugly )
 *   - produces an infinite number of 'None' tokens at end of input
 *     ( -> makes parser a little cleaner )
 *  Supported tokens:
 *    - identifier: [_a-zA-Z][_a-zA-Z0-9]*
 *    - integers: [0-9]+
 *    - floats: [0-9]+(.[0-9]*)?([eE][+-]?[0-9]+)?
 *    - strings: single- or double-quoted
 *    - operators: [+*-/%^(),;.=] and many more
 *  Future design choices:
 *    - should be include a location into Token for nicer errors?
 *    - should we support meaningful whitespace?
 */
class Lexer
{
	size_t pos_ = 0;
	/*const*/ std::string_view src_ = {};
	Token curr_ = {Tok::none(), {}};

	// TODO: replacing std::string_view by std::bitset<128> would be faster
	char try_match_char(std::string_view chars)
	{
		if (pos_ == src_.size())
			return '\0';
		for (auto c : chars)
			if (c == src_[pos_])
				return src_[pos_++];
		return '\0';
	}

	char match_char(std::string_view chars)
	{
		if (char c = try_match_char(chars); c)
			return c;
		else
			throw ParseError("expected character not found");
	}

	size_t match_all(std::string_view chars)
	{
		size_t count = 0;
		while (try_match_char(chars))
			++count;
		return count;
	}

  public:
	Lexer() = default;
	explicit Lexer(std::string_view buf) : src_(buf) { advance(); }

	bool empty() const { return curr_.tok == Tok::none(); }

	void advance()
	{
		// skip whitespace and quit if nothing is left
		match_all(" \t\n\r");
		if (pos_ == src_.size())
		{
			curr_ = {};
			return;
		}

		size_t start = pos_;

		// identifier
		if (try_match_char("_abcdefghijklmnopqrstuvwxyz"
		                   "ABCDEFGHIJKLMNOPQRSTUVWXYZ"))
		{
			curr_.tok = Tok::ident();
			match_all("_abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ"
			          "0123456789");
		}
		// int/float literal
		else if (try_match_char("0123456789"))
		{
			curr_.tok = Tok::integer();
			match_all("0123456789");
			if (try_match_char("."))
			{
				curr_.tok = Tok::floating();
				match_all("0123456789");
			}
			if (try_match_char("eE"))
			{
				curr_.tok = Tok::floating();
				try_match_char("+-");
				if (match_all("0123456789") == 0)
					throw ParseError("expected exponent after 'e'");
			}
		}

		// string literal
		else if (char delim = try_match_char("\"'"); delim)
		{
			curr_.tok = Tok::string();
			while (true)
			{
				if (src_[pos_] == delim)
				{
					pos_ += 1;
					break;
				}

				if (src_[pos_] == '\\')
					pos_ += 2;
				else
					pos_ += 1;
				if (pos_ >= src_.size())
					throw ParseError("undelimited string literal");
			}
		}

		// operators / parentheses
		else
		{
			curr_.tok = Tok::none();
			if (pos_ + 1 < src_.size())
				if (auto it = string2_to_tok.find({src_[pos_], src_[pos_ + 1]});
				    it != string2_to_tok.end())
				{
					curr_.tok = it->second;
					pos_ += 2;
				}

			if (!curr_.tok)
				if (auto it = string_to_tok.find(src_[pos_]);
				    it != string_to_tok.end())
				{
					curr_.tok = it->second;
					pos_ += 1;
				}

			if (!curr_.tok)
				throw ParseError(
				    fmt::format("unexpected character '{}'", src_[start]));
		}

		curr_.value = src_.substr(start, pos_ - start);
	}

	Token operator*() const { return curr_; }
	Token const *operator->() const { return &curr_; }

	Token pop()
	{
		auto r = curr_;
		advance();
		return r;
	}

	std::optional<Token> try_match(Tok tok)
	{
		if (curr_.tok != tok)
			return std::nullopt;
		else
			return pop();
	}

	Token match(Tok tok)
	{
		if (auto r = try_match(tok); r)
			return *r;
		else
			throw ParseError(fmt::format("unexpected token '{}'", curr_.value));
	}

	bool peek(Tok tok) { return curr_.tok == tok; }

	bool peek(Tok tok1, Tok tok2)
	{
		if (curr_.tok != tok1)
			return false;
		auto oldPos = pos_;
		auto oldTok = curr_;
		advance();
		bool r = curr_.tok == tok2;
		pos_ = oldPos;
		curr_ = oldTok;
		return r;
	}
};

} // namespace util
