//
//  lua_private.cpp
//  Cubzh
//
//  Created by Adrien Duermael on 28/07/2021.
//

#include "lua_private.hpp"

// Lua
#include "lua.hpp"
#include "lua_logs.hpp"
#include "lua_utils.hpp"
#include "lua_constants.h"
#include "lua_color.hpp"
#include "lua_shape.hpp"

#include "VXGame.hpp"
#include "VXNode.hpp"
#include "VXButton.hpp"
#include "VXLabel.hpp"
#include "VXMenuConfig.hpp"

#include "config.h"
#include "color_palette.h"

#include "vxlog.h"

#define LUA_PRIVATE_FIELD_SERVER_TAG "ServerTag" // read-only string

static int _private_index(lua_State *L);
static int _private_newindex(lua_State *L);

void _set_private_metatable(lua_State *L) {
    
    lua_newtable(L); // metatable
    
    lua_pushstring(L, "__index");
    lua_pushcfunction(L, _private_index, "");
    lua_rawset(L, -3);
    
    lua_pushstring(L, "__newindex");
    lua_pushcfunction(L, _private_newindex, "");
    lua_rawset(L, -3);
    
    lua_pushstring(L, "__metatable");
    lua_pushboolean(L, false);
    lua_rawset(L, -3);
    
    lua_pushstring(L, P3S_LUA_OBJ_METAFIELD_TYPE); // used to log tables
    lua_pushinteger(L, ITEM_TYPE_PRIVATE);
    lua_rawset(L, -3);
    
    lua_setmetatable(L, -2);
}

void lua_private_create_and_push(lua_State *L) {
    LUAUTILS_STACK_SIZE(L)
    lua_newtable(L);
    _set_private_metatable(L);
    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
}

static int _private_index(lua_State *L) {
    vx_assert(L != nullptr);
    LUAUTILS_ASSERT_ARGS_COUNT(L, 2)
    LUAUTILS_ASSERT_ARG_TYPE(L, 1, ITEM_TYPE_PRIVATE)
    LUAUTILS_STACK_SIZE(L)
    
    // get 2nd argument: key string
    if (lua_utils_isStringStrict(L, 2) == false) {
        // Dev does not support non-string keys
        // just return nil
        lua_pushnil(L);
        LUAUTILS_STACK_SIZE_ASSERT(L, 1)
        return 1;
    }
    
    const char *key = lua_tostring(L, 2);
    LUAUTILS_STACK_SIZE_ASSERT(L, 0)
    
    if (strcmp(key, LUA_PRIVATE_FIELD_SERVER_TAG) == 0) {
        lua_pushstring(L, vx::Config::shared().gameServerTag().c_str());
    } else {
        lua_pushnil(L); // if key is unknown, return nil
    }
    
    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
    return 1;
}

///
static int _private_newindex(lua_State *L) {
    RETURN_VALUE_IF_NULL(L, 0)
    LUAUTILS_STACK_SIZE(L)
    LUAUTILS_ERROR(L, "Private table is read-only"); // returns
    LUAUTILS_STACK_SIZE_ASSERT(L, 0)
    return 0;
}
