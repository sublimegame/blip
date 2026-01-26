// -------------------------------------------------------------
//  Cubzh
//  lua_rotation.cpp
//  Created by Arthur Cormerais on August 3, 2023.
// -------------------------------------------------------------

#include "lua_rotation.hpp"

#include <cstring>
#include <math.h>

#include "lua.hpp"
#include "lua_utils.hpp"
#include "lua_number3.hpp"
#include "sbs.hpp"
#include "xptools.h"
#include "config.h" // for vx_assert
#include "weakptr.h"

#define LUA_ROTATION_REGISTRY_ON_SET_CALLBACKS "_rot_osc"

#define LUA_G_ROTATION_INSTANCE_METATABLE "__imt"

// properties
#define LUA_ROTATION_FIELD_X "X"
#define LUA_ROTATION_FIELD_Y "Y"
#define LUA_ROTATION_FIELD_Z "Z"

// functions
#define LUA_ROTATION_FIELD_COPY "Copy"
#define LUA_ROTATION_FIELD_INVERSE "Inverse"
#define LUA_ROTATION_FIELD_LERP "Lerp"
#define LUA_ROTATION_FIELD_SLERP "Slerp"
#define LUA_ROTATION_FIELD_ANGLE "Angle"
#define LUA_ROTATION_FIELD_SET "Set"
#define LUA_ROTATION_FIELD_AXISANGLE "SetAxisAngle"
#define LUA_ROTATION_FIELD_LOOK "SetLookRotation"
#define LUA_ROTATION_FIELD_FROMTO "SetFromToRotation"
#define LUA_ROTATION_FIELD_ON_SET_CALLBACKS "OnSetCallbacks" // callbacks
#define LUA_ROTATION_FIELD_ADD_ON_SET_CALLBACK "AddOnSetCallback" // function, receives a callback
#define LUA_ROTATION_FIELD_REMOVE_ON_SET_CALLBACK "RemoveOnSetCallback" // function, receives a callback

typedef struct rotation_handlers {
    rotation_handler_set set;
    rotation_handler_get get;
    rotation_handler_userdata userdata;
    bool isUserdataWeakptr;
} rotation_handlers;

typedef struct rotation_userdata {
    rotation_handlers *handlers; // null by default
    double x;
    double y;
    double z;
    double w;
    double eulerX;
    double eulerY;
    double eulerZ;
    bool eulerDirty;
    bool hasOnSetCallbacks; // false by default, fetch callbacks and trigger them only if true
} rotation_userdata;

void rotation_userdata_free_handlers(rotation_userdata *ud) {
    if (ud->handlers == nullptr) return;
    if (ud->handlers->isUserdataWeakptr) {
        weakptr_release(static_cast<Weakptr*>(ud->handlers->userdata.ptr));
    }
    free(ud->handlers);
    ud->handlers = nullptr;
}

// MARK: - Static functions' prototypes -

static void _rotation_set_cache(lua_State *L, int idx, Quaternion *q);
static void _rotation_get_cache(lua_State *L, int idx, Quaternion *q);
static void _rotation_set_euler_cache(lua_State *L, int idx, float *x, float *y, float *z);
static void _rotation_get_euler_cache(lua_State *L, int idx, float *x, float *y, float *z);

static int _g_rotation_metatable_index(lua_State *L);
static int _g_rotation_metatable_newindex(lua_State *L);
static int _g_rotation_metatable_call(lua_State *L);
static int _rotation_metatable_index(lua_State *L);
static void _rotation_metatable_gc(void *_ud);
static int _rotation_metatable_new_index(lua_State *L);
static int _rotation_metatable_add(lua_State *L);
static int _rotation_metatable_sub(lua_State *L);
static int _rotation_metatable_unm(lua_State *L);
static int _rotation_metatable_mul(lua_State *L);
static int _rotation_metatable_div(lua_State *L);
static int _rotation_metatable_idiv(lua_State *L);
static int _rotation_metatable_pow(lua_State *L);
static int _rotation_metatable_eq(lua_State *L);
static int _rotation_metatable_tostring(lua_State *L);
static int _rotation_copy(lua_State *L);
static int _rotation_inverse(lua_State *L);
static int _rotation_lerp(lua_State *L);
static int _rotation_slerp(lua_State *L);
static int _rotation_set(lua_State *L);
static int _rotation_angle(lua_State *L);
static int _rotation_axisangle(lua_State *L);
static int _rotation_look(lua_State *L);
static int _rotation_fromto(lua_State *L);
static int _rotation_addOnSetCallback(lua_State *L);
static int _rotation_removeOnSetCallback(lua_State *L);

// MARK: - Public functions -

void lua_g_rotation_create_and_push(lua_State *L) {
    vx_assert(L != nullptr);
    LUAUTILS_STACK_SIZE(L)
    
    lua_newuserdata(L, 1);
    lua_newtable(L); // metatable
    {
        lua_pushstring(L, "__index");
        lua_pushcfunction(L, _g_rotation_metatable_index, "");
        lua_rawset(L, -3);
        
        lua_pushstring(L, "__newindex");
        lua_pushcfunction(L, _g_rotation_metatable_newindex, "");
        lua_rawset(L, -3);
        
        lua_pushstring(L, "__call");
        lua_pushcfunction(L, _g_rotation_metatable_call, "");
        lua_rawset(L, -3);

        lua_pushstring(L, "__metatable");
        lua_pushboolean(L, false);
        lua_rawset(L, -3);

        lua_pushstring(L, "__type"); // used to log tables
        lua_pushstring(L, "RotationInterface");
        lua_rawset(L, -3);

        lua_pushstring(L, "__typen"); // used to log tables
        lua_pushinteger(L, ITEM_TYPE_G_ROTATION);
        lua_rawset(L, -3);
        
        lua_pushstring(L, LUA_G_ROTATION_INSTANCE_METATABLE);
        lua_newtable(L);
        {
            lua_pushstring(L, "__index");
            lua_pushcfunction(L, _rotation_metatable_index, "");
            lua_rawset(L, -3);
            
            lua_pushstring(L, "__newindex");
            lua_pushcfunction(L, _rotation_metatable_new_index, "");
            lua_rawset(L, -3);
            
            lua_pushstring(L, "__add");
            lua_pushcfunction(L, _rotation_metatable_add, "");
            lua_rawset(L, -3);
            
            lua_pushstring(L, "__sub");
            lua_pushcfunction(L, _rotation_metatable_sub, "");
            lua_rawset(L, -3);
            
            lua_pushstring(L, "__unm");
            lua_pushcfunction(L, _rotation_metatable_unm, "");
            lua_rawset(L, -3);
            
            lua_pushstring(L, "__mul");
            lua_pushcfunction(L, _rotation_metatable_mul, "");
            lua_rawset(L, -3);
            
            lua_pushstring(L, "__div");
            lua_pushcfunction(L, _rotation_metatable_div, "");
            lua_rawset(L, -3);
            
            lua_pushstring(L, "__idiv");
            lua_pushcfunction(L, _rotation_metatable_idiv, "");
            lua_rawset(L, -3);

            lua_pushstring(L, "__pow");
            lua_pushcfunction(L, _rotation_metatable_pow, "");
            lua_rawset(L, -3);
            
            lua_pushstring(L, "__eq");
            lua_pushcfunction(L, _rotation_metatable_eq, "");
            lua_rawset(L, -3);

            lua_pushstring(L, "__tostring");
            lua_pushcfunction(L, _rotation_metatable_tostring, "");
            lua_rawset(L, -3);

            lua_pushstring(L, "__metatable");
            lua_pushboolean(L, false);
            lua_rawset(L, -3);

            lua_pushstring(L, "__type");
            lua_pushstring(L, "Rotation");
            lua_rawset(L, -3);

            lua_pushstring(L, "__typen");
            lua_pushinteger(L, ITEM_TYPE_ROTATION);
            lua_rawset(L, -3);
        }
        lua_rawset(L, -3);
    }
    lua_setmetatable(L, -2);

    lua_newtable(L);
    lua_setfield(L, LUA_REGISTRYINDEX, LUA_ROTATION_REGISTRY_ON_SET_CALLBACKS);

    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
}

