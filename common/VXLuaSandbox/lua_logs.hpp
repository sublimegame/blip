//
//  lua_logs.hpp
//  Cubzh
//
//  Created by Adrien Duermael on 30/06/2021.
//

#pragma once

// C++
#include <string>

// forward declaration
typedef struct lua_State lua_State;
namespace vx { class Game; }

// prints a simple log in the game console
void lua_log_info_CStr(lua_State *L, const char* str);

// prints a simple log in the game console
void lua_log_info(lua_State *L, const std::string& str);

// a bit faster compared to lua_log_info, in case game reference is available
void lua_log_info_with_game(vx::Game *game, const std::string& str);

// prints a simple log in the game console
void lua_log_error_CStr(lua_State *L, const char* str);

// prints a simple log in the game console
void lua_log_error(lua_State *L, const std::string& str);

// a bit faster compared to lua_log_error, in case game reference is available
void lua_log_error_with_game(vx::Game *game, const std::string& str);

// prints a simple log in the game console
void lua_log_warning_CStr(lua_State *L, const char* str);

// prints a simple log in the game console
void lua_log_warning(lua_State *L, const std::string& str);

// a bit faster compared to lua_log_warning, in case game reference is available
void lua_log_warning_with_game(vx::Game *game, const std::string& str);


