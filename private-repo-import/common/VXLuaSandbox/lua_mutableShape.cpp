//
//  lua_mutableShape.cpp
//  Cubzh
//
//  Created by Gaetan de Villele on 23/02/2021.
//

#include "lua_mutableShape.hpp"

// C++
#include <algorithm>
#include <string>

// Blip
#include "VXHubClient.hpp"
#include "VXApplication.hpp"

// xptools
#include "xptools.h"

// engine
#include "cache.h"
#include "game.h"
#include "serialization.h"
#include "serialization_vox.h"
#include "lua_number3.hpp"

// Lua
#include "lua.hpp"
#include "lua_utils.hpp"
#include "lua_constants.h"
#include "lua_items.hpp"
#include "lua_shape.hpp"
#include "lua_block.hpp"
#include "lua_object.hpp"
#include "lua_data.hpp"
#include "lua_color.hpp"

// MutableShape instance fields
#define LUA_MUTABLESHAPE_FIELD_ADDBLOCK    "AddBlock"    // function (read-only)

#define LUA_SHAPE_FIELD_HISTORY         "History"       // boolean (read/write)
#define LUA_SHAPE_FIELD_UNDO            "Undo"          // function (read-only)
#define LUA_SHAPE_FIELD_REDO            "Redo"          // function (read-only)
#define LUA_SHAPE_FIELD_CANUNDO         "CanUndo"       // boolean (read-only)
#define LUA_SHAPE_FIELD_CANREDO         "CanRedo"       // boolean (read-only)

#define LUA_SHAPE_FIELD_KEEP_HISTORY_TRANSACTION_PENDING "KeepHistoryTransactionPending" // bool (read/write)

// --------------------------------------------------
//
// MARK: - Static functions prototypes -
//
// --------------------------------------------------

// For MutableShape global table

static int _g_mutableShape_metatable_index(lua_State *L);
static int _g_mutableShape_metatable_newindex(lua_State *L);

/// Creates a new MutableShape instance.
/// Lua function receives the following arguments:
/// - 1 - MutableShape global table
/// - 2 - Resource
/// @param L Lua state
/// @return Number of Lua return values
static int _g_mutableShape_metatable_call(lua_State *L);

// For MutableShape instance tables

static int _mutableShape_instance_metatable_index(lua_State *L);
static int _mutableShape_instance_metatable_newindex(lua_State *L);
static int _mutableShape_addBlock(lua_State *L);
static int _mutableShape_lua_capture(lua_State *L);
static int _mutableShape_undo(lua_State *L);
static int _mutableShape_redo(lua_State *L);

// --------------------------------------------------
//
// MARK: - Exposed functions -
//
// --------------------------------------------------

void lua_g_mutableShape_createAndPush(lua_State * const L) {
    vx_assert(L != nullptr);
    LUAUTILS_STACK_SIZE(L)
    
    lua_newuserdata(L, 1); // create MutableShape global table
    lua_newtable(L); // metatable
    {
        lua_pushstring(L, "__index");
        lua_pushcfunction(L, _g_mutableShape_metatable_index, "");
        lua_settable(L, -3);
        
        lua_pushstring(L, "__newindex");
        lua_pushcfunction(L, _g_mutableShape_metatable_newindex, "");
        lua_settable(L, -3);
        
        lua_pushstring(L, "__call");
        lua_pushcfunction(L, _g_mutableShape_metatable_call, "");
        lua_settable(L, -3);
        
        // hide the metatable from the Lua sandbox user
        lua_pushstring(L, "__metatable");
        lua_pushboolean(L, false);
        lua_settable(L, -3);
        
        lua_pushstring(L, "__type");
        lua_pushstring(L, "MutableShapeInterface");
        lua_settable(L, -3);

        lua_pushstring(L, "__typen");
        lua_pushinteger(L, ITEM_TYPE_G_MUTABLESHAPE);
        lua_settable(L, -3);
    }
    lua_setmetatable(L, -2);

    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
}

