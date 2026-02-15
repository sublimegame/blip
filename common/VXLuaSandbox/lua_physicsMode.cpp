// -------------------------------------------------------------
//  Cubzh
//  lua_physicsMode.cpp
//  Created by Arthur Cormerais on January 12, 2023.
// -------------------------------------------------------------

#include "lua_physicsMode.hpp"

#include "config.h"
#include "lua_utils.hpp"
#include "sbs.hpp"
#include "vxlog.h"

#define LUA_PHYSICSMODE_FIELD_DISABLED      "Disabled"
#define LUA_PHYSICSMODE_FIELD_TRIGGER       "Trigger"
#define LUA_PHYSICSMODE_FIELD_TRIGGERPB     "TriggerPerBlock"
#define LUA_PHYSICSMODE_FIELD_STATIC        "Static"
#define LUA_PHYSICSMODE_FIELD_STATICPB      "StaticPerBlock"
#define LUA_PHYSICSMODE_FIELD_DYNAMIC       "Dynamic"

// MARK: - Private functions prototypes -

bool _physicsMode_createAndPushTable(lua_State *L, RigidbodyMode mode);
int _physicsMode_g_metatable_newindex(lua_State *L);
int _physicsMode_instance_metatable_index(lua_State *L);
int _physicsMode_instance_metatable_newindex(lua_State *L);

// MARK: - Exposed functions -

bool lua_g_physicsMode_createAndPush(lua_State *L) {
    vx_assert(L != nullptr);
    LUAUTILS_STACK_SIZE(L)
    
    lua_newuserdata(L, 1); // global PhysicsMode table
    lua_newtable(L); // metatable
    {
        lua_pushstring(L, "__index");
        lua_newtable(L);
        {
            lua_pushstring(L, LUA_PHYSICSMODE_FIELD_DISABLED);
            _physicsMode_createAndPushTable(L, RigidbodyMode_Disabled);
            lua_rawset(L, -3);
            
            lua_pushstring(L, LUA_PHYSICSMODE_FIELD_TRIGGER);
            _physicsMode_createAndPushTable(L, RigidbodyMode_Trigger);
            lua_rawset(L, -3);

            lua_pushstring(L, LUA_PHYSICSMODE_FIELD_TRIGGERPB);
            _physicsMode_createAndPushTable(L, RigidbodyMode_TriggerPerBlock);
            lua_rawset(L, -3);

            lua_pushstring(L, LUA_PHYSICSMODE_FIELD_STATIC);
            _physicsMode_createAndPushTable(L, RigidbodyMode_Static);
            lua_rawset(L, -3);

            lua_pushstring(L, LUA_PHYSICSMODE_FIELD_STATICPB);
            _physicsMode_createAndPushTable(L, RigidbodyMode_StaticPerBlock);
            lua_rawset(L, -3);

            lua_pushstring(L, LUA_PHYSICSMODE_FIELD_DYNAMIC);
            _physicsMode_createAndPushTable(L, RigidbodyMode_Dynamic);
            lua_rawset(L, -3);
        }
        lua_rawset(L, -3);
        
        lua_pushstring(L, "__newindex");
        lua_pushcfunction(L, _physicsMode_g_metatable_newindex, "");
        lua_rawset(L, -3);
        
        lua_pushstring(L, "__metatable");
        lua_pushboolean(L, false);
        lua_rawset(L, -3);

        lua_pushstring(L, "__type");
        lua_pushstring(L, "PhysicsModeInterface");
        lua_rawset(L, -3);

        lua_pushstring(L, "__typen");
        lua_pushinteger(L, ITEM_TYPE_G_PHYSICSMODE);
        lua_rawset(L, -3);
    }
    lua_setmetatable(L, -2);

    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
    return true;
}

bool lua_physicsMode_pushTable(lua_State *L, uint8_t mode) {
    if (mode > RigidbodyMode_Max) {
        return false;
    }
    return lua_physicsMode_pushTable(L, static_cast<RigidbodyMode>(mode));
}
    
bool lua_physicsMode_pushTable(lua_State *L, RigidbodyMode mode) {
    vx_assert(L != nullptr);
    LUAUTILS_STACK_SIZE(L)

    lua_getglobal(L, LUA_G_PHYSICSMODE);

    switch(mode) {
        case RigidbodyMode_Disabled:
            lua_getfield(L, -1, LUA_PHYSICSMODE_FIELD_DISABLED);
            break;
        case RigidbodyMode_Trigger:
            lua_getfield(L, -1, LUA_PHYSICSMODE_FIELD_TRIGGER);
            break;
        case RigidbodyMode_TriggerPerBlock:
            lua_getfield(L, -1, LUA_PHYSICSMODE_FIELD_TRIGGERPB);
            break;
        case RigidbodyMode_Static:
            lua_getfield(L, -1, LUA_PHYSICSMODE_FIELD_STATIC);
            break;
        case RigidbodyMode_StaticPerBlock:
            lua_getfield(L, -1, LUA_PHYSICSMODE_FIELD_STATICPB);
            break;
        case RigidbodyMode_Dynamic:
            lua_getfield(L, -1, LUA_PHYSICSMODE_FIELD_DYNAMIC);
            break;
    }

    lua_remove(L, -2); // remove global PhysicsMode
    
    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
    return true;
}

