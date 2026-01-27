//
//  lua_ray.hpp
//  Cubzh
//
//  Created by Adrien Duermael on 10/08/2021.
//

#pragma once

#include <stddef.h>

#include "float3.h"

typedef struct lua_State lua_State;

// Creates and pushes global Ray table
void lua_g_ray_create_and_push(lua_State *L);

// Creates and pushes Ray table
void lua_ray_create_and_push(lua_State *L, int originIdx, int directionIdx);
void lua_ray_create_and_push(lua_State *L, float3 origin, float3 unit);

/// Pushes ray cast function
void lua_ray_push_cast_function(lua_State *L);

/// Prints global Ray
int lua_g_ray_snprintf(lua_State *L,
                       char* result,
                       size_t maxLen,
                       bool spacePrefix);

/// Prints Ray instance
int lua_ray_snprintf(lua_State *L,
                     char* result,
                     size_t maxLen,
                     bool spacePrefix);
