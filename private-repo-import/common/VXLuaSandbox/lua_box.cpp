//
//  lua_box.cpp
//  Cubzh
//
//  Created by Xavier Legland on 24/09/2021.
//  Copyright © 2020 Voxowl Inc. All rights reserved.
//

#include "lua_box.hpp"

#include <float.h>

// lua sandbox
#include "lua.hpp"
#include "lua_utils.hpp"
#include "sbs.hpp"
#include "lua_collision_groups.hpp"
#include "lua_object.hpp"
#include "lua_impact.hpp"

// engine
#include "box.h"
#include "float3.h"
#include "config.h"
#include "game.h"

#include "VXGameRenderer.hpp"

#define LUA_BOX_FIELD_MIN       "Min"     // Number3
#define LUA_BOX_FIELD_MAX       "Max"     // Number3
#define LUA_BOX_FIELD_COPY      "Copy"    // function
#define LUA_BOX_FIELD_CAST      "Cast"    // function
#define LUA_BOX_FIELD_CENTER    "Center"  // Number3 (read-only)
#define LUA_BOX_FIELD_FIT       "Fit"     // function
#define LUA_BOX_FIELD_MERGE     "Merge"   // function
#define LUA_BOX_FIELD_SIZE      "Size"    // Number3
#define LUA_BOX_FIELD_WIDTH     "Width"   // number
#define LUA_BOX_FIELD_HEIGHT    "Height"  // number
#define LUA_BOX_FIELD_DEPTH     "Depth"   // number

// --------------------------------------------------
//
// MARK: - Static functions' prototypes -
//
// --------------------------------------------------

static int _g_box_index(lua_State *L);
static int _g_box_call(lua_State *L);
static int _box_index(lua_State *L);
static int _box_newindex(lua_State *L);
static void _box_gc(void *_ud);
static int _box_copy(lua_State *L);
static int _box_cast(lua_State *L);
static int _box_fit(lua_State *L);
static int _box_merge(lua_State *L);

void _lua_box_set_min_n3_handler(const float *x, const float *y, const float *z,
                              lua_State *L, const number3_C_handler_userdata *userdata);
void _readonly_lua_box_set_min_n3_handler(const float *x, const float *y, const float *z,
                              lua_State *L, const number3_C_handler_userdata *userdata);
void _lua_box_get_min_n3_handler(float *x, float *y, float *z,
                              lua_State *L, const number3_C_handler_userdata *userdata);
void _lua_box_set_max_n3_handler(const float *x, const float *y, const float *z,
                                 lua_State *L, const number3_C_handler_userdata *userdata);
void _readonly_lua_box_set_max_n3_handler(const float *x, const float *y, const float *z,
                                          lua_State *L, const number3_C_handler_userdata *userdata);
void _lua_box_get_max_n3_handler(float *x, float *y, float *z,
                              lua_State *L, const number3_C_handler_userdata *userdata);

// --------------------------------------------------
//
// MARK: - C handlers
//
// --------------------------------------------------

void _set_box_handler(const float3 * const min,
                      const float3 * const max,
                      lua_State *L,
                      void *userdata) {
    Box *box = static_cast<Box *>(userdata);
    vx_assert(box != nullptr);

    if (min != nullptr) {
        box->min = *min;
    }
    if (max != nullptr) {
        box->max = *max;
    }
}

/// does nothing
void _readonly_set_box_handler(const float3 *min,
                               const float3 *max,
                               lua_State *L,
                               void *userdata) {
    LUAUTILS_ERROR(L, "This box is read-only");
}

void _get_box_handler(float3 *min,
                      float3 *max,
                      lua_State *L,
                      const void *userdata) {
    const Box *box = static_cast<const Box *>(userdata);

    if (min != nullptr) *min = box->min;
    if (max != nullptr) *max = box->max;
}

void _free_box_handler(void *userdata) {
    Box *box = static_cast<Box *>(userdata);
    box_free(box);
}

typedef struct box_userdata {
    box_C_handler *handler;
} box_userdata;

