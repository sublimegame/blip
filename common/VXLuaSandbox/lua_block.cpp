//
//  lua_block.cpp
//  Cubzh
//
//  Created by Gaetan de Villele on 12/12/2019.
//  Copyright © 2019 Voxowl Inc. All rights reserved.
//

#include "lua_block.hpp"

// C++
#include <cmath>
#include <cstring>

#include "VXGame.hpp"

// Lua
#include "lua.hpp"
#include "lua_utils.hpp"
#include "lua_number3.hpp"
#include "lua_face.hpp"
#include "lua_map.hpp"
#include "lua_color.hpp"

// sandbox
#include "sbs.hpp"
#include "lua_block_properties.hpp"

// engine
#include "colors.h"
#include "filo_list_int3.h"
#include "game.h"

// xptools
#include "xptools.h"

/// Lua table keys for block
#define LUA_BLOCK_FIELD_COORDINATES     "Coordinates"       // get (Number3)
#define LUA_BLOCK_FIELD_COORDS          "Coords"            // get (Number3)
#define LUA_BLOCK_FIELD_POSITION        "Position"          // get (Number3)
#define LUA_BLOCK_FIELD_POS             "Pos"               // get (Number3)
#define LUA_BLOCK_FIELD_LOCALPOSITION   "LocalPosition"     // get (Number3)
#define LUA_BLOCK_FIELD_ADDNEIGHBOR     "AddNeighbor"       // function (read only)
#define LUA_BLOCK_FIELD_REMOVE          "Remove"            // function (read only)
#define LUA_BLOCK_FIELD_REPLACE         "Replace"           // function (read only)
#define LUA_BLOCK_FIELD_PALETTEINDEX    "PaletteIndex"      // color palette index (read only)
#define LUA_BLOCK_FIELD_COLOR           "Color"             // color

// --------------------------------------------------
// MARK: - C handlers -
// --------------------------------------------------

typedef void (*block_C_handler_free) (void *userdata);

// Default handler
typedef struct block_C_handler {
    block_C_handler_free free_userdata;
    void *userdata;
} block_C_handler;

// NOTE (aduermael): we could have different types of userdata to save space
typedef struct block_C_handler_userdata {
    Weakptr *parentShapeTransform;
    // color stored from standalone block constructor
    RGBAColor color;
    // lua coordinates
    SHAPE_COORDS_INT_T x, y, z;
    // lua index is ordered and starts at 1, it is only set if block came from a shape
    SHAPE_COLOR_INDEX_INT_T luaPaletteIndex;
} block_C_handler_userdata;

void _free_block_handler(void *userdata);
block_C_handler *_block_getCHandler(lua_State * const L, const int idx);

typedef struct block_userdata {
    // Userdata instance requirements
    vx::UserDataStore *store;
    block_userdata *next;
    block_userdata *previous;

    block_C_handler *handler;
} block_userdata;
// --------------------------------------------------
// MARK: - private functions prototypes -
// --------------------------------------------------

static int _g_block_index(lua_State * const L);
static int _g_block_call(lua_State * const L);
static int _g_block_newindex(lua_State * const L);

static int _block_index(lua_State * const L);
static int _block_newindex(lua_State * const L);
static void _block_gc(void* _ud);

static int _block_addneighbor(lua_State * const L);
static int _block_remove(lua_State * const L);
static int _block_replace(lua_State * const L);

SHAPE_COLOR_INDEX_INT_T _block_get_or_refresh_palette_index(Shape *s,
                                                            const block_C_handler_userdata *userdata);
bool _block_pushBlock(lua_State * const L, block_C_handler_userdata *userdata);
bool _block_pushBlockWithIndex(lua_State * const L,
                               Shape *parentShape,
                               const SHAPE_COLOR_INDEX_INT_T luaPaletteIndex,
                               const SHAPE_COORDS_INT_T *x,
                               const SHAPE_COORDS_INT_T *y,
                               const SHAPE_COORDS_INT_T *z);
bool _block_pushBlockWithColor(lua_State * const L,
                               RGBAColor color,
                               const SHAPE_COORDS_INT_T *x,
                               const SHAPE_COORDS_INT_T *y,
                               const SHAPE_COORDS_INT_T *z);

// --------------------------------------------------
// MARK: - exposed functions
// --------------------------------------------------

