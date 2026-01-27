// -------------------------------------------------------------
//  Cubzh
//  lua_clouds.cpp
//  Created by Arthur Cormerais on December 2, 2020.
// -------------------------------------------------------------

#include "lua_clouds.hpp"

// Lua
#include "lua.hpp"
#include "lua_utils.hpp"
#include "lua_constants.h"
#include "sbs.hpp"
#include "lua_number3.hpp"
#include "lua_color.hpp"

// engine
#include "game.h"

// vx
#include "VXGameRenderer.hpp"

// MARK: - Clouds field names -

#define LUA_G_CLOUDS_FIELD_ON           "On"        // boolean
#define LUA_G_CLOUDS_FIELD_MIN          "Min"       // Number3
#define LUA_G_CLOUDS_FIELD_ALTITUDE     "Altitude"  // number
#define LUA_G_CLOUDS_FIELD_MAX          "Max"       // Number3
#define LUA_G_CLOUDS_FIELD_SIZE         "Size"      // Number3
#define LUA_G_CLOUDS_FIELD_WIDTH        "Width"     // number
#define LUA_G_CLOUDS_FIELD_DEPTH        "Depth"     // number
#define LUA_G_CLOUDS_FIELD_UNLIT        "IsUnlit"   // boolean
#define LUA_G_CLOUDS_FIELD_COLOR        "Color"     // Color

// MARK: undocumented fields

#define LUA_G_CLOUDS_FIELD_MINSCALE        "PrivateMinScale"   // number
#define LUA_G_CLOUDS_FIELD_MAXSCALE        "PrivateMaxScale"   // number
#define LUA_G_CLOUDS_FIELD_SPREAD          "PrivateSpread"     // number
#define LUA_G_CLOUDS_FIELD_SPEED           "PrivateSpeed"      // number

// MARK: - static private prototypes -

static int _clouds_g_mt_index(lua_State *L);
static int _clouds_g_mt_newindex(lua_State *L);

// MARK: - static private functions -

static int _clouds_g_mt_index(lua_State *L) {
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
        // return 0;
    }

    vx::GameRendererState_SharedPtr grs = g->getGameRendererState();

    const char *key = lua_tostring(L, 2);

    if (strcmp(key, LUA_G_CLOUDS_FIELD_ON) == 0) {
        lua_pushboolean(L, grs->cloudsToggled());
    } else if (strcmp(key, LUA_G_CLOUDS_FIELD_MIN) == 0) {
        const float3 min = grs->getCloudsOrigin();
        lua_number3_pushNewObject(L, min.x, min.y, min.z);
    } else if (strcmp(key, LUA_G_CLOUDS_FIELD_MAX) == 0) {
        const float3 min = grs->getCloudsOrigin();
        lua_number3_pushNewObject(L, min.x + grs->getCloudsWidth(), min.y, min.z + grs->getCloudsDepth());
    } else if (strcmp(key, LUA_G_CLOUDS_FIELD_ALTITUDE) == 0) {
        const float3 min = grs->getCloudsOrigin();
        lua_pushnumber(L, static_cast<double>(min.y));
    } else if (strcmp(key, LUA_G_CLOUDS_FIELD_SIZE) == 0) {
        lua_number3_pushNewObject(L, grs->getCloudsWidth(), 0.0f, grs->getCloudsDepth());
    } else if (strcmp(key, LUA_G_CLOUDS_FIELD_WIDTH) == 0) {
        lua_pushnumber(L, static_cast<double>(grs->getCloudsWidth()));
    } else if (strcmp(key, LUA_G_CLOUDS_FIELD_DEPTH) == 0) {
        lua_pushnumber(L, static_cast<double>(grs->getCloudsDepth()));
    } else if (strcmp(key, LUA_G_CLOUDS_FIELD_UNLIT) == 0) {
        lua_pushboolean(L, grs->getCloudsUnlit());
    } else if (strcmp(key, LUA_G_CLOUDS_FIELD_COLOR) == 0) {
        const float3 rgb = grs->getCloudsColor();
        lua_color_create_and_push_table(L, rgb.x, rgb.y, rgb.z, 1.0f, false, true);
    } else if (strcmp(key, LUA_G_CLOUDS_FIELD_MINSCALE) == 0) {
        lua_pushnumber(L, static_cast<double>(grs->getCloudsBaseScale()));
    } else if (strcmp(key, LUA_G_CLOUDS_FIELD_MAXSCALE) == 0) {
        lua_pushnumber(L, static_cast<double>(grs->getCloudsAddScale()));
    } else if (strcmp(key, LUA_G_CLOUDS_FIELD_SPREAD) == 0) {
        lua_pushnumber(L, static_cast<double>(grs->getCloudsSpread()));
    } else if (strcmp(key, LUA_G_CLOUDS_FIELD_SPEED) == 0) {
        lua_pushnumber(L, static_cast<double>(grs->getCloudsSpeed()));
    }

    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
    return 1;
}

