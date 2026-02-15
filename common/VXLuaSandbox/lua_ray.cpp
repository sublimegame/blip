//
//  lua_ray.cpp
//  Cubzh
//
//  Created by Adrien Duermael on 10/08/2021.
//

#include "lua_ray.hpp"

#include <string.h>

#include "lua.hpp"
#include "lua_constants.h"
#include "lua_utils.hpp"

#include "lua_number3.hpp"
#include "lua_impact.hpp"
#include "lua_collision_groups.hpp"
#include "lua_shape.hpp"
#include "lua_object.hpp"

// sandbox
#include "sbs.hpp"

// engine
#include "config.h" // for vx_assert
#include "float3.h"
#include "game.h"
#include "scene.h"

/// Lua table keys for Ray
#define LUA_RAY_FIELD_ORIGIN    "Origin"    // Number3 (read-write)
#define LUA_RAY_FIELD_DIRECTION "Direction" // Number3 (read-write)
#define LUA_RAY_FIELD_FILTERIN  "FilterIn"  // Shape or CollisionGroups (read-write)
#define LUA_RAY_FIELD_FILTEROUT "FilterOut" // Object or array of Objects (read-write) (supporting only one Object for now)

// Functions
#define LUA_RAY_FIELD_CAST      "Cast"      // function (read-only)

// --------------------------------------------------
// MARK: - Static functions' prototypes -
// --------------------------------------------------

/// __index for global Ray
static int _g_ray_index(lua_State *L);

/// __new_index for global Ray
static int _g_ray_newindex(lua_State *L);

/// __call for global Ray
static int _g_ray_call(lua_State *L);

/// __index for Ray instances
static int _ray_index(lua_State *L);

/// __new_index for Ray instances
static int _ray_newindex(lua_State *L);

/// Casts a ray, returns an impact
static int _ray_cast(lua_State *L);

void lua_g_ray_create_and_push(lua_State *L) {
    vx_assert(L != nullptr);
    LUAUTILS_STACK_SIZE(L)
    
    lua_newuserdata(L, 1); // global Ray
    lua_newtable(L); // metatable
    {
        lua_pushstring(L, "__index");
        lua_pushcfunction(L, _g_ray_index, "");
        lua_rawset(L, -3);
        
        lua_pushstring(L, "__newindex");
        lua_pushcfunction(L, _g_ray_newindex, "");
        lua_rawset(L, -3);
        
        lua_pushstring(L, "__call");
        lua_pushcfunction(L, _g_ray_call, "");
        lua_rawset(L, -3);

        lua_pushstring(L, "__metatable");
        lua_pushboolean(L, false);
        lua_rawset(L, -3);

        lua_pushstring(L, "__type"); // used to log tables
        lua_pushstring(L, "RayInterface");
        lua_rawset(L, -3);

        lua_pushstring(L, "__typen"); // used to log tables
        lua_pushinteger(L, ITEM_TYPE_G_RAY);
        lua_rawset(L, -3);
    }
    lua_setmetatable(L, -2);
    
    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
    return;
}

