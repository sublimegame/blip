//
//  scripting.hpp
//  Cubzh
//
//  Created by Gaetan de Villele on 12/06/2019.
//  Copyright © 2019 voxowl. All rights reserved.
//

#pragma once

#include "game.h"

// C++
#include <cstdbool>
#include <cstdint>
#include <string>
#include <memory>
#include <vector>

typedef struct lua_State lua_State;

namespace vx {
class Game;
typedef std::shared_ptr<Game> Game_SharedPtr;
typedef std::weak_ptr<Game> Game_WeakPtr;
}


#define PC_LUA_TABLE_TYPE_EVENT "Event"
#define PC_LUA_TABLE_TYPE_EVENT_PLAYER "Event.Player"

/// returns whether the operation has succeeded
bool scripting_loadScript(vx::Game *g,
                          lua_State **L);

// --------------------------------------------------
//
// MARK: - C - Lua interface
//
// --------------------------------------------------

/// Adds new Player to the Lua environment
/// This is called for all players, including the local player.
bool scripting_addPlayer(vx::Game* g, 
                         const uint8_t playerID,
                         const std::string &username,
                         const std::string &userID,
                         bool isLocalPlayer);

/// Sends an OnPlayerJoin event
/// This is called for all players, including the local player.
bool scripting_onPlayerJoin(vx::Game_SharedPtr &g, const uint8_t playerID);

/// Loads world and calls Client.OnStart Lua function, if found
void scripting_loadWorld_onStart(vx::Game* g);

/// Calls Client.DidReceiveEvent Lua function, if found
void scripting_client_didReceiveEvent(CGame * const g,
                                      const uint32_t eventType,
                                      const uint8_t senderID,
                                      const DoublyLinkedListUint8 *recipients,
                                      const uint8_t *data,
                                      const size_t size);

void scripting_client_didReceiveChatMessage(const vx::Game_SharedPtr& g,
                                            const uint8_t& senderID,
                                            const std::string& senderUserID,
                                            const std::string& senderUsername,
                                            const std::string& localUUID,
                                            const std::string& uuid,
                                            const std::vector<uint8_t>& recipients,
                                            const std::string& chatMessage);

void scripting_client_didReceiveChatMessageACK(const vx::Game_SharedPtr& g,
                                               const std::string& localUUID,
                                               const std::string& uuid,
                                               const uint8_t& status);

// ! \\ should only be called from game_tick
void scripting_do_string_called_from_game(vx::Game *g, const char* luaCode);

///
void scripting_enableHTTP(const vx::Game_SharedPtr& g);

///
void scripting_tick(vx::Game* g, CGame *cgame, const TICK_DELTA_SEC_T dt);
void scripting_object_tick(CGame *g, Transform *t, const TICK_DELTA_SEC_T dt);

void scripting_object_onCollision(CGame *g, Transform *self, Transform *other, float3 wNormal, uint8_t type);
void scripting_object_onDestroy(const uint16_t id, void *managed);
