#pragma once

#include "fmt/format.h"
#include "util/lexer.h"
#include "util/vector.h"
#include <string>
#include <variant>
#include <vector>

namespace util {

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
		else if (auto tok = lexer.try_match(Tok::floating()); tok)
			return floating(parse_float<floating_type>(tok->value));
		else if (auto tok = lexer.try_match(Tok::string()); tok)
			return string(parse_string(tok->value));
		else if (auto tok = lexer.try_match(Tok::ident()); tok)
		{
			if (tok->value == "null")
				return null();
			else if (tok->value == "false")
				return boolean(false);
			else if (tok->value == "true")
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
				lexer.match(":");
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

	// serialization delegates to the specializable trait struct
	template <typename T> Json(T const &value)
	{
		*this = JsonSerializer<T>::serialize(value);
	}

	// de-serialization
	template <typename T> T get()
	{
		if constexpr (std::is_same_v<T, bool>)
		{
			return visit(overloaded{[](null_type) -> bool { return false; },
			                        [](auto v) -> bool { return v; }});
		}
		else if constexpr (std::is_integral_v<T>)
		{
			return visit(
			    overloaded{[](null_type) -> T { return 0; },
			               [](boolean_type v) -> T { return v ? 1 : 0; },
			               [](integer_type v) -> T { return v; },
			               [](auto _) -> T { assert(false); }});
		}
		else if constexpr (std::is_floating_point_v<T>)
		{
			return visit(
			    overloaded{[](null_type) -> T { return 0; },
			               [](boolean_type v) -> T { return v ? 1 : 0; },
			               [](integer_type v) -> T { return v; },
			               [](floating_type v) -> T { return v; },
			               [](auto _) -> T { assert(false); }});
		}
		else
			assert(false);
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
	static Json parse(std::string_view s)
	{
		auto lex = Lexer(s);
		auto j = parse(lex);
		if (!lex.empty())
			throw ParseError(fmt::format("unexpected token '{}' at end of json",
			                             lex->value));
		return j;
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

  private:
	template <typename It> void print_impl(It &it, Json::null_type)
	{
		it = fmt::format_to(it, "null");
	}
	template <typename It> void print_impl(It &it, Json::boolean_type value)
	{
		it = fmt::format_to(it, value ? "true" : "false");
	}
	template <typename It> void print_impl(It &it, Json::integer_type value)
	{
		it = fmt::format_to(it, "{}", value);
	}
	template <typename It> void print_impl(It &it, Json::floating_type value)
	{
		it = fmt::format_to(it, "{}", value);
	}
	template <typename It>
	void print_impl(It &it, Json::string_type const &value)
	{
		// TODO: escape special characters
		it = fmt::format_to(it, "\"{}\"", value);
	}
	template <typename It> void print_impl(It &it, Json::array_type const &arr)
	{
		if (arr.empty())
		{
			it = fmt::format_to(it, "[]");
			return;
		}

		for (size_t i = 0; i < arr.size(); ++i)
		{
			it = fmt::format_to(it, i ? ", " : "[");
			print(it, arr[i]);
		}
		it = fmt::format_to(it, "]");
	}
	template <typename It> void print_impl(It &it, Json::object_type const &obj)
	{
		if (obj.empty())
		{
			it = fmt::format_to(it, "{{}}");
			return;
		}
		it = fmt::format_to(it, "{{");
		bool first = true;
		for (auto &[key, value] : obj)
		{
			if (!first)
				it = fmt::format_to(it, ", ");
			first = false;
			it = fmt::format_to(it, "\"{}\": ", key);
			print(it, value);
		}
		it = fmt::format_to(it, "}}");
	}

	template <typename It> void print(It &it, util::Json const &j)
	{
		j.visit([&](auto &value) { print_impl(it, value); });
	}

  public:
	constexpr auto parse(format_parse_context &ctx) { return ctx.begin(); }

	template <typename FormatContext>
	auto format(Json const &j, FormatContext &ctx)
	{
		auto it = ctx.out();
		print(it, j);
		return it;
	}
};
