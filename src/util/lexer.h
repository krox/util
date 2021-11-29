#pragma once

/**
 * A simple tokenizer based on std::string_view. Right now it is mostly useful
 * for simple math expressions. But the hope is that it might become general
 * enough to support some programming languages (or at least JSON and the like)
 */

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

inline int parse_int(std::string_view s)
{
	int r = 0;
	for (size_t i = 0; i < s.size(); ++i)
	{
		if (!('0' <= s[i] && s[i] <= '9'))
			throw ParseError(
			    fmt::format("unexpected character '{}' in integer", s[i]));
		if (r > (INT_MAX - 9) / 10) // not exactly tight
			throw ParseError(
			    fmt::format("integer overflow while parsing in '{}'", s));
		r = 10 * r + (s[i] - '0');
	}
	return r;
}

inline int64_t parse_int64(std::string_view s)
{
	int64_t r = 0;
	for (size_t i = 0; i < s.size(); ++i)
	{
		if (!('0' <= s[i] && s[i] <= '9'))
			throw ParseError(
			    fmt::format("unexpected character '{}' in integer", s[i]));
		if (r > (INT64_MAX - 9) / 10) // not exactly tight
			throw ParseError(
			    fmt::format("integer overflow while parsing in '{}'", s));
		r = 10 * r + (s[i] - '0');
	}
	return r;
}

enum class Tok
{
	// clang-format off
	None,                         // end of input
	Ident,                        // identifier
	Int, Float,                   // literals
	Add, Sub, Mul, Div, Mod, Pow, // arithmetic operators
	Assign,                       // =
	Comma, Semi, Dot,             // , ; .
	OpenParen, CloseParen,        // ( )

	// clang-format on
};

struct Token
{
	Tok tok = Tok::None;         // "type" of token
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
 *    - operators: [+*-/%^(),;.=]
 *  Future design choices:
 *    - should be include a location into Token for nicer errors?
 *    - should we support meaningful whitespace?
 *    - how to handle keywords?
 *    - how to handle multi-character operators?
 */
class Lexer
{
	size_t pos_ = 0;
	/*const*/ std::string_view src_ = {};
	Token curr_ = {Tok::None, {}};

	// TODO: replacing std::string_view by std::bitset<128> would be faster
	char tryMatch(std::string_view chars)
	{
		if (pos_ == src_.size())
			return '\0';
		for (auto c : chars)
			if (c == src_[pos_])
				return src_[pos_++];
		return '\0';
	}

	char matchOne(std::string_view chars)
	{
		if (char c = tryMatch(chars); c)
			return c;
		else
			throw ParseError("expected character not found");
	}

	size_t matchAll(std::string_view chars)
	{
		size_t count = 0;
		while (tryMatch(chars))
			++count;
		return count;
	}

  public:
	Lexer() = default;
	Lexer(std::string_view buf) : src_(buf) { advance(); }

	bool empty() const { return curr_.tok == Tok::None; }

	void advance()
	{
		// skip whitespace and quit if nothing is left
		matchAll(" \t\n\r");
		if (pos_ == src_.size())
		{
			curr_ = {Tok::None, {}};
			return;
		}

		size_t start = pos_;

		// identifier
		if (tryMatch("_abcdefghijklmnopqrstuvwxyz"))
		{
			curr_.tok = Tok::Ident;
			matchAll("_abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ"
			         "0123456789");
		}
		// operator
		else if (char c = tryMatch("+-*/%^()=,;."); c)
		{
			switch (c)
			{
				// clang-format off
			case '+': curr_.tok = Tok::Add; break;
			case '-': curr_.tok = Tok::Sub; break;
			case '*': curr_.tok = Tok::Mul; break;
			case '/': curr_.tok = Tok::Div; break;
			case '^': curr_.tok = Tok::Pow; break;
			case '(': curr_.tok = Tok::OpenParen; break;
			case ')': curr_.tok = Tok::CloseParen; break;
			case '=': curr_.tok = Tok::Assign; break;
			case ',': curr_.tok = Tok::Comma; break;
			case ';': curr_.tok = Tok::Semi; break;
			case '.': curr_.tok = Tok::Dot; break;
				// clang-format on
			default:
				throw ParseError(fmt::format("unexpected operator '{}'", c));
			}
		}
		// int/float literal
		else if (tryMatch("0123456789"))
		{
			curr_.tok = Tok::Int;
			matchAll("0123456789");
			if (tryMatch("."))
			{
				curr_.tok = Tok::Float;
				matchAll("0123456789");
			}
			if (tryMatch("eE"))
			{
				curr_.tok = Tok::Float;
				tryMatch("+-");
				if (matchAll("0123456789") == 0)
					throw ParseError("expected exponent after 'e'");
			}
		}
		else
			throw ParseError(
			    fmt::format("unexpected character '{}'", src_[start]));

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

	std::optional<Token> tryMatch(Tok tok)
	{
		if (curr_.tok != tok)
			return std::nullopt;
		else
			return pop();
	}

	Token match(Tok tok)
	{
		if (auto r = tryMatch(tok); r)
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
