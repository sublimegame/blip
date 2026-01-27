//
//  lua_palette.cpp
//  Cubzh
//
//  Created by Adrien Duermael on 7/29/20.
//

// C++
#include <cstring>

#include "lua_color.hpp"
#include "lua_palette.hpp"
#include "lua.hpp"
#include "lua_utils.hpp"
#include "sbs.hpp"
#include "lua_block_properties.hpp"
#include "lua_shape.hpp"

#include "xptools.h"
#include "game.h"
#include "VXGame.hpp"

#define LUA_G_PALETTE_FIELD_LOAD            "Load"            // function (read-only)

#define LUA_PALETTE_FIELD_ADDCOLOR          "AddColor"        // function (read-only)
#define LUA_PALETTE_FIELD_REMOVECOLOR       "RemoveColor"     // function (read-only)
#define LUA_PALETTE_FIELD_MAX               "MaximumEntries"  // integer
#define LUA_PALETTE_FIELD_GETINDEX          "GetIndex"        // function (read-only)
#define LUA_PALETTE_FIELD_COPY              "Copy"            // function (read-only)
#define LUA_PALETTE_FIELD_MERGE             "Merge"           // function (read-only)
#define LUA_PALETTE_FIELD_REDUCE            "Reduce"          // function (read-only)

static int _lua_palette_addColor(lua_State *L);
static int _lua_palette_removeColor(lua_State *L);
static int _lua_palette_getIndex(lua_State *L);
static int _lua_palette_copy(lua_State *L);
static int _lua_palette_merge(lua_State *L);
static int _lua_palette_reduce(lua_State *L);

// --------------------------------------------------
// MARK: - C handlers
// --------------------------------------------------

typedef void (*palette_C_handler_free) (void *userdata);

// Default handler
typedef struct palette_C_handler {
    palette_C_handler_free free_userdata;
    void *userdata;
} palette_C_handler;

typedef struct palette_C_handler_userdata {
    Weakptr *palette;
    Weakptr *optionalShape;
} palette_C_handler_userdata;

void _free_palette_handler(void *userdata) {
    palette_C_handler_userdata *_userdata = static_cast<palette_C_handler_userdata *>(userdata);
    if (_userdata->optionalShape != nullptr) {
        weakptr_release(_userdata->optionalShape);
        _userdata->optionalShape = nullptr;
    }
    color_palette_release(static_cast<ColorPalette *>(weakptr_get(_userdata->palette)));
    weakptr_release(_userdata->palette);
    _userdata->palette = nullptr;
    free(_userdata);
}

typedef struct palette_userdata {
    // Userdata instance requirements
    vx::UserDataStore *store;
    palette_userdata *next;
    palette_userdata *previous;

    palette_C_handler *handler;
} palette_userdata;

palette_C_handler *_palette_getCHandler(lua_State * const L, const int idx) {
    vx_assert(L != nullptr);
    LUAUTILS_ASSERT_ARG_TYPE(L, idx, ITEM_TYPE_PALETTE)
    palette_userdata *ud = static_cast<palette_userdata*>(lua_touserdata(L, idx));
    return ud->handler;
}

// --------------------------------------------------
//
// MARK: - Static functions' prototypes -
//
// --------------------------------------------------

static int _palette_metatable_index(lua_State *L);
static int _palette_metatable_newindex(lua_State *L);
static void _palette_gc(void *_ud);
static int _palette_metatable_len(lua_State *L);

static int _g_palette_metatable_index(lua_State *L);
static int _g_palette_metatable_call(lua_State *L);

// --------------------------------------------------
//
// MARK: - Exposed functions -
//
// --------------------------------------------------