bool lua_rotation_push_new(lua_State *L, Quaternion q) {
    vx_assert(L != nullptr);
    LUAUTILS_STACK_SIZE(L)

    rotation_userdata* ud = static_cast<rotation_userdata*>(lua_newuserdata(L, sizeof(rotation_userdata)));
    ud->x = static_cast<double>(q.x);
    ud->y = static_cast<double>(q.y);
    ud->z = static_cast<double>(q.z);
    ud->w = static_cast<double>(q.w);
    ud->eulerX = 0.0;
    ud->eulerY = 0.0;
    ud->eulerZ = 0.0;
    ud->eulerDirty = true;
    ud->handlers = nullptr;
    ud->hasOnSetCallbacks = false;

    // set shared metatable
    lua_getglobal(L, P3S_LUA_G_ROTATION);
    LUA_GET_METAFIELD(L, -1, LUA_G_ROTATION_INSTANCE_METATABLE);
    lua_remove(L, -2);
    lua_setmetatable(L, -2);

    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
    return true;
}

bool lua_rotation_push_new_euler(lua_State *L, float x, float y, float z) {
    vx_assert(L != nullptr);
    LUAUTILS_STACK_SIZE(L)

    Quaternion q; euler_to_quaternion(x, y, z, &q);

    rotation_userdata* ud = static_cast<rotation_userdata*>(lua_newuserdatadtor(L, sizeof(rotation_userdata), _rotation_metatable_gc));
    ud->x = static_cast<double>(q.x);
    ud->y = static_cast<double>(q.y);
    ud->z = static_cast<double>(q.z);
    ud->w = static_cast<double>(q.w);
    ud->eulerX = static_cast<double>(x);
    ud->eulerY = static_cast<double>(y);
    ud->eulerZ = static_cast<double>(z);
    ud->eulerDirty = false;
    ud->handlers = nullptr;
    ud->hasOnSetCallbacks = false;
    
    // set shared metatable
    lua_getglobal(L, P3S_LUA_G_ROTATION);
    LUA_GET_METAFIELD(L, -1, LUA_G_ROTATION_INSTANCE_METATABLE);
    lua_remove(L, -2);
    lua_setmetatable(L, -2);
    
    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
    return true;
}

bool lua_rotation_set_handlers(lua_State *L, int idx,
                               rotation_handler_set set,
                               rotation_handler_get get,
                               rotation_handler_userdata userdata,
                               bool isUserdataWeakptr) {
    vx_assert(L != nullptr);
    LUAUTILS_STACK_SIZE(L)

    if (lua_utils_getObjectType(L, idx) != ITEM_TYPE_ROTATION) {
        LUAUTILS_INTERNAL_ERROR(L) // returns
    }

    rotation_userdata *ud = static_cast<rotation_userdata*>(lua_touserdata(L, idx));
    rotation_userdata_free_handlers(ud);

    rotation_handlers *handlers = static_cast<rotation_handlers*>(malloc(sizeof(rotation_handlers)));
    handlers->set = set;
    handlers->get = get;
    handlers->userdata = userdata;
    handlers->isUserdataWeakptr = isUserdataWeakptr;

    ud->handlers = handlers;

    LUAUTILS_STACK_SIZE_ASSERT(L, 0)

    return true;
}

bool lua_rotation_set(lua_State *L, int idx, Quaternion *q) {
    vx_assert(L != nullptr);
    LUAUTILS_STACK_SIZE(L)

    if (lua_utils_getObjectType(L, idx) != ITEM_TYPE_ROTATION) {
        LUAUTILS_STACK_SIZE_ASSERT(L, 0)
        return false;
    }

    quaternion_op_normalize(q);

    rotation_userdata *ud = static_cast<rotation_userdata*>(lua_touserdata(L, idx));
    rotation_handlers *handlers  = ud->handlers;

    if (handlers != nullptr) {
        vx_assert(handlers->set != nullptr);
        handlers->set(q, L, &handlers->userdata);
    }

    _rotation_set_cache(L, idx, q);
    LUAUTILS_STACK_SIZE_ASSERT(L, 0)

    if (ud->hasOnSetCallbacks) {
        lua_getfield(L, LUA_REGISTRYINDEX, LUA_ROTATION_REGISTRY_ON_SET_CALLBACKS);
        lua_pushlightuserdata(L, ud);
        if (lua_rawget(L, -2) != LUA_TNIL) {
            // size_t len = lua_rawlen(L, -1);
            lua_pushnil(L); // callbacks at -2, nil at -1
            while(lua_next(L, -2) != 0) {
                // callbacks at -3, key at -2, value at -1
                if (lua_isfunction(L, -1)) {
                    lua_pushvalue(L, idx < 0 ? idx - 3 : idx);
                    lua_call(L, 1, 0);
                    // value popped by function
                } else {
                    lua_pop(L, 1); // pop value
                }
            }
        }
        lua_pop(L, 2);
    }

    LUAUTILS_STACK_SIZE_ASSERT(L, 0)
    return true;
}

