// -------------------------------------------------------------
//  Cubzh
//  lua_fog.cpp
//  Created by Arthur Cormerais on December 2, 2020.
// -------------------------------------------------------------

#include "lua_fog.hpp"

// Lua
#include "lua.hpp"
#include "lua_utils.hpp"
#include "lua_constants.h"
#include "sbs.hpp"
#include "lua_color.hpp"

// engine
#include "game.h"

// vx
#include "VXGameRenderer.hpp"
#include "VXConfig.hpp"

// MARK: - Fog field names -

#define LUA_G_FOG_FIELD_ON                  "On"
#define LUA_G_FOG_FIELD_LIGHT_ABSORPTION    "LightAbsorption"
#define LUA_G_FOG_FIELD_DISTANCE            "Distance"
#define LUA_G_FOG_FIELD_NEAR                "Near"
#define LUA_G_FOG_FIELD_FAR                 "Far"
#define LUA_G_FOG_FIELD_LAYERS              "Layers"
#define LUA_G_FOG_FIELD_COLOR               "Color"

// MARK: - static private prototypes -

static int _fog_g_mt_index(lua_State *L);

static int _fog_g_mt_newindex(lua_State *L);

// MARK: - static private functions -

static int _fog_g_mt_index(lua_State *L) {
    vx_assert(L != nullptr);
    LUAUTILS_STACK_SIZE(L)

    // validate arguments count
    if (lua_utils_assertArgsCount(L, 2) == false) {
        vxlog_error("Wrong argument count. Returning nil.");
        lua_pushnil(L);
        LUAUTILS_STACK_SIZE_ASSERT(L, 1)
        return 1;
    }

    // validate type of 1st argument
    if (lua_istable(L, 1) == false) {
        vxlog_error("Wrong 1st argument type. Returning nil.");
        lua_pushnil(L);
        LUAUTILS_STACK_SIZE_ASSERT(L, 1)
        return 1;
    }

    // validate type of 2nd argument
    if (lua_utils_isStringStrict(L, 2) == false) {
        vxlog_error("Wrong 2nd argument type. Returning nil.");
        lua_pushnil(L);
        LUAUTILS_STACK_SIZE_ASSERT(L, 1)
        return 1;
    }

    // retrieve Game from Lua state
    vx::Game *g = nullptr;
    if (lua_utils_getGameCppWrapperPtr(L, &g) == false || g == nullptr) {
        LUAUTILS_INTERNAL_ERROR(L)
        return 0;
    }

    vx::GameRendererState_SharedPtr grs = g->getGameRendererState();

    const char *key = lua_tostring(L, 2);

    if (strcmp(key, LUA_G_FOG_FIELD_ON) == 0) {
        lua_pushboolean(L, grs->fogToggled());
    } else if (strcmp(key, LUA_G_FOG_FIELD_DISTANCE) == 0) {
        lua_pushnumber(L, static_cast<lua_Number>(grs->getFogStart()));
    } else if (strcmp(key, LUA_G_FOG_FIELD_LIGHT_ABSORPTION) == 0) {
        lua_pushnumber(L, static_cast<lua_Number>(grs->getFogEmission()));
    } else if (strcmp(key, LUA_G_FOG_FIELD_NEAR) == 0) {
        lua_pushnumber(L, static_cast<lua_Number>(grs->getFogStart()));
    } else if (strcmp(key, LUA_G_FOG_FIELD_FAR) == 0) {
        lua_pushnumber(L, static_cast<lua_Number>(grs->getFogEnd()));
    } else if (strcmp(key, LUA_G_FOG_FIELD_LAYERS) == 0) {
        lua_utils_pushMaskAsTable(L, grs->getFogLayers(), CAMERA_LAYERS_MASK_API_BITS);
    } else if (strcmp(key, LUA_G_FOG_FIELD_COLOR) == 0) {
        lua_color_create_and_push_table(L, grs->fogColor.x, grs->fogColor.y, grs->fogColor.z, 1.0f, false, true);
    } else {
        lua_pushnil(L);
    }

    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
    return 1;
}