// --------------------------------------------------
//
// MARK: - Exposed functions -
//
// --------------------------------------------------

bool lua_g_box_create_and_push(lua_State *L) { 
    vx_assert(L != nullptr);
    
    LUAUTILS_STACK_SIZE(L)

    lua_newuserdata(L, 1);
    lua_newtable(L); // metatable
    {
        lua_pushstring(L, "__index");
        lua_pushcfunction(L, _g_box_index, "");
        lua_rawset(L, -3);

        lua_pushstring(L, "__call");
        lua_pushcfunction(L, _g_box_call, "");
        lua_rawset(L, -3);

        lua_pushstring(L, "__metatable");
        lua_pushboolean(L, false);
        lua_rawset(L, -3);

        lua_pushstring(L, "__type");
        lua_pushstring(L, "BoxInterface");
        lua_rawset(L, -3);

        lua_pushstring(L, "__typen");
        lua_pushinteger(L, ITEM_TYPE_G_BOX);
        lua_rawset(L, -3);
    }
    lua_setmetatable(L, -2);

    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
    return true;
}

void lua_box_pushNewTable(lua_State * const L,
                          void * const userdata,
                          box_C_handler_set setFunction,
                          box_C_handler_get getFunction,
                          box_C_handler_free freeFunction,
                          number3_C_handler_set n3MinSetFunction,
                          number3_C_handler_get n3MinGetFunction,
                          number3_C_handler_set n3MaxSetFunction,
                          number3_C_handler_get n3MaxGetFunction) {
    vx_assert(L != nullptr);
    LUAUTILS_STACK_SIZE(L)
    
    vx::Game *g = nullptr;
    if (lua_utils_getGameCppWrapperPtr(L, &g) == false) {
        LUAUTILS_INTERNAL_ERROR(L);
    }

    box_C_handler *handler = static_cast<box_C_handler *>(malloc(sizeof(box_C_handler)));
    handler->userdata = userdata;
    handler->set = setFunction;
    handler->get = getFunction;
    handler->free_userdata = freeFunction;
    handler->wptr = weakptr_new(handler);

    box_userdata *ud = static_cast<box_userdata *>(lua_newuserdatadtor(L, sizeof(box_userdata), _box_gc));
    ud->handler = handler;

    lua_newtable(L); // Box instance metatable
    {
        lua_pushstring(L, "__index");
        lua_pushcfunction(L, _box_index, "");
        lua_rawset(L, -3);

        lua_pushstring(L, "__newindex");
        lua_pushcfunction(L, _box_newindex, "");
        lua_rawset(L, -3);

        // Number3 handlers
        number3_C_handler_userdata n3Userdata = number3_C_handler_userdata_zero;
        n3Userdata.ptr = handler->wptr;
        
        lua_pushstring(L, LUA_BOX_FIELD_MIN);
        lua_number3_pushNewObject(L, 0.0f, 0.0f, 0.0f);
        weakptr_retain(handler->wptr);
        lua_number3_setHandlers(L, -1,
                                n3MinSetFunction,
                                n3MinGetFunction,
                                n3Userdata, true);
        lua_rawset(L, -3);
        
        lua_pushstring(L, LUA_BOX_FIELD_MAX);
        lua_number3_pushNewObject(L, 0.0f, 0.0f, 0.0f);
        weakptr_retain(handler->wptr);
        lua_number3_setHandlers(L, -1,
                                n3MaxSetFunction,
                                n3MaxGetFunction,
                                n3Userdata, true);
        lua_rawset(L, -3);
        
        // hide the metatable from the sandbox user
        lua_pushstring(L, "__metatable");
        lua_pushboolean(L, false);
        lua_rawset(L, -3);

        lua_pushstring(L, "__type");
        lua_pushstring(L, "Box");
        lua_rawset(L, -3);

        lua_pushstring(L, "__typen");
        lua_pushinteger(L, ITEM_TYPE_BOX);
        lua_rawset(L, -3);
    }
    lua_setmetatable(L, -2);

    LUAUTILS_STACK_SIZE_ASSERT(L, 1);
}

