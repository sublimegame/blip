//
//  lua_inputs.hpp
//  Cubzh
//
//  Created by Xavier Legland on 7/21/20.
//

#pragma once

// Lua
#include "lua.hpp"

#define LUA_INPUTS_FIELD_POINTER   "Pointer"     // Table

/// Creates a new "Inputs" table and pushes it onto the Lua stack.
bool lua_g_inputs_pushNewTable(lua_State *L);

/// Pushes the Local.Inputs table onto the Lua stack
bool lua_g_inputs_push_table(lua_State *L);

// Pushes field for given key.
// It's better to use this function in case fields end up being stored differently.
// Pushes nil if the field is not found.
void lua_g_inputs_pushField(lua_State * const L, const int idx, const char *key);

