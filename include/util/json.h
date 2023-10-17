#pragma once

#include "fmt/format.h"
#include "util/io.h"
#include "util/lexer.h"
#include "util/vector.h"
#include <stdexcept>
#include <string>
#include <variant>
#include <vector>

namespace util {

// TODO: not sure of line between JsonError/ParseError
class JsonError : public std::runtime_error
{
  public:
	JsonError(std::string const &what) : std::runtime_error(what) {}
};

// helper for std::visit
template <class... Ts> struct overloaded : Ts...
{
	using Ts::operator()...;
};
template <class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

template <class T, class... Ts> T &get_or_init(std::variant<Ts...> &v) noexcept
{
	if (!std::holds_alternative<T>(v))
		v.template emplace<T>();
	return std::get<T>(v);
}

template <typename T> struct JsonSerializer;

class Json
{
  public:
	using null_type = std::monostate;
	using boolean_type = bool;
	using integer_type = int64_t;
	using floating_type = double;
	using string_type = std::string;
	using array_type = std::vector<Json>;
	// TODO: Tiny_map is not suitable if the number of keys is large.
	//       But std::map/std::unordered_map/util::hash_set do not keep the
	//       insertion order, which I like here.
	using object_type = util::tiny_map<std::string, Json>;

	using variant_type =
	    std::variant<null_type, boolean_type, integer_type, floating_type,
	                 string_type, array_type, object_type>;

	template <class R = void, class Visitor> R visit(Visitor &&vis)
	{
		return std::visit(std::forward<Visitor>(vis), value_);
	}
	template <class R = void, class Visitor> R visit(Visitor &&vis) const
	{
		return std::visit(std::forward<Visitor>(vis), value_);
	}

  private:
	variant_type value_ = {};

	struct direct
	{};
	Json(direct, variant_type &&val) : value_(std::move(val)) {}

	static Json parse(Lexer &lexer)
	{
		if (auto tok = lexer.try_match(Tok::integer()); tok)
			return integer(parse_int<integer_type>(tok->value));
		else if (auto tok = lexer.try_match("-"); tok)
			return integer(
			    -parse_int<integer_type>(lexer.match(Tok::integer()).value));
		else if (auto tok = lexer.try_match(Tok::floating()); tok)
			return floating(parse_float<floating_type>(tok->value));
		else if (auto tok = lexer.try_match(Tok::string()); tok)
			return string(parse_string(tok->value));
		else if (auto tok = lexer.try_match(Tok::ident()); tok)
		{
			if (tok->value == "null" || tok->value == "None")
				return null();
			else if (tok->value == "false" || tok->value == "False")
				return boolean(false);
			else if (tok->value == "true" || tok->value == "True")
				return boolean(true);
			else
				throw ParseError(
				    fmt::format("unknown identifier '{}' in json", tok->value));
		}
		else if (lexer.try_match("["))
		{
			Json j;
			auto &a = j.as_array();
			while (!lexer.try_match("]"))
			{
				// commas are optional, trailing comma is allowed
				a.push_back(parse(lexer));
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
				a.push_back(parse(lexer));
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
				a[std::move(key)] = parse(lexer);

				// commas are optional, trailing comma is allowed
				lexer.try_match(",");
			}
			return j;
		}
		else
			throw ParseError(
			    fmt::format("unexpected token '{}' in json", lexer->value));
	}

  public:
	Json() = default;

	// Prevent usage of the templated constructor which might lead to weird
	// circular calls. Not sure if necessary.
	Json(Json const &) = default;
	Json(Json &&) = default;
	Json &operator=(Json const &) = default;
	Json &operator=(Json &&) = default;

	// pseudo-constructors with explicit type
	static Json null() { return {}; }
	static Json boolean(boolean_type v) { return {direct(), v}; }
	static Json integer(integer_type v) { return {direct(), v}; }
	static Json floating(floating_type v) { return {direct(), v}; }
	static Json string(std::string_view v)
	{
		return {direct(), string_type(v)};
	}
	static Json array() { return {direct(), array_type()}; }
	static Json object() { return {direct(), object_type()}; }

	// get underlying data, change and default-init if it does not match
	null_type &as_null() { return get_or_init<null_type>(value_); }
	boolean_type &as_boolean() { return get_or_init<boolean_type>(value_); }
	integer_type &as_integer() { return get_or_init<integer_type>(value_); }
	floating_type &as_floating() { return get_or_init<floating_type>(value_); }
	string_type &as_string() { return get_or_init<string_type>(value_); }
	array_type &as_array() { return get_or_init<array_type>(value_); }
	object_type &as_object() { return get_or_init<object_type>(value_); }

