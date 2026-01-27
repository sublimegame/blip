//
//  lua_timer.hpp
//  Cubzh
//
//  Created by Xavier Legland on 10/27/2021.
//  Copyright © 2020 Voxowl Inc. All rights reserved.
//

#pragma once

typedef struct lua_State lua_State;

/// Creates and pushes a new "Timer" table onto the Lua stack
bool lua_g_timer_create_and_push(lua_State * const L);
