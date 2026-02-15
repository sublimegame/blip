//
//  lua_color.cpp
//  Cubzh
//
//  Created by Adrien Duermael on 7/8/20.
//

#include "lua_color.hpp"

// C++
#include <cstring>

// Lua
#include "lua.hpp"
#include "lua_utils.hpp"

#include "utils.h"

#include "VXColor.hpp"

// xptools
#include "xptools.h"

// sandbox
#include "sbs.hpp"

#define LUA_G_COLOR_FIELD_RANDOM "Random"
#define LUA_G_COLOR_FIELD_CLEAR "Clear"

#define LUA_COLOR_FIELD_R "R"
#define LUA_COLOR_FIELD_RED "Red"
#define LUA_COLOR_FIELD_G "G"
#define LUA_COLOR_FIELD_GREEN "Green"
#define LUA_COLOR_FIELD_B "B"
#define LUA_COLOR_FIELD_BLUE "Blue"
#define LUA_COLOR_FIELD_A "A"
#define LUA_COLOR_FIELD_ALPHA "Alpha"
#define LUA_COLOR_FIELD_LIGHT "Light" // undocumented (TODO:shouldn't be here, but only in BlockProperties.Light)
#define LUA_COLOR_FIELD_APPLY_BRIGHTNESS_DIFF "ApplyBrightnessDiff" // deprecated TODO: set warning message "Deprecated, use color.V instead"
#define LUA_COLOR_FIELD_LERP "Lerp"
#define LUA_COLOR_FIELD_SET "Set"
#define LUA_COLOR_FIELD_COPY "Copy"

// by default, colors are stored in RGB
// but users can access and set HSV components too
#define LUA_COLOR_FIELD_H "H"
#define LUA_COLOR_FIELD_HUE "Hue"
#define LUA_COLOR_FIELD_S "S"
#define LUA_COLOR_FIELD_SATURATION "Saturation"
#define LUA_COLOR_FIELD_V "V"
#define LUA_COLOR_FIELD_VALUE "Value"
#define LUA_COLOR_FIELD_HSV "HSV"

#define LUA_G_COLOR_INSTANCE_METATABLE "__imt"

// Default handler
typedef struct color_C_handler {
    color_C_handler_set set;
    color_C_handler_get get;
    color_C_handler_set_hsv setHSV;
    color_C_handler_get_hsv getHSV;
    color_C_handler_free free_userdata;
    void *userdata;
} color_C_handler;

static void _free_color_handler_userdata(void *userdata) {
    color_C_handler_userdata *_userdata = static_cast<color_C_handler_userdata *>(userdata);
    if (_userdata->hsvColor != nullptr) {
        free(_userdata->hsvColor);
    }
    free(userdata);
}

typedef struct color_userdata {
    color_C_handler *handler;
    bool readOnly; // false by default
} color_userdata;

void color_userdata_flush(color_userdata *ud) {
    ud->handler->free_userdata(ud->handler->userdata);
    free(ud->handler);
}

static void _set_color_handler(const uint8_t *r,
                               const uint8_t *g,
                               const uint8_t *b,
                               const uint8_t *a,
                               const bool *light,
                               lua_State *L,
                               void *userdata) {
    
    color_C_handler_userdata *_userdata = static_cast<color_C_handler_userdata *>(userdata);
    
    if (r != nullptr) _userdata->color.r = CLAMP(*r, 0, 255);
    if (g != nullptr) _userdata->color.g = CLAMP(*g, 0, 255);
    if (b != nullptr) _userdata->color.b = CLAMP(*b, 0, 255);
    if (a != nullptr) _userdata->color.a = CLAMP(*a, 0, 255);
    if (light != nullptr) _userdata->light = *light;
    
    // invalidate hsv
    if (_userdata->hsvColor != nullptr) {
        free(_userdata->hsvColor);
        _userdata->hsvColor = nullptr;
    }
}

static void _get_color_handler(uint8_t *r,
                               uint8_t *g,
                               uint8_t *b,
                               uint8_t *a,
                               bool *light,
                               lua_State *L,
                               const void *userdata) {
    
    const color_C_handler_userdata *_userdata = static_cast<const color_C_handler_userdata *>(userdata);
    
    if (r != nullptr) *r = _userdata->color.r;
    if (g != nullptr) *g = _userdata->color.g;
    if (b != nullptr) *b = _userdata->color.b;
    if (a != nullptr) *a = _userdata->color.a;
    if (light != nullptr) *light = _userdata->light;
}

static void _set_color_hsv_handler(const double *h,
                                   const double *s,
                                   const double *v,
                                   lua_State *L,
                                   void *userdata) {
    
    color_C_handler_userdata *_userdata = static_cast<color_C_handler_userdata *>(userdata);
    
    if (_userdata->hsvColor == nullptr) {
        _userdata->hsvColor = static_cast<hsv*>(malloc(sizeof(hsv)));
        
        rgb c;
        c.r = static_cast<double>(_userdata->color.r) / 255.0;
        c.g = static_cast<double>(_userdata->color.g) / 255.0;
        c.b = static_cast<double>(_userdata->color.b) / 255.0;
        
        hsv c2 = rgb2hsv(c);
        _userdata->hsvColor->h = c2.h;
        _userdata->hsvColor->s = c2.s;
        _userdata->hsvColor->v = c2.v;
    }
    
    if (h != nullptr) _userdata->hsvColor->h = CLAMP(*h, 0.0, 360.0);
    if (s != nullptr) _userdata->hsvColor->s = CLAMP01(*s);
    if (v != nullptr) _userdata->hsvColor->v = CLAMP01(*v);
    
    rgb c = hsv2rgb(*_userdata->hsvColor);
    _userdata->color.r = static_cast<uint8_t>(c.r * 255.0);
    _userdata->color.g = static_cast<uint8_t>(c.g * 255.0);
    _userdata->color.b = static_cast<uint8_t>(c.b * 255.0);
}

static void _get_color_hsv_handler(double *h,
                                   double *s,
                                   double *v,
                                   lua_State *L,
                                   void *userdata) {
    
    color_C_handler_userdata *_userdata = static_cast<color_C_handler_userdata *>(userdata);
    
    if (_userdata->hsvColor == nullptr) {
        _userdata->hsvColor = static_cast<hsv*>(malloc(sizeof(hsv)));
        
        rgb c;
        c.r = static_cast<double>(_userdata->color.r) / 255.0;
        c.g = static_cast<double>(_userdata->color.g) / 255.0;
        c.b = static_cast<double>(_userdata->color.b) / 255.0;
        
        hsv c2 = rgb2hsv(c);
        _userdata->hsvColor->h = c2.h;
        _userdata->hsvColor->s = c2.s;
        _userdata->hsvColor->v = c2.v;
    }
    
    if (h != nullptr) *h = _userdata->hsvColor->h;
    if (s != nullptr) *s = _userdata->hsvColor->s;
    if (v != nullptr) *v = _userdata->hsvColor->v;
}

