//
//  lua_palette.hpp
//  Cubzh
//
//  Created by Adrien Duermael on 7/29/20.
//

#pragma once

#include <cstddef>

#include "shape.h"

typedef struct lua_State lua_State;

bool lua_g_palette_create_and_push(lua_State * const L);
int lua_g_palette_snprintf(lua_State * const L, char *result, const size_t maxLen, const bool spacePrefix);

bool lua_palette_create_and_push_table(lua_State * const L, ColorPalette *palette, Shape *optionalShape);
int lua_palette_snprintf(lua_State *L, char* result, size_t maxLen, bool spacePrefix);
ColorPalette *lua_palette_getCPointer(lua_State *L, const int idx, Shape **optionalShape);
