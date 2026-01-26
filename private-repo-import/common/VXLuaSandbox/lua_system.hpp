//
//  lua_system.hpp
//  Cubzh
//
//  Created by Corentin Cailleaud on 24/05/2023.
//

#pragma once

#include "lua.hpp"

// Creates tables used to create sandboxed environments:
// - "root globals": APIs available anywhere
// - "system globals": "root globals" wrapper (read-only), with the addition of `System` table
void lua_init_root_and_system_globals(lua_State * const L);

// Replaces globals in given state by a new table, exposing "root globals" in read-only
void lua_sandbox_globals(lua_State * const L);

// Replaces globals in given states by a new table, exposing "system globals" in read-only
void lua_sandbox_system_globals(lua_State * const L);

// Pushes System table, the one referenced within "system globals"
void lua_system_table_push(lua_State * const L);