void lua_ray_create_and_push(lua_State *L, int originIdx, int directionIdx) {
    vx_assert(L != nullptr);
    LUAUTILS_STACK_SIZE(L)
    
    lua_newuserdata(L, 1); // Ray
    lua_newtable(L); // metatable
    {
        lua_pushstring(L, "__index");
        lua_pushcfunction(L, _ray_index, "");
        lua_rawset(L, -3);
        
        lua_pushstring(L, "__newindex");
        lua_pushcfunction(L, _ray_newindex, "");
        lua_rawset(L, -3);

        lua_pushstring(L, "__metatable");
        lua_pushboolean(L, false);
        lua_rawset(L, -3);
        
        lua_pushstring(L, LUA_RAY_FIELD_ORIGIN);
        if (originIdx < 0) originIdx -= 3;
        if (lua_utils_getObjectType(L, originIdx) == ITEM_TYPE_NUMBER3) {
            lua_pushvalue(L, originIdx);
        } else {
            vxlog_error("Creating Ray with non-Number3 Origin");
            lua_pushnil(L);
        }
        lua_rawset(L, -3);

        lua_pushstring(L, LUA_RAY_FIELD_DIRECTION);
        if (directionIdx < 0) directionIdx -= 3;
        if (lua_utils_getObjectType(L, directionIdx) == ITEM_TYPE_NUMBER3) {
            lua_pushvalue(L, directionIdx);
        } else {
            vxlog_error("Creating Ray with non-Number3 Direction");
            lua_pushnil(L);
        }
        lua_rawset(L, -3);
        
        lua_pushstring(L, LUA_RAY_FIELD_FILTERIN);
        lua_collision_groups_create_and_push(L, PHYSICS_GROUP_ALL_API);
        lua_rawset(L, -3);
        
        lua_pushstring(L, LUA_RAY_FIELD_FILTEROUT);
        lua_pushnil(L);
        lua_rawset(L, -3);

        lua_pushstring(L, "__type"); // used to log tables
        lua_pushstring(L, "Ray");
        lua_rawset(L, -3);

        lua_pushstring(L, "__typen"); // used to log tables
        lua_pushinteger(L, ITEM_TYPE_RAY);
        lua_rawset(L, -3);
    }
    lua_setmetatable(L, -2);
    
    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
}

void lua_ray_create_and_push(lua_State *L, float3 origin, float3 unit) {
    vx_assert(L != nullptr);
    LUAUTILS_STACK_SIZE(L)

    lua_newuserdata(L, 1); // Ray
    lua_newtable(L); // metatable
    {
        lua_pushstring(L, "__index");
        lua_pushcfunction(L, _ray_index, "");
        lua_rawset(L, -3);

        lua_pushstring(L, "__newindex");
        lua_pushcfunction(L, _ray_newindex, "");
        lua_rawset(L, -3);

        lua_pushstring(L, "__metatable");
        lua_pushboolean(L, false);
        lua_rawset(L, -3);

        lua_pushstring(L, LUA_RAY_FIELD_ORIGIN);
        lua_number3_pushNewObject(L, origin.x, origin.y, origin.z);
        lua_rawset(L, -3);

        lua_pushstring(L, LUA_RAY_FIELD_DIRECTION);
        lua_number3_pushNewObject(L, unit.x, unit.y, unit.z);
        lua_rawset(L, -3);

        lua_pushstring(L, LUA_RAY_FIELD_FILTERIN);
        lua_collision_groups_create_and_push(L, PHYSICS_GROUP_ALL_API);
        lua_rawset(L, -3);

        lua_pushstring(L, LUA_RAY_FIELD_FILTEROUT);
        lua_pushnil(L);
        lua_rawset(L, -3);

        lua_pushstring(L, P3S_LUA_OBJ_METAFIELD_TYPE); // used to log tables
        lua_pushstring(L, "Ray");
        lua_rawset(L, -3);

        lua_pushstring(L, "__typen"); // used to log tables
        lua_pushinteger(L, ITEM_TYPE_RAY);
        lua_rawset(L, -3);
    }
    lua_setmetatable(L, -2);

    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
}

void lua_ray_push_cast_function(lua_State *L) {
    vx_assert(L != nullptr);
    LUAUTILS_STACK_SIZE(L)
    
    lua_pushcfunction(L, _ray_cast, "");
    
    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
    return;
}

int lua_g_ray_snprintf(lua_State *L,
                       char* result,
                       size_t maxLen,
                       bool spacePrefix) {
    vx_assert(L != nullptr);
    return snprintf(result, maxLen, "%s[Ray (global)]", spacePrefix ? " " : "");
}

