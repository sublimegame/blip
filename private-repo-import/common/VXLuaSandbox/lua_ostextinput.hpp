//
//  lua_ostextinput.hpp
//  Blip
//
//  Created by Adrien Duermael on 10/04/2024.
//

#pragma once

// Lua
#include "lua.hpp"

/// Creates a new "OSTextInput" table and pushes it onto the Lua stack.
bool lua_g_ostextinputs_pushNewTable(lua_State *L);
