//
//  lua_server.hpp
//  Cubzh
//
//  Created by guest on 14/08/2019.
//  Copyright © 2019 Voxowl Inc. All rights reserved.
//

#pragma once

// C++
#include <cstddef>

// Properties
#define LUA_SERVER_FIELD_DIDRECEIVEEVENT     "DidReceiveEvent" // function (R/W)
#define LUA_SERVER_FIELD_TICK                "Tick"            // function (R/W)
#define LUA_SERVER_FIELD_ONSTART             "OnStart"         // function (R/W)

// Not documented yet
#define LUA_SERVER_FIELD_ONPLAYERJOIN        "OnPlayerJoin"    // function (R/W)
#define LUA_SERVER_FIELD_ONPLAYERLEAVE       "OnPlayerLeave"   // function (R/W)



typedef struct lua_State lua_State;

/// Defines "Server" global table for server environment.
void lua_server_createAndPushForServer(lua_State * const L);

///
void lua_server_createAndPushForClient(lua_State * const L);

/// Pushes the global Server table onto the stack.
bool lua_server_pushTable(lua_State * const L);

///
bool lua_server_pushOnStartFunction(lua_State * const L);

/// Should be called with a Server at the top of the stack, returns description string for it.
int lua_server_snprintf(lua_State * const L,
                        char* result,
                        size_t maxLen,
                        bool spacePrefix);
