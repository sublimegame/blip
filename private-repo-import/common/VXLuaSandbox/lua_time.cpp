//
//  lua_time.cpp
//  Cubzh
//
//  Created by Adrien Duermael on 7/10/20.
//

#include "lua_time.hpp"

// Lua

#include "lua.hpp"
#include "lua_utils.hpp"
#include "lua_logs.hpp"

// xptools
#include "xptools.h"

#include "config.h"
#include <cstring>

// sandbox
 #include "sbs.hpp"

#define LUA_G_TIME_FIELD_UNIX "Unix" // seconds since January 1, 1970 UTC
#define LUA_G_TIME_FIELD_UNIXMILLI "UnixMilli" // milliseconds since January 1, 1970 UTC

/// __index for global Time
static int _g_time_metatable_index(lua_State *L);
static int _g_time_metatable_call(lua_State *L);
static int _g_time_metatable_newindex(lua_State *L);
static int _g_time_push_unix(lua_State *L);
static int _g_time_push_unixmilli(lua_State *L);

void lua_g_time_create_and_push(lua_State *L) {
    vx_assert(L != nullptr);
    LUAUTILS_STACK_SIZE(L)
    
    lua_newtable(L);
    lua_newtable(L); // metatable
    {
        lua_pushstring(L, "__index");
        lua_pushcfunction(L, _g_time_metatable_index, "");
        lua_settable(L, -3);
        
        lua_pushstring(L, "__newindex");
        lua_pushcfunction(L, _g_time_metatable_newindex, "");
        lua_settable(L, -3);
        
        lua_pushstring(L, "__call");
        lua_pushcfunction(L, _g_time_metatable_call, "");
        lua_settable(L, -3);
        
        lua_pushstring(L, "__metatable");
        lua_pushboolean(L, false);
        lua_settable(L, -3);
        
        lua_pushstring(L, P3S_LUA_OBJ_METAFIELD_TYPE); // used to log tables
        lua_pushinteger(L, ITEM_TYPE_G_TIME);
        lua_settable(L, -3);
    }
    lua_setmetatable(L, -2);
    
    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
}

/// arguments
/// - lua table : global Time table
/// - lua string : the key that is being accessed
static int _g_time_metatable_index(lua_State *L) {
    vx_assert(L != nullptr);
    LUAUTILS_ASSERT_ARGS_COUNT(L, 2)
    LUAUTILS_ASSERT_ARG_TYPE(L, 1, ITEM_TYPE_G_TIME)
    LUAUTILS_STACK_SIZE(L)
    
    const char *key = lua_tostring(L, 2);
    LUAUTILS_STACK_SIZE_ASSERT(L, 0)
    
    if (strcmp(key, LUA_G_TIME_FIELD_UNIX) == 0) {
        lua_pushcfunction(L, _g_time_push_unix, "");
    } else if (strcmp(key, LUA_G_TIME_FIELD_UNIXMILLI) == 0) {
        lua_pushcfunction(L, _g_time_push_unixmilli, "");
    } else if (strcmp(key, "Dawn") == 0 ||
               strcmp(key, "Noon") == 0 ||
               strcmp(key, "Dusk") == 0 ||
               strcmp(key, "Midnight") == 0) {
        lua_log_warning(L, "Time."+std::string(key)+" does not exist anymore");
        lua_pushnil(L);
    } else {
        // if key is unknown, return nil
        lua_pushnil(L);
    }

    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
    return 1; // the __index function always has 1 return value
}

static int _g_time_metatable_call(lua_State *L) {
    lua_log_warning(L, "Time class can't be called");
    return 0;
}

static int _g_time_metatable_newindex(lua_State *L) {
    lua_log_warning(L, "Time class is read-only");
    return 0;
}

int lua_g_time_snprintf(lua_State *L,
                        char* result,
                        size_t maxLen,
                        bool spacePrefix) {
    return snprintf(result, maxLen, "%s[Time (global)]", spacePrefix ? " " : "");
}

#include <chrono>

static int _g_time_push_unix(lua_State *L) {
    vx_assert(L != nullptr);
    LUAUTILS_STACK_SIZE(L)
    
    using namespace std::chrono;
    uint64_t ms = duration_cast<seconds>(system_clock::now().time_since_epoch()).count();
    lua_pushnumber(L, static_cast<double>(ms));

    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
    return 1;
}

static int _g_time_push_unixmilli(lua_State *L) {
    vx_assert(L != nullptr);
    LUAUTILS_STACK_SIZE(L)
    
    using namespace std::chrono;
    uint64_t ms = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
    lua_pushnumber(L, static_cast<double>(ms));
    
    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
    return 1;
}