/// __index for global Color
static int _g_color_metatable_index(lua_State *L);

/// __call for global Color
static int _g_color_metatable_call(lua_State *L);

/// __index for Color
static int _color_metatable_index(lua_State *L);

/// __newindex for Color
static int _color_metatable_newindex(lua_State *L);

static void _color_metatable_gc(void *ud);
static int _color_metatable_add(lua_State *L);
static int _color_metatable_sub(lua_State *L);
static int _color_metatable_mul(lua_State *L);
static int _color_metatable_div(lua_State *L);
static int _color_metatable_idiv(lua_State *L);
static int _color_metatable_eq(lua_State *L);

static int _lua_color_apply_brightness_diff(lua_State *L);
static int _lua_color_lerp(lua_State *L);
static int _lua_color_set(lua_State *L);
static int _lua_color_copy(lua_State *L);

// Utility functions
static uint8_t _color_attribute_from_float(const float attr);

static const std::map<std::string, std::vector<uint8_t>> DEFAULT_COLORS = {
    { "Black", { 0, 0, 0 } },
    { "White", { 255, 255, 255 } },
    { "LightGrey", { 212, 212, 212 } },
    { "Grey", { 168, 168, 168 } },
    { "DarkGrey", { 84, 84, 84 } },
    { "Red", { 255, 18, 0 } },
    { "Green", { 6, 238, 0 } },
    { "Blue", { 0, 120, 255 } },
    { "DarkRed", { 184, 13, 0 } },
    { "DarkGreen", { 20, 160, 17 } },
    { "DarkBlue", { 0, 81, 173 } },
    { "LightBlue", { 76, 215, 255 } },
    { "Brown", { 86, 51, 23 } },
    { "Pink", { 255, 12, 236 } },
    { "Yellow", { 255, 224, 58 } },
    { "Orange", { 253, 110, 14 } },
    { "Purple", { 173, 49, 255 } },
    { "Beige", { 255, 220, 191 } },
};

void lua_g_color_create_and_push(lua_State *L) {
    vx_assert(L != nullptr);
    LUAUTILS_STACK_SIZE(L)

    lua_newuserdata(L, 1);
    lua_newtable(L); // metatable
    {
        lua_pushstring(L, "__index");
        lua_pushcfunction(L, _g_color_metatable_index, "");
        lua_rawset(L, -3);

        lua_pushstring(L, "__call");
        lua_pushcfunction(L, _g_color_metatable_call, "");
        lua_rawset(L, -3);

        lua_pushstring(L, "__metatable");
        lua_pushboolean(L, false);
        lua_rawset(L, -3);

        lua_pushstring(L, "__type");
        lua_pushstring(L, "ColorInterface");
        lua_rawset(L, -3);

        lua_pushstring(L, "__typen");
        lua_pushinteger(L, ITEM_TYPE_G_COLOR);
        lua_rawset(L, -3);

        lua_pushstring(L, LUA_G_COLOR_INSTANCE_METATABLE);
        lua_newtable(L);
        {
            lua_pushstring(L, "__index");
            lua_pushcfunction(L, _color_metatable_index, "");
            lua_rawset(L, -3);

            lua_pushstring(L, "__newindex");
            lua_pushcfunction(L, _color_metatable_newindex, "");
            lua_rawset(L, -3);

            lua_pushstring(L, "__eq");
            lua_pushcfunction(L, _color_metatable_eq, "");
            lua_rawset(L, -3);

            lua_pushstring(L, "__add");
            lua_pushcfunction(L, _color_metatable_add, "");
            lua_rawset(L, -3);

            lua_pushstring(L, "__sub");
            lua_pushcfunction(L, _color_metatable_sub, "");
            lua_rawset(L, -3);

            lua_pushstring(L, "__mul");
            lua_pushcfunction(L, _color_metatable_mul, "");
            lua_rawset(L, -3);

            lua_pushstring(L, "__div");
            lua_pushcfunction(L, _color_metatable_div, "");
            lua_rawset(L, -3);

            lua_pushstring(L, "__idiv");
            lua_pushcfunction(L, _color_metatable_idiv, "");
            lua_rawset(L, -3);

            lua_pushstring(L, "__metatable");
            lua_pushboolean(L, false);
            lua_rawset(L, -3);

            lua_pushstring(L, "__type");
            lua_pushstring(L, "Color");
            lua_rawset(L, -3);

            lua_pushstring(L, "__typen");
            lua_pushinteger(L, ITEM_TYPE_COLOR);
            lua_rawset(L, -3);
        }
        lua_rawset(L, -3);
    }
    lua_setmetatable(L, -2);
    
    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
}

/// arguments
/// - lua table : global Color table
/// - lua string : the key that is being accessed
static int _g_color_metatable_index(lua_State *L) {
    
    RETURN_VALUE_IF_NULL(L, 0)
    
    LUAUTILS_STACK_SIZE(L)

    // validate arguments count
    if (!lua_utils_assertArgsCount(L, 2)) {
        LUAUTILS_ERROR(L, "incorrect argument count"); // returns
    }

    // check 1st argument: global Color table
    if (lua_utils_getObjectType(L, 1) != ITEM_TYPE_G_COLOR) {
        LUAUTILS_ERROR(L, "incorrect argument type");
    }

    // get 2nd argument: key string
    if (lua_utils_isStringStrict(L, 2) == false) {
        LUAUTILS_ERROR(L, "incorrect argument 2"); // returns
    }
    
    const char *key = lua_tostring(L, 2);
    LUAUTILS_STACK_SIZE_ASSERT(L, 0)

    // Looking for the key in default colors
    const auto value = DEFAULT_COLORS.find(key);
    if (value != DEFAULT_COLORS.end()) {
        std::vector<uint8_t> color = value->second;
        lua_color_create_and_push_table(L, color[0], color[1], color[2], 255, false, true);
        LUAUTILS_STACK_SIZE_ASSERT(L, 1)
        return 1; // the __index function always has 1 return value
    } else if (strcmp(key, LUA_G_COLOR_FIELD_RANDOM) == 0) {
        lua_color_create_and_push_table(L, frand(), frand(), frand(), 1.0f, false, true);
        LUAUTILS_STACK_SIZE_ASSERT(L, 1)
        return 1;
    } else if (strcmp(key, LUA_G_COLOR_FIELD_CLEAR) == 0) {
        lua_color_create_and_push_table(L, 0.0f, 0.0f, 0.0f, 0.0f, false, true);
        LUAUTILS_STACK_SIZE_ASSERT(L, 1)
        return 1;
    }
    lua_pushnil(L);

    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
    return 1; // the __index function always has 1 return value
}

