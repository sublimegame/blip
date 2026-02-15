// -------------------------------------------------------------
//  Cubzh
//  lua_textType.hpp
//  Created by Arthur Cormerais on October 13, 2022.
// -------------------------------------------------------------

#pragma once

#include <stdint.h>

#include "lua.hpp"
#include "world_text.h"

#define LUA_TEXTTYPE_VALUE "text_type"

bool lua_g_textType_createAndPush(lua_State *L);
bool lua_textType_pushTable(lua_State *L, TextType type);
TextType lua_textType_get(lua_State *L, int idx);
int lua_textType_snprintf(lua_State *L, char *result, size_t maxLen, bool spacePrefix);
