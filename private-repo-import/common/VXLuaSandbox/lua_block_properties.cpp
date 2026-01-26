//
//  lua_block_properties.cpp
//  Cubzh
//
//  Created by Adrien Duermael on 7/31/20.
//

#include "lua_block_properties.hpp"

// C++
#include <cstring>

// Lua
#include "lua.hpp"
#include "lua_utils.hpp"
#include "lua_color.hpp"

#include "xptools.h"

#include "sbs.hpp"
#include "lua_color.hpp"

#include "colors.h"
#include "color_palette.h"
#include "game.h"
#include "game_config.h"

#include "VXGame.hpp"

#define LUA_BLOCK_PROPERTIES_FIELD_ID "ID" // deprecated
#define LUA_BLOCK_PROPERTIES_FIELD_INDEX "PaletteIndex"
#define LUA_BLOCK_PROPERTIES_FIELD_COLOR "Color"
#define LUA_BLOCK_PROPERTIES_FIELD_LIGHT "Light"
#define LUA_BLOCK_PROPERTIES_FIELD_REMOVE "Remove"
#define LUA_BLOCK_PROPERTIES_FIELD_COUNT "BlocksCount"
#define LUA_BLOCK_PROPERTIES_FIELD_ISUSED "IsUsed"

static int _block_properties_index(lua_State * const L);
static void _block_properties_gc(void *_ud);

static int _block_properties_newindex(lua_State * const L);
static ColorPalette *_lua_block_properties_getPalettePointer(lua_State *L, int idx);
static SHAPE_COLOR_INDEX_INT_T _lua_block_properties_get_or_refresh_palette_index(lua_State *L, int idx, ColorPalette **out, SHAPE_COLOR_INDEX_INT_T *outEntryIdx);
static int _lua_block_properties_remove(lua_State *L);
static int _lua_block_properties_is_used(lua_State *L);

typedef struct _block_properties_color_handler_userdata {
    SHAPE_COLOR_INDEX_INT_T index;
    Weakptr *palette;
} _block_properties_color_handler_userdata;

static void _block_properties_set_color_handler(const uint8_t *r,
                                                const uint8_t *g,
                                                const uint8_t *b,
                                                const uint8_t *a,
                                                const bool *light,
                                                lua_State * const L,
                                                void *userdata);

static void _block_properties_get_color_handler(uint8_t *r,
                                                uint8_t *g,
                                                uint8_t *b,
                                                uint8_t *a,
                                                bool *light,
                                                lua_State * const L,
                                                const void *userdata);

static void _block_properties_handler_free_userdata(void *userdata);

typedef struct blockProperties_userdata {
    // Userdata instance requirements
    vx::UserDataStore *store;
    blockProperties_userdata *next;
    blockProperties_userdata *previous;

    Weakptr * paletteWeakPtr;
} blockProperties_userdata;

void lua_block_properties_create_and_push_table(lua_State * const L,
                                                ColorPalette *palette,
                                                SHAPE_COLOR_INDEX_INT_T luaIdx) {
    vx_assert(L != nullptr);
    vx_assert(palette != nullptr);
    LUAUTILS_STACK_SIZE(L)

    if (luaIdx < 1 || luaIdx > color_palette_get_ordered_count(palette)) {
        // out of range
        lua_pushnil(L);
        LUAUTILS_STACK_SIZE_ASSERT(L, 1)
        return;
    }

    vx::Game *g = nullptr;
    if (lua_utils_getGameCppWrapperPtr(L, &g) == false) {
        LUAUTILS_INTERNAL_ERROR(L);
    }

    blockProperties_userdata *ud = static_cast<blockProperties_userdata *>(lua_newuserdatadtor(L, sizeof(blockProperties_userdata), _block_properties_gc));
    ud->paletteWeakPtr = color_palette_get_and_retain_weakptr(palette);

    // connect to userdata store
    ud->store = g->userdataStoreForBlockProperties;
    ud->previous = nullptr;
    blockProperties_userdata* next = static_cast<blockProperties_userdata*>(ud->store->add(ud));
    ud->next = next;
    if (next != nullptr) {
        next->previous = ud;
    }

    lua_newtable(L); // metatable
    {
        lua_pushstring(L, "__index");
        lua_pushcfunction(L, _block_properties_index, "");
        lua_rawset(L, -3);

        lua_pushstring(L, "__metatable");
        lua_pushboolean(L, false);
        lua_rawset(L, -3);

        lua_pushstring(L, "__type");
        lua_pushstring(L, "BlockProperties");
        lua_rawset(L, -3);

        lua_pushstring(L, "__typen");
        lua_pushinteger(L, ITEM_TYPE_BLOCKPROPERTIES);
        lua_rawset(L, -3);

        lua_pushstring(L, LUA_BLOCK_PROPERTIES_FIELD_INDEX); // lua index (+1)
        lua_pushinteger(L, luaIdx);
        lua_rawset(L, -3);

        lua_pushstring(L, "__newindex");
        lua_pushcfunction(L, _block_properties_newindex, "");
        lua_rawset(L, -3);
        
        int cIndex = luaIdx - 1;
        
        _block_properties_color_handler_userdata *userdata = static_cast<_block_properties_color_handler_userdata*>(malloc(sizeof(_block_properties_color_handler_userdata)));
        userdata->index = cIndex;
        userdata->palette = color_palette_get_and_retain_weakptr(palette);
        
        lua_pushstring(L, LUA_BLOCK_PROPERTIES_FIELD_COLOR);
        uint8_t zero = 0;
        lua_color_create_and_push_table(L, zero, zero, zero, zero, false, false);
        lua_color_setHandlers(L, -1,
                              _block_properties_set_color_handler,
                              _block_properties_get_color_handler,
                              _block_properties_handler_free_userdata,
                              userdata);
        lua_rawset(L, -3);
    }
    lua_setmetatable(L, -2);
    
    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
    return;
}

