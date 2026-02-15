//
//  lua_mutableShape.hpp
//  Cubzh
//
//  Created by Gaetan de Villele on 23/02/2021.
//

#pragma once

// C++
#include <cstddef>

#include "shape.h"

typedef struct lua_State lua_State;
typedef struct _Transform Transform;
typedef struct _CGame CGame;

// --------------------------------------------------
//
// MutableShape global table
//
// --------------------------------------------------

/// Creates the MutableShape global table
/// @param L the Lua state
void lua_g_mutableShape_createAndPush(lua_State * const L);

/// Lua print function for MutableShape global table
/// @param L the Lua state
int lua_g_mutableShape_snprintf(lua_State *L, char *result, size_t maxLen, bool spacePrefix);



// --------------------------------------------------
//
// MutableShape instance tables
//
// --------------------------------------------------

/// This should only be called by lua MutableShape constructor, use lua_g_object_getIndexEntry
/// for existing lua MutableShapes
bool lua_mutableShape_pushNewInstance(lua_State *L, Shape *s);
bool lua_mutableShape_isMutableShape(lua_State *L, const int idx);
bool lua_mutableShape_isMutableShapeType(const int type);

int lua_mutableShape_snprintf(lua_State *L, char *result, size_t maxLen, bool spacePrefix);

// MARK: - Internal -

// Used by other lua objects that have a MutableShape component
void _lua_mutableShape_push_fields(lua_State *L, Shape *s);
void _lua_mutableShape_update_fields(lua_State *L, Shape *s);
bool _lua_mutableShape_metatable_index(lua_State *L, Shape *s, const char *key);
bool _lua_mutableShape_metatable_newindex(lua_State *L, Shape *s, const char *key);
