//
//  lua_default_colors.hpp
//  Cubzh
//
//  Created by Mina Pecheux on 8/16/22.
//

#pragma once

typedef struct lua_State lua_State;

/// Pushes DefaultColors table on the Lua stack
void lua_default_colors_create_and_push(lua_State *L);
