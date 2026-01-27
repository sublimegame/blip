//
//  lua_url.cpp
//  Cubzh
//
//  Created by Xavier Legland on 05/24/2022.
//  Copyright 2020 Voxowl Inc. All rights reserved.
//

#include "lua_url.hpp"

#include "web.hpp"
#include "config.h"

// Lua
#include "lua.hpp"
#include "lua_utils.hpp"
#include "lua_constants.h"
#include "lua_logs.hpp"
#include "lua_utils.hpp"
#include "lua_menu.hpp"
#include "scripting.hpp"

// vx
#include "GameCoordinator.hpp"
#include "VXApplication.hpp"

#define LUA_G_URL_FIELD_OPEN "Open" // read-only function

// --------------------------------------------------
//
// MARK: - Static functions' prototypes -
//
// --------------------------------------------------

static int _g_url_newindex(lua_State *const L);
static int _g_url_open(lua_State *const L);

// --------------------------------------------------
//
// MARK: - Exposed functions -
//
// --------------------------------------------------

bool lua_g_url_create_and_push(lua_State *L) {
    vx_assert(L != nullptr);

    LUAUTILS_STACK_SIZE(L)

    lua_newtable(L);
    lua_newtable(L);
    {
        lua_pushstring(L, "__index");
        lua_newtable(L); // table for read-only keys
        {
            lua_pushstring(L, LUA_G_URL_FIELD_OPEN);
            lua_pushcfunction(L, _g_url_open, "");
            lua_rawset(L, -3);
        }
        lua_rawset(L, -3);

        lua_pushstring(L, "__newindex");
        lua_pushcfunction(L, _g_url_newindex, "");
        lua_rawset(L, -3);

        lua_pushstring(L, "__metatable");
        lua_pushboolean(L, false);
        lua_settable(L, -3);

        lua_pushstring(L, P3S_LUA_OBJ_METAFIELD_TYPE);
        lua_pushinteger(L, ITEM_TYPE_G_URL);
        lua_settable(L, -3);
    }
    lua_setmetatable(L, -2);

    LUAUTILS_STACK_SIZE_ASSERT(L, 1);
    return true;
}

int lua_g_url_snprintf(lua_State *L, char *result, size_t maxLen,
                       bool spacePrefix) {
    LUAUTILS_STACK_SIZE(L)
    LUAUTILS_STACK_SIZE_ASSERT(L, 0)
    return snprintf(result, maxLen, "%s[URL]", spacePrefix ? " " : "");
}

// --------------------------------------------------
//
// MARK: - static functions
//
// --------------------------------------------------

static int _g_url_newindex(lua_State *L) {
    LUAUTILS_STACK_SIZE(L)
    LUAUTILS_ERROR(L, "URL is read-only"); // returns
    LUAUTILS_STACK_SIZE_ASSERT(L, 0)
    return 0;
}

static int _g_url_popupCount = 0;
static std::mutex _g_url_popupCountMutex;

static void _g_url_popupRetain() {
    std::lock_guard<std::mutex> lock(_g_url_popupCountMutex);
    _g_url_popupCount += 1;
}

// returns true if it's the last release
static bool _g_url_popupRelease() {
    std::lock_guard<std::mutex> lock(_g_url_popupCountMutex);
    _g_url_popupCount -= 1;
    if (_g_url_popupCount < 0) {
        _g_url_popupCount = 0;
    }
    const bool result = _g_url_popupCount == 0;
    return result;
}

static int _g_url_open(lua_State *L) {
    LUAUTILS_STACK_SIZE(L)

    const int argsCount = lua_gettop(L);
    
    if (argsCount < 1 || lua_utils_getObjectType(L, 1) != ITEM_TYPE_G_URL) {
        LUAUTILS_ERROR(L, "URL:Open - function should be called with `:`"); // returns
    }

    if (argsCount != 2) {
        LUAUTILS_ERROR(L, "URL:Open - 1 argument needed"); // returns
    }

    if (lua_isstring(L, 2) == false) {
        LUAUTILS_ERROR(L, "URL:Open - argument 1 should be a string"); // returns
    }

    std::string url(lua_tostring(L, 2));

    // add https prefix if there is none
    if (url.find("://") == std::string::npos) {
        url = "https://" + url;
    }
    
    const vx::URL u = vx::URL::make(url);
    if (u.isValid() == false) {
        LUAUTILS_ERROR(L, "URL:Open - url is not valid"); // returns
    }
    
    if (u.host() == "app.cu.bzh") {
        vx::VXApplication::getInstance()->openURL(url);
        LUAUTILS_STACK_SIZE_ASSERT(L, 0)
        return 0;
    }

    CGame *g = nullptr;
    if (lua_utils_getGamePtr(L, &g) == false) {
        LUAUTILS_INTERNAL_ERROR(L)
    }

    // call Lua module function menu:openURLWarning
    MENU_OPEN_URL_WITH_WARN(L, url.c_str())

    LUAUTILS_STACK_SIZE_ASSERT(L, 0)
    return 0;
}