static int _fog_g_mt_newindex(lua_State *L) {
    vx_assert(L != nullptr);
    LUAUTILS_STACK_SIZE(L)

    // validate arguments count
    if (lua_utils_assertArgsCount(L, 3) == false) {
        vxlog_error("Wrong argument count. Returning nil.");
        lua_pushnil(L);
        LUAUTILS_STACK_SIZE_ASSERT(L, 1)
        return 1;
    }

    // validate type of 1st argument
    if (lua_istable(L, 1) == false) {
        vxlog_error("Wrong 1st argument type. Returning nil.");
        lua_pushnil(L);
        LUAUTILS_STACK_SIZE_ASSERT(L, 1)
        return 1;
    }
    if (lua_utils_getObjectType(L, 1) != ITEM_TYPE_G_FOG) {
        LUAUTILS_STACK_SIZE_ASSERT(L, 0)
        return false;
    }

    // validate type of 2nd argument
    if (lua_utils_isStringStrict(L, 2) == false) {
        vxlog_error("Wrong 2nd argument type. Returning nil.");
        lua_pushnil(L);
        LUAUTILS_STACK_SIZE_ASSERT(L, 1)
        return 1;
    }

    // retrieve Game from Lua state
    vx::Game *g = nullptr;
    if (lua_utils_getGameCppWrapperPtr(L, &g) == false || g == nullptr) {
        LUAUTILS_INTERNAL_ERROR(L)
        return 0;
    }

    vx::GameRendererState_SharedPtr grs = g->getGameRendererState();

    const char *key = lua_tostring(L, 2);

    if (strcmp(key, LUA_G_FOG_FIELD_ON) == 0) {
        if (lua_isboolean(L, 3)) {
            grs->toggleFog(lua_toboolean(L, 3));
        } else {
            LUAUTILS_ERROR(L, "Fog.On: incorrect argument type"); // returns
        }
    } else if (strcmp(key, LUA_G_FOG_FIELD_DISTANCE) == 0) {
        if (lua_isnumber(L, 3)) {
            lua_Number value = lua_tonumber(L, 3);
            float diff = grs->getFogEnd() - grs->getFogStart();
            grs->setFogStart(static_cast<float>(value));
            grs->setFogEnd(static_cast<float>(value) + diff);
        } else {
            LUAUTILS_ERROR(L, "Fog.Distance: incorrect argument type"); // returns
        }
    } else if (strcmp(key, LUA_G_FOG_FIELD_LIGHT_ABSORPTION) == 0) {
        if (lua_isnumber(L, 3)) {
            float value = static_cast<float>(lua_tonumber(L, 3));
            if (value > 1.0f || value < 0.0f) {
                LUAUTILS_ERROR(L, "Fog.LightAbsorption: the value should be between 0 and 1"); // returns
            }
            grs->setFogEmission(value);
        } else {
            LUAUTILS_ERROR(L, "Fog.LightAbsorption: incorrect argument type"); // returns
        }
    } else if (strcmp(key, LUA_G_FOG_FIELD_NEAR) == 0) {
        if (lua_isnumber(L, 3)) {
            grs->setFogStart(lua_tonumber(L, 3));
        } else {
            LUAUTILS_ERROR(L, "Fog.Near: incorrect argument type"); // returns
        }
    } else if (strcmp(key, LUA_G_FOG_FIELD_FAR) == 0) {
        if (lua_isnumber(L, 3)) {
            grs->setFogEnd(lua_tonumber(L, 3));
        } else {
            LUAUTILS_ERROR(L, "Fog.Far: incorrect argument type"); // returns
        }
    } else if (strcmp(key, LUA_G_FOG_FIELD_LAYERS) == 0) {
        uint16_t mask = 0;
        if (lua_utils_getMask(L, 3, &mask, CAMERA_LAYERS_MASK_API_BITS) == false) {
            LUAUTILS_ERROR_F(L, "Fog.Layers cannot be set (new value should be one or a table of integers between 1 and %d)", CAMERA_LAYERS_MASK_API_BITS)
        }
        grs->setFogLayers(mask);
        return true;
    } else if (strcmp(key, LUA_G_FOG_FIELD_COLOR) == 0) {
        if (lua_utils_getObjectType(L, 3) == ITEM_TYPE_COLOR) {
            RGBAColor color;
            lua_color_getRGBAL(L, 3, &color.r, &color.g, &color.b, nullptr, nullptr);
            float3_set(&grs->fogColor, color.r / 255.0f, color.g / 255.0f, color.b / 255.0f);
        } else {
            LUAUTILS_ERROR(L, "Fog.Color must be a Color");
        }
    }

    LUAUTILS_STACK_SIZE_ASSERT(L, 0)
    return 0;
}

// MARK: - public functions -

bool lua_g_fog_pushNewTable(lua_State *L) {
    vx_assert(L != nullptr);
    
    LUAUTILS_STACK_SIZE(L)
    // "Client.Fog" is defined only in the client sandbox
#ifdef CLIENT_ENV
    lua_newtable(L);
    lua_newtable(L); // metatable
    {
        lua_pushstring(L, "__index");
        lua_pushcfunction(L, _fog_g_mt_index, "");
        lua_settable(L, -3);

        lua_pushstring(L, "__newindex");
        lua_pushcfunction(L, _fog_g_mt_newindex, "");
        lua_settable(L, -3);

        // hide the metatable from the Lua sandbox user
        lua_pushstring(L, "__metatable");
        lua_pushboolean(L, false);
        lua_settable(L, -3);

        lua_pushstring(L, P3S_LUA_OBJ_METAFIELD_TYPE); // used to log tables
        lua_pushinteger(L, ITEM_TYPE_G_FOG);
        lua_settable(L, -3);
    }
    lua_setmetatable(L, -2);
#else
    lua_pushnil(L);
#endif
    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
    return true;
}

int lua_g_fog_snprintf(lua_State *L, char *result, size_t maxLen, bool spacePrefix) {
    vx_assert(L != nullptr);
    LUAUTILS_STACK_SIZE(L)
    LUAUTILS_STACK_SIZE_ASSERT(L, 0)
    return snprintf(result, maxLen, "%s[Fog]", spacePrefix ? " " : "");
}
