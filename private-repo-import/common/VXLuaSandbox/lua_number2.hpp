// -------------------------------------------------------------
//  Cubzh
//  lua_number2.hpp
//  Created by Arthur Cormerais on October 25, 2022.
// -------------------------------------------------------------

#pragma once

#include <cmath>

#include <stddef.h>
#include <stdint.h>
#include <math.h>

typedef struct lua_State lua_State;

typedef struct number2_C_handler_userdata {
    uint8_t uint8_value;
    void *ptr;
} number2_C_handler_userdata;

const number2_C_handler_userdata number2_C_handler_userdata_zero = {0, nullptr};

typedef void (*number2_C_handler_set) (const float *x,
                                       const float *y,
                                       lua_State *L,
                                       const number2_C_handler_userdata *userdata);

typedef void (*number2_C_handler_get) (float *x,
                                       float *y,
                                       lua_State *L,
                                       const number2_C_handler_userdata *userdata);

void lua_g_number2_createAndPush(lua_State *L);
bool lua_number2_pushNewObject(lua_State *L, const float x, const float y);
bool lua_number2_setHandlers(lua_State *L,
                             const int idx,
                             number2_C_handler_set set,
                             number2_C_handler_get get,
                             number2_C_handler_userdata handler_userdata,
                             bool isUserdataWeakptr);
bool lua_number2_or_table_getXY(lua_State *L, const int idx, float *x, float *y);
bool lua_number2_or_table_or_numbers_getXY(lua_State *L, const int idx, float *x, float *y);
bool lua_number2_setXY(lua_State *L, const int idx, const float *x, const float *y);
bool lua_number2_getXY(lua_State *L, const int idx, float *x, float *y);
int lua_g_number2_snprintf(lua_State *L, char* result, size_t maxLen, bool spacePrefix);
int lua_number2_snprintf(lua_State *L, char* result, size_t maxLen, bool spacePrefix);