void lua_box_standalone_pushNewTable(lua_State * const L,
                                     const float3 * const min,
                                     const float3 * const max,
                                     const bool& readonly) {
    vx_assert(L != nullptr);
    vx_assert(min != nullptr);
    vx_assert(max != nullptr);
    LUAUTILS_STACK_SIZE(L)
    
    Box *box = box_new_2(min->x, min->y, min->z, max->x, max->y, max->z);
    lua_box_pushNewTable(L, static_cast<void *>(box),
                         readonly ? _readonly_set_box_handler : _set_box_handler,
                         _get_box_handler,
                         _free_box_handler,
                         readonly ? _readonly_lua_box_set_min_n3_handler : _lua_box_set_min_n3_handler,
                         _lua_box_get_min_n3_handler,
                         readonly ? _readonly_lua_box_set_max_n3_handler : _lua_box_set_max_n3_handler,
                        _lua_box_get_max_n3_handler);
    LUAUTILS_STACK_SIZE_ASSERT(L, 1);
}

box_C_handler *lua_box_getCHandler(lua_State *L, const int luaBoxIdx) {
    box_userdata *ud = static_cast<box_userdata*>(lua_touserdata(L, luaBoxIdx));
    return ud->handler;
}

bool lua_box_get_box(lua_State *L,
                     const int luaBoxIdx,
                     Box *box) {
    vx_assert(L != nullptr);

    LUAUTILS_STACK_SIZE(L)

    if (lua_utils_getObjectType(L, luaBoxIdx) != ITEM_TYPE_BOX) {
        LUAUTILS_STACK_SIZE_ASSERT(L, 0)
        return false;
    }

    box_userdata *ud = static_cast<box_userdata*>(lua_touserdata(L, luaBoxIdx));
    box_C_handler *handler = ud->handler;
    
    if (handler->get != nullptr) {
        float3 min = float3_zero;
        float3 max = float3_zero;
        handler->get(&min, &max, L, handler->userdata);
        box->min = min;
        box->max = max;
        
        LUAUTILS_STACK_SIZE_ASSERT(L, 0)
        return true;
    }
    
    LUAUTILS_STACK_SIZE_ASSERT(L, 0)
    return false;
}

int lua_g_box_snprintf(lua_State *L,
                       char *result,
                       size_t maxLen,
                       bool spacePrefix) {
    vx_assert(L != nullptr);
    return snprintf(result, maxLen, "%s[Box (global)]",
                    spacePrefix ? " " : "");
}

int lua_box_snprintf(lua_State *L,
                     char *result,
                     size_t maxLen,
                     bool spacePrefix) {
    vx_assert(L != nullptr);

    Box box = box_zero;
    if (lua_box_get_box(L, -1, &box) == false) {
        LUAUTILS_INTERNAL_ERROR(L)
    }

    return snprintf(result, maxLen, "%s[Box min X: %.2f Y: %.2f Z: %.2f, Box max X: %.2f Y: %.2f Z: %.2f]",
                    spacePrefix ? " " : "",
                    static_cast<double>(box.min.x),
                    static_cast<double>(box.min.y),
                    static_cast<double>(box.min.z),
                    static_cast<double>(box.max.x),
                    static_cast<double>(box.max.y),
                    static_cast<double>(box.max.z));
}

// --------------------------------------------------
//
// MARK: - static functions
//
// --------------------------------------------------

static int _g_box_index(lua_State *L) {
    lua_pushnil(L);
    return 1;
}

