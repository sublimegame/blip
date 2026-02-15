//
//  lua_player.hpp
//  Cubzh
//
//  Created by guest on 14/08/2019.
//  Copyright © 2019 Voxowl Inc. All rights reserved.
//

#pragma once

// engine
#include "game.h"

typedef struct lua_State lua_State;

/// Creates a Lua Player object and pushes it onto the Lua stack.
/// Careful, in theory, this should only be used when a new player joins
/// the game, to insert it in the Players table.
/// One player should only be represented by one Player instance.
/// For a Player that's already been added, use lua_player_pushPlayerTable instead.
bool lua_player_createAndPushPlayerTable(lua_State * const L,
                                         const uint8_t playerId,
                                         const char *playerUsername,
                                         const char *playerUserID,
                                         const bool isLocalPlayer);

/// Gets the Id field value of the Player Lua object located at the index <idx>.
/// Returns whether the operation has succeeded.
bool lua_player_getIdFromLuaPlayer(lua_State * const L,
                                   const int idx,
                                   uint8_t * const playerIDPtr);

///
int lua_player_snprintf(lua_State *L,
                        char* result,
                        size_t maxLen,
                        bool spacePrefix);
