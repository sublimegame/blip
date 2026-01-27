
#include "lua_world.hpp"

// C++
#include <cmath>
#include <cstring>

// Lua
#include "lua.hpp"

// engine
#include "scene.h"
#include "serialization.h"
#include "game.h"

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

#define LUA_WORLD_FIELD_BLOCKED_SETPARENT           "SetParent"
#define LUA_WORLD_FIELD_BLOCKED_REMOVEPARENT        "RemoveFromParent"
#define LUA_WORLD_FIELD_BLOCKED_ROTATELOCAL         "RotateLocal"
#define LUA_WORLD_FIELD_BLOCKED_ROTATEWORLD         "RotateWorld"
#define LUA_WORLD_FIELD_BLOCKED_PHYSICS             "Physics"
#define LUA_WORLD_FIELD_BLOCKED_VELOCITY            "Velocity"
#define LUA_WORLD_FIELD_BLOCKED_MOTION              "Motion"
#define LUA_WORLD_FIELD_BLOCKED_ACCELERATION        "Acceleration"
#define LUA_WORLD_FIELD_BLOCKED_ISHIDDEN            "IsHidden"
#define LUA_WORLD_FIELD_BLOCKED_ISONGROUND          "IsOnGround"
#define LUA_WORLD_FIELD_BLOCKED_COLLISIONBOX        "CollisionBox"
#define LUA_WORLD_FIELD_BLOCKED_COLLISIONGROUPSMASK "CollisionGroupsMask"
#define LUA_WORLD_FIELD_BLOCKED_COLLIDESWITHMASK    "CollidesWithMask"
#define LUA_WORLD_FIELD_BLOCKED_COLLIDESWITHGROUPS  "CollidesWithGroups"
#define LUA_WORLD_FIELD_BLOCKED_COLLISIONGROUPS     "CollisionGroups"
#define LUA_WORLD_FIELD_BLOCKED_COLLIDESWITH        "CollidesWith"
#define LUA_WORLD_FIELD_BLOCKED_MASS                "Mass"
#define LUA_WORLD_FIELD_BLOCKED_FRICTION            "Friction"
#define LUA_WORLD_FIELD_BLOCKED_BOUNCINESS          "Bounciness"
#define LUA_WORLD_FIELD_BLOCKED_APPLYFORCE          "ApplyForce"
#define LUA_WORLD_FIELD_BLOCKED_ONCOLLISION         "OnCollision"
#define LUA_WORLD_FIELD_BLOCKED_TICK                "Tick"

// MARK: - Static functions prototypes -

static int _world_metatable_index(lua_State * const L);
static int _world_metatable_newindex(lua_State * const L);

// MARK: - Exposed functions -

bool lua_g_world_create_and_push(lua_State * const L, Scene * const sc) {
    vx_assert(L != nullptr);
    LUAUTILS_STACK_SIZE(L)
    
    lua_newtable(L); // Lua World
    lua_newtable(L); // metatable
    {
        lua_pushstring(L, "__index");
        lua_pushcfunction(L, _world_metatable_index, "");
        lua_rawset(L, -3);
        
        lua_pushstring(L, "__newindex");
        lua_pushcfunction(L, _world_metatable_newindex, "");
        lua_rawset(L, -3);
        
        // hide the metatable from the Lua sandbox user
        lua_pushstring(L, "__metatable");
        lua_pushboolean(L, false);
        lua_rawset(L, -3);
        
        lua_pushstring(L, P3S_LUA_OBJ_METAFIELD_TYPE); // used to log tables
        lua_pushinteger(L, ITEM_TYPE_WORLD);
        lua_rawset(L, -3);
        
        _lua_object_set_readonly(L, true, false);
    }
    lua_setmetatable(L, -2);
    
    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
    
    if (lua_g_object_addIndexEntry(L, scene_get_root(sc), -1) == false) {
        vxlog_error("failed to add Lua World to Object registry");
        lua_pop(L, 1); // pop Lua World
        LUAUTILS_STACK_SIZE_ASSERT(L, 0)
        return false;
    }
    
    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
    return true;
}

int lua_world_snprintf(lua_State * const L,
                       char* result,
                       size_t maxLen,
                       bool spacePrefix) {
    return snprintf(result, maxLen, "%s[World]", spacePrefix ? " " : "");
}

// --------------------------------------------------
//
// MARK: - Static functions -
//
// --------------------------------------------------

