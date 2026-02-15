//
//  lua_nil.h
//  Cubzh
//
//  Created by Gaetan de Villele on 29/07/2019.
//  Copyright © 2019 voxowl. All rights reserved.
//

#pragma once

typedef struct lua_State lua_State;

/// Creates the Magic Empty Table in the Lua registry
void lua_nil_setupRegistry(lua_State *L);
