//
//  lua_client.hpp
//  Cubzh
//
//  Created by Gaetan de Villele on 20/12/2019.
//  Copyright © 2019 Voxowl Inc. All rights reserved.
//

#pragma once

typedef struct lua_State lua_State;
typedef struct _CGame CGame;

#define LUA_CLIENT_FIELD_CLOUDS               "Clouds"             // Table
#define LUA_CLIENT_FIELD_FOG                  "Fog"                // Table
#define LUA_CLIENT_FIELD_SKY                  "Sky"                // Table
#define LUA_CLIENT_FIELD_PLAYER               "Player"             // Table
#define LUA_CLIENT_FIELD_SCREEN               "Screen"             // Table

#define LUA_CLIENT_FIELD_OS_NAME              "OSName"             // string
#define LUA_CLIENT_FIELD_OS_VERSION           "OSVersion"          // string
#define LUA_CLIENT_FIELD_IS_MOBILE            "IsMobile"           // bool
#define LUA_CLIENT_FIELD_IS_PC                "IsPC"               // bool
#define LUA_CLIENT_FIELD_IS_CONSOLE           "IsConsole"          // bool
#define LUA_CLIENT_FIELD_HAS_TOUCHSCREEN      "HasTouchScreen"     // bool

#define LUA_CLIENT_FIELD_APP_VERSION          "AppVersion"         // string
#define LUA_CLIENT_FIELD_BUILD_NUMBER         "BuildNumber"        // int

#define LUA_CLIENT_FIELD_INPUTS               "Inputs"             // Table
#define LUA_CLIENT_FIELD_OSTEXTINPUT          "OSTextInput"        // Table
#define LUA_CLIENT_FIELD_TICK                 "Tick"               // function
#define LUA_CLIENT_FIELD_DIDRECEIVEEVENT      "DidReceiveEvent"    // function
#define LUA_CLIENT_FIELD_DIRECTIONALPAD       "DirectionalPad"     // function
#define LUA_CLIENT_FIELD_ANALOGPAD            "AnalogPad"          // function
#define LUA_CLIENT_FIELD_ACTION_1             "Action1"            // function
#define LUA_CLIENT_FIELD_ACTION_1_RELEASE     "Action1Release"     // function
#define LUA_CLIENT_FIELD_ACTION_2             "Action2"            // function
#define LUA_CLIENT_FIELD_ACTION_2_RELEASE     "Action2Release"     // function
#define LUA_CLIENT_FIELD_ACTION_3             "Action3"            // function
#define LUA_CLIENT_FIELD_ACTION_3_RELEASE     "Action3Release"     // function
#define LUA_CLIENT_FIELD_ONPLAYERJOIN         "OnPlayerJoin"       // function (R/W)
#define LUA_CLIENT_FIELD_ONPLAYERLEAVE        "OnPlayerLeave"      // function (R/W)
#define LUA_CLIENT_FIELD_ONSTART              "OnStart"            // function (R/W)
#define LUA_CLIENT_FIELD_ONCHAT               "OnChat"             // function (R/W)
#define LUA_CLIENT_FIELD_ONWORLDOBJECTLOAD    "OnWorldObjectLoad"  // function (R/W)

#define LUA_CLIENT_FIELD_CONNECTING_TO_SERVER      "ConnectingToServer" // function
#define LUA_CLIENT_FIELD_SERVER_CONNECTION_LOST    "ServerConnectionLost" // function
#define LUA_CLIENT_FIELD_SERVER_CONNECTION_FAILED  "ServerConnectionFailed" // function
#define LUA_CLIENT_FIELD_SERVER_CONNECTION_SUCCESS "ServerConnectionSuccess" // function
#define LUA_CLIENT_FIELD_CONNECTED "Connected" // read-only boolean

// Creates and pushes a Client table
void lua_client_createAndPush(lua_State * const L);

/// Pushes the Client metatable on top of the lua stack
bool lua_client_pushMetaAttrs(lua_State * const L);

/// Retrieves the "Client.Tick" function and pushes it onto the Lua stack.
/// Returns whether the operation has succeeded.
/// If the operation succeeds, the Client.Tick function has been pushed onto the
/// Lua stack, otherwise nothing has been pushed on the stack.
bool lua_client_pushFunc(lua_State * const L, const char *function);

/// Pushes client field at given key.
/// Returns false and pushes nothing if not found.
bool lua_client_pushField(lua_State * const L, const char *key);
