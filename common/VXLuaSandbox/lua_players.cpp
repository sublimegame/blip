//
//  lua_players.cpp
//  Cubzh
//
//  Created by guest on 03/07/2019.
//  Copyright © 2019 voxowl. All rights reserved.
//

#include "lua_players.hpp"

#include "lua.hpp"

#include "lua_player.hpp"
#include "lua_utils.hpp"
#include "lua_constants.h"

#include "VXGame.hpp"

#include "xptools.h"

// shared
static int _g_players_len(lua_State * const L);
static int _g_players_iter(lua_State * const L);
bool _insertPlayer(lua_State * const L, const int luaPlayerIdx, bool otherPlayers);
bool _removePlayer(lua_State * const L, const uint8_t playerID, bool otherPlayers);

static int _g_otherPlayers_index(lua_State * const L);
static int _g_otherPlayers_newindex(lua_State * const L);

static int _g_players_index(lua_State * const L);
static int _g_players_newindex(lua_State * const L);

// MARK: - OtherPlayers

void lua_g_otherPlayers_createAndPush(lua_State * const L) {
    vx_assert(L != nullptr);
    LUAUTILS_STACK_SIZE(L)
    
    lua_newtable(L); // "OtherPlayers" global table
    lua_newtable(L); // metatable
    {
        lua_pushstring(L, "__index");
        lua_pushcfunction(L, _g_otherPlayers_index, "");
        lua_rawset(L, -3);
        
        lua_pushstring(L, "__newindex");
        lua_pushcfunction(L, _g_otherPlayers_newindex, "");
        lua_rawset(L, -3);
        
        lua_pushstring(L, "__len");
        lua_pushcfunction(L, _g_players_len, "");
        lua_rawset(L, -3);

        lua_pushstring(L, "__iter");
        lua_pushcfunction(L, _g_players_iter, "");
        lua_rawset(L, -3);

        // Array table to store players in the metable
        // and prevent changes in the exposed one.
        lua_pushstring(L, "__array");
        lua_newtable(L);
        lua_rawset(L, -3);

        // players indexed by ConnectionID
        lua_pushstring(L, "__map");
        lua_newtable(L);
        lua_rawset(L, -3);

        lua_pushstring(L, "__metatable");
        lua_pushboolean(L, false);
        lua_rawset(L, -3);

        lua_pushstring(L, "__type");
        lua_pushstring(L, "OtherPlayers");
        lua_rawset(L, -3);

        lua_pushstring(L, "__typen");
        lua_pushinteger(L, ITEM_TYPE_G_OTHERPLAYERS);
        lua_rawset(L, -3);
    }
    lua_setmetatable(L, -2);
    
    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
}

bool lua_g_otherPlayers_push(lua_State * const L) {
    vx_assert(L != nullptr);
    LUAUTILS_STACK_SIZE(L)
    lua_utils_pushRootGlobalsFromRegistry(L);
    if (lua_getmetatable(L, -1) != 1) { // get its metatable
        lua_pop(L, 1); // pop global table
        vxlog_error("lua_g_otherPlayers_push - can't get metatable");
        LUAUTILS_STACK_SIZE_ASSERT(L, 0)
        return false;
    }
    const int type = lua_getfield(L, -1, P3S_LUA_G_OTHER_PLAYERS);
    lua_remove(L, -2); // pop Global metatable
    lua_remove(L, -2); // pop Global
    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
    LUAUTILS_ASSERT_ARG_TYPE(L, -1, ITEM_TYPE_G_OTHERPLAYERS);
    return type == LUA_TTABLE;
}

bool lua_g_otherPlayers_insertPlayerTable(lua_State * const L, const int luaPlayerIdx) {
    return _insertPlayer(L, luaPlayerIdx, true);
}

bool lua_g_otherPlayers_removePlayerTable(lua_State * const L, const uint8_t playerID) {
    return _removePlayer(L, playerID, true);
}