int lua_g_mutableShape_snprintf(lua_State *L, char *result, size_t maxLen, bool spacePrefix) {
    RETURN_VALUE_IF_NULL(L, 0)
    return snprintf(result, maxLen, "%s[MutableShape] (global)", spacePrefix ? " " : "");
}

bool lua_mutableShape_pushNewInstance(lua_State *L, Shape *s) {
    vx_assert(L != nullptr);
    vx_assert(s != nullptr);
    vx_assert(shape_is_lua_mutable(s));

    Transform *t = shape_get_transform(s);

    // lua MutableShape takes ownership
    if (transform_retain(t) == false) {
        LUAUTILS_INTERNAL_ERROR(L);
        // return false;
    }
    
    LUAUTILS_STACK_SIZE(L)

    vx::Game *g = nullptr;
    if (lua_utils_getGameCppWrapperPtr(L, &g) == false) {
        LUAUTILS_INTERNAL_ERROR(L);
    }

    shape_userdata *ud = static_cast<shape_userdata*>(lua_newuserdatadtor(L, sizeof(shape_userdata), lua_shape_gc));
    ud->shape = s;

    // connect to userdata store
    ud->store = g->userdataStoreForShapes;
    ud->previous = nullptr;
    shape_userdata* next = static_cast<shape_userdata*>(ud->store->add(ud));
    ud->next = next;
    if (next != nullptr) {
        next->previous = ud;
    }

    lua_newtable(L); // metatable
    {
        lua_pushstring(L, "__index");
        lua_pushcfunction(L, _mutableShape_instance_metatable_index, "");
        lua_settable(L, -3);
        
        lua_pushstring(L, "__newindex");
        lua_pushcfunction(L, _mutableShape_instance_metatable_newindex, "");
        lua_settable(L, -3);
        
        // hide the metatable from the Lua sandbox user
        lua_pushstring(L, "__metatable");
        lua_pushboolean(L, false);
        lua_settable(L, -3);
        
        lua_pushstring(L, "__type");
        lua_pushstring(L, "MutableShape");
        lua_settable(L, -3);

        lua_pushstring(L, "__typen");
        lua_pushinteger(L, ITEM_TYPE_MUTABLESHAPE);
        lua_settable(L, -3);
        
        // private fields
        _lua_shape_push_fields(L, s); // TODO: store in registry when accessed
        _lua_mutableShape_push_fields(L, s); // TODO: store in registry when accessed
    }
    lua_setmetatable(L, -2);
    
    if (lua_g_object_addIndexEntry(L, t, -1) == false) {
        vxlog_error("Failed to add Lua MutableShape to Object registry");
        lua_pop(L, 1); // pop Lua MutableShape
        LUAUTILS_STACK_SIZE_ASSERT(L, 0)
        return false;
    }

    scene_register_managed_transform(game_get_scene(g->getCGame()), t);

    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
    return true;
}

bool lua_mutableShape_isMutableShape(lua_State *L, const int idx) {
    vx_assert(L != nullptr);
    return lua_mutableShape_isMutableShapeType(lua_utils_getObjectType(L, idx));
}

bool lua_mutableShape_isMutableShapeType(const int type) {
    return type == ITEM_TYPE_MUTABLESHAPE || type == ITEM_TYPE_MAP;
}

int lua_mutableShape_snprintf(lua_State *L, char *result, size_t maxLen, bool spacePrefix) {
    RETURN_VALUE_IF_NULL(L, 0)
    Transform *root; lua_object_getTransformPtr(L, -1, &root);
    if (root == nullptr) {
        return snprintf(result, maxLen, "%s[MutableShape (destroyed)]", spacePrefix ? " " : "");
    }
    return snprintf(result, maxLen, "%s[MutableShape %d Name:%s] (instance)", spacePrefix ? " " : "",
                    transform_get_id(root), transform_get_name(root));
}

// MARK: - Exposed Internal Functions -

void _lua_mutableShape_push_fields(lua_State *L, Shape *s) {
    // nothing currently
}

void _lua_mutableShape_update_fields(lua_State *L, Shape *s) {
    // nothing currently
}