// arguments:
// none: min = {0, 0, 0} and max = {0, 0, 0}
// OR
// Number3 min, Number3 max
static int _g_box_call(lua_State *L) {
    vx_assert(L != nullptr);
    LUAUTILS_STACK_SIZE(L)

    const int argCount = lua_gettop(L);

    if (argCount == 1) {
        float3 zero = float3_zero;
        lua_box_standalone_pushNewTable(L, &zero, &zero, false);

    } else {
        float3 n1 = float3_zero;
        float3 n2 = float3_zero;

        if (lua_number3_or_table_getXYZ(L, 2, &(n1.x), &(n1.y), &(n1.z)) == false ||
            lua_number3_or_table_getXYZ(L, 3, &(n2.x), &(n2.y), &(n2.z)) == false) {
            LUAUTILS_ERROR(L, "Box: arguments 1 and 2 must be Number3");
        }

        // automatically reverse memberwise min/max to correctly define box
        const float3 min = float3_mmin2(&n1, &n2);
        const float3 max = float3_mmax2(&n1, &n2);

        lua_box_standalone_pushNewTable(L, &min, &max, false);
    }

    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
    return 1;
}

static int _box_index(lua_State *L) {
    vx_assert(L != nullptr);
    LUAUTILS_ASSERT_ARGS_COUNT(L, 2)
    LUAUTILS_ASSERT_ARG_TYPE(L, 1, ITEM_TYPE_BOX)
    LUAUTILS_STACK_SIZE(L)

    const char *key = lua_tostring(L, 2);

    if (strcmp(key, LUA_BOX_FIELD_MIN) == 0) {
        
        // TMP
        float3 minValue = float3_zero;
        box_C_handler * const handler = lua_box_getCHandler(L, 1);
        handler->get(&minValue, nullptr, L, handler->userdata);
        lua_number3_pushNewObject(L, minValue.x, minValue.y, minValue.z);

    } else if (strcmp(key, LUA_BOX_FIELD_MAX) == 0) {
        
        // TMP
        float3 maxValue = float3_zero;
        box_C_handler * const handler = lua_box_getCHandler(L, 1);
        handler->get(nullptr, &maxValue, L, handler->userdata);
        lua_number3_pushNewObject(L, maxValue.x, maxValue.y, maxValue.z);

    } else if (strcmp(key, LUA_BOX_FIELD_COPY) == 0) {
        lua_pushcfunction(L, _box_copy, "");

    } else if (strcmp(key, LUA_BOX_FIELD_CAST) == 0) {
        lua_pushcfunction(L, _box_cast, "");
    
    } else if (strcmp(key, LUA_BOX_FIELD_CENTER) == 0) {
        box_C_handler * const handler = lua_box_getCHandler(L, 1);
        if (handler != nullptr) {
            float3 min, max;
            handler->get(&min, &max, L, handler->userdata);
    
            float3 center = {
                (max.x - min.x) * .5f + min.x,
                (max.y - min.y) * .5f + min.y,
                (max.z - min.z) * .5f + min.z
            };
            lua_number3_pushNewObject(L, center.x, center.y, center.z);
        } else {
            lua_pushnil(L);
        }
    } else if (strcmp(key, LUA_BOX_FIELD_FIT) == 0) {
        lua_pushcfunction(L, _box_fit, "");
    } else if (strcmp(key, LUA_BOX_FIELD_MERGE) == 0) {
        lua_pushcfunction(L, _box_merge, "");
    } else if (strcmp(key, LUA_BOX_FIELD_SIZE) == 0 || strcmp(key, LUA_BOX_FIELD_WIDTH) == 0
        || strcmp(key, LUA_BOX_FIELD_HEIGHT) == 0 || strcmp(key, LUA_BOX_FIELD_DEPTH) == 0) {

        box_C_handler *handler = lua_box_getCHandler(L, 1);
        if (handler != nullptr) {
            float3 min, max;
            handler->get(&min, &max, L, handler->userdata);

            float3 size = {
                max.x - min.x,
                max.y - min.y,
                max.z - min.z
            };
            if (key[0] == LUA_BOX_FIELD_SIZE[0]) {
                lua_number3_pushNewObject(L, size.x, size.y, size.z);
            } else if (key[0] == LUA_BOX_FIELD_WIDTH[0]) {
                lua_pushnumber(L, size.x);
            } else if (key[0] == LUA_BOX_FIELD_HEIGHT[0]) {
                lua_pushnumber(L, size.y);
            } else {
                lua_pushnumber(L, size.z);
            }
        } else {
            lua_pushnil(L);
        }
    } else {
        // if key is unknown, return nil
        lua_pushnil(L);
    }

    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
    return 1; // the __index function always has 1 return value
}

