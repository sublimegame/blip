//
//  lua_assets.hpp
//  Cubzh
//
//  Created by Corentin Cailleaud on 25/10/2022.
//

#pragma once

#include <stddef.h>
#include <string>

#include "asset.h"
#include "doubly_linked_list.h"
#include "color_palette.h"

typedef struct lua_State lua_State;

/// Creates and pushes a new "Assets" table onto the Lua stack
bool lua_g_assets_create_and_push(lua_State * const L);

// Called after C userdata has been freed.
// Can be used to clean associated Lua metadata.
// Can be called with NULL lua state (after state has been closed)
namespace vx { class UserDataStore; }
void lua_assets_request_gc_finalize(vx::UserDataStore *store, lua_State * const L);

/// Lua print function for global Assets
int lua_g_assets_snprintf(lua_State * const L, char *result, const size_t maxLen, const bool spacePrefix);
