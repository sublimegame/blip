//
//  lua_impact.cpp
//  Cubzh
//
//  Created by Gaetan de Villele on 01/10/2019.
//  Copyright © 2019 voxowl. All rights reserved.
//

#include "lua_impact.hpp"

// Lua
#include "lua.hpp"
#include "lua_utils.hpp"

// sandbox
#include "sbs.hpp"
#include "lua_block.hpp"
#include "lua_players.hpp"
#include "lua_shape.hpp"
#include "lua_face.hpp"
#include "lua_object.hpp"

// xptools
#include "xptools.h"
#include "vxconfig.h"

// engine
#include "game.h"

#define LUA_IMPACT_FIELD_BLOCK       "Block"       //
#define LUA_IMPACT_FIELD_PLAYER      "Player"      //
#define LUA_IMPACT_FIELD_SHAPE       "Shape"       //
#define LUA_IMPACT_FIELD_OBJECT      "Object"      //
#define LUA_IMPACT_FIELD_DISTANCE    "Distance"    //
#define LUA_IMPACT_FIELD_FACETOUCHED "FaceTouched" //

// --------------------------------------------------
//
// MARK: - Static functions protoypes -
//
// --------------------------------------------------
static int impact_metatable_newindex(lua_State *L);

bool lua_impact_push(lua_State *L, CastResult hit) {
    if (hit.hitTr == nullptr) {
        lua_pushnil(L);
        return false;
    }

    switch (hit.type) {
        case Hit_None: {
            lua_pushnil(L);
            break;
        }
        case Hit_CollisionBox: {
            return lua_impact_pushImpactTableWithObject(L, hit.hitTr, hit.distance);
        }
        case Hit_Block: {
            Shape *s = static_cast<Shape *>(transform_get_ptr(hit.hitTr));
            return lua_impact_pushImpactTableWithBlock(L, s,
                                                       hit.blockCoords.x,
                                                       hit.blockCoords.y,
                                                       hit.blockCoords.z,
                                                       hit.distance,
                                                       hit.faceTouched);
        }
    }
    return false;
}

/// blockParentShape : optional
bool lua_impact_pushImpactTableWithBlock(lua_State *L,
                                         Shape *blockParentShape,
                                         SHAPE_COORDS_INT_T positionX,
                                         SHAPE_COORDS_INT_T positionY,
                                         SHAPE_COORDS_INT_T positionZ,
                                         const float distance,
                                         const FACE_INDEX_INT_T faceTouched) {

    vx_assert(L != nullptr);
    vx_assert(blockParentShape != nullptr);
    LUAUTILS_STACK_SIZE(L)

    lua_newtable(L); // impact table
    lua_newtable(L); // metatable +2
    {
        lua_pushstring(L, "__index");
        {
            lua_newtable(L); // index
            
            lua_pushstring(L, LUA_IMPACT_FIELD_BLOCK);
            lua_block_pushBlock(L, blockParentShape, positionX, positionY, positionZ);
            lua_rawset(L, -3);
            
            lua_pushstring(L, LUA_IMPACT_FIELD_PLAYER);
            lua_pushnil(L);
            lua_rawset(L, -3);
            
            lua_pushstring(L, LUA_IMPACT_FIELD_SHAPE);
            lua_g_object_getOrCreateInstance(L, shape_get_transform(blockParentShape));
            lua_rawset(L, -3);
    
            lua_pushstring(L, LUA_IMPACT_FIELD_OBJECT);
            lua_g_object_getOrCreateInstance(L, shape_get_transform(blockParentShape));
            lua_rawset(L, -3);
            
            lua_pushstring(L, LUA_IMPACT_FIELD_DISTANCE);
            lua_pushnumber(L, static_cast<double>(distance));
            lua_rawset(L, -3);
            
            lua_pushstring(L, LUA_IMPACT_FIELD_FACETOUCHED);
            lua_face_pushTable(L, faceTouched);
            lua_rawset(L, -3);
        }
        lua_rawset(L, -3); // set metatable.__index
        
        lua_pushstring(L, "__newindex");
        lua_pushcfunction(L, impact_metatable_newindex, "");
        lua_rawset(L, -3);
        
        lua_pushstring(L, "__metatable");
        lua_pushboolean(L, false);
        lua_rawset(L, -3);
        
        lua_pushstring(L, P3S_LUA_OBJ_METAFIELD_TYPE); // used to log tables
        lua_pushinteger(L, ITEM_TYPE_IMPACT);
        lua_rawset(L, -3);
    }
    lua_setmetatable(L, -2); // metatable is popped here // + 2

    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
    return true;
}

