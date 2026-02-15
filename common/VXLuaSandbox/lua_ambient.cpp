// -------------------------------------------------------------
//  Cubzh
//  lua_ambient.cpp
//  Created by Arthur Cormerais on April 5, 2023.
// -------------------------------------------------------------

#include "lua_ambient.hpp"

#include <string.h>

#include "config.h"
#include "lua_utils.hpp"
#include "lua_constants.h"
#include "lua_color.hpp"
#include "VXGameRenderer.hpp"

#define LUA_G_AMBIENT_FIELD_INTENSITY "Intensity" // number
#define LUA_G_AMBIENT_FIELD_DIRECTIONAL "DirectionalLightFactor" // number
#define LUA_G_AMBIENT_FIELD_SKY "SkyLightFactor" // number
#define LUA_G_AMBIENT_FIELD_COLOR "Color" // Color

static int _g_ambient_metatable_index(lua_State *L);
static int _g_ambient_metatable_newindex(lua_State *L);

void lua_g_ambient_createAndPush(lua_State * const L) {
    vx_assert(L != nullptr);
    LUAUTILS_STACK_SIZE(L)

    lua_newtable(L); // global "Ambient" table
    lua_newtable(L); // metatable
    {
        lua_pushstring(L, "__index");
        lua_pushcfunction(L, _g_ambient_metatable_index, "");
        lua_rawset(L, -3);

        lua_pushstring(L, "__newindex");
        lua_pushcfunction(L, _g_ambient_metatable_newindex, "");
        lua_rawset(L, -3);

        // hide the metatable from the Lua sandbox user
        lua_pushstring(L, "__metatable");
        lua_pushboolean(L, false);
        lua_rawset(L, -3);
    }
    lua_setmetatable(L, -2);

    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
}

static int _g_ambient_metatable_index(lua_State *L) {
    vx_assert(L != nullptr);
    LUAUTILS_STACK_SIZE(L)

    vx::Game *cppGame = nullptr;
    if (lua_utils_getGameCppWrapperPtr(L, &cppGame) == false || cppGame == nullptr) {
        LUAUTILS_INTERNAL_ERROR(L)
    }
    vx_assert(cppGame != nullptr);
    
    vx::GameRendererState_SharedPtr grs = cppGame->getGameRendererState();

    const char *key = lua_tostring(L, 2);
    if (strcmp(key, LUA_G_AMBIENT_FIELD_INTENSITY) == 0) {
        lua_pushnumber(L, static_cast<double>(grs->bakedIntensity));
    } else if (strcmp(key, LUA_G_AMBIENT_FIELD_DIRECTIONAL) == 0) {
        lua_pushnumber(L, static_cast<double>(grs->lightsAmbientFactor));
    } else if (strcmp(key, LUA_G_AMBIENT_FIELD_SKY) == 0) {
        lua_pushnumber(L, static_cast<double>(grs->skyboxAmbientFactor));
    } else if (strcmp(key, LUA_G_AMBIENT_FIELD_COLOR) == 0) {
        if (grs->ambientColorOverride.x >= 0) {
            lua_color_create_and_push_table(L, grs->ambientColorOverride.x, grs->ambientColorOverride.y, grs->ambientColorOverride.z, 1.0f, false, true);
        } else {
            lua_pushnil(L);
        }
    } else {
        lua_pushnil(L);
    }

    LUAUTILS_STACK_SIZE_ASSERT(L, 1);
    return 1;
}

static int _g_ambient_metatable_newindex(lua_State *L) {
    vx_assert(L != nullptr);
    LUAUTILS_STACK_SIZE(L)

    vx::Game *cppGame = nullptr;
    if (lua_utils_getGameCppWrapperPtr(L, &cppGame) == false || cppGame == nullptr) {
        LUAUTILS_INTERNAL_ERROR(L)
    }
    vx_assert(cppGame != nullptr);
    
    vx::GameRendererState_SharedPtr grs = cppGame->getGameRendererState();

    const char *key = lua_tostring(L, 2);
    if (strcmp(key, LUA_G_AMBIENT_FIELD_INTENSITY) == 0) {
        if (lua_isnumber(L, 3)) {
            grs->bakedIntensity = lua_tonumber(L, 3);
        } else {
            LUAUTILS_ERROR(L, "Ambient.Intensity should be a number");
        }
    } else if (strcmp(key, LUA_G_AMBIENT_FIELD_DIRECTIONAL) == 0) {
        if (lua_isnumber(L, 3)) {
            grs->lightsAmbientFactor = lua_tonumber(L, 3);
        } else {
            LUAUTILS_ERROR(L, "Ambient.DirectionalLightFactor should be a number");
        }
    } else if (strcmp(key, LUA_G_AMBIENT_FIELD_SKY) == 0) {
        if (lua_isnumber(L, 3)) {
            grs->skyboxAmbientFactor = lua_tonumber(L, 3);
        } else {
            LUAUTILS_ERROR(L, "Ambient.SkyLightFactor should be a number");
        }
    } else if (strcmp(key, LUA_G_AMBIENT_FIELD_COLOR) == 0) {
        if (lua_utils_getObjectType(L, 3) == ITEM_TYPE_COLOR) {
            RGBAColor color;
            lua_color_getRGBAL(L, 3, &color.r, &color.g, &color.b, nullptr, nullptr);
            float3_set(&grs->ambientColorOverride, color.r / 255.0f, color.g / 255.0f, color.b / 255.0f);
        } else if (lua_isnil(L, 3)) {
            float3_set(&grs->ambientColorOverride, -1.0f, -1.0f, -1.0f);
        } else {
            LUAUTILS_ERROR(L, "Ambient.Color must be a Color or nil");
        }
    }

    LUAUTILS_STACK_SIZE_ASSERT(L, 0);
    return 0;
}
