//
//  lua_shape.cpp
//  Cubzh
//
//  Created by Gaetan de Villele on 06/08/2020.
//

#include "lua_shape.hpp"

// C++
#include <cmath>

// Lua
#include "lua.hpp"

// sandbox
#include "lua_block.hpp"
#include "lua_palette.hpp"
#include "lua_number3.hpp"
#include "lua_rotation.hpp"
#include "lua_items.hpp"
#include "lua_constants.h"
#include "lua_utils.hpp"
#include "lua_object.hpp"
#include "lua_point.hpp"
#include "lua_box.hpp"
#include "lua_mutableShape.hpp"
#include "lua_data.hpp"
#include "lua_logs.hpp"
#include "lua_palette.hpp"
#include "lua_camera.hpp"
#include "lua_color.hpp"

// engine
#include "cache.h"
#include "game.h"
#include "serialization_vox.h"
#include "serialization.h"
#include "shape.h"
#include "scene.h"

#include "VXApplication.hpp"
#include "VXGameRenderer.hpp"
#include "VXGame.hpp"
#include "VXResource.hpp"
#include "VXHubClient.hpp"
#include "fileutils.h"

// xptools
#include "xptools.h"
#include "OperationQueue.hpp"

// Shape class fields
#define LUA_G_SHAPE_FIELD_LOAD              "Load"           // function

// Shape class metafields
#define LUA_G_SHAPE_METAFIELD_BAKECALLBACKS "bake_callbacks" // table

// Shape instance properties
// NOTES:
// - BoundingBox returns the model space AABB
// - Width/Height/Depth are the dimensions of the model space AABB
// - Min/Max/Center are shortcuts to BoundingBox points
#define LUA_SHAPE_FIELD_PIVOT           "Pivot"           // Number3
#define LUA_SHAPE_FIELD_MIN             "Min"             // Number3 (read-only)
#define LUA_SHAPE_FIELD_CENTER          "Center"          // Number3 (read-only)
#define LUA_SHAPE_FIELD_MAX             "Max"             // Number3 (read-only)
#define LUA_SHAPE_FIELD_SIZE            "Size"            // Number3 (read-only)
#define LUA_SHAPE_FIELD_WIDTH           "Width"           // number (read-only)
#define LUA_SHAPE_FIELD_HEIGHT          "Height"          // number (read-only)
#define LUA_SHAPE_FIELD_DEPTH           "Depth"           // number (read-only)
#define LUA_SHAPE_FIELD_BOUNDINGBOX     "BoundingBox"     // Box (read-only)
#define LUA_SHAPE_FIELD_SHADOW          "Shadow"          // boolean
#define LUA_SHAPE_FIELD_UNLIT           "IsUnlit"         // boolean
#define LUA_SHAPE_FIELD_COUNT           "BlocksCount"     // integer (read-only)
#define LUA_SHAPE_FIELD_LAYERS          "Layers"
#define LUA_SHAPE_FIELD_LOCAL_PALETTE   "LocalPalette"    // LocalPalette (DEPRECATED)
#define LUA_SHAPE_FIELD_PALETTE         "Palette"         // ColorPalette
#define LUA_SHAPE_FIELD_INNERTR         "InnerTransparentFaces" // boolean
#define LUA_SHAPE_FIELD_ITEMNAME        "ItemName"        // string (read-only)
#define LUA_SHAPE_FIELD_DRAWMODE        "DrawMode"        // table

// Shape instance functions
#define LUA_SHAPE_FIELD_GETBLOCK          "GetBlock"          // function
#define LUA_SHAPE_FIELD_BLOCKTOWORLD      "BlockToWorld"      // function
#define LUA_SHAPE_FIELD_BLOCKTOLOCAL      "BlockToLocal"      // function
#define LUA_SHAPE_FIELD_WORLDTOBLOCK      "WorldToBlock"      // function
#define LUA_SHAPE_FIELD_LOCALTOBLOCK      "LocalToBlock"      // function
#define LUA_SHAPE_FIELD_LOCALAABB         "ComputeLocalBoundingBox"
#define LUA_SHAPE_FIELD_WORLDAABB         "ComputeWorldBoundingBox"
#define LUA_SHAPE_FIELD_SAVE              "Save"              // function (read-only)
#define LUA_SHAPE_FIELD_IGNOREANIMATIONS  "IgnoreAnimations"  // boolean
#define LUA_SHAPE_FIELD_BAKELIGHT         "ComputeBakedLight" // function (read-only)
#define LUA_SHAPE_FIELD_CLEARLIGHT        "ClearBakedLight" // function (read-only)
#define LUA_SHAPE_FIELD_FITTOSCREEN       "FitToScreen"

// Not documented
#define LUA_SHAPE_FIELD_ADDPOINT        "AddPoint"        // function (read-only)
#define LUA_SHAPE_FIELD_GETPOINT        "GetPoint"        // function (read-only)
#define LUA_SHAPE_FIELD_PRIVATEDRAWMODE "PrivateDrawMode" // number
#define LUA_SHAPE_FIELD_REFRESHMODEL    "RefreshModel"    // function (read-only)
#define LUA_SHAPE_FIELD_PRIVATERTREE    "PrivateRtreeCheck" // function (read-only)

// MARK: Metatable

static int _g_shape_metatable_index(lua_State * const L);
static int _g_shape_metatable_newindex(lua_State * const L);
static int _g_shape_metatable_call(lua_State * const L);
static int _shape_metatable_index(lua_State * const L);
static int _shape_metatable_newindex(lua_State * const L);

// MARK: Lua functions

static int _shape_getBlock(lua_State * const L);
static int _shape_blockToWorld(lua_State * const L);
static int _shape_blockToLocal(lua_State * const L);
static int _shape_worldToBlock(lua_State * const L);
static int _shape_localToBlock(lua_State * const L);
static int _shape_getLocalAABB(lua_State * const L);
static int _shape_getWorldAABB(lua_State * const L);
static int _shape_addPoint(lua_State * const L);
static int _shape_getPoint(lua_State * const L);
static int _shape_refreshModel(lua_State * const L);
static int _shape_bakeLight(lua_State * const L);
static int _shape_clearLight(lua_State * const L);
static int _shape_save(lua_State * const L);
static int _shape_FitToScreen(lua_State * const L);
static int _shape_PrivateRtreeCheck(lua_State * const L);

// MARK: Lua fields

Shape* _lua_shape_get_ptr_from_number3_handler(const number3_C_handler_userdata *userdata);
void _lua_shape_set_pivot(Shape *s, const float *x, const float *y, const float *z);
void _lua_shape_get_pivot(Shape *s, float *x, float *y, float *z);
void _lua_shape_set_pivot_handler(const float *x, const float *y, const float *z, lua_State * const L, const number3_C_handler_userdata *userdata);
void _lua_shape_get_pivot_handler(float *x, float *y, float *z, lua_State * const L, const number3_C_handler_userdata *userdata);
void _lua_shape_set_bounding_box_handler(const float3 *min, const float3 *max, lua_State * const L, void *userdata);
void _lua_shape_get_bounding_box_handler(float3 *min, float3 *max, lua_State * const L, const void *userdata);
void _lua_shape_free_bounding_box_handler(void * const userdata);
void _lua_shape_set_bounding_box_min_n3_handler(const float *x, const float *y, const float *z,
                                                lua_State * const L, const number3_C_handler_userdata *userdata);
void _lua_shape_get_bounding_box_min_n3_handler(float *x, float *y, float *z, lua_State * const L,
                                                const number3_C_handler_userdata *userdata);
void _lua_shape_set_bounding_box_max_n3_handler(const float *x, const float *y, const float *z,
                                                lua_State * const L, const number3_C_handler_userdata *userdata);
void _lua_shape_get_bounding_box_max_n3_handler(float *x, float *y, float *z, lua_State * const L,
                                                const number3_C_handler_userdata *userdata);


/// Creates needed metatables and store them in the Lua registry
/// @param L the Lua state
void lua_g_shape_createAndPush(lua_State * const L) {
    vx_assert(L != nullptr);
    LUAUTILS_STACK_SIZE(L)
    
    lua_newuserdata(L, 1); // create Shape global table
    lua_newtable(L); // metatable
    {
        lua_pushstring(L, "__index");
        lua_pushcfunction(L, _g_shape_metatable_index, "");
        lua_rawset(L, -3);

        lua_pushstring(L, "__newindex");
        lua_pushcfunction(L, _g_shape_metatable_newindex, "");
        lua_rawset(L, -3);

        lua_pushstring(L, "__call");
        lua_pushcfunction(L, _g_shape_metatable_call, "");
        lua_rawset(L, -3);

        // hide the metatable from the Lua sandbox user
        lua_pushstring(L, "__metatable");
        lua_pushboolean(L, false);
        lua_rawset(L, -3);

        // callback functions for Shape:ComputeBakedLight()
        lua_pushstring(L, LUA_G_SHAPE_METAFIELD_BAKECALLBACKS);
        lua_newtable(L);
        lua_rawset(L, -3);

        lua_pushstring(L, "__type");
        lua_pushstring(L, "ShapeInterface");
        lua_rawset(L, -3);

        lua_pushstring(L, "__typen");
        lua_pushinteger(L, ITEM_TYPE_G_SHAPE);
        lua_rawset(L, -3);
    }
    lua_setmetatable(L, -2);
    
    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
}

