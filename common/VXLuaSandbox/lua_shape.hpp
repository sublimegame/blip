//
//  lua_shape.hpp
//  Cubzh
//
//  Created by Gaetan de Villele on 06/08/2020.
//

#pragma once

// C++
#include <cstddef>

// engine
#include "float3.h"

typedef struct _CGame CGame;
typedef struct _Shape Shape;
typedef struct _Transform Transform;
typedef struct lua_State lua_State;
typedef struct number3_C_handler_userdata number3_C_handler_userdata;
namespace vx { class UserDataStore; }

typedef struct shape_userdata {
    // Userdata instance requirements
    vx::UserDataStore *store;
    shape_userdata *next;
    shape_userdata *previous;

    Shape *shape;
} shape_userdata;

void lua_shape_release_transform(void *_ud);
void lua_shape_gc(void *_ud);

// --------------------------------------------------
// MARK: - Shape global table -
// --------------------------------------------------

/// Creates needed metatables and store them in the Lua registry
/// @param L the Lua state
void lua_g_shape_createAndPush(lua_State * const L);

/// Lua print function for global Shape table
int lua_g_shape_snprintf(lua_State * const L,
                         char *result,
                         size_t maxLen,
                         bool spacePrefix);

// --------------------------------------------------
// MARK: - Shape instances tables -
// --------------------------------------------------

/// This should only be called by lua Shape constructor, use lua_g_object_getIndexEntry for existing lua Shapes
bool lua_shape_pushNewInstance(lua_State * const L, Shape *s);

bool lua_shape_isShape(lua_State * const L, const int idx);
bool lua_shape_isShapeType(const int type);

///
Shape *lua_shape_getShapePtr(lua_State * const L, const int idx);

/// Lua print function for Shape instance tables
int lua_shape_snprintf(lua_State * const L,
                       char *result,
                       size_t maxLen,
                       bool spacePrefix);

// --------------------------------------------------
// MARK: - Internal -
// Used by other lua Object types that extend lua Shape
// --------------------------------------------------

void _lua_shape_push_fields(lua_State * const L, Shape *s);
bool _lua_shape_metatable_index(lua_State * const L, Shape *s, const char *key);
bool _lua_shape_metatable_newindex(lua_State * const L, Shape *s, const char *key);
typedef enum {
    ShapeCopyMutable_Keep,
    ShapeCopyMutable_Enable,
    ShapeCopyMutable_Disable
} ShapeCopyMutable;
void _lua_shape_copy_and_push(lua_State * const L, const int idx, const ShapeCopyMutable copyMutable, const bool includeChildren);
void _lua_shape_load_or_compute_baked_light(Shape *s);

