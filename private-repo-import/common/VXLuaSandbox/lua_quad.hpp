// -------------------------------------------------------------
//  Cubzh
//  lua_quad.hpp
//  Created by Arthur Cormerais on May 4, 2023.
// -------------------------------------------------------------

#pragma once

#include "lua.hpp"

#include "quad.h"
#include "game.h"

// MARK: - Quad global table -

void lua_g_quad_createAndPush(lua_State * const L);
int lua_g_quad_snprintf(lua_State *L, char *result, size_t maxLen, bool spacePrefix);

// MARK: - Quad instances tables -

bool lua_quad_pushNewInstance(lua_State *L, Quad *d);
bool lua_quad_getOrCreateInstance(lua_State * const L, Quad *q); // get lua Quad from quad ptr
bool lua_quad_isQuad(lua_State * const L, const int idx);
Quad *lua_quad_getQuadPtr(lua_State * const L, const int idx);
int lua_quad_snprintf(lua_State *L, char *result, size_t maxLen, bool spacePrefix);

// MARK: - Internal -
// Used by other lua Object types that extend lua Quad

bool _lua_quad_metatable_index(lua_State * const L, Quad *q, const char *key);
bool _lua_quad_metatable_newindex(lua_State * const L, Quad *q, const char *key);

void lua_quad_release_transform(void *_ud);