/// Lua print function for global Shape
int lua_g_shape_snprintf(lua_State * const L, char *result, size_t maxLen, bool spacePrefix) {
    RETURN_VALUE_IF_NULL(L, 0)
    return snprintf(result, maxLen, "%s[Shape] (global)", spacePrefix ? " " : "");
}

///
Shape *lua_shape_getShapePtr(lua_State * const L, const int idx) {
    vx_assert(L != nullptr);
    LUAUTILS_STACK_SIZE(L)
    
    // The object in the Lua stack is:
    // 1) a Shape or MutableShape, get the shape ptr directly
    if (lua_utils_getObjectType(L, idx) == ITEM_TYPE_SHAPE || lua_utils_getObjectType(L, idx) == ITEM_TYPE_MUTABLESHAPE) {
        shape_userdata* ud = static_cast<shape_userdata*>(lua_touserdata(L, idx));
        return ud->shape;
    }
    // 2) a Map, get the shape ptr from map
    else if (lua_utils_getObjectType(L, idx) == ITEM_TYPE_MAP) {
        CGame *g = nullptr;
        if (lua_utils_getGamePtr(L, &g) == false || g == nullptr) {
            LUAUTILS_STACK_SIZE_ASSERT(L, 0)
            return nullptr;
        }
        Shape *s = game_get_map_shape(g);
        if (s == nullptr) {
            LUAUTILS_STACK_SIZE_ASSERT(L, 0)
            return nullptr;
        }
        return s;
    }
    
    LUAUTILS_STACK_SIZE_ASSERT(L, 0)
    return nullptr;
}

/// Lua print function for Shape instances
int lua_shape_snprintf(lua_State * const L, char *result, size_t maxLen, bool spacePrefix) {
    RETURN_VALUE_IF_NULL(L, 0)
    Transform *root; lua_object_getTransformPtr(L, -1, &root);
    if (root == nullptr) {
        return snprintf(result, maxLen, "%s[Shape (destroyed)]", spacePrefix ? " " : "");
    }
    return snprintf(result, maxLen, "%s[Shape %d Name:%s] (instance)", spacePrefix ? " " : "",
                    transform_get_id(root), transform_get_name(root));
}

///
static int _g_shape_metatable_index(lua_State * const L) {
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
    if (strcmp(key, LUA_G_SHAPE_FIELD_LOAD) == 0) {
        luau_dostring(L, R"""(
return function(self, itemName, callback)
    Assets:Load(itemName, AssetType.Shape, function(assets)
        local shapesNotParented = {}
        for _,v in ipairs(assets) do
            if v:GetParent() == nil then
                table.insert(shapesNotParented, v)
            end
        end

        local finalShape
        if #shapesNotParented == 1 then
            finalShape = shapesNotParented[1]
        elseif #shapesNotParented > 1 then
            local root = Object()
            for _,v in ipairs(shapesNotParented) do
                root:AddChild(v)
            end
            finalShape = root
        end
        callback(finalShape)
    end)
end
        )""", "Shape.Load");
        vx::Game *cppGame = nullptr;
        if (lua_utils_getGameCppWrapperPtr(L, &cppGame) == false) {
            LUAUTILS_INTERNAL_ERROR(L) // returns
        }
        lua_log_warning(L, "Shape:Load is deprecated, please use Object:Load instead");
    } else {
        lua_pushnil(L);
    }

    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
    return 1;
}

///
static int _g_shape_metatable_newindex(lua_State * const L) {
    vx_assert(L != nullptr);
    LUAUTILS_STACK_SIZE(L)
    LUAUTILS_STACK_SIZE_ASSERT(L, 0)
    return 0;
}

/// Arguments:
/// 1: Shape global table
/// 2: Item, Shape, MutableShape or Data
/// 3: config table (optional) - default { bakedLight = false, includeChildren = false }
static int _g_shape_metatable_call(lua_State * const L) {
    RETURN_VALUE_IF_NULL(L, 0)
    LUAUTILS_STACK_SIZE(L)
    
    bool bakedLight = false;
    bool includeChildren = false; // used when copying shape

    const int argCount = lua_gettop(L);
    
    if (argCount < 2) {
        LUAUTILS_ERROR(L, "Shape() - expects an Item, Shape, MutableShape or Data as first argument");
    }
    
    if (argCount > 3) {
        LUAUTILS_ERROR(L, "Shape() - too many arguments");
    }
    
    if (argCount == 3 && lua_isnil(L, 3) == false) { // config provided
        if (lua_istable(L, 3) == false) {
            LUAUTILS_STACK_SIZE_ASSERT(L, 0);
            LUAUTILS_ERROR(L, "Shape() - config argument must be a table")
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
            LUAUTILS_ERROR(L, "Shape() - config.bakedLight must be a boolean")
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
            LUAUTILS_ERROR(L, "Shape() - config.recurse must be a boolean")
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
            LUAUTILS_ERROR(L, "Shape() - config.includeChildren must be a boolean")
            return 0;
        }
    }
    
    const char* itemName = nullptr;
    Shape *itemShape = nullptr;
    
    ShapeSettings shapeSettings;
    shapeSettings.isMutable = false;
    shapeSettings.lighting = bakedLight;
    
    if (lua_isuserdata(L, 2) || lua_istable(L, 2)) {
        int type = lua_utils_getObjectType(L, 2);
        if (type == ITEM_TYPE_ITEM) {
            
            itemName = lua_item_getName(L, 2);
            if (itemName == nullptr) {
                LUAUTILS_ERROR(L, "Shape() - failed to resolve item name");
            }
            
        } else if (lua_shape_isShapeType(type)) {

            Transform *self; lua_object_getTransformPtr(L, 2, &self);
            Transform *copy = includeChildren ? transform_utils_copy_recurse(self) : transform_new_copy(self);
            Shape *s = static_cast<Shape*>(transform_get_ptr(copy));
            shape_set_lua_mutable(s, false);
            lua_shape_pushNewInstance(L, s);
            transform_release(copy); // owned by lua Shape

            LUAUTILS_STACK_SIZE_ASSERT(L, 1)
            return 1;

        } else if (type == ITEM_TYPE_DATA) {
            size_t len = 0;
            const char *bytes = static_cast<const char*>(lua_data_getBuffer(L, 2, &len));
            
            CGame *g = nullptr;
            if (lua_utils_getGamePtr(L, &g) == false) {
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
                                                                          false,
                                                                          game_get_color_atlas(g));
                stream_free(s);
                if (err != no_error) {
                    itemShape = nullptr;
                }
            }
            
        } else {
            LUAUTILS_ERROR(L, "Shape() - wrong type of argument")
        }
    } else if (lua_isstring(L, 2)) {
        
        // NOTE: we could accept string parameters,
        // BUT, they have to link to Items loaded in the current sandbox.
        // Otherwise, it would allow to open things from the cache,
        // loaded by experiences.
        // That's why we don't support it for now.
        LUAUTILS_ERROR(L, "Shape() - string argument not yet supported");

    } else {
        LUAUTILS_ERROR(L, "Shape() - wrong type of argument");
    }
    
    // loading from file
    if (itemName != nullptr && itemShape == nullptr) {
        
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
                LUAUTILS_ERROR_F(L, "Shape() - file not found %s", itemName);
            } else {
                fd = vx::fs::openBundleFile(bundleRelPath, "rb");
            }
        }
        
        if (fd == nullptr) {
            LUAUTILS_ERROR(L, "Shape() - internal error");
        }
        
        // create shape from file
        CGame *g = nullptr;
        if (lua_utils_getGamePtr(L, &g) == false || g == nullptr) {
            LUAUTILS_INTERNAL_ERROR(L) // returns
        }
        // The file descriptor is owned by the stream, which will fclose it in the future.
        Stream *s = stream_new_file_read(fd);
        itemShape = serialization_load_shape(s, // frees stream, closing fd
                                             itemName,
                                             game_get_color_atlas(g),
                                             &shapeSettings,
                                             false);
    }
    
    if (itemShape == nullptr) {
        LUAUTILS_ERROR(L, "Shape() - Shape could not be created");
    }

    // if resulting shape didn't have serialized lighting data, compute it
    if (shapeSettings.lighting && shape_uses_baked_lighting(itemShape) == false) {
        _lua_shape_load_or_compute_baked_light(itemShape);
    }

    if (lua_shape_pushNewInstance(L, itemShape) == false) {
        lua_pushnil(L); // GC will release shape
    }
    shape_release(itemShape); // now owned by lua Shape
    
    return 1;
}

///
static int _shape_metatable_index(lua_State * const L) {
    vx_assert(L != nullptr);
    LUAUTILS_ASSERT_ARGS_COUNT(L, 2)
    LUAUTILS_ASSERT_ARG_TYPE(L, 1, ITEM_TYPE_SHAPE)
    LUAUTILS_STACK_SIZE(L)

    const char *key = lua_tostring(L, 2);
    if (_lua_object_metatable_index(L, key) == false) {
        // no field found in Object 
        // retrieve underlying Shape struct
        Shape *s = lua_shape_getShapePtr(L, 1);
        if (s == nullptr) {
            lua_pushnil(L);
            LUAUTILS_STACK_SIZE_ASSERT(L, 1)
            return 1;
        }

        if (_lua_shape_metatable_index(L, s, key) == false) {
            // nothing found at that key
            lua_pushnil(L);
        }
    }
    
    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
    return 1;
}

