//
//  lua_map.cpp
//
//  Created by Gaetan de Villele on 26/08/2019.
//  Copyright © 2019 Voxowl Inc. All rights reserved.
//

#include "lua_map.hpp"

// C++
#include <cmath>
#include <cstring>

// Lua
#include "lua.hpp"

// engine
#include "scene.h"
#include "serialization.h"

// sandbox
#include "lua_number3.hpp"
#include "lua_shape.hpp"
#include "lua_mutableShape.hpp"
#include "lua_items.hpp"
#include "lua_utils.hpp"
#include "lua_constants.h"
#include "lua_type.hpp"
#include "lua_block.hpp"
#include "lua_object.hpp"
#include "lua_palette.hpp"

// xptools
#include "xptools.h"

// #define LUA_MAP_FIELD_LOCAL_PALETTE  "LocalPalette" // DEPRECATED (use "Palette" instead)

// Blocked properties
#define LUA_MAP_FIELD_BLOCKED_PHYSICS           "Physics"
#define LUA_MAP_FIELD_BLOCKED_VELOCITY          "Velocity"
#define LUA_MAP_FIELD_BLOCKED_MOTION            "Motion"
#define LUA_MAP_FIELD_BLOCKED_ACCELERATION      "Acceleration"
#define LUA_MAP_FIELD_BLOCKED_ISONGROUND        "IsOnGround"
#define LUA_MAP_FIELD_BLOCKED_COLLISIONBOX      "CollisionBox"
#define LUA_MAP_FIELD_BLOCKED_PIVOT             "Pivot"
#define LUA_MAP_FIELD_BLOCKED_COPY              "Copy"
#define LUA_MAP_FIELD_BLOCKED_DESTROY           "Destroy"

// Blocked functions
#define LUA_MAP_FIELD_BLOCKED_SETPARENT         "SetParent"
#define LUA_MAP_FIELD_BLOCKED_REMOVEPARENT      "RemoveFromParent"
#define LUA_MAP_FIELD_BLOCKED_APPLYFORCE        "ApplyForce"

static int _map_metatable_index(lua_State *L);
static int _map_metatable_newindex(lua_State *L);

bool lua_g_map_create_and_push(lua_State * const L, Shape *mapShape) {
    vx_assert(L != nullptr);
    vx_assert(mapShape != nullptr);
    LUAUTILS_STACK_SIZE(L)
    
    lua_newuserdata(L, 1); // Lua Map
    lua_newtable(L); // metatable
    {
        lua_pushstring(L, "__index");
        lua_pushcfunction(L, _map_metatable_index, "");
        lua_rawset(L, -3);
        
        lua_pushstring(L, "__newindex");
        lua_pushcfunction(L, _map_metatable_newindex, "");
        lua_rawset(L, -3);
        
        // hide the metatable from the Lua sandbox user
        lua_pushstring(L, "__metatable");
        lua_pushboolean(L, false);
        lua_rawset(L, -3);

        lua_pushstring(L, "__type");
        lua_pushstring(L, "Map");
        lua_rawset(L, -3);

        lua_pushstring(L, "__typen");
        lua_pushinteger(L, ITEM_TYPE_MAP);
        lua_rawset(L, -3);
        
        _lua_shape_push_fields(L, mapShape);
        _lua_mutableShape_push_fields(L, mapShape);
    }
    lua_setmetatable(L, -2);
    
    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
    
    if (lua_g_object_addIndexEntry(L, shape_get_transform(mapShape), -1) == false) {
        vxlog_error("failed to add Lua Map to Object registry");
        lua_pop(L, 1); // pop Lua Map
        LUAUTILS_STACK_SIZE_ASSERT(L, 0)
        return false;
    }

    // manage resources in lua object storage
    CGame *g = nullptr;
    if (lua_utils_getGamePtr(L, &g) == false || g == nullptr) {
        LUAUTILS_INTERNAL_ERROR(L);
    }
    scene_register_managed_transform(game_get_scene(g), shape_get_transform(mapShape));
    
    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
    return true;
}

int lua_map_snprintf(lua_State *L,
                     char* result,
                     size_t maxLen,
                     bool spacePrefix) {
    return snprintf(result, maxLen, "%s[Map]", spacePrefix ? " " : "");
}

// --------------------------------------------------
//
// MARK: - Static functions -
//
// --------------------------------------------------

