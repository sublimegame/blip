//
//  lua_file.hpp
//  Cubzh
//
//  Created by Xavier Legland on 06/07/2022.
//  Copyright 2020 Voxowl Inc. All rights reserved.
//

#pragma once

// C
#include "stdlib.h"

typedef struct lua_State lua_State;

/// Creates and pushes a new "File" table onto the Lua stack
bool lua_g_file_create_and_push(lua_State *L);

/// Lua print function for global File
int lua_g_file_snprintf(lua_State *L, char *result, size_t maxLen,
                        bool spacePrefix);