bool _lua_mutableShape_metatable_index(lua_State *L, Shape *s, const char *key) {
    if (strcmp(key, LUA_MUTABLESHAPE_FIELD_ADDBLOCK) == 0) {
        lua_pushcfunction(L, _mutableShape_addBlock, "");
        return true;
    } else if (strcmp(key, LUA_SHAPE_FIELD_HISTORY) == 0) {
        lua_pushboolean(L, shape_history_getEnabled(s));
        return true;
    } else if (strcmp(key, LUA_SHAPE_FIELD_KEEP_HISTORY_TRANSACTION_PENDING) == 0) {
        lua_pushboolean(L, shape_history_getKeepTransactionPending(s));
        return true;
    } else if (strcmp(key, LUA_SHAPE_FIELD_UNDO) == 0) {
        lua_pushcfunction(L, _mutableShape_undo, "");
        return true;
    } else if (strcmp(key, LUA_SHAPE_FIELD_REDO) == 0) {
        lua_pushcfunction(L, _mutableShape_redo, "");
        return true;
    } else if (strcmp(key, LUA_SHAPE_FIELD_CANUNDO) == 0) {
        lua_pushboolean(L, shape_history_canUndo(s));
        return true;
    } else if (strcmp(key, LUA_SHAPE_FIELD_CANREDO) == 0) {
        lua_pushboolean(L, shape_history_canRedo(s));
        return true;
    } else {
        // do not push anything
        return false;
    }
}

/// returns true if a key has been set without error
/// false if an uppercase-starting key has not been found
bool _lua_mutableShape_metatable_newindex(lua_State *L, Shape *s, const char *key) {
    if (strcmp(key, LUA_SHAPE_FIELD_HISTORY) == 0) {
        if (lua_isboolean(L, 3)) {
            const bool historyEnabled = lua_toboolean(L, 3);
            shape_history_setEnabled(s, historyEnabled);
            return true;
        } else {
            LUAUTILS_ERROR_F(L, "MutableShape.%s cannot be set (new value is not a boolean)", key);
        }
    } else if (strcmp(key, LUA_SHAPE_FIELD_KEEP_HISTORY_TRANSACTION_PENDING) == 0) {
        if (lua_isboolean(L, 3)) {
            const bool keep = lua_toboolean(L, 3);
            shape_history_setKeepTransactionPending(s, keep);
            return true;
        } else {
            LUAUTILS_ERROR_F(L, "MutableShape.%s cannot be set (new value is not a boolean)", key);
        }
    }
    
    return false; // nothing was set
}

// --------------------------------------------------
//
// MARK: - Static functions implementation
//
// --------------------------------------------------

static int _g_mutableShape_metatable_index(lua_State *L) {
    RETURN_VALUE_IF_NULL(L, 0);
    LUAUTILS_STACK_SIZE(L)
    lua_pushnil(L);
    LUAUTILS_STACK_SIZE_ASSERT(L, 1);
    return 1;
}

static int _g_mutableShape_metatable_newindex(lua_State *L) {
    RETURN_VALUE_IF_NULL(L, 0);
    LUAUTILS_STACK_SIZE(L)
    // nothing
    LUAUTILS_STACK_SIZE_ASSERT(L, 0);
    return 0;
}

