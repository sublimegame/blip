//
//  lua_point.cpp
//  Cubzh
//
//  Created by Gaetan de Villele on 20/05/2021.
//

#include "lua_point.hpp"

// C++
#include <cstring>

// Lua
#include "lua.hpp"
#include "lua_utils.hpp"
#include "lua_number3.hpp"

#include "config.h"
#include "float3.h"
#include "shape.h"
#include "game.h"

#include "lua_constants.h"

#define LUA_POINT_FIELD_COORDINATES     "Coordinates"    // number3 (read/write)
#define LUA_POINT_FIELD_COORDS          "Coords"         // number3 (read/write)
#define LUA_POINT_FIELD_POSITION        "Position"       // number3 (read/write)
#define LUA_POINT_FIELD_POS             "Pos"            // number3 (read/write)
#define LUA_POINT_FIELD_LOCALPOSITION   "LocalPosition"  // number3 (read/write)
#define LUA_POINT_FIELD_ROTATION        "Rotation"       // number3 (read/write)

// --------------------------------------------------
// MARK: - Local functions' prototypes -
// --------------------------------------------------

static int _point_metatable_index(lua_State *L);
static int _point_metatable_newindex(lua_State *L);
static void _point_gc(void *_ud);

// --------------------------------------------------
// MARK: - C handlers
// --------------------------------------------------

typedef void (*point_C_handler_set) (const float *posX,
                                     const float *posY,
                                     const float *posZ,
                                     const float *rotX,
                                     const float *rotY,
                                     const float *rotZ,
                                     lua_State *L,
                                     void *userdata);

typedef void (*point_C_handler_get) (float *posX,
                                     float *posY,
                                     float *posZ,
                                     float *rotX,
                                     float *rotY,
                                     float *rotZ,
                                     lua_State *L,
                                     const void *userdata);

typedef void (*point_C_handler_free) (void *userdata);

// Default handler
typedef struct point_C_handler {
    point_C_handler_set set;
    point_C_handler_get get;
    point_C_handler_free free_userdata;
    void *userdata;
} point_C_handler;

typedef struct point_C_handler_userdata {
    // position has known in Lua
    // it can map to a different Point position internally
    // if cubes within the shape have been moved,
    // and thus an offset has been set.
    // A point position in Lua should always remain the same.
    float3 position;
    // point rotation (offset has no impact on this)
    float3 rotation;
    char* name;
    Shape *parentShape;
} point_C_handler_userdata;

void _get_point_handler(float *posX,
                        float *posY,
                        float *posZ,
                        float *rotX,
                        float *rotY,
                        float *rotZ,
                        lua_State *L,
                        const void *userdata) {
    
    const point_C_handler_userdata *_userdata = static_cast<const point_C_handler_userdata *>(userdata);
    
    if (posX != nullptr) *posX = _userdata->position.x;
    if (posY != nullptr) *posY = _userdata->position.y;
    if (posZ != nullptr) *posZ = _userdata->position.z;
    
    if (rotX != nullptr) *rotX = _userdata->rotation.x;
    if (rotY != nullptr) *rotY = _userdata->rotation.y;
    if (rotZ != nullptr) *rotZ = _userdata->rotation.z;
}

void _set_point_handler(const float *posX,
                        const float *posY,
                        const float *posZ,
                        const float *rotX,
                        const float *rotY,
                        const float *rotZ,
                        lua_State *L,
                        void *userdata) {
    
    point_C_handler_userdata *_userdata = static_cast<point_C_handler_userdata *>(userdata);
    vx_assert(_userdata != nullptr);

    if (posX != nullptr) _userdata->position.x = *posX;
    if (posY != nullptr) _userdata->position.y = *posY;
    if (posZ != nullptr) _userdata->position.z = *posZ;
    if (rotX != nullptr) _userdata->rotation.x = *rotX;
    if (rotY != nullptr) _userdata->rotation.y = *rotY;
    if (rotZ != nullptr) _userdata->rotation.z = *rotZ;
    
    shape_set_point_of_interest(_userdata->parentShape, _userdata->name, &(_userdata->position));
    shape_set_point_rotation(_userdata->parentShape, _userdata->name, &(_userdata->rotation));
}

void _free_point_handler(void *userdata) {
    point_C_handler_userdata *_userdata = static_cast<point_C_handler_userdata *>(userdata);
    if (_userdata->parentShape != nullptr) {
        shape_release(_userdata->parentShape);
        free(_userdata->name);
        _userdata->parentShape = nullptr;
    }
    free(_userdata);
}

typedef struct point_userdata {
    point_C_handler *handler;
} point_userdata;

point_C_handler *_point_getCHandler(lua_State *L, const int idx) {
    point_userdata *ud = static_cast<point_userdata*>(lua_touserdata(L, idx));
    return ud->handler;
}

