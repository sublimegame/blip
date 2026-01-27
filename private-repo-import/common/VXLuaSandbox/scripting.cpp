//
//  scripting.cpp
//  Cubzh
//
//  Created by Gaetan de Villele on 12/06/2019.
//  Copyright © 2019 voxowl. All rights reserved.
//

#include "scripting.hpp"

// engine
#include "camera.h"
#include "game.h"
#include "strings.hpp"
#include "filo_list_uint16.h"

// VX Framework
#include "VXAccount.hpp"
#include "VXHubClient.hpp"
#include "VXGameClient.hpp"
#include "VXGame.hpp"
#include "GameCoordinator.hpp"
#include "VXLuaExecutionLimiter.hpp"

// Lua
#include "lua.hpp"
#include "lua_utils.hpp"
#include "lua_logs.hpp"
#include "lua_http.hpp"
#include "lua_require.hpp"
#include "lua_assets.hpp"

// xptools
#include "xptools.h"

// sandbox
#include "sbs.hpp"
#include "lua_client.hpp"
#include "lua_modules.hpp"
#include "lua_event.hpp"
#include "lua_player.hpp"
#include "lua_players.hpp"
#include "lua_pointer.hpp"
#include "lua_pointer_event.hpp"
#include "lua_sandbox.hpp"
#include "lua_object.hpp"
#include "lua_screen.hpp"
#include "lua_local_event.hpp"
#include "lua_system.hpp"

#include "vxlog.h"

static const char *scriptPrefixClient = R"""(

-- require early to catch all messages
-- including script load error ones.
require("chat")

Client.OnChat = function(payload)
    -- ok
end

Client.DirectionalPad = function(x, y)
    Player.Motion = (Player.Forward * y + Player.Right * x) * 50
end

Pointer.Drag = function(pointerEvent)
    local dx = pointerEvent.DX
    local dy = pointerEvent.DY

    -- Player.LocalRotation.Y = Player.LocalRotation.Y + dx * 0.01
    -- Player:RotateLocal(0, dx * 0.01, 0)
    Player.LocalRotation = Rotation(0, dx * 0.01, 0) * Player.LocalRotation

    -- Player.Head.LocalRotation.X = Player.Head.LocalRotation.X + -dy * 0.01
    -- Player.Head:RotateLocal(-dy * 0.01, 0, 0)
    Player.Head.LocalRotation = Rotation(-dy * 0.01, 0, 0) * Player.Head.LocalRotation
    
    local dpad = require("controls").DirectionalPadValues
    Player.Motion = (Player.Forward * dpad.Y + Player.Right * dpad.X) * 50
end

Client.AnalogPad = function(dx, dy)
    Player.LocalRotation = Rotation(0, dx * 0.01, 0) * Player.LocalRotation
    Player.Head.LocalRotation = Rotation(-dy * 0.01, 0, 0) * Player.Head.LocalRotation

    local dpad = require("controls").DirectionalPadValues
    Player.Motion = (Player.Forward * dpad.Y + Player.Right * dpad.X) * 50
end

if AudioListener ~= nil then
    AudioListener:SetParent(Camera)
end

)""";

static int _user_env_newindex(lua_State * const L);
static int _newindex_readonly(lua_State * const L);

// --------------------------------------------------
//
// MARK: - Unexposed functions
// 
// --------------------------------------------------

