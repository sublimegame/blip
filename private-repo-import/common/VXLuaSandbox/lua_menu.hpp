//
//  lua_menu.hpp
//  Cubzh
//
//  Created by Adrien Duermael on 18/07/2023.
//

// Mneu is defined as a Lua module.
// This is just a set of defines to easily use it in C++.

#pragma once

#include "lua_logs.hpp"
#include "lua_system.hpp"
#include "lua_utils.hpp"

#define MENU_PUSH_HEIGHT(L) lua_getglobal(L, "Menu"); \
lua_getfield(L, -1, "Height"); \
lua_remove(L, -2);

#define MENU_CALL_SYSTEM_LOADING(L, message) lua_getglobal(L, "Menu"); \
lua_getfield(L, -1, "loading"); \
lua_pushvalue(L, -2); \
lua_remove(L, -3); \
lua_pushstring(L, message); \
lua_system_table_push(L); \
if (lua_pcall(L, 3, 0, 0) != LUA_OK) { \
    if (lua_utils_isStringStrict(L, -1)) { \
        lua_log_error_CStr(L, lua_tostring(L, -1)); \
    } else { \
        lua_log_error(L, "Menu.loading error"); \
    } \
    lua_pop(L, 1); \
}

#define MENU_CALL_SYSTEM_HIDE_LOADING(L) lua_getglobal(L, "Menu"); \
lua_getfield(L, -1, "hideLoading"); \
lua_pushvalue(L, -2); \
lua_remove(L, -3); \
lua_system_table_push(L); \
if (lua_pcall(L, 2, 0, 0) != LUA_OK) { \
    if (lua_utils_isStringStrict(L, -1)) { \
        lua_log_error_CStr(L, lua_tostring(L, -1)); \
    } else { \
        lua_log_error(L, "Menu.hideLoading error"); \
    } \
    lua_pop(L, 1); \
}

#define MENU_OPEN_URL_WITH_WARN(L, urlString) \
lua_getglobal(L, "Menu"); \
lua_getfield(L, -1, "openURLWarning"); \
lua_pushvalue(L, -2); \
lua_remove(L, -3); \
lua_pushstring(L, urlString); \
lua_system_table_push(L); \
if (lua_pcall(L, 3, 0, 0) != LUA_OK) { \
    if (lua_utils_isStringStrict(L, -1)) { \
        lua_log_error_CStr(L, lua_tostring(L, -1)); \
    } else { \
        lua_log_error(L, "Menu.openURLWarning error"); \
    } \
    lua_pop(L, 1); \
}
