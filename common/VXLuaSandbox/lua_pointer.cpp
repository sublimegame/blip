//
//  lua_pointer.cpp
//  Cubzh
//
//  Created by Xavier Legland on 7/28/20.
//

#include "lua_pointer.hpp"

// Lua
#include "lua.hpp"

#include "lua_utils.hpp"
#include "lua_client.hpp"

#include "lua_inputs.hpp"
#include "lua_logs.hpp"
#include "lua_local_event.hpp"

// xptools
#include "xptools.h"

// sandbox
#include "sbs.hpp"

// Game Coordinator
#include "GameCoordinator.hpp"

#include "VXNotifications.hpp"
#include "VXPrefs.hpp"

#define LUA_POINTER_FIELD_DOWN "Down"
#define LUA_POINTER_FIELD_UP "Up"
#define LUA_POINTER_FIELD_SHOW "Show"
#define LUA_POINTER_FIELD_HIDE "Hide"
#define LUA_POINTER_FIELD_IS_HIDDEN "IsHidden"
#define LUA_POINTER_FIELD_CLICK "Click"
#define LUA_POINTER_FIELD_SENSITIVITY "Sensitivity" // number (read-only)
#define LUA_POINTER_FIELD_ZOOM_SENSITIVITY "ZoomSensitivity" // number (read-only)

// --------------------------------------------------
//
// MARK: - Static functions' prototypes -
//
// --------------------------------------------------

/// __index for global Pointer
static int _g_pointer_metatable_index(lua_State *L);

/// __newindex for global Pointer
static int _g_pointer_metatable_newindex(lua_State *L);

static int _pointer_show(lua_State * const L);
static int _pointer_hide(lua_State * const L);

// --------------------------------------------------
//
// MARK: - Exposed functions -
//
// --------------------------------------------------

bool lua_g_pointer_pushNewTable(lua_State *L) {
    vx_assert(L != nullptr);
    LUAUTILS_STACK_SIZE(L)
    lua_newtable(L);
    lua_newtable(L); // metatable
    {
        lua_pushstring(L, "__index");
        lua_pushcfunction(L, _g_pointer_metatable_index, "");
        lua_rawset(L, -3);

        lua_pushstring(L, "__newindex");
        lua_pushcfunction(L, _g_pointer_metatable_newindex, "");
        lua_rawset(L, -3);

        lua_pushstring(L, "__metatable");
        lua_pushboolean(L, false);
        lua_rawset(L, -3);

        lua_pushstring(L, P3S_LUA_OBJ_METAFIELD_TYPE); // used to log tables
        lua_pushinteger(L, ITEM_TYPE_G_POINTER);
        lua_rawset(L, -3);
    }
    lua_setmetatable(L, -2);
    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
    return true;
}

///
bool lua_g_pointer_pushFunc(lua_State *L, const char *function) {
    vx_assert(L != nullptr);
    LUAUTILS_STACK_SIZE(L)
    
    // push "Client.Inputs" table onto the stack
    if (lua_g_inputs_push_table(L) == false) {
        LUAUTILS_STACK_SIZE_ASSERT(L, 0)
        return false;
    }
    
    // get "Client.Inputs.Pointer" table onto the stack
    if (lua_getfield(L, -1, LUA_INPUTS_FIELD_POINTER) != LUA_TTABLE) {
        lua_pop(L, 2);
        LUAUTILS_STACK_SIZE_ASSERT(L, 0)
        return false;
    }
    
    // remove "Client.Input" from the stack and keep only "Client.Input.Pointer"
    lua_remove(L, -2);
    
    // push key
    lua_pushstring(L, function);
    const int type = lua_rawget(L, -2); // pops the key and push the value
    if (type != LUA_TFUNCTION) {
        lua_pop(L, 2); // pop value and "Pointer" table
        LUAUTILS_STACK_SIZE_ASSERT(L, 0)
        return false;
    }
    
    lua_remove(L, -2); // removes "Pointer" table
    
    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
    return true;
}

// --------------------------------------------------
//
// MARK: - static functions
//
// --------------------------------------------------