static int _box_newindex(lua_State *L) {
    vx_assert(L != nullptr);
    LUAUTILS_ASSERT_ARGS_COUNT(L, 3)
    LUAUTILS_ASSERT_ARG_TYPE(L, 1, ITEM_TYPE_BOX)
    LUAUTILS_STACK_SIZE(L)
    
    // key is a capitalized string
    const char *key = lua_tostring(L, 2);

    if (strcmp(key, LUA_BOX_FIELD_MIN) == 0) {
        float3 newMin = float3_zero;
        if (lua_number3_or_table_getXYZ(L, 3, &newMin.x, &newMin.y, &newMin.z) == false) {
            LUAUTILS_ERROR_F(L, "Box.%s cannot be set (argument is not a Number3)", key);
        }

        box_C_handler *handler = lua_box_getCHandler(L, 1);

        handler->set(&newMin, nullptr, L, handler->userdata);

    } else if (strcmp(key, LUA_BOX_FIELD_MAX) == 0) {
        float3 newMax = float3_zero;
        if (lua_number3_or_table_getXYZ(L, 3, &newMax.x, &newMax.y, &newMax.z) == false) {
            LUAUTILS_ERROR_F(L, "Box.%s cannot be set (argument is not a Number3)", key);
        }

        box_C_handler *handler = lua_box_getCHandler(L, 1);

        handler->set(nullptr, &newMax, L, handler->userdata);

    } else if (strcmp(key, LUA_BOX_FIELD_SIZE) == 0 || strcmp(key, LUA_BOX_FIELD_WIDTH) == 0
               || strcmp(key, LUA_BOX_FIELD_HEIGHT) == 0 || strcmp(key, LUA_BOX_FIELD_DEPTH) == 0) {

        box_C_handler *handler = lua_box_getCHandler(L, 1);

        float3 min, max;
        handler->get(&min, &max, L, handler->userdata);

        float3 newSize = {
            max.x - min.x,
            max.y - min.y,
            max.z - min.z
        };
        const bool isSize = key[0] == LUA_BOX_FIELD_SIZE[0];
        if (lua_isnumber(L, 3)) {
            if (isSize) {
                newSize.x = newSize.y = newSize.z = lua_tonumber(L, 3);
            } else if (key[0] == LUA_BOX_FIELD_WIDTH[0]) {
                newSize.x = lua_tonumber(L, 3);
            } else if (key[0] == LUA_BOX_FIELD_HEIGHT[0]) {
                newSize.y = lua_tonumber(L, 3);
            } else {
                newSize.z = lua_tonumber(L, 3);
            }
        } else if (isSize) {
            if (lua_number3_or_table_getXYZ(L, 3, &newSize.x, &newSize.y, &newSize.z) == false) {
                LUAUTILS_ERROR_F(L, "Box.%s must be a Number3 or number", key);
            }
        } else {
            LUAUTILS_ERROR_F(L, "Box.%s must be a number", key);
        }

        float3 newMax = min;
        float3_op_add(&newMax, &newSize);

        handler->set(nullptr, &max, L, handler->userdata);
    } else {
        lua_pushfstring(L, "Box.%s field is not settable", key);
    }

    LUAUTILS_STACK_SIZE_ASSERT(L, 0)
    return 0;
}

static void _box_gc(void *_ud) {
    box_userdata *ud = static_cast<box_userdata*>(_ud);
    box_C_handler *handler = ud->handler;
    vx_assert(handler != nullptr);
    if (handler->free_userdata != nullptr) {
        handler->free_userdata(handler->userdata);
    }
    weakptr_release(handler->wptr);
    free(handler);
}