static int _g_otherPlayers_index(lua_State * const L) {
    vx_assert(L != nullptr);
    LUAUTILS_ASSERT_ARGS_COUNT(L, 2)
    LUAUTILS_ASSERT_ARG_TYPE(L, 1, ITEM_TYPE_G_OTHERPLAYERS)
    LUAUTILS_STACK_SIZE(L)

    if (lua_type(L, 2) == LUA_TNUMBER) {
        const lua_Integer key = lua_tointeger(L, 2);
        luaL_getmetafield(L, 1, "__array");
        lua_rawgeti(L, -1, key);
        lua_remove(L, -2); // remove __array
        LUAUTILS_STACK_SIZE_ASSERT(L, 1)
        return 1;
    }

    lua_pushnil(L);
    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
    return 1;
}

static int _g_otherPlayers_newindex(lua_State * const L) {
    vx_assert(L != nullptr);
    LUAUTILS_STACK_SIZE(L)
    LUAUTILS_ERROR(L, "OtherPlayers cannot be modified.");
    LUAUTILS_STACK_SIZE_ASSERT(L, 0)
    return 0;
}

// MARK: - Players

void lua_g_players_createAndPush(lua_State * const L) {
    vx_assert(L != nullptr);
    LUAUTILS_STACK_SIZE(L)
    
    lua_newtable(L); // "Players" global table
    lua_newtable(L); // metatable
    {
        lua_pushstring(L, "__index");
        lua_pushcfunction(L, _g_players_index, "");
        lua_rawset(L, -3);
        
        lua_pushstring(L, "__newindex");
        lua_pushcfunction(L, _g_players_newindex, "");
        lua_rawset(L, -3);
        
        lua_pushstring(L, "__len");
        lua_pushcfunction(L, _g_players_len, "");
        lua_rawset(L, -3);

        lua_pushstring(L, "__iter");
        lua_pushcfunction(L, _g_players_iter, "");
        lua_rawset(L, -3);

        // Array table to store players in the metable
        // and prevent changes in the exposed one.
        lua_pushstring(L, "__array");
        lua_newtable(L);
        lua_rawset(L, -3);

        // players indexed by ConnectionID
        lua_pushstring(L, "__map");
        lua_newtable(L);
        lua_rawset(L, -3);

        lua_pushstring(L, "__metatable");
        lua_pushboolean(L, false);
        lua_rawset(L, -3);

        lua_pushstring(L, "__type");
        lua_pushstring(L, "Players");
        lua_rawset(L, -3);

        lua_pushstring(L, "__typen");
        lua_pushinteger(L, ITEM_TYPE_G_PLAYERS);
        lua_rawset(L, -3);
    }
    lua_setmetatable(L, -2);
    
    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
}

bool lua_g_players_push(lua_State * const L) {
    vx_assert(L != nullptr);
    LUAUTILS_STACK_SIZE(L)
    lua_utils_pushRootGlobalsFromRegistry(L);
    if (lua_getmetatable(L, -1) != 1) { // get its metatable
        lua_pop(L, 1); // pop global table
        vxlog_error("lua_g_players_push - can't get metatable");
        LUAUTILS_STACK_SIZE_ASSERT(L, 0)
        return false;
    }
    const int type = lua_getfield(L, -1, P3S_LUA_G_PLAYERS);
    lua_remove(L, -2); // pop Global metatable
    lua_remove(L, -2); // pop Global
    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
    LUAUTILS_ASSERT_ARG_TYPE(L, -1, ITEM_TYPE_G_PLAYERS);
    return type == LUA_TTABLE;
}

bool lua_g_players_pushPlayerTable(lua_State * const L, const uint8_t playerID) {
    vx_assert(L != nullptr);
    LUAUTILS_STACK_SIZE(L)
    
    // push Players table
    if (lua_g_players_push(L) == false) {
        lua_pop(L, 1); // pop luaPlayerIdx
        LUAUTILS_STACK_SIZE_ASSERT(L, 0)
        return false;
    }
    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
    
    luaL_getmetafield(L, -1, "__map");

    lua_rawgeti(L, -1, playerID);
    const bool foundPlayer = lua_type(L, -1) == LUA_TUSERDATA;

    if (foundPlayer == false) {
        lua_pop(L, 3); // pop value, Map & Players
        LUAUTILS_STACK_SIZE_ASSERT(L, 0)
        return false;
    }
    
    LUAUTILS_STACK_SIZE_ASSERT(L, 3)
    lua_remove(L, -2); // remove Map
    lua_remove(L, -2); // remove Players

    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
    return true;
}

