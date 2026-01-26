//
//  lua_box.hpp
//  Cubzh
//
//  Created by Xavier Legland on 24/09/2021.
//  Copyright © 2020 Voxowl Inc. All rights reserved.
//

#pragma once

#include "box.h"

#include "lua_number3.hpp"

// C++
#include <cstddef>

// engine
#include "weakptr.h"

typedef struct lua_State lua_State;

typedef void (*box_C_handler_set)(const float3 *min,
                                  const float3 *max,
                                  lua_State *L, void *userdata);

typedef void (*box_C_handler_get)(float3 *min,
                                  float3 *max,
                                  lua_State *L, const void *userdata);

typedef void (*box_C_handler_free)(void *userdata);

// Default handler
typedef struct {
    box_C_handler_set set;
    box_C_handler_get get;
    box_C_handler_free free_userdata;
    void *userdata;
    Weakptr* wptr;
} box_C_handler;

/// Creates and pushes a new "Box" class table onto the Lua stack
bool lua_g_box_create_and_push(lua_State *L);

/// Pushes a new lua Box object onto the stack
void lua_box_pushNewTable(lua_State * const L,
                          void * const userdata,
                          box_C_handler_set setFunction,
                          box_C_handler_get getFunction,
                          box_C_handler_free freeFunction,
                          number3_C_handler_set n3MinSetFunction,
                          number3_C_handler_get n3MinGetFunction,
                          number3_C_handler_set n3MaxSetFunction,
                          number3_C_handler_get n3MaxGetFunction);

void lua_box_standalone_pushNewTable(lua_State * const L,
                                     const float3 * const min,
                                     const float3 * const max,
                                     const bool& readonly);

///
box_C_handler *lua_box_getCHandler(lua_State *L,
                                   const int luaBoxIdx);

bool lua_box_get_box(lua_State *L,
                     const int luaBoxIdx,
                     Box *box);

/// Prints global Box
int lua_g_box_snprintf(lua_State *L,
                       char *result,
                       size_t maxLen,
                       bool spacePrefix);

/// Prints Box instance
int lua_box_snprintf(lua_State *L,
                     char *result,
                     size_t maxLen,
                     bool spacePrefix);
