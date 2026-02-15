// -------------------------------------------------------------
//  Cubzh
//  lua_object.hpp
//  Created by Arthur Cormerais on March 12, 2021.
// -------------------------------------------------------------

#pragma once

// C
#include <cstddef>
#include <cstdint>

#include "config.h"
#include "float3.h"
#include "lua_constants.h"

#define LUA_OBJECT_FIELD_ONCOLLISIONBEGIN       "OnCollisionBegin"     // function
#define LUA_OBJECT_FIELD_ONCOLLISION            "OnCollision"          // function
#define LUA_OBJECT_FIELD_ONCOLLISIONEND         "OnCollisionEnd"       // function

typedef struct _CGame CGame;
typedef struct _Transform Transform;
typedef struct _Shape Shape;
typedef struct lua_State lua_State;
typedef struct number3_C_handler_userdata number3_C_handler_userdata;

// Sets root transform for given object, triggers Lua internal error on failure
#define GET_OBJECT_ROOT_TRANSFORM(idx,transformPtr) lua_object_getTransformPtr(L, idx, &transformPtr); \
if (transformPtr == nullptr) { \
    LUAUTILS_INTERNAL_ERROR(L); \
}

#define GET_OBJECT_ROOT_TRANSFORM_WITH_FIELD_ERROR(idx, transformPtr, errorFormat, fieldName) lua_object_getTransformPtr(L, idx, &transformPtr); \
if (transformPtr == nullptr) { \
    LUAUTILS_ERROR_F(L, errorFormat, fieldName); \
}

// --------------------------------------------------
// MARK: - Object global table -
// --------------------------------------------------

void lua_g_object_createAndPush(lua_State * const L);
int lua_g_object_snprintf(lua_State *L,
                         char *result,
                         size_t maxLen,
                         bool spacePrefix);
void lua_g_object_pushObjectsIndexedByTransformID(lua_State *L);
bool lua_g_object_addIndexEntry(lua_State *L, Transform *t, const int idx);
bool lua_g_object_removeIndexEntry(lua_State *L, Transform *t);
bool lua_g_object_getIndexEntry(lua_State *L, Transform *t);
bool lua_g_object_getOrCreateInstance(lua_State *L, Transform *t); // Pushes the relevant object type for t

// Storage for all Object Lua values

void lua_g_object_pushObjectStorage(lua_State *L);
// stores value at valueIdx for transformID (preserves value wherever it is on stack)
void lua_g_object_storage_set_field(lua_State *L, const uint16_t transformID, const char *name, const int valueIdx);
// pushes `nil` if not found
void lua_g_object_storage_get_field(lua_State *L, const uint16_t transformID, const char *name);

// --------------------------------------------------
// MARK: - Object instances tables -
// --------------------------------------------------

/// This should only be called by lua Object constructor, use lua_g_object_getIndexEntry for existing lua Objects
/// @return true if object was created, if so, transform is retained
bool lua_object_pushNewInstance(lua_State *L, Transform *t);

// Called after C userdata has been freed.
// Can be used to clean associated Lua metadata.
// Can be called with NULL lua state (after state has been closed)
namespace vx { class UserDataStore; }
void lua_object_gc_finalize(vx::UserDataStore *store, CGame *g, lua_State * const L);

bool lua_object_isObject(lua_State *L, const int idx);
bool lua_object_isObjectType(const int type);

// Returns the type of the Object along with its root transform.
// (the type can be ITEM_TYPE_OBJECT_DESTROYED or ITEM_TYPE_NOT_AN_OBJECT)
LUA_ITEM_TYPE lua_object_getTransformPtr(lua_State *L, const int idx, Transform **root);
int lua_object_snprintf(lua_State *L,
                       char *result,
                       size_t maxLen,
                       bool spacePrefix);
bool lua_object_get_and_call_collision_callback(lua_State *L, Transform *self, Transform *other, float3 wNormal, uint8_t type);
bool lua_object_call_tick(lua_State * const L, Transform *self, const TICK_DELTA_SEC_T dt);

// returns NULL on success, error message otherwise
const char* lua_object_copy(lua_State *L, const int objectIdx, const int configIdx);

// --------------------------------------------------
// MARK: - Internal -
// --------------------------------------------------

// Used by other lua objects that extend lua Object, providing their root transform as a shortcut
bool _lua_object_metatable_index(lua_State *L, const char *key);
bool _lua_object_metatable_newindex(lua_State *L, Transform *root, const char *key);

// Returns true if Object at -1 is read-only
// (using metatable at -1 if pushMetatable is false)
bool _lua_object_is_readonly(lua_State *L, bool pushMetatable);

// Makes Object at -1 read-only or not based on b parameter.
// (using metatable at -1 if pushMetatable is false)
void _lua_object_set_readonly(lua_State *L, bool b, bool pushMetatable);