/// Color(r, g, b, a (optional))
/// Arguments:
/// 1: red (float between 0 and 1 or int between 0 and 255) (optional, default: 1.0)
/// 2: green (float between 0 and 1 or int between 0 and 255) (optional, default: 1.0)
/// 3: blue (float between 0 and 1 or int between 0 and 255) (optional, default: 1.0)
/// 4: alpha (float between 0 and 1 or int between 0 and 255) (optional, default: 1.0)
static int _g_color_metatable_call(lua_State *L) {
    
    RETURN_VALUE_IF_NULL(L, 0)
    
    LUAUTILS_STACK_SIZE(L)
    
    int nbParameters = lua_gettop(L);
    
    bool isColorCopy = false;
    if (nbParameters == 2) {
        if (lua_utils_getObjectType(L, 2) != ITEM_TYPE_COLOR) {
            LUAUTILS_ERROR(L, "incorrect argument type"); // returns
        }
        isColorCopy = true;
    }
    
    uint8_t r = 255;
    uint8_t g = 255;
    uint8_t b = 255;
    uint8_t a = 255;
    
    if (isColorCopy) {
        lua_color_getRGBAL(L, 2, &r, &g, &b, &a, nullptr);
    } else {
        if (nbParameters < 4 || nbParameters > 5) { // global Color + r + g + b + a (optional)
            LUAUTILS_ERROR(L, "incorrect argument count"); // returns
        }
        
        if (lua_utils_getObjectType(L, 1) != ITEM_TYPE_G_COLOR) {
            LUAUTILS_ERROR(L, "incorrect argument type"); // returns
        }
        // r
        if (lua_utils_isIntegerStrict(L, 2)) {
            r = static_cast<uint8_t>(lua_tointeger(L, 2));
        } else if (lua_isnumber(L, 2)) {
            float rf = lua_tonumber(L, 2);
            r = _color_attribute_from_float(rf);
        } else {
            LUAUTILS_ERROR(L, "incorrect argument type"); // returns
        }
        
        // g
        if (lua_utils_isIntegerStrict(L, 3)) {
            g = static_cast<uint8_t>(lua_tointeger(L, 3));
        } else if (lua_isnumber(L, 3)) {
            float gf = lua_tonumber(L, 3);
            g = _color_attribute_from_float(gf);
        } else {
            LUAUTILS_ERROR(L, "incorrect argument type"); // returns
        }
        
        // b
        if (lua_utils_isIntegerStrict(L, 4)) {
            b = static_cast<uint8_t>(lua_tointeger(L, 4));
        } else if (lua_isnumber(L, 4)) {
            float bf = lua_tonumber(L, 4);
            b = _color_attribute_from_float(bf);
        } else {
            LUAUTILS_ERROR(L, "incorrect argument type"); // returns
        }
        
        // a
        if (nbParameters == 5) {
            if (lua_utils_isIntegerStrict(L, 5)) {
                a = static_cast<uint8_t>(lua_tointeger(L, 5));
            } else if (lua_isnumber(L, 5)) {
                float af = lua_tonumber(L, 5);
                a = _color_attribute_from_float(af);
            } else {
                LUAUTILS_ERROR(L, "incorrect argument type"); // returns
            }
        }
    }

    lua_color_create_and_push_table(L, r, g, b, a, false, false);

    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
    return 1;
}

void lua_color_create_and_push_table(lua_State *L,
                                     const float r,
                                     const float g,
                                     const float b,
                                     const float a,
                                     const bool light,
                                     const bool readOnly) {
    
    lua_color_create_and_push_table(L,
                                    _color_attribute_from_float(r),
                                    _color_attribute_from_float(g),
                                    _color_attribute_from_float(b),
                                    _color_attribute_from_float(a),
                                    light,
                                    readOnly);
}

bool lua_color_getHSV(lua_State *L, const int idx, double *h, double *s, double *v) {
    
    RETURN_VALUE_IF_NULL(L, false)
    
    LUAUTILS_STACK_SIZE(L)
 
    if (lua_utils_getObjectType(L, idx) != ITEM_TYPE_COLOR) {
        LUAUTILS_STACK_SIZE_ASSERT(L, 0)
        return false;
    }

    color_userdata *ud = static_cast<color_userdata*>(lua_touserdata(L, idx));
    color_C_handler *handler = ud->handler;
    
    if (handler->getHSV == nullptr) {
        LUAUTILS_STACK_SIZE_ASSERT(L, 0)
        return false;
    }
    
    handler->getHSV(h, s, v, L, handler->userdata);
    LUAUTILS_STACK_SIZE_ASSERT(L, 0)
    return true;
}

bool lua_color_setHSV(lua_State *L, const int idx, const double *h, const double *s, const double *v) {
    RETURN_VALUE_IF_NULL(L, false)
    LUAUTILS_STACK_SIZE(L)
    
    if (lua_utils_getObjectType(L, idx) != ITEM_TYPE_COLOR) {
        LUAUTILS_STACK_SIZE_ASSERT(L, 0)
        return false;
    }

    color_userdata *ud = static_cast<color_userdata*>(lua_touserdata(L, idx));
    color_C_handler *handler = ud->handler;
    
    if (handler->setHSV == nullptr) {
        LUAUTILS_STACK_SIZE_ASSERT(L, 0)
        return false;
    }
    
    handler->setHSV(h, s, v, L, handler->userdata);
    LUAUTILS_STACK_SIZE_ASSERT(L, 0)
    return true;
}

bool lua_color_getRGBAL(lua_State *L, const int idx, uint8_t *r, uint8_t *g, uint8_t *b, uint8_t *a, bool *light) {
    
    RETURN_VALUE_IF_NULL(L, false)
    
    LUAUTILS_STACK_SIZE(L)
 
    if (lua_utils_getObjectType(L, idx) != ITEM_TYPE_COLOR) {
        LUAUTILS_STACK_SIZE_ASSERT(L, 0)
        return false;
    }

    color_userdata *ud = static_cast<color_userdata*>(lua_touserdata(L, idx));
    color_C_handler *handler = ud->handler;
    
    if (handler->get == nullptr) {
        LUAUTILS_STACK_SIZE_ASSERT(L, 0)
        return false;
    }
    
    handler->get(r, g, b, a, light, L, handler->userdata);
    LUAUTILS_STACK_SIZE_ASSERT(L, 0)
    return true;
    
}

