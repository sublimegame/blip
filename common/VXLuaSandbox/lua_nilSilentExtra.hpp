//
//  lua_nilSilentExtra.hpp
//  Cubzh
//
//  Created by Gaetan de Villele on 16/11/2019.
//  Copyright � 2019 Voxowl Inc. All rights reserved.
//

#pragma once


typedef struct lua_State lua_State;

/// Creates the extra silent nil table in the Lua registry
void lua_nilSilentExtra_setupRegistry(lua_State* L);

/// Pushes the nil silent extra table onto the Lua stack
bool lua_nilSilentExtra_push(lua_State* L);