void lua_g_block_create_and_push(lua_State * const L) {
    vx_assert(L != nullptr);
    LUAUTILS_STACK_SIZE(L)
    
    lua_newuserdata(L, 1); // global "Block" table
    lua_newtable(L); // metatable
    // creates metatable for "Block" global table
    {
        lua_pushstring(L, "__index");
        lua_pushcfunction(L, _g_block_index, "");
        lua_rawset(L, -3);

        lua_pushstring(L, "__newindex");
        lua_pushcfunction(L, _g_block_newindex, "");
        lua_rawset(L, -3);

        lua_pushstring(L, "__call");
        lua_pushcfunction(L, _g_block_call, "");
        lua_rawset(L, -3);

        // hide the metatable from the Lua sandbox user
        lua_pushstring(L, "__metatable");
        lua_pushboolean(L, false);
        lua_rawset(L, -3);

        lua_pushstring(L, "__type");
        lua_pushstring(L, "BlockInterface");
        lua_rawset(L, -3);

        lua_pushstring(L, "__typen");
        lua_pushinteger(L, ITEM_TYPE_G_BLOCK);
        lua_rawset(L, -3);
    }
    lua_setmetatable(L, -2);
    
    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
}

void lua_block_pushBlock(lua_State * const L,
                         Shape *parentShape,
                         const SHAPE_COORDS_INT_T x,
                         const SHAPE_COORDS_INT_T y,
                         const SHAPE_COORDS_INT_T z) {
    vx_assert(L != nullptr);
    vx_assert(parentShape != nullptr);
    LUAUTILS_STACK_SIZE(L)
    
    const Block *b = shape_get_block(parentShape, x, y, z);
    
    if (block_is_solid(b) == false) {
        lua_pushnil(L);
    } else {
        const SHAPE_COLOR_INDEX_INT_T orderedIdx = color_palette_entry_idx_to_ordered_idx(
                shape_get_palette(parentShape), b->colorIndex);
        _block_pushBlockWithIndex(L, parentShape, orderedIdx + 1, &x, &y, &z);
    }
    
    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
}

void lua_block_addBlockInShapeAndPush(lua_State * const L,
                                      Shape * const parentShape,
                                      const SHAPE_COLOR_INDEX_INT_T luaPaletteIndex,
                                      const SHAPE_COORDS_INT_T x,
                                      const SHAPE_COORDS_INT_T y,
                                      const SHAPE_COORDS_INT_T z) {
    vx_assert(L != nullptr);
    LUAUTILS_STACK_SIZE(L)
    
    if (parentShape == nullptr) {
        lua_pushboolean(L, false);
    } else {
        if (shape_is_lua_mutable(parentShape) == false) {
            lua_pushboolean(L, false);
        } else {
            vx::Game *cppGame = nullptr;
            if (lua_utils_getGameCppWrapperPtr(L, &cppGame) == false || cppGame == nullptr) {
                LUAUTILS_INTERNAL_ERROR(L)
                // return;
            }

            const SHAPE_COLOR_INDEX_INT_T entryIdx = color_palette_ordered_idx_to_entry_idx(
                    shape_get_palette(parentShape), luaPaletteIndex - 1);

            const bool added = shape_add_block_as_transaction(parentShape,
                                                        game_get_scene(cppGame->getCGame()),
                                                        entryIdx,
                                                        x,
                                                        y,
                                                        z);
            lua_pushboolean(L, added);
        }
    }
    
    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
}

void lua_block_getInfo(lua_State * const L, const int idx,
                       Shape **parentShape,
                       SHAPE_COORDS_INT_T *x,
                       SHAPE_COORDS_INT_T *y,
                       SHAPE_COORDS_INT_T *z,
                       SHAPE_COLOR_INDEX_INT_T *luaPaletteIndex,
                       RGBAColor *color) {
    
    vx_assert(L != nullptr);
    vx_assert(lua_utils_getObjectType(L, idx) == ITEM_TYPE_BLOCK);
    LUAUTILS_STACK_SIZE(L)
    
    block_C_handler *handler = _block_getCHandler(L, idx);
    vx_assert(handler != nullptr);
    
    block_C_handler_userdata *userdata = static_cast<block_C_handler_userdata *>(handler->userdata);

    Shape *s = transform_utils_get_shape(static_cast<Transform *>(weakptr_get(userdata->parentShapeTransform)));
    if (parentShape != nullptr) {
        *parentShape = s;
    }
    if (x != nullptr) *x = userdata->x;
    if (y != nullptr) *y = userdata->y;
    if (z != nullptr) *z = userdata->z;
    if (luaPaletteIndex != nullptr) *luaPaletteIndex = _block_get_or_refresh_palette_index(s, userdata);
    if (color != nullptr) {
        if (s == nullptr) {
            *color = userdata->color;
        } else {
            const SHAPE_COLOR_INDEX_INT_T luaPaletteIdx = _block_get_or_refresh_palette_index(s, userdata);
            const ColorPalette *p = shape_get_palette(s);
            *color = color_palette_get_color(p, color_palette_ordered_idx_to_entry_idx(p, luaPaletteIdx - 1));
        }
    }
    
    LUAUTILS_STACK_SIZE_ASSERT(L, 0)
}

