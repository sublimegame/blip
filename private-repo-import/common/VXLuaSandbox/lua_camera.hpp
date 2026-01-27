//
//  lua_camera.hpp
//  Cubzh
//
//  Created by Gaetan de Villele on 05/05/2020.
//  Copyright © 2020 Voxowl Inc. All rights reserved.
//

#pragma once

// C++
#include <cstddef>

#include "camera.h"
#include "game.h"

typedef struct lua_State lua_State;

// MARK: - Light global table -

void lua_g_camera_createAndPush(lua_State * const L);

// MARK: - Light instances tables -

bool lua_camera_pushNewInstance(lua_State * const L, Camera *c);
Camera *lua_camera_getCameraPtr(lua_State * const L, const int idx);
int lua_camera_snprintf(lua_State * const L, char *result, size_t maxLen, bool spacePrefix);

// MARK: - Internal -
// Used by other lua Object types that extend lua Camera

void _lua_camera_push_fields(lua_State * const L, Camera *c);
void _lua_camera_update_fields(lua_State * const L, Camera *c);
bool _lua_camera_metatable_index(lua_State * const L, Camera *c, const char *key);
bool _lua_camera_metatable_newindex(lua_State * const L, Camera *c, const char *key);