/// Creates a new Lua state for the provided game, using the provided game script.
/// Also stores the memory address of the Game in the Lua registry of the state
/// in order to access the Game later in the life of the lua state.
/// (since Game is NOT a global singleton anymore)
bool scripting_loadScript(vx::Game *game,
                          lua_State **_L) {
    vx_assert(_L != nullptr);
    vx_assert(game != nullptr);
    
    vx::GameData_SharedPtr data = game->getData();
    if (data == nullptr) {
        vxlog_error("game data is NULL");
        return false;
    }

    // If there is already a Lua state, we close it before creating a new one.
    if (*_L != nullptr) {
        lua_close(*_L);
        *_L = nullptr;
    }

    // init new Lua state
    lua_State *L = luaL_newstate();
    if (L == nullptr) {
        vxlog_error("Lua state creation failed");
        return false;
    }
    
    *_L = L;

    LUAUTILS_STACK_SIZE(L)
    
    if (lua_utils_setGamePtr(L, game->getCGame()) == false) {
        return false;
    }
    
    // store World pointer inside the lua state
    if (lua_utils_setGameCppWrapperPtr(L, game) == false) {
        return false;
    }

    lua_init_root_and_system_globals(*_L);

    if (game->isInClientMode()) {
        // setup Lua sandbox
        lua_sandbox_client_setup_pre_load(*_L, game->getEnvironment());

        // *_L = lua_newthread(*_L);
        // luaL_sandboxthread(*_L); // creates Global table for _L proxying reads to parent thread globals

        // Load default script
        if (luau_dostring(*_L, scriptPrefixClient, "preScript") == false) {
            lua_close(*_L);
            *_L = nullptr;
            return false; // failure
        }
    } else { // server
        // setup Lua sandbox
        lua_sandbox_server_setup_pre_load(*_L);

        // *_L = lua_newthread(*mainL);
        // luaL_sandboxthread(*_L); // creates Global table for _L proxying reads to parent thread globals
    }

    // LUAUTILS_STACK_SIZE_ASSERT(L, 1) // 1 thread

    lua_sandbox_globals(*_L); // install sandboxed globals for world

    lua_pushvalue(*_L, LUA_GLOBALSINDEX);
    lua_getmetatable(*_L, -1);
    lua_setreadonly(*_L, -1, false);
    lua_pushstring(*_L, "__newindex");
    lua_pushcfunction(*_L, _user_env_newindex, "");
    lua_rawset(*_L, -3);
    lua_setreadonly(*_L, -1, true);
    lua_pop(*_L, 2);

    // luau_dostring(*_L, data->getScript(), "script");

    if (luau_loadstring(L, data->getScript(), "script") == false) {
        std::string err = lua_tostring(L, -1);
        // lua_log_error_with_game(g, err);
        lua_log_error_CStr(L, err.c_str());
        lua_pop(L, 1);
        LUAUTILS_STACK_SIZE_ASSERT(L, 0)
        return true;
    }

    if (lua_pcall(L, 0, LUA_MULTRET, 0) != 0) {
        std::string err = "";
        if (lua_isstring(L, -1)) {
            err = lua_tostring(L, -1);
        } else {
            err = "unspecified error";
        }
        lua_log_error_CStr(L, err.c_str());
        lua_pop(L, 1);
        LUAUTILS_STACK_SIZE_ASSERT(L, 0)
        return true;
    }

//    std::string bytecode = luau_compile(data->getScript());
//    if (luau_load(*_L, "script", bytecode.data(), bytecode.size(), 0) == 0)
//    {
//        int status = lua_resume(*_L, *mainL, 0);
//        if (status == 0) {
//            // OK!
//        } else if (status == LUA_YIELD) {
//            vxlog_error("main script can not yield");
//        } else if (lua_isstring(*_L, -1)) {
//            vxlog_error("error while running main script: %s", lua_tostring(L, -1));
//        } else {
//            vxlog_error("unknown error while running main script");
//        }
//    }

//    if (luau_loadstring(*_L, data->getScript(), "script") == false) {
//        std::string err(lua_tostring(*_L, -1));
//        lua_pop(*_L, 1); // pop error message
//        lua_log_error_with_game(game, err);
//        // CONTINUE ANYWAY TO SETUP USER ENV
//        // + users should be able to modify the script
//        // load empty string instead
//        // for lua_pcall to fail down the road
//        luau_loadstring(*_L, "", "script");
//    }

//    // TODO: use lua_resume instead of pcall
//    if (lua_pcall(*_L, 0, LUA_MULTRET, 0) != LUA_OK) {
//        std::string err = "";
//        if (lua_isstring(*_L, -1)) {
//            err = lua_tostring(*_L, -1);
//        } else {
//            err = "load script error";
//        }
//        lua_pop(*_L, 1); // pop error message
//        lua_log_error_with_game(game, err);
//        // CONTINUE ANYWAY
//        // users should be able to modify the script
//    }
    
    // NOTE: Config parsed and injected in CGame in lua_config
    // TODO -> move config to CPP Game

    // lua_pop(L, 1); // pop thread

    // ! \\ let's check how garbage collection works for threads

    LUAUTILS_STACK_SIZE_ASSERT(L, 0)
    return true; // success
}

