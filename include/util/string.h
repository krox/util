#pragma once

// utilities for string parsing

#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace util {

// Thrown on failed string parsing. Might be used outside this module
// (e.g. json parsing)
class ParseError : public std::runtime_error
{
  public:
	ParseError(std::string const &what) : std::runtime_error(what) {}
};

// trim leading and trailing whitespace from a string_view
std::string_view trim_white(std::string_view s);

// split a string by a delimiter
std::vector<std::string_view> split(std::string_view s, char delim);

// split a string by whitespace (std::isspace).
//   * Leading/trailing whitespace is trimmed
//   * multiple whitespace is collapsed. Thus the resulting
//     vector will not contain empty strings.
//   * resulting string_views point into the original string
std::vector<std::string_view> split_white(std::string_view s);

// Parse an integer literal, throwing ParseError if not the whole s was matched.
// Accepts {-}[0-9]+
template <std::integral T = int> T parse_int(std::string_view s);

// General tokenizer in the form of a pull-parser. It is intended to build
// custom domain-specific parsers on top of this.
//   * Does not do any allocations, only deals with 'string_view's that point to
//     chunks of the input string. Also, there is not explicit 'Token' class.
//     The caller has to figure out the class token coming next, either by
//     calling multiple matching functions, or using '.peek()' to look at the
//     next character.
//   * The matching functions simply return false/empty if the token the next
//     token does not match. 'ParseError' can still thrown in some cases, e.g.:
//       - unterminated strings
//       - unterminated comments (only of comment skipping is enabled)
//   * Tokens are only returned as 'string_view's, no further processing is
//     done. For example to get an actual integer value from an integer token,
//     the caller must invoke 'util::parse_int()' or similar themselves.
class Parser
{
  public:
	struct Options
	{
		// if not empty, this string starts a comment that runs to the end of
		// the line. Example: "//" (C/C++/Java), "#" (Python, shell)
		std::string comment_start;
	};

  private:
	// design note: keep the source around as a whole. Could be useful for nice
	// parse-error messages.
	std::string_view src_;
	size_t pos_ = 0;
	Options opt_ = {};

	// Advances through whitespace. This is called automatically after each
	// matched token. No need to call manually, thus private. Also skips
	// comments if enabled.
	void skip_white();

	// throws ParseError, decorated with position information
	[[noreturn]] void raise(std::string_view msg);

  public:
	// Caller must ensure that the string_view remains valid for the lifetime of
	// the parser, because the parser does not copy the source string.
	explicit Parser(std::string_view src, Options const &opt = {});

	// look at the next character without advancing.
	// returns 0 on end of input
	char peek() const;

	// advance if match, returns false if not
	bool match(char ch);
	bool match(std::string_view word);

	// matches everything up to the next whitespace or end of string
	// returns empty on end of string
	std::string_view word();

	// similar to 'match(std::string_view)', but only matches if the next
	// character after the word is not a letter or digit
	bool ident(std::string_view word);

	// matches [_a-zA-Z][a-zA-Z_0-9]* (or empty on mismatch)
	std::string_view ident();

	// matches -?[0-9]+ (returns empty on mismatch)
	std::string_view integer();

	// matches single- or double-quoted strings
	//   * Returns empty on mismatch. Matching return includes the quotes.
	//   * Quotes preceded by odd number of backslashes do not end the string.
	//     Other than that, escape sequences are not validated
	//   * unterminated strings throw ParseError
	std::string_view string();

	// true if the end of the string is reached
	bool end() const;

	// expect_* functions throw on mismatch
	void expect(char ch);
	void expect(std::string_view word);
	void expect_ident(std::string_view word);
	std::string_view expect_ident();
	std::string_view expect_integer();
	std::string_view expect_string();
	void expect_end();

	// convenience: 'expect_integer()' followed by 'parse_int()'
	template <std::integral T = int> T expect_int()
	{
		return parse_int<T>(expect_integer());
	}
};

} // namespace util