static int _world_metatable_index(lua_State * const L) {
    vx_assert(L != nullptr);
    LUAUTILS_ASSERT_ARGS_COUNT(L, 2)
    LUAUTILS_ASSERT_ARG_TYPE(L, 1, ITEM_TYPE_WORLD)
    LUAUTILS_STACK_SIZE(L)
    
    CGame *g = nullptr;
    Scene *sc = lua_world_getSceneFromGame(L, &g);
    if (g == nullptr || sc == nullptr) {
        LUAUTILS_INTERNAL_ERROR(L) // returns
    }
    
    const char *key = lua_tostring(L, 2);
    
    if (strcmp(key, LUA_WORLD_FIELD_BLOCKED_SETPARENT) == 0
        || strcmp(key, LUA_WORLD_FIELD_BLOCKED_REMOVEPARENT) == 0
        || strcmp(key, LUA_WORLD_FIELD_BLOCKED_ROTATELOCAL) == 0
        || strcmp(key, LUA_WORLD_FIELD_BLOCKED_ROTATEWORLD) == 0
        || strcmp(key, LUA_WORLD_FIELD_BLOCKED_PHYSICS) == 0
        || strcmp(key, LUA_WORLD_FIELD_BLOCKED_VELOCITY) == 0
        || strcmp(key, LUA_WORLD_FIELD_BLOCKED_MOTION) == 0
        || strcmp(key, LUA_WORLD_FIELD_BLOCKED_ACCELERATION) == 0
        || strcmp(key, LUA_WORLD_FIELD_BLOCKED_ISHIDDEN) == 0
        || strcmp(key, LUA_WORLD_FIELD_BLOCKED_ISONGROUND) == 0
        || strcmp(key, LUA_WORLD_FIELD_BLOCKED_COLLISIONBOX) == 0
        || strcmp(key, LUA_WORLD_FIELD_BLOCKED_COLLISIONGROUPSMASK) == 0
        || strcmp(key, LUA_WORLD_FIELD_BLOCKED_COLLIDESWITHMASK) == 0
        || strcmp(key, LUA_WORLD_FIELD_BLOCKED_COLLIDESWITHGROUPS) == 0
        || strcmp(key, LUA_WORLD_FIELD_BLOCKED_COLLISIONGROUPS) == 0
        || strcmp(key, LUA_WORLD_FIELD_BLOCKED_COLLIDESWITH) == 0
        || strcmp(key, LUA_WORLD_FIELD_BLOCKED_MASS) == 0
        || strcmp(key, LUA_WORLD_FIELD_BLOCKED_FRICTION) == 0
        || strcmp(key, LUA_WORLD_FIELD_BLOCKED_BOUNCINESS) == 0
        || strcmp(key, LUA_WORLD_FIELD_BLOCKED_APPLYFORCE) == 0
        || strcmp(key, LUA_WORLD_FIELD_BLOCKED_ONCOLLISION) == 0
        || strcmp(key, LUA_WORLD_FIELD_BLOCKED_TICK) == 0) {
        // blocked for Lua World
        LUAUTILS_ERROR_F(L, "World: %s cannot be used", key);

    } else if (_lua_object_metatable_index(L, key)) {
        // a field from Object was pushed onto the stack
    } else {
        // if key is unknown warn the user
        // LUAUTILS_ERROR_F(L, "World: %s field does not exist", key);
        lua_pushnil(L);
    }
    
    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
    return 1;
}

static int _world_metatable_newindex(lua_State * const L) {
    vx_assert(L != nullptr);
    LUAUTILS_ASSERT_ARGS_COUNT(L, 3)
    LUAUTILS_ASSERT_ARG_TYPE(L, 1, ITEM_TYPE_WORLD)
    LUAUTILS_STACK_SIZE(L)
    
    CGame *g = nullptr;
    Scene *sc = lua_world_getSceneFromGame(L, &g);
    if (g == nullptr || sc == nullptr) {
        LUAUTILS_INTERNAL_ERROR(L) // returns
    }
    
    const char *key = lua_tostring(L, 2);
    if (strcmp(key, LUA_WORLD_FIELD_BLOCKED_PHYSICS) == 0
        || strcmp(key, LUA_WORLD_FIELD_BLOCKED_VELOCITY) == 0
        || strcmp(key, LUA_WORLD_FIELD_BLOCKED_MOTION) == 0
        || strcmp(key, LUA_WORLD_FIELD_BLOCKED_ACCELERATION) == 0
        || strcmp(key, LUA_WORLD_FIELD_BLOCKED_ISHIDDEN) == 0
        || strcmp(key, LUA_WORLD_FIELD_BLOCKED_COLLISIONBOX) == 0
        || strcmp(key, LUA_WORLD_FIELD_BLOCKED_COLLISIONGROUPSMASK) == 0
        || strcmp(key, LUA_WORLD_FIELD_BLOCKED_COLLIDESWITHMASK) == 0
        || strcmp(key, LUA_WORLD_FIELD_BLOCKED_COLLIDESWITHGROUPS) == 0
        || strcmp(key, LUA_WORLD_FIELD_BLOCKED_COLLISIONGROUPS) == 0
        || strcmp(key, LUA_WORLD_FIELD_BLOCKED_MASS) == 0
        || strcmp(key, LUA_WORLD_FIELD_BLOCKED_FRICTION) == 0
        || strcmp(key, LUA_WORLD_FIELD_BLOCKED_BOUNCINESS) == 0
        || strcmp(key, LUA_WORLD_FIELD_BLOCKED_ONCOLLISION) == 0
        || strcmp(key, LUA_WORLD_FIELD_BLOCKED_TICK) == 0) {
        // blocked for Lua World
        LUAUTILS_ERROR_F(L, "World: %s is not settable", key);

    } else {
        if (_lua_object_metatable_newindex(L, scene_get_root(sc), key) == false) {
            LUAUTILS_ERROR_F(L, "World: %s is not settable", key);
        }
    }
    
    LUAUTILS_STACK_SIZE_ASSERT(L, 0)
    return 0;
}

// MARK: - Utils -

Scene *lua_world_getSceneFromGame(lua_State * const L, CGame **g) {
    CGame *game = nullptr;
    if (lua_utils_getGamePtr(L, &game) == false || game == nullptr) {
        return nullptr;
    }
    if (g != nullptr) {
        *g = game;
    }
    return game_get_scene(game);
}
