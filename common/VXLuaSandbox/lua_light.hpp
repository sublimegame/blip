// -------------------------------------------------------------
//  Cubzh
//  lua_light.hpp
//  Created by Arthur Cormerais on May 11, 2022.
// -------------------------------------------------------------

#pragma once

#include "lua.hpp"

#include "light.h"
#include "game.h"

// MARK: - Light global table -

void lua_g_light_createAndPush(lua_State * const L);
int lua_g_light_snprintf(lua_State *L, char *result, size_t maxLen, bool spacePrefix);

// MARK: - Light instances tables -

bool lua_light_pushNewInstance(lua_State *L, Light *l);
bool lua_light_getOrCreateInstance(lua_State * const L, Light *l); // get lua Light from light ptr
bool lua_light_isLight(lua_State * const L, const int idx);
Light *lua_light_getLightPtr(lua_State * const L, const int idx);
int lua_light_snprintf(lua_State *L, char *result, size_t maxLen, bool spacePrefix);

// MARK: - Internal -
// Used by other lua Object types that extend lua Light

void _lua_light_push_fields(lua_State * const L, Light *l);
bool _lua_light_metatable_index(lua_State * const L, Light *l, const char *key);
bool _lua_light_metatable_newindex(lua_State * const L, Light *l, const char *key);

void lua_light_release_transform(void *_ud);