RigidbodyMode lua_physicsMode_get(lua_State *L, int idx) {
    vx_assert(L != nullptr);
    LUAUTILS_STACK_SIZE(L)

    if (LUA_GET_METAFIELD_AND_RETURN_TYPE(L, idx, LUA_PHYSICSMODE_VALUE) == LUA_TNIL) {
        LUAUTILS_INTERNAL_ERROR(L)
    }

    RigidbodyMode mode = static_cast<RigidbodyMode>(lua_tointeger(L, -1));
    lua_pop(L, 1);

    LUAUTILS_STACK_SIZE_ASSERT(L, 0)
    return mode;
}

int lua_physicsMode_snprintf(lua_State *L, char* result, size_t maxLen, bool spacePrefix) {
    vx_assert(L != nullptr);
    LUAUTILS_STACK_SIZE(L)

    // printing the global table
    if (LUA_GET_METAFIELD_AND_RETURN_TYPE(L, -1, LUA_PHYSICSMODE_VALUE) == LUA_TNIL) {
        LUAUTILS_STACK_SIZE_ASSERT(L, 0)
        return snprintf(result, maxLen, "%s[PhysicsMode]", spacePrefix ? " " : "");
    }
    // else, printing one of its field

    RigidbodyMode mode = static_cast<RigidbodyMode>(lua_tointeger(L, -1));

    lua_pop(L, 1);
    LUAUTILS_STACK_SIZE_ASSERT(L, 0)

    switch (mode) {
        case RigidbodyMode_Disabled:
            return snprintf(result, maxLen, "%s[PhysicsMode: Disabled]", spacePrefix ? " " : "");
        case RigidbodyMode_Trigger:
            return snprintf(result, maxLen, "%s[PhysicsMode: Trigger]", spacePrefix ? " " : "");
        case RigidbodyMode_TriggerPerBlock:
            return snprintf(result, maxLen, "%s[PhysicsMode: TriggerPerBlock]", spacePrefix ? " " : "");
        case RigidbodyMode_Static:
            return snprintf(result, maxLen, "%s[PhysicsMode: Static]", spacePrefix ? " " : "");
        case RigidbodyMode_StaticPerBlock:
            return snprintf(result, maxLen, "%s[PhysicsMode: StaticPerBlock]", spacePrefix ? " " : "");
        case RigidbodyMode_Dynamic:
            return snprintf(result, maxLen, "%s[PhysicsMode: Dynamic]", spacePrefix ? " " : "");
        default:
            return snprintf(result, maxLen, "%s[PhysicsMode: unknown]", spacePrefix ? " " : "");
    }
}

// MARK: - private functions -

bool _physicsMode_createAndPushTable(lua_State *L, RigidbodyMode mode) {
    vx_assert(L != nullptr);
    LUAUTILS_STACK_SIZE(L)
    
    lua_newuserdata(L, 1); // PhysicsMode table
    lua_newtable(L); // metatable
    {
        lua_pushstring(L, "__index");
        lua_pushcfunction(L, _physicsMode_instance_metatable_index, "");
        lua_rawset(L, -3);

        lua_pushstring(L, "__newindex");
        lua_pushcfunction(L, _physicsMode_instance_metatable_newindex, "");
        lua_rawset(L, -3);

        lua_pushstring(L, "__metatable");
        lua_pushboolean(L, false);
        lua_rawset(L, -3);

        lua_pushstring(L, LUA_PHYSICSMODE_VALUE);
        lua_pushinteger(L, static_cast<int>(mode));
        lua_rawset(L, -3);

        lua_pushstring(L, "__type");
        lua_pushstring(L, "PhysicsMode");
        lua_rawset(L, -3);

        lua_pushstring(L, "__typen");
        lua_pushinteger(L, ITEM_TYPE_PHYSICSMODE);
        lua_rawset(L, -3);
    }
    lua_setmetatable(L, -2);
    
    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
    return true;
}

int _physicsMode_g_metatable_newindex(lua_State *L) {
    LUAUTILS_ERROR(L, "PhysicsMode is read only");
    return 0;
}

int _physicsMode_instance_metatable_index(lua_State *L) {
    LUAUTILS_STACK_SIZE(L)

    lua_pushnil(L);

    LUAUTILS_STACK_SIZE_ASSERT(L, 1);
    return 1;
}

int _physicsMode_instance_metatable_newindex(lua_State *L) {
    LUAUTILS_ERROR(L, "PhysicsMode is read only");
    return 0;
}
