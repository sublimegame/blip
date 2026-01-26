//
//  lua_sandbox.hpp
//  Cubzh
//
//  Created by Adrien Duermael on 01/02/2020.
//  Copyright © 2020 Voxowl Inc. All rights reserved.
//

#pragma once

// C++
#include <unordered_map>
#include <string>

// vx
#include "VXGame.hpp"

typedef struct lua_State lua_State;
typedef struct _CGame CGame;

/// Sets up the Lua sandbox, for a client.
/// Called as soon as the Lua sandbox is created.
/// Before the script is parsed.
void lua_sandbox_client_setup_pre_load(lua_State *const L,
                                       const std::unordered_map<std::string,
                                       std::string> &environment);

/// Completes the client Lua sandbox before game start.
/// Called right after script parsing (Config loaded).
void lua_sandbox_client_setup_post_load(lua_State * const L, vx::Game *g);

/// Sets up the Lua sandbox for the gameserver.
/// Called as soon as the Lua sandbox is created.
/// Before the script is parsed.
void lua_sandbox_server_setup_pre_load(lua_State * const L);

/// Completes the server Lua sandbox before game start.
/// Called right after script parsing (Config loaded).
void lua_sandbox_server_setup_post_load(lua_State * const L, vx::Game *g);

/// Defines Lua shortcuts
void lua_sandbox_set_shortcuts_post_load(lua_State * const L, const bool isOnServer);