/// arguments
/// - lua table : global Pointer table
/// - lua string : the key that is being accessed
static int _g_pointer_metatable_index(lua_State *L) {
    vx_assert(L != nullptr);
    LUAUTILS_STACK_SIZE(L)

    // validate arguments count
    if (!lua_utils_assertArgsCount(L, 2)) {
        LUAUTILS_ERROR(L, "incorrect argument count"); // returns
    }

    // check 1st argument: global Pointer table
    if (lua_utils_getObjectType(L, 1) != ITEM_TYPE_G_POINTER) {
        LUAUTILS_ERROR(L, "incorrect argument type");
    }

    // get 2nd argument: key string
    if (lua_utils_isStringStrict(L, 2) == false) {
        LUAUTILS_ERROR(L, "incorrect argument 2"); // returns
    }
    
    const char *key = lua_tostring(L, 2);
    LUAUTILS_STACK_SIZE_ASSERT(L, 0)
    
    if (strcmp(key, LUA_POINTER_FIELD_SHOW) == 0) {
        lua_pushcfunction(L, _pointer_show, "");
        
    } else if (strcmp(key, LUA_POINTER_FIELD_HIDE) == 0) {
        lua_pushcfunction(L, _pointer_hide, "");
        
    } else if (strcmp(key, LUA_POINTER_FIELD_IS_HIDDEN) == 0) {

        CGame *g = nullptr;
        if (lua_utils_getGamePtr(L, &g) == false) {
            LUAUTILS_INTERNAL_ERROR(L)
        }

        lua_pushboolean(L, game_ui_state_get_isPointerVisible(g) == false);

    } else if (strcmp(key, LUA_POINTER_FIELD_SENSITIVITY) == 0) {
        float sensitivity = vx::Prefs::shared().sensitivity();
        lua_pushnumber(L, static_cast<lua_Number>(sensitivity));

    } else if (strcmp(key, LUA_POINTER_FIELD_ZOOM_SENSITIVITY) == 0) {
        float zoomSensitivity = vx::Prefs::shared().zoomSensitivity();
        lua_pushnumber(L, static_cast<lua_Number>(zoomSensitivity));

    } else {
        if (strcmp(key, LUA_POINTER_FIELD_UP) == 0 ||
            strcmp(key, LUA_POINTER_FIELD_CLICK) == 0 ||
            strcmp(key, LUA_POINTER_FIELD_DOWN) == 0 ||
            strcmp(key, LUA_POINTER_FIELD_DRAG_BEGIN) == 0 ||
            strcmp(key, LUA_POINTER_FIELD_DRAG) == 0 ||
            strcmp(key, LUA_POINTER_FIELD_DRAG_END) == 0 ||
            strcmp(key, LUA_POINTER_FIELD_DRAG2_BEGIN) == 0 ||
            strcmp(key, LUA_POINTER_FIELD_DRAG2) == 0 ||
            strcmp(key, LUA_POINTER_FIELD_DRAG2_END) == 0 ||
            strcmp(key, LUA_POINTER_FIELD_ZOOM) == 0 ||
            strcmp(key, LUA_POINTER_FIELD_CANCEL) == 0 ||
            strcmp(key, LUA_POINTER_FIELD_LONGPRESS) == 0) {
            LUA_GET_METAFIELD_AND_RETURN_TYPE_PUSHING_NIL_IF_NOT_FOUND(L, 1, key);
        } else {
             lua_pushnil(L);
        }
    }
    
    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
    return 1; // the __index function always has 1 return value
}

