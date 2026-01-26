//
//  lua_event.cpp
//  Cubzh
//
//  Created by Gaetan de Villele on 02/08/2019.
//  Copyright © 2019 voxowl. All rights reserved.
//

#include "lua_event.hpp"

// lua
#include "lua.hpp"
#include "lua_utils.hpp"

// sandbox
#include "lua_constants.h"

#include "lua_players.hpp"
#include "lua_player.hpp"
#include "lua_server.hpp"
#include "lua_bin_serializer.hpp"

// engine
#include "config.h"

#include "VXGameMessage.hpp"
#include "VXGame.hpp"

// fields of an Event object table
#define LUA_EVENT_FIELD_TYPE "Type"
#define LUA_EVENT_FIELD_SENDER "Sender"
#define LUA_EVENT_FIELD_RECIPIENTS "Recipients"
#define LUA_EVENT_FIELD_SENDTO "SendTo"
#define LUA_EVENT_FIELD_SENDTO_WITHDEBUG "SendToWithDebug"

// key of the table containing the user fields, in the event's metatable
#define LUA_EVENT_METAFIELD_USERDATA "Userdata"

// --------------------------------------------------
//
// MARK: - Static functions prototypes
//
// --------------------------------------------------

/// "Event" global table "__newindex" metamethod
static int _lua_g_event_newindex(lua_State * const L);

/// "Event" global table "__call" metamethod
static int _lua_g_event_call(lua_State * const L);

/// __index function for the metatable of the event objects
static int _lua_event_index(lua_State * const L);
/// __newindex function for the metatable of the event objects
static int _lua_event_newindex(lua_State * const L);

/// idx is index of event object table in the Lua stack
static bool _event_create_and_push_recipients(lua_State * const L, const DoublyLinkedListUint8 *recipients);

///
static bool _create_and_push_sender_for_sender_id(lua_State * const L, const uint8_t& senderId);

// myEventInstance:SendTo(player1, Server)
static int _lua_event_sendTo(lua_State *L);

// Same as _lua_event_sendTo with headers to
// debug the event (not documented)
static int _lua_event_sendTo_withDebug(lua_State *L);

// utility functions
static void _processSendToLuaArg(std::vector<uint8_t>& recipientIds,
                                 lua_State * const L,
                                 const int argIdx);

// --------------------------------------------------
//
// MARK: - Exposed functions
//
// --------------------------------------------------

/// we currently use the "readonly" metatable for the "Event" global table,
/// we might want to use the "caps protect" metatable instead.
void lua_g_event_create_and_push(lua_State * const L) {
    vx_assert(L != nullptr);
    LUAUTILS_STACK_SIZE(L)
    
    // store pointer on function to send ApiMessages
    
    lua_newtable(L);
    lua_newtable(L); // metatable
    {
        lua_pushstring(L, "__index");
        lua_newtable(L); // empty table
        lua_rawset(L, -3);
        
        lua_pushstring(L, "__newindex");
        lua_pushcfunction(L, _lua_g_event_newindex, "");
        lua_rawset(L, -3);
        
        lua_pushstring(L, "__call");
        lua_pushcfunction(L, _lua_g_event_call, "");
        lua_rawset(L, -3);
        
        lua_pushstring(L, "__metatable"); // hide metatable from the Lua sandbox user
        lua_pushboolean(L, false);
        lua_rawset(L, -3);
        
        lua_pushstring(L, P3S_LUA_OBJ_METAFIELD_TYPE); // used to log tables
        lua_pushinteger(L, ITEM_TYPE_G_EVENT);
        lua_rawset(L, -3);
    }
    lua_setmetatable(L, -2);
    
    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
}