bool lua_rotation_set_euler(lua_State *L, int idx, float *x, float *y, float *z) {
    vx_assert(L != nullptr);
    LUAUTILS_STACK_SIZE(L)

    if (lua_utils_getObjectType(L, idx) != ITEM_TYPE_ROTATION) {
        LUAUTILS_STACK_SIZE_ASSERT(L, 0)
        return false;
    }

    float3 euler;
    if (x == nullptr || y == nullptr || z == nullptr) {
        if (lua_rotation_get_euler(L, idx, &euler.x, &euler.y, &euler.z) == false) {
            LUAUTILS_INTERNAL_ERROR(L);
        }
        if (x != nullptr) {
            euler.x = *x;
        }
        if (y != nullptr) {
            euler.y = *y;
        }
        if (z != nullptr) {
            euler.z = *z;
        }
    } else {
        float3_set(&euler, *x, *y, *z);
    }
    Quaternion q; euler_to_quaternion_vec(&euler, &q);
    quaternion_op_normalize(&q);

    _rotation_set_euler_cache(L, idx, x, y, z);
    
    if (lua_rotation_set(L, idx, &q) == false) {
        return false;
    }

    LUAUTILS_STACK_SIZE_ASSERT(L, 0)
    return true;
}

bool lua_rotation_get(lua_State *L, int idx, Quaternion *q) {
    vx_assert(L != nullptr);
    LUAUTILS_STACK_SIZE(L)
    
    if (q != nullptr) {
        *q = quaternion_identity;
    }

    if (lua_utils_getObjectType(L, idx) != ITEM_TYPE_ROTATION) {
        LUAUTILS_STACK_SIZE_ASSERT(L, 0)
        return false;
    }

    rotation_userdata *ud = static_cast<rotation_userdata*>(lua_touserdata(L, idx));
    rotation_handlers *handlers  = ud->handlers;

    if (handlers != nullptr) {
        vx_assert(handlers->get != nullptr);
        handlers->get(q, L, &handlers->userdata);
        return true;
    }

    _rotation_get_cache(L, idx, q);

    LUAUTILS_STACK_SIZE_ASSERT(L, 0)
    return true;
}

bool lua_rotation_get_euler(lua_State *L, int idx, float *x, float *y, float *z) {
    vx_assert(L != nullptr);
    LUAUTILS_STACK_SIZE(L)

    if (lua_utils_getObjectType(L, idx) != ITEM_TYPE_ROTATION) {
        if (x != nullptr) { *x = 0.0f; }
        if (y != nullptr) { *y = 0.0f; }
        if (z != nullptr) { *z = 0.0f; }
        LUAUTILS_STACK_SIZE_ASSERT(L, 0)
        return false;
    }

    rotation_userdata *ud = static_cast<rotation_userdata*>(lua_touserdata(L, idx));
    rotation_handlers *handlers  = ud->handlers;

    float3 euler;

    if (handlers != nullptr) {
        vx_assert(handlers->get != nullptr);

        Quaternion q;
        handlers->get(&q, L, &handlers->userdata);

        // 1) euler cache dirty: update cache from object quaternion q
        if (ud->eulerDirty) {
            _rotation_set_cache(L, idx, &q);
            quaternion_to_euler(&q, &euler);
            _rotation_set_euler_cache(L, idx, &euler.x, &euler.y, &euler.z);
        } else {
            Quaternion qCache;
            _rotation_get_cache(L, idx, &qCache);

            // 2) object quaternion hasn't changed: use euler cache
            if (quaternion_is_equal(&qCache, &q, EPSILON_ZERO_RAD)) {
                _rotation_get_euler_cache(L, idx, &euler.x, &euler.y, &euler.z);
            }
            // 3) object quaternion has changed (source of truth): override cache
            else {
                _rotation_set_cache(L, idx, &q);

                quaternion_to_euler(&q, &euler);
                _rotation_set_euler_cache(L, idx, &euler.x, &euler.y, &euler.z);
            }
        }
    } else {
        // 4) standalone rotation instance can function solely based on cached quaternion
        if (ud->eulerDirty) {
            Quaternion qCache;
            _rotation_get_cache(L, idx, &qCache);

            quaternion_to_euler(&qCache, &euler);
            _rotation_set_euler_cache(L, idx, &euler.x, &euler.y, &euler.z);
        } else {
            _rotation_get_euler_cache(L, idx, &euler.x, &euler.y, &euler.z);
        }
    }

    if (x != nullptr) {
        *x = euler.x;
    }
    if (y != nullptr) {
        *y = euler.y;
    }
    if (z != nullptr) {
        *z = euler.z;
    }

    LUAUTILS_STACK_SIZE_ASSERT(L, 0)
    return true;
}

bool lua_rotation_or_table_get(lua_State *L, int idx, Quaternion *q) {
    vx_assert(L != nullptr);
    LUAUTILS_STACK_SIZE(L)

    if (lua_rotation_get(L, idx, q)) {
        LUAUTILS_STACK_SIZE_ASSERT(L, 0)
        return true;
    }

    // check value is a table
    if (lua_istable(L, idx) == false) {
        LUAUTILS_STACK_SIZE_ASSERT(L, 0)
        return false;
    }

    lua_Integer len = static_cast<lua_Integer>(lua_objlen(L, idx));

    if (len != 3) {
        LUAUTILS_STACK_SIZE_ASSERT(L, 0)
        return false;
    }

    lua_rawgeti(L, idx, 1);
    lua_rawgeti(L, idx < 0 ? idx - 1 : idx, 2);
    lua_rawgeti(L, idx < 0 ? idx - 2 : idx, 3);

    int xType = lua_type(L, -3);
    int yType = lua_type(L, -2);
    int zType = lua_type(L, -1);


    if (xType == LUA_TNUMBER && yType == LUA_TNUMBER && zType == LUA_TNUMBER) {
        // values are in reverse order in the Lua stack
        const float x = static_cast<float>(lua_tonumber(L, -3));
        const float y = static_cast<float>(lua_tonumber(L, -2));
        const float z = static_cast<float>(lua_tonumber(L, -1));
        lua_pop(L, 3);

        euler_to_quaternion(x, y, z, q);

        LUAUTILS_STACK_SIZE_ASSERT(L, 0)
        return true;
    }

    lua_pop(L, 3);
    LUAUTILS_STACK_SIZE_ASSERT(L, 0)
    return false;
}

