// -------------------------------------------------------------
//  Cubzh
//  lua_text.hpp
//  Created by Arthur Cormerais on September 27, 2022.
// -------------------------------------------------------------

#pragma once

#include "lua.hpp"

#include "world_text.h"
#include "game.h"

// MARK: - Text global table -

void lua_g_text_createAndPush(lua_State * const L);
int lua_g_text_snprintf(lua_State *L, char *result, size_t maxLen, bool spacePrefix);

// MARK: - Text instances tables -

bool lua_text_pushNewInstance(lua_State *const L, WorldText *wt);
bool lua_text_getOrCreateInstance(lua_State * const L, WorldText *wt);
bool lua_text_isText(lua_State * const L, const int idx);
WorldText *lua_text_getWorldTextPtr(lua_State * const L, const int idx);
int lua_text_snprintf(lua_State *L, char *result, size_t maxLen, bool spacePrefix);

// MARK: - Internal -
// Used by other lua Object types that extend lua Text

void _lua_text_push_fields(lua_State * const L, WorldText *wt);
bool _lua_text_metatable_index(lua_State * const L, WorldText *wt, const char *key);
bool _lua_text_metatable_newindex(lua_State * const L, WorldText *wt, const char *key);

void lua_text_release_transform(void *_ud);