static int _clouds_g_mt_newindex(lua_State *L) {
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
    if (lua_utils_getObjectType(L, 1) != ITEM_TYPE_G_CLOUDS) {
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
        // return 0;
    }

    vx::GameRendererState_SharedPtr grs = g->getGameRendererState();

    const char *key = lua_tostring(L, 2);

    if (strcmp(key, LUA_G_CLOUDS_FIELD_ON) == 0) {
        if (lua_isboolean(L, 3)) {
            grs->toggleClouds(lua_toboolean(L, 3));
        } else {
            LUAUTILS_ERROR(L, "Clouds.On should be a boolean"); // returns
        }
    } else if (strcmp(key, LUA_G_CLOUDS_FIELD_MIN) == 0) {
        float3 min;
        if (lua_number3_or_table_getXYZ(L, 3, &min.x, &min.y, &min.z)) {
            grs->setCloudsOrigin(min);
        } else {
            LUAUTILS_ERROR(L, "Clouds.Min should be a Number3"); // returns
        }
    } else if (strcmp(key, LUA_G_CLOUDS_FIELD_MAX) == 0) {
        float3 max;
        if (lua_number3_or_table_getXYZ(L, 3, &max.x, &max.y, &max.z)) {
            const float3 min = grs->getCloudsOrigin();
            grs->setCloudsWidth(max.x - min.x);
            grs->setCloudsDepth(max.z - min.z);
        } else {
            LUAUTILS_ERROR(L, "Clouds.Max should be a Number3"); // returns
        }
    } else if (strcmp(key, LUA_G_CLOUDS_FIELD_ALTITUDE) == 0) {
        if (lua_isnumber(L, 3)) {
            float3 origin = grs->getCloudsOrigin();
            origin.y = lua_tonumber(L, 3);
            grs->setCloudsOrigin(origin);
        } else {
            LUAUTILS_ERROR(L, "Clouds.Altitude should be a number"); // returns
        }
    } else if (strcmp(key, LUA_G_CLOUDS_FIELD_SIZE) == 0) {
        float width, depth;
        if (lua_isnumber(L, 3)) {
            width = lua_tonumber(L, 3);
            depth = width;
        } else if (lua_number3_or_table_getXYZ(L, 3, &width, nullptr, &depth) == false) {
            LUAUTILS_ERROR(L, "Clouds.Size must be a Number3 or number"); // returns
        }
        grs->setCloudsWidth(width);
        grs->setCloudsDepth(depth);
    } else if (strcmp(key, LUA_G_CLOUDS_FIELD_WIDTH) == 0) {
        if (lua_isnumber(L, 3)) {
            grs->setCloudsWidth(lua_tonumber(L, 3));
        } else {
            LUAUTILS_ERROR(L, "Clouds.Width should be a number"); // returns
        }
    } else if (strcmp(key, LUA_G_CLOUDS_FIELD_DEPTH) == 0) {
        if (lua_isnumber(L, 3)) {
            grs->setCloudsDepth(lua_tonumber(L, 3));
        } else {
            LUAUTILS_ERROR(L, "Clouds.Depth should be a number"); // returns
        }
    } else if (strcmp(key, LUA_G_CLOUDS_FIELD_UNLIT) == 0) {
        if (lua_isboolean(L, 3)) {
            grs->setCloudsUnlit(lua_toboolean(L, 3));
        } else {
            LUAUTILS_ERROR(L, "Clouds.IsUnlit should be a boolean");
        }
    } else if (strcmp(key, LUA_G_CLOUDS_FIELD_COLOR) == 0) {
        float3 rgb;
        if (lua_color_getRGBAL_float(L, 3, &rgb.x, &rgb.y, &rgb.z, nullptr, nullptr) ||
            lua_number3_or_table_getXYZ(L, 3, &rgb.x, &rgb.y, &rgb.z)) {

            grs->setCloudsColor(rgb);
        } else {
            LUAUTILS_ERROR(L, "Clouds.Color should be a Color, a Number3, or a table of numbers");
        }
    } else if (strcmp(key, LUA_G_CLOUDS_FIELD_MINSCALE) == 0) {
        if (lua_isnumber(L, 3)) {
            grs->setCloudsBaseScale(lua_tonumber(L, 3));
        } else {
            LUAUTILS_ERROR(L, "Clouds.PrivateBaseScale should be a number"); // returns
        }
    } else if (strcmp(key, LUA_G_CLOUDS_FIELD_MAXSCALE) == 0) {
        if (lua_isnumber(L, 3)) {
            grs->setCloudsAddScale(lua_tonumber(L, 3));
        } else {
            LUAUTILS_ERROR(L, "Clouds.PrivateMaxAddScale should be a number"); // returns
        }
    } else if (strcmp(key, LUA_G_CLOUDS_FIELD_SPREAD) == 0) {
        if (lua_isnumber(L, 3)) {
            grs->setCloudsSpread(lua_tonumber(L, 3));
        } else {
            LUAUTILS_ERROR(L, "Clouds.PrivateSpread should be a number"); // returns
        }
    } else if (strcmp(key, LUA_G_CLOUDS_FIELD_SPEED) == 0) {
        if (lua_isnumber(L, 3)) {
            grs->setCloudsSpeed(lua_tonumber(L, 3));
        } else {
            LUAUTILS_ERROR(L, "Clouds.PrivateSpeed should be a number"); // returns
        }
    }

    LUAUTILS_STACK_SIZE_ASSERT(L, 0)
    return 0;
}