/// Arguments:
/// 1: MutableShape global table
/// 2: Item, Shape or MutableShape (optional, empty MutableShape created when not provided)
/// 3: config table (optional) - default { bakedLight = false, includeChildren = false }
static int _g_mutableShape_metatable_call(lua_State *L) {
    RETURN_VALUE_IF_NULL(L, 0)
    LUAUTILS_STACK_SIZE(L)
    
    bool bakedLight = false;
    bool includeChildren = false; // used when copying shape
    
    const char* itemName = nullptr;
    Shape *itemShape = nullptr;
    
    const int argCount = lua_gettop(L);
    
    if (argCount > 3) {
        LUAUTILS_ERROR(L, "MutableShape() - too many arguments");
    }
    
    if (argCount == 3 && lua_isnil(L, 3) == false) { // config provided
        if (lua_istable(L, 3) == false) {
            LUAUTILS_STACK_SIZE_ASSERT(L, 0);
            LUAUTILS_ERROR(L, "MutableShape() - config argument must be a table")
            return 0;
        }
        
        int type = lua_getfield(L, 3, "bakedLight");
        if (type == LUA_TNIL) {
            lua_pop(L, 1); // pop nil
        } else if (type == LUA_TBOOLEAN) {
            bakedLight = lua_toboolean(L, -1);
            lua_pop(L, 1);
        } else {
            lua_pop(L, 1);
            LUAUTILS_STACK_SIZE_ASSERT(L, 0);
            LUAUTILS_ERROR(L, "MutableShape() - config.bakedLight must be a boolean")
            return 0;
        }

        type = lua_getfield(L, 3, "recurse");
        if (type == LUA_TNIL) {
            lua_pop(L, 1); // pop nil
        } else if (type == LUA_TBOOLEAN) {
            includeChildren = lua_toboolean(L, -1);
            lua_pop(L, 1);
        } else {
            lua_pop(L, 1);
            LUAUTILS_STACK_SIZE_ASSERT(L, 0);
            LUAUTILS_ERROR(L, "MutableShape() - config.recurse must be a boolean")
            return 0;
        }

        // DEPRECATED, USE `recurse` INSTEAD
        type = lua_getfield(L, 3, "includeChildren");
        if (type == LUA_TNIL) {
            lua_pop(L, 1); // pop nil
        } else if (type == LUA_TBOOLEAN) {
            includeChildren = lua_toboolean(L, -1);
            lua_pop(L, 1);
        } else {
            lua_pop(L, 1);
            LUAUTILS_STACK_SIZE_ASSERT(L, 0);
            LUAUTILS_ERROR(L, "MutableShape() - config.includeChildren must be a boolean")
            return 0;
        }
    }
    
    CGame *g = nullptr;
    if (lua_utils_getGamePtr(L, &g) == false || g == nullptr) {
        LUAUTILS_INTERNAL_ERROR(L) // returns
    }

    ShapeSettings shapeSettings;
    shapeSettings.isMutable = true;
    shapeSettings.lighting = bakedLight;

    if (lua_isuserdata(L, 2) || lua_istable(L, 2)) {
        int type = lua_utils_getObjectType(L, 2);
        if (type == ITEM_TYPE_ITEM) {

            itemName = lua_item_getName(L, 2);
            if (itemName == nullptr) {
                LUAUTILS_ERROR(L, "MutableShape() - failed to resolve item name");
            }

        } else if (lua_shape_isShapeType(type)) {

            Transform *self; lua_object_getTransformPtr(L, 2, &self);
            Transform *copy = includeChildren ? transform_utils_copy_recurse(self) : transform_new_copy(self);
            Shape *s = static_cast<Shape*>(transform_get_ptr(copy));
            shape_set_lua_mutable(s, true);
            lua_mutableShape_pushNewInstance(L, s);
            transform_release(copy); // owned by lua MutableShape

            LUAUTILS_STACK_SIZE_ASSERT(L, 1)
            return 1;

        } else if (type == ITEM_TYPE_DATA) {

            size_t len = 0;
            const char *bytes = static_cast<const char*>(lua_data_getBuffer(L, 2, &len));
            
            CGame *g = nullptr;
            if (lua_utils_getGamePtr(L, &g) == false || g == nullptr) {
                LUAUTILS_INTERNAL_ERROR(L) // returns
            }

            // try to load native format
            Stream *s = stream_new_buffer_read(bytes, len);
            itemShape = serialization_load_shape(s,
                                                 nullptr,
                                                 game_get_color_atlas(g),
                                                 &shapeSettings,
                                                 true);

            // try to load as .vox if it didn't work
            if (itemShape == nullptr) {
                s = stream_new_buffer_read(bytes, len);

                enum serialization_vox_error err = serialization_vox_load(s,
                                                                          &itemShape,
                                                                          true,
                                                                          game_get_color_atlas(g));
                stream_free(s);
                if (err != no_error) {
                    itemShape = nullptr;
                }
            }
        } else {
            LUAUTILS_ERROR(L, "MutableShape() - wrong type of argument")
        }
    } else if (lua_isstring(L, 2)) {

        // NOTE: we could accept string parameters,
        // BUT, they have to link to Items loaded in the current sandbox.
        // Otherwise, it would allow to open things from the cache,
        // loaded by experiences.
        // That's why we don't support it for now.
        LUAUTILS_ERROR(L, "MutableShape() - string argument not yet supported");

    } else { // no resource arg, create empty mutable shape
        itemShape = shape_new_2(true);
        shape_set_palette(itemShape, color_palette_new(game_get_color_atlas(g)), false);
    }

    // loading from file
    if (itemName != nullptr) {

        FILE *fd = nullptr;

        // Check whether wanted file is present in cache,
        // if it isn't we also look in the app bundle.
        if (cache_isFileInCache(itemName, nullptr)) {

            // file exists in cache
            fd = cache_fopen(itemName, "rb");

        } else {

            // file not found in cache, checking in bundle...
            char* sanitized_path = cache_ensure_path_format(itemName, false);
            const std::string bundleRelPath = std::string("cache") + vx::fs::getPathSeparatorStr() + std::string(sanitized_path);
            free(sanitized_path);
            bool isDir = false;
            const bool exists = c_bundleFileExists(bundleRelPath.c_str(), &isDir);
            if (exists == false || isDir == true) {
                // not found
                LUAUTILS_ERROR_F(L, "MutableShape() - file not found %s", itemName);
            } else {
                fd = vx::fs::openBundleFile(bundleRelPath, "rb");
            }
        }

        if (fd == nullptr) {
            LUAUTILS_ERROR(L, "MutableShape() - internal error");
        }

        // create shape from file

        // The file descriptor is owned by the stream, which will fclose it in the future.
        Stream *s = stream_new_file_read(fd); 
        itemShape = serialization_load_shape(s, // frees stream, closing fd
                                             itemName,
                                             game_get_color_atlas(g),
                                             &shapeSettings,
                                             false);
    }
    
    if (itemShape == nullptr) {
        LUAUTILS_ERROR(L, "MutableShape() - MutableShape could not be created");
    }

    if (shape_is_lua_mutable(itemShape) == false) {
        LUAUTILS_INTERNAL_ERROR(L)
    }

    // if resulting shape didn't have serialized lighting data, compute it
    if (shapeSettings.lighting && shape_uses_baked_lighting(itemShape) == false) {
        _lua_shape_load_or_compute_baked_light(itemShape);
    }

    // Create a Lua Shape instance table
    if (lua_mutableShape_pushNewInstance(L, itemShape) == false) {
        lua_pushnil(L); // GC will release shape
    }
    shape_release(itemShape); // now owned by lua MutableShape
    
    LUAUTILS_STACK_SIZE_ASSERT(L, 1);
    return 1;
}

