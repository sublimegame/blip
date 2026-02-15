//
//  lua_event.hpp
//  Cubzh
//
//  Created by Gaetan de Villele on 02/08/2019.
//  Copyright © 2019 voxowl. All rights reserved.
//

#pragma once

// C++
#include <cstdint>
#include <cstdlib>
#include <vector>
#include <string>

// engine
#include "doubly_linked_list_uint8.h"

typedef struct lua_State lua_State;

/// creates the "Event" global table
void lua_g_event_create_and_push(lua_State * const L);

/// creates an event Lua object having the provided attributes and push it onto
/// the Lua stack.
/// This is useful for calling Player.DidReceiveEvent(event) and
/// Server.DidReceiveEvent(event)
bool lua_event_create_and_push(lua_State * const L,
                               const uint32_t& eventType,
                               const uint8_t& senderId,
                               const DoublyLinkedListUint8 *recipients,
                               const uint8_t *data,
                               const size_t size);

/// Prints global Event
int lua_g_event_snprintf(lua_State *L,
                         char* result,
                         size_t maxLen,
                         bool spacePrefix);

/// Prints Event instance
int lua_event_snprintf(lua_State *L,
                       char* result,
                       size_t maxLen,
                       bool spacePrefix);