int lua_block_snprintf(lua_State * const L, char* result, size_t maxLen, bool spacePrefix) {
    vx_assert(L != nullptr);
    LUAUTILS_STACK_SIZE(L)
    
    SHAPE_COORDS_INT_T x;
    SHAPE_COORDS_INT_T y;
    SHAPE_COORDS_INT_T z;
    SHAPE_COLOR_INDEX_INT_T luaPaletteIndex;
    
    lua_block_getInfo(L, -1, nullptr, &x, &y, &z, &luaPaletteIndex);
    
    LUAUTILS_STACK_SIZE_ASSERT(L, 0)
    
    return snprintf(result, maxLen, "%s[Block ID: %d X: %d Y: %d Z: %d]", spacePrefix ? " " : "", luaPaletteIndex, x, y, z);
}

// --------------------------------------------------
//
// MARK: - private functions -
//
// --------------------------------------------------

static int _g_block_index(lua_State * const L) {
    vx_assert(L != nullptr);
    LUAUTILS_ASSERT_ARGS_COUNT(L, 2)
    LUAUTILS_ASSERT_ARG_TYPE(L, 1, ITEM_TYPE_G_BLOCK)
    LUAUTILS_STACK_SIZE(L)
    
    LUAUTILS_STACK_SIZE_ASSERT(L, 0)
    
    // no fields in Block class
    lua_pushnil(L);
    
    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
    return 1;
}

///
static int _g_block_newindex(lua_State * const L) {
    LUAUTILS_ERROR(L, "Block class is read-only");
    return 0;
}

/// args are:
/// - Color
/// - X (number) (optional)
/// - Y (number) (optional)
/// - Z (number) (optional)
/// OR
/// - Color
/// - XYZ (Number3)
static int _g_block_call(lua_State * const L) {
    vx_assert(L != nullptr);
    LUAUTILS_STACK_SIZE(L)
    
    const int argCount = lua_gettop(L);
    // NOTE: first arg is the global Block itself
    if (argCount != 2 && argCount != 3 && argCount != 5) {
        LUAUTILS_ERROR(L, "Block - this function accepts 1, 2 or 4 arguments");
    }

    RGBAColor color;
    if (lua_color_getRGBAL(L, 2, &color.r, &color.g, &color.b, &color.a, nullptr) == false) {
        LUAUTILS_ERROR(L, "Block - 1st argument should be a Color");
    }

    float fx = 0.0f, fy = 0.0f, fz = 0.0f;
    if (argCount == 3) { // Number3
        if (lua_number3_or_table_getXYZ(L, 3, &fx, &fy, &fz) == false) {
            LUAUTILS_ERROR(L, "Block - argument 2 should be a Number3");
        }
        fx = std::floor(fx);
        fy = std::floor(fy);
        fz = std::floor(fz);

    } else if (argCount == 5) { // X, Y, Z (numbers)
        if (lua_isnumber(L, 3) == false || lua_isnumber(L, 4) == false ||
            lua_isnumber(L, 5) == false) {
            LUAUTILS_ERROR(L, "Block - arguments 2, 3 and 4 type should be numbers");
        }
        fx = std::floor(lua_tonumber(L, 3));
        fy = std::floor(lua_tonumber(L, 4));
        fz = std::floor(lua_tonumber(L, 5));
    }

    if (fx < SHAPE_COORDS_MIN || fx > SHAPE_COORDS_MAX ||
        fy < SHAPE_COORDS_MIN || fy > SHAPE_COORDS_MAX ||
        fz < SHAPE_COORDS_MIN || fz > SHAPE_COORDS_MAX) {
        LUAUTILS_ERROR(L, "Block - maximum coordinates reached");
    }

    const SHAPE_COORDS_INT_T x = static_cast<SHAPE_COORDS_INT_T>(fx);
    const SHAPE_COORDS_INT_T y = static_cast<SHAPE_COORDS_INT_T>(fy);
    const SHAPE_COORDS_INT_T z = static_cast<SHAPE_COORDS_INT_T>(fz);
    
    if (_block_pushBlockWithColor(L, color, &x, &y, &z) == false) {
        lua_pushnil(L);
    }
    
    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
    return 1;
}

