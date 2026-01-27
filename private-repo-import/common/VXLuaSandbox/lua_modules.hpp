//
//  lua_modules.hpp
//  Cubzh
//
//  Created by Adrien Duermael on 05/01/2024.
//  Copyright © 2024 Voxowl Inc. All rights reserved.
//

#pragma once

typedef struct lua_State lua_State;

// Creates and pushes a Modules table
void lua_modules_createAndPush(lua_State * const L);

// sets modules from table at idx
void lua_modules_set_from_table(lua_State *L, const int idx);

// makes Modules table read-only
void lua_modules_make_read_only(lua_State *L, const int idx);
