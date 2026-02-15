//
//  lua_player.cpp
//  Cubzh
//
//  Created by guest on 14/08/2019.
//  Copyright © 2019 Voxowl Inc. All rights reserved.
//

#include "lua_player.hpp"

// Lua
#include "lua.hpp"
#include "lua_utils.hpp"
#include "lua_box.hpp"

// sandbox
#include "lua_client.hpp"
#include "lua_block.hpp"
#include "lua_number3.hpp"
#include "lua_players.hpp"
#include "lua_impact.hpp"
#include "lua_items.hpp"
#include "lua_constants.h"
#include "lua_sandbox.hpp"
#include "lua_object.hpp"
#include "lua_shape.hpp"
#include "lua_logs.hpp"

// engine
#include "scene.h"
#include "vxlog.h"
#include "GameCoordinator.hpp"
#include "VXGameMessage.hpp"
#ifdef CLIENT_ENV
#include "VXGameClient.hpp"
#else
#include "VXMessageQueues.hpp"
#endif // CLIENT_ENV

// xptools
#include "xptools.h"


#ifndef CLIENT_ENV
extern vx::GameServer* gameServer;
#endif

///
bool lua_player_getIdFromLuaPlayer(lua_State * const L,
                                   const int idx,
                                   uint8_t * const playerIDPtr) {
    vx_assert(L != nullptr);
    LUAUTILS_STACK_SIZE(L)
    
    // get player id value from metatable
    if (lua_getfield(L, idx, "ConnectionID") == LUA_TNIL) {
        return false;
    }
    
    *playerIDPtr = static_cast<uint8_t>(luaL_checkinteger(L, -1));
    lua_pop(L, 1); // pop player.id
    
    LUAUTILS_STACK_SIZE_ASSERT(L, 0)
    return true;
}

///
bool lua_player_createAndPushPlayerTable(lua_State * const L,
                                         const uint8_t playerID,
                                         const char *playerUsername,
                                         const char *playerUserID,
                                         const bool isLocalPlayer) {
    vx_assert(L != nullptr);
    LUAUTILS_STACK_SIZE(L)

    std::string str;
    if (isLocalPlayer) {
        str = "return require(\"player\")(" + std::to_string(playerID) +
        ", \"" + playerUsername + "\", \"" + playerUserID + "\", true)";
    } else {
        str = "return require(\"player\")(" + std::to_string(playerID) +
        ", \"" + playerUsername + "\", \"" + playerUserID + "\", false)";
    }

    if (luau_dostring(L, str, "createPlayer") == false) {
        lua_pushstring(L, "luau_dostring error");
    }

    if (lua_isstring(L, -1)) {
        vxlog_error("lua_player_createAndPushPlayerTable ERROR: %s", lua_tostring(L, -1));
    }

    LUAUTILS_ASSERT_ARG_TYPE(L, -1, ITEM_TYPE_PLAYER)
    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
    return true;
}

int lua_player_snprintf(lua_State *L,
                        char* result,
                        size_t maxLen,
                        bool spacePrefix) {
    vx_assert(L != nullptr);
    
    lua_getfield(L, -1, "Username");
    const char *username = lua_tostring(L, -1);
    lua_pop(L, 1);
    
    return snprintf(result,
                    maxLen,
                    "%s[Player (%s)]",
                    spacePrefix ? " " : "",
                    username);
}
