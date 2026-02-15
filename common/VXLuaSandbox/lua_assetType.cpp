//
//  lua_assetType.cpp
//  Cubzh
//
//  Created by Corentin Cailleaud on 04/11/2022.
//

#include "lua_assetType.hpp"

#include "config.h"
#include "lua_utils.hpp"
#include "sbs.hpp"
#include "vxlog.h"

#define LUA_ASSETTYPE_FIELD_UNKNOWN     "Unknown"
#define LUA_ASSETTYPE_FIELD_SHAPE       "Shape"
#define LUA_ASSETTYPE_FIELD_PALETTE     "Palette"
#define LUA_ASSETTYPE_FIELD_OBJECT      "Object"
#define LUA_ASSETTYPE_FIELD_CAMERA      "Camera"
#define LUA_ASSETTYPE_FIELD_LIGHT       "Light"
#define LUA_ASSETTYPE_FIELD_MESH        "Mesh"

#define LUA_ASSETTYPE_FIELD_ANY         "Any"
#define LUA_ASSETTYPE_FIELD_ANYOBJECT   "AnyObject"

// MARK: - Private functions prototypes -

bool _lua_assetType_createAndpushTable(lua_State *L, AssetType type);
int _assetType_g_metatable_newindex(lua_State *L);
int _assetType_instance_metatable_index(lua_State *L);
int _assetType_instance_metatable_newindex(lua_State *L);

// MARK: - Exposed functions -

bool lua_g_assetType_createAndPush(lua_State *L) {
    vx_assert(L != nullptr);
    LUAUTILS_STACK_SIZE(L)
    
    lua_newtable(L); // global AssetType table
    lua_newtable(L); // metatable
    {
        lua_pushstring(L, "__index");
        lua_newtable(L);
        {
            lua_pushstring(L, LUA_ASSETTYPE_FIELD_UNKNOWN);
            _lua_assetType_createAndpushTable(L, AssetType_Unknown);
            lua_rawset(L, -3);
            
            lua_pushstring(L, LUA_ASSETTYPE_FIELD_SHAPE);
            _lua_assetType_createAndpushTable(L, AssetType_Shape);
            lua_rawset(L, -3);

            lua_pushstring(L, LUA_ASSETTYPE_FIELD_PALETTE);
            _lua_assetType_createAndpushTable(L, AssetType_Palette);
            lua_rawset(L, -3);

            lua_pushstring(L, LUA_ASSETTYPE_FIELD_OBJECT);
            _lua_assetType_createAndpushTable(L, AssetType_Object);
            lua_rawset(L, -3);

            lua_pushstring(L, LUA_ASSETTYPE_FIELD_CAMERA);
            _lua_assetType_createAndpushTable(L, AssetType_Camera);
            lua_rawset(L, -3);

            lua_pushstring(L, LUA_ASSETTYPE_FIELD_LIGHT);
            _lua_assetType_createAndpushTable(L, AssetType_Light);
            lua_rawset(L, -3);

            lua_pushstring(L, LUA_ASSETTYPE_FIELD_MESH);
            _lua_assetType_createAndpushTable(L, AssetType_Mesh);
            lua_rawset(L, -3);

            lua_pushstring(L, LUA_ASSETTYPE_FIELD_ANY);
            _lua_assetType_createAndpushTable(L, AssetType_Any);
            lua_rawset(L, -3);

            lua_pushstring(L, LUA_ASSETTYPE_FIELD_ANYOBJECT);
            _lua_assetType_createAndpushTable(L, AssetType_AnyObject);
            lua_rawset(L, -3);
        }
        lua_rawset(L, -3);
        
        lua_pushstring(L, "__newindex");
        lua_pushcfunction(L, _assetType_g_metatable_newindex, "");
        lua_rawset(L, -3);
        
        lua_pushstring(L, "__metatable");
        lua_pushboolean(L, false);
        lua_rawset(L, -3);
        
        lua_pushstring(L, P3S_LUA_OBJ_METAFIELD_TYPE);
        lua_pushinteger(L, ITEM_TYPE_G_ASSETTYPE);
        lua_rawset(L, -3);
    }
    lua_setmetatable(L, -2);

    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
    return true;
}
    
bool lua_assetType_pushTable(lua_State *L, AssetType type) {
    vx_assert(L != nullptr);
    LUAUTILS_STACK_SIZE(L)

    lua_getglobal(L, LUA_G_ASSETTYPE);

    switch(type) {
        case AssetType_Shape:
            lua_getfield(L, -1, LUA_ASSETTYPE_FIELD_SHAPE);
            break;
        case AssetType_Palette:
            lua_getfield(L, -1, LUA_ASSETTYPE_FIELD_PALETTE);
            break;
        case AssetType_Object:
            lua_getfield(L, -1, LUA_ASSETTYPE_FIELD_OBJECT);
            break;
        case AssetType_Camera:
            lua_getfield(L, -1, LUA_ASSETTYPE_FIELD_CAMERA);
            break;
        case AssetType_Light:
            lua_getfield(L, -1, LUA_ASSETTYPE_FIELD_LIGHT);
            break;
        case AssetType_Mesh:
            lua_getfield(L, -1, LUA_ASSETTYPE_FIELD_MESH);
            break;
        case AssetType_Any:
            lua_getfield(L, -1, LUA_ASSETTYPE_FIELD_ANY);
            break;
        case AssetType_AnyObject:
            lua_getfield(L, -1, LUA_ASSETTYPE_FIELD_ANYOBJECT);
            break;
        default:
            lua_getfield(L, -1, LUA_ASSETTYPE_FIELD_UNKNOWN);
            break;
    }

    lua_remove(L, -2); // remove global AssetType
    
    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
    return true;
}

