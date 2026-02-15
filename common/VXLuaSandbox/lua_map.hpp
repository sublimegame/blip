//
//  sbs_luaMap.h
//  sbs
//
//  Created by Gaetan de Villele on 26/08/2019.
//  Copyright © 2019 Voxowl Inc. All rights reserved.
//

#pragma once

typedef struct lua_State lua_State;
typedef struct _CGame CGame;

// C++
#include <cstddef>

#include "game.h"

// Type of function to save the Map's shape on the Hub
typedef bool (*MapPrivateSaveFuncPtr)(CGame *g, const char *itemID, const char *itemRepo, const char *itemName, const char *itemTag);

void lua_g_map_setFuncs(MapPrivateSaveFuncPtr mapPrivateSaveFunc);

/// Creates global Map table fields w/ given map
bool lua_g_map_create_and_push(lua_State * const L, Shape *mapShape);

/// Prints Map instance
int lua_map_snprintf(lua_State *L,
                     char *result,
                     size_t maxLen,
                     bool spacePrefix);