bool lua_rotation_or_table_get_euler(lua_State *L, int idx, float *x, float *y, float *z) {
    vx_assert(L != nullptr);
    LUAUTILS_STACK_SIZE(L)

    if (lua_rotation_get_euler(L, idx, x, y, z)) {
        LUAUTILS_STACK_SIZE_ASSERT(L, 0)
        return true;
    }

    // check value is a table
    if (lua_istable(L, idx) == false) {
        LUAUTILS_STACK_SIZE_ASSERT(L, 0)
        return false;
    }

    lua_Integer len = static_cast<lua_Integer>(lua_objlen(L, idx));

    if (len != 3) {
        LUAUTILS_STACK_SIZE_ASSERT(L, 0)
        return false;
    }

    lua_rawgeti(L, idx, 1);
    lua_rawgeti(L, idx < 0 ? idx - 1 : idx, 2);
    lua_rawgeti(L, idx < 0 ? idx - 2 : idx, 3);

    int xType = lua_type(L, -3);
    int yType = lua_type(L, -2);
    int zType = lua_type(L, -1);

    if (xType == LUA_TNUMBER && yType == LUA_TNUMBER && zType == LUA_TNUMBER) {
        // values are in reverse order in the Lua stack
        if (x != nullptr) {
            *x = lua_tonumber(L, -3);
        }
        if (y != nullptr) {
            *y = lua_tonumber(L, -2);
        }
        if (z != nullptr) {
            *z = lua_tonumber(L, -1);
        }
        lua_pop(L, 3);

        LUAUTILS_STACK_SIZE_ASSERT(L, 0)
        return true;
    }

    lua_pop(L, 3);
    LUAUTILS_STACK_SIZE_ASSERT(L, 0)
    return false;
}

int lua_g_rotation_snprintf(lua_State *L, char* result, size_t maxLen, bool spacePrefix) {
    vx_assert(L != nullptr);
    return snprintf(result, maxLen, "%s[Rotation (global)]", spacePrefix ? " " : "");
}

int lua_rotation_snprintf(lua_State *L, char* result, size_t maxLen, bool spacePrefix) {
    vx_assert(L != nullptr);
    
    float3 f3;
    lua_rotation_get_euler(L, -1, &f3.x, &f3.y, &f3.z);
    
    return snprintf(result,
                    maxLen,
                    "%s[Rotation X: %.2f Y: %.2f Z: %.2f]",
                    spacePrefix ? " " : "",
                    static_cast<double>(f3.x),
                    static_cast<double>(f3.y),
                    static_cast<double>(f3.z));
}

// MARK: - static functions

static void _rotation_set_cache(lua_State *L, int idx, Quaternion *q) {
    rotation_userdata *ud = static_cast<rotation_userdata*>(lua_touserdata(L, idx));
    ud->x = static_cast<double>(q->x);
    ud->y = static_cast<double>(q->y);
    ud->z = static_cast<double>(q->z);
    ud->w = static_cast<double>(q->w);
    ud->eulerDirty = true;
}

static void _rotation_get_cache(lua_State *L, int idx, Quaternion *q) {
    rotation_userdata *ud = static_cast<rotation_userdata*>(lua_touserdata(L, idx));
    q->x = ud->x;
    q->y = ud->y;
    q->z = ud->z;
    q->w = ud->w;
}

static void _rotation_set_euler_cache(lua_State *L, int idx, float *x, float *y, float *z) {
    rotation_userdata *ud = static_cast<rotation_userdata*>(lua_touserdata(L, idx));
    if (x != nullptr) { ud->eulerX = static_cast<double>(*x); }
    if (y != nullptr) { ud->eulerY = static_cast<double>(*y); }
    if (z != nullptr) { ud->eulerZ = static_cast<double>(*z); }
    ud->eulerDirty = false;
}

static void _rotation_get_euler_cache(lua_State *L, int idx, float *x, float *y, float *z) {
    rotation_userdata *ud = static_cast<rotation_userdata*>(lua_touserdata(L, idx));
    if (x != nullptr) { *x = ud->eulerX; }
    if (y != nullptr) { *y = ud->eulerY; }
    if (z != nullptr) { *z = ud->eulerZ; }
}

static int _g_rotation_metatable_index(lua_State *L) {
    vx_assert(L != nullptr);
    LUAUTILS_STACK_SIZE(L)
    lua_pushnil(L);
    LUAUTILS_STACK_SIZE_ASSERT(L, 1);
    return 1;
}

static int _g_rotation_metatable_newindex(lua_State *L) {
    LUAUTILS_ERROR(L, "Rotation class is read-only");
    return 0;
}

static int _g_rotation_metatable_call(lua_State *L) {
    vx_assert(L != nullptr);
    LUAUTILS_ASSERT_ARG_TYPE(L, 1, ITEM_TYPE_G_ROTATION)
    LUAUTILS_STACK_SIZE(L)

    int nbParameters = lua_gettop(L);

    // empty constructor
    if (nbParameters == 1) {
        lua_rotation_push_new_euler(L, 0, 0, 0);
    }
    // euler constructor (3x number)
    else if (nbParameters == 4) {
        float x = 0.0f, y = 0.0f, z = 0.0f;
        if (lua_isnumber(L, 2)) {
            x = static_cast<float>(lua_tonumber(L, 2));
        } else {
            LUAUTILS_ERROR(L, "Rotation: argument 1 should be a number");
        }
        if (lua_isnumber(L, 3)) {
            y = static_cast<float>(lua_tonumber(L, 3));
        } else {
            LUAUTILS_ERROR(L, "Rotation: argument 2 should be a number");
        }
        if (lua_isnumber(L, 4)) {
            z = static_cast<float>(lua_tonumber(L, 4));
        } else {
            LUAUTILS_ERROR(L, "Rotation: argument 3 should be a number");
        }
        lua_rotation_push_new_euler(L, x, y, z);
    }
    // euler constructor (table or Number3)
    else if (nbParameters == 2) {
        float x = 0.0f, y = 0.0f, z = 0.0f;
        if (lua_number3_or_table_getXYZ(L, 2, &x, &y, &z) == false) {
            LUAUTILS_ERROR(L, "Rotation: argument should be a Number3 or table of numbers");
        }
        lua_rotation_push_new_euler(L, x, y, z);
    }
    // axis-angle constructor (table or Number3, number)
    else if (nbParameters == 3) {
        float3 axis;
        if (lua_number3_or_table_getXYZ(L, 2, &axis.x, &axis.y, &axis.z) == false) {
            LUAUTILS_ERROR(L, "Rotation: argument 1 should be a Number3 or table of numbers");
        }
        if (lua_isnumber(L, 3) == false) {
            LUAUTILS_ERROR(L, "Rotation: argument 2 should be a number");
        }
        const float angle = lua_tonumber(L, 3);

        Quaternion q; axis_angle_to_quaternion(&axis, angle, &q);
        float3 euler; quaternion_to_euler(&q, &euler);

        lua_rotation_push_new_euler(L, euler.x, euler.y, euler.z);
    } else {
        LUAUTILS_ERROR(L, "Rotation: incorrect argument count");
    }

    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
    return 1;
}

