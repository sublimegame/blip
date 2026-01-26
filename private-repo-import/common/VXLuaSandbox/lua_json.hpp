//
//  lua_json.hpp
//  Cubzh
//
//  Created by Corentin Cailleaud on 05/06/2022.
//  Copyright � 2020 Voxowl Inc. All rights reserved.
//

#pragma once

typedef struct lua_State lua_State;

#include <sstream>

/// Creates and pushes a new "JSON" table onto the Lua stack
bool lua_g_json_create_and_push(lua_State *L);

/// <summary>
///  Encodes a Lua table into a JSON string
/// </summary>
/// <param name="encoded"></param>
/// <param name="L"></param>
/// <param name="tableIndex"></param>
void lua_table_to_json_string(std::ostringstream &encoded, lua_State * const L, const int tableIndex);

/// Lua print function for global json
int lua_g_json_snprintf(lua_State *L, char *result, size_t maxLen, bool spacePrefix);