static int _block_index(lua_State * const L) {
    vx_assert(L != nullptr);
    LUAUTILS_ASSERT_ARGS_COUNT(L, 2)
    LUAUTILS_ASSERT_ARG_TYPE(L, 1, ITEM_TYPE_BLOCK)
    LUAUTILS_STACK_SIZE(L)
    
    const char *key = lua_tostring(L, 2);
    
    if (strcmp(key, LUA_BLOCK_FIELD_COORDS) == 0 || strcmp(key, LUA_BLOCK_FIELD_COORDINATES) == 0) {
        SHAPE_COORDS_INT_T x, y, z;
        lua_block_getInfo(L, 1, nullptr, &x, &y, &z, nullptr);
        lua_number3_pushNewObject(L, x, y, z);
    } else if (strcmp(key, LUA_BLOCK_FIELD_POS) == 0 || strcmp(key, LUA_BLOCK_FIELD_POSITION) == 0) {
        Shape *s;
        SHAPE_COORDS_INT_T x, y, z;
        lua_block_getInfo(L, 1, &s, &x, &y, &z, nullptr);
        if (s == nullptr) {
            lua_pushnil(L);
        } else {
            float3 world = shape_block_to_world(s, x, y, z);
            lua_number3_pushNewObject(L, world.x, world.y, world.z);
        }
    } else if (strcmp(key, LUA_BLOCK_FIELD_LOCALPOSITION) == 0) {
        Shape *s;
        SHAPE_COORDS_INT_T x, y, z;
        lua_block_getInfo(L, 1, &s, &x, &y, &z, nullptr);
        if (s == nullptr) {
            lua_pushnil(L);
        } else {
            float3 local = shape_block_to_local(s, x, y, z);
            lua_number3_pushNewObject(L, local.x, local.y, local.z);
        }
    } else if (strcmp(key, LUA_BLOCK_FIELD_ADDNEIGHBOR) == 0) {
        lua_pushcfunction(L, _block_addneighbor, "");
    } else if (strcmp(key, LUA_BLOCK_FIELD_REMOVE) == 0) {
        lua_pushcfunction(L, _block_remove, "");
    } else if (strcmp(key, LUA_BLOCK_FIELD_REPLACE) == 0) {
        lua_pushcfunction(L, _block_replace, "");
    } else if (strcmp(key, LUA_BLOCK_FIELD_PALETTEINDEX) == 0) {
        SHAPE_COLOR_INDEX_INT_T luaPaletteIndex;
        lua_block_getInfo(L, 1, nullptr, nullptr, nullptr, nullptr, &luaPaletteIndex);
        lua_pushinteger(L, luaPaletteIndex);
    } else if (strcmp(key, LUA_BLOCK_FIELD_COLOR) == 0) {
        Shape *shape = nullptr;
        RGBAColor color;
        SHAPE_COLOR_INDEX_INT_T luaPaletteIdx;
        lua_block_getInfo(L, 1, &shape, nullptr, nullptr, nullptr, &luaPaletteIdx, &color);
        if (shape != nullptr) {
            ColorPalette *palette = shape_get_palette(shape);
            const SHAPE_COORDS_INT_T entryIdx = color_palette_ordered_idx_to_entry_idx(palette, luaPaletteIdx - 1);
            color = color_palette_get_color(palette, entryIdx);
        }
        lua_color_create_and_push_table(L, color.r, color.g, color.b, color.a, false, true);
    } else {
        lua_pushnil(L);
    }
    
    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
    return 1;
}

