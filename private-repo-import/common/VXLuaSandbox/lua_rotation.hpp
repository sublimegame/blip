// -------------------------------------------------------------
//  Cubzh
//  lua_rotation.hpp
//  Created by Arthur Cormerais on August 3, 2023.
// -------------------------------------------------------------

#pragma once

#include <cmath>

#include <stddef.h>
#include <stdint.h>
#include <math.h>

#include "quaternion.h"

typedef struct lua_State lua_State;

typedef struct rotation_handler_userdata {
    void *ptr;
} rotation_handler_userdata;

const rotation_handler_userdata rotation_handler_userdata_zero = { nullptr };

typedef void (*rotation_handler_set) (Quaternion *q, lua_State *L, const rotation_handler_userdata *userdata);

typedef void (*rotation_handler_get) (Quaternion *q, lua_State *L, const rotation_handler_userdata *userdata);

void lua_g_rotation_create_and_push(lua_State *L);
bool lua_rotation_push_new(lua_State *L, Quaternion q);
bool lua_rotation_push_new_euler(lua_State *L, float x, float y, float z);
bool lua_rotation_set_handlers(lua_State *L, int idx,
                               rotation_handler_set set,
                               rotation_handler_get get,
                               rotation_handler_userdata userdata,
                               bool isUserdataWeakptr);
bool lua_rotation_set(lua_State *L, int idx, Quaternion *q);
bool lua_rotation_get_euler(lua_State *L, int idx, float *x, float *y, float *z);
bool lua_rotation_get(lua_State *L, int idx, Quaternion *q);
bool lua_rotation_set_euler(lua_State *L, int idx, float *x, float *y, float *z);
bool lua_rotation_or_table_get(lua_State *L, int idx, Quaternion *q);
bool lua_rotation_or_table_get_euler(lua_State *L, int idx, float *x, float *y, float *z);
int lua_g_rotation_snprintf(lua_State *L, char* result, size_t maxLen, bool spacePrefix);
int lua_rotation_snprintf(lua_State *L, char* result, size_t maxLen, bool spacePrefix);
