//
//  lua_block_properties.hpp
//  Cubzh
//
//  Created by Adrien Duermael on 7/31/20.
//

#pragma once

#include <cstdint>

#include "lua_palette.hpp"

typedef struct lua_State lua_State;

/// Prints BlockProperties instance
int lua_block_properties_snprintf(lua_State * const L,
                                  char* result,
                                  const size_t maxLen,
                                  const bool spacePrefix);

// Creates BlockProperties table and pushes it on the top of the stack
// NOTE: it's not (yet?) possible to keep a weak reference of a palette.
// But each Shape has its own palette, so keeping a weak reference on the Shape instead for now.
void lua_block_properties_create_and_push_table(lua_State * const L,
                                                ColorPalette *palette,
                                                SHAPE_COLOR_INDEX_INT_T luaIdx);
