// -------------------------------------------------------------
//  Cubzh
//  lua_lightType.hpp
//  Created by Arthur Cormerais on May 11, 2022.
// -------------------------------------------------------------

#pragma once

#include <stdint.h>

#include "lua.hpp"
#include "light.h"

#define LUA_LIGHTTYPE_VALUE "light_type"

bool lua_g_lightType_createAndPush(lua_State *L);
bool lua_lightType_pushTable(lua_State *L, LightType type);
LightType lua_lightType_get(lua_State *L, int idx);
int lua_lightType_snprintf(lua_State *L, char *result, size_t maxLen, bool spacePrefix);