bool lua_g_palette_create_and_push(lua_State * const L) {
    vx_assert(L != nullptr);
    LUAUTILS_STACK_SIZE(L)

    lua_newuserdata(L, 1);
    lua_newtable(L);
    {
        lua_pushstring(L, "__index");
        lua_pushcfunction(L, _g_palette_metatable_index, "");
        lua_rawset(L, -3);

        lua_pushstring(L, "__call");
        lua_pushcfunction(L, _g_palette_metatable_call, "");
        lua_rawset(L, -3);
        
        lua_pushstring(L, "__metatable");
        lua_pushboolean(L, false);
        lua_rawset(L, -3);

        lua_pushstring(L, "__type");
        lua_pushstring(L, "PaletteInterface");
        lua_rawset(L, -3);

        lua_pushstring(L, "__typen");
        lua_pushinteger(L, ITEM_TYPE_G_PALETTE);
        lua_rawset(L, -3);
    }
    lua_setmetatable(L, -2);

    LUAUTILS_STACK_SIZE_ASSERT(L, 1);
    return true;
}

int lua_g_palette_snprintf(lua_State * const L, char *result, const size_t maxLen, const bool spacePrefix) {
    LUAUTILS_STACK_SIZE(L)
    LUAUTILS_STACK_SIZE_ASSERT(L, 0)
    return snprintf(result, maxLen, "%s[Palette] (global)", spacePrefix ? " " : "");
}

static int _g_palette_metatable_index(lua_State * const L) {
    if (L == nullptr) {
        return 0;
    }
    LUAUTILS_STACK_SIZE(L)

    // validate 2nd argument: key
    if (lua_utils_isStringStrict(L, 2) == false) {
        lua_pushnil(L);
        LUAUTILS_STACK_SIZE_ASSERT(L, 1)
        return 1;
    }

    const char *key = lua_tostring(L, 2);
    if (strcmp(key, LUA_G_PALETTE_FIELD_LOAD) == 0) {
        if(luau_dostring(L, R"""(
return function(self, itemName, callback)
    Assets:Load(itemName, AssetType.Palette, function(assets)
        if assets == nil or #assets == 0 then return callback(nil) end
        callback(assets[1])
    end)
end
        )""", "Palette.Load") == false) {
            lua_pushnil(L);
        }
    } else {
        lua_pushnil(L);
    }

    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
    return 1;
}

static int _g_palette_metatable_call(lua_State *L) {
    RETURN_VALUE_IF_NULL(L, 0)
    LUAUTILS_STACK_SIZE(L)

    ColorPalette *p = color_palette_new(nullptr);
    if (lua_palette_create_and_push_table(L, p, nullptr) == false) {
        lua_pushnil(L); // GC will release transform
    }
    color_palette_release(p); // now owned by lua Palette

    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
    return 1;
}

int lua_palette_snprintf(lua_State *L,
                         char* result,
                         size_t maxLen,
                         bool spacePrefix) {
    ColorPalette * const palette = lua_palette_getCPointer(L, -1, nullptr);
    if (palette == nullptr) {
        LUAUTILS_INTERNAL_ERROR(L);
    }
    return snprintf(result, maxLen, "%s[Palette %d colors] (instance)", spacePrefix ? " " : "",
                    color_palette_get_ordered_count(palette));
}

bool lua_palette_create_and_push_table(lua_State * const L, ColorPalette *palette, Shape *optionalShape) {
    vx_assert(L != nullptr);

    // lua Palette takes ownership
    if (color_palette_retain(palette) == false) {
        LUAUTILS_INTERNAL_ERROR(L);
        // return false;
    }

    LUAUTILS_STACK_SIZE(L)
    
    vx::Game *g = nullptr;
    if (lua_utils_getGameCppWrapperPtr(L, &g) == false) {
        LUAUTILS_INTERNAL_ERROR(L);
    }

    palette_C_handler *handler = static_cast<palette_C_handler*>(malloc(sizeof(palette_C_handler)));
    handler->free_userdata = _free_palette_handler;
    palette_C_handler_userdata *userdata = static_cast<palette_C_handler_userdata*>(malloc(sizeof(palette_C_handler_userdata)));
    userdata->palette = color_palette_get_and_retain_weakptr(palette);
    userdata->optionalShape = optionalShape != nullptr ? shape_get_and_retain_weakptr(optionalShape) : nullptr;
    handler->userdata = userdata;

    palette_userdata *ud = static_cast<palette_userdata *>(lua_newuserdatadtor(L, sizeof(palette_userdata), _palette_gc));
    ud->handler = handler; // TODO: get rid of "__handlers"

    // connect to userdata store
    ud->store = g->userdataStoreForPalettes;
    ud->previous = nullptr;
    palette_userdata* next = static_cast<palette_userdata*>(ud->store->add(ud));
    ud->next = next;
    if (next != nullptr) {
        next->previous = ud;
    }

    lua_newtable(L); // metatable + 2
    {
        lua_pushstring(L, "__index");
        lua_pushcfunction(L, _palette_metatable_index, "");
        lua_rawset(L, -3);
        
        lua_pushstring(L, "__newindex");
        lua_pushcfunction(L, _palette_metatable_newindex, "");
        lua_rawset(L, -3);

        lua_pushstring(L, "__len");
        lua_pushcfunction(L, _palette_metatable_len, "");
        lua_rawset(L, -3);
        
        lua_pushstring(L, "__metatable");
        lua_pushboolean(L, false);
        lua_rawset(L, -3);

        lua_pushstring(L, "__type");
        lua_pushstring(L, "Palette");
        lua_rawset(L, -3);

        lua_pushstring(L, "__typen");
        lua_pushinteger(L, ITEM_TYPE_PALETTE);
        lua_rawset(L, -3);
    }
    lua_setmetatable(L, -2); // metatable is popped here // + 2

    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
    return true;
}

