#pragma once

// My own little JSON library. Most notably, the parser is very forgiving,
// allowing pretty much "json5" syntax (trailing commas, single-quoted strings,
// non-quoted object keys, etc.).

#include "fmt/format.h"
#include "util/error.h"
#include "util/vector.h"
#include <concepts>
#include <stdexcept>
#include <string>
#include <variant>
#include <vector>

namespace util {

// TODO: not sure of line between JsonError/ParseError
class JsonTypeError : public std::runtime_error
{
  public:
	JsonTypeError(std::string const &what) : std::runtime_error(what) {}
};

// helper for std::visit
template <class... Ts> struct overloaded : Ts...
{
	using Ts::operator()...;
};
template <class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

template <typename T> struct JsonSerializer;

// At its core, 'Json' is a variant type that can hold any one of
// Null/Boolean/Integer/Floating/String/Array/Object
//   * The (implicit) templated constructor converts from any serializable type.
//   * By convention, "Null" is usable as a default value for any type, silently
//     converting to "empty array/object" and the like as needed. For example:
//         Json a,b;        // both are Null
//         a["foo"] = 42;   // a is now an Object
//         b.push_back(42); // b is now an Array
//         a.push_back(42); // throws "cant push_back to Object"
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

	// internal type used to store a json value (make sure 'null_type' is first
	// so default-construction works as expected)
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

	// throws JsonTypeError if *this is neither Nulll nor T
	template <class T> T &as()
	{
		if (std::holds_alternative<null_type>(value_))
			value_.template emplace<T>();
		T *p = std::get_if<T>(&value_);
		if (p == nullptr)
			raise<JsonTypeError>("json value is not a {}", typeid(T).name());
		return *p;
	}
	template <class T> T const &as() const
	{
		if (std::holds_alternative<null_type>(value_))
		{
			static const T default_value = {};
			return default_value;
		}
		T const *p = std::get_if<T>(&value_);
		if (p == nullptr)
			raise<JsonTypeError>("json value is not a {}", typeid(T).name());
		return *p;
	}

  public:
	// creates a Null value
	Json() = default;

	// Explicitly defaulting copy/move constructor/assignment prevents
	// accidentally using the templated constructor (which would lead to weird
	// circular calls). Not completely sure if this is necessary.
	Json(Json const &) = default;
	Json(Json &&) noexcept = default;
	Json &operator=(Json const &) = default;
	Json &operator=(Json &&) noexcept = default;

	// Pseudo-constructors with explicit type.
	// Typically not necessary thanks to the templated constructor.
	static Json null() { return {}; }
	static Json boolean(boolean_type v) { return {direct(), v}; }
	static Json integer(integer_type v) { return {direct(), v}; }
	static Json floating(floating_type v) { return {direct(), v}; }
	static Json string(std::string_view v)
	{
		return {direct(), string_type(v)};
	}
	static Json array(size_t n = 0) { return {direct(), array_type(n)}; }
	static Json object() { return {direct(), object_type()}; }

	// Get underlying data.
	// 'Null' is converted to requested type, other mismatches throw.
	null_type &as_null() { return as<null_type>(); }
	boolean_type &as_boolean() { return as<boolean_type>(); }
	integer_type &as_integer() { return as<integer_type>(); }
	floating_type &as_floating() { return as<floating_type>(); }
	string_type &as_string() { return as<string_type>(); }
	array_type &as_array() { return as<array_type>(); }
	object_type &as_object() { return as<object_type>(); }

	null_type as_null() const { return as<null_type>(); }
	boolean_type as_boolean() const { return as<boolean_type>(); }
	integer_type as_integer() const { return as<integer_type>(); }
	floating_type as_floating() const { return as<floating_type>(); }
	string_type const &as_string() const { return as<string_type>(); }
	array_type const &as_array() const { return as<array_type>(); }
	object_type const &as_object() const { return as<object_type>(); }

	// explicitly check type
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
	bool is_number() const { return is_integer() || is_floating(); }

	// Serialize arbitry types to Json.
	//     * For convenience, this is non-explicit, making things like
	//           Json j; j["foo"] = 42;
	//       much nicer to look at.
	//     * For custom types, you can either specialize JsonSerializer<T> or
	//       overload to_json(T) to be found by ADL.
	template <class T> Json(T const &value)
	{
		*this = JsonSerializer<T>::serialize(value);
	}

	// de-serialization to arbitrary types.
	//     * For custom types, you can either specialize JsonSerializer<T> or
	//       overload from_json(T, Json) to be found by ADL.
	template <class T> T get() const
	{
		return JsonSerializer<T>::deserialize(*this);
	}