static int _rotation_metatable_index(lua_State *L) {
    vx_assert(L != nullptr);
    LUAUTILS_ASSERT_ARGS_COUNT(L, 2)
    LUAUTILS_ASSERT_ARG_TYPE(L, 1, ITEM_TYPE_ROTATION)
    LUAUTILS_STACK_SIZE(L)
    
    const char *key = lua_tostring(L, 2);
    
    if (strcmp(key, LUA_ROTATION_FIELD_X) == 0) {
        float x;
        if (lua_rotation_get_euler(L, 1, &x, nullptr, nullptr)) {
            lua_pushnumber(L, static_cast<double>(x));
        } else {
            lua_pushnil(L);
        }
    } else if (strcmp(key, LUA_ROTATION_FIELD_Y) == 0) {
        float y;
        if (lua_rotation_get_euler(L, 1, nullptr, &y, nullptr)) {
            lua_pushnumber(L, static_cast<double>(y));
        } else {
            lua_pushnil(L);
        }
    } else if (strcmp(key, LUA_ROTATION_FIELD_Z) == 0) {
        float z;
        if (lua_rotation_get_euler(L, 1, nullptr, nullptr, &z)) {
            lua_pushnumber(L, static_cast<double>(z));
        } else {
            lua_pushnil(L);
        }
    } else if (strcmp(key, LUA_ROTATION_FIELD_COPY) == 0) {
        lua_pushcfunction(L, _rotation_copy, "");
    } else if (strcmp(key, LUA_ROTATION_FIELD_INVERSE) == 0) {
        lua_pushcfunction(L, _rotation_inverse, "");
    } else if (strcmp(key, LUA_ROTATION_FIELD_LERP) == 0) {
        lua_pushcfunction(L, _rotation_lerp, "");
    } else if (strcmp(key, LUA_ROTATION_FIELD_SLERP) == 0) {
        lua_pushcfunction(L, _rotation_slerp, "");
    } else if (strcmp(key, LUA_ROTATION_FIELD_ANGLE) == 0) {
        lua_pushcfunction(L, _rotation_angle, "");
    } else if (strcmp(key, LUA_ROTATION_FIELD_SET) == 0) {
        lua_pushcfunction(L, _rotation_set, "");
    } else if (strcmp(key, LUA_ROTATION_FIELD_AXISANGLE) == 0) {
        lua_pushcfunction(L, _rotation_axisangle, "");
    } else if (strcmp(key, LUA_ROTATION_FIELD_LOOK) == 0) {
        lua_pushcfunction(L, _rotation_look, "");
    } else if (strcmp(key, LUA_ROTATION_FIELD_FROMTO) == 0) {
        lua_pushcfunction(L, _rotation_fromto, "");
    } else if (strcmp(key, LUA_ROTATION_FIELD_ADD_ON_SET_CALLBACK) == 0) {
        lua_pushcfunction(L, _rotation_addOnSetCallback, "");
    } else if (strcmp(key, LUA_ROTATION_FIELD_REMOVE_ON_SET_CALLBACK) == 0) {
        lua_pushcfunction(L, _rotation_removeOnSetCallback, "");
    } else if (strcmp(key, LUA_ROTATION_FIELD_ON_SET_CALLBACKS) == 0) {
        rotation_userdata* ud = static_cast<rotation_userdata*>(lua_touserdata(L, sizeof(rotation_userdata)));
        if (ud->hasOnSetCallbacks == false) {
            lua_pushnil(L);
        } else {
            lua_getfield(L, LUA_REGISTRYINDEX, LUA_ROTATION_REGISTRY_ON_SET_CALLBACKS);
            lua_pushlightuserdata(L, ud);
            lua_rawget(L, -2);
            lua_remove(L, -2); // remove registry table with all callback sets
        }
    } else {
        // if key is unknown, return nil
        lua_pushnil(L);
    }
    
    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
    return 1;
}

static int _rotation_metatable_new_index(lua_State *L) {
    vx_assert(L != nullptr);
    LUAUTILS_ASSERT_ARGS_COUNT(L, 3)
    LUAUTILS_ASSERT_ARG_TYPE(L, 1, ITEM_TYPE_ROTATION)
    LUAUTILS_STACK_SIZE(L)
    
    const char *key = lua_tostring(L, 2);
    if (strcmp(key, LUA_ROTATION_FIELD_X) == 0) {
        if (lua_isnumber(L, 3) == false) {
            LUAUTILS_ERROR(L, "Rotation: X can only be a number");
        }

        float x = static_cast<float>(lua_tonumber(L, 3));
        lua_rotation_set_euler(L, 1, &x, nullptr, nullptr);
    } else if (strcmp(key, LUA_ROTATION_FIELD_Y) == 0) {
        if (lua_isnumber(L, 3) == false) {
            LUAUTILS_ERROR(L, "Rotation: Y can only be a number");
        }

        float y = static_cast<float>(lua_tonumber(L, 3));
        lua_rotation_set_euler(L, 1, nullptr, &y, nullptr);
    } else if (strcmp(key, LUA_ROTATION_FIELD_Z) == 0) {
        if (lua_isnumber(L, 3) == false) {
            LUAUTILS_ERROR(L, "Rotation: Z can only be a number");
        }

        float z = static_cast<float>(lua_tonumber(L, 3));
        lua_rotation_set_euler(L, 1, nullptr, nullptr, &z);
    } else {
        LUAUTILS_ERROR_F(L, "Rotation.%s is not settable", key); // returns
    }
    
    LUAUTILS_STACK_SIZE_ASSERT(L, 0)
    return 0;
}

static void _rotation_metatable_gc(void *_ud) {
    rotation_userdata *ud = static_cast<rotation_userdata*>(_ud);
    // remove "on set" callbacks
    if (ud->hasOnSetCallbacks) {
        // TODO: found a way to cleanup those OnSet callbacks
        //        lua_getfield(L, LUA_REGISTRYINDEX, LUA_ROTATION_REGISTRY_ON_SET_CALLBACKS);
        //        lua_pushlightuserdata(L, ud);
        //        lua_pushnil(L);
        //        lua_rawset(L, -3);
        //        lua_pop(L, 1);
    }
    rotation_userdata_free_handlers(ud);
}

static int _rotation_metatable_add(lua_State *L) {
    vx_assert(L != nullptr);
    LUAUTILS_STACK_SIZE(L)

    // left operand: Rotation or vector (as Number3 or table)
    float3 eulerLeft;
    if (lua_rotation_get_euler(L, 1, &eulerLeft.x, &eulerLeft.y, &eulerLeft.z) == false &&
        lua_number3_or_table_getXYZ(L, 1, &eulerLeft.x, &eulerLeft.y, &eulerLeft.z) == false) {

        LUAUTILS_ERROR(L, "wrong arithmetic operation");
    }

    // right operand: Rotation or vector (as Number3 or table)
    float3 eulerRight;
    if (lua_rotation_get_euler(L, 2, &eulerRight.x, &eulerRight.y, &eulerRight.z) == false &&
        lua_number3_or_table_getXYZ(L, 2, &eulerRight.x, &eulerRight.y, &eulerRight.z) == false) {

        LUAUTILS_ERROR(L, "wrong arithmetic operation");
    }

    lua_rotation_push_new_euler(L,
                                eulerLeft.x + eulerRight.x,
                                eulerLeft.y + eulerRight.y,
                                eulerLeft.z + eulerRight.z);

    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
    return 1;
}

