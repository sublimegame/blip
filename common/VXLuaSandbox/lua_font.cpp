// -------------------------------------------------------------
//  Cubzh
//  lua_font.cpp
//  Created by Arthur Cormerais on April 24, 2024.
// -------------------------------------------------------------

#include "lua_font.hpp"

#include "config.h"
#include "lua_utils.hpp"
#include "sbs.hpp"
#include "vxlog.h"

#define LUA_FONT_FIELD_PIXEL        "Pixel"
#define LUA_FONT_FIELD_NOTO         "Noto"
#define LUA_FONT_FIELD_NOTOMONO     "NotoMono"

// MARK: - Private functions prototypes -

bool _font_createAndpushTable(lua_State *L, FontName font);
int _font_g_metatable_newindex(lua_State *L);
int _font_instance_metatable_index(lua_State *L);
int _font_instance_metatable_newindex(lua_State *L);

// MARK: - Exposed functions -

bool lua_g_font_createAndPush(lua_State *L) {
    vx_assert(L != nullptr);
    LUAUTILS_STACK_SIZE(L)
    
    lua_newtable(L); // global Font table
    lua_newtable(L); // metatable
    {
        lua_pushstring(L, "__index");
        lua_newtable(L);
        {
            lua_pushstring(L, LUA_FONT_FIELD_PIXEL);
            _font_createAndpushTable(L, FontName_Pixel);
            lua_rawset(L, -3);
            
            lua_pushstring(L, LUA_FONT_FIELD_NOTO);
            _font_createAndpushTable(L, FontName_Noto);
            lua_rawset(L, -3);

            lua_pushstring(L, LUA_FONT_FIELD_NOTOMONO);
            _font_createAndpushTable(L, FontName_NotoMono);
            lua_rawset(L, -3);
        }
        lua_rawset(L, -3);
        
        lua_pushstring(L, "__newindex");
        lua_pushcfunction(L, _font_g_metatable_newindex, "");
        lua_rawset(L, -3);
        
        lua_pushstring(L, "__metatable");
        lua_pushboolean(L, false);
        lua_rawset(L, -3);
        
        lua_pushstring(L, P3S_LUA_OBJ_METAFIELD_TYPE);
        lua_pushinteger(L, ITEM_TYPE_G_FONT);
        lua_rawset(L, -3);
    }
    lua_setmetatable(L, -2);

    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
    return true;
}
    
bool lua_font_pushTable(lua_State *L, FontName name) {
    vx_assert(L != nullptr);
    LUAUTILS_STACK_SIZE(L)

    lua_getglobal(L, LUA_G_FONT);

    switch(name) {
        case FontName_Pixel:
            lua_getfield(L, -1, LUA_FONT_FIELD_PIXEL);
            break;
        case FontName_Noto:
            lua_getfield(L, -1, LUA_FONT_FIELD_NOTO);
            break;
        case FontName_NotoMono:
            lua_getfield(L, -1, LUA_FONT_FIELD_NOTOMONO);
            break;
    }

    lua_remove(L, -2); // remove global TextType
    
    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
    return true;
}

FontName lua_font_get(lua_State *L, int idx) {
    vx_assert(L != nullptr);
    LUAUTILS_STACK_SIZE(L)

    if (LUA_GET_METAFIELD_AND_RETURN_TYPE(L, idx, LUA_FONT_VALUE) == LUA_TNIL) {
        LUAUTILS_INTERNAL_ERROR(L)
    }

    FontName name = static_cast<FontName>(lua_tointeger(L, -1));
    lua_pop(L, 1);

    LUAUTILS_STACK_SIZE_ASSERT(L, 0)
    return name;
}

int lua_font_snprintf(lua_State *L, char* result, size_t maxLen, bool spacePrefix) {
    vx_assert(L != nullptr);
    LUAUTILS_STACK_SIZE(L)

    // printing the global table
    if (LUA_GET_METAFIELD_AND_RETURN_TYPE(L, -1, LUA_FONT_VALUE) == LUA_TNIL) {
        LUAUTILS_STACK_SIZE_ASSERT(L, 0)
        return snprintf(result, maxLen, "%s[Font]", spacePrefix ? " " : "");
    }
    // else, printing one of its field

    FontName name = static_cast<FontName>(lua_tointeger(L, -1));

    lua_pop(L, 1);
    LUAUTILS_STACK_SIZE_ASSERT(L, 0)

    switch (name) {
        case FontName_Pixel:
            return snprintf(result, maxLen, "%s[Font: Pixel]", spacePrefix ? " " : "");
        case FontName_Noto:
            return snprintf(result, maxLen, "%s[Font: Noto]", spacePrefix ? " " : "");
        case FontName_NotoMono:
            return snprintf(result, maxLen, "%s[Font: NotoMono]", spacePrefix ? " " : "");
        case FontName_Emoji:
            return 0;
    }

    return 0;
}

// MARK: - private functions -

bool _font_createAndpushTable(lua_State *L, FontName name) {
    vx_assert(L != nullptr);
    LUAUTILS_STACK_SIZE(L)
    
    lua_newtable(L); // Font table
    lua_newtable(L); // metatable
    {
        lua_pushstring(L, "__index");
        lua_pushcfunction(L, _font_instance_metatable_index, "");
        lua_rawset(L, -3);

        lua_pushstring(L, "__newindex");
        lua_pushcfunction(L, _font_instance_metatable_newindex, "");
        lua_rawset(L, -3);

        lua_pushstring(L, "__metatable");
        lua_pushboolean(L, false);
        lua_rawset(L, -3);

        lua_pushstring(L, LUA_FONT_VALUE);
        lua_pushinteger(L, static_cast<int>(name));
        lua_rawset(L, -3);

        lua_pushstring(L, P3S_LUA_OBJ_METAFIELD_TYPE);
        lua_pushinteger(L, ITEM_TYPE_FONT);
        lua_rawset(L, -3);
    }
    lua_setmetatable(L, -2);
    
    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
    return true;
}

int _font_g_metatable_newindex(lua_State *L) {
    LUAUTILS_ERROR(L, "Font is read only");
    return 0;
}

int _font_instance_metatable_index(lua_State *L) {
    LUAUTILS_STACK_SIZE(L)

    lua_pushnil(L);

    LUAUTILS_STACK_SIZE_ASSERT(L, 1);
    return 1;
}

int _font_instance_metatable_newindex(lua_State *L) {
    LUAUTILS_ERROR(L, "Font is read only");
    return 0;
}
