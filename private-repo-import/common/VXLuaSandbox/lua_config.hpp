//
//  lua_config.hpp
//  Cubzh
//
//  Created by Gaetan de Villele on 16/07/2019.
//  Copyright © 2019 voxowl. All rights reserved.
//

#pragma once

#define LUA_GLOBAL_FIELD_CONFIG "Config"
#define LUA_GLOBAL_FIELD_ITEMS "Items"

typedef struct lua_State lua_State;

/**
 Creates and pushes default Config instance to the top of the stack
 The unique Config instance is stored in the global metatable for __newindex
 to be called when trying to set it like this:
 Config = { Map = "some_user.some_map" }
 */
void lua_config_create_default_and_push(lua_State * const L);

/**
 Sets Config (unique instance) using table at idx.
 */
void lua_config_set_from_table(lua_State *L, const int tableIdx);

// pushes Items on top of the stack
bool lua_config_push_items(lua_State *L);
