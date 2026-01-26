// -------------------------------------------------------------
//  Cubzh
//  lua_number3.hpp
//  Created by Gaetan de Villele on July 24, 2020.
// -------------------------------------------------------------

#pragma once

#include <cmath>

#include <stddef.h>
#include <stdint.h>
#include <math.h>

typedef struct lua_State lua_State;

// Handler that can be set to get/set
// engine values instead of Lua state ones.

typedef struct number3_C_handler_userdata {
    uint8_t uint8_value;
    void *ptr;
} number3_C_handler_userdata;

const number3_C_handler_userdata number3_C_handler_userdata_zero = {0, nullptr};

typedef void (*number3_C_handler_set) (const float *x,
                                       const float *y,
                                       const float *z,
                                       lua_State *L,
                                       const number3_C_handler_userdata *userdata);

typedef void (*number3_C_handler_get) (float *x,
                                       float *y,
                                       float *z,
                                       lua_State *L,
                                       const number3_C_handler_userdata *userdata);

/// Creates and pushed global Number3 table
void lua_g_number3_create_and_push(lua_State *L);

///
bool lua_number3_pushNewObject(lua_State *L,
                               const float x,
                               const float y,
                               const float z);

///
bool lua_number3_setHandlers(lua_State *L,
                             const int idx,
                             number3_C_handler_set set,
                             number3_C_handler_get get,
                             number3_C_handler_userdata handler_userdata,
                             bool isUserdataWeakptr);

/// Returns true if there is a Number3 or a table of 3 numbers at idx
bool lua_number3_or_table_getXYZ(lua_State *L,
                                 const int idx,
                                 float *x,
                                 float *y,
                                 float *z);

/// Returns true if a Number3, table of 3 numbers, or 3 individual numbers can be found starting at idx
bool lua_number3_or_table_or_numbers_getXYZ(lua_State *L,
                                            const int idx,
                                            float *x,
                                            float *y,
                                            float *z);

///
bool lua_number3_setXYZ(lua_State *L,
                        const int idx,
                        const float *x,
                        const float *y,
                        const float *z);

///
bool lua_number3_getXYZ(lua_State *L,
                        const int idx,
                        float *x,
                        float *y,
                        float *z);

/// Prints global Number3
int lua_g_number3_snprintf(lua_State *L,
                           char* result,
                           size_t maxLen,
                           bool spacePrefix);

/// Prints Number3 instance
int lua_number3_snprintf(lua_State *L,
                         char* result,
                         size_t maxLen,
                         bool spacePrefix);
