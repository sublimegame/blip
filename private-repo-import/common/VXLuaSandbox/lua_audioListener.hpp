//
//  lua_audioListener.hpp
//  Cubzh
//
//  Created by Gaetan de Villele on 17/08/2022.
//

#pragma once

// C++
#include <cstddef>

#include "audioListener.hpp"

typedef struct lua_State lua_State;

// MARK: - AudioListener global table -

void lua_g_audiolistener_createAndPush(lua_State * const L);
void lua_g_audioListener_push(lua_State * const L);
AudioListener *lua_g_audioListener_getAudioListenerPtr(lua_State * const L, const int idx);
int lua_g_audiolistener_snprintf(lua_State * const L, char *result, size_t maxLen, bool spacePrefix);