// --------------------------------------------------
//
// MARK: - C - Lua interface
//
// --------------------------------------------------

bool scripting_addPlayer(vx::Game* g,
                         const uint8_t playerID,
                         const std::string &username,
                         const std::string &userID,
                         bool isLocalPlayer) {
    
    vx_assert(g != nullptr);
    
    if (game_isPlayerIDValid(playerID) == false) {
        vxlog_error("[%s:%d] playerID is not valid", __func__, __LINE__);
        return false;
    }

    lua_State *L = g->getLuaState();
    if (L == nullptr) {
        vxlog_error("[%s:%d] adding player before Lua state is available", __func__, __LINE__);
        return false;
    }
    LUAUTILS_STACK_SIZE(L)
 
    // this will update local player table with tmp ID if isLocalPlayer == true
    bool ok = lua_player_createAndPushPlayerTable(L, playerID, username.c_str(), userID.c_str(), isLocalPlayer);
    if (ok == false) {
        vxlog_error("[%s:%d] %s", __func__, __LINE__, "scripting_addPlayer failed");
        LUAUTILS_STACK_SIZE_ASSERT(L, 0)
        return false;
    }
    if (lua_g_players_insertPlayerTable(L, -1) == false) {
        vxlog_error("[%s:%d] %s", __func__, __LINE__, "scripting_addPlayer failed");
        lua_pop(L, 1);
        LUAUTILS_STACK_SIZE_ASSERT(L, 0)
        return false;
    }
    if (isLocalPlayer == false && g->isInClientMode()) {
        if (lua_g_otherPlayers_insertPlayerTable(L, -1) == false) {
            vxlog_error("[%s:%d] %s", __func__, __LINE__, "scripting_addPlayer failed");
            lua_pop(L, 1);
            LUAUTILS_STACK_SIZE_ASSERT(L, 0)
            return false;
        }
    }
    lua_pop(L, 1); // pops the newly created Player table

    LUAUTILS_STACK_SIZE_ASSERT(L, 0)
    return true;
}

bool scripting_onPlayerJoin(vx::Game_SharedPtr &g, const uint8_t playerID) {
    vx_assert(g != nullptr);

    if (game_isPlayerIDValid(playerID) == false) {
        vxlog_error("[%s:%d] playerID is not valid", __func__, __LINE__);
        return false;
    }
    
    // retrieve Lua state from Game
    lua_State *L = g->getLuaState();
    if (L == nullptr) { return false; }
    LUAUTILS_STACK_SIZE(L)
    
    LOCAL_EVENT_SEND_PUSH_WITH_SYSTEM(L, LOCAL_EVENT_NAME_OnPlayerJoin)
    if (lua_g_players_pushPlayerTable(L, playerID) == false) {
        LUAUTILS_INTERNAL_ERROR(L)
    }
    LOCAL_EVENT_SEND_CALL_WITH_SYSTEM(L, 1)
    
    LUAUTILS_STACK_SIZE_ASSERT(L, 0)
    return true;
}

static int on_done(lua_State *const L) {
    // L -> game -> started() qui change le state (setState(started))
    vx::Game *cppGame = nullptr;
    if (lua_utils_getGameCppWrapperPtr(L, &cppGame) == false) {
        LUAUTILS_INTERNAL_ERROR(L)
    }
    vx_assert(cppGame != nullptr);

    cppGame->onStart();

    return 0;
}

static int on_load(lua_State *const L) {
    LUAUTILS_STACK_SIZE(L)
    LOCAL_EVENT_SEND_PUSH(L, LOCAL_EVENT_NAME_OnWorldObjectLoad);
    lua_pushvalue(L, 1);
    LOCAL_EVENT_SEND_CALL(L, 1);
    LUAUTILS_STACK_SIZE_ASSERT(L, 0)
    return 0;
}