/// arguments
/// - table (table)  : BlockProperties table
/// - key   (string) : the key that is being accessed
static int _block_properties_index(lua_State * const L) {
    vx_assert(L != nullptr);
    LUAUTILS_STACK_SIZE(L)
    
    const char *key = lua_tostring(L, 2);
    
    if (strcmp(key, LUA_BLOCK_PROPERTIES_FIELD_ID) == 0 || strcmp(key, LUA_BLOCK_PROPERTIES_FIELD_INDEX) == 0) {
        SHAPE_COLOR_INDEX_INT_T paletteIndex = _lua_block_properties_get_or_refresh_palette_index(L, 1, nullptr, nullptr);
        if (paletteIndex != SHAPE_COLOR_INDEX_AIR_BLOCK) {
            lua_pushinteger(L, paletteIndex);
        } else {
            lua_pushnil(L);
        }
    } else if (strcmp(key, LUA_BLOCK_PROPERTIES_FIELD_COLOR) == 0) {
        LUA_GET_METAFIELD_PUSHING_NIL_IF_NOT_FOUND(L, 1, LUA_BLOCK_PROPERTIES_FIELD_COLOR);
    } else if (strcmp(key, LUA_BLOCK_PROPERTIES_FIELD_LIGHT) == 0) {
        if (LUA_GET_METAFIELD_AND_RETURN_TYPE(L, 1, LUA_BLOCK_PROPERTIES_FIELD_COLOR) == LUA_TNIL) {
            lua_pushnil(L);
        } else {
            // extract light from color
            bool light;
            lua_color_getRGBAL(L, -1, nullptr, nullptr, nullptr, nullptr, &light);
            lua_pop(L, 1); // pop color
            lua_pushboolean(L, light);
        }
    } else if (strcmp(key, LUA_BLOCK_PROPERTIES_FIELD_REMOVE) == 0) {
        lua_pushcfunction(L, _lua_block_properties_remove, "");
    } else if (strcmp(key, LUA_BLOCK_PROPERTIES_FIELD_COUNT) == 0) {
        ColorPalette *palette = nullptr;
        SHAPE_COLOR_INDEX_INT_T entryIdx;
        _lua_block_properties_get_or_refresh_palette_index(L, 1, &palette, &entryIdx);
        if (palette != nullptr) {
            const uint32_t count = color_palette_get_color_use_count(palette, entryIdx);

            lua_pushinteger(L, count);
        } else {
            lua_pushinteger(L, 0);
        }
    } else if (strcmp(key, LUA_BLOCK_PROPERTIES_FIELD_ISUSED) == 0) {
        lua_pushcfunction(L, _lua_block_properties_is_used, "");
    } else {
        LUAUTILS_ERROR_F(L, "BlockProperties.%s is undefined", key); // returns
    }
    
    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
    return 1;
}

static void _block_properties_gc(void *_ud) {
    blockProperties_userdata *ud = static_cast<blockProperties_userdata*>(_ud);
    if (ud->paletteWeakPtr != nullptr) {
        weakptr_release(ud->paletteWeakPtr);
        ud->paletteWeakPtr = nullptr;
    }

    // disconnect from userdata store
    {
        if (ud->previous != nullptr) {
            ud->previous->next = ud->next;
        }
        if (ud->next != nullptr) {
            ud->next->previous = ud->previous;
        }
        ud->store->removeWithoutKeepingID(ud, ud->next);
    }
}

