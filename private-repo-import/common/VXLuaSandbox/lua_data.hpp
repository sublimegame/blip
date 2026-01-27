//
//  lua_data.hpp
//  Cubzh
//
//  Created by Xavier Legland on 17/03/2022.
//  Copyright 2020 Voxowl Inc. All rights reserved.
//

#pragma once

// C++
#include <string>

typedef struct lua_State lua_State;

/// Creates and pushes global "Data" table onto the Lua stack
bool lua_g_data_createAndPush(lua_State *const L);

/// Creates and pushes a new "Data" instance onto the Lua stack
bool lua_data_pushNewTable(lua_State * const L,
                           const void *data = nullptr,
                           const size_t len = 0);

///
std::string lua_data_getDataAsString(lua_State * const L,
                                     const int idx,
                                     const std::string& format = "utf-8");

///
const void *lua_data_getBuffer(lua_State * const L,
                               const int idx,
                               size_t *len);

/// Lua print function for Data instance
int lua_data_snprintf(lua_State * const L,
                      char *result,
                      size_t maxLen,
                      bool spacePrefix);