int lua_ray_snprintf(lua_State *L,
                     char* result,
                     size_t maxLen,
                     bool spacePrefix) {
    
    vx_assert(L != nullptr);
    
    float3 origin = float3_zero;
    float3 direction = float3_zero;

    if (LUA_GET_METAFIELD_AND_RETURN_TYPE(L, -1, LUA_RAY_FIELD_ORIGIN) != LUA_TNIL) {
        lua_number3_getXYZ(L, -1, &origin.x, &origin.y, &origin.z);
        lua_pop(L, 1); // pop Origin
    }
    
    if (LUA_GET_METAFIELD_AND_RETURN_TYPE(L, -1, LUA_RAY_FIELD_DIRECTION) != LUA_TNIL) {
        lua_number3_getXYZ(L, -1, &direction.x, &direction.y, &direction.z);
        lua_pop(L, 1); // pop Direction
    }
    
    // TODO: print collision groups & filter out if set
        
    return snprintf(result,
                    maxLen,
                    "%s[Ray Origin: %.2f,%.2f,%.2f Direction: %.2f,%.2f,%.2f]",
                    spacePrefix ? " " : "",
                    static_cast<double>(origin.x),
                    static_cast<double>(origin.y),
                    static_cast<double>(origin.z),
                    static_cast<double>(direction.x),
                    static_cast<double>(direction.y),
                    static_cast<double>(direction.z));
}

// --------------------------------------------------
// MARK: - static functions
// --------------------------------------------------

static int _g_ray_index(lua_State *L) {
    // global Ray has no fields
    lua_pushnil(L);
    return 1;
}

static int _g_ray_newindex(lua_State *L) {
    LUAUTILS_ERROR(L, "Ray class is read-only");
    return 0;
}

// Ray(origin, direction, [filterIn, filterOut])
// collisionGroups: filters in collision groups
static int _g_ray_call(lua_State *L) {
    vx_assert(L != nullptr);
    
    LUAUTILS_STACK_SIZE(L)
    
    // validate and parse arguments

    int argCount = lua_gettop(L);
    if (argCount < 3 || argCount > 4) { // global Ray, origin, direction, [collisionGroups]
        LUAUTILS_ERROR(L, "Ray(origin, direction): 2 arguments needed");
    }
    
    if (lua_utils_getObjectType(L, 1) != ITEM_TYPE_G_RAY) {
        // should not happen, because __call
        // puts the table in first argument automatically
        // happens only if there's an error in the engine,
        // can't be user's fault.
        LUAUTILS_INTERNAL_ERROR(L)
    }
    
    float3 origin;
    float3 direction;
    {
        if (lua_number3_or_table_getXYZ(L, 2, &(origin.x), &(origin.y), &(origin.z)) == false) {
            LUAUTILS_ERROR(L, "Ray(origin, direction): argument 1 should be a Number3 or table of 3 numbers");
        }
        if (lua_number3_or_table_getXYZ(L, 3, &(direction.x), &(direction.y), &(direction.z)) == false) {
            LUAUTILS_ERROR(L, "Ray(origin, direction): argument 2 should be a Number3 or table of 3 numbers");
        }
    }
    
    // optional collision group filter
    if (argCount == 4) {
        // TODO: implement once collision groups are available
        lua_pop(L, 2); // pop Origin & Direction
        LUAUTILS_ERROR(L, "Ray(): collision groups filter not supported yet"); // returns
    }
    
    lua_ray_create_and_push(L, origin, direction);
    
    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
    return 1;
}

static int _ray_index(lua_State *L) {
    vx_assert(L != nullptr);
    LUAUTILS_ASSERT_ARGS_COUNT(L, 2)
    LUAUTILS_ASSERT_ARG_TYPE(L, 1, ITEM_TYPE_RAY)
    LUAUTILS_STACK_SIZE(L)
    
    const char *key = lua_tostring(L, 2);
    
    if (strcmp(key, LUA_RAY_FIELD_ORIGIN) == 0) {
        LUA_GET_METAFIELD_PUSHING_NIL_IF_NOT_FOUND(L, 1, LUA_RAY_FIELD_ORIGIN);
    } else if (strcmp(key, LUA_RAY_FIELD_DIRECTION) == 0) {
        LUA_GET_METAFIELD_PUSHING_NIL_IF_NOT_FOUND(L, 1, LUA_RAY_FIELD_DIRECTION);
    } else if (strcmp(key, LUA_RAY_FIELD_FILTERIN) == 0) {
        LUA_GET_METAFIELD_PUSHING_NIL_IF_NOT_FOUND(L, 1, LUA_RAY_FIELD_FILTERIN);
    } else if (strcmp(key, LUA_RAY_FIELD_FILTEROUT) == 0) {
        LUA_GET_METAFIELD_PUSHING_NIL_IF_NOT_FOUND(L, 1, LUA_RAY_FIELD_FILTEROUT);
    } else if (strcmp(key, LUA_RAY_FIELD_CAST) == 0) {
        lua_pushcfunction(L, _ray_cast, "");
    } else {
        lua_pushnil(L);
    }
    
    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
    return 1;
}