static int _block_properties_newindex(lua_State * const L) {
    vx_assert(L != nullptr);
    LUAUTILS_ASSERT_ARGS_COUNT(L, 3)
    LUAUTILS_ASSERT_ARG_TYPE(L, 1, ITEM_TYPE_BLOCKPROPERTIES)
    LUAUTILS_STACK_SIZE(L)
    
    const char *key = lua_tostring(L, 2);
    
    if (strcmp(key, LUA_BLOCK_PROPERTIES_FIELD_COLOR) == 0) {
        
        if (lua_utils_getObjectType(L, 3) != ITEM_TYPE_COLOR) {
            LUAUTILS_ERROR_F(L, "BlockProperties.%s: incorrect argument type", key); // returns
        }
        
        uint8_t r, g, b, a;
        lua_color_getRGBAL(L, 3, &r, &g, &b, &a, nullptr);
        
        if (LUA_GET_METAFIELD_AND_RETURN_TYPE(L, 1, LUA_BLOCK_PROPERTIES_FIELD_COLOR) == LUA_TNIL) {
            LUAUTILS_INTERNAL_ERROR(L); // returns
        }
        
        lua_color_setRGBAL(L, -1, &r, &g, &b, &a, nullptr);
        lua_pop(L, 1); // pop Color
        
        LUAUTILS_STACK_SIZE_ASSERT(L, 0)
        
    } else if (strcmp(key, LUA_BLOCK_PROPERTIES_FIELD_LIGHT) == 0) {
        
        if (lua_isboolean(L, 3) == false) {
            LUAUTILS_ERROR_F(L, "BlockProperties.%s: incorrect argument type", key); // returns
        }
        
        bool light = lua_toboolean(L, 3);
        
        if (LUA_GET_METAFIELD_AND_RETURN_TYPE(L, 1, LUA_BLOCK_PROPERTIES_FIELD_COLOR) == LUA_TNIL) {
            LUAUTILS_INTERNAL_ERROR(L); // returns
        }
        
        lua_color_setRGBAL(L, -1, nullptr, nullptr, nullptr, nullptr, &light);
        lua_pop(L, 1); // pop Color
        
        LUAUTILS_STACK_SIZE_ASSERT(L, 0)
        
    } else {
        LUAUTILS_ERROR_F(L, "BlockProperties.%s is not settable", key); // returns
    }

    LUAUTILS_STACK_SIZE_ASSERT(L, 0)
    return 0;
}

int lua_g_block_properties_snprintf(lua_State * const L,
                         char* result,
                         size_t maxLen,
                         bool spacePrefix) {
     return snprintf(result, maxLen, "%s[BlockProperties class]", spacePrefix ? " " : "");
}

/// Prints BlockProperties instance
int lua_block_properties_snprintf(lua_State * const L,
                       char* result,
                       const size_t maxLen,
                       const bool spacePrefix) {
    vx_assert(L != nullptr);
    LUAUTILS_STACK_SIZE(L)
    
    int index = 0;
    
    if (LUA_GET_METAFIELD_AND_RETURN_TYPE(L, -1, LUA_BLOCK_PROPERTIES_FIELD_INDEX) != LUA_TNIL) {
        index = static_cast<int>(lua_tointeger(L, -1));
        lua_pop(L, 1);
    }
    
    LUAUTILS_STACK_SIZE_ASSERT(L, 0)
    
    return snprintf(result, maxLen, "%s[BlockProperties ID: %d]",
                    spacePrefix ? " " : "",
                    index);
}

static void _block_properties_handler_free_userdata(void *userdata) {
    if (userdata != nullptr) {
        _block_properties_color_handler_userdata *_userdata = static_cast<_block_properties_color_handler_userdata*>(userdata);
        weakptr_release(_userdata->palette);
    }
    free(userdata);
}

static void _block_properties_set_color_handler(const uint8_t *r,
                                       const uint8_t *g,
                                       const uint8_t *b,
                                       const uint8_t *a,
                                       const bool *light,
                                       lua_State * const L,
                                       void *userdata) {
    
    _block_properties_color_handler_userdata *_userdata = static_cast<_block_properties_color_handler_userdata*>(userdata);
    vx_assert(_userdata->palette != nullptr);
    
    ColorPalette *p = static_cast<ColorPalette *>(weakptr_get(_userdata->palette));
    vx_assert(p != nullptr);

    const SHAPE_COORDS_INT_T entryIdx = color_palette_ordered_idx_to_entry_idx(p, _userdata->index);
    
    if (r != nullptr || g != nullptr || b != nullptr || a != nullptr) {
        RGBAColor color = color_palette_get_color(p, entryIdx);
        
        if (r != nullptr) color.r = *r;
        if (g != nullptr) color.g = *g;
        if (b != nullptr) color.b = *b;
        if (a != nullptr) color.a = *a;
        
        color_palette_set_color(p, entryIdx, color);
    }
    
    if (light != nullptr) {
        color_palette_set_emissive(p, entryIdx, *light);
    }
}

