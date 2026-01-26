// -------------------------------------------------------------
//  Cubzh
//  lua_number3.cpp
//  Created by Gaetan de Villele on July 24, 2020.
// -------------------------------------------------------------

#include "lua_number3.hpp"

// C++
#include <cstring>
#include <math.h>

// Lua
#include "lua.hpp"
#include "lua_utils.hpp"
#include "lua_rotation.hpp"

// sandbox
#include "sbs.hpp"

// xptools
#include "xptools.h"

// engine
#include "config.h" // for vx_assert
#include "weakptr.h"

#define LUA_NUMBER3_REGISTRY_ON_SET_CALLBACKS "_n3_osc"

#define LUA_G_NUMBER3_INSTANCE_METATABLE "__imt"
#define LUA_G_NUMBER3_FIELD_ZERO "Zero"
#define LUA_G_NUMBER3_FIELD_ONE "One"
#define LUA_G_NUMBER3_FIELD_RIGHT "Right"
#define LUA_G_NUMBER3_FIELD_LEFT "Left"
#define LUA_G_NUMBER3_FIELD_FORWARD "Forward"
#define LUA_G_NUMBER3_FIELD_BACKWARD "Backward"
#define LUA_G_NUMBER3_FIELD_UP "Up"
#define LUA_G_NUMBER3_FIELD_DOWN "Down"

#define LUA_NUMBER3_FIELD_X "X"
#define LUA_NUMBER3_FIELD_Y "Y"
#define LUA_NUMBER3_FIELD_Z "Z"
#define LUA_NUMBER3_FIELD_WIDTH "Width"
#define LUA_NUMBER3_FIELD_HEIGHT "Height"
#define LUA_NUMBER3_FIELD_DEPTH "Depth"
#define LUA_NUMBER3_FIELD_ROTATE "Rotate"

#define LUA_NUMBER3_FIELD_LENGTH "Length" // read-write
#define LUA_NUMBER3_FIELD_SQUARED_LENGTH "SquaredLength" // read-write
#define LUA_NUMBER3_FIELD_NORMALIZE "Normalize" // function
#define LUA_NUMBER3_FIELD_DOT "Dot" // function
#define LUA_NUMBER3_FIELD_CROSS "Cross" // function
#define LUA_NUMBER3_FIELD_COPY "Copy" // function
#define LUA_NUMBER3_FIELD_LERP "Lerp" // function
#define LUA_NUMBER3_FIELD_SET "Set" // function
#define LUA_NUMBER3_FIELD_ANGLE "Angle" // function

#define LUA_NUMBER3_FIELD_ON_SET_CALLBACKS "OnSetCallbacks" // callbacks
#define LUA_NUMBER3_FIELD_ADD_ON_SET_CALLBACK "AddOnSetCallback" // function, receives a callback
#define LUA_NUMBER3_FIELD_REMOVE_ON_SET_CALLBACK "RemoveOnSetCallback" // function, receives a callback

#define LUA_NUMBER3_USERDATA_TYPE_DOUBLES 0
#define LUA_NUMBER3_USERDATA_TYPE_HANDLER 1

typedef struct number3_userdata {
    void *userdata; // double[3] by default, but can also be a number3_C_handler pointer
    uint8_t type; // LUA_NUMBER3_DATA_TYPE_DOUBLES = double[3], LUA_NUMBER3_USERDATA_TYPE_HANDLER = number3_C_handler
    bool hasOnSetCallbacks; // false by default, fetch callbacks and trigger them only if true
} number3_userdata;

typedef struct number3_C_handler {
    number3_C_handler_set set;
    number3_C_handler_get get;
    number3_C_handler_userdata userdata;
    bool isUserdataWeakptr;
} number3_C_handler;

static void number3_userdata_flush(number3_userdata *ud) {
    switch (ud->type) {
        case LUA_NUMBER3_USERDATA_TYPE_DOUBLES:
        {
            free(ud->userdata);
            break;
        }
        case LUA_NUMBER3_USERDATA_TYPE_HANDLER:
        {
            number3_C_handler *handler = static_cast<number3_C_handler*>(ud->userdata);
            if (handler->isUserdataWeakptr) {
                weakptr_release(static_cast<Weakptr*>(handler->userdata.ptr));
            }
            free(handler);
            break;
        }
        default:
        {
            vx_assert(true); // not supposed to happen
            break;
        }
    }
}

// --------------------------------------------------
//
// MARK: - Static functions' prototypes -
//
// --------------------------------------------------

/// __index for global Number3
static int _g_number3_metatable_index(lua_State *L);

/// __newindex for global Number3
static int _g_number3_metatable_newindex(lua_State *L);

/// __call for global Number3
static int _g_number3_metatable_call(lua_State *L);

/// __index for Number3 instances
static int _number3_metatable_index(lua_State *L);

/// __gc for Number3 instances
static void _number3_metatable_gc(void *ud);

/// __new_index for Number3 instances
static int _number3_metatable_new_index(lua_State *L);

/// __add for Number3 instances
static int _number3_metatable_add(lua_State *L);

/// __sub for Number3 instances
static int _number3_metatable_sub(lua_State *L);

/// __unm for Number3 instances
static int _number3_metatable_unm(lua_State *L);

/// __mul for Number3 instances
static int _number3_metatable_mul(lua_State *L);

/// __div for Number3 instances
static int _number3_metatable_div(lua_State *L);

/// __idiv for Number3 instances
static int _number3_metatable_idiv(lua_State *L);

/// __pow for Number3 instances
static int _number3_metatable_pow(lua_State *L);

///
static int _number3_tostring(lua_State *L);

///
static int _number3_eq(lua_State *L);

static int _number3_rotate(lua_State *L);
static int _number3_normalize(lua_State *L);
static int _number3_dot(lua_State *L);
static int _number3_cross(lua_State *L);
static int _number3_copy(lua_State *L);
static int _number3_lerp(lua_State *L);
static int _number3_set(lua_State *L);
static int _number3_angle(lua_State *L);

static int _number3_addOnSetCallback(lua_State *L);
static int _number3_removeOnSetCallback(lua_State *L);

// --------------------------------------------------
//
// MARK: - Exposed functions -
//
// --------------------------------------------------

