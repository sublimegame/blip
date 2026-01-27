//
//  scripting_server.cpp
//  gameserver
//
//  Created by Gaetan de Villele on 12/10/2020.
//

#include "scripting_server.hpp"

// Lua
#include "lua.hpp"
#include "lua_utils.hpp"

// Sandbox
#include "sbs.hpp"
#include "lua_player.hpp"
#include "lua_players.hpp"
#include "lua_sandbox.hpp"
#include "lua_server.hpp"
#include "lua_logs.hpp"
#include "lua_event.hpp"
#include "lua_local_event.hpp"

// xptools
#include "xptools.h"
#include "VXGame.hpp"

// VXFramework
#include "VXLuaExecutionLimiter.hpp"

void scriptingServer_didReceiveEvent(const vx::Game_SharedPtr &g,
                                     const uint32_t eventType,
                                     const uint8_t senderID,
                                     const DoublyLinkedListUint8 *recipients,
                                     const uint8_t *data,
                                     const size_t size) {

    vx_assert(g != nullptr);
    vx_assert(recipients != nullptr);
    vx_assert(data != nullptr);

    lua_State * const L = g->getLuaState();
    vx_assert(L != nullptr);
    LUAUTILS_STACK_SIZE(L)

    LOCAL_EVENT_SEND_PUSH(L, LOCAL_EVENT_NAME_DidReceiveEvent)
    lua_event_create_and_push(L, eventType, senderID, recipients, data, size);
    LOCAL_EVENT_SEND_CALL(L, 1)

    LUAUTILS_STACK_SIZE_ASSERT(L, 0)
}

void scriptingServer_onStart(vx::Game *g) {
    vx_assert(g != nullptr);
    g->onStart();
}
