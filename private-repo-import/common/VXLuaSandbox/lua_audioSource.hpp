//
//  lua_audioSource.hpp
//  Cubzh
//
//  Created by Gaetan de Villele on 17/08/2022.
//

#pragma once

// C++
#include <cstddef>

//
#include "audioSource.hpp"

typedef struct lua_State lua_State;

// MARK: - AudioSource global table -

void lua_g_audioSource_createAndPush(lua_State * const L);
int lua_g_audioSource_snprintf(lua_State * const L, char *result, size_t maxLen, bool spacePrefix);

// MARK: - AudioSource instances tables -

bool lua_audioSource_pushNewInstance(lua_State * const L, AudioSource * const as);
AudioSource *lua_audioSource_getAudioSourcePtr(lua_State * const L, const int idx);
int lua_audioSource_snprintf(lua_State * const L, char *result, size_t maxLen, bool spacePrefix);

typedef struct _CGame CGame;
namespace vx { class UserDataStore; }

void lua_audioSource_release_transform(void *_ud);