// MARK: - public functions -

bool lua_g_clouds_pushNewTable(lua_State *L) {
    vx_assert(L != nullptr);
    
    LUAUTILS_STACK_SIZE(L)
    // "Client.Clouds" is defined only in the client sandbox
#ifdef CLIENT_ENV
    lua_newtable(L);
    lua_newtable(L); // metatable
    {
        lua_pushstring(L, "__index");
        lua_pushcfunction(L, _clouds_g_mt_index, "");
        lua_settable(L, -3);

        lua_pushstring(L, "__newindex");
        lua_pushcfunction(L, _clouds_g_mt_newindex, "");
        lua_settable(L, -3);

        // hide the metatable from the Lua sandbox user
        lua_pushstring(L, "__metatable");
        lua_pushboolean(L, false);
        lua_settable(L, -3);

        lua_pushstring(L, P3S_LUA_OBJ_METAFIELD_TYPE); // used to log tables
        lua_pushinteger(L, ITEM_TYPE_G_CLOUDS);
        lua_settable(L, -3);
    }
    lua_setmetatable(L, -2);
#else
    lua_pushnil(L);
#endif
    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
    return true;
}

int lua_g_clouds_snprintf(lua_State *L, char *result, size_t maxLen, bool spacePrefix) {
    vx_assert(L != nullptr);
    LUAUTILS_STACK_SIZE(L)
    LUAUTILS_STACK_SIZE_ASSERT(L, 0)
    return snprintf(result, maxLen, "%s[Clouds]", spacePrefix ? " " : "");
}
