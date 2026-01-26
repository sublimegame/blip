// -------------------------------------------------------------
//  Cubzh
//  lua_number2.cpp
//  Created by Arthur Cormerais on October 25, 2022.
// -------------------------------------------------------------

#include "lua_number2.hpp"

// C
#include <math.h>
#include <string.h>

#include "lua.hpp"
#include "lua_utils.hpp"
#include "sbs.hpp"
#include "xptools.h"
#include "config.h"
#include "weakptr.h"
#include "utils.h"

#define LUA_G_NUMBER2_FIELD_ZERO "Zero"
#define LUA_G_NUMBER2_FIELD_ONE "One"
#define LUA_G_NUMBER2_FIELD_RIGHT "Right"
#define LUA_G_NUMBER2_FIELD_LEFT "Left"
#define LUA_G_NUMBER2_FIELD_UP "Up"
#define LUA_G_NUMBER2_FIELD_DOWN "Down"

#define LUA_NUMBER2_FIELD_X "X"
#define LUA_NUMBER2_FIELD_Y "Y"
#define LUA_NUMBER2_FIELD_WIDTH "Width"
#define LUA_NUMBER2_FIELD_HEIGHT "Height"

#define LUA_NUMBER3_FIELD_COPY "Copy" // function
#define LUA_NUMBER2_FIELD_NORMALIZE "Normalize" // function
#define LUA_NUMBER2_FIELD_LERP "Lerp" // function
#define LUA_NUMBER2_FIELD_SET "Set" // function

#define LUA_NUMBER2_FIELD_LENGTH "Length"
#define LUA_NUMBER3_FIELD_SQUARED_LENGTH "SquaredLength"

#define LUA_G_NUMBER2_INSTANCE_METATABLE "__imt"

#define LUA_NUMBER2_USERDATA_TYPE_DOUBLES 0
#define LUA_NUMBER2_USERDATA_TYPE_HANDLER 1

typedef struct number2_userdata {
    void *userdata; // double[2] by default, but can also be a number2_C_handler pointer
    uint8_t type; // LUA_NUMBER2_USERDATA_TYPE_DOUBLES = double[2], LUA_NUMBER2_USERDATA_TYPE_HANDLER = number3_C_handler
} number3_userdata;

typedef struct number2_C_handler {
    number2_C_handler_set set;
    number2_C_handler_get get;
    number2_C_handler_userdata userdata;
    bool isUserdataWeakptr;
} number2_C_handler;