// arguments:
// none
static int _box_copy(lua_State *L) {
    vx_assert(L != nullptr);
    LUAUTILS_STACK_SIZE(L)

    if (lua_gettop(L) != 1 || lua_utils_getObjectType(L, 1) != ITEM_TYPE_BOX) {
        LUAUTILS_ERROR(L, "Box:Copy() should be called with `:`");
    }

    box_userdata *ud = static_cast<box_userdata*>(lua_touserdata(L, 1));
    box_C_handler *handler = ud->handler;

    float3 min = float3_zero;
    float3 max = float3_zero;
    handler->get(&min, &max, L, handler->userdata);
    lua_box_standalone_pushNewTable(L, &min, &max, false);

    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
    return 1;
}

/// Box:Cast(unit, [max distance], [collision groups], [filter out])
/// Note: filter out takes only one Object currently
static int _box_cast(lua_State *L) {
    vx_assert(L != nullptr);
    LUAUTILS_STACK_SIZE(L)
    
    vx::Game *cppGame = nullptr;
    if (lua_utils_getGameCppWrapperPtr(L, &cppGame) == false || cppGame == nullptr) {
        LUAUTILS_INTERNAL_ERROR(L) // returns
        // return 0;
    }
    
    const int argCount = lua_gettop(L);
    if (argCount < 1 || lua_utils_getObjectType(L, 1) != ITEM_TYPE_BOX) {
        LUAUTILS_ERROR(L, "Box:Cast - function should be called with `:`");
    }
    if (argCount > 5) {
        LUAUTILS_ERROR(L, "Box:Cast - wrong number of arguments"); // returns
    }
    
    Box box;
    if (lua_box_get_box(L, 1, &box) == false) {
        LUAUTILS_INTERNAL_ERROR(L) // returns
    }
    
    float3 unit;
    if (lua_number3_getXYZ(L, 2, &unit.x, &unit.y, &unit.z) == false) {
        LUAUTILS_ERROR(L, "Box:Cast - 1st argument should be a Number3 (direction)"); // returns
    }
    
    // optional parameters default values
    float maxDist = CAMERA_DEFAULT_FAR;
    uint16_t groups = PHYSICS_GROUP_ALL_API;
    DoublyLinkedList *filterOut = nullptr;
    
    // check optional parameters, they can be skipped as long as parameters order is respected
    uint8_t param = 3;
    // max distance
    if (param <= argCount) {
        if (lua_isnumber(L, param)) {
            maxDist = lua_tonumber(L, param);
        }
        ++param;
    }
    // collision groups
    if (param <= argCount) {
        lua_collision_groups_get_mask(L, param, &groups);
        ++param;
    }
    // filter out (one Object for now)
    if (param <= argCount) {
        if (lua_object_isObject(L, param)) {
            Transform *t; lua_object_getTransformPtr(L, param, &t);
            if (t != nullptr) {
                filterOut = doubly_linked_list_new();
                doubly_linked_list_push_first(filterOut, t);
                
            }
        }
        ++param;
    }
    
    CastResult result;
    scene_cast_box(game_get_scene(cppGame->getCGame()), &box, &unit, maxDist, groups, filterOut, &result);
    
    if (filterOut != nullptr) {
        doubly_linked_list_free(filterOut);
    }
    
    lua_impact_push(L, result);
    
    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
    return 1;
}

bool _box_fit_recurse(Transform *t, void *ptr, uint32_t depth) {
    if (depth >= LUA_MAX_RECURSION_DEPTH) {
        return true; // stop recursion
    }

    transform_refresh(t, false, false); // refresh mtx for intra-frame calculations
    
    if (transform_get_type(t) == ShapeTransform) {
        Shape *s = transform_utils_get_shape(t);

        shape_apply_current_transaction(s, true);

        Box *fit = static_cast<Box*>(ptr);
        Box aabb; shape_get_world_aabb(s, &aabb, false);
        if (box_is_empty(fit)) {
            box_copy(fit, &aabb);
        } else {
            box_op_merge(fit, &aabb, fit);
        }
    } else if (transform_get_type(t) == MeshTransform) {
        Box *fit = static_cast<Box*>(ptr);
        Box aabb; mesh_get_world_aabb(static_cast<Mesh*>(transform_get_ptr(t)), &aabb, false);
        if (box_is_empty(fit)) {
            box_copy(fit, &aabb);
        } else {
            box_op_merge(fit, &aabb, fit);
        }
    }
    return false;
}

