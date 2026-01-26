//
//  lua_face.hpp
//  Cubzh
//
//  Created by Xavier Legland on 27/04/2021.
//  Copyright © 2020 Voxowl Inc. All rights reserved.
//

#pragma once

// Lua
#include "lua.hpp"

// C
#include <stdint.h>

#define LUA_FACE_TYPE "face_type"
#define LUA_FACE_TYPE_RIGHT 0
#define LUA_FACE_TYPE_LEFT 1
#define LUA_FACE_TYPE_FRONT 2
#define LUA_FACE_TYPE_BACK 3
#define LUA_FACE_TYPE_TOP 4
#define LUA_FACE_TYPE_BOTTOM 5

/// Creates the metatables needed for the Face type,
/// and adds it in the lua registry
/// Returns true on success, false otherwise.
bool lua_g_face_createAndPush(lua_State *L);

/// Pushes a Face table onto the Lua stack
bool lua_face_pushTable(lua_State *L, uint8_t face);

/// Returns int representation of the face for the given index
int lua_face_getFaceInt(lua_State *L, int idx);

/// Should be called with a Face at the top of the stack,
/// returns description string for it.
int lua_face_snprintf(lua_State *L,
                      char *result,
                      size_t maxLen,
                      bool spacePrefix);
