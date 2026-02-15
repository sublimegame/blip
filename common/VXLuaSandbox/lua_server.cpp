//
//  lua_server.cpp
//  Cubzh
//
//  Created by guest on 14/08/2019.
//  Copyright © 2019 Voxowl Inc. All rights reserved.
//

#include "lua_server.hpp"

// C++
#include <ctime>
#include <cstring>

// Lua
#include "lua.hpp"
#include "lua_constants.h"
#include "lua_event.hpp"
#include "lua_nilSilentExtra.hpp"
#include "lua_utils.hpp"
#include "lua_logs.hpp"
#include "lua_local_event.hpp"
#include "lua_utils.hpp"

#include "sbs.hpp"

#include "config.h"

// xptools
#include <xptools.h>

// Functions
#define LUA_SERVER_FIELD_UNIXTIME "UnixTime" // function (read-only) (DEPRECATED, not documented, use Time:Unix() instead)

// --------------------------------------------------
//
// MARK: - Static function prototypes -
//
// --------------------------------------------------

static int _server_newindex(lua_State *L);
static int _server_newindex_forClient(lua_State *L);

static int _server_push_unix_time(lua_State *L);

// utility functions

static bool _server_pushFunction(lua_State * const L, const char *funcName);

// --------------------------------------------------
//
// MARK: - Exposed functions -
//
// --------------------------------------------------

///
void lua_server_createAndPushForServer(lua_State *L) {
    vx_assert(L != nullptr);
    LUAUTILS_STACK_SIZE(L)

    // global "Server" table
    lua_newtable(L);
    lua_newtable(L); // metatable
    {
        lua_pushstring(L, "__index");
        lua_newtable(L);
        {
            lua_pushstring(L, LUA_SERVER_FIELD_UNIXTIME);
            lua_pushcfunction(L, _server_push_unix_time, "");
            lua_rawset(L, -3);
        }
        lua_rawset(L, -3);
        
        lua_pushstring(L, "__newindex");
        lua_pushcfunction(L, _server_newindex, "");
        lua_rawset(L, -3);
        
        lua_pushstring(L, "__metatable"); // hide the metatable from the Lua sandbox user
        lua_pushboolean(L, false);
        lua_rawset(L, -3);
        
        lua_pushstring(L, P3S_LUA_OBJ_METAFIELD_TYPE); // used to log tables
        lua_pushinteger(L, ITEM_TYPE_SERVER);
        lua_rawset(L, -3);
    }
    lua_setmetatable(L, -2);
    
    // The following functions are nil by default but can be overriden:
    // - Server.DidReceiveEvent
    // - Server.OnPlayerJoin
    // - Server.Tick
    
    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
}

void lua_server_createAndPushForClient(lua_State * const L) {
    vx_assert(L != nullptr);
    LUAUTILS_STACK_SIZE(L)

    lua_newtable(L); // Server table
    lua_newtable(L); // metatable
    {
        lua_pushstring(L, "__index");
        lua_pushnil(L);
        lua_rawset(L, -3);

        lua_pushstring(L, "__newindex");
        lua_pushcfunction(L, _server_newindex_forClient, "");
        lua_rawset(L, -3);

        lua_pushstring(L, "__metatable"); // hide the metatable from the Lua sandbox user
        lua_pushboolean(L, false);
        lua_rawset(L, -3);

        lua_pushstring(L, P3S_LUA_OBJ_METAFIELD_TYPE); // used to log tables
        lua_pushinteger(L, ITEM_TYPE_SERVER);
        lua_rawset(L, -3);
    }
    lua_setmetatable(L, -2);

    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
}

/// Pushes the global Server table onto the stack.
bool lua_server_pushTable(lua_State * const L) {
    vx_assert(L != nullptr);
    LUAUTILS_STACK_SIZE(L)
    
    lua_utils_pushRootGlobalsFromRegistry(L);
    LUA_GET_METAFIELD(L, -1, P3S_LUA_G_SERVER);

    LUAUTILS_ASSERT_ARG_TYPE(L, -1, ITEM_TYPE_SERVER);
    
    lua_remove(L, -2); // remove Global table
    
    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
    return true;
}

bool lua_server_pushOnStartFunction(lua_State * const L) {
    vx_assert(L != nullptr);
    LUAUTILS_STACK_SIZE(L)
    const bool pushed = _server_pushFunction(L, LUA_SERVER_FIELD_ONSTART);
    LUAUTILS_STACK_SIZE_ASSERT(L, (pushed ? 1 : 0))
    return pushed;
}

static int _server_newindex_forClient(lua_State *L) {
    vx_assert(L != nullptr);
    LUAUTILS_STACK_SIZE(L)
    // does nothing
    LUAUTILS_STACK_SIZE_ASSERT(L, 0)
    return 0;
}