// call Lua function Client.OnStart
void scripting_loadWorld_onStart(vx::Game* g) {
    vx_assert(g != nullptr);

    // reset allowed rotations
    vx::device::setScreenAllowedOrientation("all");

    g->doString("require(\"controls\")");
    
    // retrieve Lua state from Game
    lua_State *L = g->getLuaState();
    vx_assert(L != nullptr);

    LUAUTILS_STACK_SIZE(L)

    if (g->getData()->getMapBase64().empty()) {
        g->onStart();
    } else {
        const std::string mapBase64 = g->getData()->getMapBase64();
        
        lua_require(L, "world");
        lua_getfield(L, -1, "load");
        lua_remove(L, -2);
        lua_newtable(L); // config table
        {
            lua_pushstring(L, "b64");
            lua_pushstring(L, mapBase64.c_str());
            lua_rawset(L, -3);

            lua_pushstring(L, "onDone");
            lua_pushcfunction(L, on_done, "");
            lua_rawset(L, -3);

            lua_pushstring(L, "onLoad");
            lua_pushcfunction(L, on_load, "");
            lua_rawset(L, -3);

            lua_pushstring(L, "skipMap");
            lua_pushboolean(L, true);
            lua_rawset(L, -3);
        }
        if (lua_pcall(L, 1, 0, 0) != LUA_OK) {
            if (lua_utils_isStringStrict(L, -1)) {
                lua_log_error_CStr(L, lua_tostring(L, -1));
            } else {
                lua_log_error_CStr(L, "can't load world");
            }
            lua_pop(L, 1); // pop error
        }
    }
    LUAUTILS_STACK_SIZE_ASSERT(L, 0)
}

void scripting_client_didReceiveEvent(CGame * const g,
                                      const uint32_t eventType,
                                      const uint8_t senderID,
                                      const DoublyLinkedListUint8 *recipients,
                                      const uint8_t *data,
                                      const size_t size) {
    vx_assert(g != nullptr);
    vx_assert(recipients != nullptr);
    vx_assert(data != nullptr);
    
    vx::Game* cppGame = reinterpret_cast<vx::Game*>(game_get_cpp_wrapper(g));
    vx_assert(cppGame != nullptr);

    lua_State * const L = cppGame->getLuaState();
    vx_assert(L != nullptr);
    LUAUTILS_STACK_SIZE(L)

    LOCAL_EVENT_SEND_PUSH(L, LOCAL_EVENT_NAME_DidReceiveEvent)
    lua_event_create_and_push(L, eventType, senderID, recipients, data, size);
    LOCAL_EVENT_SEND_CALL(L, 1)

    LUAUTILS_STACK_SIZE_ASSERT(L, 0)
}

void scripting_client_didReceiveChatMessage(const vx::Game_SharedPtr& g,
                                            const uint8_t& senderID,
                                            const std::string& senderUserID,
                                            const std::string& senderUsername,
                                            const std::string& localUUID,
                                            const std::string& uuid,
                                            const std::vector<uint8_t>& recipients,
                                            const std::string& chatMessage) {
    lua_State * const L = g->getLuaState();
    vx_assert(L != nullptr);
    LUAUTILS_STACK_SIZE(L)

    LOCAL_EVENT_SEND_PUSH_WITH_SYSTEM(L, LOCAL_EVENT_NAME_ChatMessage)
    lua_pushstring(L, chatMessage.c_str());
    lua_pushinteger(L, senderID); // sender connection ID when message is sent
    lua_pushstring(L, senderUserID.c_str());
    lua_pushstring(L, senderUsername.c_str());
    lua_pushstring(L, "ok");
    lua_pushstring(L, uuid.c_str());
    lua_pushstring(L, localUUID.c_str());
    LOCAL_EVENT_SEND_CALL_WITH_SYSTEM(L, 7)

    LUAUTILS_STACK_SIZE_ASSERT(L, 0)
}