static void number2_userdata_flush(number2_userdata *ud) {
    switch (ud->type) {
        case LUA_NUMBER2_USERDATA_TYPE_DOUBLES:
        {
            free(ud->userdata);
            break;
        }
        case LUA_NUMBER2_USERDATA_TYPE_HANDLER:
        {
            number2_C_handler *handler = static_cast<number2_C_handler*>(ud->userdata);
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

// MARK: - Private functions prototypes -

static int _g_number2_metatable_index(lua_State *L);
static int _g_number2_metatable_newindex(lua_State *L);
static int _g_number2_metatable_call(lua_State *L);
static int _number2_metatable_index(lua_State *L);
static int _number2_metatable_new_index(lua_State *L);
static void _number2_metatable_gc(void *ud);
static int _number2_metatable_add(lua_State *L);
static int _number2_metatable_sub(lua_State *L);
static int _number2_metatable_unm(lua_State *L);
static int _number2_metatable_mul(lua_State *L);
static int _number2_metatable_div(lua_State *L);
static int _number2_metatable_idiv(lua_State *L);
static int _number2_metatable_pow(lua_State *L);
static int _number2_copy(lua_State *L);
static int _number2_eq(lua_State *L);
static int _number2_normalize(lua_State *L);
static int _number2_lerp(lua_State *L);
static int _number2_set(lua_State *L);

// MARK: - Exposed functions -

void lua_g_number2_createAndPush(lua_State *L) {
    vx_assert(L != nullptr);
    LUAUTILS_STACK_SIZE(L)
    
    lua_newuserdata(L, 1);
    lua_newtable(L); // metatable
    {
        lua_pushstring(L, "__index");
        lua_pushcfunction(L, _g_number2_metatable_index, "");
        lua_rawset(L, -3);
        
        lua_pushstring(L, "__newindex");
        lua_pushcfunction(L, _g_number2_metatable_newindex, "");
        lua_rawset(L, -3);
        
        lua_pushstring(L, "__call");
        lua_pushcfunction(L, _g_number2_metatable_call, "");
        lua_rawset(L, -3);

        // hide the metatable from the Lua sandbox user
        lua_pushstring(L, "__metatable");
        lua_pushboolean(L, false);
        lua_rawset(L, -3);

        lua_pushstring(L, "__type");
        lua_pushstring(L, "Number2Interface");
        lua_rawset(L, -3);

        lua_pushstring(L, "__typen"); // used to log tables
        lua_pushinteger(L, ITEM_TYPE_G_NUMBER2);
        lua_rawset(L, -3);

        lua_pushstring(L, LUA_G_NUMBER2_INSTANCE_METATABLE);
        lua_newtable(L); // metatable
        {
            lua_pushstring(L, "__index");
            lua_pushcfunction(L, _number2_metatable_index, "");
            lua_rawset(L, -3);

            lua_pushstring(L, "__newindex");
            lua_pushcfunction(L, _number2_metatable_new_index, "");
            lua_rawset(L, -3);

            lua_pushstring(L, "__add");
            lua_pushcfunction(L, _number2_metatable_add, "");
            lua_rawset(L, -3);

            lua_pushstring(L, "__sub");
            lua_pushcfunction(L, _number2_metatable_sub, "");
            lua_rawset(L, -3);

            lua_pushstring(L, "__unm");
            lua_pushcfunction(L, _number2_metatable_unm, "");
            lua_rawset(L, -3);

            lua_pushstring(L, "__mul");
            lua_pushcfunction(L, _number2_metatable_mul, "");
            lua_rawset(L, -3);

            lua_pushstring(L, "__div");
            lua_pushcfunction(L, _number2_metatable_div, "");
            lua_rawset(L, -3);

            lua_pushstring(L, "__idiv");
            lua_pushcfunction(L, _number2_metatable_idiv, "");
            lua_rawset(L, -3);

            lua_pushstring(L, "__pow");
            lua_pushcfunction(L, _number2_metatable_pow, "");
            lua_rawset(L, -3);

            lua_pushstring(L, "__eq");
            lua_pushcfunction(L, _number2_eq, "");
            lua_rawset(L, -3);

            lua_pushstring(L, "__metatable");
            lua_pushboolean(L, false);
            lua_rawset(L, -3);

            lua_pushstring(L, "__type");
            lua_pushstring(L, "Number2");
            lua_rawset(L, -3);

            lua_pushstring(L, "__typen");
            lua_pushinteger(L, ITEM_TYPE_NUMBER2);
            lua_rawset(L, -3);
        }
        lua_rawset(L, -3);
    }
    lua_setmetatable(L, -2);
    
    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
}

bool lua_number2_pushNewObject(lua_State *L, const float x, const float y) {
    vx_assert(L != nullptr);
    LUAUTILS_STACK_SIZE(L)

    number2_userdata* ud = static_cast<number2_userdata*>(lua_newuserdatadtor(L, sizeof(number2_userdata), _number2_metatable_gc));
    ud->type = LUA_NUMBER2_USERDATA_TYPE_DOUBLES;
    double *n2 = static_cast<double*>(malloc(sizeof(double) * 2));
    n2[0] = static_cast<double>(x);
    n2[1] = static_cast<double>(y);
    ud->userdata = n2;

    lua_getglobal(L, LUA_G_NUMBER2);
    LUA_GET_METAFIELD(L, -1, LUA_G_NUMBER2_INSTANCE_METATABLE);
    lua_remove(L, -2);
    lua_setmetatable(L, -2);

    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
    return true;
}

bool lua_number2_or_table_getXY(lua_State *L, const int idx, float *x, float *y) {
    vx_assert(L != nullptr);
    LUAUTILS_STACK_SIZE(L)
    
    if (lua_utils_getObjectType(L, idx) == ITEM_TYPE_NUMBER2) {
        LUAUTILS_STACK_SIZE_ASSERT(L, 0)
        return lua_number2_getXY(L, idx, x, y);
    }
    
    // check value is a table
    if (lua_istable(L, idx) == false) {
        LUAUTILS_STACK_SIZE_ASSERT(L, 0)
        return false;
    }
    
    lua_Integer len = static_cast<lua_Integer>(lua_objlen(L, idx));
    
    if (len != 2) {
        LUAUTILS_STACK_SIZE_ASSERT(L, 0)
        return false;
    }
    
    lua_rawgeti(L, idx, 1);
    int xType = lua_type(L, -1);

    lua_rawgeti(L, idx < 0 ? idx - 1 : idx, 2);
    int yType = lua_type(L, -1);

    if (xType == LUA_TNUMBER && yType == LUA_TNUMBER) {
        // values are in reverse order in the Lua stack
        if (x != nullptr) {
            *x = lua_tonumber(L, -2);
        }
        if (y != nullptr) {
            *y = lua_tonumber(L, -1);
        }
        lua_pop(L, 2);
        
        LUAUTILS_STACK_SIZE_ASSERT(L, 0)
        return true;
    }
    
    lua_pop(L, 2);
    LUAUTILS_STACK_SIZE_ASSERT(L, 0)
    return false;
}

bool lua_number2_or_table_or_numbers_getXY(lua_State *L, const int idx, float *x, float *y) {
    vx_assert(L != nullptr);
    LUAUTILS_STACK_SIZE(L)

    if (lua_number2_or_table_getXY(L, idx, x, y)) {
        LUAUTILS_STACK_SIZE_ASSERT(L, 0)
        return true;
    }

    if (lua_isnumber(L, idx) && lua_isnumber(L, idx + 1)) {
        *x = lua_tonumber(L, idx);
        *y = lua_tonumber(L, idx + 1);

        LUAUTILS_STACK_SIZE_ASSERT(L, 0)
        return true;
    }

    LUAUTILS_STACK_SIZE_ASSERT(L, 0)
    return false;
}

bool lua_number2_getXY(lua_State *L, const int idx, float *x, float *y) {
    vx_assert(L != nullptr);
    LUAUTILS_STACK_SIZE(L)
    
    if (lua_utils_getObjectType(L, idx) != ITEM_TYPE_NUMBER2) {
        LUAUTILS_STACK_SIZE_ASSERT(L, 0)
        return false;
    }

    number2_userdata *ud = static_cast<number2_userdata*>(lua_touserdata(L, idx));
    switch (ud->type) {
        case LUA_NUMBER2_USERDATA_TYPE_DOUBLES:
        {
            double *n2 = static_cast<double*>(ud->userdata);
            if (x != nullptr) {
                *x = n2[0];
            }
            if (y != nullptr) {
                *y = n2[1];
            }
            break;
        }
        case LUA_NUMBER2_USERDATA_TYPE_HANDLER:
        {
            number2_C_handler *handler = static_cast<number2_C_handler*>(ud->userdata);
            vx_assert(handler->get != nullptr);
            handler->get(x, y, L, &handler->userdata);
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

bool lua_number2_setXY(lua_State *L, const int idx, const float *x, const float *y) {
    vx_assert(L != nullptr);
    LUAUTILS_STACK_SIZE(L)
    
    if (lua_utils_getObjectType(L, idx) != ITEM_TYPE_NUMBER2) {
        LUAUTILS_STACK_SIZE_ASSERT(L, 0)
        return false;
    }

    number2_userdata *ud = static_cast<number2_userdata*>(lua_touserdata(L, idx));

    switch (ud->type) {
        case LUA_NUMBER2_USERDATA_TYPE_DOUBLES:
        {
            double *n2 = static_cast<double*>(ud->userdata);
            if (x != nullptr) {
                n2[0] = static_cast<double>(*x);
            }
            if (y != nullptr) {
                n2[1] = static_cast<double>(*y);
            }
            break;
        }
        case LUA_NUMBER2_USERDATA_TYPE_HANDLER:
        {
            number2_C_handler *handler = static_cast<number2_C_handler*>(ud->userdata);
            vx_assert(handler->set != nullptr);
            handler->set(x, y, L, &handler->userdata);
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

bool lua_number2_setHandlers(lua_State *L,
                             const int idx,
                             number2_C_handler_set set,
                             number2_C_handler_get get,
                             number2_C_handler_userdata userdata,
                             bool isUserdataWeakptr) {
    vx_assert(L != nullptr);
    vx_assert(set != nullptr && get != nullptr); // assuming both set & get aren't null
    LUAUTILS_STACK_SIZE(L)
    
    if (lua_utils_getObjectType(L, idx) != ITEM_TYPE_NUMBER2) {
        LUAUTILS_INTERNAL_ERROR(L) // returns
    }

    number2_userdata *ud = static_cast<number2_userdata*>(lua_touserdata(L, idx));
    number2_userdata_flush(ud);

    number2_C_handler *handlers = static_cast<number2_C_handler*>(malloc(sizeof(number2_C_handler)));
    handlers->set = set;
    handlers->get = get;
    handlers->userdata = userdata;
    handlers->isUserdataWeakptr = isUserdataWeakptr;

    ud->type = LUA_NUMBER2_USERDATA_TYPE_HANDLER;
    ud->userdata = handlers;

    LUAUTILS_STACK_SIZE_ASSERT(L, 0)
    
    return true;
}

int lua_g_number2_snprintf(lua_State *L, char* result, size_t maxLen, bool spacePrefix) {
    vx_assert(L != nullptr);
    return snprintf(result, maxLen, "%s[Number2 (global)]", spacePrefix ? " " : "");
}

int lua_number2_snprintf(lua_State *L, char* result, size_t maxLen, bool spacePrefix) {
    vx_assert(L != nullptr);
    
    float x, y;
    lua_number2_getXY(L, -1, &x, &y);
    
    return snprintf(result,
                    maxLen,
                    "%s[Number2 X: %.2f Y: %.2f]",
                    spacePrefix ? " " : "",
                    static_cast<double>(x),
                    static_cast<double>(y));
}

// MARK: - Private functions -

static int _g_number2_metatable_index(lua_State *L) {
    vx_assert(L != nullptr);
    LUAUTILS_STACK_SIZE(L)

    const char *key = lua_tostring(L, 2);
    if (strcmp(key, LUA_G_NUMBER2_FIELD_ZERO) == 0) {
        lua_number2_pushNewObject(L, 0, 0);
    } else if (strcmp(key, LUA_G_NUMBER2_FIELD_ONE) == 0) {
        lua_number2_pushNewObject(L, 1, 1);
    } else if (strcmp(key, LUA_G_NUMBER2_FIELD_RIGHT) == 0) {
        lua_number2_pushNewObject(L, 1, 0);
    } else if (strcmp(key, LUA_G_NUMBER2_FIELD_LEFT) == 0) {
        lua_number2_pushNewObject(L, -1, 0);
    } else if (strcmp(key, LUA_G_NUMBER2_FIELD_UP) == 0) {
        lua_number2_pushNewObject(L, 0, 1);
    } else if (strcmp(key, LUA_G_NUMBER2_FIELD_DOWN) == 0) {
        lua_number2_pushNewObject(L, 0, -1);
    } else {
        lua_pushnil(L);
    }

    LUAUTILS_STACK_SIZE_ASSERT(L, 1);
    return 1;
}

static int _g_number2_metatable_newindex(lua_State *L) {
    LUAUTILS_ERROR(L, "Number2 class is read-only");
    return 0;
}

static int _g_number2_metatable_call(lua_State *L) {
    vx_assert(L != nullptr);
    LUAUTILS_STACK_SIZE(L)
    
    int nbParameters = lua_gettop(L);
    if (nbParameters != 1 && nbParameters != 3) { // global Number2 + x + y
        LUAUTILS_ERROR(L, "Number2: incorrect argument count"); // returns
    }
    
    if (lua_utils_getObjectType(L, 1) != ITEM_TYPE_G_NUMBER2) {
        LUAUTILS_ERROR(L, "Number2: incorrect argument type"); // returns
    }
    
    float x = 0.0f;
    float y = 0.0f;
    if (nbParameters == 3) {
        if (lua_isnumber(L, 2)) {
            x = static_cast<float>(lua_tonumber(L, 2));
        } else {
            LUAUTILS_ERROR(L, "Number2: argument 1 should be a number"); // returns
        }

        if (lua_isnumber(L, 3)) {
            y = static_cast<float>(lua_tonumber(L, 3));
        } else {
            LUAUTILS_ERROR(L, "Number2: argument 1 should be a number"); // returns
        }
    }

    lua_number2_pushNewObject(L, x, y);
    
    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
    return 1;
}

static int _number2_metatable_index(lua_State *L) {
    vx_assert(L != nullptr);
    LUAUTILS_ASSERT_ARGS_COUNT(L, 2)
    LUAUTILS_ASSERT_ARG_TYPE(L, 1, ITEM_TYPE_NUMBER2)
    LUAUTILS_STACK_SIZE(L)
    
    // get 2nd argument: key string
    if (lua_utils_isStringStrict(L, 2) == false) {
        // Number2 does not support non-string keys
        lua_pushnil(L);
        LUAUTILS_STACK_SIZE_ASSERT(L, 1)
        return 1;
    }
    
    const char *key = lua_tostring(L, 2);
    
    if (strcmp(key, LUA_NUMBER2_FIELD_X) == 0 || strcmp(key, LUA_NUMBER2_FIELD_WIDTH) == 0) {
        float x;
        if (lua_number2_getXY(L, 1, &x, nullptr)) {
            lua_pushnumber(L, static_cast<double>(x));
        } else {
            lua_pushnil(L);
        }
    } else if (strcmp(key, LUA_NUMBER2_FIELD_Y) == 0 || strcmp(key, LUA_NUMBER2_FIELD_HEIGHT) == 0) {
        float y;
        if (lua_number2_getXY(L, 1, nullptr, &y)) {
            lua_pushnumber(L, static_cast<double>(y));
        } else {
            lua_pushnil(L);
        }
    } else if (strcmp(key, LUA_NUMBER3_FIELD_COPY) == 0) {
        lua_pushcfunction(L, _number2_copy, "");
    } else if  (strcmp(key, LUA_NUMBER2_FIELD_NORMALIZE) == 0) {
        lua_pushcfunction(L, _number2_normalize, "");
    } else if (strcmp(key, LUA_NUMBER2_FIELD_LERP) == 0) {
        lua_pushcfunction(L, _number2_lerp, "");
    } else if (strcmp(key, LUA_NUMBER2_FIELD_SET) == 0) {
        lua_pushcfunction(L, _number2_set, "");
    } else if  (strcmp(key, LUA_NUMBER2_FIELD_LENGTH) == 0) {
        float x, y;
        if (lua_number2_getXY(L, 1, &x, &y)) {
            lua_pushnumber(L, static_cast<double>(sqrtf(x * x + y * y)));
        } else {
            lua_pushnil(L);
        }
    } else if  (strcmp(key, LUA_NUMBER3_FIELD_SQUARED_LENGTH) == 0) {
        float x, y;
        if (lua_number2_getXY(L, 1, &x, &y)) {
            lua_pushnumber(L, static_cast<double>(x * x + y * y));
        } else {
            lua_pushnil(L);
        }
    } else {
        lua_pushnil(L);
    }
    
    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
    return 1;
}

static void _number2_metatable_gc(void *_ud) {
    number2_userdata *ud = static_cast<number2_userdata*>(_ud);
    number2_userdata_flush(ud);
}

static int _number2_metatable_new_index(lua_State *L) {
    vx_assert(L != nullptr);
    LUAUTILS_ASSERT_ARGS_COUNT(L, 3)
    LUAUTILS_ASSERT_ARG_TYPE(L, 1, ITEM_TYPE_NUMBER2)
    LUAUTILS_STACK_SIZE(L)
    
    const char *key = lua_tostring(L, 2);
    
    if (strcmp(key, LUA_NUMBER2_FIELD_X) == 0 || strcmp(key, LUA_NUMBER2_FIELD_WIDTH) == 0) {
        if (lua_isnumber(L, 3) == false) {
            LUAUTILS_ERROR_F(L, "Number2: %s can only be a number", key);
        }
        float x = static_cast<float>(lua_tonumber(L, 3));
        lua_number2_setXY(L, 1, &x, nullptr);
    } else if (strcmp(key, LUA_NUMBER2_FIELD_Y) == 0 || strcmp(key, LUA_NUMBER2_FIELD_HEIGHT) == 0) {
        if (lua_isnumber(L, 3) == false) {
            LUAUTILS_ERROR_F(L, "Number2: %s can only be a number", key);
        }
        float y = static_cast<float>(lua_tonumber(L, 3));
        lua_number2_setXY(L, 1, nullptr, &y);
    } else if (strcmp(key, LUA_NUMBER2_FIELD_LENGTH) == 0) {
        if (lua_isnumber(L, 3) == false) {
            LUAUTILS_ERROR(L, "Number2: Length can only be a number");
        }

        float x, y;
        if (lua_number2_getXY(L, 1, &x, &y) == false) {
            LUAUTILS_ERROR(L, "can't set Number2.Length"); // returns
        }

        const float newMag = static_cast<float>(lua_tonumber(L, 3));

        const float mag = sqrtf(x *x + y * y);
        x = x / mag * newMag;
        y = y / mag * newMag;

        lua_number2_setXY(L, 1, &x, &y);

    } else if (strcmp(key, LUA_NUMBER3_FIELD_SQUARED_LENGTH) == 0) {
        if (lua_isnumber(L, 3) == false) {
            LUAUTILS_ERROR(L, "Number2: SquaredLength can only be a number");
        }

        float x, y;
        if (lua_number2_getXY(L, 1, &x, &y) == false) {
            LUAUTILS_ERROR(L, "can't set Number2.SquaredLength"); // returns
        }

        const float newMag = sqrtf(static_cast<float>(lua_tonumber(L, 3)));

        const float mag = sqrtf(x *x + y * y);
        x = x / mag * newMag;
        y = y / mag * newMag;

        lua_number2_setXY(L, 1, &x, &y);

    } else {
        LUAUTILS_ERROR_F(L, "Number2.%s is not settable", key);
    }
    
    LUAUTILS_STACK_SIZE_ASSERT(L, 0)
    return 0;
}

static int _number2_metatable_add(lua_State *L) {
    vx_assert(L != nullptr);
    LUAUTILS_STACK_SIZE(L)

    float x1, y1, x2, y2;
    if (lua_isnumber(L, 1)) {
        x1 = y1 = lua_tonumber(L, 1);
    } else if (lua_number2_getXY(L, 1, &x1, &y1) == false) {
        LUAUTILS_ERROR(L, "wrong arithmetic operation")
        return 0; // silence warnings
    }
    if (lua_isnumber(L, 2)) {
        x2 = y2 = lua_tonumber(L, 2);
    } else if (lua_number2_getXY(L, 2, &x2, &y2) == false) {
        LUAUTILS_ERROR(L, "wrong arithmetic operation")
        return 0; // silence warnings
    }
    lua_number2_pushNewObject(L, x1 + x2, y1 + y2);
    
    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
    return 1;
}

static int _number2_metatable_sub(lua_State *L) {
    vx_assert(L != nullptr);
    LUAUTILS_STACK_SIZE(L)

    float x1, y1, x2, y2;
    if (lua_isnumber(L, 1)) {
        x1 = y1 = lua_tonumber(L, 1);
    } else if (lua_number2_getXY(L, 1, &x1, &y1) == false) {
        LUAUTILS_ERROR(L, "wrong arithmetic operation")
        return 0; // silence warnings
    }
    if (lua_isnumber(L, 2)) {
        x2 = y2 = lua_tonumber(L, 2);
    } else if (lua_number2_getXY(L, 2, &x2, &y2) == false) {
        LUAUTILS_ERROR(L, "wrong arithmetic operation")
        return 0; // silence warnings
    }
    lua_number2_pushNewObject(L, x1 - x2, y1 - y2);
    
    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
    return 1;
}

static int _number2_metatable_unm(lua_State *L) {
    vx_assert(L != nullptr);
    LUAUTILS_ASSERT_ARG_TYPE(L, 1, ITEM_TYPE_NUMBER2)
    LUAUTILS_STACK_SIZE(L)
    
    float x, y;
    if (lua_number2_getXY(L, 1, &x, &y) == false) {
        LUAUTILS_ERROR(L, "wrong arithmetic operation");
        return 0; // return statement not to confuse code analyzer
    }
    lua_number2_pushNewObject(L, -x, -y);
    
    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
    return 1;
}

static int _number2_metatable_mul(lua_State *L) {
    vx_assert(L != nullptr);
    LUAUTILS_STACK_SIZE(L)

    float x1, y1, x2, y2;
    if (lua_isnumber(L, 1)) {
        x1 = y1 = lua_tonumber(L, 1);
    } else if (lua_number2_getXY(L, 1, &x1, &y1) == false) {
        LUAUTILS_ERROR(L, "wrong arithmetic operation")
        return 0; // silence warnings
    }
    if (lua_isnumber(L, 2)) {
        x2 = y2 = lua_tonumber(L, 2);
    } else if (lua_number2_getXY(L, 2, &x2, &y2) == false) {
        LUAUTILS_ERROR(L, "wrong arithmetic operation")
        return 0; // silence warnings
    }
    lua_number2_pushNewObject(L, x1 * x2, y1 * y2);

    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
    return 1;
}

static int _number2_metatable_div(lua_State *L) {
    vx_assert(L != nullptr);
    LUAUTILS_STACK_SIZE(L)

    float x1, y1, x2, y2;
    if (lua_isnumber(L, 1)) {
        x1 = y1 = lua_tonumber(L, 1);
    } else if (lua_number2_getXY(L, 1, &x1, &y1) == false) {
        LUAUTILS_ERROR(L, "wrong arithmetic operation")
        return 0; // silence warnings
    }
    if (lua_isnumber(L, 2)) {
        x2 = y2 = lua_tonumber(L, 2);
    } else if (lua_number2_getXY(L, 2, &x2, &y2) == false) {
        LUAUTILS_ERROR(L, "wrong arithmetic operation")
        return 0; // silence warnings
    }
    if (x2 == 0.0f || y2 == 0.0f) {
        LUAUTILS_ERROR(L, "Number2: wrong arithmetic operation (can't divide by zero)"); // returns
    }
    lua_number2_pushNewObject(L, x1 / x2, y1 / y2);

    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
    return 1;
}

static int _number2_metatable_idiv(lua_State *L) {
    vx_assert(L != nullptr);
    LUAUTILS_STACK_SIZE(L)

    float x1, y1, x2, y2;
    if (lua_isnumber(L, 1)) {
        x1 = y1 = lua_tonumber(L, 1);
    } else if (lua_number2_getXY(L, 1, &x1, &y1) == false) {
        LUAUTILS_ERROR(L, "wrong arithmetic operation")
        return 0; // silence warnings
    }
    if (lua_isnumber(L, 2)) {
        x2 = y2 = lua_tonumber(L, 2);
    } else if (lua_number2_getXY(L, 2, &x2, &y2) == false) {
        LUAUTILS_ERROR(L, "wrong arithmetic operation")
        return 0; // silence warnings
    }
    lua_number2_pushNewObject(L, floorf(x1 / x2), floorf(y1 / y2));

    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
    return 1;
}

static int _number2_metatable_pow(lua_State *L) {
    vx_assert(L != nullptr);
    LUAUTILS_STACK_SIZE(L)

    float x, y, pow;
    if (lua_number2_getXY(L, 1, &x, &y) == false) {
        LUAUTILS_ERROR(L, "wrong arithmetic operation");
        return 0; // return statement not to confuse code analyzer
    }
    if (lua_isnumber(L, 2)) {
        pow = lua_tonumberx(L, 2, nullptr);
    } else {
        LUAUTILS_ERROR(L, "wrong arithmetic operation");
        return 0; // return statement not to confuse code analyzer
    }
    lua_number2_pushNewObject(L, powf(x, pow), powf(y, pow));

    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
    return 1;
}

static int _number2_copy(lua_State *L) {
    vx_assert(L != nullptr);
    LUAUTILS_STACK_SIZE(L)
    
    const int argCount = lua_gettop(L);

    if (argCount < 1 || lua_utils_getObjectType(L, 1) != ITEM_TYPE_NUMBER2) {
        LUAUTILS_ERROR(L, "Number2:Copy - function should be called with `:`");
        return 0; // return statement not to confuse code analyzer
    }
    
    if (argCount != 1) {
        LUAUTILS_ERROR(L, "Number2:Copy - wrong number of arguments");
        return 0; // return statement not to confuse code analyzer
    }

    float x, y;
    if (lua_number2_getXY(L, 1, &x, &y) == false) {
        LUAUTILS_INTERNAL_ERROR(L);
        return 0; // return statement not to confuse code analyzer
    }

    lua_number2_pushNewObject(L, x, y);

    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
    return 1;
}

static int _number2_eq(lua_State *L) {
    vx_assert(L != nullptr);
    LUAUTILS_STACK_SIZE(L)
    
    float x1, y1, x2, y2;
    if (lua_number2_or_table_getXY(L, 1, &x1, &y1) == false ||
        lua_number2_or_table_getXY(L, 2, &x2, &y2) == false) {
        lua_pushboolean(L, false);
    } else {
        lua_pushboolean(L, float_isEqual(x1, x2, EPSILON_ZERO)
                        && float_isEqual(y1, y2, EPSILON_ZERO));
    }
    
    LUAUTILS_STACK_SIZE_ASSERT(L, 1);
    return 1;
}

static int _number2_normalize(lua_State *L) {
    vx_assert(L != nullptr);
    LUAUTILS_STACK_SIZE(L)
    
    const int argCount = lua_gettop(L);
    
    if (argCount < 1 || lua_utils_getObjectType(L, 1) != ITEM_TYPE_NUMBER2) {
        LUAUTILS_ERROR(L, "Number2:Normalize - function should be called with `:`");
    }

    if (argCount != 1) {
        LUAUTILS_ERROR(L, "Number2:Normalize - wrong number of arguments"); // returns
    }

    float x, y;
    if (lua_number2_getXY(L, 1, &x, &y) == false) {
        LUAUTILS_INTERNAL_ERROR(L);
        return 0; // return statement not to confuse code analyzer
    }
    
    float3 f3 = {x, y, 0};
    float3_normalize(&f3);
    
    lua_number2_setXY(L, 1, &f3.x, &f3.y);

    lua_pushvalue(L, -1);

    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
    return 1;
}

static int _number2_lerp(lua_State *L) {
    vx_assert(L != nullptr);
    LUAUTILS_STACK_SIZE(L)

    const int argCount = lua_gettop(L);

    if (argCount < 1 || lua_utils_getObjectType(L, 1) != ITEM_TYPE_NUMBER2) {
        LUAUTILS_ERROR(L, "Number2:Lerp - function should be called with `:`");
    }

    if (argCount != 4) {
        LUAUTILS_ERROR(L, "Number2:Lerp - wrong number of arguments, expected 3"); // returns
    }

    float x1 = 0, y1 = 0;
    if (lua_number2_or_table_getXY(L, 2, &x1, &y1) == false) {
        LUAUTILS_ERROR(L, "Number2:Lerp - first argument should be a Number2 or table of numbers");
    }

    float x2 = 0, y2 = 0;
    if (lua_number2_or_table_getXY(L, 3, &x2, &y2) == false) {
        LUAUTILS_ERROR(L, "Number2:Lerp - second argument should be a Number2 or table of numbers");
    }

    if (lua_isnumber(L, 4) == false) {
        LUAUTILS_ERROR(L, "Number2:Lerp - third argument should be a number");
    }

    const float v = static_cast<float>(lua_tonumber(L, 4));

    x1 = x1 + (x2 - x1) * v;
    y1 = y1 + (y2 - y1) * v;

    lua_number2_setXY(L, 1, &x1, &y1);

    LUAUTILS_STACK_SIZE_ASSERT(L, 0)
    return 0;
}

static int _number2_set(lua_State *L) {
    vx_assert(L != nullptr);
    LUAUTILS_STACK_SIZE(L)

    const int argCount = lua_gettop(L);

    if (argCount < 1 || lua_utils_getObjectType(L, 1) != ITEM_TYPE_NUMBER2) {
        LUAUTILS_ERROR(L, "Number2:Set - function should be called with `:`");
    }

    if (argCount == 3) {
        if (lua_isnumber(L, 2) == false) {
            LUAUTILS_ERROR(L, "Number2:Set - argument 1 should be a number");
        }
        float x = lua_tonumber(L, 2);
        if (lua_isnumber(L, 3) == false) {
            LUAUTILS_ERROR(L, "Number2:Set - argument 2 should be a number");
        }
        float y = lua_tonumber(L, 3);

        lua_number2_setXY(L, 1, &x, &y);
    } else if (argCount == 2) {
        float x = 0.0f;
        float y = 0.0f;
        if (lua_number2_or_table_getXY(L, 2, &x, &y) == false) {
            LUAUTILS_ERROR(L, "Number2:Set - argument should be a Number2 or table of numbers");
        }

        lua_number2_setXY(L, 1, &x, &y);
    } else {
        LUAUTILS_ERROR(L, "Number2:Set - wrong number of arguments");
    }

    lua_pushvalue(L, 1);

    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
    return 1;
}
