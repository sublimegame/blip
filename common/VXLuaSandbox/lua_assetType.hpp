//
//  lua_assetType.hpp
//  Cubzh
//
//  Created by Corentin Cailleaud on 04/11/2022.
//

#include <stdint.h>

#include "lua.hpp"
#include "asset.h"

#define LUA_ASSETTYPE_VALUE "asset_type"

bool lua_g_assetType_createAndPush(lua_State *L);
bool lua_assetType_pushTable(lua_State *L, AssetType type);
AssetType lua_assetType_get(lua_State *L, int idx);
int lua_assetType_snprintf(lua_State *L, char *result, size_t maxLen, bool spacePrefix);