bool lua_g_players_insertPlayerTable(lua_State * const L, const int luaPlayerIdx) {
    return _insertPlayer(L, luaPlayerIdx, false);
}

bool lua_g_players_updatePlayer(lua_State * const L,
                                const uint8_t currentID,
                                const uint8_t *newID,
                                const std::string *username,
                                const std::string *userid) {
    
    LUAUTILS_STACK_SIZE(L)
    
    if (lua_g_players_push(L) == false) {
        lua_pop(L, 1); // pop luaPlayerIdx
        vxlog_error("lua_g_players_updatePlayerID - can't get Players");
        LUAUTILS_STACK_SIZE_ASSERT(L, 0)
        return false;
    }
    LUAUTILS_STACK_SIZE_ASSERT(L, 1)

    luaL_getmetafield(L, -1, "__map");
    lua_rawgeti(L, -1, currentID);

    if (lua_utils_getObjectType(L, -1) != ITEM_TYPE_PLAYER) {
        // not found or not a Player
        lua_pop(L, 3); // Pop value, Map and Players
        LUAUTILS_STACK_SIZE_ASSERT(L, 0)
        return false;
    }
    
    LUAUTILS_STACK_SIZE_ASSERT(L, 3) // Player, Map, Players

    // push Player's metatable
    // It's not allowed to modify Username, UserID or ID on Player,
    // bypassing restriction by setting metatable fields directly.
    lua_getmetatable(L, -1);

    // Player's metatable: -1
    // Player: -2
    // Map: -3
    // Players: -4

    if (username != nullptr) {
        lua_pushstring(L, "Username");
        if (username->empty()) {
            std::string placeholder = PLAYER_DEFAULT_USERNAME;
            lua_pushstring(L, placeholder.c_str());
        } else {
            lua_pushstring(L, (*username).c_str());
        }
        lua_rawset(L, -3);
    }
    
    if (userid != nullptr) {
        lua_pushstring(L, "UserID");
        lua_pushstring(L, (*userid).c_str());
        lua_rawset(L, -3);
    }
    
    // Assign new ID to player
    if (newID != nullptr) {
        lua_pushstring(L, "ConnectionID");
        lua_pushinteger(L, *newID);
        lua_rawset(L, -3);

        lua_pop(L, 1); // pop Player's metatable

        // set old slot to nil
        lua_pushnil(L);

        // nil: -1
        // Player: -2
        // Map: -3
        // Players: -4

        lua_rawseti(L, -3, currentID);

        // Player: -1
        // Map: -2
        // Players: -3

        lua_rawseti(L, -2, *newID);
        lua_pop(L, 2); // pop Map & Players
    } else {
        lua_pop(L, 4); // pop Player's metatable, Player, Map & Players
    }

    LUAUTILS_STACK_SIZE_ASSERT(L, 0)
    return true;
}

bool lua_g_players_removePlayerTable(lua_State * const L, const uint8_t playerID) {
    return _removePlayer(L, playerID, false);
}

static int _g_players_index(lua_State * const L) {
    vx_assert(L != nullptr);
    LUAUTILS_ASSERT_ARGS_COUNT(L, 2)
    LUAUTILS_ASSERT_ARG_TYPE(L, 1, ITEM_TYPE_G_PLAYERS)
    LUAUTILS_STACK_SIZE(L)
    
    const int keyType = lua_type(L, 2);
    
    if (keyType == LUA_TSTRING) {
        const char *keyStr = lua_tostring(L, 2);
        if (strcmp(keyStr, "Max") == 0) {
            vx::Game *cppGame = nullptr;
            if (lua_utils_getGameCppWrapperPtr(L, &cppGame) == false) {
                LUAUTILS_INTERNAL_ERROR(L);
            }
            lua_pushinteger(L, cppGame->getData()->getMaxPlayers());
            LUAUTILS_STACK_SIZE_ASSERT(L, 1)
            return 1;
        } else if (strcmp(keyStr, "DefaultUsername") == 0) {
            lua_pushstring(L, PLAYER_DEFAULT_USERNAME);
            LUAUTILS_STACK_SIZE_ASSERT(L, 1)
            return 1;
        }
    } else if (keyType == LUA_TNUMBER) {
        const lua_Integer key = lua_tointeger(L, 2);
        luaL_getmetafield(L, 1, "__array");
        lua_rawgeti(L, -1, key);
        lua_remove(L, -2); // remove Array
        LUAUTILS_STACK_SIZE_ASSERT(L, 1)
        return 1;
    }

    lua_pushnil(L);
    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
    return 1;
}

