//
//  lua_time.hpp
//  Cubzh
//
//  Created by Adrien Duermael on 7/10/20.
//

#pragma once

// C
#include <stdint.h>
#include <cstddef>

// forward declarations
typedef struct lua_State lua_State;

#define LUA_TIME_FIELD_H "H"
#define LUA_TIME_FIELD_M "M"
#define LUA_TIME_FIELD_S "S"

//
void lua_g_time_create_and_push(lua_State *L);

/// Prints global Time
int lua_g_time_snprintf(lua_State *L,
                        char* result,
                        size_t maxLen,
                        bool spacePrefix);
