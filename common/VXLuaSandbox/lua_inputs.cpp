//
//  lua_inputs.cpp
//  Cubzh
//
//  Created by Xavier Legland on 7/21/20.
//

#include "lua_inputs.hpp"

// lua
#include "lua_utils.hpp"
#include "lua_client.hpp"
#include "lua_pointer.hpp"

// xptools
#include "xptools.h"

// sandbox
#include "sbs.hpp"

#include "config.h"

// Game Coordinator
#include "GameCoordinator.hpp"

#include "VXNotifications.hpp"

// --------------------------------------------------
//
// MARK: - Static functions' prototypes -
//
// --------------------------------------------------

/// __index for global Input
static int _g_inputs_metatable_index(lua_State *L);

/// __newindex for global Input
static int _g_inputs_metatable_newindex(lua_State *L);

// --------------------------------------------------
//
// MARK: - Exposed functions -
//
// --------------------------------------------------

bool lua_g_inputs_pushNewTable(lua_State *L) {
    vx_assert(L != nullptr);
    LUAUTILS_STACK_SIZE(L)
    
    lua_newtable(L);
    lua_newtable(L); // metatable
    {
        lua_pushstring(L, "__index");
        lua_pushcfunction(L, _g_inputs_metatable_index, "");
        lua_settable(L, -3);

        lua_pushstring(L, "__newindex");
        lua_pushcfunction(L, _g_inputs_metatable_newindex, "");
        lua_settable(L, -3);

        lua_pushstring(L, "__metatable");
        lua_pushboolean(L, false);
        lua_settable(L, -3);
        
        // add "Pointer" table to Input's metatable (it's a private field)
        lua_pushstring(L, LUA_INPUTS_FIELD_POINTER);
        lua_g_pointer_pushNewTable(L);
        lua_settable(L, -3);

        lua_pushstring(L, P3S_LUA_OBJ_METAFIELD_TYPE); // used to log tables
        lua_pushinteger(L, ITEM_TYPE_G_INPUT);
        lua_settable(L, -3);
    }
    lua_setmetatable(L, -2);
    
    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
    return true;
}

bool lua_g_inputs_push_table(lua_State *L) {
    vx_assert(L != nullptr);
    LUAUTILS_STACK_SIZE(L)
    
    if (lua_client_pushField(L, LUA_CLIENT_FIELD_INPUTS) == false) {
        vxlog_error("failed to push Client.Inputs onto the stack");
        LUAUTILS_STACK_SIZE_ASSERT(L, 0)
        return false;
    }

    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
    return true;
}

void lua_g_inputs_pushField(lua_State * const L, const int idx, const char *key) {
    LUAUTILS_STACK_SIZE(L)
    LUA_GET_METAFIELD_AND_RETURN_TYPE_PUSHING_NIL_IF_NOT_FOUND(L, idx, key);
    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
}

// --------------------------------------------------
//
// MARK: - static functions
//
// --------------------------------------------------

/// arguments
/// - lua table : global Input table
/// - lua string : the key that is being accessed
static int _g_inputs_metatable_index(lua_State *L) {
    vx_assert(L != nullptr);
    LUAUTILS_ASSERT_ARGS_COUNT(L, 2)
    LUAUTILS_ASSERT_ARG_TYPE(L, 1, ITEM_TYPE_G_INPUT)
    LUAUTILS_STACK_SIZE(L)

    const char *key = lua_tostring(L, 2);

    if (strcmp(key, LUA_INPUTS_FIELD_POINTER) == 0) {
        LUA_GET_METAFIELD_AND_RETURN_TYPE_PUSHING_NIL_IF_NOT_FOUND(L, 1, LUA_INPUTS_FIELD_POINTER);
    } else {
        lua_pushnil(L);
    }

    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
    return 1; // the __index function always has 1 return value
}

/// arguments
/// - table (table)  : Input global table
/// - key   (string) : the key that is being set
/// - value (any)    : the value being stored
static int _g_inputs_metatable_newindex(lua_State *L) {
    vx_assert(L != nullptr);
    LUAUTILS_ASSERT_ARGS_COUNT(L, 3)
    LUAUTILS_ASSERT_ARG_TYPE(L, 1, ITEM_TYPE_G_INPUT)
    LUAUTILS_STACK_SIZE(L)

    const char *key = lua_tostring(L, 2);

    if (strcmp(key, LUA_INPUTS_FIELD_POINTER) == 0) {

        if (lua_isboolean(L, 3)) {
            CGame *g;
            if (lua_utils_getGamePtr(L, &g) == false) {
                LUAUTILS_ERROR(L, "Could not get game"); // returns
            }
            game_ui_state_set_isPointerVisible(g, lua_toboolean(L, 3));
        } else {
            LUAUTILS_ERROR(L, "incorrect argument type"); // returns
        }

        LUAUTILS_STACK_SIZE_ASSERT(L, 0)

    } else {
        LUAUTILS_ERROR_F(L, "Input.%s is not settable", key); // returns
    }

    LUAUTILS_STACK_SIZE_ASSERT(L, 0)
    return 0;
}
