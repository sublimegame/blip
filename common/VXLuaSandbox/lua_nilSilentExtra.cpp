//
//  lua_nilSilentExtra.cpp
//  Cubzh
//
//  Created by Gaetan de Villele on 16/11/2020.
//  Copyright � 2019 Voxowl Inc. All rights reserved.
//

// This is like a silent Lua nil, but it is not linked to the 
// "nil" key word in the language.
// This way you can have this luaSilent while "nil" 
// is not silent.

#include "lua_nilSilentExtra.hpp"

// Lua
#include "lua.hpp"
#include "lua_utils.hpp"

#include "config.h"

#define LUA_NILSILENTEXTRA_REGISTRY_NAME "p3s_nilSilentExtra"

// --------------------------------------------------
//
// MARK: - Static functions prototypes -
//
// --------------------------------------------------
static int _metatable_index(lua_State *L);
static int _metatable_newindex(lua_State *L);
static int _metatable_eq(lua_State *L);
static int _metatable_call(lua_State *L);

// --------------------------------------------------
//
// MARK: - Exposed functions -
//
// --------------------------------------------------

/// Creates the extra silent nil table in the Lua registry
void lua_nilSilentExtra_setupRegistry(lua_State* L) {
    vx_assert(L != nullptr);
    LUAUTILS_STACK_SIZE(L)

    // create new "extra silent nil" table (that will be stored in registry)
    lua_newtable(L);
    lua_newtable(L);
    {
        lua_pushstring(L, "__index");
        lua_pushcfunction(L, _metatable_index, "");
        lua_settable(L, -3);
        
        lua_pushstring(L, "__newindex");
        lua_pushcfunction(L, _metatable_newindex, "");
        lua_settable(L, -3);
        
        lua_pushstring(L, "__metatable");
        lua_pushboolean(L, false);
        lua_settable(L, -3);
        
        lua_pushstring(L, "__eq");
        lua_pushcfunction(L, _metatable_eq, "");
        lua_settable(L, -3);
        
        lua_pushstring(L, "__call");
        lua_pushcfunction(L, _metatable_call, "");
        lua_settable(L, -3);
    }
    // assign metatable to table
    lua_setmetatable(L, -2); // pops table from the top of the stack
    
    // "extra silent nil" table is on top of the stack
    // We now store it into the Lua registry
    
    // Define sandbox role value in Lua sandbox registry (Player)
    lua_pushvalue(L, LUA_REGISTRYINDEX);
    lua_pushstring(L, LUA_NILSILENTEXTRA_REGISTRY_NAME); // key
    lua_pushvalue(L, -3); // value
    lua_rawset(L, -3);
    lua_pop(L, 1); // pop registry table
    lua_pop(L, 1); // pop extra silent nil table
    
    LUAUTILS_STACK_SIZE_ASSERT(L, 0)
}

/// Pushes the nil silent extra table onto the Lua stack
bool lua_nilSilentExtra_push(lua_State *L) {
    vx_assert(L != nullptr);
    LUAUTILS_STACK_SIZE(L)
    
    lua_pushvalue(L, LUA_REGISTRYINDEX); // push registry
    lua_pushstring(L, LUA_NILSILENTEXTRA_REGISTRY_NAME); // key
    lua_rawget(L, -2);
    lua_remove(L, -2); // remove registry
    
    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
    return true;
}

// --------------------------------------------------
//
// MARK: - Static functions implementation -
//
// --------------------------------------------------

static int _metatable_index(lua_State *L) {
    vx_assert(L != nullptr);
    LUAUTILS_ASSERT_ARGS_COUNT(L, 2)
    // LUAUTILS_ASSERT_ARG_TYPE(L, 1, )
    LUAUTILS_STACK_SIZE(L)
    
    lua_pushvalue(L, 1); // pushes itself on the stack

    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
    return 1;
}

static int _metatable_newindex(lua_State *L) {
    // doesn't do anything, in silence
    return 0;
}

static int _metatable_eq(lua_State *L) {
    return 0;
}

static int _metatable_call(lua_State *L) {
    return 0;
}