static int _ray_newindex(lua_State *L) {
    vx_assert(L != nullptr);
    LUAUTILS_ASSERT_ARGS_COUNT(L, 3)
    LUAUTILS_ASSERT_ARG_TYPE(L, 1, ITEM_TYPE_RAY)
    LUAUTILS_STACK_SIZE(L)
    
    const char *key = lua_tostring(L, 2);
    
    if (strcmp(key, LUA_RAY_FIELD_ORIGIN) == 0) {
        
        if (lua_utils_getObjectType(L, 3) == ITEM_TYPE_NUMBER3) {
            lua_pushvalue(L, 3);
        } else {
            float3 f3 = float3_zero;
            if (lua_number3_or_table_getXYZ(L, 3, &f3.x, &f3.y, &f3.z)) {
                lua_number3_pushNewObject(L, f3.x, f3.y, f3.z);
            } else {
                LUAUTILS_ERROR(L, "Ray.Origin can only be a Number3"); // returns
            }
        }
        
        // Number3 at -1
        
        lua_getmetatable(L, 1);
        lua_pushstring(L, LUA_RAY_FIELD_ORIGIN);
        lua_pushvalue(L, -3);
        lua_rawset(L, -3);
        
        lua_pop(L, 2); // pop metatable & Number3
        
    } else if (strcmp(key, LUA_RAY_FIELD_DIRECTION) == 0) {
        
        if (lua_utils_getObjectType(L, 3) == ITEM_TYPE_NUMBER3) {
            lua_pushvalue(L, 3);
        } else {
            float3 f3 = float3_zero;
            if (lua_number3_or_table_getXYZ(L, 3, &f3.x, &f3.y, &f3.z)) {
                lua_number3_pushNewObject(L, f3.x, f3.y, f3.z);
            } else {
                LUAUTILS_ERROR(L, "Ray.Direction can only be a Number3"); // returns
            }
        }
        
        // Number3 at -1
        
        lua_getmetatable(L, 1);
        lua_pushstring(L, LUA_RAY_FIELD_DIRECTION);
        lua_pushvalue(L, -3);
        lua_rawset(L, -3);
        
        lua_pop(L, 2); // pop metatable & Number3
        
    } else if (strcmp(key, LUA_RAY_FIELD_FILTERIN) == 0) {
        
        int filterInType = lua_utils_getObjectType(L, 3);
        
        if (lua_isnil(L, 3)) {
            
            // when filtering with nil, it means ray
            // should collide with all groups.
            lua_getmetatable(L, 1);
            lua_pushstring(L, LUA_RAY_FIELD_FILTERIN);
            lua_collision_groups_create_and_push(L, PHYSICS_GROUP_ALL_API);
            lua_rawset(L, -3);
            lua_pop(L, 1); // pop metatable
            
        } else if (filterInType == ITEM_TYPE_SHAPE ||
                   filterInType == ITEM_TYPE_MUTABLESHAPE ||
                   filterInType == ITEM_TYPE_COLLISIONGROUPS) {
            
            lua_getmetatable(L, 1);
            lua_pushstring(L, LUA_RAY_FIELD_FILTERIN);
            lua_pushvalue(L, 3); // store Shape as FilterIn
            lua_rawset(L, -3);
            lua_pop(L, 1); // pop metatable
            
        } else if (lua_istable(L, 3)) {
            
            lua_getmetatable(L, 1);
            lua_pushstring(L, LUA_RAY_FIELD_FILTERIN);
            if (lua_collision_groups_push_from_table(L, 3) == false) {
                lua_pop(L, 2); // pop metatable & string
                LUAUTILS_ERROR(L, "Ray.FilterIn can't be set to given value"); // returns
            }
            lua_rawset(L, -3);
            lua_pop(L, 1); // pop metatable
            
        } else if (lua_isnumber(L, 3)) {
            
            lua_getmetatable(L, 1);
            lua_pushstring(L, LUA_RAY_FIELD_FILTERIN);
            if (lua_collision_groups_create_and_push_with_group_number(L, static_cast<int>(lua_tonumber(L, 3))) == false) {
                lua_pop(L, 2); // pop metatable & string
                LUAUTILS_ERROR(L, "Ray.FilterIn can't be set to given value"); // returns
            }
            lua_rawset(L, -3);
            lua_pop(L, 1); // pop metatable
        } else {
            LUAUTILS_ERROR(L, "Ray.FilterIn can't be set to given value"); // returns
        }
        
    } else if (strcmp(key, LUA_RAY_FIELD_FILTEROUT) == 0) {
        
        int filterOutType = lua_utils_getObjectType(L, 3);
        
        if (lua_isnil(L, 3) ||
            filterOutType == ITEM_TYPE_SHAPE ||
            filterOutType == ITEM_TYPE_MUTABLESHAPE ||
            filterOutType == ITEM_TYPE_OBJECT ||
            filterOutType == ITEM_TYPE_MAP ||
            filterOutType == ITEM_TYPE_PLAYER) {
            
            lua_getmetatable(L, 1);
            lua_pushstring(L, LUA_RAY_FIELD_FILTEROUT);
            lua_pushvalue(L, 3); // set FilterOut
            lua_rawset(L, -3);
            lua_pop(L, 1); // pop metatable
        }
        
    } else {
        LUAUTILS_ERROR_F(L, "Ray.%s is not settable", key); // returns
    }
    
    LUAUTILS_STACK_SIZE_ASSERT(L, 0)
    return 0;
}