///
static int _shape_metatable_newindex(lua_State * const L) {
    vx_assert(L != nullptr);
    // validate arguments count
    if (lua_gettop(L) != 3) {
        LUAUTILS_ERROR(L, "[Shape.?] incorrect argument count"); // returns
    }
    LUAUTILS_STACK_SIZE(L)
    
    // retrieve underlying Shape struct
    Shape *s = lua_shape_getShapePtr(L, 1);
    if (s == nullptr) {
        lua_pushnil(L);
        LUAUTILS_STACK_SIZE_ASSERT(L, 1)
        return 1;
    }
    Transform *t = shape_get_transform(s);

    const char *key = lua_tostring(L, 2);
    
    if (_lua_object_metatable_newindex(L, t, key) == false) {
        // the key is not from Object
        if (_lua_shape_metatable_newindex(L, s, key) == false) {
            // key not found
            LUAUTILS_ERROR_F(L, "Shape: %s field is not settable", key);
        }
    }
    
    LUAUTILS_STACK_SIZE_ASSERT(L, 0)
    return 0;
}

void lua_shape_release_transform(void *_ud) {
    shape_userdata *ud = static_cast<shape_userdata*>(_ud);
    Shape *shape = ud->shape;
    if (shape != nullptr) {
        shape_release(shape);
        ud->shape = nullptr;
    }
}