static int _rotation_metatable_sub(lua_State *L) {
    vx_assert(L != nullptr);
    LUAUTILS_STACK_SIZE(L)

    // left operand: Rotation or vector (as Number3 or table)
    float3 eulerLeft;
    if (lua_rotation_get_euler(L, 1, &eulerLeft.x, &eulerLeft.y, &eulerLeft.z) == false &&
        lua_number3_or_table_getXYZ(L, 1, &eulerLeft.x, &eulerLeft.y, &eulerLeft.z) == false) {

        LUAUTILS_ERROR(L, "wrong arithmetic operation");
    }

    // right operand: Rotation or vector (as Number3 or table)
    float3 eulerRight;
    if (lua_rotation_get_euler(L, 2, &eulerRight.x, &eulerRight.y, &eulerRight.z) == false &&
        lua_number3_or_table_getXYZ(L, 2, &eulerRight.x, &eulerRight.y, &eulerRight.z) == false) {

        LUAUTILS_ERROR(L, "wrong arithmetic operation");
    }

    lua_rotation_push_new_euler(L,
                                eulerLeft.x - eulerRight.x,
                                eulerLeft.y - eulerRight.y,
                                eulerLeft.z - eulerRight.z);

    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
    return 1;
}

static int _rotation_metatable_unm(lua_State *L) {
    vx_assert(L != nullptr);
    LUAUTILS_STACK_SIZE(L)

    Quaternion q;
    if (lua_rotation_get(L, 1, &q) == false) {
        LUAUTILS_INTERNAL_ERROR(L);
    }

    quaternion_op_inverse(&q);

    lua_rotation_push_new(L, q);

    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
    return 1;
}

static int _rotation_metatable_mul(lua_State *L) {
    vx_assert(L != nullptr);
    LUAUTILS_STACK_SIZE(L)

    // Note: one vector at most, at least one operand is a Rotation for this function to be called
    float3 vector;

    // left operand: Rotation or vector (as Number3 or table)
    Quaternion qLeft;
    bool isLeftRot = lua_rotation_get(L, 1, &qLeft);
    if (isLeftRot == false && lua_number3_or_table_getXYZ(L, 1, &vector.x, &vector.y, &vector.z) == false) {
        LUAUTILS_ERROR(L, "wrong arithmetic operation");
    }

    // right operand: Rotation or vector (as Number3 or table)
    Quaternion qRight;
    bool isRightRot = lua_rotation_get(L, 2, &qRight);
    if (isRightRot == false && lua_number3_or_table_getXYZ(L, 2, &vector.x, &vector.y, &vector.z) == false) {
        LUAUTILS_ERROR(L, "wrong arithmetic operation");
    }

    // quaternion mult
    if (isLeftRot && isRightRot) {
        lua_rotation_push_new(L, quaternion_op_mult(&qLeft, &qRight));
    }
    // rotate vector
    else {
        quaternion_rotate_vector(&qLeft, &vector);
        lua_number3_pushNewObject(L, vector.x, vector.y, vector.z);
    }

    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
    return 1;
}

static int _rotation_metatable_div(lua_State *L) {
    vx_assert(L != nullptr);
    LUAUTILS_ERROR(L, "wrong arithmetic operation, use the * operator to combine or apply rotations");
    return 0;
}

static int _rotation_metatable_idiv(lua_State *L) {
    vx_assert(L != nullptr);
    LUAUTILS_ERROR(L, "wrong arithmetic operation, use the * operator to combine or apply rotations");
    return 0;
}

static int _rotation_metatable_pow(lua_State *L) {
    vx_assert(L != nullptr);
    LUAUTILS_ERROR(L, "wrong arithmetic operation, use the * operator to combine or apply rotations");
    return 0;
}

static int _rotation_metatable_eq(lua_State *L) {
    vx_assert(L != nullptr);
    LUAUTILS_STACK_SIZE(L)

    // left operand: Rotation or rotation (as Number3 or table)
    Quaternion left;
    if (lua_rotation_get(L, 1, &left) == false) {
        float3 euler;
        if (lua_number3_or_table_getXYZ(L, 1, &euler.x, &euler.y, &euler.z)) {
            euler_to_quaternion_vec(&euler, &left);
        } else {
            LUAUTILS_ERROR(L, "wrong arithmetic operation");
        }
    }

    // right operand: Rotation or rotation (as Number3 or table)
    Quaternion right;
    if (lua_rotation_get(L, 2, &right) == false) {
        float3 euler;
        if (lua_number3_or_table_getXYZ(L, 2, &euler.x, &euler.y, &euler.z)) {
            euler_to_quaternion_vec(&euler, &right);
        } else {
            LUAUTILS_ERROR(L, "wrong arithmetic operation");
        }
    }

    lua_pushboolean(L, quaternion_is_equal(&left, &right, EPSILON_ZERO_RAD));
    
    LUAUTILS_STACK_SIZE_ASSERT(L, 1);
    return 1;
}

static int _rotation_metatable_tostring(lua_State *L) {
    vx_assert(L != nullptr);
    LUAUTILS_ASSERT_ARGS_COUNT(L, 1)
    LUAUTILS_ASSERT_ARG_TYPE(L, 1, ITEM_TYPE_ROTATION)
    LUAUTILS_STACK_SIZE(L)

    float3 self;
    if (lua_rotation_get_euler(L, 1, &self.x, &self.y, &self.z) == false) {
        LUAUTILS_INTERNAL_ERROR(L);
    }

    const int buff_size = 1 + snprintf(nullptr,
                                       0, "[Rotation X: %.2f Y: %.2f Z: %.2f]",
                                       static_cast<double>(self.x),
                                       static_cast<double>(self.y),
                                       static_cast<double>(self.z)); // to know the size of the string
    std::string str(buff_size, '\0');
    int return_value = snprintf(&str[0], buff_size, "[Rotation X: %.2f Y: %.2f Z: %.2f]",
                                static_cast<double>(self.x),
                                static_cast<double>(self.y),
                                static_cast<double>(self.z)); // convert number3 into a string
    if (return_value < 0 || return_value > buff_size) { //if snprintf() fail
        LUAUTILS_ERROR(L, "Rotation:Failed to convert to string");
    }
    lua_pushlstring(L, str.c_str(), buff_size);

    LUAUTILS_STACK_SIZE_ASSERT(L, 1);
    return 1;
}