/// Lua print function for Shape instance tables
int lua_point_snprintf(lua_State *L,
                       char *result,
                       size_t maxLen,
                       bool spacePrefix) {
    vx_assert(L != nullptr);
    
    float posX = 0.0f;
    float posY = 0.0f;
    float posZ = 0.0f;
    float rotX = 0.0f;
    float rotY = 0.0f;
    float rotZ = 0.0f;
    
    lua_point_getValues(L,
                        -1,
                        nullptr,
                        &posX, &posY, &posZ,
                        &rotX, &rotY, &rotZ);
    
    return snprintf(result,
                    maxLen,
                    "%s[Point posX: %.2f posY: %.2f posZ: %.2f rotX: %.2f rotY: %.2f rotZ: %.2f]",
                    spacePrefix ? " " : "",
                    static_cast<double>(posX),
                    static_cast<double>(posY),
                    static_cast<double>(posZ),
                    static_cast<double>(rotX),
                    static_cast<double>(rotY),
                    static_cast<double>(rotZ));
}

// --------------------------------------------------
// MARK: - Shape instances tables -
// --------------------------------------------------

void lua_point_push(lua_State *L,
                    const std::string& pointName,
                    Shape *parentShape,
                    bool createIfNotFound) {
    vx_assert(L != nullptr);
    LUAUTILS_STACK_SIZE(L)
    
    if (parentShape == nullptr) {
        lua_pushnil(L);
        LUAUTILS_STACK_SIZE_ASSERT(L, 1)
        return;
    }
    
    const float3 *pointPosition = shape_get_point_of_interest(parentShape, pointName.c_str());
    const float3 *pointRotation = shape_get_point_rotation(parentShape, pointName.c_str());
    
    if (pointPosition == nullptr) {
        if (createIfNotFound) {
            float3 zero = float3_zero;
            shape_set_point_of_interest(parentShape, pointName.c_str(), &zero);
            pointPosition = shape_get_point_of_interest(parentShape, pointName.c_str());
        } else {
            // rotation can be NULL, but position is a minimum requirement
            lua_pushnil(L);
            LUAUTILS_STACK_SIZE_ASSERT(L, 1)
            return;
        }
    }
    
    vx_assert(pointPosition != nullptr);
    
    if (pointRotation == nullptr) {
        float3 zero = float3_zero;
        shape_set_point_rotation(parentShape, pointName.c_str(), &zero);
        pointRotation = shape_get_point_rotation(parentShape, pointName.c_str());
    }
    vx_assert(pointRotation != nullptr);
    
    if (shape_retain(parentShape) == false) {
        LUAUTILS_ERROR(L, "Too many references on same Object."); // returns
    }
    
    point_userdata *ud = static_cast<point_userdata *>(lua_newuserdatadtor(L, sizeof(point_userdata), _point_gc));

    point_C_handler *handler = static_cast<point_C_handler*>(malloc(sizeof(point_C_handler)));
    if (shape_is_lua_mutable(parentShape)) {
        handler->set = _set_point_handler;
    } else {
        handler->set = nullptr; // read-only shape have no set handler function
    }

    handler->get = _get_point_handler;
    handler->free_userdata = _free_point_handler;
    point_C_handler_userdata *userdata = static_cast<point_C_handler_userdata*>(malloc(sizeof(point_C_handler_userdata)));
    userdata->parentShape = parentShape; // parentShape retained above
    userdata->name = string_new_copy(pointName.c_str());

    float3_copy(&userdata->position, pointPosition);
    float3_copy(&userdata->rotation, pointRotation);

    handler->userdata = userdata;

    ud->handler = handler;

    // NOTE: metatable doesn't seem to be used to store instance specific fields
    // it should be easy to share it between all Point instances.
    lua_newtable(L); // metatable
    {
        lua_pushstring(L, "__index");
        lua_pushcfunction(L, _point_metatable_index, "");
        lua_rawset(L, -3);

        lua_pushstring(L, "__newindex");
        lua_pushcfunction(L, _point_metatable_newindex, "");
        lua_rawset(L, -3);

        // hide the metatable from the Lua sandbox user
        lua_pushstring(L, "__metatable");
        lua_pushboolean(L, false);
        lua_rawset(L, -3);

        lua_pushstring(L, "__type");
        lua_pushstring(L, "Point");
        lua_rawset(L, -3);

        lua_pushstring(L, "__typen");
        lua_pushinteger(L, ITEM_TYPE_POINT);
        lua_rawset(L, -3);
    }
    lua_setmetatable(L, -2);

    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
    return;
}

bool lua_point_getValues(lua_State *L,
                         const int idx,
                         Shape **parentShape,
                         float *posX, float *posY, float *posZ,
                         float *rotX, float *rotY, float *rotZ) {
    vx_assert(L != nullptr);
    LUAUTILS_STACK_SIZE(L)
    
    // make sure Lua value at index <idx> is a Point instance
    if (lua_utils_getObjectType(L, idx) != ITEM_TYPE_POINT) {
        LUAUTILS_STACK_SIZE_ASSERT(L, 0)
        return false;
    }
    
    point_C_handler *handler = _point_getCHandler(L, idx);
    vx_assert(handler != nullptr);

    handler->get(posX, posY, posZ, rotX, rotY, rotZ, L, handler->userdata);
    
    if (parentShape != nullptr) {
        point_C_handler_userdata *userdata = static_cast<point_C_handler_userdata *>(handler->userdata);
        *parentShape = userdata->parentShape;
    }
    
    LUAUTILS_STACK_SIZE_ASSERT(L, 0)
    return true;
}