static int _mutableShape_instance_metatable_index(lua_State *L) {
    vx_assert(L != nullptr);
    LUAUTILS_ASSERT_ARGS_COUNT(L, 2)
    LUAUTILS_ASSERT_ARG_TYPE(L, 1, ITEM_TYPE_MUTABLESHAPE)
    LUAUTILS_STACK_SIZE(L)

    const char *key = lua_tostring(L, 2);
    if (_lua_object_metatable_index(L, key) == false) {
        // no field found in Object
        // retrieve underlying Shape struct
        Shape *s = lua_shape_getShapePtr(L, 1);
        if (s == nullptr) {
            lua_pushnil(L);
        } else if (_lua_shape_metatable_index(L, s, key)) {
            // a field from Shape was pushed onto the stack
        } else if (_lua_mutableShape_metatable_index(L, s, key) == false) {
            // key is not known and starts with an uppercase letter
            // LUAUTILS_ERROR_F(L, "MutableShape: %s field does not exist", key);
            lua_pushnil(L);
        }
    }
    
    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
    return 1;
}

static int _mutableShape_instance_metatable_newindex(lua_State *L) {
    RETURN_VALUE_IF_NULL(L, 0);
    LUAUTILS_STACK_SIZE(L)
    
    // retrieve underlying Shape struct
    Shape *s = lua_shape_getShapePtr(L, 1);
    if (s == nullptr) {
        lua_pushnil(L);
        LUAUTILS_STACK_SIZE_ASSERT(L, 1)
        return 1;
    }
    
    Transform *t = shape_get_transform(s);
    if (t == nullptr) {
        lua_pushnil(L);
        LUAUTILS_STACK_SIZE_ASSERT(L, 1)
        return 1;
    }
    
    const char *key = lua_tostring(L, 2);
    
    if (_lua_object_metatable_newindex(L, t, key) == false) {
        // the key is not from Object
        if (_lua_shape_metatable_newindex(L, s, key) == false) {
            // the key is not from Shape
            if (_lua_mutableShape_metatable_newindex(L, s, key) == false) {
                // key not found
                LUAUTILS_ERROR_F(L, "MutableShape: %s field is not settable", key);
            }
        }
    }
    
    LUAUTILS_STACK_SIZE_ASSERT(L, 0);
    return 0;
}

