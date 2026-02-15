//
//  lua_block.hpp
//  Cubzh
//
//  Created by Gaetan de Villele on 12/12/2019.
//  Copyright © 2019 Voxowl Inc. All rights reserved.
//

#pragma once

// C++
#include <cstdbool>

// Engine
#include "block.h"

typedef struct lua_State lua_State;
typedef struct _CGame CGame;
typedef struct _Shape Shape;
typedef struct _Scene Scene;

//
void lua_g_block_create_and_push(lua_State * const L);

// Pushes the block found at (x,y,z) coords within parentShape.
// Pushes nil if no block can be found.
// parentShape can't be NULL when calling this.
void lua_block_pushBlock(lua_State * const L,
                         Shape *parentShape,
                         const SHAPE_COORDS_INT_T x,
                         const SHAPE_COORDS_INT_T y,
                         const SHAPE_COORDS_INT_T z);

// Adds block in shape and pushes a boolean
void lua_block_addBlockInShapeAndPush(lua_State * const L,
                                      Shape * const parentShape,
                                      const SHAPE_COLOR_INDEX_INT_T luaPaletteIndex,
                                      const SHAPE_COORDS_INT_T x,
                                      const SHAPE_COORDS_INT_T y,
                                      const SHAPE_COORDS_INT_T z);

/// Should be called with a block at the top of the stack,
/// returns description string for it.
int lua_block_snprintf(lua_State * const L,
                       char* result,
                       size_t maxLen,
                       bool spacePrefix);

// Returns lua Block information.
void lua_block_getInfo(lua_State * const L, const int idx,
                       Shape **parentShape,
                       SHAPE_COORDS_INT_T *x,
                       SHAPE_COORDS_INT_T *y,
                       SHAPE_COORDS_INT_T *z,
                       SHAPE_COLOR_INDEX_INT_T *luaPaletteIndex,
                       RGBAColor *color=nullptr);