bool lua_color_setRGBAL(lua_State *L, const int idx, const uint8_t *r, const uint8_t *g, const uint8_t *b, const uint8_t *a, const bool *light) {
    RETURN_VALUE_IF_NULL(L, false)
    LUAUTILS_STACK_SIZE(L)
    
    if (lua_utils_getObjectType(L, idx) != ITEM_TYPE_COLOR) {
        LUAUTILS_STACK_SIZE_ASSERT(L, 0)
        return false;
    }

    color_userdata *ud = static_cast<color_userdata*>(lua_touserdata(L, idx));
    color_C_handler *handler = ud->handler;
    
    if (handler->set == nullptr) {
        LUAUTILS_STACK_SIZE_ASSERT(L, 0)
        return false;
    }
    
    handler->set(r, g, b, a, light, L, handler->userdata);
    LUAUTILS_STACK_SIZE_ASSERT(L, 0)
    return true;
    
}

bool lua_color_getRGBAL_float(lua_State *L, const int idx, float *r, float *g, float *b, float *a, bool *light) {
    
    uint8_t _r, _g, _b, _a;
    
    if (lua_color_getRGBAL(L, idx, &_r, &_g, &_b, &_a, light) == false) {
        return false;
    }
    
    if (r != nullptr) *r = static_cast<float>(_r) / 255.0f;
    if (g != nullptr) *g = static_cast<float>(_g) / 255.0f;
    if (b != nullptr) *b = static_cast<float>(_b) / 255.0f;
    if (a != nullptr) *a = static_cast<float>(_a) / 255.0f;
    
    return true;
}

bool lua_color_setRGBAL_float(lua_State *L, const int idx, const float *r, const float *g, const float *b, const float *a, const bool *light) {
    
    uint8_t _r, _g, _b, _a;
    
    if (r != nullptr) _r = _color_attribute_from_float(*r);
    if (g != nullptr) _g = _color_attribute_from_float(*g);
    if (b != nullptr) _b = _color_attribute_from_float(*b);
    if (a != nullptr) _a = _color_attribute_from_float(*a);
    
    return lua_color_setRGBAL(L, idx,
                              r != nullptr ? &_r : nullptr,
                              g != nullptr ? &_g : nullptr,
                              b != nullptr ? &_b : nullptr,
                              a != nullptr ? &_a : nullptr,
                              light);
}

    
void lua_color_create_and_push_table(lua_State *L,
                                     const uint8_t r,
                                     const uint8_t g,
                                     const uint8_t b,
                                     const uint8_t a,
                                     const bool light,
                                     bool readOnly) {

    // color_userdata* ud = static_cast<color_userdata*>(lua_newuserdata(L, sizeof(color_userdata)));
    color_userdata* ud = static_cast<color_userdata*>(lua_newuserdatadtor(L, sizeof(color_userdata), _color_metatable_gc));
    
    // default handler
    color_C_handler *handler = static_cast<color_C_handler*>(malloc(sizeof(color_C_handler)));
    handler->set = _set_color_handler;
    handler->get = _get_color_handler;
    handler->setHSV = _set_color_hsv_handler;
    handler->getHSV = _get_color_hsv_handler;
    handler->free_userdata = _free_color_handler_userdata;
    color_C_handler_userdata* handler_ud = static_cast<color_C_handler_userdata*>(malloc(sizeof(color_C_handler_userdata)));
    handler_ud->hsvColor = nullptr;
    handler->userdata = handler_ud;
    ud->handler = handler;

    ud->readOnly = false;

    lua_getglobal(L, P3S_LUA_G_COLOR);
    LUA_GET_METAFIELD(L, -1, LUA_G_COLOR_INSTANCE_METATABLE);
    lua_remove(L, -2);
    lua_setmetatable(L, -2);

    lua_color_setRGBAL(L, -1, &r, &g, &b, &a, &light);
}

bool lua_color_setHandlers(lua_State *L,
                           const int idx,
                           color_C_handler_set set,
                           color_C_handler_get get,
                           color_C_handler_free free_userdata,
                           void *userdata) {
    
    RETURN_VALUE_IF_NULL(L, false)
    
    LUAUTILS_STACK_SIZE(L)
    
    if (lua_utils_getObjectType(L, idx) != ITEM_TYPE_COLOR) {
        LUAUTILS_INTERNAL_ERROR(L) // returns
    }

    color_userdata *ud = static_cast<color_userdata*>(lua_touserdata(L, idx));

    // free current handlers
    color_userdata_flush(ud);
    
    color_C_handler *handler = static_cast<color_C_handler*>(malloc(sizeof(color_C_handler)));
    handler->set = set;
    handler->get = get;
    handler->free_userdata = free_userdata;
    handler->userdata = userdata;

    ud->handler = handler;

    LUAUTILS_STACK_SIZE_ASSERT(L, 0)
    
    return true;
}

