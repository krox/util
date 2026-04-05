#pragma once

#ifdef UTIL_LUA

extern "C"
{
#include <lauxlib.h>
#include <lua.h>
}

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

namespace util {

template <class T> struct lua_struct_traits;

template <class T>
concept lua_struct_type =
    requires { lua_struct_traits<std::remove_cvref_t<T>>::fields; };

template <class Struct, class Member> struct LuaField
{
	std::string_view name;
	Member Struct::*member;
};

template <class Struct, class Member>
constexpr LuaField<Struct, Member> lua_field(std::string_view name,
                                             Member Struct::*member)
{
	return {name, member};
}

class LuaError : public std::runtime_error
{
  public:
	using std::runtime_error::runtime_error;
};

class Lua
{
  private:
	template <class T> struct is_std_function : std::false_type
	{};
	template <class R, class... Args>
	struct is_std_function<std::function<R(Args...)>> : std::true_type
	{};
	template <class T>
	static constexpr bool is_std_function_v =
	    is_std_function<std::remove_cvref_t<T>>::value;

	template <class T>
	static constexpr bool can_deduce_std_function_v =
	    requires { std::function(std::declval<T>()); };

  public:
	// Note: Lua is not thread-safe. External synchronization is required when
	// sharing one instance across threads.
	Lua();
	Lua(Lua const &) = delete;
	Lua &operator=(Lua const &) = delete;
	Lua(Lua &&other) noexcept;
	Lua &operator=(Lua &&other) noexcept;
	~Lua();

	void close() noexcept;
	explicit operator bool() const noexcept;

	void run(std::string_view source,
	         std::string_view chunk_name = "util::Lua::run");

	template <class T>
	    requires(!is_std_function_v<std::remove_cvref_t<T>> &&
	             !can_deduce_std_function_v<std::remove_cvref_t<T>>)
	void set(std::string_view key, T const &value)
	{
		if (state_ == nullptr)
			throw LuaError("lua state is closed");
		StackGuard guard(state_);
		push_value(state_, value);
		lua_setglobal(state_, std::string(key).c_str());
	}

	template <class T> T get(std::string_view key) const
	{
		if (state_ == nullptr)
			throw LuaError("lua state is closed");
		StackGuard guard(state_);
		lua_getglobal(state_, std::string(key).c_str());
		return read_value<T>(state_, -1);
	}

	template <class R, class... Args>
	void set(std::string_view key, std::function<R(Args...)> fn)
	{
		if (state_ == nullptr)
			throw LuaError("lua state is closed");
		StackGuard guard(state_);
		using Holder = FunctionHolder<R, Args...>;
		auto holder = std::make_unique<Holder>(std::move(fn));
		Holder *raw = holder.get();
		functions_.push_back(std::move(holder));

		lua_pushlightuserdata(state_, raw);
		lua_pushcclosure(state_, &Holder::dispatch, 1);
		lua_setglobal(state_, std::string(key).c_str());
	}

	template <class F>
	    requires(!is_std_function_v<std::remove_cvref_t<F>> &&
	             can_deduce_std_function_v<std::remove_cvref_t<F>>)
	void set(std::string_view key, F &&fn)
	{
		auto wrapped = std::function(std::forward<F>(fn));
		set(key, std::move(wrapped));
	}

	static int64_t read_integer(lua_State *L, int index);
	static double read_floating(lua_State *L, int index);
	static bool read_boolean(lua_State *L, int index);
	static std::string read_string(lua_State *L, int index);

  private:
	struct StackGuard
	{
		explicit StackGuard(lua_State *L) : L(L), top(lua_gettop(L)) {}
		~StackGuard() { lua_settop(L, top); }

		lua_State *L;
		int top;
	};

	struct FunctionBase
	{
		virtual ~FunctionBase() = default;
	};