///
static int _server_newindex(lua_State *L) {
    vx_assert(L != nullptr);
    LUAUTILS_ASSERT_ARG_TYPE(L, 1, ITEM_TYPE_SERVER)
    LUAUTILS_ASSERT_ARGS_COUNT(L, 3)
    LUAUTILS_STACK_SIZE(L)
    
    // setting non-string or non-capitalized string keys is allowed
    if (lua_utils_isStringStrict(L, 2) == false || lua_utils_isStringStrictStartingWithUppercase(L, 2) == false) {
        
        if (LUA_GET_METAFIELD_AND_RETURN_TYPE(L, 1, "__index") == LUA_TNIL) {
            LUAUTILS_INTERNAL_ERROR(L)
        }
        
        // store the key/value pair
        lua_pushvalue(L, 2); // key
        lua_pushvalue(L, 3); // value
        lua_rawset(L, -3);
        
        lua_pop(L, 1); // pop __index
        
        LUAUTILS_STACK_SIZE_ASSERT(L, 0)
        return 0;
    }
    
    // key is a capitalized string
    const char *key = lua_tostring(L, 2);
    
    if (strcmp(key, LUA_SERVER_FIELD_DIDRECEIVEEVENT) == 0 ||
        strcmp(key, LUA_SERVER_FIELD_ONPLAYERJOIN) == 0 ||
        strcmp(key, LUA_SERVER_FIELD_ONPLAYERLEAVE) == 0 ||
        strcmp(key, LUA_SERVER_FIELD_TICK) == 0 ||
        strcmp(key, LUA_SERVER_FIELD_ONSTART) == 0) {

        if (lua_isfunction(L, 3) == false && lua_isnil(L, 3) == false) {
            LUAUTILS_ERROR_F(L, "Server.%s can only be a function or nil", key); // returns
        }
        
        if (LUA_GET_METAFIELD_AND_RETURN_TYPE(L, 1, "__index") == LUA_TNIL) {
            LUAUTILS_INTERNAL_ERROR(L)
        }
        
        const std::string listenerKey = std::string(key) + "Listener";
        
        // event key
        int eventID = 0;
        bool priority = false;
        if (strcmp(key, LUA_SERVER_FIELD_TICK) == 0) {
            eventID = LOCAL_EVENT_NAME_Tick;
        } else if (strcmp(key, LUA_SERVER_FIELD_DIDRECEIVEEVENT) == 0) {
            eventID = LOCAL_EVENT_NAME_DidReceiveEvent;
        } else if (strcmp(key, LUA_SERVER_FIELD_ONPLAYERJOIN) == 0) {
            eventID = LOCAL_EVENT_NAME_OnPlayerJoin;
        } else if (strcmp(key, LUA_SERVER_FIELD_ONPLAYERLEAVE) == 0) {
            eventID = LOCAL_EVENT_NAME_OnPlayerLeave;
        } else if (strcmp(key, LUA_SERVER_FIELD_ONSTART) == 0) {
            eventID = LOCAL_EVENT_NAME_OnStart;
            priority = true;
        }
        
        // Remove LocalEvent Listener if set
        lua_pushstring(L, listenerKey.c_str());
        lua_rawget(L, -2);
        if (lua_isnil(L, -1)) {
            lua_pop(L, 1); // pop nil
        } else {
            LOCAL_EVENT_LISTENER_CALL_REMOVE(L, -1)
        }

        lua_pushstring(L, listenerKey.c_str());
        if (lua_isnil(L, 3) == false) { // value
            if (priority) {
                LOCAL_EVENT_LISTEN_PRIORITY(L, eventID, 3);
            } else {
                LOCAL_EVENT_LISTEN(L, eventID, 3); // callbackIdx: 3
            }
        } else {
            lua_pushnil(L);
        }
        lua_rawset(L, -3);
        
        lua_pushvalue(L, 2); // key
        lua_pushvalue(L, 3); // value
        lua_rawset(L, -3);
        
        lua_pop(L, 1); // pop __index
        
        LUAUTILS_STACK_SIZE_ASSERT(L, 0)
        
    } else if (strcmp(key, LUA_SERVER_FIELD_ONSTART) == 0) {
        
        if (lua_isfunction(L, 3) == false && lua_isnil(L, 3) == false) {
            LUAUTILS_ERROR_F(L, "Server.%s can only be a function or nil", key); // returns
        }
        
        if (LUA_GET_METAFIELD_AND_RETURN_TYPE(L, 1, "__index") == LUA_TNIL) {
            LUAUTILS_INTERNAL_ERROR(L)
        }
        
        lua_pushvalue(L, 2); // key
        lua_pushvalue(L, 3); // value
        lua_rawset(L, -3);
        
        lua_pop(L, 1); // pop __index
        
        LUAUTILS_STACK_SIZE_ASSERT(L, 0)
        
    } else {
        // field is not settable
        vxlog_error("🌒⚠️ Server.%s field is not settable", key);
    }
    LUAUTILS_STACK_SIZE_ASSERT(L, 0)
    return 0;
}

///
static bool _server_pushFunction(lua_State * const L, const char *funcName) {
    vx_assert(L != nullptr);
    LUAUTILS_STACK_SIZE(L)
    if (lua_server_pushTable(L) == false) {
        LUAUTILS_STACK_SIZE_ASSERT(L, 0)
        return false;
    }
    
    if (lua_getfield(L, -1, funcName) == LUA_TNIL) {
        lua_pop(L, 2); // pop nil & Server table
        LUAUTILS_STACK_SIZE_ASSERT(L, 0)
        return false;
    }
    
    if (lua_type(L, -1) != LUA_TFUNCTION) {
        lua_pop(L, 2); // pops value and Server table
        LUAUTILS_STACK_SIZE_ASSERT(L, 0)
        return false;
    }
    
    lua_remove(L, -2); // removes Server table
    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
    return true;
}

int lua_server_snprintf(lua_State * const L, char* result, size_t maxLen, bool spacePrefix) {
    vx_assert(L != nullptr);
    return snprintf(result, maxLen, "%s[Server]", spacePrefix ? " " : "");
}

static int _server_push_unix_time(lua_State *L) {
    vx_assert(L != nullptr);
    LUAUTILS_STACK_SIZE(L)
    
    std::time_t t = std::time(nullptr);
    lua_pushinteger(L, static_cast<lua_Integer>(t));
    
    lua_log_warning(L, "Server.UnixTime is deprecated, use Time.Unix instead");
    
    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
    return 1;
}