/// arguments:
/// option1: (Block)
/// option2: (paletteIndex (integer), Number3)
/// option3: (paletteIndex (integer), x (number), y(number), z(number))
/// option4: (color (Color), Number3)
/// option5: (color (Color), x (number), y(number), z(number))
static int _mutableShape_addBlock(lua_State *L) {
    vx_assert(L != NULL);
    LUAUTILS_STACK_SIZE(L)
    
    const int argCount = lua_gettop(L);
    
    if (argCount < 1 || lua_mutableShape_isMutableShape(L, 1) == false) {
        LUAUTILS_ERROR(L, "MutableShape:AddBlock - function should be called with `:`");
    }

    if (argCount < 2 || argCount > 5) {
        LUAUTILS_ERROR(L, "MutableShape:AddBlock - wrong number of arguments");
    }
    
    SHAPE_COORDS_INT_T x = 0;
    SHAPE_COORDS_INT_T y = 0;
    SHAPE_COORDS_INT_T z = 0;
    SHAPE_COLOR_INDEX_INT_T luaPaletteIndex = 0;
    RGBAColor colorValue;
    bool addByColor = false;
    Shape *sh = lua_shape_getShapePtr(L, 1);

    if (argCount == 2) {
        // 1st argument as Block
        if (lua_utils_getObjectType(L, 2) != ITEM_TYPE_BLOCK) {
            LUAUTILS_ERROR(L, "MutableShape:AddBlock - 1st argument should be a Block");
        }

        // retrieves color from palette or from value stored in Block
        lua_block_getInfo(L, 2, nullptr, &x, &y, &z, nullptr, &colorValue);
        addByColor = true;
        
    } else {
        // 1st argument as palette index
        const bool pIdxIsInteger = lua_utils_isIntegerStrict(L, 2);
        if ((pIdxIsInteger || lua_isnumber(L, 2))) {
            luaPaletteIndex = static_cast<SHAPE_COLOR_INDEX_INT_T>(pIdxIsInteger ?
                                                                   lua_tointeger(L, 2) :
                                                                   lua_tonumber(L, 2));

            if (luaPaletteIndex < 1 || luaPaletteIndex > color_palette_get_ordered_count(shape_get_palette(sh))) {
                LUAUTILS_ERROR(L, "MutableShape:AddBlock - palette index out of range");
            }
        }
        // 1st argument as Color
        else {
            if (!lua_color_getRGBAL(L, 2, &colorValue.r, &colorValue.g, &colorValue.b, &colorValue.a, nullptr)) {
                LUAUTILS_ERROR(L, "MutableShape:AddBlock - 1st argument should be a palette index or a Color");
            }

            addByColor = true;
        }

        // 2nd argument as Number3 or numbers table
        float fx, fy, fz;
        if (argCount == 3) {
            if (lua_number3_or_table_getXYZ(L, 3, &fx, &fy, &fz) == false) {
                LUAUTILS_ERROR(L, "MutableShape:AddBlock - 2nd argument should be a Number3");
            }

            fx = std::floor(fx);
            fy = std::floor(fy);
            fz = std::floor(fz);
        }
        // 2nd to 5th arguments as numbers
        else if (argCount == 5) {
            if (lua_isnumber(L, 3) == false) {
                LUAUTILS_ERROR(L, "MutableShape:AddBlock - 2nd argument should be a number");
            }
            if (lua_isnumber(L, 4) == false) {
                LUAUTILS_ERROR(L, "MutableShape:AddBlock - 3rd argument should be a number");
            }
            if (lua_isnumber(L, 5) == false) {
                LUAUTILS_ERROR(L, "MutableShape:AddBlock - 4th argument should be a number");
            }

            fx = std::floor(lua_tonumber(L, 3));
            fy = std::floor(lua_tonumber(L, 4));
            fz = std::floor(lua_tonumber(L, 5));
        } else {
            LUAUTILS_ERROR(L, "MutableShape:AddBlock - coordinates should be a Number3, a table of 3 numbers, or 3 individual numbers");
            return 0; // silence warnings
        }

        if (fx < SHAPE_COORDS_MIN || fx > SHAPE_COORDS_MAX ||
            fy < SHAPE_COORDS_MIN || fy > SHAPE_COORDS_MAX ||
            fz < SHAPE_COORDS_MIN || fz > SHAPE_COORDS_MAX) {
            LUAUTILS_ERROR(L, "MutableShape:AddBlock - maximum coordinates reached");
        }

        x = static_cast<SHAPE_COORDS_INT_T>(fx);
        y = static_cast<SHAPE_COORDS_INT_T>(fy);
        z = static_cast<SHAPE_COORDS_INT_T>(fz);
    }

    // if adding by color, add to palette and get lua index
    if (addByColor) {
        ColorPalette *palette = shape_get_palette(sh);
        SHAPE_COLOR_INDEX_INT_T entryIdx;
        if (color_palette_check_and_add_color(palette, colorValue, &entryIdx, false) == false) {
            LUAUTILS_ERROR(L, "MutableShape:AddBlock - color palette is full");
        }

        const SHAPE_COLOR_INDEX_INT_T orderedIdx = color_palette_entry_idx_to_ordered_idx(palette, entryIdx);
        luaPaletteIndex = orderedIdx + 1;
    }

    // pushes false on failure
    lua_block_addBlockInShapeAndPush(L, sh, luaPaletteIndex, x, y, z);
    
    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
    return 1;
}