AssetType lua_assetType_get(lua_State *L, int idx) {
    vx_assert(L != nullptr);
    LUAUTILS_STACK_SIZE(L)

    if (LUA_GET_METAFIELD_AND_RETURN_TYPE(L, idx, LUA_ASSETTYPE_VALUE) == LUA_TNIL) {
        LUAUTILS_INTERNAL_ERROR(L)
    }

    AssetType type = static_cast<AssetType>(lua_tointeger(L, -1));
    lua_pop(L, 1);

    LUAUTILS_STACK_SIZE_ASSERT(L, 0)
    return type;
}

int lua_assetType_snprintf(lua_State *L, char* result, size_t maxLen, bool spacePrefix) {
    vx_assert(L != nullptr);
    LUAUTILS_STACK_SIZE(L)

    // printing the global table
    if (LUA_GET_METAFIELD_AND_RETURN_TYPE(L, -1, LUA_ASSETTYPE_VALUE) == LUA_TNIL) {
        LUAUTILS_STACK_SIZE_ASSERT(L, 0)
        return snprintf(result, maxLen, "%s[AssetType]", spacePrefix ? " " : "");
    }
    // else, printing one of its field

    AssetType type = static_cast<AssetType>(lua_tointeger(L, -1));

    lua_pop(L, 1);
    LUAUTILS_STACK_SIZE_ASSERT(L, 0)

    switch (type) {
        case AssetType_Shape:
            return snprintf(result, maxLen, "%s[AssetType: Shape]", spacePrefix ? " " : "");
        case AssetType_Palette:
            return snprintf(result, maxLen, "%s[AssetType: Palette]", spacePrefix ? " " : "");
        case AssetType_Object:
            return snprintf(result, maxLen, "%s[AssetType: Object]", spacePrefix ? " " : "");
        case AssetType_Camera:
            return snprintf(result, maxLen, "%s[AssetType: Camera]", spacePrefix ? " " : "");
        case AssetType_Light:
            return snprintf(result, maxLen, "%s[AssetType: Light]", spacePrefix ? " " : "");
        case AssetType_Mesh:
            return snprintf(result, maxLen, "%s[AssetType: Mesh]", spacePrefix ? " " : "");
        case AssetType_Any:
            return snprintf(result, maxLen, "%s[AssetType: Any]", spacePrefix ? " " : "");
        case AssetType_AnyObject:
            return snprintf(result, maxLen, "%s[AssetType: AnyObject]", spacePrefix ? " " : "");
        default:
            return snprintf(result, maxLen, "%s[AssetType: unknown]", spacePrefix ? " " : "");
    }
}

// MARK: - private functions -

bool _lua_assetType_createAndpushTable(lua_State *L, AssetType type) {
    vx_assert(L != nullptr);
    LUAUTILS_STACK_SIZE(L)
    
    lua_newtable(L); // AssetType table
    lua_newtable(L); // metatable
    {
        lua_pushstring(L, "__index");
        lua_pushcfunction(L, _assetType_instance_metatable_index, "");
        lua_rawset(L, -3);

        lua_pushstring(L, "__newindex");
        lua_pushcfunction(L, _assetType_instance_metatable_newindex, "");
        lua_rawset(L, -3);

        lua_pushstring(L, "__metatable");
        lua_pushboolean(L, false);
        lua_rawset(L, -3);

        lua_pushstring(L, LUA_ASSETTYPE_VALUE);
        lua_pushinteger(L, static_cast<int>(type));
        lua_rawset(L, -3);

        lua_pushstring(L, P3S_LUA_OBJ_METAFIELD_TYPE);
        lua_pushinteger(L, ITEM_TYPE_ASSETTYPE);
        lua_rawset(L, -3);
    }
    lua_setmetatable(L, -2);
    
    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
    return true;
}

int _assetType_g_metatable_newindex(lua_State *L) {
    LUAUTILS_ERROR(L, "AssetType is read only");
    return 0;
}

int _assetType_instance_metatable_index(lua_State *L) {
    LUAUTILS_STACK_SIZE(L)

    lua_pushnil(L);

    LUAUTILS_STACK_SIZE_ASSERT(L, 1);
    return 1;
}

int _assetType_instance_metatable_newindex(lua_State *L) {
    LUAUTILS_ERROR(L, "AssetType is read only");
    return 0;
}
