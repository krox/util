#include "util/lua.h"

#ifdef UTIL_LUA

extern "C"
{
#include <lualib.h>
}

#include <string>

namespace util {

Lua::Lua()
{
	state_ = luaL_newstate();
	if (state_ == nullptr)
		throw LuaError("failed to create lua_State");
	luaL_openlibs(state_);
}

Lua::Lua(Lua &&other) noexcept
    : state_(std::exchange(other.state_, nullptr)),
      functions_(std::move(other.functions_))
{}

Lua &Lua::operator=(Lua &&other) noexcept
{
	if (this == &other)
		return *this;
	close();
	state_ = std::exchange(other.state_, nullptr);
	functions_ = std::move(other.functions_);
	return *this;
}

Lua::~Lua() { close(); }

void Lua::close() noexcept
{
	if (state_ == nullptr)
		return;
	lua_close(state_);
	state_ = nullptr;
	functions_.clear();
}

Lua::operator bool() const noexcept { return state_ != nullptr; }

void Lua::run(std::string_view source, std::string_view chunk_name)
{
	if (state_ == nullptr)
		throw LuaError("lua state is closed");

	int ec = luaL_loadbuffer(state_, source.data(), source.size(),
	                         std::string(chunk_name).c_str());
	if (ec != LUA_OK)
	{
		char const *s = lua_tostring(state_, -1);
		std::string msg = s ? s : "Lua load error";
		lua_pop(state_, 1);
		throw LuaError(msg);
	}

	ec = lua_pcall(state_, 0, LUA_MULTRET, 0);
	if (ec != LUA_OK)
	{
		char const *s = lua_tostring(state_, -1);
		std::string msg = s ? s : "Lua runtime error";
		lua_pop(state_, 1);
		throw LuaError(msg);
	}
}

int64_t Lua::read_integer(lua_State *L, int index)
{
	if (!lua_isinteger(L, index))
		throw LuaError("expected Lua integer");
	return static_cast<int64_t>(lua_tointeger(L, index));
}

double Lua::read_floating(lua_State *L, int index)
{
	if (!lua_isnumber(L, index))
		throw LuaError("expected Lua number");
	return static_cast<double>(lua_tonumber(L, index));
}

bool Lua::read_boolean(lua_State *L, int index)
{
	if (!lua_isboolean(L, index))
		throw LuaError("expected Lua boolean");
	return lua_toboolean(L, index) != 0;
}

std::string Lua::read_string(lua_State *L, int index)
{
	if (!lua_isstring(L, index))
		throw LuaError("expected Lua string");
	size_t len = 0;
	char const *s = lua_tolstring(L, index, &len);
	if (s == nullptr)
		throw LuaError("expected Lua string");
	return std::string(s, len);
}

} // namespace util

#endif