ColorPalette *lua_palette_getCPointer(lua_State *L, const int idx, Shape **optionalShape) {
    palette_C_handler *handlerPalette = _palette_getCHandler(L, idx);
    vx_assert(handlerPalette != nullptr);

    palette_C_handler_userdata *_userdata = static_cast<palette_C_handler_userdata *>(handlerPalette->userdata);

    ColorPalette *palette = static_cast<ColorPalette*>(weakptr_get(_userdata->palette));
    if (optionalShape != nullptr) {
        *optionalShape = static_cast<Shape*>(weakptr_get(_userdata->optionalShape));
    }

    return palette;
}

static void _palette_gc(void *_ud) {
    palette_userdata *ud = static_cast<palette_userdata*>(_ud);
    if (ud->handler != nullptr) {
        ud->handler->free_userdata(ud->handler->userdata);
        free(ud->handler);
        ud->handler = nullptr;
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

/// arguments
/// - lua table : Palette table
/// - lua int : the key that is being accessed
static int _palette_metatable_index(lua_State *L) {
    vx_assert(L != nullptr);
    LUAUTILS_ASSERT_ARGS_COUNT(L, 2)
    LUAUTILS_ASSERT_ARG_TYPE(L, 1, ITEM_TYPE_PALETTE)
    LUAUTILS_STACK_SIZE(L)

    ColorPalette * const palette = lua_palette_getCPointer(L, 1, nullptr);
    if (palette == nullptr) {
        LUAUTILS_INTERNAL_ERROR(L);
    }

    if (lua_utils_isIntegerStrict(L, 2)) {
        const int luaIdx = static_cast<int>(lua_tointeger(L, 2));
        if (luaIdx < 1 || luaIdx > color_palette_get_ordered_count(palette)) {
            LUAUTILS_ERROR(L, "Palette entries index out of range"); // returns
        }

        lua_block_properties_create_and_push_table(L, palette, luaIdx);
    } else if (lua_isstring(L, 2)) {
        const char *key = lua_tostring(L, 2);
        if (strcmp(key, LUA_PALETTE_FIELD_ADDCOLOR) == 0) {
            lua_pushcfunction(L, _lua_palette_addColor, "");
        } else if (strcmp(key, LUA_PALETTE_FIELD_REMOVECOLOR) == 0) {
            lua_pushcfunction(L, _lua_palette_removeColor, "");
        } else if (strcmp(key, LUA_PALETTE_FIELD_MAX) == 0) {
            lua_pushinteger(L, SHAPE_COLOR_INDEX_MAX_COUNT);
        } else if (strcmp(key, LUA_PALETTE_FIELD_GETINDEX) == 0) {
            lua_pushcfunction(L, _lua_palette_getIndex, "");
        } else if (strcmp(key, LUA_PALETTE_FIELD_COPY) == 0) {
            lua_pushcfunction(L, _lua_palette_copy, "");
        } else if (strcmp(key, LUA_PALETTE_FIELD_MERGE) == 0) {
            lua_pushcfunction(L, _lua_palette_merge, "");
        } else if (strcmp(key, LUA_PALETTE_FIELD_REDUCE) == 0) {
            lua_pushcfunction(L, _lua_palette_reduce, "");
        } else {
            LUAUTILS_ERROR_F(L, "Palette: %s field does not exist", key);
        }
    } else {
        lua_pushnil(L);
    }

    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
    return 1; // the __index function always has 1 return value
}

/// arguments
/// - table (table) : Palette table
/// - key   (int)   : the key that is being set
/// - value (any)   : the value being stored
static int _palette_metatable_newindex(lua_State *L) {
    vx_assert(L != nullptr);
    LUAUTILS_ASSERT_ARGS_COUNT(L, 3)
    LUAUTILS_ASSERT_ARG_TYPE(L, 1, ITEM_TYPE_PALETTE)
    LUAUTILS_STACK_SIZE(L)

    if (lua_utils_getObjectType(L, 3) != ITEM_TYPE_BLOCKPROPERTIES) {
        ColorPalette * const palette = lua_palette_getCPointer(L, 1, nullptr);
        if (palette == nullptr) {
            LUAUTILS_INTERNAL_ERROR(L);
        }

        const char *key = lua_tostring(L, 2);
        LUAUTILS_ERROR_F(L, "Palette: %s field does not exist", key);
    }
    
    LUAUTILS_STACK_SIZE_ASSERT(L, 0)
    return 0;
}

static int _palette_metatable_len(lua_State *L) {
    vx_assert(L != nullptr);
    // docs seems to say only one argument is expected, but I get 2...
    // https://www.lua.org/manual/5.3/manual.html (see __len)
    LUAUTILS_ASSERT_ARGS_COUNT(L, 2)
    LUAUTILS_ASSERT_ARG_TYPE(L, 1, ITEM_TYPE_PALETTE)
    LUAUTILS_STACK_SIZE(L)

    ColorPalette * const palette = lua_palette_getCPointer(L, 1, nullptr);
    if (palette == nullptr) {
        LUAUTILS_INTERNAL_ERROR(L);
    }

    lua_pushinteger(L, color_palette_get_ordered_count(palette));
    
    LUAUTILS_STACK_SIZE_ASSERT(L, 1);
    return 1;
    
}

// MARK: - Private functions -

static int _lua_palette_addColor(lua_State *L) {
    RETURN_VALUE_IF_NULL(L, 0)
    LUAUTILS_STACK_SIZE(L)

    if (lua_utils_getObjectType(L, 1) != ITEM_TYPE_PALETTE) {
        LUAUTILS_ERROR(L, "[Palette:AddColor] function must be called with `:`");
    }

    if (lua_gettop(L) != 2) {
        LUAUTILS_ERROR(L, "[Palette:AddColor] this function takes 1 argument (Color)");
    }

    ColorPalette * const palette = lua_palette_getCPointer(L, 1, nullptr);
    if (palette == nullptr) {
        LUAUTILS_INTERNAL_ERROR(L);
    }

    RGBAColor color;
    if (lua_color_getRGBAL(L, 2, &color.r, &color.g, &color.b, &color.a, nullptr) == false) {
        LUAUTILS_ERROR(L, "Palette:AddColor argument should be a Color");
    }

    SHAPE_COLOR_INDEX_INT_T entryIdx;
    if (color_palette_check_and_add_color(palette, color, &entryIdx, true) == false) {
        LUAUTILS_ERROR(L, "Palette:AddColor could not add color (palette full?)");
    }

    const SHAPE_COLOR_INDEX_INT_T orderedIdx = color_palette_entry_idx_to_ordered_idx(palette, entryIdx);
    lua_pushinteger(L, orderedIdx + 1); // Lua is 1-indexed

    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
    return 1;
}

static int _lua_palette_removeColor(lua_State *L) {
    RETURN_VALUE_IF_NULL(L, 0)
    LUAUTILS_STACK_SIZE(L)

    if (lua_utils_getObjectType(L, 1) != ITEM_TYPE_PALETTE) {
        LUAUTILS_ERROR(L, "[Palette:RemoveColor] function must be called with `:`");
    }

    if (lua_gettop(L) != 2) {
        LUAUTILS_ERROR(L, "[Palette:RemoveColor] this function takes 1 argument (integer)");
    }

    Shape *shape = nullptr;
    ColorPalette * const palette = lua_palette_getCPointer(L, 1, &shape);
    if (palette == nullptr) {
        LUAUTILS_INTERNAL_ERROR(L);
    }

    if (lua_utils_isIntegerStrict(L, 2) == false) {
        LUAUTILS_ERROR(L, "Palette:RemoveColor argument should be an integer");
    }

    const long luaIdx = lua_tointeger(L, 2);
    if (luaIdx < 1 || luaIdx > color_palette_get_ordered_count(palette)) {
        LUAUTILS_ERROR(L, "Palette:RemoveColor index out of range");
    }

    if (shape != nullptr) {
        shape_apply_current_transaction(shape, true);
    }

    const SHAPE_COLOR_INDEX_INT_T entryIdx = color_palette_ordered_idx_to_entry_idx(palette, luaIdx - 1);
    const bool removed = color_palette_remove_unused_color(palette, entryIdx, true);

    lua_pushboolean(L, removed);

    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
    return 1;
}

static int _lua_palette_getIndex(lua_State *L) {
    RETURN_VALUE_IF_NULL(L, 0)
    LUAUTILS_STACK_SIZE(L)

    if (lua_utils_getObjectType(L, 1) != ITEM_TYPE_PALETTE) {
        LUAUTILS_ERROR(L, "[Palette:GetIndex] function must be called with `:`");
    }

    if (lua_gettop(L) != 2) {
        LUAUTILS_ERROR(L, "[Palette:GetIndex] this function takes 1 argument");
    }

    ColorPalette * const palette = lua_palette_getCPointer(L, 1, nullptr);
    if (palette == nullptr) {
        LUAUTILS_INTERNAL_ERROR(L);
    }

    RGBAColor color;
    if (lua_color_getRGBAL(L, 2, &color.r, &color.g, &color.b, &color.a, nullptr) == false) {
        LUAUTILS_ERROR(L, "Palette:GetIndex argument should be a Color");
    }

    SHAPE_COLOR_INDEX_INT_T entryIdx;
    if (color_palette_find(palette, color, &entryIdx)) {
        const SHAPE_COLOR_INDEX_INT_T orderedIdx = color_palette_entry_idx_to_ordered_idx(palette, entryIdx);
        lua_pushinteger(L, orderedIdx + 1); // Lua is 1-indexed
    } else {
        lua_pushnil(L);
    }

    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
    return 1;
}

static int _lua_palette_copy(lua_State *L) {
    RETURN_VALUE_IF_NULL(L, 0)
    LUAUTILS_STACK_SIZE(L)

    if (lua_utils_getObjectType(L, 1) != ITEM_TYPE_PALETTE) {
        LUAUTILS_ERROR(L, "[Palette:Copy] function must be called with `:`");
    }

    ColorPalette *palette = lua_palette_getCPointer(L, 1, nullptr);
    if (palette == nullptr) {
        LUAUTILS_INTERNAL_ERROR(L);
    }

    ColorPalette *copy = color_palette_new_copy(palette);
    if (lua_palette_create_and_push_table(L, copy, nullptr) == false) {
        lua_pushnil(L); // GC will release transform
    }
    color_palette_release(copy); // now owned by lua Palette

    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
    return 1;
}

typedef struct {
    ColorPalette *palette;
    bool allowDuplicates;
    bool remap;
} _lua_palette_merge_data;

static bool _lua_palette_merge_recurse_func(Transform *t, void *ptr, uint32_t depth) {
    if (depth >= LUA_MAX_RECURSION_DEPTH) {
        return true; // stop recursion
    }
    if (transform_get_type(t) == ShapeTransform) {
        Shape *mergeShape = static_cast<Shape*>(transform_get_ptr(t));
        _lua_palette_merge_data *data = static_cast<_lua_palette_merge_data*>(ptr);

        if (data->remap) {
            SHAPE_COLOR_INDEX_INT_T *remap;
            color_palette_merge(data->palette, shape_get_palette(mergeShape), data->allowDuplicates, &remap);
            shape_set_palette(mergeShape, data->palette, true);
            shape_remap_colors(mergeShape, remap);
            free(remap);
        } else {
            color_palette_merge(data->palette, shape_get_palette(mergeShape), data->allowDuplicates, nullptr);
        }
    }
    return false; // do not stop recursion
}

/// Palette:Merge(palette, { duplicates=false })
/// Palette:Merge(shape, { duplicates=false, remap=false, recurse=false })
static int _lua_palette_merge(lua_State *L) {
    RETURN_VALUE_IF_NULL(L, 0)
    LUAUTILS_STACK_SIZE(L)

    if (lua_utils_getObjectType(L, 1) != ITEM_TYPE_PALETTE) {
        LUAUTILS_ERROR(L, "[Palette:Merge] function must be called with `:`");
    }

    ColorPalette *palette = lua_palette_getCPointer(L, 1, nullptr);
    if (palette == nullptr) {
        LUAUTILS_INTERNAL_ERROR(L);
    }

    // optional table parameters
    bool allowDuplicates = false;
    bool remap = false;
    bool recurse = false;
    if (lua_istable(L, 3)) {
        lua_getfield(L, 3, "duplicates");
        if (lua_isboolean(L, -1)) {
            allowDuplicates = lua_toboolean(L, -1);
        }
        lua_pop(L, 1);
        lua_getfield(L, 3, "remap");
        if (lua_isboolean(L, -1)) {
            remap = lua_toboolean(L, -1);
        }
        lua_pop(L, 1);
        lua_getfield(L, 3, "recurse");
        if (lua_isboolean(L, -1)) {
            recurse = lua_toboolean(L, -1);
        }
        lua_pop(L, 1);
    }

    if (lua_utils_getObjectType(L, 2) == ITEM_TYPE_PALETTE) {
        ColorPalette *mergePalette = lua_palette_getCPointer(L, 2, nullptr);
        if (mergePalette == nullptr) {
            LUAUTILS_INTERNAL_ERROR(L);
        }

        color_palette_merge(palette, mergePalette, allowDuplicates, nullptr);
    } else if (lua_shape_isShape(L , 2)) {
        Shape *mergeShape = lua_shape_getShapePtr(L, 2);
        if (mergeShape == nullptr) {
            LUAUTILS_INTERNAL_ERROR(L);
        }

        _lua_palette_merge_data *data = static_cast<_lua_palette_merge_data*>(malloc(sizeof(_lua_palette_merge_data)));
        data->palette = palette;
        data->allowDuplicates = allowDuplicates;
        data->remap = remap;
        _lua_palette_merge_recurse_func(shape_get_transform(mergeShape), data, 0);

        if (recurse) {
            transform_recurse_depth(shape_get_transform(mergeShape), _lua_palette_merge_recurse_func, data, false, false, 0);
        }

        free(data);
    } else {
        LUAUTILS_ERROR(L, "[Palette:Merge] argument should be a Palette or a Shape");
    }

    LUAUTILS_STACK_SIZE_ASSERT(L, 0)
    return 0;
}

/// Palette:Reduce()
static int _lua_palette_reduce(lua_State *L) {
    RETURN_VALUE_IF_NULL(L, 0)
    LUAUTILS_STACK_SIZE(L)

    if (lua_utils_getObjectType(L, 1) != ITEM_TYPE_PALETTE) {
        LUAUTILS_ERROR(L, "[Palette:Reduce] function must be called with `:`");
    }

    ColorPalette *palette = lua_palette_getCPointer(L, 1, nullptr);
    if (palette == nullptr) {
        LUAUTILS_INTERNAL_ERROR(L);
    }

    color_palette_remove_all_unused_colors(palette, true);

    LUAUTILS_STACK_SIZE_ASSERT(L, 0)
    return 0;
}
