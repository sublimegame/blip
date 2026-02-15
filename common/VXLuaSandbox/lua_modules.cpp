//
//  lua_modules.cpp
//  Cubzh
//
//  Created by Adrien Duermael on 05/01/2024.
//  Copyright © 2024 Voxowl Inc. All rights reserved.
//

#include "lua_modules.hpp"

#include "lua.hpp"
#include "lua_utils.hpp"
#include "lua_constants.h"
#include "xptools.h"
#include "config.h"

static int _lua_modules_new_index(lua_State * const L);
static int _lua_modules_new_index_readonly(lua_State * const L);
static int _modules_metatable_len(lua_State * const L);

void lua_modules_createAndPush(lua_State * const L) {
    vx_assert(L != nullptr);
    LUAUTILS_STACK_SIZE(L)
    
    lua_newtable(L); // global "Modules" table
    lua_newtable(L); // metatable
    {
        lua_pushstring(L, "__index");
        lua_newtable(L); // empty table by default
        lua_rawset(L, -3);
        
        lua_pushstring(L, "__newindex");
        lua_pushcfunction(L, _lua_modules_new_index, "");
        lua_rawset(L, -3);

        lua_pushstring(L, "__len");
        lua_pushcfunction(L, _modules_metatable_len, "");
        lua_rawset(L, -3);

        lua_pushstring(L, "__metatable"); // hide the metatable from the Lua sandbox user
        lua_pushboolean(L, false);
        lua_rawset(L, -3);
        
        lua_pushstring(L, P3S_LUA_OBJ_METAFIELD_TYPE);
        lua_pushinteger(L, ITEM_TYPE_G_MODULES);
        lua_rawset(L, -3);
    }
    lua_setmetatable(L, -2);
    
    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
}

void lua_modules_set_from_table(lua_State *L, const int idx) {
    LUAUTILS_ASSERT_ARG_TYPE(L, -1, ITEM_TYPE_G_MODULES)
    LUAUTILS_STACK_SIZE(L)

    // check value is a table
    if (lua_istable(L, idx) == false) {
        LUAUTILS_STACK_SIZE_ASSERT(L, 0)
        return;
    }

    lua_pushvalue(L, idx);

    // check if table can be accepted
    lua_pushnil(L);  /* first key, now at -1 */
    while (lua_next(L, idx) != 0) {
        // key now at -2, value at -1

        // all key types are accepted
        // string keys will pre-require module and define 
        // globals if starting with lowercase chars

        if (lua_isstring(L, -1) == false) {
            LUAUTILS_ERROR(L, "Modules expects string values")
        }

        // pop value, keep key for next iteration
        lua_pop(L, 1);
    }

    // -1 : table, -2: modules
    lua_getmetatable(L, -2);
    vx_assert(lua_istable(L, -1));
    // -1: modules' metatable, -2 : table, -3: modules

    lua_pushstring(L, "__index");
    // -1: "__index", -2: modules' metatable, -3 : table, -4: modules

    lua_pushvalue(L, -3);
    // -1 : table, -2: "__index", -3: modules' metatable, -4 : table, -5: modules

    lua_rawset(L, -3);
    // -1: modules' metatable, -2 : table, -3: modules

    lua_pop(L, 2);

    LUAUTILS_STACK_SIZE_ASSERT(L, 0)
    return;
}

void lua_modules_make_read_only(lua_State *L, const int idx) {
    vx_assert(lua_utils_getObjectType(L, idx) == ITEM_TYPE_G_MODULES);
    LUAUTILS_STACK_SIZE(L)

    lua_getmetatable(L, idx);
    vx_assert(lua_istable(L, -1));

    lua_pushstring(L, "__newindex");
    lua_pushcfunction(L, _lua_modules_new_index_readonly, "");
    lua_rawset(L, -3);

    lua_pop(L, 1); // pop metatable
    LUAUTILS_STACK_SIZE_ASSERT(L, 0)
}

static int _lua_modules_new_index_readonly(lua_State * const L) {
    vx_assert(L != nullptr);
    LUAUTILS_ERROR(L, "Modules is read-only now")
    return 0;
}

static int _lua_modules_new_index(lua_State * const L) {
    vx_assert(L != nullptr);
    LUAUTILS_STACK_SIZE(L)
    LUAUTILS_ASSERT_ARGS_COUNT(L, 3)
    LUAUTILS_ASSERT_ARG_TYPE(L, 1, ITEM_TYPE_G_MODULES)

    if (lua_isstring(L, 3) == false) {
        LUAUTILS_ERROR(L, "Modules expects string values")
    }

    LUA_GET_METAFIELD(L, 1, "__index");
    lua_pushvalue(L, 2); // key
    lua_pushvalue(L, 3); // value
    lua_rawset(L, -3);

    lua_pop(L, 1);

    LUAUTILS_STACK_SIZE_ASSERT(L, 0)
    return 0;
}

static int _modules_metatable_len(lua_State *L) {
    vx_assert(L != nullptr);
    LUAUTILS_ASSERT_ARGS_COUNT(L, 2)
    LUAUTILS_ASSERT_ARG_TYPE(L, 1, ITEM_TYPE_G_MODULES)
    LUAUTILS_STACK_SIZE(L)

    LUA_GET_METAFIELD(L, 1, "__index");
    lua_pushinteger(L, lua_objlen(L, -1));
    lua_remove(L, -2);
    
    LUAUTILS_STACK_SIZE_ASSERT(L, 1);
    return 1;
}
