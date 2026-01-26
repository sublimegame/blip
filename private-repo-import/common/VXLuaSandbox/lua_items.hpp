//
//  lua_items.hpp
//  Cubzh
//
//  Created by Gaetan de Villele on 21/11/2019.
//  Copyright © 2019 Voxowl Inc. All rights reserved.
//

#pragma once

#include <stddef.h>

typedef struct lua_State lua_State;

// Creates a "Items" table an pushes it.
// NOTE: only one instance is used -> Config.Items
// (Items is a shortcut to it)
bool lua_items_createAndPush(lua_State *L);

// Sets Items content what's at idx on the Lua stack.
// Accepted arguments:
// - array of strings --> Items = {"foo", "bar"}
bool lua_items_set_all(lua_State *L, const int idx);

//
bool lua_item_createAndPush(lua_State *L, const char* fullname, const int itemsIdx, const int reposIdx);

// The Map is like other items, but no need to index it.
// It is only stored in Config.Map.
bool lua_item_createAndPushMap(lua_State *L, const char* fullname);

/// If the lua value at index "idx" is an Item object, this function returns
/// its name ("aduermael.sword" for example), otherwise it returns NULL;
/// If the return value is not NULL, it must be freed by the caller.
const char* lua_item_getName(lua_State *L, const int idx);

///
int lua_item_snprintf(lua_State *L, char *result, size_t maxLen, bool spacePrefix);

///
int lua_items_snprintf(lua_State *L, char *result, size_t maxLen, bool spacePrefix);
