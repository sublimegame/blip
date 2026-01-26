//
//  scripting_server.hpp
//  gameserver
//
//  Created by Gaetan de Villele on 12/10/2020.
//

#pragma once

#include <string>
#include <stdint.h>
#include <memory>

#include "game.h"

namespace vx {
class Game;
typedef std::shared_ptr<Game> Game_SharedPtr;
typedef std::weak_ptr<Game> Game_WeakPtr;
}

///
void scriptingServer_didReceiveEvent(const vx::Game_SharedPtr &g,
                                     const uint32_t eventType,
                                     const uint8_t senderID,
                                     const DoublyLinkedListUint8 *recipients,
                                     const uint8_t *data,
                                     const size_t size);

///
void scriptingServer_onStart(vx::Game *g);
