// -------------------------------------------------------------
//  Cubzh
//  lua_physicsMode.hpp
//  Created by Arthur Cormerais on January 12, 2023.
// -------------------------------------------------------------

#pragma once

#include <stdint.h>

#include "lua.hpp"
#include "rigidBody.h"

#define LUA_PHYSICSMODE_VALUE "physics_mode"

bool lua_g_physicsMode_createAndPush(lua_State *L);
bool lua_physicsMode_pushTable(lua_State *L, RigidbodyMode mode);
bool lua_physicsMode_pushTable(lua_State *L, uint8_t mode);
RigidbodyMode lua_physicsMode_get(lua_State *L, int idx);
int lua_physicsMode_snprintf(lua_State *L, char *result, size_t maxLen, bool spacePrefix);