/// arguments
/// - table (table)  : Color table
/// - key   (string) : the key that is being accessed
static int _color_metatable_index(lua_State *L) {
    
    RETURN_VALUE_IF_NULL(L, 0)
    
    LUAUTILS_STACK_SIZE(L)
    
    const char *key = lua_tostring(L, 2);
    
    // getting non-string or non-capitalized string keys is NOT allowed
    if (lua_utils_isStringStrict(L, 2) == false || lua_utils_isStringStrictStartingWithUppercase(L, 2) == false) {
        LUAUTILS_ERROR_F(L, "Color.%s is undefined", key); // returns
    }
    
    if (strcmp(key, LUA_COLOR_FIELD_R) == 0 || strcmp(key, LUA_COLOR_FIELD_RED) == 0) {
        
        uint8_t r;
        if (lua_color_getRGBAL(L, 1, &r, nullptr, nullptr, nullptr, nullptr)) {
            lua_pushinteger(L, r);
        } else {
            lua_pushnil(L);
        }
        
    } else if (strcmp(key, LUA_COLOR_FIELD_G) == 0 || strcmp(key, LUA_COLOR_FIELD_GREEN) == 0) {
        
        uint8_t g;
        if (lua_color_getRGBAL(L, 1, nullptr, &g, nullptr, nullptr, nullptr)) {
            lua_pushinteger(L, g);
        } else {
            lua_pushnil(L);
        }
        
    } else if (strcmp(key, LUA_COLOR_FIELD_B) == 0 || strcmp(key, LUA_COLOR_FIELD_BLUE) == 0) {
        
        uint8_t b;
        if (lua_color_getRGBAL(L, 1, nullptr, nullptr, &b, nullptr, nullptr)) {
            lua_pushinteger(L, b);
        } else {
            lua_pushnil(L);
        }
        
    } else if (strcmp(key, LUA_COLOR_FIELD_A) == 0 || strcmp(key, LUA_COLOR_FIELD_ALPHA) == 0) {
        
        uint8_t a;
        if (lua_color_getRGBAL(L, 1, nullptr, nullptr, nullptr, &a, nullptr)) {
            lua_pushinteger(L, a);
        } else {
            lua_pushnil(L);
        }
        
    } else if (strcmp(key, LUA_COLOR_FIELD_LIGHT) == 0) {
        
        bool light;
        if (lua_color_getRGBAL(L, 1, nullptr, nullptr, nullptr, nullptr, &light)) {
            lua_pushboolean(L, light);
        } else {
            lua_pushnil(L);
        }
    } else if (strcmp(key, LUA_COLOR_FIELD_H) == 0 || strcmp(key, LUA_COLOR_FIELD_HUE) == 0) {
        
        double h;
        if (lua_color_getHSV(L, 1, &h, nullptr, nullptr)) {
            lua_pushnumber(L, h);
        } else {
            lua_pushnil(L);
        }
        
    } else if (strcmp(key, LUA_COLOR_FIELD_S) == 0 || strcmp(key, LUA_COLOR_FIELD_SATURATION) == 0) {
        
        double s;
        if (lua_color_getHSV(L, 1, nullptr, &s, nullptr)) {
            lua_pushnumber(L, s);
        } else {
            lua_pushnil(L);
        }
        
    } else if (strcmp(key, LUA_COLOR_FIELD_V) == 0 || strcmp(key, LUA_COLOR_FIELD_VALUE) == 0) {
        
        double v;
        if (lua_color_getHSV(L, 1, nullptr, nullptr, &v)) {
            lua_pushnumber(L, v);
        } else {
            lua_pushnil(L);
        }
        
    } else if (strcmp(key, LUA_COLOR_FIELD_HSV) == 0) {
        
        double h, s, v;
        if (lua_color_getHSV(L, 1, &h, &s, &v)) {
            lua_pushnumber(L, h);
            lua_pushnumber(L, s);
            lua_pushnumber(L, v);
        } else {
            lua_pushnil(L);
            lua_pushnil(L);
            lua_pushnil(L);
        }
        
        LUAUTILS_STACK_SIZE_ASSERT(L, 3)
        return 3;
        
    } else if (strcmp(key, LUA_COLOR_FIELD_APPLY_BRIGHTNESS_DIFF) == 0) {

        lua_pushcfunction(L, _lua_color_apply_brightness_diff, "");
    } else if (strcmp(key, LUA_COLOR_FIELD_LERP) == 0) {
        lua_pushcfunction(L, _lua_color_lerp, "");
    } else if (strcmp(key, LUA_COLOR_FIELD_SET) == 0) {
        lua_pushcfunction(L, _lua_color_set, "");
    } else if (strcmp(key, LUA_COLOR_FIELD_COPY) == 0) {
        lua_pushcfunction(L, _lua_color_copy, "");
    } else {
        LUAUTILS_ERROR_F(L, "Color.%s is undefined", key); // returns
    }
    
    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
    return 1;
}

static int _lua_color_apply_brightness_diff(lua_State *L) {
    vx_assert(L != nullptr);
    LUAUTILS_ASSERT_ARGS_COUNT(L, 2)
    LUAUTILS_ASSERT_ARG_TYPE(L, 1, ITEM_TYPE_COLOR)
    LUAUTILS_STACK_SIZE(L)

    uint8_t r = 0;
    uint8_t g = 0;
    uint8_t b = 0;
    uint8_t a = 0;
    lua_color_getRGBAL(L, 1, &r, &g, &b, &a, nullptr);
    vx::Color color = vx::Color(r,g,b,a);
    double diff = lua_tonumber(L, 2);
    color.applyBrightnessDiff(diff);
    lua_color_setRGBAL(L, 1, &color.r(), &color.g(), &color.b(), &color.a(), nullptr);
    LUAUTILS_STACK_SIZE_ASSERT(L, 0)
    return 0;
}

static int _lua_color_lerp(lua_State *L) {
    vx_assert(L != nullptr);
    LUAUTILS_STACK_SIZE(L)

    const int argCount = lua_gettop(L);

    if (argCount < 1 || lua_utils_getObjectType(L, 1) != ITEM_TYPE_COLOR) {
        LUAUTILS_ERROR(L, "Color:Lerp - function should be called with `:`");
    }

    if (argCount != 4) {
        LUAUTILS_ERROR(L, "Color:Lerp - wrong number of arguments, expected 3"); // returns
    }

    uint8_t r1 = 0, g1 = 0, b1 = 0, a1 = 0;
    if (lua_color_getRGBAL(L, 2, &r1, &g1, &b1, &a1, nullptr) == false) {
        LUAUTILS_ERROR(L, "Color:Lerp - first argument should be a Color");
    }

    uint8_t r2 = 0, g2 = 0, b2 = 0, a2 = 0;
    if (lua_color_getRGBAL(L, 3, &r2, &g2, &b2, &a2, nullptr) == false) {
        LUAUTILS_ERROR(L, "Color:Lerp - second argument should be a Color");
    }

    if (lua_isnumber(L, 4) == false) {
        LUAUTILS_ERROR(L, "Color:Lerp - third argument should be a number");
    }

    double v = lua_tonumber(L, 4);

    r1 = r1 + (r2 - r1) * v;
    g1 = g1 + (g2 - g1) * v;
    b1 = b1 + (b2 - b1) * v;
    a1 = a1 + (a2 - a1) * v;

    lua_color_setRGBAL(L, 1, &r1, &g1, &b1, &a1, nullptr);

    LUAUTILS_STACK_SIZE_ASSERT(L, 0)
    return 0;
}