void scripting_client_didReceiveChatMessageACK(const vx::Game_SharedPtr& g,
                                               const std::string& localUUID,
                                               const std::string& uuid,
                                               const uint8_t& status) {
    lua_State * const L = g->getLuaState();
    vx_assert(L != nullptr);
    LUAUTILS_STACK_SIZE(L)

    LOCAL_EVENT_SEND_PUSH_WITH_SYSTEM(L, LOCAL_EVENT_NAME_ChatMessageACK)
    lua_pushstring(L, uuid.c_str());
    lua_pushstring(L, localUUID.c_str());
    if (status == 0) {
        lua_pushstring(L, "ok");
    } else if (status == 2) {
        lua_pushstring(L, "reported");
    } else {
        lua_pushstring(L, "error");
    }
    LOCAL_EVENT_SEND_CALL_WITH_SYSTEM(L, 3)

    LUAUTILS_STACK_SIZE_ASSERT(L, 0)
}

void scripting_do_string_called_from_game(vx::Game *g, const char* luaCode) {
    if (g == nullptr) { return; }

    lua_State * const L = g->getLuaState();
    if (L == nullptr) { return; }
    
    LUAUTILS_STACK_SIZE(L)

    std::string s = std::string(luaCode);

    if (luau_loadstring(L, s, "dostring") == false) {
         std::string err = lua_tostring(L, -1);
         lua_log_error_with_game(g, err);
         lua_pop(L, 1);
        return;
    }
    
    if (lua_pcall(L, 0, LUA_MULTRET, 0) != 0) {
        std::string err = "";
        if (lua_isstring(L, -1)) {
            err = lua_tostring(L, -1);
        } else {
            err = "unspecified error";
        }
        lua_log_error_with_game(g, err);
        lua_pop(L, 1);
    }

    LUAUTILS_STACK_SIZE_ASSERT(L, 0)
}

void scripting_enableHTTP(const vx::Game_SharedPtr& g) {
    lua_State *L = g->getLuaState();
    LUAUTILS_STACK_SIZE(L)

    lua_utils_pushRootGlobalsFromRegistry(L);
    LUAUTILS_STACK_SIZE_ASSERT(L, 1)

    if (lua_getmetatable(L, -1) != 1) {
        vxlog_error("Couldn't get globals table metatable");
        LUAUTILS_INTERNAL_ERROR(L)
    }
    LUAUTILS_STACK_SIZE_ASSERT(L, 2)

    // HTTP global
    lua_pushstring(L, LUA_G_HTTP);
    LUAUTILS_STACK_SIZE_ASSERT(L, 3)
    lua_g_http_create_and_push(L);
    LUAUTILS_STACK_SIZE_ASSERT(L, 4)
    lua_rawset(L, -3);
    LUAUTILS_STACK_SIZE_ASSERT(L, 2)
    lua_pop(L, 2);
    
    LUAUTILS_STACK_SIZE_ASSERT(L, 0)
}

// NOT WITHIN CGame TICK
void scripting_tick(vx::Game* g, CGame *cgame, const TICK_DELTA_SEC_T dt) {
    vx_assert(g != nullptr);
    
    lua_State *L = g->getLuaState();
    if (L == nullptr) { return; }
    
    LUAUTILS_STACK_SIZE(L)

    LOCAL_EVENT_SEND_PUSH(L, LOCAL_EVENT_NAME_Tick)
    lua_pushnumber(L, dt);
    LOCAL_EVENT_SEND_CALL(L, 1)

    lua_object_gc_finalize(g->userdataStoreForObjects, cgame, L);
    lua_assets_request_gc_finalize(g->userdataStoreForAssetsRequests, L);

    LUAUTILS_STACK_SIZE_ASSERT(L, 0)
}

void scripting_object_tick(CGame *g, Transform *t, const TICK_DELTA_SEC_T dt) {
    vx::Game* cppGame = reinterpret_cast<vx::Game*>(game_get_cpp_wrapper(g));
    vx_assert(cppGame != nullptr);
    lua_State *L = cppGame->getLuaState();
    vx_assert(L != nullptr);
    lua_object_call_tick(L, t, dt);
}