	template <class R, class... Args> struct FunctionHolder : FunctionBase
	{
		explicit FunctionHolder(std::function<R(Args...)> fn)
		    : fn(std::move(fn))
		{}

		std::function<R(Args...)> fn;

		static int dispatch(lua_State *L)
		{
			auto *self = static_cast<FunctionHolder *>(
			    lua_touserdata(L, lua_upvalueindex(1)));
			if (self == nullptr)
			{
				lua_pushliteral(L, "invalid C++ function binding");
				return lua_error(L);
			}

			try
			{
				if constexpr (std::is_void_v<R>)
				{
					self->call_void(L, std::index_sequence_for<Args...>{});
					return 0;
				}
				else
				{
					R result =
					    self->call_value(L, std::index_sequence_for<Args...>{});
					Lua::push_value(L, result);
					return 1;
				}
			}
			catch (std::exception const &e)
			{
				lua_pushstring(L, e.what());
				return lua_error(L);
			}
			catch (...)
			{
				lua_pushliteral(L, "unknown C++ exception");
				return lua_error(L);
			}
		}

		template <std::size_t... I>
		R call_value(lua_State *L, std::index_sequence<I...>)
		{
			check_arity(L);
			return fn(Lua::read_value<std::remove_cvref_t<Args>>(
			    L, static_cast<int>(I) + 1)...);
		}

		template <std::size_t... I>
		void call_void(lua_State *L, std::index_sequence<I...>)
		{
			check_arity(L);
			fn(Lua::read_value<std::remove_cvref_t<Args>>(
			    L, static_cast<int>(I) + 1)...);
		}

		void check_arity(lua_State *L) const
		{
			int argc = lua_gettop(L);
			if (argc != static_cast<int>(sizeof...(Args)))
				throw LuaError(
				    "wrong number of arguments when calling C++ function");
		}
	};

	template <class T> static T read_value(lua_State *L, int index)
	{
		if constexpr (std::is_same_v<std::remove_cvref_t<T>, bool>)
			return read_boolean(L, index);
		else if constexpr (std::is_integral_v<std::remove_cvref_t<T>>)
			return static_cast<T>(read_integer(L, index));
		else if constexpr (std::is_floating_point_v<std::remove_cvref_t<T>>)
			return static_cast<T>(read_floating(L, index));
		else if constexpr (std::is_same_v<std::remove_cvref_t<T>, std::string>)
			return read_string(L, index);
		else if constexpr (is_lua_struct_v<std::remove_cvref_t<T>>)
			return read_struct<std::remove_cvref_t<T>>(L, index);
		else if constexpr (is_std_vector_v<std::remove_cvref_t<T>>)
			return read_vector<T>(L, index);
		else
			static_assert(always_false_v<T>,
			              "unsupported Lua <-> C++ conversion");
	}

	template <class T> static void push_value(lua_State *L, T const &value)
	{
		if constexpr (std::is_same_v<std::remove_cvref_t<T>, bool>)
			lua_pushboolean(L, value ? 1 : 0);
		else if constexpr (std::is_integral_v<std::remove_cvref_t<T>>)
			lua_pushinteger(L, static_cast<lua_Integer>(value));
		else if constexpr (std::is_floating_point_v<std::remove_cvref_t<T>>)
			lua_pushnumber(L, static_cast<lua_Number>(value));
		else if constexpr (std::is_same_v<std::remove_cvref_t<T>, std::string>)
			lua_pushlstring(L, value.data(), value.size());
		else if constexpr (std::is_same_v<std::remove_cvref_t<T>,
		                                  std::string_view>)
			lua_pushlstring(L, value.data(), value.size());
		else if constexpr (is_lua_struct_v<std::remove_cvref_t<T>>)
			push_struct(L, value);
		else if constexpr (is_std_span_v<std::remove_cvref_t<T>>)
			push_sequence(L, value);
		else if constexpr (is_std_vector_v<std::remove_cvref_t<T>>)
			push_sequence(L, value);
		else
			static_assert(always_false_v<T>,
			              "unsupported Lua <-> C++ conversion");
	}

	template <class T> struct is_std_vector : std::false_type
	{};
	template <class T, class Alloc>
	struct is_std_vector<std::vector<T, Alloc>> : std::true_type
	{};
	template <class T>
	static constexpr bool is_std_vector_v =
	    is_std_vector<std::remove_cvref_t<T>>::value;

	template <class T> struct is_std_span : std::false_type
	{};
	template <class T, std::size_t Extent>
	struct is_std_span<std::span<T, Extent>> : std::true_type
	{};
	template <class T>
	static constexpr bool is_std_span_v =
	    is_std_span<std::remove_cvref_t<T>>::value;