void lua_g_number3_create_and_push(lua_State *L) {
    vx_assert(L != nullptr);
    LUAUTILS_STACK_SIZE(L)
    
    lua_newuserdata(L, 1);
    lua_newtable(L); // metatable
    {
        lua_pushstring(L, "__index");
        lua_pushcfunction(L, _g_number3_metatable_index, "");
        lua_rawset(L, -3);
        
        lua_pushstring(L, "__newindex");
        lua_pushcfunction(L, _g_number3_metatable_newindex, "");
        lua_rawset(L, -3);
        
        lua_pushstring(L, "__call");
        lua_pushcfunction(L, _g_number3_metatable_call, "");
        lua_rawset(L, -3);

        lua_pushstring(L, "__metatable");
        lua_pushboolean(L, false);
        lua_rawset(L, -3);

        lua_pushstring(L, "__type");
        lua_pushstring(L, "Number3Interface");
        lua_rawset(L, -3);

        lua_pushstring(L, "__typen"); // used to log tables
        lua_pushinteger(L, ITEM_TYPE_G_NUMBER3);
        lua_rawset(L, -3);

        lua_pushstring(L, LUA_G_NUMBER3_INSTANCE_METATABLE);
        lua_newtable(L);
        {
            lua_pushstring(L, "__index");
            lua_pushcfunction(L, _number3_metatable_index, "");
            lua_rawset(L, -3);
            
            lua_pushstring(L, "__newindex");
            lua_pushcfunction(L, _number3_metatable_new_index, "");
            lua_rawset(L, -3);
            
            lua_pushstring(L, "__add");
            lua_pushcfunction(L, _number3_metatable_add, "");
            lua_rawset(L, -3);
            
            lua_pushstring(L, "__sub");
            lua_pushcfunction(L, _number3_metatable_sub, "");
            lua_rawset(L, -3);
            
            lua_pushstring(L, "__unm");
            lua_pushcfunction(L, _number3_metatable_unm, "");
            lua_rawset(L, -3);
            
            lua_pushstring(L, "__mul");
            lua_pushcfunction(L, _number3_metatable_mul, "");
            lua_rawset(L, -3);
            
            lua_pushstring(L, "__div");
            lua_pushcfunction(L, _number3_metatable_div, "");
            lua_rawset(L, -3);
            
            lua_pushstring(L, "__idiv");
            lua_pushcfunction(L, _number3_metatable_idiv, "");
            lua_rawset(L, -3);
            
            lua_pushstring(L, "__pow");
            lua_pushcfunction(L, _number3_metatable_pow, "");
            lua_rawset(L, -3);
            
            lua_pushstring(L, "__eq");
            lua_pushcfunction(L, _number3_eq, "");
            lua_rawset(L, -3);

            lua_pushstring(L, "__tostring");
            lua_pushcfunction(L, _number3_tostring, "");
            lua_rawset(L, -3);

            lua_pushstring(L, "__metatable");
            lua_pushboolean(L, false);
            lua_rawset(L, -3);

            lua_pushstring(L, "__type");
            lua_pushstring(L, "Number3");
            lua_rawset(L, -3);

            lua_pushstring(L, "__typen");
            lua_pushinteger(L, ITEM_TYPE_NUMBER3);
            lua_rawset(L, -3);
        }
        lua_rawset(L, -3);
    }
    lua_setmetatable(L, -2);

    lua_newtable(L);
    lua_setfield(L, LUA_REGISTRYINDEX, LUA_NUMBER3_REGISTRY_ON_SET_CALLBACKS);

    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
}

bool lua_number3_pushNewObject(lua_State *L,
                                const float x,
                                const float y,
                                const float z) {
    vx_assert(L != nullptr);
    
    LUAUTILS_STACK_SIZE(L)

    number3_userdata* ud = static_cast<number3_userdata*>(lua_newuserdatadtor(L, sizeof(number3_userdata), _number3_metatable_gc));
    ud->type = LUA_NUMBER3_USERDATA_TYPE_DOUBLES;
    double *n3 = static_cast<double*>(malloc(sizeof(double) * 3));
    n3[0] = static_cast<double>(x);
    n3[1] = static_cast<double>(y);
    n3[2] = static_cast<double>(z);
    ud->userdata = n3;
    ud->hasOnSetCallbacks = false;

    // set shared metatable
    lua_getglobal(L, P3S_LUA_G_NUMBER3);
    LUA_GET_METAFIELD(L, -1, LUA_G_NUMBER3_INSTANCE_METATABLE);
    lua_remove(L, -2);
    lua_setmetatable(L, -2);
    
    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
    return true;
}

bool lua_number3_or_table_getXYZ(lua_State *L,
                                 const int idx,
                                 float *x,
                                 float *y,
                                 float *z) {
    vx_assert(L != nullptr);
    LUAUTILS_STACK_SIZE(L)
    
    if (lua_number3_getXYZ(L, idx, x, y, z)) {
        LUAUTILS_STACK_SIZE_ASSERT(L, 0)
        return true;
    }
    
    // see if table can be considered as a Number3
    
    // check value is a table
    if (lua_istable(L, idx) == false) {
        LUAUTILS_STACK_SIZE_ASSERT(L, 0)
        return false;
    }
    
    size_t len = lua_objlen(L, idx);

    int type;
    if (x != nullptr) {
        *x = 0.0f;
    }
    if (y != nullptr) {
        *y = 0.0f;
    }
    if (z != nullptr) {
        *z = 0.0f;
    }

    if (len >= 1) {
        lua_rawgeti(L, idx, 1);
        type = lua_type(L, -1);

        if (type == LUA_TNUMBER) {
            if (x != nullptr) {
                *x = lua_tonumber(L, -1);
            }
            lua_pop(L, 1);
        } else if (type == LUA_TNIL) {
            lua_pop(L, 1);
            LUAUTILS_STACK_SIZE_ASSERT(L, 0)
            return true;
        } else {
            lua_pop(L, 1);
            LUAUTILS_STACK_SIZE_ASSERT(L, 0)
            return false;
        }
    }

    if (len >= 2) {
        lua_rawgeti(L, idx, 2);
        type = lua_type(L, -1);

        if (type == LUA_TNUMBER) {
            if (y != nullptr) {
                *y = lua_tonumber(L, -1);
            }
            lua_pop(L, 1);
        } else if (type == LUA_TNIL) {
            lua_pop(L, 1);
            LUAUTILS_STACK_SIZE_ASSERT(L, 0)
            return true;
        } else {
            lua_pop(L, 1);
            LUAUTILS_STACK_SIZE_ASSERT(L, 0)
            return false;
        }
    }

    if (len >= 3) {
        lua_rawgeti(L, idx, 3);
        type = lua_type(L, -1);

        if (type == LUA_TNUMBER) {
            if (z != nullptr) {
                *z = lua_tonumber(L, -1);
            }
            lua_pop(L, 1);
        } else if (type == LUA_TNIL) {
            lua_pop(L, 1);
            LUAUTILS_STACK_SIZE_ASSERT(L, 0)
            return true;
        } else {
            lua_pop(L, 1);
            LUAUTILS_STACK_SIZE_ASSERT(L, 0)
            return false;
        }
    }

    LUAUTILS_STACK_SIZE_ASSERT(L, 0)
    return true;
}