// myShape:Undo()
// arguments:
// 1 - Shape: function caller
// return values: none
static int _mutableShape_undo(lua_State *L) {
    vx_assert(L != nullptr);
    LUAUTILS_STACK_SIZE(L)
    
    const int argCount = lua_gettop(L);
    
    if (argCount < 1 || lua_mutableShape_isMutableShape(L, 1) == false) {
        LUAUTILS_ERROR(L, "MutableShape:Undo - function should be called with `:`");
    }
    
    if (argCount > 1) {
        LUAUTILS_ERROR(L, "MutableShape:Undo - wrong number of arguments");
    }
    
    Shape *shape = lua_shape_getShapePtr(L, 1);
    if (shape != nullptr && shape_history_canUndo(shape) == true) {
        shape_history_undo(shape);
    }
    
    LUAUTILS_STACK_SIZE_ASSERT(L, 0)
    return 0;
}

// myShape:Undo()
// arguments:
// 1 - Shape: function caller
// return values: none
static int _mutableShape_redo(lua_State *L) {
    vx_assert(L != nullptr);
    LUAUTILS_STACK_SIZE(L)
    
    const int argCount = lua_gettop(L);
    
    if (argCount < 1 || lua_mutableShape_isMutableShape(L, 1) == false) {
        LUAUTILS_ERROR(L, "MutableShape:Redo - function should be called with `:`");
    }
    
    if (argCount > 1) {
        LUAUTILS_ERROR(L, "MutableShape:Redo - wrong number of arguments");
    }
    
    Shape *shape = lua_shape_getShapePtr(L, 1);
    if (shape != nullptr && shape_history_canRedo(shape) == true) {
        shape_history_redo(shape);
    }
    
    LUAUTILS_STACK_SIZE_ASSERT(L, 0)
    return 0;
}