/// arguments
/// - table (table)  : Pointer global table
/// - key   (string) : the key that is being set
/// - value (any)    : the value being stored
static int _g_pointer_metatable_newindex(lua_State *L) {
    vx_assert(L != nullptr);
    LUAUTILS_STACK_SIZE(L)

    // validate arguments count
    if (!lua_utils_assertArgsCount(L, 3)) {
        LUAUTILS_ERROR(L, "incorrect argument count"); // returns
    }

    // check 1st argument: global Pointer table
    if (lua_utils_getObjectType(L, 1) != ITEM_TYPE_G_POINTER) {
        LUAUTILS_ERROR(L, "incorrect argument type");
    }

    // get 2nd argument: key string
    if (lua_utils_isStringStrict(L, 2) == false) {
        LUAUTILS_ERROR(L, "incorrect argument 2"); // returns
    }

    const char *key = lua_tostring(L, 2);

    if (strcmp(key, LUA_POINTER_FIELD_UP) == 0 ||
        strcmp(key, LUA_POINTER_FIELD_CLICK) == 0 ||
        strcmp(key, LUA_POINTER_FIELD_DOWN) == 0 ||
        strcmp(key, LUA_POINTER_FIELD_DRAG_BEGIN) == 0 ||
        strcmp(key, LUA_POINTER_FIELD_DRAG) == 0 ||
        strcmp(key, LUA_POINTER_FIELD_DRAG_END) == 0 ||
        strcmp(key, LUA_POINTER_FIELD_DRAG2_BEGIN) == 0 ||
        strcmp(key, LUA_POINTER_FIELD_DRAG2) == 0 ||
        strcmp(key, LUA_POINTER_FIELD_DRAG2_END) == 0 ||
        strcmp(key, LUA_POINTER_FIELD_ZOOM) == 0 ||
        strcmp(key, LUA_POINTER_FIELD_CANCEL) == 0 ||
        strcmp(key, LUA_POINTER_FIELD_LONGPRESS) == 0) {

        // make sure the new value is a function or nil
        if (lua_isfunction(L, 3) == false && lua_isnil(L, 3) == false) {
            LUAUTILS_ERROR_F(L, "Pointer.%s can only have a function or nil value", key);
        }
        
        // store fields in metatable not in Pointer table directly,
        // to avoid bypassing __newindex
        lua_getmetatable(L, 1);
 
        // now storing callback itself at intended key, within metatable
        lua_pushvalue(L, 2); // key
        lua_pushvalue(L, 3); // value
        lua_rawset(L, -3);
        
        lua_pop(L, 1); // pop metatable

    } else {
        LUAUTILS_ERROR_F(L, "Client.Input.Pointer.%s is not settable", key); // returns
    }

    LUAUTILS_STACK_SIZE_ASSERT(L, 0)
    return 0;
}

static int _pointer_show(lua_State * const L) {
    vx_assert(L != nullptr);
    LUAUTILS_STACK_SIZE(L)
    
    const int argCount = lua_gettop(L);
    
    if (argCount < 1 || lua_utils_getObjectType(L, 1) != ITEM_TYPE_G_POINTER) {
        LUAUTILS_ERROR(L, "Pointer:Show - function should be called with `:`");
    }
    
    if (argCount > 1) {
        LUAUTILS_ERROR(L, "Pointer:Show - wrong number of arguments");
    }

    CGame *g = nullptr;
    if (lua_utils_getGamePtr(L, &g) == false) {
        LUAUTILS_INTERNAL_ERROR(L)
    }
    
    game_ui_state_set_isPointerVisible(g, true);
    
    LUAUTILS_STACK_SIZE_ASSERT(L, 0)
    return 0;
}

static int _pointer_hide(lua_State * const L) {
    vx_assert(L != nullptr);
    LUAUTILS_STACK_SIZE(L)

    const int argCount = lua_gettop(L);
    
    if (argCount < 1 || lua_utils_getObjectType(L, 1) != ITEM_TYPE_G_POINTER) {
        LUAUTILS_ERROR(L, "Pointer:Hide - function should be called with `:`");
    }
    
    if (argCount > 1) {
        LUAUTILS_ERROR(L, "Pointer:Hide - wrong number of arguments");
    }

    CGame *g = nullptr;
    if (lua_utils_getGamePtr(L, &g) == false) {
        LUAUTILS_INTERNAL_ERROR(L)
    }
    
    game_ui_state_set_isPointerVisible(g, false);

    LUAUTILS_STACK_SIZE_ASSERT(L, 0)
    return 0;
}
