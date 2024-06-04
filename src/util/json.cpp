#include "util/json.h"

#include "util/io.h"
#include "util/lexer.h"

namespace util {
namespace {
Json parse_json(Lexer &lexer)
{
	if (auto tok = lexer.try_match(Tok::integer()); tok)
		return Json::integer(parse_int<Json::integer_type>(tok->value));
	else if (auto tok = lexer.try_match(Tok::floating()); tok)
		return Json::floating(parse_float<Json::floating_type>(tok->value));
	else if (lexer.try_match("-"))
	{
		if (auto tok = lexer.try_match(Tok::integer()); tok)
			return Json::integer(-parse_int<Json::integer_type>(tok->value));
		else if (auto tok = lexer.try_match(Tok::floating()); tok)
			return Json::floating(
			    -parse_float<Json::floating_type>(tok->value));
		else
			raise<ParseError>(
			    "expected integer or floating point number after '-' in json");
	}
	else if (auto tok = lexer.try_match(Tok::string()); tok)
		return Json::string(parse_string(tok->value));
	else if (auto tok = lexer.try_match(Tok::ident()); tok)
	{
		if (tok->value == "null" || tok->value == "None")
			return Json::null();
		else if (tok->value == "false" || tok->value == "False")
			return Json::boolean(false);
		else if (tok->value == "true" || tok->value == "True")
			return Json::boolean(true);
		else
			raise<ParseError>("unknown identifier '{}' in json", tok->value);
	}
	else if (lexer.try_match("["))
	{
		Json j;
		auto &a = j.as_array();
		while (!lexer.try_match("]"))
		{
			// commas are optional, trailing comma is allowed
			a.push_back(parse_json(lexer));
			lexer.try_match(",");
		}
		return j;
	}
	else if (lexer.try_match("("))
	{
		Json j;
		auto &a = j.as_array();
		while (!lexer.try_match(")"))
		{
			// commas are optional, trailing comma is allowed
			a.push_back(parse_json(lexer));
			lexer.try_match(",");
		}
		return j;
	}
	else if (auto tok = lexer.try_match("{"))
	{
		Json j;
		auto &a = j.as_object();
		while (!lexer.try_match("}"))
		{
			std::string key;
			if (auto k = lexer.try_match(Tok::ident()); k)
				key = k->value;
			else
				key = parse_string(lexer.match(Tok::string()).value);
			if (!lexer.try_match(":"))
				lexer.match("=");
			a[std::move(key)] = parse_json(lexer);

			// commas are optional, trailing comma is allowed
			lexer.try_match(",");
		}
		return j;
	}
	else
		raise<ParseError>("unexpected token '{}' in json", lexer->value);
}
} // namespace

Json Json::parse(std::string_view s)
{
	auto lex = Lexer(s);
	auto j = parse_json(lex);
	check<ParseError>(lex.empty(), "unexpected token '{}' in json", lex->value);
	return j;
}

Json Json::parse_file(std::string_view filename)
{
	return parse(read_file(filename));
}

} // namespace util