	template <class T>
	static constexpr bool is_lua_struct_v =
	    lua_struct_type<std::remove_cvref_t<T>>;

	template <class T> static constexpr bool always_false_v = false;

	template <class Struct, class Member>
	static bool assign_struct_field(lua_State *L, Struct &out,
	                                std::string_view key, int index,
	                                LuaField<Struct, Member> const &field)
	{
		if (field.name != key)
			return false;

		try
		{
			out.*(field.member) = read_value<Member>(L, index);
		}
		catch (LuaError const &e)
		{
			throw LuaError("invalid value for field '" + std::string(key) +
			               "': " + e.what());
		}

		return true;
	}

	template <class Struct, class Tuple, std::size_t... I>
	static bool assign_struct_field(lua_State *L, Struct &out,
	                                std::string_view key, int index,
	                                Tuple const &fields,
	                                std::index_sequence<I...>)
	{
		return (assign_struct_field(L, out, key, index, std::get<I>(fields)) ||
		        ...);
	}

	template <class Struct>
	static bool assign_struct_field(lua_State *L, Struct &out,
	                                std::string_view key, int index)
	{
		using Traits = lua_struct_traits<Struct>;
		return assign_struct_field(
		    L, out, key, index, Traits::fields,
		    std::make_index_sequence<std::tuple_size_v<
		        std::remove_reference_t<decltype(Traits::fields)>>>{});
	}

	template <class Struct> static Struct read_struct(lua_State *L, int index)
	{
		if (!lua_istable(L, index))
			throw LuaError("expected Lua table");

		StackGuard guard(L);
		int abs = lua_absindex(L, index);
		Struct out{};

		lua_pushnil(L);
		while (lua_next(L, abs) != 0)
		{
			if (!lua_isstring(L, -2))
				throw LuaError("expected string key in Lua table");

			size_t len = 0;
			char const *key = lua_tolstring(L, -2, &len);
			if (key == nullptr)
				throw LuaError("expected string key in Lua table");

			std::string_view name(key, len);
			if (!assign_struct_field(L, out, name, -1))
				throw LuaError("unknown Lua table key '" + std::string(name) +
				               "'");

			lua_pop(L, 1);
		}

		return out;
	}

	template <class Struct, class Member>
	static void push_struct_field(lua_State *L, Struct const &value,
	                              LuaField<Struct, Member> const &field)
	{
		push_value(L, value.*(field.member));
		lua_setfield(L, -2, std::string(field.name).c_str());
	}

	template <class Struct, class Tuple, std::size_t... I>
	static void push_struct_fields(lua_State *L, Struct const &value,
	                               Tuple const &fields,
	                               std::index_sequence<I...>)
	{
		(push_struct_field(L, value, std::get<I>(fields)), ...);
	}

	template <class Struct>
	static void push_struct(lua_State *L, Struct const &value)
	{
		using Traits = lua_struct_traits<Struct>;
		using Fields = std::remove_reference_t<decltype(Traits::fields)>;

		lua_createtable(L, 0, static_cast<int>(std::tuple_size_v<Fields>));
		push_struct_fields(
		    L, value, Traits::fields,
		    std::make_index_sequence<std::tuple_size_v<Fields>>{});
	}

	template <class Seq>
	static void push_sequence(lua_State *L, Seq const &value)
	{
		lua_createtable(L, static_cast<int>(value.size()), 0);
		for (std::size_t i = 0; i < value.size(); ++i)
		{
			push_value(L, value[i]);
			lua_rawseti(L, -2, static_cast<lua_Integer>(i + 1));
		}
	}

	template <class Vec> static Vec read_vector(lua_State *L, int index)
	{
		if (!lua_istable(L, index))
			throw LuaError("expected Lua table");
		StackGuard guard(L);
		int abs = lua_absindex(L, index);
		size_t n = lua_rawlen(L, abs);
		Vec out;
		out.reserve(n);
		for (size_t i = 1; i <= n; ++i)
		{
			lua_rawgeti(L, abs, static_cast<lua_Integer>(i));
			out.push_back(read_value<typename Vec::value_type>(L, -1));
			lua_pop(L, 1);
		}
		return out;
	}

	lua_State *state_ = nullptr;
	std::vector<std::unique_ptr<FunctionBase>> functions_;
};

} // namespace util

#endif