///
bool lua_event_create_and_push(lua_State * const L,
                               const uint32_t& eventType,
                               const uint8_t& senderId,
                               const DoublyLinkedListUint8 *recipients,
                               const uint8_t *data,
                               const size_t size) {
    vx_assert(L != nullptr);
    LUAUTILS_STACK_SIZE(L)
    
    // create "event" table
    lua_newtable(L);
    lua_newtable(L); // metatable
    {
        lua_pushstring(L, "__index");
        lua_pushcfunction(L, _lua_event_index, "");
        lua_rawset(L, -3);
        
        lua_pushstring(L, "__newindex");
        lua_pushcfunction(L, _lua_event_newindex, "");
        lua_rawset(L, -3);
        
        lua_pushstring(L, "__metatable");
        lua_pushboolean(L, false);
        lua_rawset(L, -3);
        
        lua_pushstring(L, LUA_EVENT_FIELD_TYPE);
        lua_pushinteger(L, eventType);
        lua_rawset(L, -3);
        
        lua_pushstring(L, LUA_EVENT_FIELD_SENDER);
        _create_and_push_sender_for_sender_id(L, senderId);
        lua_rawset(L, -3);
        
        lua_pushstring(L, LUA_EVENT_FIELD_RECIPIENTS);
        _event_create_and_push_recipients(L, recipients);
        lua_rawset(L, -3);
        
        lua_pushstring(L, P3S_LUA_OBJ_METAFIELD_TYPE); // used to log tables
        lua_pushinteger(L, ITEM_TYPE_EVENT);
        lua_rawset(L, -3);
        
        // Data is a generic table,
        // decoded from data or created empty.
        lua_pushstring(L, LUA_EVENT_METAFIELD_USERDATA);
        if (data != nullptr) {
            std::string err = "";
            if (lua_bin_serializer_decode_and_push_table(L, data, size, err) == false) {
                vxlog_error("can't decode serializable table: %s", err.c_str());
                lua_newtable(L);
            }
        } else {
            lua_newtable(L);
        }
        lua_rawset(L, -3);
    }
    lua_setmetatable(L, -2);
    
    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
    return true;
}

// --------------------------------------------------
//
// MARK: - static functions
//
// --------------------------------------------------

static int _lua_g_event_newindex(lua_State * const L) {
    LUAUTILS_ERROR(L, "Event class is read-only");
    return 0;
}

static int _lua_g_event_call(lua_State * const L) {
    vx_assert(L != nullptr);
    LUAUTILS_ASSERT_ARG_TYPE(L, 1, ITEM_TYPE_G_EVENT)
    LUAUTILS_STACK_SIZE(L)
    
    const int argCount = lua_gettop(L);
    
    if (argCount != 1) {
        LUAUTILS_ERROR(L, "Event() takes no arguments");
    }
    
    vx::Game *cppGame = nullptr;
    if (lua_utils_getGameCppWrapperPtr(L, &cppGame) == false) {
        LUAUTILS_INTERNAL_ERROR(L)
    }
    
    uint8_t senderId = 0;
    if (cppGame->isInServerMode()) {
        senderId = PLAYER_ID_SERVER;
    } else {
        senderId = cppGame->getLocalPlayerID();
    }
    
    lua_event_create_and_push(L,
                              EVENT_TYPE_FROM_SCRIPT,
                              senderId,
                              nullptr, // empty recipients
                              nullptr,
                              0);
    
    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
    return 1;
}

static int _lua_event_index(lua_State * const L) {
    vx_assert(L != nullptr);
    LUAUTILS_ASSERT_ARGS_COUNT(L, 2)
    LUAUTILS_ASSERT_ARG_TYPE(L, 1, ITEM_TYPE_EVENT)
    LUAUTILS_STACK_SIZE(L)
    
    const char *key = lua_tostring(L, 2);
    LUAUTILS_STACK_SIZE_ASSERT(L, 0)
    
    if (strcmp(key, LUA_EVENT_FIELD_TYPE) == 0 ||
        strcmp(key, LUA_EVENT_FIELD_SENDER) == 0 ||
        strcmp(key, LUA_EVENT_FIELD_RECIPIENTS) == 0) {
        
        LUA_GET_METAFIELD_AND_RETURN_TYPE_PUSHING_NIL_IF_NOT_FOUND(L, 1, key);
        LUAUTILS_STACK_SIZE_ASSERT(L, 1)
        
    } else if (strcmp(key, LUA_EVENT_FIELD_SENDTO) == 0) {
        
        lua_pushcfunction(L, _lua_event_sendTo, "");
        
    } else if (strcmp(key, LUA_EVENT_FIELD_SENDTO_WITHDEBUG) == 0) {
        
        lua_pushcfunction(L, _lua_event_sendTo_withDebug, "");
        
    } else {
        // user defined keys
        const int type = LUA_GET_METAFIELD_AND_RETURN_TYPE(L, 1, LUA_EVENT_METAFIELD_USERDATA);
        if (type != LUA_TTABLE) {
            LUAUTILS_INTERNAL_ERROR(L)
        }
        
        lua_pushvalue(L, 2); // key
        lua_gettable(L, -2);
        // result now on top
        lua_remove(L, -2); // Data table
    }
    
    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
    return 1;
}