static int _lua_color_set(lua_State *L) {
    vx_assert(L != nullptr);
    LUAUTILS_STACK_SIZE(L)

    const int argCount = lua_gettop(L);

    if (argCount < 1 || lua_utils_getObjectType(L, 1) != ITEM_TYPE_COLOR) {
        LUAUTILS_ERROR(L, "Color:Set - function should be called with `:`");
    }

    if (argCount >= 4 && argCount <= 6) {
        uint8_t r, g, b, a;
        bool light = false;

        // r
        if (lua_utils_isIntegerStrict(L, 2)) {
            r = static_cast<uint8_t>(lua_tointeger(L, 2));
        } else if (lua_isnumber(L, 2)) {
            float rf = lua_tonumber(L, 2);
            r = _color_attribute_from_float(rf);
        } else {
            LUAUTILS_ERROR(L, "Color:Set - argument 1 should be a number or integer");
        }

        // g
        if (lua_utils_isIntegerStrict(L, 3)) {
            g = static_cast<uint8_t>(lua_tointeger(L, 3));
        } else if (lua_isnumber(L, 3)) {
            float gf = lua_tonumber(L, 3);
            g = _color_attribute_from_float(gf);
        } else {
            LUAUTILS_ERROR(L, "Color:Set - argument 2 should be a number or integer");
        }

        // b
        if (lua_utils_isIntegerStrict(L, 4)) {
            b = static_cast<uint8_t>(lua_tointeger(L, 4));
        } else if (lua_isnumber(L, 4)) {
            float bf = lua_tonumber(L, 4);
            b = _color_attribute_from_float(bf);
        } else {
            LUAUTILS_ERROR(L, "Color:Set - argument 3 should be a number or integer");
        }

        // a
        if (argCount == 5) {
            if (lua_utils_isIntegerStrict(L, 5)) {
                a = static_cast<uint8_t>(lua_tointeger(L, 5));
            } else if (lua_isnumber(L, 5)) {
                float af = lua_tonumber(L, 5);
                a = _color_attribute_from_float(af);
            } else {
                LUAUTILS_ERROR(L, "Color:Set - argument 4 should be a number or integer");
            }
        }

        // light
        if (argCount == 6) {
            if (lua_isboolean(L, 6) == false) {
                LUAUTILS_ERROR(L, "Color:Set - argument 5 should be a boolean");
            }
            light = lua_toboolean(L, 6);
        }

        lua_color_setRGBAL(L, 1, &r, &g, &b, &a, &light);
    } else {
        LUAUTILS_ERROR(L, "Color:Set - wrong number of arguments");
    }

    LUAUTILS_STACK_SIZE_ASSERT(L, 0)
    return 0;
}

static int _lua_color_copy(lua_State *L) {
    vx_assert(L != nullptr);
    LUAUTILS_STACK_SIZE(L)

    const int argCount = lua_gettop(L);

    if (argCount < 1 || lua_utils_getObjectType(L, 1) != ITEM_TYPE_COLOR) {
        LUAUTILS_ERROR(L, "Color:Copy - function should be called with `:`");
    }

    if (argCount != 1) {
        LUAUTILS_ERROR(L, "Color:Copy - function expects no argument");
    }

    uint8_t r, g, b, a;
    bool light;
    lua_color_getRGBAL(L, 1, &r, &g, &b, &a, &light);

    lua_color_create_and_push_table(L, r, g, b, a, light, false);

    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
    return 1;
}

/// arguments
/// - table (table)  : Color table
/// - key   (string) : the key that is being set
/// - value (any)    : the value being stored
static int _color_metatable_newindex(lua_State *L) {
    vx_assert(L != nullptr);
    LUAUTILS_ASSERT_ARGS_COUNT(L, 3)
    LUAUTILS_ASSERT_ARG_TYPE(L, 1, ITEM_TYPE_COLOR)
    LUAUTILS_STACK_SIZE(L)

    color_userdata *ud = static_cast<color_userdata*>(lua_touserdata(L, 1));
    if (ud->readOnly) {
        LUAUTILS_ERROR(L, "read-only Color cannot be modified"); // returns
    }

    const char *key = lua_tostring(L, 2);
    
    if (strcmp(key, LUA_COLOR_FIELD_R) == 0 || strcmp(key, LUA_COLOR_FIELD_RED) == 0) {
        
        uint8_t r = 255;
        
        if (lua_utils_isIntegerStrict(L, 3)) {
            r = static_cast<uint8_t>(lua_tointeger(L, 3));
        } else if (lua_isnumber(L, 3)) {
            float rf = lua_tonumber(L, 3);
            r = _color_attribute_from_float(rf);
        } else {
            LUAUTILS_ERROR(L, "incorrect argument type"); // returns
        }
        
        lua_color_setRGBAL(L, 1, &r, nullptr, nullptr, nullptr, nullptr);

        LUAUTILS_STACK_SIZE_ASSERT(L, 0)
        
    } else if (strcmp(key, LUA_COLOR_FIELD_G) == 0 || strcmp(key, LUA_COLOR_FIELD_GREEN) == 0) {
        
        uint8_t g = 255;
        
        if (lua_utils_isIntegerStrict(L, 3)) {
            g = static_cast<uint8_t>(lua_tointeger(L, 3));
        } else if (lua_isnumber(L, 3)) {
            float gf = lua_tonumber(L, 3);
            g = _color_attribute_from_float(gf);
        } else {
            LUAUTILS_ERROR(L, "incorrect argument type"); // returns
        }
        
        lua_color_setRGBAL(L, 1, nullptr, &g, nullptr, nullptr, nullptr);

        LUAUTILS_STACK_SIZE_ASSERT(L, 0)
        
    } else if (strcmp(key, LUA_COLOR_FIELD_B) == 0 || strcmp(key, LUA_COLOR_FIELD_BLUE) == 0) {
        
        uint8_t b = 255;
        
        if (lua_utils_isIntegerStrict(L, 3)) {
            b = static_cast<uint8_t>(lua_tointeger(L, 3));
        } else if (lua_isnumber(L, 3)) {
            float bf = lua_tonumber(L, 3);
            b = _color_attribute_from_float(bf);
        } else {
            LUAUTILS_ERROR(L, "incorrect argument type"); // returns
        }
        
        lua_color_setRGBAL(L, 1, nullptr, nullptr, &b, nullptr, nullptr);

        LUAUTILS_STACK_SIZE_ASSERT(L, 0)
        
    } else if (strcmp(key, LUA_COLOR_FIELD_A) == 0 || strcmp(key, LUA_COLOR_FIELD_ALPHA) == 0) {
        
        uint8_t a = 255;
        
        if (lua_utils_isIntegerStrict(L, 3)) {
            a = static_cast<uint8_t>(lua_tointeger(L, 3));
        } else if (lua_isnumber(L, 3)) {
            float af = lua_tonumber(L, 3);
            a = _color_attribute_from_float(af);
        } else {
            LUAUTILS_ERROR(L, "incorrect argument type"); // returns
        }
        
        lua_color_setRGBAL(L, 1, nullptr, nullptr, nullptr, &a, nullptr);

        LUAUTILS_STACK_SIZE_ASSERT(L, 0)
        
    } else if (strcmp(key, LUA_COLOR_FIELD_LIGHT) == 0) {
        
        bool light = false;
        
        if (lua_isboolean(L, 3)) {
            light = static_cast<int>(lua_toboolean(L, 3));
        } else {
            LUAUTILS_ERROR(L, "incorrect argument type"); // returns
        }
        
        lua_color_setRGBAL(L, 1, nullptr, nullptr, nullptr, nullptr, &light);
        
        LUAUTILS_STACK_SIZE_ASSERT(L, 0)
        
    } else if (strcmp(key, LUA_COLOR_FIELD_H) == 0 || strcmp(key, LUA_COLOR_FIELD_HUE) == 0) {
        
        double h = 0.0;
        
        if (lua_isnumber(L, 3)) {
            h = lua_tonumber(L, 3);
        } else {
            LUAUTILS_ERROR(L, "incorrect argument type"); // returns
        }
        
        lua_color_setHSV(L, 1, &h, nullptr, nullptr);

        LUAUTILS_STACK_SIZE_ASSERT(L, 0)
        
    } else if (strcmp(key, LUA_COLOR_FIELD_S) == 0 || strcmp(key, LUA_COLOR_FIELD_SATURATION) == 0) {
        
        double s = 0.0;
        
        if (lua_isnumber(L, 3)) {
            s = lua_tonumber(L, 3);
        } else {
            LUAUTILS_ERROR(L, "incorrect argument type"); // returns
        }
        
        lua_color_setHSV(L, 1, nullptr, &s, nullptr);

        LUAUTILS_STACK_SIZE_ASSERT(L, 0)
        
    } else if (strcmp(key, LUA_COLOR_FIELD_V) == 0 || strcmp(key, LUA_COLOR_FIELD_VALUE) == 0) {
        
        double v = 0.0;
        
        if (lua_isnumber(L, 3)) {
            v = lua_tonumber(L, 3);
        } else {
            LUAUTILS_ERROR(L, "incorrect argument type"); // returns
        }
        
        lua_color_setHSV(L, 1, nullptr, nullptr, &v);

        LUAUTILS_STACK_SIZE_ASSERT(L, 0)
        
    } else {
        LUAUTILS_ERROR_F(L, "Color.%s is not settable", key); // returns
    }

    LUAUTILS_STACK_SIZE_ASSERT(L, 0)
    return 0;
}

