// -------------------------------------------------------------
//  Cubzh
//  lua_clouds.hpp
//  Created by Arthur Cormerais on December 2, 2020.
// -------------------------------------------------------------

#pragma once

#include "xptools.h"

typedef struct lua_State lua_State;

/// Creates a new global Clouds object and pushes it onto the lua stack
/// @param L the Lua state
bool lua_g_clouds_pushNewTable(lua_State *L);

/// Lua print function for global Clouds
int lua_g_clouds_snprintf(lua_State *L, char *result, size_t maxLen, bool spacePrefix);
