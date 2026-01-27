//
//  lua_face.cpp
//  Cubzh
//
//  Created by Xavier Legland on 27/04/2021.
//  Copyright © 2020 Voxowl Inc. All rights reserved.
//

#include "lua_face.hpp"

// engine
#include "config.h" // for vx_assert

// LuaUtils
#include "lua_utils.hpp"

// libsbs
#include "sbs.hpp"

#include "vxlog.h"

#define LUA_FACE_FIELD_TOP "Top"
#define LUA_FACE_FIELD_BOTTOM "Bottom"
#define LUA_FACE_FIELD_RIGHT "Right"
#define LUA_FACE_FIELD_LEFT "Left"
#define LUA_FACE_FIELD_FRONT "Front"
#define LUA_FACE_FIELD_BACK "Back"

// --------------------------------------------------
//
// MARK: - Static functions' prototypes -
//
// --------------------------------------------------

static bool _lua_face_createAndpushTable(lua_State *L, uint8_t face);

static int _face_g_metatable_newindex(lua_State *L);

static int _face_instance_metatable_index(lua_State *L);
static int _face_instance_metatable_newindex(lua_State *L);

// --------------------------------------------------
//
// MARK: - Exposed functions -
//
// --------------------------------------------------

bool lua_g_face_createAndPush(lua_State *L) {
    vx_assert(L != nullptr);
    LUAUTILS_STACK_SIZE(L)
    
    lua_newuserdata(L, 1); // global "Face" table
    lua_newtable(L); // metatable
    {
        lua_pushstring(L, "__index");
        lua_newtable(L);
        {
            lua_pushstring(L, LUA_FACE_FIELD_TOP);
            _lua_face_createAndpushTable(L, LUA_FACE_TYPE_TOP);
            lua_rawset(L, -3);
            
            lua_pushstring(L, LUA_FACE_FIELD_BOTTOM);
            _lua_face_createAndpushTable(L, LUA_FACE_TYPE_BOTTOM);
            lua_rawset(L, -3);
            
            lua_pushstring(L, LUA_FACE_FIELD_LEFT);
            _lua_face_createAndpushTable(L, LUA_FACE_TYPE_LEFT);
            lua_rawset(L, -3);
            
            lua_pushstring(L, LUA_FACE_FIELD_RIGHT);
            _lua_face_createAndpushTable(L, LUA_FACE_TYPE_RIGHT);
            lua_rawset(L, -3);
            
            lua_pushstring(L, LUA_FACE_FIELD_FRONT);
            _lua_face_createAndpushTable(L, LUA_FACE_TYPE_FRONT);
            lua_rawset(L, -3);
            
            lua_pushstring(L, LUA_FACE_FIELD_BACK);
            _lua_face_createAndpushTable(L, LUA_FACE_TYPE_BACK);
            lua_rawset(L, -3);
        }
        lua_rawset(L, -3);
        
        lua_pushstring(L, "__newindex");
        lua_pushcfunction(L, _face_g_metatable_newindex, "");
        lua_rawset(L, -3);
        
        lua_pushstring(L, "__metatable");
        lua_pushboolean(L, false);
        lua_rawset(L, -3);

        lua_pushstring(L, "__type");
        lua_pushstring(L, "FaceInterface");
        lua_rawset(L, -3);

        lua_pushstring(L, "__typen");
        lua_pushinteger(L, ITEM_TYPE_G_FACE);
        lua_rawset(L, -3);
    }
    lua_setmetatable(L, -2);

    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
    return true;
}
    
bool lua_face_pushTable(lua_State *L, uint8_t face) {
    vx_assert(L != nullptr);
    LUAUTILS_STACK_SIZE(L)

    lua_getglobal(L, P3S_LUA_G_FACE);
    
    switch (face) {
        case LUA_FACE_TYPE_RIGHT:
            lua_getfield(L, -1, LUA_FACE_FIELD_RIGHT);
            break;
        case LUA_FACE_TYPE_LEFT:
            lua_getfield(L, -1, LUA_FACE_FIELD_LEFT);
            break;
        case LUA_FACE_TYPE_FRONT:
            lua_getfield(L, -1, LUA_FACE_FIELD_FRONT);
            break;
        case LUA_FACE_TYPE_BACK:
            lua_getfield(L, -1, LUA_FACE_FIELD_BACK);
            break;
        case LUA_FACE_TYPE_TOP:
            lua_getfield(L, -1, LUA_FACE_FIELD_TOP);
            break;
        case LUA_FACE_TYPE_BOTTOM:
            lua_getfield(L, -1, LUA_FACE_FIELD_BOTTOM);
            break;
        default:
            // should not happen
            vx_assert(false);
    }
    
    lua_remove(L, -2); // remove global Face
    
    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
    return true;
}