	// array-like access
	void push_back(Json const &val) { as_array().push_back(val); }
	void push_back(Json &&val) { as_array().push_back(std::move(val)); }
	Json &operator[](size_t i) { return as_array().at(i); }
	Json const &operator[](size_t i) const { return as_array().at(i); }

	// object-like access
	Json &operator[](std::string_view key) { return as_object()[key]; }
	Json &at(std::string_view key) { return as_object().at(key); }
	Json const &at(std::string_view key) const { return as_object().at(key); }

	// short-hand for object-like access with default value. This is an
	// extremely common operation when reading config files.
	template <class T>
	T value(std::string_view key, T const &default_value) const
	{
		auto &o = as_object();
		if (auto it = o.find(key); it != o.end())
			return it->second.get<T>();
		return default_value;
	}

	bool operator==(Json const &other) const noexcept
	{
		return value_ == other.value_;
	}

	// parse (a superset of) Json, throwing on syntax errors
	//     - comments are accepted and ignored (// and /* */)
	//     - commas in array/object are optional and trailing commas are allowed
	//     - single-quoted and double-quoted strings allowed
	//     - object keys without quotes are allowed (C identifier rules)
	//     - arrays can be surrounded by () instead of []
	//     - python-like True/False/None are synonymous to true/false/null
	//     - '=' can be used instead of ':' in objects
	static Json parse(std::string_view s);

	// read and parse a json file
	static Json parse_file(std::string_view filename);
};

// serializer for standard types
inline Json to_json(std::nullptr_t) { return Json::null(); }
inline Json to_json(bool value) { return Json::boolean(value); }
Json to_json(std::integral auto value) { return Json::integer(value); }
Json to_json(std::floating_point auto value) { return Json::floating(value); }
inline Json to_json(std::string_view s) { return Json::string(s); }
inline Json to_json(char const *s) { return Json::string(s); }
template <class T> Json to_json(std::vector<T> const &a)
{
	Json j;
	auto &v = j.as_array();
	v.reserve(a.size());
	for (auto &x : a)
		v.emplace_back(x);
	return j;
}

// deserializer for standard types
inline void from_json(bool &value, Json const &j) { value = j.as_boolean(); }
void from_json(std::integral auto &value, Json const &j)
{
	value = j.as_integer();
}
inline void from_json(std::string &value, Json const &j)
{
	value = j.as_string();
}

// template for floating point types

void from_json(std::floating_point auto &value, Json const &j)
{
	j.visit(util::overloaded{
	    [&](Json::null_type const &) { value = 0; },
	    [&](Json::integer_type const &a) { value = a; },
	    [&](Json::floating_type const &a) { value = a; },
	    [&](auto const &) { raise<JsonTypeError>("expected float"); },
	});
}

template <class T> void from_json(std::vector<T> &value, Json const &j)
{
	auto &a = j.as_array();
	value.resize(0);
	value.reserve(a.size());
	for (auto &x : a)
		value.emplace_back(x.get<T>());
}

// For user-defined types, you can either overload to_json() to be found by ADL,
// or specialize this struct.
template <typename T> struct JsonSerializer
{
	static Json serialize(T const &value) { return to_json(value); }

	static T deserialize(Json const &j)
	{
		T value;
		from_json(value, j);
		return value;
	}
};

template <size_t N> struct JsonSerializer<char[N]>
{
	static Json serialize(char const *s) { return Json::string(s); }
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

	template <typename It> void print_impl(It &it, Json::null_type, int) const
	{
		it = fmt::format_to(it, "null");
	}
	template <typename It>
	void print_impl(It &it, Json::boolean_type value, int) const
	{
		it = fmt::format_to(it, "{}", value ? "true" : "false");
	}
	template <typename It>
	void print_impl(It &it, Json::integer_type value, int) const
	{
		it = fmt::format_to(it, "{}", value);
	}
	template <typename It>
	void print_impl(It &it, Json::floating_type value, int) const
	{
		it = fmt::format_to(it, "{}", value);
	}
	template <typename It>
	void print_impl(It &it, Json::string_type const &value, int) const
	{
		// TODO: escape special characters
		it = fmt::format_to(it, "\"{}\"", value);
	}
	template <typename It>
	void print_impl(It &it, Json::array_type const &arr, int level) const
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
	void print_impl(It &it, Json::object_type const &obj, int level) const
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

	template <typename It>
	void print(It &it, util::Json const &j, int level) const
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
	auto format(Json const &j, FormatContext &ctx) const
	{
		auto it = ctx.out();
		print(it, j, 0);
		return it;
	}
};