/// __newindex function for the metatable of the event objects
/// settable fields:
/// - "Recipients"
/// - <any non caps field>
static int _lua_event_newindex(lua_State * const L) {
    vx_assert(L != nullptr);
    LUAUTILS_ASSERT_ARG_TYPE(L, 1, ITEM_TYPE_EVENT)
    LUAUTILS_STACK_SIZE(L)
    
    int keyType = lua_type(L, 2);
    if (keyType != LUA_TNUMBER && keyType != LUA_TSTRING) {
        LUAUTILS_ERROR(L, "Event - only string and number keys are allowed"); // returns
    }
    
    // setting non-string or non-capitalized string keys is allowed
    if (lua_utils_isStringStrictStartingWithUppercase(L, 2)) {
        LUAUTILS_ERROR(L, "Event - setting keys starting with uppercase is not allowed"); // returns
    }
    
    const int type = LUA_GET_METAFIELD_AND_RETURN_TYPE(L, 1, LUA_EVENT_METAFIELD_USERDATA);
    if (type != LUA_TTABLE) {
        LUAUTILS_INTERNAL_ERROR(L)
    }
    
    lua_pushvalue(L, 2); // push key
    lua_pushvalue(L, 3); // push value
    lua_settable(L, -3);
    
    lua_pop(L, 1); // pop "Userdata" table
    
    LUAUTILS_STACK_SIZE_ASSERT(L, 0)
    return 0;
}

static bool _create_and_push_sender_for_sender_id(lua_State * const L, const uint8_t& senderId) {
    vx_assert(L != nullptr);
    LUAUTILS_STACK_SIZE(L)
    
    if (senderId == PLAYER_ID_SERVER) {
        if (lua_server_pushTable(L) == false) {
            vxlog_error("event.Sender can't be set to Server");
            lua_pushnil(L);
        }
    } else if (lua_g_players_pushPlayerTable(L, senderId) == false) {
        lua_pushnil(L);
        vxlog_error("event.Sender can't be set - can't push Player for senderId: %d", senderId);
        // no need to push nil, nil is pushed when the player is not found
    }
    
    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
    return true;
}

static bool _event_create_and_push_recipients(lua_State * const L,
                                              const DoublyLinkedListUint8 *recipients) {
    vx_assert(L != nullptr);
    LUAUTILS_STACK_SIZE(L)
    
    // construct value (a Lua array of recipients ids)
    lua_newtable(L);
    
    if (recipients != nullptr) {
        lua_Integer pos = 1; // brand new array we can start at 1
        DoublyLinkedListUint8Node *n = doubly_linked_list_uint8_front(recipients);
        while (n != nullptr) {
            const uint8_t id = doubly_linked_list_uint8_node_get_value(n);
            lua_pushinteger(L, id);
            lua_rawseti(L, -2, pos);
            pos += 1;
            n = doubly_linked_list_uint8_node_next(n);
        }
    }
    
    LUAUTILS_STACK_SIZE_ASSERT(L, 1);
    return true;
}

