//
//  lua_require.hpp
//  Cubzh
//
//  Created by Corentin Cailleaud on 10/08/2022.
//  Copyright � 2020 Voxowl Inc. All rights reserved.
//

#pragma once

// C++
#include <cstddef>
#include <string>
#include <vector>

#include "game.h"

typedef struct lua_State lua_State;

void lua_g_require_create_and_push(lua_State *const L);

bool lua_require(lua_State *const L, const char* moduleName);

void lua_require_with_script(lua_State *const L, std::vector<std::string> names, const std::string& script);

int lua_require_snprintf(lua_State *const L, char *result, size_t maxLen, bool spacePrefix);

bool lua_require_verify_local_script(const std::string &script, const std::string &name);
