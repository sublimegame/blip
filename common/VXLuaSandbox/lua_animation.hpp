//
//  lua_animation.hpp
//  Cubzh
//
//  Created by Corentin Cailleaud on 01/08/2023.
//

#pragma once

// C++
#include <cstddef>

#include "animation.h"

typedef struct lua_State lua_State;

// MARK: - Animation global table -

void lua_g_animation_create_and_push(lua_State * const L);
int lua_g_animation_snprintf(lua_State * const L, char *result, size_t maxLen, bool spacePrefix);

Animation *lua_animation_getAnimationPtr(lua_State * const L, const int idx);
int lua_animation_snprintf(lua_State * const L, char *result, size_t maxLen, bool spacePrefix);
