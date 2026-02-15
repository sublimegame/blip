//
//  lua_logs.cpp
//  Cubzh
//
//  Created by Adrien Duermael on 30/06/2021.
//

#include "lua_logs.hpp"

#include "lua.hpp"
#include "VXGame.hpp"
#include "vxlog.h"
#include "lua_local_event.hpp"

#include "lua_utils.hpp"

void lua_log_info_CStr(lua_State * const L, const char* str) {
    lua_log_info(L, std::string(str));
}

// prints a simple log in the game console
void lua_log_info(lua_State * const L, const std::string& str) {
    if (L == nullptr) return;
    vx::Game *game;
    lua_utils_getGameCppWrapperPtr(L, &game);
    lua_log_info_with_game(game, str);
    vxlog_info(str.c_str());
}

void lua_log_info_with_game(vx::Game *game, const std::string& str) {
    if (game->isInServerMode()) {
        std::vector<uint8_t> recipientIds;
        recipientIds.push_back(PLAYER_ID_ALL);

        vx::GameMessage_SharedPtr msg = vx::GameMessage::makeGameEvent(EVENT_TYPE_SERVER_LOG_INFO,
                                                                       PLAYER_ID_SERVER,
                                                                       recipientIds,
                                                                       reinterpret_cast<const uint8_t *>(str.c_str()),
                                                                       str.size());
        game->sendMessage(msg);
    }
    
    lua_State *L = game->getLuaState();
    vx_assert(L != nullptr);

    LOCAL_EVENT_SEND_PUSH(L, LOCAL_EVENT_NAME_InfoMessage);
    lua_pushstring(L, str.c_str());
    LOCAL_EVENT_SEND_CALL(L, 1);
}

void lua_log_error_CStr(lua_State * const L, const char* str) {
    lua_log_error(L, std::string(str));
}

void lua_log_error(lua_State * const L, const std::string& str) {
    if (L == nullptr) return;
    vx::Game *game;
    lua_utils_getGameCppWrapperPtr(L, &game);
    lua_log_error_with_game(game, str);
    vxlog_error("%s", str.c_str());
}

void lua_log_error_with_game(vx::Game *game, const std::string& str) {
    if (game->isInServerMode()) {
        std::vector<uint8_t> recipientIds;
        recipientIds.push_back(PLAYER_ID_ALL);
        
        vx::GameMessage_SharedPtr msg = vx::GameMessage::makeGameEvent(EVENT_TYPE_SERVER_LOG_ERROR,
                                                                       PLAYER_ID_SERVER,
                                                                       recipientIds,
                                                                       reinterpret_cast<const uint8_t *>(str.c_str()),
                                                                       str.size());
        game->sendMessage(msg);
    }

    lua_State *L = game->getLuaState();
    vx_assert(L != nullptr);

    lua_getglobal(L, "LocalEvent");
    if (lua_isnil(L, -1)) {
        vxlog_debug("LocalEvent IS NIL!");
    } else {
        lua_getfield(L, -1, "Send");
        if (lua_isnil(L, -1)) {
            vxlog_debug("LocalEvent.Send IS NIL!");
        }
        lua_pop(L, 1);
    }
    lua_pop(L, 1);

    LOCAL_EVENT_SEND_PUSH(L, LOCAL_EVENT_NAME_ErrorMessage);
    lua_pushstring(L, str.c_str());
    LOCAL_EVENT_SEND_CALL(L, 1);
}

void lua_log_warning_CStr(lua_State * const L, const char* str) {
    lua_log_warning(L, std::string(str));
}

void lua_log_warning(lua_State * const L, const std::string& str) {
    if (L == nullptr) return;
    vx::Game *game;
    lua_utils_getGameCppWrapperPtr(L, &game);
    lua_log_warning_with_game(game, str);
    vxlog_warning("%s", str.c_str());
}

void lua_log_warning_with_game(vx::Game *game, const std::string& str) {
    if (game->isInServerMode()) {
        std::vector<uint8_t> recipientIds;
        recipientIds.push_back(PLAYER_ID_ALL);

        vx::GameMessage_SharedPtr msg = vx::GameMessage::makeGameEvent(EVENT_TYPE_SERVER_LOG_WARNING,
                                                                       PLAYER_ID_SERVER,
                                                                       recipientIds,
                                                                       reinterpret_cast<const uint8_t *>(str.c_str()),
                                                                       str.size());
        game->sendMessage(msg);
    }
    
    lua_State *L = game->getLuaState();
    vx_assert(L != nullptr);

    LOCAL_EVENT_SEND_PUSH(L, LOCAL_EVENT_NAME_WarningMessage);
    lua_pushstring(L, str.c_str());
    LOCAL_EVENT_SEND_CALL(L, 1);
}
