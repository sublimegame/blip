//
//  lua_impact.hpp
//  Cubzh
//
//  Created by Gaetan de Villele on 01/10/2019.
//  Copyright © 2019 voxowl. All rights reserved.
//

#pragma once

typedef struct lua_State lua_State;

// engine
#include "block.h"
#include "shape.h"
#include "game.h"
#include "scene.h"

bool lua_impact_push(lua_State *L, CastResult hit);

/// Returns whether the operation succeeded
bool lua_impact_pushImpactTableWithBlock(lua_State *L,
                                         Shape *blockParentShape,
                                         SHAPE_COORDS_INT_T positionX,
                                         SHAPE_COORDS_INT_T positionY,
                                         SHAPE_COORDS_INT_T positionZ,
                                         const float distance,
                                         const FACE_INDEX_INT_T faceTouched);

/// Returns whether the operation succeeded
bool lua_impact_pushImpactTableWithObject(lua_State *L,
                                          Transform *t,
                                          const float distance);

int lua_impact_snprintf(lua_State *L, char *result, size_t maxLen, bool spacePrefix);