	bool is_null() const { return std::holds_alternative<null_type>(value_); }
	bool is_boolean() const
	{
		return std::holds_alternative<boolean_type>(value_);
	}
	bool is_integer() const
	{
		return std::holds_alternative<integer_type>(value_);
	}
	bool is_floating() const
	{
		return std::holds_alternative<floating_type>(value_);
	}
	bool is_string() const
	{
		return std::holds_alternative<string_type>(value_);
	}
	bool is_array() const { return std::holds_alternative<array_type>(value_); }
	bool is_object() const
	{
		return std::holds_alternative<object_type>(value_);
	}

	// serialization delegates to the specializable trait struct
	template <typename T> Json(T const &value)
	{
		*this = JsonSerializer<T>::serialize(value);
	}

	// de-serialization. Does some type conversion, but nothing too speculative
	template <typename T> T get() const
	{
		if constexpr (std::is_same_v<T, bool>)
		{
			if (is_boolean())
				return std::get<boolean_type>(value_);
			else
				throw JsonError("json value is not a boolean");
		}
		else if constexpr (std::is_integral_v<T>)
		{
			if (is_integer())
				return std::get<integer_type>(value_);
			else
				throw JsonError("json value is not an integer");
		}
		else if constexpr (std::is_floating_point_v<T>)
		{
			if (is_floating())
				return std::get<floating_type>(value_);
			else if (is_integer())
				return std::get<integer_type>(value_);
			else
				throw JsonError("json value is not a number");
		}
		else if constexpr (std::is_same_v<T, std::string>)
		{
			if (is_string())
				return std::get<string_type>(value_);
			else if (is_integer())
				return fmt::format("{}", std::get<integer_type>(value_));
			else if (is_floating())
				return fmt::format("{}", std::get<integer_type>(value_));
			else if (is_null())
				return "null";
			else if (is_boolean())
				return std::get<boolean_type>(value_) ? "true" : "false";
			else
				throw JsonError("json value is not a string");
		}
		else if constexpr (std::is_same_v<T, std::vector<size_t>>)
		{
			// TODO: yikes. need to redo this whole function with some
			//       "struct JsonDeserializer" or so...

			if (!is_array())
				throw JsonError("json value is not an array");
			auto &arr = std::get<array_type>(value_);
			std::vector<size_t> r;
			r.reserve(arr.size());
			for (auto &x : arr)
				r.push_back(x.get<size_t>());
			return r;
		}
		else
			throw JsonError("unsuppoerted json conversion");
	}

	// array-like access
	//     * automatically resizes to sufficient number of elements
	//     * converts to array (discarding old data) if it wasnt an array before
	template <typename T> void push_back(T &&val)
	{
		as_array().push_back(std::forward<T>(val));
	}
	Json &operator[](size_t i)
	{
		auto &a = as_array();
		if (i >= a.size())
		{
			// NOTE: typically, std::vector does not over-allocate on resize
			a.reserve(a.capacity() * 2);
			a.resize(i + 1);
		}
		return a[i];
	}

	// object-like access
	//     * discards ald value, if not an object before
	Json &operator[](std::string_view key) { return as_object()[key]; }

	// parse (a superset of) Json, throwing on syntax errors
	//     - commas in array/object are optional and trailing commas are allowed
	//     - single-quoted and double-quoted strings allowed
	//     - object keys without quotes are allowed (C identifier rules)
	//     - arrays can be surrounded by () instead of []
	//     - python-like True/False/None are synonymous to true/false/null
	static Json parse(std::string_view s)
	{
		auto lex = Lexer(s);
		auto j = parse(lex);
		if (!lex.empty())
			throw ParseError(fmt::format("unexpected token '{}' at end of json",
			                             lex->value));
		return j;
	}

	static Json parse_file(std::string_view filename)
	{
		return parse(read_file(filename));
	}
};

// serializer for standard types
inline Json to_json(bool value) { return Json::boolean(value); }
inline Json to_json(char value) { return Json::integer(value); }
inline Json to_json(signed char value) { return Json::integer(value); }
inline Json to_json(unsigned char value) { return Json::integer(value); }
inline Json to_json(short value) { return Json::integer(value); }
inline Json to_json(unsigned short value) { return Json::integer(value); }
inline Json to_json(int value) { return Json::integer(value); }
inline Json to_json(unsigned int value) { return Json::integer(value); }
inline Json to_json(long value) { return Json::integer(value); }
inline Json to_json(unsigned long value) { return Json::integer(value); }
inline Json to_json(long long value) { return Json::integer(value); }
inline Json to_json(unsigned long long value) { return Json::integer(value); }
inline Json to_json(float value) { return Json::floating(value); }
inline Json to_json(double value) { return Json::floating(value); }
inline Json to_json(long double value) { return Json::floating(value); }
inline Json to_json(std::string_view s) { return Json::string(s); }
inline Json to_json(char const *s) { return Json::string(s); }
template <size_t N> Json to_json(const char s[N]) { return Json::string(s); }
template <class T> Json to_json(std::vector<T> const &a)
{
	Json j;
	auto &v = j.as_array();
	v.reserve(a.size());
	for (auto &x : a)
		v.emplace_back(x);
	return j;
}

// For user-defined types, you can either overload to_json() to be found by ADL,
// or specialize this struct.
template <typename T> struct JsonSerializer
{
	static Json serialize(T const &value) { return to_json(value); }
};

} // namespace util