bool lua_number3_or_table_or_numbers_getXYZ(lua_State *L,
                                            const int idx,
                                            float *x,
                                            float *y,
                                            float *z) {
    vx_assert(L != nullptr);
    LUAUTILS_STACK_SIZE(L)

    if (lua_number3_or_table_getXYZ(L, idx, x, y, z)) {
        LUAUTILS_STACK_SIZE_ASSERT(L, 0)
        return true;
    }

    if (lua_isnumber(L, idx) && lua_isnumber(L, idx + 1) && lua_isnumber(L, idx + 2)) {
        *x = lua_tonumber(L, idx);
        *y = lua_tonumber(L, idx + 1);
        *z = lua_tonumber(L, idx + 2);

        LUAUTILS_STACK_SIZE_ASSERT(L, 0)
        return true;
    }

    LUAUTILS_STACK_SIZE_ASSERT(L, 0)
    return false;
}

///
bool lua_number3_getXYZ(lua_State *L,
                         const int idx,
                         float *x,
                         float *y,
                         float *z) {
    vx_assert(L != nullptr);

    LUAUTILS_STACK_SIZE(L)

    if (lua_utils_getObjectType(L, idx) != ITEM_TYPE_NUMBER3) {
        LUAUTILS_STACK_SIZE_ASSERT(L, 0)
        return false;
    }

    number3_userdata *ud = static_cast<number3_userdata*>(lua_touserdata(L, idx));
    switch (ud->type) {
        case LUA_NUMBER3_USERDATA_TYPE_DOUBLES:
        {
            double *n3 = static_cast<double*>(ud->userdata);
            if (x != nullptr) {
                *x = static_cast<float>(n3[0]);
            }
            if (y != nullptr) {
                *y = static_cast<float>(n3[1]);
            }
            if (z != nullptr) {
                *z = static_cast<float>(n3[2]);
            }
            break;
        }
        case LUA_NUMBER3_USERDATA_TYPE_HANDLER:
        {
            number3_C_handler *handler = static_cast<number3_C_handler*>(ud->userdata);
            vx_assert(handler->get != nullptr);
            handler->get(x, y, z, L, &handler->userdata);
            break;
        }
        default:
        {
            vx_assert(true); // not supposed to happen
            break;
        }
    }

    LUAUTILS_STACK_SIZE_ASSERT(L, 0)
    return true;
}