static int _color_metatable_eq(lua_State *L) {
    vx_assert(L != nullptr);
    LUAUTILS_ASSERT_ARGS_COUNT(L, 2)
    LUAUTILS_STACK_SIZE(L)
    
    if (lua_utils_getObjectType(L, 1) != ITEM_TYPE_COLOR ||
        lua_utils_getObjectType(L, 2) != ITEM_TYPE_COLOR) {
        LUAUTILS_ERROR(L, "Color - impossible comparison"); // returns
    }
    
    uint8_t r1 = 0;
    uint8_t g1 = 0;
    uint8_t b1 = 0;
    uint8_t a1 = 0;
    
    uint8_t r2 = 0;
    uint8_t g2 = 0;
    uint8_t b2 = 0;
    uint8_t a2 = 0;
    
    if (lua_color_getRGBAL(L, 1, &r1, &g1, &b1, &a1, nullptr) == false) {
        LUAUTILS_ERROR(L, "Color - impossible comparison")
        return 0; // silence warnings
    }
    
    if (lua_color_getRGBAL(L, 2, &r2, &g2, &b2, &a2, nullptr) == false) {
        LUAUTILS_ERROR(L, "Color - impossible comparison")
        return 0; // silence warnings
    }
    
    lua_pushboolean(L, (r1 == r2 && g1 == g2 && b1 == b2 && a1 == a2));
    
    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
    return 1;
}

static int _color_metatable_add(lua_State *L) {
    vx_assert(L != nullptr);
    LUAUTILS_STACK_SIZE(L)

    uint8_t r1, g1, b1, a1;
    bool light1 = false;
    if (lua_utils_isIntegerStrict(L, 1)) {
        r1 = g1 = b1 = a1 = static_cast<uint8_t>(lua_tointeger(L, 1));
        light1 = false;
    } else if (lua_isnumber(L, 1)) {
        r1 = g1 = b1 = a1 = _color_attribute_from_float(lua_tonumber(L, 1));
    } else if (lua_color_getRGBAL(L, 1, &r1, &g1, &b1, &a1, &light1) == false) {
        LUAUTILS_ERROR(L, "wrong arithmetic operation")
        return 0; // silence warnings
    }

    uint8_t r2, g2, b2, a2;
    if (lua_utils_isIntegerStrict(L, 2)) {
        r2 = g2 = b2 = a2 = static_cast<uint8_t>(lua_tointeger(L, 2));
    } else if (lua_isnumber(L, 2)) {
        r2 = g2 = b2 = a2 = _color_attribute_from_float(lua_tonumber(L, 2));
    } else if (lua_color_getRGBAL(L, 2, &r2, &g2, &b2, &a2, nullptr) == false) {
        LUAUTILS_ERROR(L, "wrong arithmetic operation")
        return 0; // silence warnings
    }

    const uint8_t r = static_cast<uint8_t>(CLAMP(r1 + r2, 0, 255));
    const uint8_t g = static_cast<uint8_t>(CLAMP(g1 + g2, 0, 255));
    const uint8_t b = static_cast<uint8_t>(CLAMP(b1 + b2, 0, 255));
    const uint8_t a = static_cast<uint8_t>(CLAMP(a1 + a2, 0, 255));

    lua_color_create_and_push_table(L, r, g, b, a, light1, false);

    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
    return 1;
}

