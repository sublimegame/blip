
#pragma once

#include <stdint.h>

#include "lua.hpp"
#include "camera.h"

#define LUA_PROJECTIONMODE_VALUE "projection_mode"

bool lua_g_projectionMode_createAndPush(lua_State *L);
bool lua_projectionMode_pushTable(lua_State *L, ProjectionMode mode);
ProjectionMode lua_projectionMode_get(lua_State *L, int idx);
int lua_projectionMode_snprintf(lua_State *L, char *result, size_t maxLen, bool spacePrefix);
