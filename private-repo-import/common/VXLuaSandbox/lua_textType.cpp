// -------------------------------------------------------------
//  Cubzh
//  lua_textType.cpp
//  Created by Arthur Cormerais on October 13, 2022.
// -------------------------------------------------------------

#include "lua_textType.hpp"

#include "config.h"
#include "lua_utils.hpp"
#include "sbs.hpp"
#include "vxlog.h"

#define LUA_TEXTTYPE_FIELD_WORLD        "World"
#define LUA_TEXTTYPE_FIELD_SCREEN       "Screen"

// MARK: - Private functions prototypes -

bool _lua_textType_createAndpushTable(lua_State *L, TextType type);
int _textType_g_metatable_newindex(lua_State *L);
int _textType_instance_metatable_index(lua_State *L);
int _textType_instance_metatable_newindex(lua_State *L);

// MARK: - Exposed functions -

bool lua_g_textType_createAndPush(lua_State *L) {
    vx_assert(L != nullptr);
    LUAUTILS_STACK_SIZE(L)
    
    lua_newtable(L); // global TextType table
    lua_newtable(L); // metatable
    {
        lua_pushstring(L, "__index");
        lua_newtable(L);
        {
            lua_pushstring(L, LUA_TEXTTYPE_FIELD_WORLD);
            _lua_textType_createAndpushTable(L, TextType_World);
            lua_rawset(L, -3);
            
            lua_pushstring(L, LUA_TEXTTYPE_FIELD_SCREEN);
            _lua_textType_createAndpushTable(L, TextType_Screen);
            lua_rawset(L, -3);
        }
        lua_rawset(L, -3);
        
        lua_pushstring(L, "__newindex");
        lua_pushcfunction(L, _textType_g_metatable_newindex, "");
        lua_rawset(L, -3);
        
        lua_pushstring(L, "__metatable");
        lua_pushboolean(L, false);
        lua_rawset(L, -3);
        
        lua_pushstring(L, P3S_LUA_OBJ_METAFIELD_TYPE);
        lua_pushinteger(L, ITEM_TYPE_G_TEXTTYPE);
        lua_rawset(L, -3);
    }
    lua_setmetatable(L, -2);

    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
    return true;
}
    
bool lua_textType_pushTable(lua_State *L, TextType type) {
    vx_assert(L != nullptr);
    LUAUTILS_STACK_SIZE(L)

    lua_getglobal(L, LUA_G_TEXTTYPE);

    switch(type) {
        case TextType_World:
            lua_getfield(L, -1, LUA_TEXTTYPE_FIELD_WORLD);
            break;
        case TextType_Screen:
            lua_getfield(L, -1, LUA_TEXTTYPE_FIELD_SCREEN);
            break;
    }

    lua_remove(L, -2); // remove global TextType
    
    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
    return true;
}

TextType lua_textType_get(lua_State *L, int idx) {
    vx_assert(L != nullptr);
    LUAUTILS_STACK_SIZE(L)

    if (LUA_GET_METAFIELD_AND_RETURN_TYPE(L, idx, LUA_TEXTTYPE_VALUE) == LUA_TNIL) {
        LUAUTILS_INTERNAL_ERROR(L)
    }

    TextType type = static_cast<TextType>(lua_tointeger(L, -1));
    lua_pop(L, 1);

    LUAUTILS_STACK_SIZE_ASSERT(L, 0)
    return type;
}

int lua_textType_snprintf(lua_State *L, char* result, size_t maxLen, bool spacePrefix) {
    vx_assert(L != nullptr);
    LUAUTILS_STACK_SIZE(L)

    // printing the global table
    if (LUA_GET_METAFIELD_AND_RETURN_TYPE(L, -1, LUA_TEXTTYPE_VALUE) == LUA_TNIL) {
        LUAUTILS_STACK_SIZE_ASSERT(L, 0)
        return snprintf(result, maxLen, "%s[TextType]", spacePrefix ? " " : "");
    }
    // else, printing one of its field

    TextType type = static_cast<TextType>(lua_tointeger(L, -1));

    lua_pop(L, 1);
    LUAUTILS_STACK_SIZE_ASSERT(L, 0)

    switch (type) {
        case TextType_World:
            return snprintf(result, maxLen, "%s[TextType: World]", spacePrefix ? " " : "");
        case TextType_Screen:
            return snprintf(result, maxLen, "%s[TextType: UI]", spacePrefix ? " " : "");
        default:
            return snprintf(result, maxLen, "%s[TextType: unknown]", spacePrefix ? " " : "");
    }
}

// MARK: - private functions -

bool _lua_textType_createAndpushTable(lua_State *L, TextType type) {
    vx_assert(L != nullptr);
    LUAUTILS_STACK_SIZE(L)
    
    lua_newtable(L); // TextType table
    lua_newtable(L); // metatable
    {
        lua_pushstring(L, "__index");
        lua_pushcfunction(L, _textType_instance_metatable_index, "");
        lua_rawset(L, -3);

        lua_pushstring(L, "__newindex");
        lua_pushcfunction(L, _textType_instance_metatable_newindex, "");
        lua_rawset(L, -3);

        lua_pushstring(L, "__metatable");
        lua_pushboolean(L, false);
        lua_rawset(L, -3);

        lua_pushstring(L, LUA_TEXTTYPE_VALUE);
        lua_pushinteger(L, static_cast<int>(type));
        lua_rawset(L, -3);

        lua_pushstring(L, P3S_LUA_OBJ_METAFIELD_TYPE);
        lua_pushinteger(L, ITEM_TYPE_TEXTTYPE);
        lua_rawset(L, -3);
    }
    lua_setmetatable(L, -2);
    
    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
    return true;
}

int _textType_g_metatable_newindex(lua_State *L) {
    LUAUTILS_ERROR(L, "TextType is read only");
    return 0;
}

int _textType_instance_metatable_index(lua_State *L) {
    LUAUTILS_STACK_SIZE(L)

    lua_pushnil(L);

    LUAUTILS_STACK_SIZE_ASSERT(L, 1);
    return 1;
}

int _textType_instance_metatable_newindex(lua_State *L) {
    LUAUTILS_ERROR(L, "TextType is read only");
    return 0;
}