static int _g_players_newindex(lua_State * const L) {
    vx_assert(L != nullptr);
    LUAUTILS_STACK_SIZE(L)
    LUAUTILS_ERROR(L, "Players cannot be modified.");
    LUAUTILS_STACK_SIZE_ASSERT(L, 0)
    return 0;
}

std::vector<uint8_t> lua_g_players_getPlayerIDs(lua_State * const L) {
    LUAUTILS_STACK_SIZE(L)
    std::vector<uint8_t> ids;
    
    if (lua_g_players_push(L) == false) {
        return ids;
    }
    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
    
    luaL_getmetafield(L, -1, "__array");

    lua_pushnil(L);  /* first key, now at -1 */
    while (lua_next(L, -2) != 0) { // lua_next pops key at -1 (nil at first iteration)
        // key now at -2, value at -1
        if (lua_utils_getObjectType(L, -1) == ITEM_TYPE_PLAYER) {
            lua_getfield(L, -1, "ConnectionID");
            if (lua_utils_isIntegerStrict(L, -1)) {
                ids.push_back(static_cast<uint8_t>(lua_tointeger(L, -1)));
            }
            lua_pop(L, 1);
        }
        lua_pop(L, 1); // pop value, keep key for next iteration
    }
    
    lua_pop(L, 2); // pop Array and Players
    LUAUTILS_STACK_SIZE_ASSERT(L, 0)
    return ids;
}

// MARK: - Shared

static int _g_players_len(lua_State * const L) {
    vx_assert(L != nullptr);
    LUAUTILS_ASSERT_ARGS_COUNT(L, 2) // ARG 1: Players or OtherPlayers
    LUAUTILS_STACK_SIZE(L)
    int len = 0;
    luaL_getmetafield(L, 1, "__array");
    len = lua_objlen(L, -1);
    lua_pop(L, 1); // pop __array
    lua_pushinteger(L, len);
    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
    return 1;
}

static int _g_players_next(lua_State * const L) {
    LUAUTILS_ASSERT_ARGS_COUNT(L, 2) // table, current index
    LUAUTILS_STACK_SIZE(L)

    int index = 1;

    if (lua_isnil(L, 2) == false) { // not first index
        if (lua_isnumber(L, 2) == false) {
            // index should a number
            LUAUTILS_INTERNAL_ERROR(L) // returns
        }

        index = static_cast<int>(lua_tointeger(L, 2));
        index = index + 1;
    }

    lua_pushinteger(L, index);

    luaL_getmetafield(L, 1, "__array");
    lua_rawgeti(L, -1, index);

    // value: -1
    // __array: -2
    // index: -3

    if (lua_isnil(L, -1)) {
        // reached the end, pop and don't return anything
        lua_pop(L, 3);
        LUAUTILS_STACK_SIZE_ASSERT(L, 0)
        return 0;
    }

    lua_remove(L, -2); // remove __array

    LUAUTILS_STACK_SIZE_ASSERT(L, 2)
    return 2;
}

static int _g_players_iter(lua_State * const L) {
    vx_assert(L != nullptr);
    LUAUTILS_ASSERT_ARGS_COUNT(L, 1) // ARG 1: Players or OtherPlayers
    LUAUTILS_STACK_SIZE(L)

    lua_pushcfunction(L, _g_players_next, "_g_players_next");
    lua_pushvalue(L, 1); // Players
    lua_pushnil(L); // starting point

    LUAUTILS_STACK_SIZE_ASSERT(L, 3)
    return 3;
}

