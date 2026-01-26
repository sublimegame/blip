//
//  lua_utils.h
//  Cubzh
//
//  Created by Gaetan de Villele on 20/08/2019.
//  Copyright © 2019 Voxowl Inc. All rights reserved.
//

#pragma once

#include <cstdint>
#include <string>
#include "lua_constants.h"

// forward declarations
typedef struct lua_State lua_State;
typedef struct _CGame CGame;
namespace vx { class Game; }

bool luau_dostring(lua_State* L, const std::string& script, const std::string& chunkname);
bool luau_loadstring(lua_State* L, const std::string& script, const std::string& chunkname);
std::string luau_compile(const std::string& script);

///
bool lua_utils_setPointerInRegistry(lua_State *L, const char *key, void *ptr);

///
bool lua_utils_getPointerInRegistry(lua_State *L, const char *key, void **ptrRef);

///
bool lua_utils_setGamePtr(lua_State *L, CGame *g);

///
bool lua_utils_setGameCppWrapperPtr(lua_State *L, void *cppGame);

///
bool lua_utils_getGamePtr(lua_State *L, CGame **gRef);

///
bool lua_utils_getGameCppWrapperPtr(lua_State *L, vx::Game **cppGameRef);

bool lua_utils_get8BitsMask(lua_State *L, const int idx, uint8_t *mask);
bool lua_utils_getMask(lua_State *L, const int idx, uint16_t *mask, uint8_t bits);
void lua_utils_pushMaskAsTable(lua_State *L, const uint16_t mask, uint8_t bits);

void lua_utils_setSystemGlobalsInRegistry(lua_State *L, const int idx);
void lua_utils_pushSystemGlobalsFromRegistry(lua_State *L);

void lua_utils_setRootGlobalsInRegistry(lua_State *L, const int idx);
void lua_utils_pushRootGlobalsFromRegistry(lua_State *L);

// LUA_GET_METAFIELD_AND_RETURN_TYPE

int lua_utils_get_metafield_and_return_type(lua_State* L, int idx, const char* name, bool pushNilIfNotFound);
#define LUA_GET_METAFIELD_AND_RETURN_TYPE(L, idx, name) lua_utils_get_metafield_and_return_type(L, idx, name, false)
#define LUA_GET_METAFIELD_AND_RETURN_TYPE_PUSHING_NIL_IF_NOT_FOUND(L, idx, name) lua_utils_get_metafield_and_return_type(L, idx, name, true)

void lua_utils_get_metafield(lua_State* L, int idx, const char* name, bool pushNilIfNotFound);
#define LUA_GET_METAFIELD(L, idx, name) lua_utils_get_metafield(L, idx, name, false)
#define LUA_GET_METAFIELD_PUSHING_NIL_IF_NOT_FOUND(L, idx, name) lua_utils_get_metafield(L, idx, name, true)

// NOTE: lua_getglobal pushes `nil` when not found
int lua_get_global_and_return_type(lua_State* L, const char* name);
#define LUA_GET_GLOBAL_AND_RETURN_TYPE(L, name) lua_get_global_and_return_type(L, name)

// ----------------------
// TYPE CHECKS
// ----------------------

/// Returns the integer value stored at the "__type" key in the metatable of the
/// lua value at index <idx> in the Lua stack.
/// ITEM_TYPE_NONE is returned when the value cannot be found.
LUA_ITEM_TYPE lua_utils_getObjectType(lua_State *L, const int idx);

///
bool lua_utils_isStringStrict(lua_State *L, const int idx);

///
bool lua_utils_isNumberStrict(lua_State *L, const int idx);

///
bool lua_utils_isIntegerStrict(lua_State *L, const int idx);

///
bool lua_utils_isStringStrictStartingWithUppercase(lua_State *L, const int idx);

// ----------------------
// ARGS COUNT
// ----------------------

///
bool lua_utils_checkArgsCountOld(lua_State *L,
                                 const int expectedArgsCount,
                                 const char *functionName);

///
#define lua_utils_assertArgsCount(L, c) lua_utils_checkArgsCountOld(L, c, __func__)

// ----------------------
// ERRORS
// ----------------------

void luautils_error(lua_State *L, const char *msg);

// Raises a Lua error with message and line number
// lua_getinfo(L, "l", &d) -> "l" to get current line
#define LUAUTILS_ERROR(L, msg) luautils_error(L, msg);

// same as LUAUTILS_ERROR with formatted string
#define LUAUTILS_ERROR_F(L, format, ...) {\
char msg[255];\
snprintf(msg, 255, format, __VA_ARGS__); \
luautils_error(L, msg); \
}

// ----------------------
// STACK CHECKS
// ----------------------

#include "vxlog.h"

// Lua stack size debugging

#define LUAUTILS_INTERNAL_ERROR(L) { \
lua_pushfstring(L, "internal error: %sL%d", __FILE_NAME__, __LINE__); \
lua_error(L); \
}

#define LUAUTILS_RETURN_INTERNAL_ERROR(L) { \
lua_pushfstring(L, "internal error: %sL%d", __FILE_NAME__, __LINE__); \
lua_error(L); \
return 0; \
}

#define LUAUTILS_COMING_SOON(L, FEATURE) { \
lua_pushfstring(L, "%s not implemented, coming soon!", FEATURE); \
lua_error(L); \
}

#ifdef DEBUG

#define LUAUTILS_STACK_SIZE(L) const int __pc_lua_size = lua_gettop(L);
#define LUAUTILS_STACK_SIZE_ASSERT(L, delta) if (lua_gettop(L) != __pc_lua_size+delta) { \
vxlog(VX_LOG_SEVERITY_ERROR, "LUA_UTILS_STACK", __LINE__, "🌒🔥 Lua stack size problem");\
vxlog(VX_LOG_SEVERITY_ERROR, "LUA_UTILS_STACK", __LINE__, "     stack %d, expected %d", lua_gettop(L), __pc_lua_size+delta);\
vxlog(VX_LOG_SEVERITY_ERROR, "LUA_UTILS_STACK", __LINE__, "     %sL%d", __FILE_NAME__, __LINE__);\
}

#define LUAUTILS_ASSERT_ARGS_COUNT(L, C) if (!lua_utils_assertArgsCount(L, C)) { \
vxlog_error("lua assert: args count");\
vx_assert(false);\
}

#define LUAUTILS_ASSERT_ARG_TYPE(L, N, T) if (lua_utils_getObjectType(L, N) != T) { \
vxlog_error("lua assert: arg type");\
vx_assert(false);\
}

#else

#define LUAUTILS_STACK_SIZE(L)
#define LUAUTILS_STACK_SIZE_ASSERT(L, delta)

#define LUAUTILS_ASSERT_ARGS_COUNT(L, C)
#define LUAUTILS_ASSERT_ARG_TYPE(L, N, T)

#endif
