//
//  lua_environment.hpp
//  Cubzh
//
//  Created by Corentin Cailleaud on /06/2022.
//  Copyright � 2020 Voxowl Inc. All rights reserved.
//

#pragma once

#include <unordered_map>
#include <string>

typedef struct lua_State lua_State;

/// Creates and pushes a new "Environment" table onto the Lua stack
bool lua_g_environment_create_and_push(lua_State *L);

/// Lua print function for global json
int lua_g_environment_snprintf(lua_State *L, char *result, size_t maxLen, bool spacePrefix);