void scripting_object_onCollision(CGame *g, Transform *self, Transform *other, float3 wNormal, uint8_t type) {
    vx_assert(g != nullptr);
    vx::Game* cppGame = reinterpret_cast<vx::Game*>(game_get_cpp_wrapper(g));
    vx_assert(cppGame != nullptr);
    
    lua_State *L = cppGame->getLuaState();
    vx_assert(L != nullptr);

    std::string funcName;
    switch(static_cast<CollisionCallbackType>(type)) {
        case CollisionCallbackType_Begin: funcName = LUA_OBJECT_FIELD_ONCOLLISIONBEGIN; break;
        case CollisionCallbackType_Tick: funcName = LUA_OBJECT_FIELD_ONCOLLISION; break;
        case CollisionCallbackType_End: funcName = LUA_OBJECT_FIELD_ONCOLLISIONEND; break;
    }
    
    if (cppGame->getLuaExecutionLimiter().isFuncDisabled(funcName)) {
        return; // this function is disabled
    }
    
    cppGame->getLuaExecutionLimiter().startLimitation(L, funcName);

    lua_object_get_and_call_collision_callback(L, self, other, wNormal, type);
    
    cppGame->getLuaExecutionLimiter().cancelLimitation(L);
}

void scripting_object_onDestroy(const uint16_t id, void *managed) {
    CGame *cgame = static_cast<CGame *>(weakptr_get(static_cast<Weakptr *>(managed)));
    if (cgame == nullptr) {
        return;
    }
    filo_list_uint16_push(game_get_managedTransformIDsToRemove(cgame), id);
}

static int _user_env_newindex(lua_State * const L) {
    vx_assert(L != nullptr);
    LUAUTILS_ASSERT_ARGS_COUNT(L, 3)
    LUAUTILS_STACK_SIZE(L)
    
    // Trying to set a system variable, it's only allowed for a few of them.
    if (lua_utils_isStringStrictStartingWithUppercase(L, 2)) {
        
        const char *key = lua_tostring(L, 2);
        
        if (strcmp(key, LUA_G_MODULES) == 0) {
            // setting Config like this:
            // Modules = { ui = "uikit", "multi", pathfinding = "github.com/aduermael/pathfinding" }

            lua_getglobal(L, LUA_G_MODULES);
            if (lua_utils_getObjectType(L, -1) != ITEM_TYPE_G_MODULES) {
                LUAUTILS_INTERNAL_ERROR(L) // returns
            }

            // Modules should be at -1 when calling this
            lua_modules_set_from_table(L, 3);
            lua_pop(L, 1); // pop Modules

        } else if (strcmp(key, LUA_GLOBAL_FIELD_CONFIG) == 0) {
            // setting Config like this:
            // Config = { Items = {"foo", "bar"}, Map = "baz" }
            
            lua_getglobal(L, LUA_GLOBAL_FIELD_CONFIG);
            if (lua_utils_getObjectType(L, -1) != ITEM_TYPE_CONFIG) {
                LUAUTILS_INTERNAL_ERROR(L) // returns
            }

            // Config should be at -1 when calling this
            lua_config_set_from_table(L, 3 /* Config table idx */);
            lua_pop(L, 1); // pop Config
        } else {
            LUAUTILS_ERROR_F(L, "\"%s\" can't be set, global variables starting with an uppercase letter are reserved", key);
        }
    } else {
        // user defined global variable
        lua_pushvalue(L, 2); // key
        lua_pushvalue(L, 3); // value
        lua_rawset(L, 1);
    }
    LUAUTILS_STACK_SIZE_ASSERT(L, 0)
    return 0;
}

static int _newindex_readonly(lua_State * const L) {
    vx_assert(L != nullptr);
    LUAUTILS_ASSERT_ARGS_COUNT(L, 3)
    LUAUTILS_STACK_SIZE(L)
    LUAUTILS_ERROR_F(L, "can't set table.%s (table is read-only)", lua_tostring(L, 2))
    LUAUTILS_STACK_SIZE_ASSERT(L, 0)
    return 0;
}