bool _insertPlayer(lua_State * const L, const int luaPlayerIdx, bool otherPlayers) {
    vx_assert(L != nullptr);
    LUAUTILS_ASSERT_ARG_TYPE(L, luaPlayerIdx, ITEM_TYPE_PLAYER)
    LUAUTILS_STACK_SIZE(L)

    // retrieve player ID (0-15)
    uint8_t playerID = 0;
    if (lua_player_getIdFromLuaPlayer(L, luaPlayerIdx, &playerID) == false) {
        vxlog_error("lua_g_players_insertPlayerTable failed to retrieve player ID from Lua Player object");
        lua_pop(L, 1);
        LUAUTILS_STACK_SIZE_ASSERT(L, 0)
        return false;
    }

    // put Player at -1
    lua_pushvalue(L, luaPlayerIdx);
    LUAUTILS_STACK_SIZE_ASSERT(L, 1)

    // push Players table
    if (otherPlayers) {
        if (lua_g_otherPlayers_push(L) == false) {
            lua_pop(L, 1); // pop Player
            LUAUTILS_INTERNAL_ERROR(L);
        }
    } else {
        if (lua_g_players_push(L) == false) {
            lua_pop(L, 1); // pop Player
            LUAUTILS_INTERNAL_ERROR(L);
        }
    }
    LUAUTILS_STACK_SIZE_ASSERT(L, 2)

    luaL_getmetafield(L, -1, "__map");
    lua_pushvalue(L, -3);

    // Player: -1
    // Map: -2
    // Players: -3
    // Player: -4

    lua_rawseti(L, -2, playerID);
    lua_pop(L, 1);

    // Players: -1
    // Player: -2

    luaL_getmetafield(L, -1, "__array");
    lua_pushvalue(L, -3);

    // Player: -1
    // Array: -2
    // Players: -3
    // Player: -4

    lua_rawseti(L, -2, lua_objlen(L, -2) + 1);

    lua_pop(L, 3); // pop Array, Players and Player
    LUAUTILS_STACK_SIZE_ASSERT(L, 0)
    return true;
}

bool _removePlayer(lua_State * const L, const uint8_t playerID, bool otherPlayers) {
    vx_assert(L != nullptr);
    LUAUTILS_STACK_SIZE(L)

    // push Players table
    if (otherPlayers) {
        const bool ok = lua_g_otherPlayers_push(L);
        vx_assert(ok);
    } else {
        const bool ok = lua_g_players_push(L);
        vx_assert(ok);
    }
    LUAUTILS_STACK_SIZE_ASSERT(L, 1)

    luaL_getmetafield(L, -1, "__map");

    // Map: -1
    // Players: -2

    lua_rawgeti(L, -1, playerID);
    const bool foundPlayer = lua_type(L, -1) == LUA_TUSERDATA;

    // Player: -1 (can be nil)
    // Map: -2
    // Players: -3

    if (foundPlayer == false) {
        lua_pop(L, 3);
        LUAUTILS_STACK_SIZE_ASSERT(L, 0)
        return false;
    }

    lua_pop(L, 1); // pop Player
    LUAUTILS_STACK_SIZE_ASSERT(L, 2)

    lua_pushnil(L); // new value
    lua_rawseti(L, -2, playerID);

    lua_pop(L, 1); // pop Map

    luaL_getmetafield(L, -1, "__array");

    // Array: -1
    // Players: -2
    LUAUTILS_STACK_SIZE_ASSERT(L, 2)

    lua_pushnil(L); // __array at -2, nil key at -1
    bool found = false;
    uint8_t pID = 0;

    while(lua_next(L, -2) != 0) {
        if (found) {
            // offset following values
            lua_pushvalue(L, -1);
            // callbacks at -4, key at -3, value at -2, value at -1
            lua_rawseti(L, -4, lua_tointeger(L, -3) - 1);
        } else {
            // callbacks at -3, key at -2, Player at -1
            if (lua_player_getIdFromLuaPlayer(L, -1, &pID) == false) {
                vxlog_error("_removePlayer: failed to retrieve player ID while iterating over players");
                lua_pop(L, 1);
            }

            if (pID == playerID) {
                // found Player to remove!
                found = true;
            }
        }
        if (found) {
            lua_pushnil(L);
            // callbacks at -4, key at -3, value at -2, nil at -1
            lua_rawseti(L, -4, lua_tointeger(L, -3));
        }
        lua_pop(L, 1); // pop value
    }

    lua_pop(L, 2); // pop Array and Players
    LUAUTILS_STACK_SIZE_ASSERT(L, 0)
    return true;
}