int lua_face_getFaceInt(lua_State *L, int idx) {
    vx_assert(L != nullptr);
    LUAUTILS_STACK_SIZE(L)

    if (LUA_GET_METAFIELD_AND_RETURN_TYPE(L, idx, LUA_FACE_TYPE) == LUA_TNIL) {
        LUAUTILS_INTERNAL_ERROR(L)
    }

    int face = static_cast<int>(lua_tointeger(L, -1));
    lua_pop(L, 1);

    LUAUTILS_STACK_SIZE_ASSERT(L, 0)
    return face;
}

int lua_face_snprintf(lua_State *L, char* result, size_t maxLen, bool spacePrefix) {
    vx_assert(L != nullptr);
    LUAUTILS_STACK_SIZE(L)

    LUA_GET_METAFIELD(L, -1, LUA_FACE_TYPE);

    switch (luaL_checkinteger(L, -1)) {
        case LUA_FACE_TYPE_RIGHT:
            lua_pop(L, 1);
            LUAUTILS_STACK_SIZE_ASSERT(L, 0)
            return snprintf(result, maxLen, "%s[Face: Right]",
                            spacePrefix ? " " : "");
        case LUA_FACE_TYPE_LEFT:
            lua_pop(L, 1);
            LUAUTILS_STACK_SIZE_ASSERT(L, 0)
            return snprintf(result, maxLen, "%s[Face: Left]",
                            spacePrefix ? " " : "");
        case LUA_FACE_TYPE_FRONT:
            lua_pop(L, 1);
            LUAUTILS_STACK_SIZE_ASSERT(L, 0)
            return snprintf(result, maxLen, "%s[Face: Front]",
                            spacePrefix ? " " : "");
        case LUA_FACE_TYPE_BACK:
            lua_pop(L, 1);
            LUAUTILS_STACK_SIZE_ASSERT(L, 0)
            return snprintf(result, maxLen, "%s[Face: Back]",
                            spacePrefix ? " " : "");
        case LUA_FACE_TYPE_TOP:
            lua_pop(L, 1);
            LUAUTILS_STACK_SIZE_ASSERT(L, 0)
            return snprintf(result, maxLen, "%s[Face: Top]",
                            spacePrefix ? " " : "");
        case LUA_FACE_TYPE_BOTTOM:
            lua_pop(L, 1);
            LUAUTILS_STACK_SIZE_ASSERT(L, 0)
            return snprintf(result, maxLen, "%s[Face: Bottom]",
                            spacePrefix ? " " : "");
        default:
            lua_pop(L, 1);
            LUAUTILS_STACK_SIZE_ASSERT(L, 0)
            return snprintf(result, maxLen, "%s[Face: unknown]",
                            spacePrefix ? " " : "");
    }
}

// --------------------------------------------------
//
// MARK: - static functions
//
// --------------------------------------------------

static bool _lua_face_createAndpushTable(lua_State *L, uint8_t face) {
    vx_assert(L != nullptr);
    if (face > 5) { return false; }
    
    LUAUTILS_STACK_SIZE(L)
    
    lua_newuserdata(L, 1); // Face table
    lua_newtable(L); // metatable
    {
        lua_pushstring(L, "__index");
        lua_pushcfunction(L, _face_instance_metatable_index, "");
        lua_rawset(L, -3);

        lua_pushstring(L, "__newindex");
        lua_pushcfunction(L, _face_instance_metatable_newindex, "");
        lua_rawset(L, -3);

        lua_pushstring(L, "__metatable");
        lua_pushboolean(L, false);
        lua_rawset(L, -3);

        lua_pushstring(L, LUA_FACE_TYPE);
        lua_pushinteger(L, face);
        lua_rawset(L, -3);

        lua_pushstring(L, "__type");
        lua_pushstring(L, "Face");
        lua_rawset(L, -3);

        lua_pushstring(L, "__typen");
        lua_pushinteger(L, ITEM_TYPE_FACE);
        lua_rawset(L, -3);
    }
    lua_setmetatable(L, -2);
    
    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
    return true;
}

static int _face_g_metatable_newindex(lua_State *L) {
    LUAUTILS_STACK_SIZE(L)

    LUAUTILS_ERROR(L, "Face table is read only");

    LUAUTILS_STACK_SIZE_ASSERT(L, 1);
    return 0;
}

static int _face_instance_metatable_index(lua_State *L) {
    LUAUTILS_STACK_SIZE(L)
    
    // nothing stored in Face tables
    lua_pushnil(L);

    LUAUTILS_STACK_SIZE_ASSERT(L, 1);
    return 1;
}

static int _face_instance_metatable_newindex(lua_State *L) {
    LUAUTILS_STACK_SIZE(L)

    LUAUTILS_ERROR(L, "Face table is read only");

    LUAUTILS_STACK_SIZE_ASSERT(L, 1);
    return 0;
}