static int _ray_cast(lua_State *L) {
    vx_assert(L != nullptr);
    LUAUTILS_STACK_SIZE(L)
    
    const int argCount = lua_gettop(L);

    if (argCount < 1 || lua_utils_getObjectType(L, 1) != ITEM_TYPE_RAY) {
        LUAUTILS_ERROR(L, "Ray:Cast - function should be called with `:`");
    }

    if (argCount > 4) {
        LUAUTILS_ERROR(L, "Ray:Cast - wrong number of arguments"); // returns
    }
    
    if (LUA_GET_METAFIELD_AND_RETURN_TYPE(L, 1, LUA_RAY_FIELD_ORIGIN) == LUA_TNIL) {
        LUAUTILS_INTERNAL_ERROR(L) // returns
    }
    
    float3 origin = float3_zero;
    
    if (lua_number3_getXYZ(L, -1, &origin.x, &origin.y, &origin.z) == false) {
        lua_pop(L, 1);
        LUAUTILS_INTERNAL_ERROR(L) // returns
    }
    
    lua_pop(L, 1); // pop Origin
    
    if (LUA_GET_METAFIELD_AND_RETURN_TYPE(L, 1, LUA_RAY_FIELD_DIRECTION) == LUA_TNIL) {
        LUAUTILS_INTERNAL_ERROR(L) // returns
    }
    
    float3 direction = float3_zero;
    
    if (lua_number3_getXYZ(L, -1, &direction.x, &direction.y, &direction.z) == false) {
        lua_pop(L, 1);
        LUAUTILS_INTERNAL_ERROR(L) // returns
    }
    
    lua_pop(L, 1); // pop Direction
    
    // retrieve Game from Lua state
    CGame *g = nullptr;
    if (lua_utils_getGamePtr(L, &g) == false || g == nullptr) {
        LUAUTILS_INTERNAL_ERROR(L) // returns
    }

    uint16_t groups = PHYSICS_GROUP_ALL_API;
    Shape *s = nullptr; // cast ray on shape if not NULL (goes down to blocks)
    
    // BUILD FILTER IN
    
    if (argCount >= 2) { // optional FilterIn provided
        lua_pushvalue(L, 2);
    } else {
        LUA_GET_METAFIELD_PUSHING_NIL_IF_NOT_FOUND(L, 1, LUA_RAY_FIELD_FILTERIN);
    }
    // FilterIn now at -1
    
    // validate FilterIn
    if (lua_shape_isShape(L, -1)) {
        s = lua_shape_getShapePtr(L, -1);
    } else if (lua_istable(L, -1) || lua_isuserdata(L, -1)) {
        if (lua_collision_groups_get_mask(L, -1, &groups) == false) {
            lua_pop(L, 1); // pop filter in
            LUAUTILS_ERROR(L, "Ray:Cast - filter IN is not a viable CollisionGroups"); // returns
        }
    } else if (lua_isnumber(L, -1)) { // group number
        const uint16_t groupNumber = static_cast<uint16_t>(lua_tonumber(L, -1));
        if (lua_collision_groups_group_number_is_valid(groupNumber) == false) {
            lua_pop(L, 1); // pop filter in
            LUAUTILS_ERROR(L, "Ray:Cast - filter IN is not a viable group number"); // returns
        }
        groups = lua_collision_groups_group_number_to_mask(groupNumber);
    }
    
    lua_pop(L, 1); // pop filter in
    
    // BUILD FILTER OUT
    
    if (argCount >= 3) { // optional FilterOut provided
        lua_pushvalue(L, 3);
    } else {
        LUA_GET_METAFIELD_PUSHING_NIL_IF_NOT_FOUND(L, 1, LUA_RAY_FIELD_FILTEROUT);
    }
    // FilterOut now at -1
    
    DoublyLinkedList *filterOutTransforms = nullptr;
    if (lua_object_isObject(L, -1)) {
        
        Transform *t = nullptr;
        lua_object_getTransformPtr(L, -1, &t);
        
        if (t != nullptr) {
            filterOutTransforms = doubly_linked_list_new();
            doubly_linked_list_push_first(filterOutTransforms, t);
        }
    }
    
    lua_pop(L, 1); // pop filter out

    // optional onlyFirstImpact provided
    // Note: has no effect if a Shape was given as parameter
    bool castAll = false;
    if (s == nullptr && argCount >= 4 && lua_isboolean(L, 4)) {
        castAll = lua_toboolean(L, 4) == false;
    }
    
    Ray *ray = ray_new(&origin, &direction);

    CastResult hit = scene_cast_result_default();
    DoublyLinkedList *hits = nullptr;
    if (castAll) {
        hits = doubly_linked_list_new();
        scene_cast_all_ray(game_get_scene(g), ray, groups, filterOutTransforms, hits);
    } else if (s == nullptr) {
        scene_cast_ray(game_get_scene(g), ray, groups, filterOutTransforms, &hit);
    } else {
        if (transform_is_removed_from_scene(shape_get_transform(s)) == false) {
            scene_cast_ray_shape_only(game_get_scene(g), shape_get_transform(s), s, ray, &hit);
        }
    }

    if (filterOutTransforms != nullptr) {
        doubly_linked_list_free(filterOutTransforms);
    }
    ray_free(ray);

    if (castAll) {
        lua_newtable(L); // array of Impacts

        DoublyLinkedListNode *n = doubly_linked_list_first(hits);
        CastResult *cr;
        int luaIdx = 1;
        while (n != nullptr) {
            cr = static_cast<CastResult *>(doubly_linked_list_node_pointer(n));

            lua_impact_push(L, *cr);
            lua_rawseti(L, -2, luaIdx);
            ++luaIdx;

            free(cr);
            n = doubly_linked_list_node_next(n);
        }
        doubly_linked_list_free(hits);
    } else {
        lua_impact_push(L, hit);
    }

    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
    return 1;
}