template <> struct fmt::formatter<util::Json>
{
	using Json = util::Json;

	// format settings (see parse() function)
	enum Spec
	{
		standard, // single-line, nice spaces
		human,    // multi-line, with indentations
		compact   // no whitespace at all
	} spec = standard;

  private:
	// print newline + indentation
	template <typename It> static void newline(It &it, int level)
	{
		*it++ = '\n';
		for (int i = 0; i < level * 4; ++i)
			*it++ = ' ';
	}

	// helper to determine array-formatting
	static bool is_trivial(Json const &j)
	{
		return j.visit<bool>(util::overloaded{
		    [](Json::array_type const &a) { return a.empty(); },
		    [](Json::object_type const &a) { return a.empty(); },
		    [](auto const &) { return true; },
		});
	}

	template <typename It> void print_impl(It &it, Json::null_type, int)
	{
		it = fmt::format_to(it, "null");
	}
	template <typename It>
	void print_impl(It &it, Json::boolean_type value, int)
	{
		it = fmt::format_to(it, "{}", value ? "true" : "false");
	}
	template <typename It>
	void print_impl(It &it, Json::integer_type value, int)
	{
		it = fmt::format_to(it, "{}", value);
	}
	template <typename It>
	void print_impl(It &it, Json::floating_type value, int)
	{
		it = fmt::format_to(it, "{}", value);
	}
	template <typename It>
	void print_impl(It &it, Json::string_type const &value, int)
	{
		// TODO: escape special characters
		it = fmt::format_to(it, "\"{}\"", value);
	}
	template <typename It>
	void print_impl(It &it, Json::array_type const &arr, int level)
	{
		// empty array -> '[]' regardless of format
		if (arr.empty())
		{
			it = fmt::format_to(it, "[]");
			return;
		}

		// if the array is small and simple, use standard even if spec == human
		Spec spec = this->spec;
		if (spec == human && arr.size() <= 4 &&
		    std::all_of(arr.begin(), arr.end(), is_trivial))
			spec = standard;

		// actual formatting
		*it++ = '[';
		for (size_t i = 0; i < arr.size(); ++i)
		{
			if (i)
				*it++ = ',';
			if (spec == human)
				newline(it, level + 1);
			if (i && spec == standard)
				*it++ = ' ';
			print(it, arr[i], level + 1);
		}
		if (spec == human)
			newline(it, level);
		*it++ = ']';
	}
	template <typename It>
	void print_impl(It &it, Json::object_type const &obj, int level)
	{
		if (obj.empty())
		{
			it = fmt::format_to(it, "{{}}");
			return;
		}

		*it++ = '{';
		bool first = true;
		for (auto &[key, value] : obj)
		{
			if (!first)
				*it++ = ',';
			if (spec == human)
				newline(it, level + 1);
			if (!first && spec == standard)
				*it++ = ' ';
			// TODO: escape special characters
			it = fmt::format_to(it, "\"{}\":", key);
			if (spec != compact)
				*it++ = ' ';
			print(it, value, level + 1);
			first = false;
		}
		if (spec == human)
			newline(it, level);
		*it++ = '}';
	}

	template <typename It> void print(It &it, util::Json const &j, int level)
	{
		j.visit([&](auto &value) { print_impl(it, value, level); });
	}

  public:
	constexpr auto parse(format_parse_context &ctx)
	{
		// Parse the presentation format and store it in the formatter:
		auto it = ctx.begin(), end = ctx.end();
		if (it != end && *it != '}')
			switch (*it++)
			{
			case 'h':
				spec = human;
				break;
			case 'c':
				spec = compact;
				break;
			default:
				throw fmt::format_error("invalid format");
			}

		if (it != end && *it != '}')
			throw format_error("invalid format");

		return it;
	}

	template <typename FormatContext>
	auto format(Json const &j, FormatContext &ctx)
	{
		auto it = ctx.out();
		print(it, j, 0);
		return it;
	}
};
