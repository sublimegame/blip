//
//  lua_environment.cpp
//  Cubzh
//
//  Created by Corentin Cailleaud on 29/06/2022.
//  Copyright 2022 Voxowl Inc. All rights reserved.
//

#include "lua_environment.hpp"
#include "VXGame.hpp"

// Lua
#include "lua.hpp"
#include "lua_utils.hpp"

#include "lua_constants.h"

static int _g_environment_newindex(lua_State *const L);
static int _g_environment_index(lua_State *const L);

bool lua_g_environment_create_and_push(lua_State *L) {
    vx_assert(L != nullptr);

    LUAUTILS_STACK_SIZE(L)

    lua_newtable(L);
    lua_newtable(L);
    {
        lua_pushstring(L, "__index");
        lua_pushcfunction(L, _g_environment_index, "");
        lua_rawset(L, -3);

        lua_pushstring(L, "__newindex");
        lua_pushcfunction(L, _g_environment_newindex, "");
        lua_rawset(L, -3);

        lua_pushstring(L, "__metatable");
        lua_pushboolean(L, false);
        lua_settable(L, -3);

        lua_pushstring(L, P3S_LUA_OBJ_METAFIELD_TYPE);
        lua_pushinteger(L, ITEM_TYPE_G_ENVIRONMENT);
        lua_settable(L, -3);
    }
    lua_setmetatable(L, -2);

    LUAUTILS_STACK_SIZE_ASSERT(L, 1);
    return true;
}

static int _g_environment_index(lua_State *L) {
    vx_assert(L != nullptr);
    LUAUTILS_STACK_SIZE(L)

    vx::Game *g = nullptr;
    if (lua_utils_getGameCppWrapperPtr(L, &g) == false || g == nullptr) {
        LUAUTILS_INTERNAL_ERROR(L)
        // return 0;
    }

    std::string key = std::string(lua_tostring(L, 2));

    const std::unordered_map<std::string, std::string>& env = g->getEnvironment();
    auto it = env.find(key);
    if (it != env.end()) {
        lua_pushstring(L, it->second.c_str());
    } else {
        lua_pushnil(L);
    }

    LUAUTILS_STACK_SIZE_ASSERT(L, 1);
    return 1;
}

static int _g_environment_newindex(lua_State *L) {
    LUAUTILS_STACK_SIZE(L)
    LUAUTILS_ERROR(L, "Environment is read-only"); // returns
    LUAUTILS_STACK_SIZE_ASSERT(L, 0)
    return 0;
}

int lua_g_environment_snprintf(lua_State *L, char *result, size_t maxLen, bool spacePrefix) {
    LUAUTILS_STACK_SIZE(L)
    LUAUTILS_STACK_SIZE_ASSERT(L, 0)
    return snprintf(result, maxLen, "%s[Environment]", spacePrefix ? " " : "");
}
