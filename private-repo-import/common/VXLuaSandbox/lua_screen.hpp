//
//  lua_screen.hpp
//  Cubzh
//
//  Created by Adrien Duermael on 14/11/2021.
//

#pragma once

#define LUA_SCREEN_FIELD_DID_RESIZE  "DidResize" // function(width, height)

typedef struct lua_State lua_State;

void lua_g_screen_create_and_push(lua_State * const L);