static void _block_properties_get_color_handler(uint8_t *r,
                                       uint8_t *g,
                                       uint8_t *b,
                                       uint8_t *a,
                                       bool *light,
                                       lua_State * const L,
                                       const void *userdata) {
    
    const _block_properties_color_handler_userdata *_userdata = static_cast<const _block_properties_color_handler_userdata*>(userdata);
    vx_assert(_userdata->palette != nullptr);
    
    ColorPalette *p = static_cast<ColorPalette *>(weakptr_get(_userdata->palette));
    vx_assert(p != nullptr);

    const SHAPE_COORDS_INT_T entryIdx = color_palette_ordered_idx_to_entry_idx(p, _userdata->index);

    if (r != nullptr || g != nullptr || b != nullptr || a != nullptr) {
        RGBAColor color = color_palette_get_color(p, entryIdx);
        
        if (r != nullptr) *r = color.r;
        if (g != nullptr) *g = color.g;
        if (b != nullptr) *b = color.b;
        if (a != nullptr) *a = color.a;
    }
    
    if (light != nullptr) {
        *light = color_palette_is_emissive(p, entryIdx);
    }
}

static ColorPalette *_lua_block_properties_getPalettePointer(lua_State *L, int idx) {
    blockProperties_userdata* ud = static_cast<blockProperties_userdata*>(lua_touserdata(L, idx));
    ColorPalette *result = static_cast<ColorPalette*>(weakptr_get(ud->paletteWeakPtr));
    return result;
}

static SHAPE_COLOR_INDEX_INT_T _lua_block_properties_get_or_refresh_palette_index(lua_State *L,
        int idx, ColorPalette **out, SHAPE_COLOR_INDEX_INT_T *outEntryIdx) {
    RETURN_VALUE_IF_NULL(L, 0)
    LUAUTILS_STACK_SIZE(L)

    ColorPalette *palette = _lua_block_properties_getPalettePointer(L, idx);
    SHAPE_COLOR_INDEX_INT_T paletteIndex = SHAPE_COLOR_INDEX_AIR_BLOCK;
    if (out != nullptr) {
        *out = nullptr;
    }
    if (outEntryIdx != nullptr) {
        *outEntryIdx = SHAPE_COLOR_INDEX_AIR_BLOCK;
    }
    if (palette != nullptr) {
        if (out != nullptr) {
            *out = palette;
        }
        if (LUA_GET_METAFIELD_AND_RETURN_TYPE(L, idx, LUA_BLOCK_PROPERTIES_FIELD_COLOR) != LUA_TNIL) {
            RGBAColor color;
            lua_color_getRGBAL(L, -1, &color.r, &color.g, &color.b, &color.a, nullptr);
            lua_pop(L, 1); // pop color

            SHAPE_COLOR_INDEX_INT_T entryIdx;
            if (color_palette_find(palette, color, &entryIdx)) {
                paletteIndex = color_palette_entry_idx_to_ordered_idx(palette, entryIdx) + 1;
            }
            if (outEntryIdx != nullptr) {
                *outEntryIdx = entryIdx;
            }
        }
    } else if (LUA_GET_METAFIELD_AND_RETURN_TYPE(L, idx, LUA_BLOCK_PROPERTIES_FIELD_INDEX) != LUA_TNIL) {
        paletteIndex = lua_tointeger(L, -1);
        lua_pop(L, 1); // pop metafield
    }

    LUAUTILS_STACK_SIZE_ASSERT(L, 0)
    return paletteIndex;
}

static int _lua_block_properties_remove(lua_State *L) {
    RETURN_VALUE_IF_NULL(L, 0)
    LUAUTILS_STACK_SIZE(L)

    ColorPalette *palette = nullptr;
    SHAPE_COLOR_INDEX_INT_T entryIdx;
    _lua_block_properties_get_or_refresh_palette_index(L, 1, &palette, &entryIdx);
    bool removed = false;
    if (palette != nullptr) {
        removed = color_palette_remove_unused_color(palette, entryIdx, true);
    }
    lua_pushboolean(L, removed);

    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
    return 1;
}

static int _lua_block_properties_is_used(lua_State *L) {
    RETURN_VALUE_IF_NULL(L, 0)
    LUAUTILS_STACK_SIZE(L)

    ColorPalette *palette = nullptr;
    SHAPE_COLOR_INDEX_INT_T entryIdx;
    _lua_block_properties_get_or_refresh_palette_index(L, 1, &palette, &entryIdx);
    if (palette != nullptr) {
        const bool isUsed = color_palette_get_color_use_count(palette, entryIdx) > 0;

        lua_pushboolean(L, isUsed);

        LUAUTILS_STACK_SIZE_ASSERT(L, 1)
        return 1;
    }

    LUAUTILS_STACK_SIZE_ASSERT(L, 0)
    return 0;
}