static int _rotation_copy(lua_State *L) {
    vx_assert(L != nullptr);
    LUAUTILS_STACK_SIZE(L)

    const int argCount = lua_gettop(L);

    if (argCount < 1 || lua_utils_getObjectType(L, 1) != ITEM_TYPE_ROTATION) {
        LUAUTILS_ERROR(L, "Rotation:Copy - function should be called with `:`");
    }

    if (argCount != 1) {
        LUAUTILS_ERROR(L, "Rotation:Copy - wrong number of arguments");
    }

    Quaternion q;
    if (lua_rotation_get(L, 1, &q) == false) {
        LUAUTILS_INTERNAL_ERROR(L);
    }

    lua_rotation_push_new(L, q);

    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
    return 1;
}

static int _rotation_inverse(lua_State *L) {
    vx_assert(L != nullptr);
    LUAUTILS_STACK_SIZE(L)

    const int argCount = lua_gettop(L);

    if (argCount < 1 || lua_utils_getObjectType(L, 1) != ITEM_TYPE_ROTATION) {
        LUAUTILS_ERROR(L, "Rotation:Inverse - function should be called with `:`");
    }

    if (argCount != 1) {
        LUAUTILS_ERROR(L, "Rotation:Inverse - wrong number of arguments");
    }

    Quaternion q;
    if (lua_rotation_get(L, 1, &q) == false) {
        LUAUTILS_INTERNAL_ERROR(L);
    }

    quaternion_op_inverse(&q);

    lua_rotation_set(L, 1, &q);

    LUAUTILS_STACK_SIZE_ASSERT(L, 0)
    return 0;
}

static int __rotation_lerp(lua_State *L, bool spherical) {
    vx_assert(L != nullptr);
    LUAUTILS_STACK_SIZE(L)

    const int argCount = lua_gettop(L);

    if (argCount < 1 || lua_utils_getObjectType(L, 1) != ITEM_TYPE_ROTATION) {
        LUAUTILS_ERROR(L, spherical ? "Rotation:Slerp - function should be called with `:`" :
                          "Rotation:Lerp - function should be called with `:`");
    }

    Quaternion from;
    if (lua_rotation_or_table_get(L, 2, &from) == false) {
        LUAUTILS_ERROR(L, spherical ? "Rotation:Slerp - first argument should be a Rotation or table of numbers" :
                          "Rotation:Lerp - first argument should be a Rotation or table of numbers");
    }

    Quaternion to;
    if (lua_rotation_or_table_get(L, 3, &to) == false) {
        LUAUTILS_ERROR(L, spherical ? "Rotation:Slerp - second argument should be a Rotation or table of numbers" :
                          "Rotation:Lerp - second argument should be a Rotation or table of numbers");
    }

    if (lua_isnumber(L, 4) == false) {
        LUAUTILS_ERROR(L, spherical ? "Rotation:Slerp - third argument should be a number" :
                          "Rotation:Lerp - third argument should be a number");
    }
    const float v = lua_tonumber(L, 4);

    if (spherical) {
        quaternion_op_slerp(&from, &to, &from, v);
    } else {
        quaternion_op_lerp(&from, &to, &from, v);
    }

    lua_rotation_set(L, 1, &from);

    LUAUTILS_STACK_SIZE_ASSERT(L, 0)
    return 0;
}

static int _rotation_lerp(lua_State *L) { return __rotation_lerp(L, false); }
static int _rotation_slerp(lua_State *L) { return __rotation_lerp(L, true); }

static int _rotation_set(lua_State *L) {
    vx_assert(L != nullptr);
    LUAUTILS_STACK_SIZE(L)

    const int argCount = lua_gettop(L);

    if (argCount < 1 || lua_utils_getObjectType(L, 1) != ITEM_TYPE_ROTATION) {
        LUAUTILS_ERROR(L, "Rotation:Set - function should be called with `:`");
    }

    if (argCount == 4) {
        if (lua_isnumber(L, 2) == false) {
            LUAUTILS_ERROR(L, "Rotation:Set - argument 1 should be a number");
        }
        float x = lua_tonumber(L, 2);
        if (lua_isnumber(L, 3) == false) {
            LUAUTILS_ERROR(L, "Rotation:Set - argument 2 should be a number");
        }
        float y = lua_tonumber(L, 3);
        if (lua_isnumber(L, 4) == false) {
            LUAUTILS_ERROR(L, "Rotation:Set - argument 3 should be a number");
        }
        float z = lua_tonumber(L, 4);

        lua_rotation_set_euler(L, 1, &x, &y, &z);
    } else if (argCount == 2) {
        
        float3 euler;
        
        if (lua_rotation_get_euler(L, 2, &euler.x, &euler.y, &euler.z)) {
            lua_rotation_set_euler(L, 1, &euler.x, &euler.y, &euler.z);
        }
        
        else if (lua_number3_or_table_getXYZ(L, 2, &euler.x, &euler.y, &euler.z)) {
            lua_rotation_set_euler(L, 1, &euler.x, &euler.y, &euler.z);
        }

        else {
            LUAUTILS_ERROR(L, "Rotation:Set - argument should be a Number3 or table of numbers");
        }


    } else {
        LUAUTILS_ERROR(L, "Rotation:Set - wrong number of arguments");
    }

    LUAUTILS_STACK_SIZE_ASSERT(L, 0)
    return 0;
}

static int _rotation_angle(lua_State *L) {
    vx_assert(L != nullptr);
    LUAUTILS_STACK_SIZE(L)

    const int argCount = lua_gettop(L);

    if (argCount < 1 || lua_utils_getObjectType(L, 1) != ITEM_TYPE_ROTATION) {
        LUAUTILS_ERROR(L, "Rotation:Angle - function should be called with `:`");
    }

    Quaternion self;
    if (lua_rotation_get(L, 1, &self) == false) {
        LUAUTILS_INTERNAL_ERROR(L);
    }

    Quaternion other;
    if (lua_rotation_or_table_get(L, 2, &other) == false) {
        LUAUTILS_ERROR(L, "Rotation:Angle - argument should be a Rotation or table of numbers");
    }

    const float angle = quaternion_angle_between(&self, &other);

    lua_pushnumber(L, static_cast<double>(angle));

    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
    return 1;
}

static int _rotation_axisangle(lua_State *L) {
    vx_assert(L != nullptr);
    LUAUTILS_STACK_SIZE(L)

    const int argCount = lua_gettop(L);

    if (argCount < 1 || lua_utils_getObjectType(L, 1) != ITEM_TYPE_ROTATION) {
        LUAUTILS_ERROR(L, "Rotation:SetAxisAngle - function should be called with `:`");
    }

    float3 axis;
    if (lua_number3_or_table_getXYZ(L, 2, &axis.x, &axis.y, &axis.z) == false) {
        LUAUTILS_ERROR(L, "Rotation:SetAxisAngle - first argument should be a Number3 or table of numbers");
    }

    if (lua_isnumber(L, 3) == false) {
        LUAUTILS_ERROR(L, "Rotation:SetAxisAngle - second argument should be a number");
    }
    const float angle = lua_tonumber(L, 3);

    Quaternion q; axis_angle_to_quaternion(&axis, angle, &q);

    lua_rotation_set(L, 1, &q);

    LUAUTILS_STACK_SIZE_ASSERT(L, 0)
    return 0;
}