// myEventInstance:SendTo(player1, Server)
static int _lua_event_sendTo(lua_State * const L) {
    vx_assert(L != nullptr);
    LUAUTILS_STACK_SIZE(L)
    
    const int argCount = lua_gettop(L);
    
    if (argCount < 1 || lua_utils_getObjectType(L, 1) != ITEM_TYPE_EVENT) {
        LUAUTILS_ERROR(L, "Event:SendTo - function should be called with `:`");
    }
    
    // validate arguments count
    if (argCount < 2) {
        LUAUTILS_ERROR(L, "Event:SendTo - needs at least one recipient");
    }
    
    // retrieve Game from Lua state
    vx::Game *cppGame = nullptr;
    if (lua_utils_getGameCppWrapperPtr(L, &cppGame) == false) {
        LUAUTILS_INTERNAL_ERROR(L)
    }
    
    // Add senderId to the event object.
    // To know the senderId value, we first get the sandbox role. If it is
    // Server, then the senderId is 0 (reserved value for the Server).
    // Otherwise, if the the role is Player, we get the value of "Player.Id"
    // in the Lua sandbox.
    uint8_t senderId = 0;
    if (cppGame->isInServerMode()) {
        senderId = PLAYER_ID_SERVER;
    } else {
        senderId = cppGame->getLocalPlayerID();
        if (senderId == PLAYER_ID_NOT_ATTRIBUTED) {
            lua_pushboolean(L, false);
            vxlog_warning("not sending event, Player ID not attributed (wait for OnPlayerJoin)");
            LUAUTILS_STACK_SIZE_ASSERT(L, 1)
            return 1;
        }
    }
    
    // ENCODE USERDATA
    LUA_GET_METAFIELD(L, 1, LUA_EVENT_METAFIELD_USERDATA);

    uint8_t *bytes = nullptr;
    size_t size = 0;
    
    std::string err = "";
    if (lua_bin_serializer_encode_table(L, -1, &bytes, &size, err) == false) {
        LUAUTILS_ERROR_F(L, "Event:SendTo error: %s", err.c_str());
    }
    
    lua_pop(L, 1); // pop userdata table
    LUAUTILS_STACK_SIZE_ASSERT(L, 0)
    
    // get the event type value
    
    if (LUA_GET_METAFIELD_AND_RETURN_TYPE(L, 1, LUA_EVENT_FIELD_TYPE) == LUA_TNIL) {
        LUAUTILS_INTERNAL_ERROR(L) // return
    }
    const int eventType = static_cast<int>(lua_tointeger(L, -1));
    lua_pop(L, 1);
    
    std::vector<uint8_t> recipientIds = std::vector<uint8_t>();
    
    // process arguments to identify recipients
    // (skip 1st arg which is the event itself)
    for (int i = 2; i < argCount+1; i++) {
        _processSendToLuaArg(recipientIds, L, i);
    }
    
    vx::GameMessage_SharedPtr msg = vx::GameMessage::makeGameEvent(eventType,
                                                                   senderId,
                                                                   recipientIds,
                                                                   bytes,
                                                                   size);
    free(bytes);
    
    if (msg == nullptr) {
        LUAUTILS_INTERNAL_ERROR(L) // return
    } else {
        cppGame->sendMessage(msg);
    }

    lua_pushboolean(L, true);
    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
    return 1;
}

static int _lua_event_sendTo_withDebug(lua_State * const L) {
    vx_assert(L != nullptr);
    LUAUTILS_STACK_SIZE(L)
    
    const int argCount = lua_gettop(L);
    
    if (argCount < 1 || lua_utils_getObjectType(L, 1) != ITEM_TYPE_EVENT) {
        LUAUTILS_ERROR(L, "Event:SendToWithDebug - function should be called with `:`");
    }
    
    // validate arguments count
    if (argCount < 2) {
        LUAUTILS_ERROR(L, "Event:SendToWithDebug - needs at least one recipient");
    }
    
    // retrieve Game from Lua state
    vx::Game *cppGame = nullptr;
    if (lua_utils_getGameCppWrapperPtr(L, &cppGame) == false) {
        LUAUTILS_INTERNAL_ERROR(L)
    }
    
    // ENCODE USERDATA
    LUA_GET_METAFIELD(L, 1, LUA_EVENT_METAFIELD_USERDATA);

    uint8_t *bytes = nullptr;
    size_t size = 0;

    std::string err = "";
    if (lua_bin_serializer_encode_table(L, -1, &bytes, &size, err) == false) {
        LUAUTILS_ERROR_F(L, "Event:SendToWithDebug error: %s", err.c_str());
    }
    
    lua_pop(L, 1); // pop userdata table
    LUAUTILS_STACK_SIZE_ASSERT(L, 0)
    
    // no need to get event type, forcing to EVENT_TYPE_FROM_SCRIPT_WITH_DEBUG
    //if (LUA_GET_METAFIELD_AND_RETURN_TYPE(L, 1, LUA_EVENT_FIELD_TYPE) == LUA_TNIL) {
    //    LUAUTILS_INTERNAL_ERROR(L) // return
    //}
    //const int eventType = static_cast<int>(lua_tointeger(L, -1));
    //lua_pop(L, 1);
    
    // Add senderId to the event object.
    // To know the senderId value, we first get the sandbox role. If it is
    // Server, then the senderId is 0 (reserved value for the Server).
    // Otherwise, if the the role is Player, we get the value of "Player.Id"
    // in the Lua sandbox.
    uint8_t senderId = 0;
    if (cppGame->isInServerMode()) {
        senderId = PLAYER_ID_SERVER;
    } else {
        senderId = cppGame->getLocalPlayerID();
        if (senderId == PLAYER_ID_NOT_ATTRIBUTED) {
            LUAUTILS_ERROR(L, "Can't create Event before local Player ID is attributed (wait for OnPlayerJoin)");
        }
    }
    
    std::vector<uint8_t> recipientIds = std::vector<uint8_t>();
    
    // process arguments to identify recipients
    // (skip 1st arg which is the event itself)
    for (int i = 2; i < argCount+1; i++) {
        _processSendToLuaArg(recipientIds, L, i);
    }
    
    vx::GameMessage_SharedPtr msg = vx::GameMessage::makeGameEvent(EVENT_TYPE_FROM_SCRIPT_WITH_DEBUG,
                                                                   senderId,
                                                                   recipientIds,
                                                                   bytes,
                                                                   size);
    free(bytes);
    cppGame->sendMessage(msg);
    
    LUAUTILS_STACK_SIZE_ASSERT(L, 0)
    return 0;
}