bool lua_impact_pushImpactTableWithObject(lua_State *L,
                                          Transform *t,
                                          const float distance) {
    
    vx_assert(L != nullptr);
    vx_assert(t != nullptr);
    LUAUTILS_STACK_SIZE(L)
    
    lua_newtable(L); // block table +1
    lua_newtable(L); // metatable +2
    {
        lua_pushstring(L, "__index");
        {
            lua_newtable(L); // index
            
            lua_pushstring(L, LUA_IMPACT_FIELD_BLOCK);
            lua_pushnil(L);
            lua_rawset(L, -3);

            lua_g_object_getOrCreateInstance(L, t);
    
            lua_pushstring(L, LUA_IMPACT_FIELD_OBJECT);
            lua_pushvalue(L, -2);
            lua_rawset(L, -4);

            lua_pushstring(L, LUA_IMPACT_FIELD_SHAPE);
            if (transform_get_type(t) == ShapeTransform) {
                lua_pushvalue(L, -2);
            } else {
                lua_pushnil(L);
            }
            lua_rawset(L, -4);

            lua_pushstring(L, LUA_IMPACT_FIELD_PLAYER);
            if (lua_utils_getObjectType(L, -2) == ITEM_TYPE_PLAYER) {
                lua_pushvalue(L, -2);
            } else {
                lua_pushnil(L);
            }
            lua_rawset(L, -4);

            lua_pop(L, 1); // pop lua object
            
            lua_pushstring(L, LUA_IMPACT_FIELD_DISTANCE);
            lua_pushnumber(L, static_cast<double>(distance));
            lua_rawset(L, -3);
            
            lua_pushstring(L, LUA_IMPACT_FIELD_FACETOUCHED);
            lua_pushnil(L);
            lua_rawset(L, -3);
        }
        lua_rawset(L, -3); // set metatable.__index
        
        lua_pushstring(L, "__newindex");
        lua_pushcfunction(L, impact_metatable_newindex, "");
        lua_settable(L, -3);
        
        lua_pushstring(L, "__metatable");
        lua_pushboolean(L, false);
        lua_settable(L, -3);
        
        lua_pushstring(L, P3S_LUA_OBJ_METAFIELD_TYPE); // used to log tables
        lua_pushinteger(L, ITEM_TYPE_IMPACT);
        lua_settable(L, -3);
    }
    lua_setmetatable(L, -2); // metatable is popped here // + 2
    
    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
    return true;
}

// --------------------------------------------------
//
// MARK: - Static functions -
//
// --------------------------------------------------

/// __newindex function for the metatable of the rayimpact table
///
/// arguments
/// - table (table)  : rayimpact table
/// - key   (string) : the key that is being set
/// - value (any)    : the value being stored
static int impact_metatable_newindex(lua_State *L) {
    vx_assert(L != nullptr);
    LUAUTILS_STACK_SIZE(L)

    // validate arguments count
    if (lua_utils_assertArgsCount(L, 3) == false) {
        LUAUTILS_ERROR(L, "incorrect argument count");
    }

    if (lua_utils_isStringStrict(L, 2) && lua_utils_isStringStrictStartingWithUppercase(L, 2)) {
        // field name is a capitalized string
        const char *key = lua_tostring(L, 2);
        // field is not settable
        vxlog_error("🌒⚠️ impact table \"%s\" field is not settable (capitalized names are reserved)", key);
        LUAUTILS_STACK_SIZE_ASSERT(L, 0)
        return 0;
    }

    // all other cases are allowed
    lua_pushvalue(L, 2); // key
    lua_pushvalue(L, 3); // value
    lua_rawset(L, 1);

    LUAUTILS_STACK_SIZE_ASSERT(L, 0)
    return 0;
}

int lua_impact_snprintf(lua_State *L, char *result, size_t maxLen, bool spacePrefix) {
    vx_assert(L != nullptr);
    return snprintf(result, maxLen, "%s[Impact] (instance)", spacePrefix ? " " : "");
}
