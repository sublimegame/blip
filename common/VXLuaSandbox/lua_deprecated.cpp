// -------------------------------------------------------------
//  Cubzh
//  lua_deprecated.cpp
//  Created by Arthur Cormerais on September 21, 2023.
// -------------------------------------------------------------

#include "lua_deprecated.hpp"

// Lua
#include "lua.hpp"
#include "lua_logs.hpp"
#include "lua_utils.hpp"
#include "lua_constants.h"

// vx
#include "GameCoordinator.hpp"
#include "process.hpp"
#include "VXApplication.hpp"
#include "VXGame.hpp"
#include "VXGameRenderer.hpp"
#include "VXHubClient.hpp"
#include "VXPrefs.hpp"

// xptools
#include "vxlog.h"

#define LUA_DEPRECATED_FIELD_UI_CROSSHAIR "Crosshair" // boolean

static int _deprecated_metatable_index(lua_State *L);
static int _deprecated_metatable_newindex(lua_State *L);

static void _deprecated_set_crosshair(lua_State *L);

void lua_deprecated_create_and_push(lua_State *L) {
    LUAUTILS_STACK_SIZE(L)
    lua_newtable(L);
    {
        lua_newtable(L); // metatable

        lua_pushstring(L, "__index");
        lua_pushcfunction(L, _deprecated_metatable_index, "");
        lua_rawset(L, -3);

        lua_pushstring(L, "__newindex");
        lua_pushcfunction(L, _deprecated_metatable_newindex, "");
        lua_rawset(L, -3);

        lua_pushstring(L, "__metatable");
        lua_pushboolean(L, false);
        lua_rawset(L, -3);

        lua_pushstring(L, P3S_LUA_OBJ_METAFIELD_TYPE); // used to log tables
        lua_pushinteger(L, ITEM_TYPE_DEPRECATED);
        lua_rawset(L, -3);

        lua_setmetatable(L, -2);
    }
    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
}

static int _deprecated_metatable_index(lua_State *L) {
    vx_assert(L != nullptr);
    LUAUTILS_ASSERT_ARG_TYPE(L, 1, ITEM_TYPE_DEPRECATED)
    LUAUTILS_STACK_SIZE(L)

    lua_pushnil(L);
    
    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
    return 1;
}

static int _deprecated_metatable_newindex(lua_State *L) {
    vx_assert(L != nullptr);
    LUAUTILS_ASSERT_ARG_TYPE(L, 1, ITEM_TYPE_DEPRECATED)
    LUAUTILS_STACK_SIZE(L)

    if (lua_utils_isStringStrictStartingWithUppercase(L, 2) == false) {
        LUAUTILS_STACK_SIZE_ASSERT(L, 0)
        return 0;
    }

    const char *key = lua_tostring(L, 2);

    if (strcmp(key, LUA_DEPRECATED_FIELD_UI_CROSSHAIR) == 0) {
        _deprecated_set_crosshair(L);
    } else {
        vxlog_error("Deprecated.%s is not settable.", key);
    }

    LUAUTILS_STACK_SIZE_ASSERT(L, 0)
    return 0;
}

static void _deprecated_set_crosshair(lua_State *L) {
    vx_assert(L != nullptr);
    LUAUTILS_STACK_SIZE(L)

    if (lua_isboolean(L, 3) == false) {
        LUAUTILS_ERROR(L, "Deprecated.Crosshair cannot be set (argument is not a boolean)");
    }

    std::string s;

    if (lua_toboolean(L, 3)) {
        s =  R"""(
            if __crosshairModule == nil then __crosshairModule = require("crosshair") end

            __crosshairModule:show()
        )""";
    } else {
        s = R"""(
            if __crosshairModule ~= nil then
                __crosshairModule:hide()
            end
        )""";
    }

    if (luau_loadstring(L, s, "set crosshair") == false) {
        std::string err = lua_tostring(L, -1);
        // lua_log_error_with_game(g, err);
        lua_log_error_CStr(L, err.c_str());
        lua_pop(L, 1);
        return;
    }

    if (lua_pcall(L, 0, LUA_MULTRET, 0) != 0) {
        std::string err = "";
        if (lua_isstring(L, -1)) {
            err = lua_tostring(L, -1);
        } else {
            err = "unspecified error";
        }
        lua_log_error_CStr(L, err.c_str());
        lua_pop(L, 1);
    }

    LUAUTILS_STACK_SIZE_ASSERT(L, 0)
}
