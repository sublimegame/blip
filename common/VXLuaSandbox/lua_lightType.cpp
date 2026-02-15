// -------------------------------------------------------------
//  Cubzh
//  lua_lightType.cpp
//  Created by Arthur Cormerais on May 11, 2022.
// -------------------------------------------------------------

#include "lua_lightType.hpp"

#include "config.h"
#include "lua_utils.hpp"
#include "sbs.hpp"
#include "vxlog.h"

#define LUA_LIGHTTYPE_FIELD_POINT "Point"
#define LUA_LIGHTTYPE_FIELD_SPOT "Spot"
#define LUA_LIGHTTYPE_FIELD_DIRECTIONAL "Directional"

// MARK: - Private functions prototypes -

bool _lua_lightType_createAndpushTable(lua_State *L, LightType type);
int _lightType_g_metatable_newindex(lua_State *L);
int _lightType_instance_metatable_index(lua_State *L);
int _lightType_instance_metatable_newindex(lua_State *L);

// MARK: - Exposed functions -

bool lua_g_lightType_createAndPush(lua_State *L) {
    vx_assert(L != nullptr);
    LUAUTILS_STACK_SIZE(L)
    
    lua_newtable(L); // global LightType table
    lua_newtable(L); // metatable
    {
        lua_pushstring(L, "__index");
        lua_newtable(L);
        {
            lua_pushstring(L, LUA_LIGHTTYPE_FIELD_POINT);
            _lua_lightType_createAndpushTable(L, LightType_Point);
            lua_rawset(L, -3);
            
            lua_pushstring(L, LUA_LIGHTTYPE_FIELD_SPOT);
            _lua_lightType_createAndpushTable(L, LightType_Spot);
            lua_rawset(L, -3);
            
            lua_pushstring(L, LUA_LIGHTTYPE_FIELD_DIRECTIONAL);
            _lua_lightType_createAndpushTable(L, LightType_Directional);
            lua_rawset(L, -3);
        }
        lua_rawset(L, -3);
        
        lua_pushstring(L, "__newindex");
        lua_pushcfunction(L, _lightType_g_metatable_newindex, "");
        lua_rawset(L, -3);
        
        lua_pushstring(L, "__metatable");
        lua_pushboolean(L, false);
        lua_rawset(L, -3);
        
        lua_pushstring(L, P3S_LUA_OBJ_METAFIELD_TYPE);
        lua_pushinteger(L, ITEM_TYPE_G_LIGHTTYPE);
        lua_rawset(L, -3);
    }
    lua_setmetatable(L, -2);

    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
    return true;
}
    
bool lua_lightType_pushTable(lua_State *L, LightType type) {
    vx_assert(L != nullptr);
    LUAUTILS_STACK_SIZE(L)

    lua_getglobal(L, P3S_LUA_G_LIGHTTYPE);

    switch(type) {
        case LightType_Point:
            lua_getfield(L, -1, LUA_LIGHTTYPE_FIELD_POINT);
            break;
        case LightType_Spot:
            lua_getfield(L, -1, LUA_LIGHTTYPE_FIELD_SPOT);
            break;
        case LightType_Directional:
            lua_getfield(L, -1, LUA_LIGHTTYPE_FIELD_DIRECTIONAL);
            break;
    }

    lua_remove(L, -2); // remove global LightType
    
    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
    return true;
}

LightType lua_lightType_get(lua_State *L, int idx) {
    vx_assert(L != nullptr);
    LUAUTILS_STACK_SIZE(L)

    if (LUA_GET_METAFIELD_AND_RETURN_TYPE(L, idx, LUA_LIGHTTYPE_VALUE) == LUA_TNIL) {
        LUAUTILS_INTERNAL_ERROR(L)
    }

    LightType type = static_cast<LightType>(lua_tointeger(L, -1));
    lua_pop(L, 1);

    LUAUTILS_STACK_SIZE_ASSERT(L, 0)
    return type;
}

int lua_lightType_snprintf(lua_State *L, char* result, size_t maxLen, bool spacePrefix) {
    vx_assert(L != nullptr);
    LUAUTILS_STACK_SIZE(L)

    // printing the global table
    if (LUA_GET_METAFIELD_AND_RETURN_TYPE(L, -1, LUA_LIGHTTYPE_VALUE) == LUA_TNIL) {
        LUAUTILS_STACK_SIZE_ASSERT(L, 0)
        return snprintf(result, maxLen, "%s[LightType]", spacePrefix ? " " : "");
    }
    // else, printing one of its field

    LightType type = static_cast<LightType>(lua_tointeger(L, -1));

    lua_pop(L, 1);
    LUAUTILS_STACK_SIZE_ASSERT(L, 0)

    switch (type) {
        case LightType_Point:
            return snprintf(result, maxLen, "%s[LightType: Point]", spacePrefix ? " " : "");
        case LightType_Spot:
            return snprintf(result, maxLen, "%s[LightType: Spot]", spacePrefix ? " " : "");
        case LightType_Directional:
            return snprintf(result, maxLen, "%s[LightType: Directional]", spacePrefix ? " " : "");
        default:
            return snprintf(result, maxLen, "%s[LightType: unknown]", spacePrefix ? " " : "");
    }
}

// MARK: - private functions -

bool _lua_lightType_createAndpushTable(lua_State *L, LightType type) {
    vx_assert(L != nullptr);
    LUAUTILS_STACK_SIZE(L)
    
    lua_newtable(L); // LightType table
    lua_newtable(L); // metatable
    {
        lua_pushstring(L, "__index");
        lua_pushcfunction(L, _lightType_instance_metatable_index, "");
        lua_rawset(L, -3);

        lua_pushstring(L, "__newindex");
        lua_pushcfunction(L, _lightType_instance_metatable_newindex, "");
        lua_rawset(L, -3);

        lua_pushstring(L, "__metatable");
        lua_pushboolean(L, false);
        lua_rawset(L, -3);

        lua_pushstring(L, LUA_LIGHTTYPE_VALUE);
        lua_pushinteger(L, static_cast<int>(type));
        lua_rawset(L, -3);

        lua_pushstring(L, P3S_LUA_OBJ_METAFIELD_TYPE);
        lua_pushinteger(L, ITEM_TYPE_LIGHTTYPE);
        lua_rawset(L, -3);
    }
    lua_setmetatable(L, -2);
    
    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
    return true;
}

int _lightType_g_metatable_newindex(lua_State *L) {
    LUAUTILS_ERROR(L, "LightType is read only");
    return 0;
}

int _lightType_instance_metatable_index(lua_State *L) {
    LUAUTILS_STACK_SIZE(L)

    lua_pushnil(L);

    LUAUTILS_STACK_SIZE_ASSERT(L, 1);
    return 1;
}

int _lightType_instance_metatable_newindex(lua_State *L) {
    LUAUTILS_ERROR(L, "LightType is read only");
    return 0;
}
