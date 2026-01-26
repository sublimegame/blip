//
//  lua_pointer_event.hpp
//  Cubzh
//
//  Created by Xavier Legland on 7/29/20.
//

#pragma once

#include "float3.h"
#include <stdint.h>

typedef struct lua_State lua_State;

/// Creates and pushes a new "PointerEvent" table onto the Lua stack
bool lua_g_pointer_event_create_and_push(lua_State *L);

/// Creates the a pointer event, and adds it in the
/// registry Returns true on success, false otherwise.
void lua_pointer_event_create_and_push_table(lua_State *L,
                                             const float x, const float y,
                                             const float dx, const float dy,
                                             const float3 * const position,
                                             const float3 * const direction,
                                             const bool down,
                                             const uint8_t index);
