// -------------------------------------------------------------
//  Cubzh
//  lua_mesh.hpp
//  Created by Arthur Cormerais on February 14, 2025.
// -------------------------------------------------------------

#pragma once

#include "lua.hpp"
#include "mesh.h"

#define LUA_MESH_METAFIELD_PTR "mesh_ptr"

// MARK: - Mesh global table -

void lua_g_mesh_createAndPush(lua_State * const L);
int lua_g_mesh_snprintf(lua_State *L, char *result, size_t maxLen, bool spacePrefix);

// MARK: - Mesh instances tables -

bool lua_mesh_pushNewInstance(lua_State *L, Mesh *m);
bool lua_mesh_getOrCreateInstance(lua_State * const L, Mesh *m);
bool lua_mesh_isMesh(lua_State * const L, const int idx);
Mesh *lua_mesh_getMeshPtr(lua_State * const L, const int idx);
int lua_mesh_snprintf(lua_State *L, char *result, size_t maxLen, bool spacePrefix);

// MARK: - Internal -
// Used by other lua Object types that extend lua Mesh

bool _lua_mesh_metatable_index(lua_State * const L, Mesh *m, const char *key);
bool _lua_mesh_metatable_newindex(lua_State * const L, Mesh *m, const char *key);

void lua_mesh_release_transform(void *_ud);