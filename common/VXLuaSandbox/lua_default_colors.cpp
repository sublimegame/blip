//
//  lua_default_colors.cpp
//  Cubzh
//
//  Created by Mina Pecheux on 8/16/22.
//

#include "lua_color.hpp"

#include "lua.hpp"
#include "lua_utils.hpp"

#include "sbs.hpp"

#include "xptools.h"

#include "game.h"

/// __newindex for DefaultColors (read-only)
static int _default_colors_newindex(lua_State *L);

/// __len for DefaultColors
static int _default_colors_len(lua_State *L);

void lua_default_colors_create_and_push(lua_State *L) {
    LUAUTILS_STACK_SIZE(L)

    lua_newtable(L); // DefaultColors instance
    {
        lua_newtable(L); // DefaultColors instance metatable

        CGame *g = nullptr;
        if (lua_utils_getGamePtr(L, &g) == false) {
            LUAUTILS_INTERNAL_ERROR(L) // returns
        }
        
        ColorPalette *p = color_palette_get_default_2021(game_get_color_atlas(g));
        lua_pushstring(L, "__index");
        {
            lua_newtable(L);
            RGBAColor c;
            for (SHAPE_COLOR_INDEX_INT_T i = 0; i < p->count; ++i) {
                c = color_palette_get_color(p, i);
                lua_color_create_and_push_table(L, c.r, c.g, c.b, c.a, false, true);
                lua_rawseti(L, -2, i + 1);
            }
        }
        lua_rawset(L, -3);
            
        lua_pushstring(L, "__newindex");
        lua_pushcfunction(L, _default_colors_newindex, "");
        lua_rawset(L, -3);

        lua_pushstring(L, "__len");
        lua_pushcfunction(L, _default_colors_len, "");
        lua_rawset(L, -3);

        lua_pushstring(L, "__metatable");
        lua_pushboolean(L, false);
        lua_rawset(L, -3);

        lua_pushstring(L, P3S_LUA_OBJ_METAFIELD_TYPE); // used to log tables
        lua_pushinteger(L, ITEM_TYPE_DEFAULT_COLORS);
        lua_settable(L, -3);
    }
    
    lua_setmetatable(L, -2);

    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
}

static int _default_colors_newindex(lua_State *L) {
    RETURN_VALUE_IF_NULL(L, 0)
    LUAUTILS_STACK_SIZE(L)
    LUAUTILS_ERROR(L, "read-only DefaultColors cannot be modified"); // returns
    LUAUTILS_STACK_SIZE_ASSERT(L, 0)
    return 0;
}

static int _default_colors_len(lua_State *L) {
    vx_assert(L != nullptr);
    LUAUTILS_ASSERT_ARGS_COUNT(L, 2)
    LUAUTILS_ASSERT_ARG_TYPE(L, 1, ITEM_TYPE_DEFAULT_COLORS)
    LUAUTILS_STACK_SIZE(L)
    
    CGame *g = nullptr;
    if (lua_utils_getGamePtr(L, &g) == false) {
        LUAUTILS_INTERNAL_ERROR(L) // returns
    }
    
    ColorPalette *p = color_palette_get_default_2021(game_get_color_atlas(g));
    lua_pushinteger(L, p->count);
    
    LUAUTILS_STACK_SIZE_ASSERT(L, 1);
    return 1;
}