static int _color_metatable_sub(lua_State *L) {
    vx_assert(L != nullptr);
    LUAUTILS_STACK_SIZE(L)

    uint8_t r1, g1, b1, a1;
    bool light1 = false;
    if (lua_utils_isIntegerStrict(L, 1)) {
        r1 = g1 = b1 = a1 = static_cast<uint8_t>(lua_tointeger(L, 1));
        light1 = false;
    } else if (lua_isnumber(L, 1)) {
        r1 = g1 = b1 = a1 = _color_attribute_from_float(lua_tonumber(L, 1));
    } else if (lua_color_getRGBAL(L, 1, &r1, &g1, &b1, &a1, &light1) == false) {
        LUAUTILS_ERROR(L, "wrong arithmetic operation")
        return 0; // silence warnings
    }

    uint8_t r2, g2, b2, a2;
    if (lua_utils_isIntegerStrict(L, 2)) {
        r2 = g2 = b2 = a2 = static_cast<uint8_t>(lua_tointeger(L, 2));
    } else if (lua_isnumber(L, 2)) {
        r2 = g2 = b2 = a2 = _color_attribute_from_float(lua_tonumber(L, 2));
    } else if (lua_color_getRGBAL(L, 2, &r2, &g2, &b2, &a2, nullptr) == false) {
        LUAUTILS_ERROR(L, "wrong arithmetic operation")
        return 0; // silence warnings
    }

    const uint8_t r = static_cast<uint8_t>(CLAMP(r1 - r2, 0, 255));
    const uint8_t g = static_cast<uint8_t>(CLAMP(g1 - g2, 0, 255));
    const uint8_t b = static_cast<uint8_t>(CLAMP(b1 - b2, 0, 255));
    const uint8_t a = static_cast<uint8_t>(CLAMP(a1 - a2, 0, 255));

    lua_color_create_and_push_table(L, r, g, b, a, light1, false);

    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
    return 1;
}

static int _color_metatable_mul(lua_State *L) {
    vx_assert(L != nullptr);
    LUAUTILS_STACK_SIZE(L)

    float r1, g1, b1, a1;
    bool light1;
    if (lua_isnumber(L, 1)) {
        r1 = g1 = b1 = a1 = lua_tonumber(L, 1);
        light1 = false;
    } else if (lua_color_getRGBAL_float(L, 1, &r1, &g1, &b1, &a1, &light1) == false) {
        LUAUTILS_ERROR(L, "wrong arithmetic operation")
        return 0; // silence warnings
    }

    float r2, g2, b2, a2;
    if (lua_isnumber(L, 2)) {
        r2 = g2 = b2 = a2 = lua_tonumber(L, 2);
    } else if (lua_color_getRGBAL_float(L, 2, &r2, &g2, &b2, &a2, nullptr) == false) {
        LUAUTILS_ERROR(L, "wrong arithmetic operation")
        return 0; // silence warnings
    }

    lua_color_create_and_push_table(L, r1 * r2, g1 * g2, b1 * b2, a1 * a2, light1, false);

    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
    return 1;
}

static int _color_metatable_div(lua_State *L) {
    vx_assert(L != nullptr);
    LUAUTILS_STACK_SIZE(L)

    float r1, g1, b1, a1;
    bool light1;
    if (lua_isnumber(L, 1)) {
        r1 = g1 = b1 = a1 = lua_tonumber(L, 1);
        light1 = false;
    } else if (lua_color_getRGBAL_float(L, 1, &r1, &g1, &b1, &a1, &light1) == false) {
        LUAUTILS_ERROR(L, "wrong arithmetic operation")
        return 0; // silence warnings
    }

    float r2, g2, b2, a2;
    if (lua_isnumber(L, 2)) {
        r2 = g2 = b2 = a2 = lua_tonumber(L, 2);
    } else if (lua_color_getRGBAL_float(L, 2, &r2, &g2, &b2, &a2, nullptr) == false) {
        LUAUTILS_ERROR(L, "wrong arithmetic operation")
        return 0; // silence warnings
    }

    lua_color_create_and_push_table(L, r1 / r2, g1 / g2, b1 / b2, a1 / a2, light1, false);

    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
    return 1;
}

static int _color_metatable_idiv(lua_State *L) {
    vx_assert(L != nullptr);
    LUAUTILS_STACK_SIZE(L)

    uint8_t r1, g1, b1, a1;
    bool light1;
    if (lua_isnumber(L, 1)) {
        r1 = g1 = b1 = a1 = static_cast<uint8_t>(lua_tointeger(L, 1));
        light1 = false;
    } else if (lua_color_getRGBAL(L, 1, &r1, &g1, &b1, &a1, &light1) == false) {
        LUAUTILS_ERROR(L, "wrong arithmetic operation")
        return 0; // silence warnings
    }

    uint8_t r2, g2, b2, a2;
    if (lua_isnumber(L, 2)) {
        r2 = g2 = b2 = a2 = static_cast<uint8_t>(lua_tointeger(L, 1));
    } else if (lua_color_getRGBAL(L, 2, &r2, &g2, &b2, &a2, nullptr) == false) {
        LUAUTILS_ERROR(L, "wrong arithmetic operation")
        return 0; // silence warnings
    }

    const uint8_t r = r1 / r2;
    const uint8_t g = g1 / g2;
    const uint8_t b = b1 / b2;
    const uint8_t a = a1 / a2;

    lua_color_create_and_push_table(L, r, g, b, a, light1, false);

    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
    return 1;
}

static void _color_metatable_gc(void *_ud) {
    color_userdata *ud = static_cast<color_userdata*>(_ud);
    color_userdata_flush(ud);
}

int lua_g_color_snprintf(lua_State *L,
                         char* result,
                         size_t maxLen,
                         bool spacePrefix) {
     return snprintf(result, maxLen, "%s[Color (global)]", spacePrefix ? " " : "");
}

/// Prints Color instance
int lua_color_snprintf(lua_State *L,
                       char* result,
                       size_t maxLen,
                       bool spacePrefix) {
    
    
    RETURN_VALUE_IF_NULL(L, 0)

    LUAUTILS_STACK_SIZE(L)
    
    uint8_t r = 0;
    uint8_t g = 0;
    uint8_t b = 0;
    uint8_t a = 0;
    bool light = false;
    
    lua_color_getRGBAL(L, -1, &r, &g, &b, &a, &light);
    
    LUAUTILS_STACK_SIZE_ASSERT(L, 0)
    
    return snprintf(result, maxLen, "%s[Color R: %d G: %d B: %d A: %d%s]",
                    spacePrefix ? " " : "",
                    r, g, b, a, light ? " (light)" : "");
}

static uint8_t _color_attribute_from_float(const float attr) {
    return static_cast<uint8_t>(CLAMP(attr * 255.0f, 0.0f, 255.0f));
}