/// Box:Fit(object, [recursive])
/// Set box to fit any & all shapes BB in given object & its children if recursive.
/// Result is an AABB in object's local space
static int _box_fit(lua_State *L) {
    vx_assert(L != nullptr);
    LUAUTILS_STACK_SIZE(L)

    const int argCount = lua_gettop(L);
    if (argCount < 1 || lua_utils_getObjectType(L, 1) != ITEM_TYPE_BOX) {
        LUAUTILS_ERROR(L, "Box:Fit - function should be called with `:`");
    }
    if (argCount > 3) {
        LUAUTILS_ERROR(L, "Box:Fit - function expects up to 2 arguments");
    }

    Box box;
    if (lua_box_get_box(L, 1, &box) == false) {
        LUAUTILS_INTERNAL_ERROR(L)
    }

    // get target object
    Transform *t = nullptr;
    if (lua_object_isObject(L, 2)) {
        lua_object_getTransformPtr(L, 2, &t);
    } else {
        LUAUTILS_ERROR(L, "Box:Fit - 1st argument should be an Object");
    }
    if (t == nullptr) {
        LUAUTILS_INTERNAL_ERROR(L)
    }

    // optional parameters
    bool recursive = false, local = false;
    if (argCount >= 3) {
        if (lua_isboolean(L, 3)) { // retro-compatibility
            recursive = lua_toboolean(L, 3);
        } else if (lua_istable(L, 3)) {
            lua_getfield(L, 3, "recurse");
            if (lua_isboolean(L, -1)) {
                recursive = lua_toboolean(L, -1);
            }
            lua_pop(L, 1);

            lua_getfield(L, 3, "localBox");
            if (lua_isboolean(L, -1)) {
                local = lua_toboolean(L, -1);
            }
            lua_pop(L, 1);
        } else {
            LUAUTILS_ERROR(L, "Box:Fit - 2nd argument should be a table { recurse:boolean, localBox:boolean }");
        }
    }

    transform_refresh(t, false, true); // refresh ltw for intra-frame calculations

    box_C_handler *handler = lua_box_getCHandler(L, 1);

    Box fit;
    if (transform_get_type(t) == ShapeTransform) {
        Shape *s = transform_utils_get_shape(t);

        shape_apply_current_transaction(s, true);

        if (local) {
            shape_get_local_aabb(s, &fit);
        } else {
            shape_get_world_aabb(s, &fit, false);
        }
    } else if (transform_get_type(t) == MeshTransform) {
        if (local) {
            mesh_get_local_aabb(static_cast<Mesh*>(transform_get_ptr(t)), &fit);
        } else {
            mesh_get_world_aabb(static_cast<Mesh*>(transform_get_ptr(t)), &fit, false);
        }
    } else {
        fit = box_zero;
    }

    if (recursive) {
        if (local) {
            transform_utils_box_fit_recurse(t, *transform_get_mtx(t), &fit, true, 0, LUA_MAX_RECURSION_DEPTH);
        } else {
            transform_recurse_depth(t, _box_fit_recurse, &fit, false, false, 0);
        }
    }

    handler->set(&fit.min, &fit.max, L, handler->userdata);

    lua_pushvalue(L, 1);

    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
    return 1;
}

