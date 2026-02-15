//
//  lua_url.hpp
//  Cubzh
//
//  Created by Xavier Legland on 05/24/2022.
//  Copyright 2020 Voxowl Inc. All rights reserved.
//

#pragma once

#include <stddef.h>

typedef struct lua_State lua_State;

/// Creates and pushes a new "URL" table onto the Lua stack
bool lua_g_url_create_and_push(lua_State *L);

/// Lua print function for global URL
int lua_g_url_snprintf(lua_State *L, char *result, size_t maxLen,
                        bool spacePrefix);
