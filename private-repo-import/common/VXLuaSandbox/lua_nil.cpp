//
//  lua_nil.c
//  Cubzh
//
//  Created by Gaetan de Villele on 29/07/2019.
//  Copyright © 2019 voxowl. All rights reserved.
//

#include "lua_nil.hpp"

// Lua
#include "lua.hpp"
#include "lua_utils.hpp"
#include "lua_logs.hpp"

#include "config.h"

static int _metatable_index(lua_State *L);
static int _metatable_newindex(lua_State *L);
static int _metatable_call(lua_State *L);

void lua_nil_setupRegistry(lua_State *L) {
    vx_assert(L != nullptr);
    LUAUTILS_STACK_SIZE(L)
    
    lua_pushnil(L);
    lua_newtable(L); // set nil's metatable
    {
        lua_pushstring(L, "__index");
        lua_pushcfunction(L, _metatable_index, "");
        lua_rawset(L, -3);
        
        lua_pushstring(L, "__newindex");
        lua_pushcfunction(L, _metatable_newindex, "");
        lua_rawset(L, -3);
        
        lua_pushstring(L, "__metatable");
        lua_pushboolean(L, false);
        lua_rawset(L, -3);
        
        // this is not needed, as __eq is invoqued only when
        // comparing similar types (not when comparing nil and a table for instance)
        // lua_pushstring(L, "__eq");
        // lua_pushcfunction(L, _metatable_eq, "");
        // lua_rawset(L, -3);
        
        lua_pushstring(L, "__call");
        lua_pushcfunction(L, _metatable_call, "");
        lua_rawset(L, -3);
        
        // assign metatable to table
    }
    lua_setmetatable(L, -2); // pops table from the top of the stack
    lua_pop(L, 1); // pop nil

    LUAUTILS_STACK_SIZE_ASSERT(L, 0)
}

static int _metatable_index(lua_State *L) {
    vx_assert(L != nullptr);
    LUAUTILS_STACK_SIZE(L)
    lua_pushnil(L);
    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
    return 1;
}

static int _metatable_newindex(lua_State *L) {
    vx_assert(L != nullptr);
    LUAUTILS_ASSERT_ARGS_COUNT(L, 3)
    LUAUTILS_STACK_SIZE(L)
    
    const char *key = lua_tostring(L, 2);
    if (key != nullptr) {
        LUAUTILS_ERROR_F(L, "nil.%s can't be set", key);
    } else {
        LUAUTILS_ERROR(L, "nil key can't be set");
    }
    
    LUAUTILS_STACK_SIZE_ASSERT(L, 0)
    return 0;
}

static int _metatable_call(lua_State *L) {
    vx_assert(L != nullptr);
    LUAUTILS_STACK_SIZE(L)
    
    LUAUTILS_ERROR(L, "nil function called");

    LUAUTILS_STACK_SIZE_ASSERT(L, 0)
    return 0;
}
