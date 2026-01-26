//
//  lua_players.h
//  Cubzh
//
//  Created by guest on 03/07/2019.
//  Copyright © 2019 voxowl. All rights reserved.
//

#pragma once

// C++
#include <cstdint>
#include <string>
#include <vector>

typedef struct lua_State lua_State;



// --------------------------------------------------
//
// MARK: - OtherPlayers -
//
// --------------------------------------------------

/// Creates and pushes a new "OtherPlayers" table onto the Lua stack
void lua_g_otherPlayers_createAndPush(lua_State * const L);

/// Retrieves the "OtherPlayers" global table and pushes it onto the Lua stack.
/// Returns whether the operation has succeeded.
/// If the operation succeeds, the OtherPlayers table has been pushed onto the
/// Lua stack, otherwise nothing is pushed on the stack.
bool lua_g_otherPlayers_push(lua_State * const L);

///
bool lua_g_otherPlayers_insertPlayerTable(lua_State * const L,
                                          const int luaPlayerIdx);

///
bool lua_g_otherPlayers_removePlayerTable(lua_State * const L,
                                          const uint8_t playerID);



// --------------------------------------------------
//
// MARK: - Players -
//
// --------------------------------------------------

/// Creates and pushes a new "Players" table onto the Lua stack
void lua_g_players_createAndPush(lua_State * const L);

/// Retrieves the "Players" global table and pushes it onto the Lua stack.
/// Returns whether the operation has succeeded.
/// If the operation succeeds, the Players table has been pushed onto the
/// Lua stack, otherwise nothing is pushed on the stack.
bool lua_g_players_push(lua_State * const L);

///
bool lua_g_players_pushPlayerTable(lua_State * const L,
                                   const uint8_t playerID);

/// inserts the player object located at index <idx> in the global Players table
bool lua_g_players_insertPlayerTable(lua_State * const L,
                                     const int luaPlayerIdx);

///
bool lua_g_players_removePlayerTable(lua_State * const L,
                                     const uint8_t playerID);

///
bool lua_g_players_updatePlayer(lua_State * const L,
                                const uint8_t currentID,
                                const uint8_t *newID,
                                const std::string *username,
                                const std::string *userid);

std::vector<uint8_t> lua_g_players_getPlayerIDs(lua_State * const L);