static void _processSendToLuaArg(std::vector<uint8_t>& recipientIds,
                                 lua_State * const L,
                                 const int argIdx) {
    vx_assert(L != nullptr);
    LUAUTILS_STACK_SIZE(L)
    
    if (lua_istable(L, argIdx) == false && lua_isuserdata(L, argIdx) == false) {
        vxlog_error("ignoring argument because it's not a table nor userdata");
        LUAUTILS_STACK_SIZE_ASSERT(L, 0)
        return;
    }
    
    // get lua object type, in order to determine whether it is a Server or a Player
    const int type = lua_utils_getObjectType(L, argIdx);
    if (type == ITEM_TYPE_SERVER) {
        recipientIds.push_back(PLAYER_ID_SERVER);
        
    } else if (type == ITEM_TYPE_PLAYER) {
        uint8_t playerId = 0;
        if (lua_player_getIdFromLuaPlayer(L, argIdx, &playerId)) {
            recipientIds.push_back(playerId);
        }
        
    } else if (type == ITEM_TYPE_G_PLAYERS) {
        recipientIds.push_back(PLAYER_ID_ALL);
        
    } else if (type == ITEM_TYPE_G_OTHERPLAYERS) {
        recipientIds.push_back(PLAYER_ID_ALL_BUT_SELF);
        
    } else {
        // Argument table is not a Server nor a Player, let's loop over its
        // items and look for Server or Player objects.
        lua_pushnil(L); // first key
        while (lua_next(L, argIdx) != 0) {
            // +2
            // 'key' is at index -2 and 'value' is at index -1
            const int itemType = lua_utils_getObjectType(L, -1);
            
            if (itemType == ITEM_TYPE_SERVER) {
                recipientIds.push_back(PLAYER_ID_SERVER);
                
            } else if (itemType == ITEM_TYPE_PLAYER) {
                uint8_t playerId = 0;
                if (lua_player_getIdFromLuaPlayer(L, -1, &playerId)) {
                    recipientIds.push_back(playerId);
                }
            }
            // pops value. Keeps 'key' for next iteration
            lua_pop(L, 1);
        }
    }
    LUAUTILS_STACK_SIZE_ASSERT(L, 0)
}

int lua_g_event_snprintf(lua_State *L,
                         char* result,
                         size_t maxLen,
                         bool spacePrefix) {
    
    vx_assert(L != nullptr);
    return snprintf(result, maxLen, "%s[Event (global)]", spacePrefix ? " " : "");
}

int lua_event_snprintf(lua_State *L,
                       char* result,
                       size_t maxLen,
                       bool spacePrefix) {
    
    vx_assert(L != nullptr);
    return snprintf(result, maxLen, "%s[Event]", spacePrefix ? " " : "");
}
