// -------------------------------------------------------------
//  Cubzh
//  lua_sky.hpp
//  Created by Arthur Cormerais on June 12, 2023.
// -------------------------------------------------------------

#pragma once

#include "xptools.h"

typedef struct lua_State lua_State;

bool lua_g_sky_pushNewTable(lua_State *L);
int lua_g_sky_snprintf(lua_State *L, char *result, size_t maxLen, bool spacePrefix);