/// Lua Shape is being garbage collected.
void lua_shape_gc(void *_ud) {
    shape_userdata *ud = static_cast<shape_userdata*>(_ud);
    Shape *shape = ud->shape;
    if (shape != nullptr) {
        shape_release(shape);
        ud->shape = nullptr;
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

///
bool lua_shape_pushNewInstance(lua_State * const L, Shape *s) {
    vx_assert(L != nullptr);
    vx_assert(s != nullptr);
    vx_assert(shape_is_lua_mutable(s) == false);

    Transform *t = shape_get_transform(s);

    // lua Shape takes ownership
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
        lua_pushcfunction(L, _shape_metatable_index, "");
        lua_rawset(L, -3);
        
        lua_pushstring(L, "__newindex");
        lua_pushcfunction(L, _shape_metatable_newindex, "");
        lua_rawset(L, -3);
        
        // hide the metatable from the Lua sandbox user
        lua_pushstring(L, "__metatable");
        lua_pushboolean(L, false);
        lua_rawset(L, -3);

        lua_pushstring(L, "__type");
        lua_pushstring(L, "Shape");
        lua_rawset(L, -3);

        lua_pushstring(L, "__typen");
        lua_pushinteger(L, ITEM_TYPE_SHAPE);
        lua_rawset(L, -3);

        // private fields
        _lua_shape_push_fields(L, s); // TODO: store in registry when accessed
    }
    lua_setmetatable(L, -2);
    
    if (lua_g_object_addIndexEntry(L, t, -1) == false) {
        vxlog_error("Failed to add Lua Shape to Object registry");
        lua_pop(L, 1); // pop Lua Shape
        LUAUTILS_STACK_SIZE_ASSERT(L, 0)
        return false;
    }

    scene_register_managed_transform(game_get_scene(g->getCGame()), t);

    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
    return true;
}

bool lua_shape_isShape(lua_State * const L, const int idx) {
    vx_assert(L != nullptr);
    return lua_shape_isShapeType(lua_utils_getObjectType(L, idx));
}

bool lua_shape_isShapeType(const int type) {
    return type == ITEM_TYPE_SHAPE || lua_mutableShape_isMutableShapeType(type);
}

// called in the context of the Shape's metatable
void _lua_shape_push_fields(lua_State * const L, Shape *s) {
    number3_C_handler_userdata userdata = number3_C_handler_userdata_zero;
    lua_pushstring(L, LUA_SHAPE_FIELD_PIVOT);
    lua_number3_pushNewObject(L, 0.0f, 0.0f, 0.0f);
    if (s != nullptr) {
        userdata.ptr = transform_get_and_retain_weakptr(shape_get_transform(s));
        lua_number3_setHandlers(L, -1,
                                _lua_shape_set_pivot_handler,
                                _lua_shape_get_pivot_handler,
                                userdata, true);
    }
    lua_rawset(L, -3);
    
    // BoundingBox (Box)
    lua_pushstring(L, LUA_SHAPE_FIELD_BOUNDINGBOX);
    lua_box_pushNewTable(L,
                         static_cast<void *>(transform_get_and_retain_weakptr(shape_get_transform(s))),
                         _lua_shape_set_bounding_box_handler,
                         _lua_shape_get_bounding_box_handler,
                         _lua_shape_free_bounding_box_handler,
                         _lua_shape_set_bounding_box_min_n3_handler,
                         _lua_shape_get_bounding_box_min_n3_handler,
                         _lua_shape_set_bounding_box_max_n3_handler,
                         _lua_shape_get_bounding_box_max_n3_handler);
    lua_rawset(L, -3);
}

bool _lua_shape_metatable_index(lua_State * const L, Shape *s, const char *key) {
    
    if (strcmp(key, LUA_SHAPE_FIELD_PIVOT) == 0) {
        shape_apply_current_transaction(s, true);
        LUA_GET_METAFIELD_PUSHING_NIL_IF_NOT_FOUND(L, 1, LUA_SHAPE_FIELD_PIVOT);
        return true;
        
    } else if (strcmp(key, LUA_SHAPE_FIELD_MIN) == 0) {
        shape_apply_current_transaction(s, true);
        
        SHAPE_COORDS_INT3_T min; shape_get_model_aabb_2(s, &min, nullptr);
        lua_number3_pushNewObject(L, min.x, min.y, min.z);
        return true;
        
    } else if (strcmp(key, LUA_SHAPE_FIELD_CENTER) == 0) {
        shape_apply_current_transaction(s, true);

        const Box aabb = shape_get_model_aabb(s);
        float3 center; box_get_center(&aabb, &center);
        lua_number3_pushNewObject(L, center.x, center.y, center.z);
        return true;
        
    } else if (strcmp(key, LUA_SHAPE_FIELD_MAX) == 0) {
        shape_apply_current_transaction(s, true);
        
        SHAPE_COORDS_INT3_T max; shape_get_model_aabb_2(s, nullptr, &max);
        lua_number3_pushNewObject(L, max.x, max.y, max.z);
        return true;

    } else if (strcmp(key, LUA_SHAPE_FIELD_SIZE) == 0) {
        shape_apply_current_transaction(s, true);

        int3 size; shape_get_bounding_box_size(s, &size);
        lua_number3_pushNewObject(L, size.x, size.y, size.z);
        return true;

    } else if (strcmp(key, LUA_SHAPE_FIELD_WIDTH) == 0) {
        shape_apply_current_transaction(s, true);

        int3 size; shape_get_bounding_box_size(s, &size);
        lua_pushnumber(L, static_cast<double>(size.x));
        return true;
        
    } else if (strcmp(key, LUA_SHAPE_FIELD_HEIGHT) == 0) {
        shape_apply_current_transaction(s, true);

        int3 size; shape_get_bounding_box_size(s, &size);
        lua_pushnumber(L, static_cast<double>(size.y));
        return true;
        
    } else if (strcmp(key, LUA_SHAPE_FIELD_DEPTH) == 0) {
        shape_apply_current_transaction(s, true);

        int3 size; shape_get_bounding_box_size(s, &size);
        lua_pushnumber(L, static_cast<double>(size.z));
        return true;
    } else if (strcmp(key, LUA_SHAPE_FIELD_PRIVATEDRAWMODE) == 0) {
        lua_pushnumber(L, static_cast<double>(shape_get_draw_mode_deprecated(s)));
        return true;
    } else if (strcmp(key, LUA_SHAPE_FIELD_GETBLOCK) == 0) {
        lua_pushcfunction(L, _shape_getBlock, "");
        return true;
    } else if (strcmp(key, LUA_SHAPE_FIELD_BLOCKTOWORLD) == 0) {
        lua_pushcfunction(L, _shape_blockToWorld, "");
        return true;
    } else if (strcmp(key, LUA_SHAPE_FIELD_BLOCKTOLOCAL) == 0) {
        lua_pushcfunction(L, _shape_blockToLocal, "");
        return true;
    } else if (strcmp(key, LUA_SHAPE_FIELD_WORLDTOBLOCK) == 0) {
        lua_pushcfunction(L, _shape_worldToBlock, "");
        return true;
    } else if (strcmp(key, LUA_SHAPE_FIELD_LOCALTOBLOCK) == 0) {
        lua_pushcfunction(L, _shape_localToBlock, "");
        return true;
    } else if (strcmp(key, LUA_SHAPE_FIELD_LOCALAABB) == 0) {
        lua_pushcfunction(L, _shape_getLocalAABB, "");
        return true;
    } else if (strcmp(key, LUA_SHAPE_FIELD_WORLDAABB) == 0) {
        lua_pushcfunction(L, _shape_getWorldAABB, "");
        return true;
    } else if (strcmp(key, LUA_SHAPE_FIELD_ADDPOINT) == 0) {
        lua_pushcfunction(L, _shape_addPoint, "");
        return true;
        
    } else if (strcmp(key, LUA_SHAPE_FIELD_GETPOINT) == 0) {
        lua_pushcfunction(L, _shape_getPoint, "");
        return true;
        
    } else if (strcmp(key, LUA_SHAPE_FIELD_BOUNDINGBOX) == 0) {
        shape_apply_current_transaction(s, true);
        LUA_GET_METAFIELD_PUSHING_NIL_IF_NOT_FOUND(L, 1, LUA_SHAPE_FIELD_BOUNDINGBOX);
        return true;
    } else if (strcmp(key, LUA_SHAPE_FIELD_SHADOW) == 0) {
        lua_pushboolean(L, shape_has_shadow(s));
        return true;
    } else if (strcmp(key, LUA_SHAPE_FIELD_UNLIT) == 0) {
        lua_pushboolean(L, shape_is_unlit(s));
        return true;
    } else if (strcmp(key, LUA_SHAPE_FIELD_COUNT) == 0) {
        shape_apply_current_transaction(s, true);
        lua_pushinteger(L, static_cast<lua_Integer>(shape_get_nb_blocks(s)));
        return true;
    } else if (strcmp(key, LUA_SHAPE_FIELD_LAYERS) == 0) {
        lua_utils_pushMaskAsTable(L, shape_get_layers(s), CAMERA_LAYERS_MASK_API_BITS);
        return true;
    } else if (strcmp(key, LUA_SHAPE_FIELD_PALETTE) == 0 || strcmp(key, LUA_SHAPE_FIELD_LOCAL_PALETTE) == 0) {
        // pushes nil if the palette can't be found
        lua_palette_create_and_push_table(L, shape_get_palette(s), s);
        return true;
    } else if (strcmp(key, LUA_SHAPE_FIELD_REFRESHMODEL) == 0) {
        lua_pushcfunction(L, _shape_refreshModel, "");
        return true;
    } else if (strcmp(key, LUA_SHAPE_FIELD_BAKELIGHT) == 0) {
        lua_pushcfunction(L, _shape_bakeLight, "");
        return true;
    } else if (strcmp(key, LUA_SHAPE_FIELD_CLEARLIGHT) == 0) {
        lua_pushcfunction(L, _shape_clearLight, "");
        return true;
    } else if (strcmp(key, LUA_SHAPE_FIELD_SAVE) == 0) {
        lua_pushcfunction(L, _shape_save, "");
        return true;
    } else if (strcmp(key, LUA_SHAPE_FIELD_IGNOREANIMATIONS) == 0) {
        const bool ignoreAnimations = shape_getIgnoreAnimations(s);
        lua_pushboolean(L, ignoreAnimations);
        return true;
    } else if (strcmp(key, LUA_SHAPE_FIELD_INNERTR) == 0) {
        lua_pushboolean(L, shape_draw_inner_transparent_faces(s));
        return true;
    } else if (strcmp(key, LUA_SHAPE_FIELD_ITEMNAME) == 0) {
        lua_pushstring(L, shape_get_fullname(s));
        return true;
    } else if (strcmp(key, LUA_SHAPE_FIELD_FITTOSCREEN) == 0) {
        lua_pushcfunction(L, _shape_FitToScreen, "");
        return true;
    } else if (strcmp(key, LUA_SHAPE_FIELD_PRIVATERTREE) == 0) {
        lua_pushcfunction(L, _shape_PrivateRtreeCheck, "");
        return true;
    } else if (strcmp(key, LUA_SHAPE_FIELD_DRAWMODE) == 0) {
        if (shape_uses_drawmodes(s)) {
            lua_newtable(L);
            {
                const bool hasMultiplicative = shape_drawmode_get_multiplicative_color(s) != SHAPE_DEFAULT_MULTIPLICATIVE_COLOR;
                const bool hasAdditive = shape_drawmode_get_additive_color(s) != SHAPE_DEFAULT_ADDITIVE_COLOR;

                if (hasMultiplicative && hasAdditive == false) {
                    const RGBAColor color = uint32_to_color(shape_drawmode_get_multiplicative_color(s));

                    lua_pushstring(L, "color");
                    lua_color_create_and_push_table(L, color.r, color.g, color.b, color.a, false, true);
                    lua_rawset(L, -3);
                } else if (hasMultiplicative && hasAdditive) {
                    lua_pushstring(L, "color");
                    lua_newtable(L);
                    {
                        const RGBAColor multColor = uint32_to_color(shape_drawmode_get_multiplicative_color(s));
                        const RGBAColor addColor = uint32_to_color(shape_drawmode_get_additive_color(s));

                        lua_pushstring(L, "multiplicative");
                        lua_color_create_and_push_table(L, multColor.r, multColor.g, multColor.b, multColor.a, false, true);
                        lua_rawset(L, -3);

                        lua_pushstring(L, "additive");
                        lua_color_create_and_push_table(L, addColor.r, addColor.g, addColor.b, 0, false, true);
                        lua_rawset(L, -3);
                    }
                    lua_rawset(L, -3);
                }
            }
        } else {
            lua_pushnil(L);
        }
        return true;
    }
    
    return false;
}

/// returns true if a key has been set without error
/// false if an uppercase-starting key has not been found
bool _lua_shape_metatable_newindex(lua_State * const L, Shape *s, const char *key) {
    if (strcmp(key, LUA_SHAPE_FIELD_PIVOT) == 0) {
        float3 f3;
         if (lua_utils_getObjectType(L, 3) == ITEM_TYPE_BLOCK) {
            SHAPE_COORDS_INT_T x, y, z;
            lua_block_getInfo(L, 3, nullptr, &x, &y, &z, nullptr);
            
            f3.x = static_cast<float>(x) + .5f;
            f3.y = static_cast<float>(y) + .5f;
            f3.z = static_cast<float>(z) + .5f;
            _lua_shape_set_pivot(s, &f3.x, &f3.y, &f3.z);
         } else if (lua_number3_or_table_getXYZ(L, 3, &f3.x, &f3.y, &f3.z)) {
             _lua_shape_set_pivot(s, &f3.x, &f3.y, &f3.z);
         } else {
            LUAUTILS_ERROR_F(L, "Shape.%s cannot be set (new value is not a Number3)", key);
        }
        return true;
    } else if (strcmp(key, LUA_SHAPE_FIELD_PRIVATEDRAWMODE) == 0) {
        if (lua_isnumber(L, 3)) {
            shape_set_draw_mode_deprecated(s, lua_tonumber(L, 3));
        } else {
            LUAUTILS_ERROR_F(L, "Shape.%s cannot be set (new value is not a number)", key);
        }
        return true;
    } else if (strcmp(key, LUA_SHAPE_FIELD_SHADOW) == 0) {
        if (lua_isboolean(L, 3)) {
            shape_set_shadow(s, lua_toboolean(L, 3));
        } else {
            lua_pushfstring(L, "Shape.%s cannot be set (new value is not a boolean)", key);
            lua_error(L);
        }
        return true;
    } else if (strcmp(key, LUA_SHAPE_FIELD_UNLIT) == 0) {
        if (lua_isboolean(L, 3)) {
            shape_set_unlit(s, lua_toboolean(L, 3));
        } else {
            LUAUTILS_ERROR_F(L, "Shape.%s cannot be set (new value is not a boolean)", key);
        }
        return true;
    } else if (strcmp(key, LUA_SHAPE_FIELD_LAYERS) == 0) {
        uint16_t mask = 0;
        if (lua_utils_getMask(L, 3, &mask, CAMERA_LAYERS_MASK_API_BITS) == false) {
            LUAUTILS_ERROR_F(L, "Shape.%s cannot be set (new value should be one or a table of integers between 1 and %d)", key, CAMERA_LAYERS_MASK_API_BITS)
        }
        shape_set_layers(s, mask);
        return true;
    } else if (strcmp(key, LUA_SHAPE_FIELD_IGNOREANIMATIONS) == 0) {
        if (lua_isboolean(L, 3)) {
            const bool ignoreAnimations = lua_toboolean(L, 3);
            if (ignoreAnimations) {
                shape_disableAnimations(s);
            } else {
                shape_enableAnimations(s);
            }
        } else {
            LUAUTILS_ERROR_F(L, "Shape.%s cannot be set (new value is not a boolean)", key);
        }
        return true;
    } else if (strcmp(key, LUA_SHAPE_FIELD_INNERTR) == 0) {
        if (lua_isboolean(L, 3)) {
            shape_set_inner_transparent_faces(s, lua_toboolean(L, 3));
        } else {
            LUAUTILS_ERROR_F(L, "Shape.%s cannot be set (new value is not a boolean)", key);
        }
        return true;
    } else if (strcmp(key, LUA_SHAPE_FIELD_PALETTE) == 0) {
        if (lua_utils_getObjectType(L, 3) == ITEM_TYPE_PALETTE) {
            ColorPalette *palette = lua_palette_getCPointer(L, 3, nullptr);
            if (palette == nullptr) {
                LUAUTILS_INTERNAL_ERROR(L)
            }
            shape_set_palette(s, palette, true);
        } else {
            LUAUTILS_ERROR_F(L, "Shape.%s cannot be set (new value is not a Palette)", key);
        }
        return true;
    } else if (strcmp(key, LUA_SHAPE_FIELD_DRAWMODE) == 0) {
        if (lua_istable(L, 3)) {
            RGBAColor color;

            lua_getfield(L, 3, "color");
            if (lua_istable(L, -1)) {
                lua_getfield(L, -1, "multiplicative");
                if (lua_color_getRGBAL(L, -1, &color.r, &color.g, &color.b, &color.a, nullptr)) {
                    shape_drawmode_set_multiplicative_color(s, color_to_uint32(&color));
                } else if (lua_isnil(L, -1) == false) {
                    LUAUTILS_ERROR_F(L, "Shape.%s 'multiplicative' must be a Color", key);
                }
                lua_pop(L, 1);

                lua_getfield(L, -1, "additive");
                if (lua_color_getRGBAL(L, -1, &color.r, &color.g, &color.b, nullptr, nullptr)) {
                    shape_drawmode_set_additive_color(s, color_to_uint32(&color));
                } else if (lua_isnil(L, -1) == false) {
                    LUAUTILS_ERROR_F(L, "Shape.%s 'additive' must be a Color", key);
                }
                lua_pop(L, 1);
            } else if (lua_color_getRGBAL(L, -1, &color.r, &color.g, &color.b, &color.a, nullptr)) {
                shape_drawmode_set_multiplicative_color(s, color_to_uint32(&color));
            } else if (lua_isnil(L, -1) == false) {
                LUAUTILS_ERROR_F(L, "Shape.%s 'color' must be a Color or a table of any or all { multiplicative, additive }", key);
            }
            lua_pop(L, 1);
        } else if (lua_isnil(L, 3) || lua_isboolean(L, 3) && lua_toboolean(L, 3) == false) {
            shape_toggle_drawmodes(s, false);
        } else {
            LUAUTILS_ERROR_F(L, "Shape.%s must be a table of draw mode parameters, or nil/false to reset parameters", key);
        }
        return true;
    }
    
    return false; // nothing was set
}

void _lua_shape_load_or_compute_baked_light(Shape *s) {
#if GLOBAL_LIGHTING_BAKE_LOAD_ENABLED
    if (cache_load_or_clear_baked_file(s)) {
        shape_refresh_all_vertices(s);
    } else {
#endif
        shape_set_model_locked(s, true);
        shape_retain(s);

        vx::OperationQueue::getSlowBackground()->dispatch([s]() {
            shape_compute_baked_lighting(s);

            vx::OperationQueue::getMain()->dispatch([s]() {
#if GLOBAL_LIGHTING_BAKE_SAVE_ENABLED
                if (shape_uses_baked_lighting(s)) {
                    cache_save_baked_file(s);
                }
#endif
                shape_set_model_locked(s, false);
                shape_release(s);
            });
        });
#if GLOBAL_LIGHTING_BAKE_LOAD_ENABLED
    }
#endif
}

// MARK: -

Shape* _lua_shape_get_ptr_from_number3_handler(const number3_C_handler_userdata *userdata) {
    Transform *t = static_cast<Transform*>(weakptr_get(static_cast<Weakptr*>(userdata->ptr)));
    return t == nullptr || transform_get_type(t) != ShapeTransform ?
    nullptr : static_cast<Shape*>(transform_get_ptr(t));
}

void _lua_shape_set_pivot(Shape *s, const float *x, const float *y, const float *z) {
    
    float3 pivot = shape_get_pivot(s);

    // Get shift to apply it to children
    float3 diff;
    diff.x = (x != nullptr ? pivot.x - *x : 0);
    diff.y = (y != nullptr ? pivot.y - *y : 0);
    diff.z = (z != nullptr ? pivot.z - *z : 0);

    shape_set_pivot(s,
                    x != nullptr ? *x : pivot.x,
                    y != nullptr ? *y : pivot.y,
                    z != nullptr ? *z : pivot.z);

    DoublyLinkedListNode *n = shape_get_transform_children_iterator(s);
    Transform *childTransform = nullptr;
    while (n != nullptr) {
        childTransform = static_cast<Transform *>(doubly_linked_list_node_pointer(n));
        const float3 *pos = transform_get_local_position(childTransform, true);
        transform_set_local_position(childTransform, pos->x + diff.x, pos->y + diff.y, pos->z + diff.z);
        n = doubly_linked_list_node_next(n);
    }
}

void _lua_shape_get_pivot(Shape *s, float *x, float *y, float *z) {
    
    float3 f3 = shape_get_pivot(s);
    
    if (x != nullptr) { *x = f3.x; }
    if (y != nullptr) { *y = f3.y; }
    if (z != nullptr) { *z = f3.z; }
}

void _lua_shape_set_pivot_handler(const float *x, const float *y, const float *z, lua_State * const L, const number3_C_handler_userdata *userdata) {
    
    Shape *s = _lua_shape_get_ptr_from_number3_handler(userdata);
    if (s == nullptr) return;
    
    _lua_shape_set_pivot(s, x, y, z);
}

void _lua_shape_get_pivot_handler(float *x, float *y, float *z, lua_State * const L, const number3_C_handler_userdata *userdata) {
    
    Shape *s = _lua_shape_get_ptr_from_number3_handler(userdata);
    if (s == nullptr) return;
    
    _lua_shape_get_pivot(s, x, y, z);
}

void _lua_shape_set_bounding_box_handler(const float3 *min, const float3 *max, lua_State * const L, void *userdata) {
    LUAUTILS_ERROR(L, "BoundingBox is readonly");
}

void _lua_shape_get_bounding_box_handler(float3 *min, float3 *max, lua_State * const L, const void *userdata) {
    Transform *t = static_cast<Transform *>(weakptr_get(static_cast<const Weakptr*>(userdata)));
    if (t == nullptr) {
        LUAUTILS_ERROR(L, "[Shape.BoundingBox] original Shape does not exist anymore"); // returns
    }
    Shape *s = static_cast<Shape*>(transform_get_ptr(t));
    
    const Box aabb = shape_get_model_aabb(s);
    if (min != nullptr) {
        *min = aabb.min;
    }
    if (max != nullptr) {
        *max = aabb.max;
    }
}

void _lua_shape_free_bounding_box_handler(void * const userdata) {
    weakptr_release(static_cast<Weakptr*>(userdata));
}

void _lua_shape_set_bounding_box_min_n3_handler(const float *x, const float *y, const float *z,
                                                lua_State * const L, const number3_C_handler_userdata *userdata) {
    LUAUTILS_ERROR(L, "BoundingBox is readonly");
}

void _lua_shape_get_bounding_box_min_n3_handler(float *x, float *y, float *z, lua_State * const L,
                                                const number3_C_handler_userdata *userdata) {
    Shape *s = static_cast<Shape*>(userdata->ptr);
    if (s == nullptr) return;
    
    SHAPE_COORDS_INT3_T min; shape_get_model_aabb_2(s, &min, nullptr);
    
    if (x != nullptr) { *x = static_cast<float>(min.x); }
    if (y != nullptr) { *y = static_cast<float>(min.y); }
    if (z != nullptr) { *z = static_cast<float>(min.z); }
}

void _lua_shape_set_bounding_box_max_n3_handler(const float *x, const float *y, const float *z,
                                                lua_State * const L, const number3_C_handler_userdata *userdata) {
    LUAUTILS_ERROR(L, "BoundingBox is readonly");
}

void _lua_shape_get_bounding_box_max_n3_handler(float *x, float *y, float *z, lua_State * const L,
                                                const number3_C_handler_userdata *userdata) {
    Shape *s = static_cast<Shape*>(userdata->ptr);
    if (s == nullptr) return;

    SHAPE_COORDS_INT3_T max; shape_get_model_aabb_2(s, nullptr, &max);
    
    if (x != nullptr) { *x = static_cast<float>(max.x); }
    if (y != nullptr) { *y = static_cast<float>(max.y); }
    if (z != nullptr) { *z = static_cast<float>(max.z); }
}

// for string delimiter
std::vector<std::string> split(std::string s, std::string delimiter) {
    size_t pos_start = 0, pos_end, delim_len = delimiter.length();
    std::string token;
    std::vector<std::string> res;

    while ((pos_end = s.find (delimiter, pos_start)) != std::string::npos) {
        token = s.substr (pos_start, pos_end - pos_start);
        pos_start = pos_end + delim_len;
        res.push_back (token);
    }

    res.push_back(s.substr (pos_start));
    return res;
}

// 2 args : MutableShape / Number3
// 4 args : MutableShape / X / Y / Z
static int _shape_getBlock(lua_State * const L) {
    vx_assert(L != nullptr);
    LUAUTILS_STACK_SIZE(L)
    
    const int argCount = lua_gettop(L);
    
    if (argCount < 1 || lua_shape_isShape(L, 1) == false) {
        LUAUTILS_ERROR(L, "Shape/MutableShape:GetBlock - function should be called with `:`");
    }

    float fx, fy, fz;
    if (argCount == 2) { // Shape/MutableShape:GetBlock(Number3)
        if (lua_number3_or_table_getXYZ(L, 2, &fx, &fy, &fz) == false) {
            LUAUTILS_ERROR(L, "Shape/MutableShape:GetBlock - 1st argument should be a Number3");
        }
        fx = std::floor(fx);
        fy = std::floor(fy);
        fz = std::floor(fz);
        
    } else if (argCount == 4) { // Shape/MutableShape:GetBlock(x, y, z)
        
        if (lua_isnumber(L, 2) == false) {
            LUAUTILS_ERROR(L, "Shape/MutableShape:GetBlock - 1st argument should be a number");
        }
        if (lua_isnumber(L, 3) == false) {
            LUAUTILS_ERROR(L, "Shape/MutableShape:GetBlock - 2nd argument should be a number");
        }
        if (lua_isnumber(L, 4) == false) {
            LUAUTILS_ERROR(L, "Shape/MutableShape:GetBlock - 3rd argument should be a number");
        }

        fx = std::floor(lua_tonumber(L, 2));
        fy = std::floor(lua_tonumber(L, 3));
        fz = std::floor(lua_tonumber(L, 4));
        
    } else {
        LUAUTILS_ERROR(L, "Shape:GetBlock - coordinates should be a Number3, a table of 3 numbers, or 3 individual numbers");
        return 0; // silence warnings
    }

    if (fx < SHAPE_COORDS_MIN || fx > SHAPE_COORDS_MAX ||
        fy < SHAPE_COORDS_MIN || fy > SHAPE_COORDS_MAX ||
        fz < SHAPE_COORDS_MIN || fz > SHAPE_COORDS_MAX) {
        LUAUTILS_ERROR(L, "Shape:GetBlock - maximum coordinates reached");
    }

    const SHAPE_COORDS_INT_T blockX = static_cast<SHAPE_COORDS_INT_T>(fx);
    const SHAPE_COORDS_INT_T blockY = static_cast<SHAPE_COORDS_INT_T>(fy);
    const SHAPE_COORDS_INT_T blockZ = static_cast<SHAPE_COORDS_INT_T>(fz);
    
    Shape *sh = lua_shape_getShapePtr(L, 1);
    shape_apply_current_transaction(sh, true);

    // pushes nil if the block can't be found
    lua_block_pushBlock(L, sh, blockX, blockY, blockZ);
    
    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
    return 1;
}

static bool _shape_blockConversions_checkAndGetArg(lua_State * const L, const char *name, float3 *arg, Shape **s,
                                                   bool acceptBlock) {
    
    // Validate arguments count
    const int argCount = lua_gettop(L);
    if (argCount != 2 && argCount != 4) {
        lua_pushfstring(L, "%s incorrect argument count", name);
        return false;
    }
    
    // Validate caller's type
    if (lua_shape_isShape(L, 1) == false) {
        lua_pushfstring(L, "%s incompatible caller", name);
        return false;
    }
    *s = lua_shape_getShapePtr(L, 1);
    
    *arg = float3_zero;
    
    if (argCount == 2) {
        if (lua_utils_getObjectType(L, 2) == ITEM_TYPE_NUMBER3) {
            if (lua_number3_getXYZ(L, 2, &(arg->x), &(arg->y), &(arg->z)) != true) {
                lua_pushfstring(L, "%s failed to get position from Number3", name);
                return false;
            }
        } else if (acceptBlock && lua_utils_getObjectType(L, 2) == ITEM_TYPE_BLOCK) {
            
            SHAPE_COORDS_INT_T blockX = 0;
            SHAPE_COORDS_INT_T blockY = 0;
            SHAPE_COORDS_INT_T blockZ = 0;
            
            lua_block_getInfo(L, 2, nullptr, &blockX, &blockY, &blockZ, nullptr);
            
            arg->x = static_cast<float>(blockX);
            arg->y = static_cast<float>(blockY);
            arg->z = static_cast<float>(blockZ);
            
        } else {
            lua_pushfstring(L, "%s argument should be a Number3 or Block", name);
            return false;
        }
    } else if (argCount == 4) {
        if (lua_isnumber(L, 2) == false || lua_isnumber(L, 3) == false || lua_isnumber(L, 4) == false) {
            lua_pushfstring(L, "%s arguments should be numbers (X, Y, Z)", name);
            return false;
        }
        arg->x = static_cast<float>(lua_tonumber(L, 2));
        arg->y = static_cast<float>(lua_tonumber(L, 3));
        arg->z = static_cast<float>(lua_tonumber(L, 4));
    }
    
    return true;
}

/// Shape:BlockToWorld(value) (A)
/// Shape:BlockToWorld(x, y, z) (B)
/// - 1: lua object w/ Shape support
/// - (A) 2 (value): lua Block or Number3
/// - (B) 2, 3 & 4 (x, y, z): block coordinates as 3 numbers
/// returns: transformed Number3
static int _shape_blockToWorld(lua_State * const L) {
    RETURN_VALUE_IF_NULL(L, 0)
    LUAUTILS_STACK_SIZE(L)
    
    Shape *s;
    float3 blockCoords;
    if (_shape_blockConversions_checkAndGetArg(L, "Shape:BlockToWorld", &blockCoords, &s, true) == false) {
        lua_error(L); // returns
        // return 0; // return statement not to confuse code analyzer
    }
    
    float3 world = shape_block_to_world(s, blockCoords.x, blockCoords.y, blockCoords.z);
    
    lua_number3_pushNewObject(L, world.x, world.y, world.z);
    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
    return 1;
}

/// Shape:BlockToLocal(self, value)
/// Shape:BlockToLocal(x, y, z) (B)
/// - 1: lua object w/ Shape support
/// - (A) 2 (value): lua Block or Number3
/// - (B) 2, 3 & 4 (x, y, z): block coordinates as 3 numbers
/// returns: transformed Number3
static int _shape_blockToLocal(lua_State * const L) {
    RETURN_VALUE_IF_NULL(L, 0)
    LUAUTILS_STACK_SIZE(L)
    
    Shape *s = nullptr;
    float3 blockCoords;
    if (_shape_blockConversions_checkAndGetArg(L, "Shape:BlockToLocal", &blockCoords, &s, true) == false) {
        lua_error(L); // returns
        // return 0; // return statement not to confuse code analyzer
    }
    
    float3 local = shape_block_to_local(s, blockCoords.x, blockCoords.y, blockCoords.z);
    
    lua_number3_pushNewObject(L, local.x, local.y, local.z);
    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
    return 1;
}

/// Shape:WorldToBlock(value) (A)
/// Shape:WorldToBlock(x, y, z) (B)
/// - 1: lua object w/ Shape support
/// - (A) 2 (value): Number3
/// - (B) 2, 3 & 4 (x, y, z): world pos as 3 numbers
/// returns: transformed Number3
static int _shape_worldToBlock(lua_State * const L) {
    RETURN_VALUE_IF_NULL(L, 0)
    LUAUTILS_STACK_SIZE(L)
    
    Shape *s = nullptr;
    float3 world;
    if (_shape_blockConversions_checkAndGetArg(L, "Shape:WorldToBlock", &world, &s, false) == false) {
        lua_error(L); // returns
        // return 0; // return statement not to confuse code analyzer
    }
    
    CGame *g = nullptr;
    if (lua_utils_getGamePtr(L, &g) == false || g == nullptr) {
        lua_pushnil(L);
        LUAUTILS_STACK_SIZE_ASSERT(L, 1)
        return 1;
    }
    
    float3 blockCoords = shape_world_to_block(s, world.x, world.y, world.z);
    
    lua_number3_pushNewObject(L, blockCoords.x, blockCoords.y, blockCoords.z);
    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
    return 1;
}

/// Shape:LocalToBlock(value) (A)
/// Shape:LocalToBlock(x, y, z) (B)
/// - 1: lua object w/ Shape support
/// - (A) 2 (value): Number3
/// - (B) 2, 3 & 4 (x, y, z): local pos as 3 numbers
/// returns: transformed Number3
static int _shape_localToBlock(lua_State * const L) {
    RETURN_VALUE_IF_NULL(L, 0)
    LUAUTILS_STACK_SIZE(L)
    
    Shape *s = nullptr;
    float3 local;
    if (_shape_blockConversions_checkAndGetArg(L, "Shape:LocalToBlock", &local, &s, false) == false) {
        lua_error(L); // returns
        // return 0; // return statement not to confuse code analyzer
    }
    
    float3 blockCoords = shape_local_to_block(s, local.x, local.y, local.z);
    
    lua_number3_pushNewObject(L, blockCoords.x, blockCoords.y, blockCoords.z);
    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
    return 1;
}

/// Shape:ComputeLocalBoundingBox()
/// returns: Box
static int _shape_getLocalAABB(lua_State * const L) {
    RETURN_VALUE_IF_NULL(L, 0)
    LUAUTILS_STACK_SIZE(L)
    
    const int argCount = lua_gettop(L);
    if (argCount < 1 || lua_shape_isShape(L, 1) == false) {
        LUAUTILS_ERROR(L, "Shape:ComputeLocalBoundingBox - function should be called with `:`");
    }
    
    if (argCount != 1) {
        LUAUTILS_ERROR(L, "Shape:ComputeLocalBoundingBox - no argument needed"); // returns
    }
    
    Shape *s = lua_shape_getShapePtr(L, 1);
    if (s == nullptr) {
        LUAUTILS_INTERNAL_ERROR(L)
    }

    shape_apply_current_transaction(s, true);
    Box local; shape_get_local_aabb(s, &local);
    
    lua_box_standalone_pushNewTable(L, &local.min, &local.max, false);
    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
    return 1;
}

/// Shape:ComputeWorldBoundingBox()
/// returns: Box
static int _shape_getWorldAABB(lua_State * const L) {
    RETURN_VALUE_IF_NULL(L, 0)
    LUAUTILS_STACK_SIZE(L)
    
    const int argCount = lua_gettop(L);
    if (argCount < 1 || lua_shape_isShape(L, 1) == false) {
        LUAUTILS_ERROR(L, "Shape:ComputeWorldBoundingBox - function should be called with `:`");
    }
    
    if (argCount != 1) {
        LUAUTILS_ERROR(L, "Shape:ComputeWorldBoundingBox - no argument needed"); // returns
    }
    
    Shape *s = lua_shape_getShapePtr(L, 1);
    if (s == nullptr) {
        LUAUTILS_INTERNAL_ERROR(L)
    }

    shape_apply_current_transaction(s, true);
    Box world; shape_get_world_aabb(s, &world, true);
    
    lua_box_standalone_pushNewTable(L, &world.min, &world.max, false);
    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
    return 1;
}

// TODO: move this to lua_mutableShape
// mutableShape:AddPoint("key", coords, rotation))
// coords: Number3 (optional, {0,0,0} by default)
// rotation: Number3 (optional, {0,0,0} by default)
static int _shape_addPoint(lua_State * const L) {
    vx_assert(L != nullptr);
    LUAUTILS_STACK_SIZE(L)
    
    // Validate arguments count
    const int argCount = lua_gettop(L);
    
    if (argCount < 1 || lua_shape_isShape(L, 1) == false) {
        LUAUTILS_ERROR(L, "Shape:AddPoint - function should be called with `:`");
    }
    
    if (argCount < 2 || argCount > 4) {
        LUAUTILS_ERROR(L, "Shape:AddPoint incorrect argument count");
    }
    
    // Validate 1st arg
    if (lua_isstring(L, 2) == false) {
        LUAUTILS_ERROR(L, "Shape:AddPoint 1st argument should be a string");
    }
    
    Shape *s = lua_shape_getShapePtr(L, 1);
    const std::string name(lua_tostring(L, 2));
    
    float3 coords = float3_zero;
    float3 rotation = float3_zero;
    
    if (argCount >= 3) {
        if (lua_number3_or_table_getXYZ(L, 3, &coords.x, &coords.y, &coords.z) == false) {
            LUAUTILS_ERROR(L, "Shape:AddPoint - 2nd argument should be a Number3");
        }
    }
    
    if (argCount == 4) {
        Quaternion q;
        if (lua_rotation_or_table_get(L, 4, &q) == false) {
            float3 euler;
            if (lua_number3_getXYZ(L, 4, &euler.x, &euler.y, &euler.z)) {
                euler_to_quaternion_vec(&euler, &q);
            } else {
                LUAUTILS_ERROR(L, "Shape:AddPoint - 3rd argument should be a Rotation");
            }
        }
        quaternion_to_euler(&q, &rotation);
    }
    
    lua_point_push(L, name, s, true);
    
    if (lua_isnil(L, -1)) {
        LUAUTILS_STACK_SIZE_ASSERT(L, 1)
        return 0;
    }
    
    lua_point_setValues(L, -1,
                        &(coords.x),
                        &(coords.y),
                        &(coords.z),
                        &(rotation.x),
                        &(rotation.y),
                        &(rotation.z));
    
    // TODO: remove this later
    // clean-up legacy "origin" POI name if present
    if (name == POINT_OF_INTEREST_HAND) {
        shape_remove_point(s, POINT_OF_INTEREST_ORIGIN);
    }
    
    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
    return 1;
}

// myPoint = shape:GetPoint("Hand")
// arguments:
// 1 - Shape
// 2 - Point name
static int _shape_getPoint(lua_State * const L) {
    vx_assert(L != nullptr);
    LUAUTILS_STACK_SIZE(L)
    
    // Validate arguments count
    const int argCount = lua_gettop(L);
    if (argCount != 2) {
        LUAUTILS_ERROR(L, "Shape:GetPoint incorrect argument count");
    }
    
    // Validate caller's type
    if (lua_shape_isShape(L, 1) == false) {
        LUAUTILS_ERROR(L, "Shape:GetPoint incompatible caller (should be a Shape or MutableShape)");
    }
    Shape *s = lua_shape_getShapePtr(L, 1);
    
    // if name is nil, return nil
    if (lua_isnil(L, 2) == true) {
        lua_pushnil(L);
        LUAUTILS_STACK_SIZE_ASSERT(L, 1)
        return 1;
    }
    
    if (lua_isstring(L, 2) == false) {
        LUAUTILS_ERROR(L, "Shape:GetPoint argument should be a string");
    }
    
    std::string name(lua_tostring(L, 2));
    
    // automatically support legacy POI name POINT_OF_INTEREST_ORIGIN
    //    if (name == POINT_OF_INTEREST_HAND &&
    //        shape_get_point_of_interest(s, POINT_OF_INTEREST_HAND) == nullptr &&
    //        shape_get_point_of_interest(s, POINT_OF_INTEREST_ORIGIN) != nullptr) {
    //        name = POINT_OF_INTEREST_ORIGIN;
    //    }
    // TODO: update "origin" name to "Hand" and use it.
    
    lua_point_push(L, name, s, false);
    
    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
    return 1;
}

static int _shape_refreshModel(lua_State * const L) {
    vx_assert(L != nullptr);
    LUAUTILS_STACK_SIZE(L)

    if (lua_shape_isShape(L, 1) == false) {
        LUAUTILS_ERROR(L, "Shape:RefreshModel incompatible caller");
    }
    Shape *s = lua_shape_getShapePtr(L, 1);

    shape_apply_current_transaction(s, true);
    shape_reset_box(s);
    shape_refresh_all_vertices(s);

    LUAUTILS_STACK_SIZE_ASSERT(L, 0)
    return 0;
}

static int _shape_bakeLight(lua_State * const L) {
    vx_assert(L != nullptr);
    LUAUTILS_STACK_SIZE(L)

    const int argCount = lua_gettop(L);
    if (argCount < 1 || argCount > 2) {
        LUAUTILS_ERROR(L, "Shape:ComputeBakedLight - incorrect argument count");
    }

    if (lua_shape_isShape(L, 1) == false) {
        LUAUTILS_ERROR(L, "Shape:ComputeBakedLight - incompatible caller");
    }
    Shape *s = lua_shape_getShapePtr(L, 1);

    if (shape_is_model_locked(s)) {
        LUAUTILS_ERROR(L, "Shape:ComputeBakedLight - previous computation hasn't finished yet");
    }

    bool useCallback = false;
    if (argCount > 1) {
        if (lua_isfunction(L, 2)) {
            useCallback = true;
        } else {
            LUAUTILS_ERROR(L, "Shape:ComputeBakedLight - argument must be a callback function")
        }
    }

    // execute callback immediately if baked lighting is up-to-date
    if (shape_uses_baked_lighting(s) &&
        color_palette_is_lighting_dirty(shape_get_palette(s)) == false) {

        if (useCallback) {
            lua_pushvalue(L, 2); // put callback on top
            lua_pcall(L, 0, 0, 0);
        }

        LUAUTILS_STACK_SIZE_ASSERT(L, 0)
        return 0;
    }
    // or if a baked file can be successfully loaded
#if GLOBAL_LIGHTING_BAKE_LOAD_ENABLED
    if (cache_load_or_clear_baked_file(s)) {
        shape_refresh_all_vertices(s);

        if (useCallback) {
            lua_pushvalue(L, 2); // put callback on top
            lua_pcall(L, 0, 0, 0);
        }

        LUAUTILS_STACK_SIZE_ASSERT(L, 0)
        return 0;
    }
#endif

    vx::Game *cppGame = nullptr;
    if (lua_utils_getGameCppWrapperPtr(L, &cppGame) == false || cppGame == nullptr) {
        LUAUTILS_INTERNAL_ERROR(L);
        // return 0; // return statement not to confuse code analyzer
    }
    vx::Game_WeakPtr weakGame = cppGame->getWeakPtr();

    int callbackIndex = -1;
    if (useCallback) {
        // store callback function
        lua_getglobal(L, "Shape");
        const int type = LUA_GET_METAFIELD_AND_RETURN_TYPE(L, -1, LUA_G_SHAPE_METAFIELD_BAKECALLBACKS);
        if (type != LUA_TTABLE) {
            LUAUTILS_INTERNAL_ERROR(L);
        }
        lua_Integer len = lua_objlen(L, -1);
        callbackIndex = static_cast<int>(len + 1);

        lua_pushinteger(L, callbackIndex); // key (first available index)
        lua_pushvalue(L, 2);           // value (callback function)
        lua_settable(L, -3);
        lua_pop(L, 1);                  // pop array of callbacks
        lua_pop(L, 1);                  // pop "Shape" global table
    }

    shape_set_model_locked(s, true);
    shape_retain(s);

    vx::OperationQueue::getSlowBackground()->dispatch([s, callbackIndex, weakGame]() {
        shape_compute_baked_lighting(s);

        vx::OperationQueue::getMain()->dispatch([s, callbackIndex, weakGame]() {
            shape_set_model_locked(s, false);
            
            vx::Game_SharedPtr strongGame = weakGame.lock();
            if (strongGame == nullptr) {
                shape_release(s);
                return;
            }

#if GLOBAL_LIGHTING_BAKE_SAVE_ENABLED
            if (shape_uses_baked_lighting(s)) {
                cache_save_baked_file(s);
            }
#endif
            // we don't need the shape anymore
            shape_release(s);

            // call Lua callback function if any
            if (callbackIndex >= 0) {
                lua_State *L = strongGame->getLuaState();
                if (L == nullptr) {
                    return;
                }

                LUAUTILS_STACK_SIZE(L)

                lua_getglobal(L, "Shape");
                const int type = LUA_GET_METAFIELD_AND_RETURN_TYPE(L, -1, LUA_G_SHAPE_METAFIELD_BAKECALLBACKS);
                if (type != LUA_TTABLE) {
                    LUAUTILS_INTERNAL_ERROR(L);
                }
                lua_pushinteger(L, callbackIndex);
                lua_gettable(L, -2); // put callback function on top of stack

                // -1 : callback function
                // -2 : callbacks table

                // remove callback function from the callbacks array table
                lua_pushinteger(L, callbackIndex); // key (first available index)
                lua_pushnil(L);
                lua_settable(L, -4);

                lua_remove(L, -2); // remove array of callbacks
                lua_remove(L, -2); // remove "Shape" global table

                // callback function is on the top of the stack
                const int state = lua_pcall(L, 0, 0, 0);
                if (state != LUA_OK) {
                    if (lua_utils_isStringStrict(L, -1)) {
                        lua_log_error_CStr(L, lua_tostring(L, -1));
                    } else {
                        lua_log_error(L, "Shape:Load callback error");
                    }
                    lua_pop(L, 1); // pops the error
                }

                LUAUTILS_STACK_SIZE_ASSERT(L, 0)
            }
        });
    });

    LUAUTILS_STACK_SIZE_ASSERT(L, 0)
    return 0;
}

static int _shape_clearLight(lua_State * const L) {
    vx_assert(L != nullptr);
    LUAUTILS_STACK_SIZE(L)

    const int argCount = lua_gettop(L);
    if (argCount != 1) {
        LUAUTILS_ERROR(L, "Shape:ClearBakedLight - incorrect argument count");
    }

    if (lua_shape_isShape(L, 1) == false) {
        LUAUTILS_ERROR(L, "Shape:ClearBakedLight - incompatible caller");
    }
    Shape *s = lua_shape_getShapePtr(L, 1);

    if (shape_is_model_locked(s)) {
        LUAUTILS_ERROR(L, "Shape:ClearBakedLight - previous computation hasn't finished yet");
    }

    shape_clear_baked_lighing(s);
    shape_refresh_all_vertices(s);

    LUAUTILS_STACK_SIZE_ASSERT(L, 0)
    return 0;
}

// Lua function
// Args:
//   1 : Shape object
//   2 : string (<itemRepo>.<itemName> OR <itemName> for official items)
//   3 : Palette (optional)
// LATER: item repo / item name / item tag ?
static int _shape_save(lua_State * const L) {
    vx_assert(L != nullptr);
    LUAUTILS_STACK_SIZE(L)

    const int argCount = lua_gettop(L);

    if (argCount < 1 || lua_shape_isShape(L, 1) == false) {
        LUAUTILS_ERROR(L, "Shape:Save - function should be called with `:`")
    }

    if (argCount > 4) {
        LUAUTILS_ERROR(L, "Shape:Save - 1st argument should be a string")
    }

    if (lua_utils_isStringStrict(L, 2) == false) {
        LUAUTILS_ERROR(L, "Shape:Save - 1st argument should be a string")
    }

    if (argCount >= 3 && lua_utils_getObjectType(L, 3) != ITEM_TYPE_PALETTE && lua_isnil(L, 3) == false) {
        LUAUTILS_ERROR(L, "Shape:Save - 2nd argument should be a palette")
    }

    if (argCount >= 4 && lua_isstring(L, 4) == false) {
        LUAUTILS_ERROR(L, "Shape:Save - 3rd argument should be a string")
    }

    // Retrieve C Shape
    Shape* shape = lua_shape_getShapePtr(L, 1);
    if (shape == nullptr) {
        LUAUTILS_STACK_SIZE_ASSERT(L, 0)
        return 0;
    }

    // Retrieve item name
    std::string itemRepoAndName(lua_tostring(L, 2));
    if (itemRepoAndName.empty()) {
        LUAUTILS_ERROR(L, "Shape:Save - 1st argument should be a non empty string")
    }
    std::vector<std::string> elems = vx::str::splitString(itemRepoAndName, ".");
    if (elems.size() != 2) {
        LUAUTILS_ERROR(L, "Shape:Save - 1st argument should be a string <repo>.<name>")
    }

    // Retrieve C Palette
    ColorPalette *artistPalette = nullptr;
    if (argCount >= 3 && lua_utils_getObjectType(L, 3) == ITEM_TYPE_PALETTE) {
        artistPalette = lua_palette_getCPointer(L, 3, nullptr);
        if (artistPalette == nullptr) {
            LUAUTILS_INTERNAL_ERROR(L)
        }
    }

    std::string category = "";
    if (argCount >= 4) {
        category = lua_tostring(L, 4);
    }
    
    // retrieve Game from Lua state
    CGame *g = nullptr;
    if (lua_utils_getGamePtr(L, &g) == false || g == nullptr) {
        LUAUTILS_INTERNAL_ERROR(L)
    }

//    NOTE: not saving shape thumbnails anymore (as of 0.0.51)
//    screenshot needs to happen immediately for line-by-line usage in Lua (i.e. setting up objects,
//    calling screenshot, resetting objects all in one go), making a full refresh necessary
//    scene_standalone_refresh(game_get_scene(g));
//    take screenshot & save to app storage
//    uint16_t w, h;
//    size_t thumbnailBytesCount = 0;
//    void *thumbnailBytes = nullptr;
//    vx::GameRenderer *renderer = vx::VXApplication::getInstance()->getGameRenderer();
//
//    renderer->screenshot(&thumbnailBytes,
//                         &w,
//                         &h,
//                         &thumbnailBytesCount,
//                         "itemEditorScreenshot",
//                         true, // out png
//                         true, // no bg
//                         256, // max width (pixels)
//                         256, // max height (pixels)
//                         false, // unflip
//                         true, // trim
//                         vx::GameRenderer::EnforceRatioStrategy::padding, // strategy to enforce ratio
//                         1.0f); // enforced ratio

    // create .3zh byte buffer
    void *buffer = nullptr;
    uint32_t bufferSize = 0;
    const bool ok = serialization_save_shape_as_buffer(shape,
                                                       artistPalette,
                                                       nullptr,
                                                       0,
                                                       &buffer,
                                                       &bufferSize);
    if (ok == false) {
        LUAUTILS_INTERNAL_ERROR(L)
    }

    const std::string itemRepoStr = elems.at(0);
    const std::string itemNameStr = elems.at(1);
    const std::string bytes(static_cast<char *>(buffer), bufferSize);
    free(buffer);

    vx::HubClient& client = vx::HubClient::getInstance();
    client.setItem3ZH(client.getUniversalSender(),
                      itemRepoStr,
                      itemNameStr,
                      category,
                      bytes, [itemRepoStr, itemNameStr](const bool &success){
        if (success) {
            vxlog_info("[💾] item save : success");
            // invalidate local cache for this item
            vx::OperationQueue::getMain()->dispatch([itemRepoStr, itemNameStr](){
                CacheErr err = cacheCPP_expireCacheForItem(itemRepoStr, itemNameStr);
                if (err != nullptr) {
                    vxlog_info("[💾] cache update failed: %s", err->c_str());
                }
            });
        } else {
            // TODO: gaetan: maybe we should return an error
            vxlog_error("[💾] item save : failure");
        }
    });

    LUAUTILS_STACK_SIZE_ASSERT(L, 0);
    return 0;
}

int _shape_FitToScreen(lua_State * const L) {
    LUAUTILS_ERROR(L, "Shape:FitToScreen has been removed, please use Camera:FitToScreen instead"); // returns
    return 0;
}

int _shape_PrivateRtreeCheck(lua_State * const L) {
    RETURN_VALUE_IF_NULL(L, 0)
    LUAUTILS_STACK_SIZE(L)

    Shape* shape = lua_shape_getShapePtr(L, 1);
    if (shape == nullptr) {
        LUAUTILS_ERROR(L, "[Shape:PrivateRtreeCheck] incorrect caller, expected Shape")
    }

    int onlyLevel = -1;
    if (lua_isnumber(L, 2)) {
        onlyLevel = static_cast<int>(lua_tonumber(L, 2));
    }

    vx::GameRenderer::getGameRenderer()->debug_checkAndDrawShapeRtree(
        shape_get_transform(shape), shape, onlyLevel);

    LUAUTILS_STACK_SIZE_ASSERT(L, 0)
    return 0;
}