static int _rotation_look(lua_State *L) {
    vx_assert(L != nullptr);
    LUAUTILS_STACK_SIZE(L)

    const int argCount = lua_gettop(L);

    if (argCount < 1 || lua_utils_getObjectType(L, 1) != ITEM_TYPE_ROTATION) {
        LUAUTILS_ERROR(L, "Rotation:SetLookRotation - function should be called with `:`");
    }

    float3 forward;
    if (lua_number3_or_table_getXYZ(L, 2, &forward.x, &forward.y, &forward.z) == false) {
        LUAUTILS_ERROR(L, "Rotation:SetLookRotation - first argument should be a Number3 or table of numbers");
    }

    const bool useUp = argCount >= 3;

    float3 up;
    if (useUp && lua_number3_or_table_getXYZ(L, 3, &up.x, &up.y, &up.z) == false) {
        LUAUTILS_ERROR(L, "Rotation:SetLookRotation - second argument should be a Number3 or table of numbers");
    }

    Quaternion *q = quaternion_from_to_vectors(&float3_forward, &forward, useUp ? &up : nullptr);

    lua_rotation_set(L, 1, q);

    LUAUTILS_STACK_SIZE_ASSERT(L, 0)
    return 0;
}

static int _rotation_fromto(lua_State *L) {
    vx_assert(L != nullptr);
    LUAUTILS_STACK_SIZE(L)

    const int argCount = lua_gettop(L);

    if (argCount < 1 || lua_utils_getObjectType(L, 1) != ITEM_TYPE_ROTATION) {
        LUAUTILS_ERROR(L, "Rotation:SetFromToRotation - function should be called with `:`");
    }

    float3 from;
    if (lua_number3_or_table_getXYZ(L, 2, &from.x, &from.y, &from.z) == false) {
        LUAUTILS_ERROR(L, "Rotation:SetFromToRotation - first argument should be a Number3 or table of numbers");
    }

    float3 to;
    if (lua_number3_or_table_getXYZ(L, 3, &to.x, &to.y, &to.z) == false) {
        LUAUTILS_ERROR(L, "Rotation:SetFromToRotation - second argument should be a Number3 or table of numbers");
    }

    const bool useUp = argCount >= 4;

    float3 up;
    if (useUp && lua_number3_or_table_getXYZ(L, 4, &up.x, &up.y, &up.z) == false) {
        LUAUTILS_ERROR(L, "Rotation:SetFromToRotation - third argument should be a Number3 or table of numbers");
    }

    Quaternion *q = quaternion_from_to_vectors(&from, &to, useUp ? &up : nullptr);

    lua_rotation_set(L, 1, q);

    LUAUTILS_STACK_SIZE_ASSERT(L, 0)
    return 0;
}

static int _rotation_addOnSetCallback(lua_State *L) {
    LUAUTILS_STACK_SIZE(L)
    
    const int argCount = lua_gettop(L);

    if (argCount < 1 || lua_utils_getObjectType(L, 1) != ITEM_TYPE_ROTATION) {
        LUAUTILS_ERROR(L, "Rotation:AddOnSetCallback - function should be called with `:`");
    }
    
    if (argCount < 2 || lua_type(L, 2) != LUA_TFUNCTION) {
        LUAUTILS_ERROR(L, "Rotation:AddOnSetCallback - first argument should be a function");
    }

    rotation_userdata* ud = static_cast<rotation_userdata*>(lua_touserdata(L, 1));
    lua_getfield(L, LUA_REGISTRYINDEX, LUA_ROTATION_REGISTRY_ON_SET_CALLBACKS);

    lua_pushlightuserdata(L, ud);

    if (lua_rawget(L, -2) == LUA_TNIL) {
        lua_pop(L, 1); // pop nil
        lua_pushlightuserdata(L, ud); // user userdata ptr as key
        lua_newtable(L);
        {
            lua_pushvalue(L, 2);
            lua_rawseti(L, -2, lua_objlen(L, -2) + 1);
        }
        lua_rawset(L, -3);
    } else {
        lua_pushvalue(L, 2);
        lua_rawseti(L, -2, lua_objlen(L, -2) + 1);
        lua_pop(L, 1);
    }

    ud->hasOnSetCallbacks = true;

    lua_pop(L, 1); // pop on set callbacks table

    LUAUTILS_STACK_SIZE_ASSERT(L, 0)
    return 0;
}

static int _rotation_removeOnSetCallback(lua_State *L) {
    LUAUTILS_STACK_SIZE(L)
    
    const int argCount = lua_gettop(L);
    
    if (argCount < 1 || lua_utils_getObjectType(L, 1) != ITEM_TYPE_ROTATION) {
        LUAUTILS_ERROR(L, "Rotation:RemoveOnSetCallback - function should be called with `:`");
    }
    
    if (argCount < 2 || lua_type(L, 2) != LUA_TFUNCTION) {
        LUAUTILS_ERROR(L, "Rotation:RemoveOnSetCallback - first argument should be a function");
    }
   
    rotation_userdata* ud = static_cast<rotation_userdata*>(lua_touserdata(L, 1));
    if (ud->hasOnSetCallbacks == false) {
        return 0;
    }

    lua_getfield(L, LUA_REGISTRYINDEX, LUA_ROTATION_REGISTRY_ON_SET_CALLBACKS);
    lua_pushlightuserdata(L, ud);

    if (lua_rawget(L, -2) != LUA_TNIL) {
        lua_pushnil(L); // callbacks at -2, nil at -1
        bool found = false;
        while(lua_next(L, -2) != 0) {
            if (found) {
                // offset following values
                lua_pushvalue(L, -1);
                // callbacks at -4, key at -3, value at -2, value at -1
                lua_rawseti(L, -4, lua_tointeger(L, -3) - 1);
            } else {
                // callbacks at -3, key at -2, value at -1
                if (lua_topointer(L, 2) == lua_topointer(L, -1)) {
                    // found callback
                    found = true;
                }
            }

            if (found) {
                lua_pushnil(L);
                // callbacks at -4, key at -3, value at -2, nil at -1
                lua_rawseti(L, -4, lua_tointeger(L, -3));
            }
            lua_pop(L, 1); // pop value
        }
    }

    lua_pop(L, 2); // pop callbacks and storage

    LUAUTILS_STACK_SIZE_ASSERT(L, 0)
    return 0;
}