bool lua_point_setValues(lua_State *L,
                         const int idx,
                         const float *posX, const float *posY, const float *posZ,
                         const float *rotX, const float *rotY, const float *rotZ) {
    
    vx_assert(L != nullptr);
    LUAUTILS_STACK_SIZE(L)
    
    // make sure Lua value at index <idx> is a Point instance
    if (lua_utils_getObjectType(L, idx) != ITEM_TYPE_POINT) {
        LUAUTILS_STACK_SIZE_ASSERT(L, 0)
        return false;
    }
    
    point_C_handler *handler = _point_getCHandler(L, idx);
    vx_assert(handler != nullptr);
    
    if (handler->set == nullptr) {
        LUAUTILS_STACK_SIZE_ASSERT(L, 0)
        return false;
    }
    handler->set(posX, posY, posZ, rotX, rotY, rotZ, L, handler->userdata);
    
    LUAUTILS_STACK_SIZE_ASSERT(L, 0)
    return true;
}

// --------------------------------------------------
// MARK: - Local functions' implementation -
// --------------------------------------------------

static int _point_metatable_index(lua_State *L) {
    vx_assert(L != nullptr);
    LUAUTILS_ASSERT_ARGS_COUNT(L, 2)
    LUAUTILS_ASSERT_ARG_TYPE(L, 1, ITEM_TYPE_POINT)
    LUAUTILS_STACK_SIZE(L)
    
    if (lua_utils_isStringStrict(L, 2) == false) {
        lua_pushnil(L);
        LUAUTILS_STACK_SIZE_ASSERT(L, 1)
        return 1;
    }
    
    const char *key = lua_tostring(L, 2);
    
    if (strcmp(key, LUA_POINT_FIELD_COORDS) == 0 || strcmp(key, LUA_POINT_FIELD_COORDINATES) == 0) {
        
        Shape *s = nullptr;
        float3 blockCoords = float3_zero;
        
        lua_point_getValues(L, 1, &s,
                            &(blockCoords.x),
                            &(blockCoords.y),
                            &(blockCoords.z),
                            nullptr,
                            nullptr,
                            nullptr);
        lua_number3_pushNewObject(L, blockCoords.x, blockCoords.y, blockCoords.z);
        
    } else if (strcmp(key, LUA_POINT_FIELD_POS) == 0 || strcmp(key, LUA_POINT_FIELD_POSITION) == 0) {
        
        Shape *s = nullptr;
        float3 blockCoords = float3_zero;
        
        lua_point_getValues(L, 1, &s,
                            &(blockCoords.x),
                            &(blockCoords.y),
                            &(blockCoords.z),
                            nullptr,
                            nullptr,
                            nullptr);

        vx_assert(s != nullptr);
    
        float3 world = shape_block_to_world(s, blockCoords.x, blockCoords.y, blockCoords.z);
        lua_number3_pushNewObject(L, world.x, world.y, world.z);
        
    } else if (strcmp(key, LUA_POINT_FIELD_LOCALPOSITION) == 0) {
        
        Shape *s = nullptr;
        float3 blockCoords = float3_zero;
        
        lua_point_getValues(L, 1, &s,
                            &(blockCoords.x),
                            &(blockCoords.y),
                            &(blockCoords.z),
                            nullptr,
                            nullptr,
                            nullptr);

        vx_assert(s != nullptr);
        
        float3 local = shape_block_to_local(s, blockCoords.x, blockCoords.y, blockCoords.z);
        lua_number3_pushNewObject(L, local.x, local.y, local.z);
        
    } else if (strcmp(key, LUA_POINT_FIELD_ROTATION) == 0) {
        
        Shape *s = nullptr;
        float3 rotation = float3_zero;
        
        lua_point_getValues(L, 1, &s,
                            nullptr,
                            nullptr,
                            nullptr,
                            &(rotation.x),
                            &(rotation.y),
                            &(rotation.z));
        
        lua_number3_pushNewObject(L, rotation.x, rotation.y, rotation.z);
        
    } else {
        lua_pushnil(L);
    }
    
    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
    return 1;
}

static int _point_metatable_newindex(lua_State *L) {
    vx_assert(L != nullptr);
    LUAUTILS_ASSERT_ARGS_COUNT(L, 2) // should be 3
    LUAUTILS_ASSERT_ARG_TYPE(L, 1, ITEM_TYPE_POINT)
    LUAUTILS_STACK_SIZE(L)
    
    // TODO: implement this
    // In the meantime, item editor can use AddPoint with correct
    // values to set the point directly (not updating values afterwards).
    // AddPoint overrides existing point values if found.
    
    LUAUTILS_STACK_SIZE_ASSERT(L, 0)
    return 0;
}

static void _point_gc(void *_ud) {
    point_userdata *ud = static_cast<point_userdata*>(_ud);
    if (ud->handler != nullptr) {
        ud->handler->free_userdata(ud->handler->userdata);
        free(ud->handler);
        ud->handler = nullptr;
    }
}