bool lua_number3_setXYZ(lua_State *L,
                        const int idx,
                        const float *x,
                        const float *y,
                        const float *z) {
    vx_assert(L != nullptr);
    LUAUTILS_STACK_SIZE(L)

    if (lua_utils_getObjectType(L, idx) != ITEM_TYPE_NUMBER3) {
        LUAUTILS_STACK_SIZE_ASSERT(L, 0)
        return false;
    }

    number3_userdata *ud = static_cast<number3_userdata*>(lua_touserdata(L, idx));
    switch (ud->type) {
        case LUA_NUMBER3_USERDATA_TYPE_DOUBLES:
        {
            double *n3 = static_cast<double*>(ud->userdata);
            if (x != nullptr) {
                n3[0] = static_cast<double>(*x);
            }
            if (y != nullptr) {
                n3[1] = static_cast<double>(*y);
            }
            if (z != nullptr) {
                n3[2] = static_cast<double>(*z);
            }
            break;
        }
        case LUA_NUMBER3_USERDATA_TYPE_HANDLER:
        {
            number3_C_handler *handler = static_cast<number3_C_handler*>(ud->userdata);
            if (handler->set != nullptr) {
                handler->set(x, y, z, L, &handler->userdata);
            }
            break;
        }
        default:
        {
            vx_assert(true); // not supposed to happen
            break;
        }
    }

    if (ud->hasOnSetCallbacks) {
        lua_getfield(L, LUA_REGISTRYINDEX, LUA_NUMBER3_REGISTRY_ON_SET_CALLBACKS);
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

bool lua_number3_setHandlers(lua_State *L,
                             const int idx,
                             number3_C_handler_set set,
                             number3_C_handler_get get,
                             number3_C_handler_userdata userdata,
                             bool isUserdataWeakptr) {
    vx_assert(L != nullptr);
    vx_assert(lua_utils_getObjectType(L, idx) == ITEM_TYPE_NUMBER3);
    LUAUTILS_STACK_SIZE(L)

    number3_userdata *ud = static_cast<number3_userdata*>(lua_touserdata(L, idx));
    number3_userdata_flush(ud);

    number3_C_handler *handlers = static_cast<number3_C_handler*>(malloc(sizeof(number3_C_handler)));
    handlers->set = set;
    handlers->get = get;
    handlers->userdata = userdata;
    handlers->isUserdataWeakptr = isUserdataWeakptr;
    
    ud->type = LUA_NUMBER3_USERDATA_TYPE_HANDLER;
    ud->userdata = handlers;
    
    LUAUTILS_STACK_SIZE_ASSERT(L, 0)
    return true;
}

/// Prints global Number3
int lua_g_number3_snprintf(lua_State *L,
                            char* result,
                            size_t maxLen,
                            bool spacePrefix) {
    vx_assert(L != nullptr);
    return snprintf(result, maxLen, "%s[Number3 (global)]", spacePrefix ? " " : "");
}

/// Prints Number3 instance
int lua_number3_snprintf(lua_State *L,
                          char* result,
                          size_t maxLen,
                          bool spacePrefix) {
    vx_assert(L != nullptr);
    
    float3 f3;
    lua_number3_getXYZ(L, -1, &f3.x, &f3.y, &f3.z);
    
    return snprintf(result,
                    maxLen,
                    "%s[Number3 X: %.2f Y: %.2f Z: %.2f]",
                    spacePrefix ? " " : "",
                    static_cast<double>(f3.x),
                    static_cast<double>(f3.y),
                    static_cast<double>(f3.z));
}

// --------------------------------------------------
//
// MARK: - static functions
//
// --------------------------------------------------

/// arguments
/// - lua table : global Number3 table
/// - lua string : the key that is being accessed
static int _g_number3_metatable_index(lua_State *L) {
    vx_assert(L != nullptr);
    LUAUTILS_STACK_SIZE(L)

    const char *key = lua_tostring(L, 2);
    if (strcmp(key, LUA_G_NUMBER3_FIELD_ZERO) == 0) {
        lua_number3_pushNewObject(L, 0, 0, 0);
    } else if (strcmp(key, LUA_G_NUMBER3_FIELD_ONE) == 0) {
        lua_number3_pushNewObject(L, 1, 1, 1);
    } else if (strcmp(key, LUA_G_NUMBER3_FIELD_RIGHT) == 0) {
        lua_number3_pushNewObject(L, 1, 0, 0);
    } else if (strcmp(key, LUA_G_NUMBER3_FIELD_LEFT) == 0) {
        lua_number3_pushNewObject(L, -1, 0, 0);
    } else if (strcmp(key, LUA_G_NUMBER3_FIELD_UP) == 0) {
        lua_number3_pushNewObject(L, 0, 1, 0);
    } else if (strcmp(key, LUA_G_NUMBER3_FIELD_DOWN) == 0) {
        lua_number3_pushNewObject(L, 0, -1, 0);
    } else if (strcmp(key, LUA_G_NUMBER3_FIELD_FORWARD) == 0) {
        lua_number3_pushNewObject(L, 0, 0, 1);
    } else if (strcmp(key, LUA_G_NUMBER3_FIELD_BACKWARD) == 0) {
        lua_number3_pushNewObject(L, 0, 0, -1);
    } else {
        lua_pushnil(L);
    }

    LUAUTILS_STACK_SIZE_ASSERT(L, 1);
    return 1;
}

static int _g_number3_metatable_newindex(lua_State *L) {
    LUAUTILS_ERROR(L, "Number3 class is read-only");
    return 0;
}

/// Number3(x, y, z)
/// Arguments:
/// 1: x (float)
/// 2: y (float)
/// 3: z (float)
static int _g_number3_metatable_call(lua_State *L) {
    vx_assert(L != nullptr);
    
    LUAUTILS_STACK_SIZE(L)
    
    int nbParameters = lua_gettop(L);
    if (nbParameters != 1 && nbParameters != 4) { // global Number3 + x + y + z
        LUAUTILS_ERROR(L, "Number3: incorrect argument count"); // returns
    }
    
    if (lua_utils_getObjectType(L, 1) != ITEM_TYPE_G_NUMBER3) {
        LUAUTILS_ERROR(L, "incorrect argument type"); // returns
    }
    
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    if (nbParameters == 4) {
        if (lua_isnumber(L, 2)) {
            x = static_cast<float>(lua_tonumber(L, 2));
        } else {
            LUAUTILS_ERROR(L, "Number3: argument 1 should be a number"); // returns
        }

        if (lua_isnumber(L, 3)) {
            y = static_cast<float>(lua_tonumber(L, 3));
        } else {
            LUAUTILS_ERROR(L, "Number3: argument 2 should be a number"); // returns
        }

        if (lua_isnumber(L, 4)) {
            z = static_cast<float>(lua_tonumber(L, 4));
        } else {
            LUAUTILS_ERROR(L, "Number3: argument 3 should be a number"); // returns
        }
    }

    lua_number3_pushNewObject(L, x, y, z);
    
    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
    return 1;
}

/// __index for Number3 instances
static int _number3_metatable_index(lua_State *L) {
    vx_assert(L != nullptr);
    LUAUTILS_ASSERT_ARGS_COUNT(L, 2)
    LUAUTILS_ASSERT_ARG_TYPE(L, 1, ITEM_TYPE_NUMBER3)
    LUAUTILS_STACK_SIZE(L)
    
    const char *key = lua_tostring(L, 2);
    
    if (strcmp(key, LUA_NUMBER3_FIELD_X) == 0 || strcmp(key, LUA_NUMBER3_FIELD_WIDTH) == 0) {
        
        float x;
        if (lua_number3_getXYZ(L, 1, &x, nullptr, nullptr)) {
            lua_pushnumber(L, static_cast<double>(x));
        } else {
            lua_pushnil(L);
        }
        
    } else if (strcmp(key, LUA_NUMBER3_FIELD_Y) == 0 || strcmp(key, LUA_NUMBER3_FIELD_HEIGHT) == 0) {
        
        float y;
        if (lua_number3_getXYZ(L, 1, nullptr, &y, nullptr)) {
            lua_pushnumber(L, static_cast<double>(y));
        } else {
            lua_pushnil(L);
        }
        
    } else if (strcmp(key, LUA_NUMBER3_FIELD_Z) == 0 || strcmp(key, LUA_NUMBER3_FIELD_DEPTH) == 0) {
        
        float z;
        if (lua_number3_getXYZ(L, 1, nullptr, nullptr, &z)) {
            lua_pushnumber(L, static_cast<double>(z));
        } else {
            lua_pushnil(L);
        }
    } else if (strcmp(key, LUA_NUMBER3_FIELD_ROTATE) == 0) {
        
        lua_pushcfunction(L, _number3_rotate, "");
        
    } else if (strcmp(key, LUA_NUMBER3_FIELD_LENGTH) == 0) {
        
        float3 f3;
        if (lua_number3_getXYZ(L, 1, &(f3.x), &(f3.y), &(f3.z))) {
            lua_pushnumber(L, static_cast<double>(float3_length(&f3)));
        } else {
            lua_pushnil(L);
        }
        
    } else if (strcmp(key, LUA_NUMBER3_FIELD_SQUARED_LENGTH) == 0) {
        
        float3 f3;
        if (lua_number3_getXYZ(L, 1, &(f3.x), &(f3.y), &(f3.z))) {
            lua_pushnumber(L, static_cast<double>(float3_sqr_length(&f3)));
        } else {
            lua_pushnil(L);
        }
        
    } else if (strcmp(key, LUA_NUMBER3_FIELD_NORMALIZE) == 0) {
        
        lua_pushcfunction(L, _number3_normalize, "");
        
    } else if (strcmp(key, LUA_NUMBER3_FIELD_DOT) == 0) {
        
        lua_pushcfunction(L, _number3_dot, "");
    
    } else if (strcmp(key, LUA_NUMBER3_FIELD_CROSS) == 0) {
        
        lua_pushcfunction(L, _number3_cross, "");
        
    } else if (strcmp(key, LUA_NUMBER3_FIELD_COPY) == 0) {
        
        lua_pushcfunction(L, _number3_copy, "");

    } else if (strcmp(key, LUA_NUMBER3_FIELD_LERP) == 0) {
        lua_pushcfunction(L, _number3_lerp, "");
    } else if (strcmp(key, LUA_NUMBER3_FIELD_SET) == 0) {
        lua_pushcfunction(L, _number3_set, "");
    } else if (strcmp(key, LUA_NUMBER3_FIELD_ANGLE) == 0) {
        lua_pushcfunction(L, _number3_angle, "");
    } else if (strcmp(key, LUA_NUMBER3_FIELD_ADD_ON_SET_CALLBACK) == 0) {
        lua_pushcfunction(L, _number3_addOnSetCallback, "");
    } else if (strcmp(key, LUA_NUMBER3_FIELD_REMOVE_ON_SET_CALLBACK) == 0) {
        lua_pushcfunction(L, _number3_removeOnSetCallback, "");
    } else if (strcmp(key, LUA_NUMBER3_FIELD_ON_SET_CALLBACKS) == 0) {
        number3_userdata* ud = static_cast<number3_userdata*>(lua_touserdata(L, 1));
        if (ud->hasOnSetCallbacks == false) {
            lua_pushnil(L);
        } else {
            lua_getfield(L, LUA_REGISTRYINDEX, LUA_NUMBER3_REGISTRY_ON_SET_CALLBACKS);
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

static void _number3_metatable_gc(void *_ud) {
    number3_userdata *ud = static_cast<number3_userdata*>(_ud);
    // remove "on set" callbacks
    if (ud->hasOnSetCallbacks) {
        // TODO: find a way to cleanup those OnSet callbacks
        //lua_getfield(L, LUA_REGISTRYINDEX, LUA_NUMBER3_REGISTRY_ON_SET_CALLBACKS);
        //lua_pushlightuserdata(L, ud);
        //lua_pushnil(L);
        //lua_rawset(L, -3);
        //lua_pop(L, 1);
    }
    number3_userdata_flush(ud);
}

/// __new_index for Number3 instances
static int _number3_metatable_new_index(lua_State *L) {
    vx_assert(L != nullptr);
    LUAUTILS_ASSERT_ARGS_COUNT(L, 3)
    LUAUTILS_ASSERT_ARG_TYPE(L, 1, ITEM_TYPE_NUMBER3)
    LUAUTILS_STACK_SIZE(L)
    
    const char *key = lua_tostring(L, 2);
    
    if (strcmp(key, LUA_NUMBER3_FIELD_X) == 0 || strcmp(key, LUA_NUMBER3_FIELD_WIDTH) == 0) {
        if (lua_isnumber(L, 3) == false) {
            LUAUTILS_ERROR(L, "Number3: X can only be a number");
        }

        float x = static_cast<float>(lua_tonumber(L, 3));
        lua_number3_setXYZ(L, 1, &x, nullptr, nullptr);
        
    } else if (strcmp(key, LUA_NUMBER3_FIELD_Y) == 0 || strcmp(key, LUA_NUMBER3_FIELD_HEIGHT) == 0) {
        if (lua_isnumber(L, 3) == false) {
            LUAUTILS_ERROR(L, "Number3: Y can only be a number");
        }

        float y = static_cast<float>(lua_tonumber(L, 3));
        lua_number3_setXYZ(L, 1, nullptr, &y, nullptr);
        
    } else if (strcmp(key, LUA_NUMBER3_FIELD_Z) == 0 || strcmp(key, LUA_NUMBER3_FIELD_DEPTH) == 0) {
        if (lua_isnumber(L, 3) == false) {
            LUAUTILS_ERROR(L, "Number3: Z can only be a number");
        }

        float z = static_cast<float>(lua_tonumber(L, 3));
        lua_number3_setXYZ(L, 1, nullptr, nullptr, &z);
        
    } else if (strcmp(key, LUA_NUMBER3_FIELD_LENGTH) == 0) {
        if (lua_isnumber(L, 3) == false) {
            LUAUTILS_ERROR(L, "Number3: Length can only be a number");
        }

        float3 f3;
        if (lua_number3_getXYZ(L, 1, &(f3.x), &(f3.y), &(f3.z)) == false) {
            LUAUTILS_ERROR(L, "can't set Number3.Length"); // returns
        }
        
        float len = static_cast<float>(lua_tonumber(L, 3));
        
        float3_normalize(&f3);
        float3_op_scale(&f3, len);
        
        lua_number3_setXYZ(L, 1, &(f3.x), &(f3.y), &(f3.z));
        
    } else if (strcmp(key, LUA_NUMBER3_FIELD_SQUARED_LENGTH) == 0) {
        if (lua_isnumber(L, 3) == false) {
            LUAUTILS_ERROR(L, "Number3: SquaredLength can only be a number");
        }
        
        float3 f3;
        if (lua_number3_getXYZ(L, 1, &(f3.x), &(f3.y), &(f3.z)) == false) {
            LUAUTILS_ERROR(L, "can't set Number3.Length"); // returns
        }
        
        float len = static_cast<float>(lua_tonumber(L, 3));
        
        float3_normalize(&f3);
        float3_op_scale(&f3, sqrtf(len));
        
        lua_number3_setXYZ(L, 1, &(f3.x), &(f3.y), &(f3.z));
        
    } else {
        LUAUTILS_ERROR_F(L, "Number3.%s is not settable", key); // returns
    }
    
    LUAUTILS_STACK_SIZE_ASSERT(L, 0)
    return 0;
}

static int _number3_metatable_add(lua_State *L) {
    vx_assert(L != nullptr);
    LUAUTILS_STACK_SIZE(L)

    float x1, y1, z1, x2, y2, z2;
    if (lua_isnumber(L, 1)) {
        x1 = y1 = z1 = lua_tonumber(L, 1);
    } else if (lua_number3_or_table_getXYZ(L, 1, &x1, &y1, &z1) == false) {
        LUAUTILS_ERROR(L, "wrong arithmetic operation")
        return 0; // silence warnings
    }
    if (lua_isnumber(L, 2)) {
        x2 = y2 = z2 = lua_tonumber(L, 2);
    } else if (lua_number3_or_table_getXYZ(L, 2, &x2, &y2, &z2) == false) {
        LUAUTILS_ERROR(L, "wrong arithmetic operation")
        return 0; // silence warnings
    }
    lua_number3_pushNewObject(L, x1 + x2, y1 + y2, z1 + z2);
    
    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
    return 1;
}

static int _number3_metatable_sub(lua_State *L) {
    vx_assert(L != nullptr);
    LUAUTILS_STACK_SIZE(L)

    float x1, y1, z1, x2, y2, z2;
    if (lua_isnumber(L, 1)) {
        x1 = y1 = z1 = lua_tonumber(L, 1);
    } else if (lua_number3_or_table_getXYZ(L, 1, &x1, &y1, &z1) == false) {
        LUAUTILS_ERROR(L, "wrong arithmetic operation")
        return 0; // silence warnings
    }
    if (lua_isnumber(L, 2)) {
        x2 = y2 = z2 = lua_tonumber(L, 2);
    } else if (lua_number3_or_table_getXYZ(L, 2, &x2, &y2, &z2) == false) {
        LUAUTILS_ERROR(L, "wrong arithmetic operation")
        return 0; // silence warnings
    }
    lua_number3_pushNewObject(L, x1 - x2, y1 - y2, z1 - z2);
    
    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
    return 1;
}

static int _number3_metatable_unm(lua_State *L) {
    vx_assert(L != nullptr);
    LUAUTILS_ASSERT_ARG_TYPE(L, 1, ITEM_TYPE_NUMBER3)
    LUAUTILS_STACK_SIZE(L)
    
    float3 f3;
    
    if (lua_number3_getXYZ(L, 1, &f3.x, &f3.y, &f3.z) == false) {
        LUAUTILS_ERROR(L, "wrong arithmetic operation"); // returns
    }
    
    lua_number3_pushNewObject(L, -f3.x, -f3.y, -f3.z);
    
    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
    return 1;
}

static int _number3_metatable_mul(lua_State *L) {
    vx_assert(L != nullptr);
    LUAUTILS_STACK_SIZE(L)

    float x1, y1, z1, x2, y2, z2;
    if (lua_isnumber(L, 1)) {
        x1 = y1 = z1 = lua_tonumber(L, 1);
    } else if (lua_number3_or_table_getXYZ(L, 1, &x1, &y1, &z1) == false) {
        LUAUTILS_ERROR(L, "wrong arithmetic operation")
        return 0; // silence warnings
    }
    if (lua_isnumber(L, 2)) {
        x2 = y2 = z2 = lua_tonumber(L, 2);
    } else if (lua_number3_or_table_getXYZ(L, 2, &x2, &y2, &z2) == false) {
        LUAUTILS_ERROR(L, "wrong arithmetic operation")
        return 0; // silence warnings
    }
    lua_number3_pushNewObject(L, x1 * x2, y1 * y2, z1 * z2);

    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
    return 1;
}

static int _number3_metatable_div(lua_State *L) {
    vx_assert(L != nullptr);
    LUAUTILS_STACK_SIZE(L)

    float x1, y1, z1, x2, y2, z2;
    if (lua_isnumber(L, 1)) {
        x1 = y1 = z1 = lua_tonumber(L, 1);
    } else if (lua_number3_or_table_getXYZ(L, 1, &x1, &y1, &z1) == false) {
        LUAUTILS_ERROR(L, "wrong arithmetic operation")
        return 0; // silence warnings
    }
    if (lua_isnumber(L, 2)) {
        x2 = y2 = z2 = lua_tonumber(L, 2);
    } else if (lua_number3_or_table_getXYZ(L, 2, &x2, &y2, &z2) == false) {
        LUAUTILS_ERROR(L, "wrong arithmetic operation")
        return 0; // silence warnings
    }
    lua_number3_pushNewObject(L, x1 / x2, y1 / y2, z1 / z2);

    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
    return 1;
}

static int _number3_metatable_idiv(lua_State *L) {
    vx_assert(L != nullptr);
    LUAUTILS_STACK_SIZE(L)

    float x1, y1, z1, x2, y2, z2;
    if (lua_isnumber(L, 1)) {
        x1 = y1 = z1 = lua_tonumber(L, 1);
    } else if (lua_number3_or_table_getXYZ(L, 1, &x1, &y1, &z1) == false) {
        LUAUTILS_ERROR(L, "wrong arithmetic operation")
        return 0; // silence warnings
    }
    if (lua_isnumber(L, 2)) {
        x2 = y2 = z2 = lua_tonumber(L, 2);
    } else if (lua_number3_or_table_getXYZ(L, 2, &x2, &y2, &z2) == false) {
        LUAUTILS_ERROR(L, "wrong arithmetic operation")
        return 0; // silence warnings
    }
    lua_number3_pushNewObject(L, floorf(x1 / x2), floorf(y1 / y2), floorf(z1 / z2));

    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
    return 1;
}

static int _number3_metatable_pow(lua_State *L) {
    vx_assert(L != nullptr);
    LUAUTILS_STACK_SIZE(L)
    
    // if second parameter is a number
    if (lua_isnumber(L, 2)) {
        
        float3 f3;
        
        if (lua_number3_getXYZ(L, 1, &f3.x, &f3.y, &f3.z) == false) {
            LUAUTILS_ERROR(L, "wrong arithmetic operation"); // returns
        }
        
        float pow = lua_tonumberx(L, 2, nullptr);
        
        
        lua_number3_pushNewObject(L,
                                  powf(f3.x, pow),
                                  powf(f3.y, pow),
                                  powf(f3.z, pow));
        
        LUAUTILS_STACK_SIZE_ASSERT(L, 1)
        return 1;
    }
    
    LUAUTILS_ERROR(L, "wrong arithmetic operation"); // returns
    return 0;
}

static int _number3_rotate(lua_State *L) {
    vx_assert(L != nullptr);
    LUAUTILS_STACK_SIZE(L)

    const int argCount = lua_gettop(L);
    
    if (argCount < 1 || lua_utils_getObjectType(L, 1) != ITEM_TYPE_NUMBER3) {
        LUAUTILS_ERROR(L, "Number3:Rotate - function should be called with `:`");
    }

    float3 self = float3_zero;
    if (lua_number3_getXYZ(L, 1, &self.x, &self.y, &self.z) == false) {
        LUAUTILS_INTERNAL_ERROR(L); // returns
    }

    Quaternion q;
    float3 f3 = float3_zero;
    bool rotType = false;
    if (argCount == 4) { // parameter as 3 numbers
        if (lua_isnumber(L, 2) == false) {
            LUAUTILS_ERROR(L, "Number3:Rotate - argument 1 should be a number");
        }
        f3.x = lua_tonumber(L, 2);
        if (lua_isnumber(L, 3) == false) {
            LUAUTILS_ERROR(L, "Number3:Rotate - argument 2 should be a number");
        }
        f3.y = lua_tonumber(L, 3);
        if (lua_isnumber(L, 4) == false) {
            LUAUTILS_ERROR(L, "Number3:Rotate - argument 3 should be a number");
        }
        f3.z = lua_tonumber(L, 4);
    } else if (argCount == 2) { // parameter as Number3, Rotation, or table of numbers
        if (lua_rotation_get(L, 2, &q)) {
            rotType = true;
        } else if (lua_number3_or_table_getXYZ(L, 2, &f3.x, &f3.y, &f3.z) == false) {
            LUAUTILS_ERROR(L, "Number3:Rotate - argument 1 should be a Number3 or table of numbers");
        }
    } else {
        LUAUTILS_ERROR(L, "Number3:Rotate - wrong number of arguments");
    }

    if (rotType) {
        quaternion_rotate_vector(&q, &self);
    } else {
        float3_rotate(&self, f3.x, f3.y, f3.z);
    }
    lua_number3_setXYZ(L, 1, &self.x, &self.y, &self.z);

    LUAUTILS_STACK_SIZE_ASSERT(L, 0)
    return 0;
}

static int _number3_normalize(lua_State *L) {
    vx_assert(L != nullptr);
    LUAUTILS_STACK_SIZE(L)
    
    const int argCount = lua_gettop(L);
    
    if (argCount < 1 || lua_utils_getObjectType(L, 1) != ITEM_TYPE_NUMBER3) {
        LUAUTILS_ERROR(L, "Number3:Normalize - function should be called with `:`");
    }

    if (argCount != 1) {
        LUAUTILS_ERROR(L, "Number3:Normalize - wrong number of arguments"); // returns
    }

    float3 f3;
    if (lua_number3_getXYZ(L, 1, &(f3.x), &(f3.y), &(f3.z)) == false) {
        LUAUTILS_INTERNAL_ERROR(L); // returns
    }
    
    float3_normalize(&f3);
    lua_number3_setXYZ(L, 1, &(f3.x), &(f3.y), &(f3.z));

    lua_pushvalue(L, -1);

    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
    return 1;
}

static int _number3_dot(lua_State *L) {
    vx_assert(L != nullptr);
    LUAUTILS_STACK_SIZE(L)
    
    const int argCount = lua_gettop(L);
    
    if (argCount < 1 || lua_utils_getObjectType(L, 1) != ITEM_TYPE_NUMBER3) {
        LUAUTILS_ERROR(L, "Number3:Dot - function should be called with `:`");
    }
    
    if (argCount != 2) {
        LUAUTILS_ERROR(L, "Number3:Dot - wrong number of arguments"); // returns
    }

    float3 self = float3_zero;
    if (lua_number3_getXYZ(L, 1, &self.x, &self.y, &self.z) == false) {
        LUAUTILS_INTERNAL_ERROR(L); // returns
    }

    float3 f3 = float3_zero;
    if (lua_number3_or_table_getXYZ(L, 2, &f3.x, &f3.y, &f3.z) == false) {
        LUAUTILS_ERROR(L, "Number3:Dot - argument should be a Number3"); // returns
    }

    lua_pushnumber(L, static_cast<double>(float3_dot_product(&self, &f3)));

    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
    return 1;
}

static int _number3_cross(lua_State *L) {
    vx_assert(L != nullptr);
    LUAUTILS_STACK_SIZE(L)
    
    const int argCount = lua_gettop(L);
    
    if (argCount < 1 || lua_utils_getObjectType(L, 1) != ITEM_TYPE_NUMBER3) {
        LUAUTILS_ERROR(L, "Number3:Cross - function should be called with `:`");
    }
    
    if (argCount != 2) {
        LUAUTILS_ERROR(L, "Number3:Cross - wrong number of arguments"); // returns
    }

    float3 self = float3_zero;
    if (lua_number3_getXYZ(L, 1, &self.x, &self.y, &self.z) == false) {
        LUAUTILS_INTERNAL_ERROR(L); // returns
    }

    float3 f3 = float3_zero;
    if (lua_number3_or_table_getXYZ(L, 2, &f3.x, &f3.y, &f3.z) == false) {
        LUAUTILS_ERROR(L, "Number3:Cross - second argument should be a Number3"); // returns
    }
    
    float3_cross_product(&self, &f3);
    lua_number3_pushNewObject(L, self.x, self.y, self.z);

    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
    return 1;
}

static int _number3_copy(lua_State *L) {
    vx_assert(L != nullptr);
    LUAUTILS_STACK_SIZE(L)
    
    const int argCount = lua_gettop(L);
    
    if (argCount < 1 || lua_utils_getObjectType(L, 1) != ITEM_TYPE_NUMBER3) {
        LUAUTILS_ERROR(L, "Number3:Copy - function should be called with `:`");
    }
    
    if (argCount != 1) {
        LUAUTILS_ERROR(L, "Number3:Copy - wrong number of arguments"); // returns
    }

    float3 self = float3_zero;
    if (lua_number3_getXYZ(L, 1, &self.x, &self.y, &self.z) == false) {
        LUAUTILS_INTERNAL_ERROR(L); // returns
    }

    lua_number3_pushNewObject(L, self.x, self.y, self.z);

    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
    return 1;
}

static int _number3_lerp(lua_State *L) {
    vx_assert(L != nullptr);
    LUAUTILS_STACK_SIZE(L)

    const int argCount = lua_gettop(L);

    if (argCount < 1 || lua_utils_getObjectType(L, 1) != ITEM_TYPE_NUMBER3) {
        LUAUTILS_ERROR(L, "Number3:Lerp - function should be called with `:`");
    }

    if (argCount != 4) {
        LUAUTILS_ERROR(L, "Number3:Lerp - wrong number of arguments, expected 3"); // returns
    }

    float x1 = 0, y1 = 0, z1 = 0;
    if (lua_number3_or_table_getXYZ(L, 2, &x1, &y1, &z1) == false) {
        LUAUTILS_ERROR(L, "Number3:Lerp - first argument should be a Number3 or table of numbers");
    }

    float x2 = 0, y2 = 0, z2 = 0;
    if (lua_number3_or_table_getXYZ(L, 3, &x2, &y2, &z2) == false) {
        LUAUTILS_ERROR(L, "Number3:Lerp - second argument should be a Number3 or table of numbers");
    }

    if (lua_isnumber(L, 4) == false) {
        LUAUTILS_ERROR(L, "Number3:Lerp - third argument should be a number");
    }

    const float v = static_cast<float>(lua_tonumber(L, 4));

    x1 = x1 + (x2 - x1) * v;
    y1 = y1 + (y2 - y1) * v;
    z1 = z1 + (z2 - z1) * v;

    lua_number3_setXYZ(L, 1, &x1, &y1, &z1);

    LUAUTILS_STACK_SIZE_ASSERT(L, 0)
    return 0;
}

static int _number3_set(lua_State *L) {
    vx_assert(L != nullptr);
    LUAUTILS_STACK_SIZE(L)

    const int argCount = lua_gettop(L);

    if (argCount < 1 || lua_utils_getObjectType(L, 1) != ITEM_TYPE_NUMBER3) {
        LUAUTILS_ERROR(L, "Number3:Set - function should be called with `:`");
    }

    if (argCount == 4) {
        if (lua_isnumber(L, 2) == false) {
            LUAUTILS_ERROR(L, "Number3:Set - argument 1 should be a number");
        }
        float x = lua_tonumber(L, 2);
        if (lua_isnumber(L, 3) == false) {
            LUAUTILS_ERROR(L, "Number3:Set - argument 2 should be a number");
        }
        float y = lua_tonumber(L, 3);
        if (lua_isnumber(L, 4) == false) {
            LUAUTILS_ERROR(L, "Number3:Set - argument 3 should be a number");
        }
        float z = lua_tonumber(L, 4);

        lua_number3_setXYZ(L, 1, &x, &y, &z);
    } else if (argCount == 2) {
        float3 euler;
        if (lua_number3_or_table_getXYZ(L, 2, &euler.x, &euler.y, &euler.z) == false) {
            LUAUTILS_ERROR(L, "Number3:Set - argument should be a Number3 or table of numbers");
        }

        lua_number3_setXYZ(L, 1, &euler.x, &euler.y, &euler.z);
    } else {
        LUAUTILS_ERROR(L, "Number3:Set - wrong number of arguments");
    }

    lua_pushvalue(L, 1);

    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
    return 1;
}

static int _number3_angle(lua_State *L) {
    vx_assert(L != nullptr);
    LUAUTILS_STACK_SIZE(L)

    const int argCount = lua_gettop(L);

    if (argCount < 1 || lua_utils_getObjectType(L, 1) != ITEM_TYPE_NUMBER3) {
        LUAUTILS_ERROR(L, "Number3:Angle - function should be called with `:`");
    }

    float3 self;
    if (lua_number3_getXYZ(L, 1, &self.x, &self.y, &self.z) == false) {
        LUAUTILS_INTERNAL_ERROR(L);
    }

    float3 other;
    if (lua_number3_or_table_or_numbers_getXYZ(L, 2, &other.x, &other.y, &other.z) == false) {
        LUAUTILS_ERROR(L, "Number3:Angle - argument should be either a Number3, a table of 3 numbers, or 3 individual numbers");
    }

    const lua_Number n = static_cast<lua_Number>(float3_angle_between(&self, &other));
    lua_pushnumber(L, n);

    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
    return 1;
}

static int _number3_eq(lua_State *L) {
    vx_assert(L != nullptr);
    LUAUTILS_STACK_SIZE(L)
    
    float3 f1;
    float3 f2;

    // if one member is not Number3-compatible then the 2 parts are not equal
    if (lua_number3_or_table_getXYZ(L, 1, &(f1.x), &(f1.y), &(f1.z)) == false ||
        lua_number3_or_table_getXYZ(L, 2, &(f2.x), &(f2.y), &(f2.z)) == false) {
        lua_pushboolean(L, false);
        LUAUTILS_STACK_SIZE_ASSERT(L, 1);
        return 1;
    }
    
    // compare the 2 Number3
    lua_pushboolean(L, float3_isEqual(&f1, &f2, EPSILON_ZERO));
    
    LUAUTILS_STACK_SIZE_ASSERT(L, 1);
    return 1;
}

static int _number3_tostring(lua_State *L) {
    float3 num3 = float3_zero;

    vx_assert(L != nullptr);
    LUAUTILS_ASSERT_ARGS_COUNT(L, 1)                  // only 1 arg provided ?
    LUAUTILS_ASSERT_ARG_TYPE(L, 1, ITEM_TYPE_NUMBER3) // is Number3 ?
    LUAUTILS_STACK_SIZE(L)                            // size verif

    if (lua_number3_getXYZ(L, 1, &num3.x, &num3.y, &num3.z) == false) {
        LUAUTILS_INTERNAL_ERROR(L); // returns
    }

    const int buff_size = 1 + snprintf(nullptr,
                                       0, "[Number3 X: %.2f Y: %.2f Z: %.2f]",
                                       static_cast<double>(num3.x),
                                       static_cast<double>(num3.y),
                                       static_cast<double>(num3.z)); // to know the size of the string
    std::string str(buff_size, '\0');
    int return_value = snprintf(&str[0], buff_size, "[Number3 X: %.2f Y: %.2f Z: %.2f]",
                            static_cast<double>(num3.x),
                            static_cast<double>(num3.y),
                            static_cast<double>(num3.z)); // convert number3 into a string
    if (return_value < 0 || return_value > buff_size) { //if snprintf() fail
        LUAUTILS_ERROR(L, "Number3:Failed to convert numbers into string"); // returns
    }

    lua_pushlstring(L, str.c_str(), buff_size); // stacks the string on the stack
    LUAUTILS_STACK_SIZE_ASSERT(L, 1);
    return 1;
}

static int _number3_addOnSetCallback(lua_State *L) {
    LUAUTILS_STACK_SIZE(L)
    
    const int argCount = lua_gettop(L);

    if (argCount < 1 || lua_utils_getObjectType(L, 1) != ITEM_TYPE_NUMBER3) {
        LUAUTILS_ERROR(L, "Number3:AddOnSetCallback - function should be called with `:`");
    }
    
    if (argCount < 2 || lua_type(L, 2) != LUA_TFUNCTION) {
        LUAUTILS_ERROR(L, "Number3:AddOnSetCallback - first argument should be a function");
    }

    number3_userdata* ud = static_cast<number3_userdata*>(lua_touserdata(L, 1));
    lua_getfield(L, LUA_REGISTRYINDEX, LUA_NUMBER3_REGISTRY_ON_SET_CALLBACKS);

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

static int _number3_removeOnSetCallback(lua_State *L) {
    LUAUTILS_STACK_SIZE(L)
    
    const int argCount = lua_gettop(L);
    
    if (argCount < 1 || lua_utils_getObjectType(L, 1) != ITEM_TYPE_NUMBER3) {
        LUAUTILS_ERROR(L, "Number3:RemoveOnSetCallback - function should be called with `:`");
    }
    
    if (argCount < 2 || lua_type(L, 2) != LUA_TFUNCTION) {
        LUAUTILS_ERROR(L, "Number3:RemoveOnSetCallback - first argument should be a function");
    }

    number3_userdata* ud = static_cast<number3_userdata*>(lua_touserdata(L, 1));
    if (ud->hasOnSetCallbacks == false) {
        return 0;
    }

    lua_getfield(L, LUA_REGISTRYINDEX, LUA_NUMBER3_REGISTRY_ON_SET_CALLBACKS);
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