/// __newindex function for the metatable of the block table
///
/// arguments
/// - table (table)  : block table
/// - key   (string) : the key that is being set
/// - value (any)    : the value being stored
static int _block_newindex(lua_State * const L) {
    vx_assert(L != nullptr);
    LUAUTILS_ASSERT_ARGS_COUNT(L, 3)
    LUAUTILS_ASSERT_ARG_TYPE(L, 1, ITEM_TYPE_BLOCK)
    LUAUTILS_STACK_SIZE(L)
    
    const char *key = lua_tostring(L, 2);
    if (strcmp(key, LUA_BLOCK_FIELD_PALETTEINDEX) == 0) {
        if (lua_utils_isIntegerStrict(L, 3)) {
            block_C_handler *handler = _block_getCHandler(L, 1);
            vx_assert(handler != nullptr);
            block_C_handler_userdata *userdata = static_cast<block_C_handler_userdata *>(handler->userdata);

            userdata->luaPaletteIndex = lua_tointeger(L, 3);

            Shape *s = transform_utils_get_shape(static_cast<Transform *>(weakptr_get(userdata->parentShapeTransform)));
            if (s != nullptr) {
                SHAPE_COLOR_INDEX_INT_T luaPaletteIndex = lua_tointeger(L, 3);
                const SHAPE_COLOR_INDEX_INT_T entryIdx = color_palette_ordered_idx_to_entry_idx(
                    shape_get_palette(s), luaPaletteIndex - 1);

                shape_paint_block_as_transaction(s, entryIdx, userdata->x, userdata->y, userdata->z);
            }
        } else {
            LUAUTILS_ERROR_F(L, "Block.%s expects an integer", key)
        }
    } else if (strcmp(key, LUA_BLOCK_FIELD_COLOR) == 0) {
        RGBAColor color;
        if (lua_color_getRGBAL(L, 3, &color.r, &color.g, &color.b, &color.a, nullptr)) {
            block_C_handler *handler = _block_getCHandler(L, 1);
            vx_assert(handler != nullptr);
            block_C_handler_userdata *userdata = static_cast<block_C_handler_userdata *>(handler->userdata);

            userdata->color = color;

            Shape *s = transform_utils_get_shape(static_cast<Transform *>(weakptr_get(userdata->parentShapeTransform)));
            if (s != nullptr) {
                ColorPalette *palette = shape_get_palette(s);
                SHAPE_COLOR_INDEX_INT_T entryIdx;
                if (color_palette_check_and_add_color(palette, color, &entryIdx, false) == false) {
                    LUAUTILS_ERROR_F(L, "Block.%s - color palette is full", key)
                }
                shape_paint_block_as_transaction(s, entryIdx, userdata->x, userdata->y, userdata->z);
            }
        } else {
            LUAUTILS_ERROR_F(L, "Block.%s expects a Color", key)
        }
    } else {
        LUAUTILS_ERROR_F(L, "Block.%s is not settable", key)
    }
    
    LUAUTILS_STACK_SIZE_ASSERT(L, 0)
    return 0;
}