/// _index metamethod of Lua Map
static int _map_metatable_index(lua_State *L) {
    vx_assert(L != nullptr);
    LUAUTILS_ASSERT_ARGS_COUNT(L, 2)
    LUAUTILS_ASSERT_ARG_TYPE(L, 1, ITEM_TYPE_MAP)
    LUAUTILS_STACK_SIZE(L)

    CGame *g = nullptr;
    lua_utils_getGamePtr(L, &g);
    vx_assert(g != nullptr);

    const char *key = lua_tostring(L, 2);

    if (strcmp(key, LUA_MAP_FIELD_BLOCKED_PHYSICS) == 0
        || strcmp(key, LUA_MAP_FIELD_BLOCKED_VELOCITY) == 0
        || strcmp(key, LUA_MAP_FIELD_BLOCKED_MOTION) == 0
        || strcmp(key, LUA_MAP_FIELD_BLOCKED_ACCELERATION) == 0
        || strcmp(key, LUA_MAP_FIELD_BLOCKED_ISONGROUND) == 0
        || strcmp(key, LUA_MAP_FIELD_BLOCKED_COLLISIONBOX) == 0
        || strcmp(key, LUA_MAP_FIELD_BLOCKED_APPLYFORCE) == 0
        || strcmp(key, LUA_MAP_FIELD_BLOCKED_PIVOT) == 0
        || strcmp(key, LUA_MAP_FIELD_BLOCKED_COPY) == 0
        || strcmp(key, LUA_MAP_FIELD_BLOCKED_DESTROY) == 0) {
        // blocked for Lua Map
        LUAUTILS_ERROR_F(L, "Map: %s cannot be used", key);
    } else if (_lua_object_metatable_index(L, key) == false) {
        // no field found in Object

        Shape *s = game_get_map_shape(g);
        if (s == nullptr) {
            lua_pushnil(L);
        } else if (_lua_shape_metatable_index(L, s, key)) {
            // a field from Shape was pushed onto the stack
        } else if (_lua_mutableShape_metatable_index(L, s, key)) {
            // a field from MutableShape was pushed onto the stack
        } else {
            // key is not known and starts with an uppercase letter
            lua_pushnil(L);
        }
    }

    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
    return 1;
}

/// __new_index metamethod of Lua Map
static int _map_metatable_newindex(lua_State *L) {
    vx_assert(L != nullptr);
    LUAUTILS_ASSERT_ARGS_COUNT(L, 3)
    LUAUTILS_ASSERT_ARG_TYPE(L, 1, ITEM_TYPE_MAP)
    LUAUTILS_STACK_SIZE(L)
    
    CGame *g = nullptr;
    lua_utils_getGamePtr(L, &g);
    vx_assert(g != nullptr);
    
    Shape *s = game_get_map_shape(g);
    if (s == nullptr) {
        LUAUTILS_INTERNAL_ERROR(L) // returns
    }
    
    Transform *t = shape_get_transform(s);
    
    const char *key = lua_tostring(L, 2);
    if (strcmp(key, LUA_MAP_FIELD_BLOCKED_PHYSICS) == 0
        || strcmp(key, LUA_MAP_FIELD_BLOCKED_VELOCITY) == 0
        || strcmp(key, LUA_MAP_FIELD_BLOCKED_MOTION) == 0
        || strcmp(key, LUA_MAP_FIELD_BLOCKED_ACCELERATION) == 0
        || strcmp(key, LUA_MAP_FIELD_BLOCKED_ISONGROUND) == 0
        || strcmp(key, LUA_MAP_FIELD_BLOCKED_COLLISIONBOX) == 0
        || strcmp(key, LUA_MAP_FIELD_BLOCKED_PIVOT) == 0) {
        // blocked for Lua Map
        LUAUTILS_ERROR_F(L, "Map: %s field is not settable", key);

    } else if (_lua_object_metatable_newindex(L, t, key) == false) {
        // the key is not from Object
        if (_lua_shape_metatable_newindex(L, s, key) == false) {
            // the key is not from Shape
            if (_lua_mutableShape_metatable_newindex(L, s, key) == false) {
                // key not found
                LUAUTILS_ERROR_F(L, "Map: %s field is not settable", key);
            }
        }
    }
    
    LUAUTILS_STACK_SIZE_ASSERT(L, 0)
    return 0;
}
