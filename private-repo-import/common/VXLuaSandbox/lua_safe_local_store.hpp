//
//  lua_safe_local_store.hpp
//  Cubzh
//
//  Created by Adrien Duermael on 09/14/2024.
//

// SafeLocalStore is defined as a Lua module.
// This is just a set of defines to easily use it in C++.

#pragma once

#include "lua_logs.hpp"
#include "lua_system.hpp"
#include "lua_require.hpp"

#define SAFE_LOCAL_STORE_SET(L, valueIndex, index, key, success) \
lua_require(L, "safe_local_store"); \
lua_getfield(L, -1, "set"); \
lua_pushvalue(L, -2); \
lua_remove(L, -3); \
{\
int vIndex = valueIndex; \
if (vIndex < 0) { vIndex -= 1; } \
lua_pushvalue(L, vIndex); \
}\
if (lua_pcall(L, 2, 2, 0) != LUA_OK) { \
    lua_pop(L, 1); \
    success = false; \
} else {\
success = true;\
index = static_cast<int>(lua_tointeger(L, -2));\
key = std::string(lua_tostring(L, -1));\
lua_pop(L, 2); \
}

// pushes value on top of the stack
#define SAFE_LOCAL_STORE_GET(L, index, key, success) \
lua_require(L, "safe_local_store"); \
lua_getfield(L, -1, "get"); \
lua_pushvalue(L, -2); \
lua_remove(L, -3); \
lua_pushinteger(L, index); \
lua_pushstring(L, key.c_str()); \
if (lua_pcall(L, 3, 1, 0) != LUA_OK) { \
    lua_pop(L, 1); \
    success = false; \
} else {\
success = true;\
}

#define SAFE_LOCAL_STORE_REMOVE(L, index, key, success) \
lua_require(L, "safe_local_store"); \
lua_getfield(L, -1, "get"); \
lua_pushvalue(L, -2); \
lua_remove(L, -3); \
lua_pushinteger(L, index); \
lua_pushstring(L, key.c_str()); \
if (lua_pcall(L, 3, 0, 0) != LUA_OK) { \
    lua_pop(L, 1); \
    success = false; \
} else {\
success = true;\
}
