// -------------------------------------------------------------
//  Cubzh
//  lua_font.hpp
//  Created by Arthur Cormerais on April 24, 2024.
// -------------------------------------------------------------

#pragma once

#include <stdint.h>

#include "lua.hpp"
#include "VXFont.hpp"

using namespace vx;

#define LUA_FONT_VALUE "font"

bool lua_g_font_createAndPush(lua_State *L);
bool lua_font_pushTable(lua_State *L, FontName font);
FontName lua_font_get(lua_State *L, int idx);
int lua_font_snprintf(lua_State *L, char *result, size_t maxLen, bool spacePrefix);