/// Box:Merge(box)
static int _box_merge(lua_State *L) {
    vx_assert(L != nullptr);
    LUAUTILS_STACK_SIZE(L)

    const int argCount = lua_gettop(L);
    if (argCount < 1 || lua_utils_getObjectType(L, 1) != ITEM_TYPE_BOX) {
        LUAUTILS_ERROR(L, "Box:Merge - function should be called with `:`");
    }
    if (argCount > 2) {
        LUAUTILS_ERROR(L, "Box:Merge - function expects 1 argument (Box)");
    }

    Box box1;
    if (lua_box_get_box(L, 1, &box1) == false) {
        LUAUTILS_INTERNAL_ERROR(L)
    }

    Box box2;
    if (lua_box_get_box(L, 2, &box2) == false) {
        LUAUTILS_ERROR(L, "Box:Merge - argument should be a Box");
    }

    box_C_handler *handler = lua_box_getCHandler(L, 1);

    box_op_merge(&box1, &box2, &box1);

    handler->set(&box1.min, &box1.max, L, handler->userdata);

    lua_pushvalue(L, 1);

    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
    return 1;
}

// --------------------------------------------------
//
// MARK: - Number3 handlers functions
//
// --------------------------------------------------

void _lua_box_set_min_n3_handler(const float *x, const float *y, const float *z,
                              lua_State *L, const number3_C_handler_userdata *userdata) {
    vx_assert(userdata != nullptr);
    
    float3 newMin = float3_zero;
    box_C_handler *handler = static_cast<box_C_handler *>(const_cast<void *>(userdata->ptr));
    handler->get(&newMin, nullptr, L, handler->userdata);
    
    if (x != nullptr) { newMin.x = *x; }
    if (y != nullptr) { newMin.y = *y; }
    if (z != nullptr) { newMin.z = *z; }
    
    handler->set(&newMin, nullptr, L, handler->userdata);
}

void _readonly_lua_box_set_min_n3_handler(const float *x, const float *y, const float *z,
                                          lua_State *L, const number3_C_handler_userdata *userdata) {
    vx_assert(userdata != nullptr);
    LUAUTILS_ERROR(L, "This box is read-only");
}

void _lua_box_get_min_n3_handler(float *x, float *y, float *z, lua_State *L,
                              const number3_C_handler_userdata *userdata) {
    vx_assert(userdata != nullptr);
    
    float3 minValue = float3_zero;
    box_C_handler *handler = static_cast<box_C_handler *>(const_cast<void *>(userdata->ptr));

    handler->get(&minValue, nullptr, L, handler->userdata);
    if (x != nullptr) { *x = minValue.x; }
    if (y != nullptr) { *y = minValue.y; }
    if (z != nullptr) { *z = minValue.z; }
}

void _lua_box_set_max_n3_handler(const float *x, const float *y, const float *z,
                              lua_State *L, const number3_C_handler_userdata *userdata) {
    vx_assert(userdata != nullptr);
    
    float3 newMax = float3_zero;
    box_C_handler *handler = static_cast<box_C_handler *>(const_cast<void *>(userdata->ptr));
    handler->get(nullptr, &newMax, L, handler->userdata);
    
    if (x != nullptr) { newMax.x = *x; }
    if (y != nullptr) { newMax.y = *y; }
    if (z != nullptr) { newMax.z = *z; }
    
    handler->set(nullptr, &newMax, L, handler->userdata);
}

void _readonly_lua_box_set_max_n3_handler(const float *x, const float *y, const float *z,
                                          lua_State *L, const number3_C_handler_userdata *userdata) {
    vx_assert(userdata != nullptr);
    LUAUTILS_ERROR(L, "This box is read-only");
}

void _lua_box_get_max_n3_handler(float *x, float *y, float *z, lua_State *L,
                                 const number3_C_handler_userdata *userdata) {
    vx_assert(userdata != nullptr);
    
    float3 maxValue = float3_zero;
    box_C_handler *handler = static_cast<box_C_handler *>(const_cast<void *>(userdata->ptr));

    handler->get(nullptr, &maxValue, L, handler->userdata);
    if (x != nullptr) { *x = maxValue.x; }
    if (y != nullptr) { *y = maxValue.y; }
    if (z != nullptr) { *z = maxValue.z; }
}