static void _block_gc(void *_ud) {
    block_userdata *ud = static_cast<block_userdata*>(_ud);
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

/// block:AddNeighbor(newBlock, face)
/// Arguments:
/// - Block (caller)
/// - Block, palette index, or Color
/// - Face
/// Returns: true if block added
static int _block_addneighbor(lua_State * const L) {
    vx_assert(L != nullptr);
    LUAUTILS_STACK_SIZE(L)
    
    const int argCount = lua_gettop(L);
    
    if (argCount < 1 || lua_utils_getObjectType(L, 1) != ITEM_TYPE_BLOCK) {
        LUAUTILS_ERROR(L, "Block:AddNeighbor - function should be called with `:`");
    }
    
    // validate arguments count
    if (argCount != 3) {
        LUAUTILS_ERROR(L, "Block:AddNeighbor - wrong number of arguments");
    }
    
    // validate 2nd argument:
    // - 2nd argument as palette index?
    const bool pIdxIsInteger = lua_utils_isIntegerStrict(L, 2);
    int luaPaletteIndex = -1;
    RGBAColor colorValue;
    
    int addType = -1; // 0: palette index, 1: block, 2: color
    if (pIdxIsInteger || lua_isnumber(L, 2)) {
        luaPaletteIndex = static_cast<SHAPE_COLOR_INDEX_INT_T>(pIdxIsInteger ?
                                                                lua_tointeger(L, 2) :
                                                                lua_tonumber(L, 2));
        addType = 0;
    }
    // - 2nd argument as Block?
    else if (lua_utils_getObjectType(L, 2) == ITEM_TYPE_BLOCK) {
        addType = 1;
    }
    // - 2nd argument as Color?
    else if (lua_color_getRGBAL(L, 2, &colorValue.r, &colorValue.g, &colorValue.b, &colorValue.a, nullptr)) {
        addType = 2;
    } else {
        LUAUTILS_ERROR(L, "MutableShape:AddBlock - 1st argument should be a palette index or a Color");
    }

    // validate 3rd argument (Face)
    if (lua_utils_getObjectType(L, 3) != ITEM_TYPE_FACE) {
        LUAUTILS_ERROR(L, "Block:AddNeighbor - 2nd argument should be a Face");
    }
    
    block_C_handler *handlerBlock1 = _block_getCHandler(L, 1);
    vx_assert(handlerBlock1 != nullptr);

    block_C_handler_userdata *_userdataBlock1 = static_cast<block_C_handler_userdata *>(handlerBlock1->userdata);
    Shape *block1ParentShape = transform_utils_get_shape(
        static_cast<Transform *>(weakptr_get(_userdataBlock1->parentShapeTransform)));

    if (block1ParentShape == nullptr) {
        LUAUTILS_ERROR(L, "Block:AddNeighbor - block has no parent Shape");
    }
    
    if (shape_is_lua_mutable(block1ParentShape) == false) {
        LUAUTILS_ERROR(L, "Block:AddNeighbor - block's parent Shape is read-only");
    }
    
    // Compute new block's coordinates
    const int touchedFace = lua_face_getFaceInt(L, 3);
    SHAPE_COORDS_INT_T newX = 0;
    SHAPE_COORDS_INT_T newY = 0;
    SHAPE_COORDS_INT_T newZ = 0;

    if (block_getNeighbourBlockCoordinates(_userdataBlock1->x,
                                           _userdataBlock1->y,
                                           _userdataBlock1->z,
                                           touchedFace,
                                           &newX,
                                           &newY,
                                           &newZ) == false) {
        LUAUTILS_ERROR(L, "Block:AddNeighbor - maximum coordinates reached");
    }

    if (addType == 1) {
        block_C_handler *handlerBlock2 = _block_getCHandler(L, 2);
        vx_assert(handlerBlock2 != nullptr);
        
        block_C_handler_userdata *_userdataBlock2 = static_cast<block_C_handler_userdata *>(handlerBlock2->userdata);

        // pushes false if it fails
        lua_block_addBlockInShapeAndPush(L,
                                        block1ParentShape,
                                        _userdataBlock2->luaPaletteIndex,
                                        newX, newY, newZ);
    } else {
        // if adding by color, add to palette and get lua index
        if (addType == 2) {
            ColorPalette *palette = shape_get_palette(block1ParentShape);
            SHAPE_COLOR_INDEX_INT_T entryIdx;
            color_palette_check_and_add_color(palette, colorValue, &entryIdx, false);

            const SHAPE_COLOR_INDEX_INT_T orderedIdx = color_palette_entry_idx_to_ordered_idx(palette, entryIdx);
            luaPaletteIndex = orderedIdx + 1;
        } else {
            if (luaPaletteIndex < 1 || luaPaletteIndex > color_palette_get_ordered_count(shape_get_palette(block1ParentShape))) {
                LUAUTILS_ERROR(L, "Block:AddNeighbor - palette index out of range");
            }
        }

        // pushes false on failure
        lua_block_addBlockInShapeAndPush(L, block1ParentShape, luaPaletteIndex, newX, newY, newZ);
    }
    
    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
    return 1;
}

/// block:Remove()
static int _block_remove(lua_State * const L) {
    vx_assert(L != nullptr);
    LUAUTILS_STACK_SIZE(L)
    
    const int argCount = lua_gettop(L);
    
    if (argCount < 1 || lua_utils_getObjectType(L, 1) != ITEM_TYPE_BLOCK) {
        LUAUTILS_ERROR(L, "Block:Remove - function should be called with `:`");
    }
    
    // validate arguments count
    if (argCount != 1) {
        LUAUTILS_ERROR(L, "block:Remove - wrong number of arguments");
    }
    
    block_C_handler *handlerBlock = _block_getCHandler(L, 1);
    if (handlerBlock == nullptr) {
        LUAUTILS_RETURN_INTERNAL_ERROR(L);
    }
    
    block_C_handler_userdata *userdataBlock = static_cast<block_C_handler_userdata *>(handlerBlock->userdata);
    Shape *parentShape = transform_utils_get_shape(
        static_cast<Transform *>(weakptr_get(userdataBlock->parentShapeTransform)));
    
    if (parentShape == nullptr) {
        LUAUTILS_ERROR(L, "Block:Remove - original shape no longer exists");
    }
    
    if (shape_is_lua_mutable(parentShape) == false) {
        LUAUTILS_ERROR(L, "Block:Remove - can't remove block from read-only Shape");
    }
    
    vx::Game *cppGame = nullptr;
    if (lua_utils_getGameCppWrapperPtr(L, &cppGame) == false || cppGame == nullptr) {
        LUAUTILS_RETURN_INTERNAL_ERROR(L);
    }
    
    shape_remove_block_as_transaction(parentShape,
                                game_get_scene(cppGame->getCGame()),
                                userdataBlock->x,
                                userdataBlock->y,
                                userdataBlock->z);
    
    // Maybe the block could not be removed because it's already been removed
    // from another reference with same coords on the same shape.
    // So it's ok, release the parent shape anyway.
    
    weakptr_release(userdataBlock->parentShapeTransform);
    userdataBlock->parentShapeTransform = nullptr;
    
    LUAUTILS_STACK_SIZE_ASSERT(L, 0)
    return 0;
}

/// block:Replace(Block newBlock)
/// block:Replace(integer newPaletteIndex)
/// block:Replace(Color newColor)
static int _block_replace(lua_State * const L) {
    vx_assert(L != nullptr);
    LUAUTILS_STACK_SIZE(L)
    
    const int argCount = lua_gettop(L);
    
    if (argCount < 1 || lua_utils_getObjectType(L, 1) != ITEM_TYPE_BLOCK) {
        LUAUTILS_ERROR(L, "Block:Replace - function should be called with `:`");
    }
    
    // validate arguments count
    if (argCount != 2) {
        LUAUTILS_ERROR(L, "Block:Replace - wrong number of arguments");
    }

    block_C_handler *handlerBlock1 = _block_getCHandler(L, 1);
    vx_assert(handlerBlock1 != nullptr);

    block_C_handler_userdata *userdata = static_cast<block_C_handler_userdata *>(handlerBlock1->userdata);
    Shape *s = transform_utils_get_shape(static_cast<Transform *>(weakptr_get(userdata->parentShapeTransform)));

    if (s == nullptr) {
        LUAUTILS_ERROR(L, "Block:Replace - original shape no longer exists");
    }

    if (shape_is_lua_mutable(s) == false) {
        LUAUTILS_ERROR(L, "Block:Replace - Shape is not mutable");
    }

    SHAPE_COLOR_INDEX_INT_T entryIdx = 0;
    const int type = lua_utils_getObjectType(L, 2);
    ColorPalette *palette = shape_get_palette(s);
    if (type == ITEM_TYPE_BLOCK) {
        SHAPE_COLOR_INDEX_INT_T luaPaletteIndex;
        lua_block_getInfo(L, 2, nullptr, nullptr, nullptr, nullptr, &luaPaletteIndex);

        if (luaPaletteIndex < 1 || luaPaletteIndex > color_palette_get_ordered_count(shape_get_palette(s))) {
            LUAUTILS_ERROR(L, "Block:Replace - palette index out of range");
        }

        entryIdx = color_palette_ordered_idx_to_entry_idx(palette, luaPaletteIndex - 1);
    } else if (type == ITEM_TYPE_COLOR) {
        RGBAColor rgba;
        if (!lua_color_getRGBAL(L, 2, &rgba.r, &rgba.g, &rgba.b, &rgba.a, nullptr)) {
            LUAUTILS_INTERNAL_ERROR(L);
        }

        if (color_palette_check_and_add_color(palette, rgba, &entryIdx, false) == false) {
            LUAUTILS_ERROR(L, "Block:Replace - could not add color to shape palette (is it full?)");
        }
    } else if (lua_utils_isIntegerStrict(L, 2)) {
        SHAPE_COLOR_INDEX_INT_T luaPaletteIndex = static_cast<SHAPE_COLOR_INDEX_INT_T>(lua_tointeger(L, 2));

        if (luaPaletteIndex < 1 || luaPaletteIndex > color_palette_get_ordered_count(shape_get_palette(s))) {
            LUAUTILS_ERROR(L, "Block:Replace - palette index out of range");
        }

        entryIdx = color_palette_ordered_idx_to_entry_idx(palette, luaPaletteIndex - 1);
    } else {
        LUAUTILS_ERROR(L, "Block:Replace - argument should be a Block, Color, or palette index (integer)");
    }

    shape_paint_block_as_transaction(s, entryIdx, userdata->x, userdata->y, userdata->z);
    
    LUAUTILS_STACK_SIZE_ASSERT(L, 0)
    return 0;
}

SHAPE_COLOR_INDEX_INT_T _block_get_or_refresh_palette_index(Shape *s, const block_C_handler_userdata *userdata) {
    SHAPE_COLOR_INDEX_INT_T result = userdata->luaPaletteIndex;

    if (s == nullptr) {
        s = transform_utils_get_shape(static_cast<Transform *>(weakptr_get(userdata->parentShapeTransform)));
    }
    if (s != nullptr) {
        const Block *b = shape_get_block(s, userdata->x, userdata->y, userdata->z);
        if (b != nullptr) {
            const SHAPE_COLOR_INDEX_INT_T orderedIdx = color_palette_entry_idx_to_ordered_idx(
                shape_get_palette(s), b->colorIndex);
            result = orderedIdx + 1;
        }
    }

    return result;
}

bool _block_pushBlock(lua_State * const L, block_C_handler_userdata *userdata) {
    vx_assert(L != nullptr);
    vx_assert(userdata != nullptr);
    LUAUTILS_STACK_SIZE(L)

    vx::Game *g = nullptr;
    if (lua_utils_getGameCppWrapperPtr(L, &g) == false) {
        LUAUTILS_INTERNAL_ERROR(L);
    }

    block_C_handler *handler = static_cast<block_C_handler*>(malloc(sizeof(block_C_handler)));
    handler->free_userdata = _free_block_handler;
    handler->userdata = userdata;

    block_userdata *ud = static_cast<block_userdata *>(lua_newuserdatadtor(L, sizeof(block_userdata), _block_gc));
    ud->handler = handler;

    // connect to userdata store
    ud->store = g->userdataStoreForBlocks;
    ud->previous = nullptr;
    block_userdata* next = static_cast<block_userdata*>(ud->store->add(ud));
    ud->next = next;
    if (next != nullptr) {
        next->previous = ud;
    }

    lua_newtable(L); // metatable
    {
        lua_pushstring(L, "__index");
        lua_pushcfunction(L, _block_index, "");
        lua_rawset(L, -3);

        lua_pushstring(L, "__newindex");
        lua_pushcfunction(L, _block_newindex, "");
        lua_rawset(L, -3);

        lua_pushstring(L, "__metatable");
        lua_pushboolean(L, false);
        lua_rawset(L, -3);

        lua_pushstring(L, "__metatable");
        lua_pushboolean(L, false);
        lua_rawset(L, -3);

        lua_pushstring(L, "__type");
        lua_pushstring(L, "Block");
        lua_settable(L, -3);

        lua_pushstring(L, "__typen");
        lua_pushinteger(L, ITEM_TYPE_BLOCK);
        lua_settable(L, -3);
    }
    lua_setmetatable(L, -2); // metatable is popped here // + 2

    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
    return true;
}

bool _block_pushBlockWithIndex(lua_State * const L,
                               Shape *parentShape,
                               const SHAPE_COLOR_INDEX_INT_T luaPaletteIndex,
                               const SHAPE_COORDS_INT_T *x,
                               const SHAPE_COORDS_INT_T *y,
                               const SHAPE_COORDS_INT_T *z) {

    block_C_handler_userdata *userdata = static_cast<block_C_handler_userdata*>(malloc(sizeof(block_C_handler_userdata)));

    if (parentShape == nullptr) {
        userdata->parentShapeTransform = nullptr;
    } else {
        userdata->parentShapeTransform = transform_get_and_retain_weakptr(shape_get_transform(parentShape));
    }
    userdata->x = *x;
    userdata->y = *y;
    userdata->z = *z;
    userdata->luaPaletteIndex = luaPaletteIndex;
    userdata->color = { 0, 0, 0, 0 };

    return _block_pushBlock(L, userdata);
}

bool _block_pushBlockWithColor(lua_State * const L,
                               RGBAColor color,
                               const SHAPE_COORDS_INT_T *x,
                               const SHAPE_COORDS_INT_T *y,
                               const SHAPE_COORDS_INT_T *z) {

    block_C_handler_userdata *userdata = static_cast<block_C_handler_userdata*>(malloc(sizeof(block_C_handler_userdata)));

    userdata->parentShapeTransform = nullptr;
    userdata->x = *x;
    userdata->y = *y;
    userdata->z = *z;
    userdata->luaPaletteIndex = SHAPE_COLOR_INDEX_AIR_BLOCK;
    userdata->color = color;

    return _block_pushBlock(L, userdata);
}

void _free_block_handler(void *userdata) {
    block_C_handler_userdata *_userdata = static_cast<block_C_handler_userdata *>(userdata);
    if (_userdata->parentShapeTransform != nullptr) {
        weakptr_release(_userdata->parentShapeTransform);
        _userdata->parentShapeTransform = nullptr;
    }
    free(_userdata);
}

block_C_handler *_block_getCHandler(lua_State * const L, const int idx) {
    LUAUTILS_ASSERT_ARG_TYPE(L, idx, ITEM_TYPE_BLOCK)
    block_userdata *ud = static_cast<block_userdata*>(lua_touserdata(L, idx));
    return ud->handler;
}
