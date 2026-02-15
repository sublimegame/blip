// -------------------------------------------------------------
//  Cubzh
//  lua_light.cpp
//  Created by Arthur Cormerais on May 11, 2022.
// -------------------------------------------------------------

#include "lua_light.hpp"

#include <string.h>

#include "config.h"
#include "lua_utils.hpp"
#include "lua_constants.h"
#include "lua_object.hpp"
#include "lua_color.hpp"
#include "lua_lightType.hpp"
#include "VXGameRenderer.hpp"
#include "lua_ambient.hpp"

#define LUA_G_LIGHT_FIELD_INTENSITY "Intensity"     // number
#define LUA_G_LIGHT_FIELD_AMBIENT   "Ambient"       // namespace

#if DEBUG_RENDERER
#define LUA_G_LIGHT_FIELD_SHADOWOFFSET "SNO" // number
#define LUA_G_LIGHT_FIELD_SHADOWBIAS "SB" // number
#endif

#define LUA_LIGHT_FIELD_TYPE        "Type"          // enum
#define LUA_LIGHT_FIELD_COLOR       "Color"         // Color
#define LUA_LIGHT_FIELD_RANGE       "Range"         // number
#define LUA_LIGHT_FIELD_RADIUS      "Radius"        // number, alias to Range
#define LUA_LIGHT_FIELD_HARDNESS    "Hardness"      // number
#define LUA_LIGHT_FIELD_ANGLE       "Angle"         // number
#define LUA_LIGHT_FIELD_PRIORITY    "PriorityGroup" // integer (0 to 255)
#define LUA_LIGHT_FIELD_ENABLED     "On"            // boolean
#define LUA_LIGHT_FIELD_LAYERS      "Layers"        // integer or table of integers (0 to 8)
#define LUA_LIGHT_FIELD_SHADOWS     "CastsShadows"  // boolean
#define LUA_LIGHT_FIELD_INTENSITY   "Intensity"     // number

typedef struct light_userdata {
    // Userdata instance requirements
    vx::UserDataStore *store;
    light_userdata *next;
    light_userdata *previous;

    Light *light;
} light_userdata;

// MARK: Metatable

static int _g_light_metatable_index(lua_State *L);
static int _g_light_metatable_newindex(lua_State *L);
static int _g_light_metatable_call(lua_State *L);
static int _light_metatable_index(lua_State *L);
static int _light_metatable_newindex(lua_State *L);
static void _light_gc(void *_ud);

// MARK: Lua fields

void _light_set_color_handler(const uint8_t *r, const uint8_t *g, const uint8_t *b, const uint8_t *a,
                              const bool *light, lua_State *L, void *userdata);
void _light_get_color_handler(uint8_t *r, uint8_t *g, uint8_t *b, uint8_t *a, bool *light, lua_State *L,
                              const void *userdata);
void _light_color_handler_free_userdata(void *userdata);

// MARK: - Light global table -

void lua_g_light_createAndPush(lua_State * const L) {
    vx_assert(L != nullptr);
    LUAUTILS_STACK_SIZE(L)

    lua_newuserdata(L, 1); // global "Light" table
    lua_newtable(L); // metatable
    {
        lua_pushstring(L, "__index");
        lua_pushcfunction(L, _g_light_metatable_index, "");
        lua_rawset(L, -3);

        lua_pushstring(L, "__newindex");
        lua_pushcfunction(L, _g_light_metatable_newindex, "");
        lua_rawset(L, -3);

        lua_pushstring(L, "__call");
        lua_pushcfunction(L, _g_light_metatable_call, "");
        lua_rawset(L, -3);

        // hide the metatable from the Lua sandbox user
        lua_pushstring(L, "__metatable");
        lua_pushboolean(L, false);
        lua_rawset(L, -3);

        lua_pushstring(L, "__type");
        lua_pushstring(L, "LightInterface");
        lua_rawset(L, -3);

        lua_pushstring(L, "__typen");
        lua_pushinteger(L, ITEM_TYPE_G_LIGHT);
        lua_rawset(L, -3);
    }
    lua_setmetatable(L, -2);

    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
}

int lua_g_light_snprintf(lua_State *L, char *result, size_t maxLen, bool spacePrefix) {
    vx_assert(L != nullptr);
    return snprintf(result, maxLen, "%s[Light] (global)", spacePrefix ? " " : "");
}

// MARK: - Light instances tables -

bool lua_light_pushNewInstance(lua_State * const L, Light *l) {
    vx_assert(L != nullptr);
    vx_assert(l != nullptr);

    Transform *t = light_get_transform(l);

    // lua Light takes ownership
    if (transform_retain(t) == false) {
        LUAUTILS_INTERNAL_ERROR(L);
        // return false;
    }

    LUAUTILS_STACK_SIZE(L)

    vx::Game *g = nullptr;
    if (lua_utils_getGameCppWrapperPtr(L, &g) == false) {
        LUAUTILS_INTERNAL_ERROR(L);
    }

    light_userdata *ud = static_cast<light_userdata*>(lua_newuserdatadtor(L, sizeof(light_userdata), _light_gc));
    ud->light = l;

    // connect to userdata store
    ud->store = g->userdataStoreForLights;
    ud->previous = nullptr;
    light_userdata* next = static_cast<light_userdata*>(ud->store->add(ud));
    ud->next = next;
    if (next != nullptr) {
        next->previous = ud;
    }
    
    lua_newtable(L); // metatable
    {
        lua_pushstring(L, "__index");
        lua_pushcfunction(L, _light_metatable_index, "");
        lua_rawset(L, -3);

        lua_pushstring(L, "__newindex");
        lua_pushcfunction(L, _light_metatable_newindex, "");
        lua_rawset(L, -3);

        // hide the metatable from the Lua sandbox user
        lua_pushstring(L, "__metatable");
        lua_pushboolean(L, false);
        lua_rawset(L, -3);

        lua_pushstring(L, "__type");
        lua_pushstring(L, "Light");
        lua_rawset(L, -3);

        lua_pushstring(L, "__typen");
        lua_pushinteger(L, ITEM_TYPE_LIGHT);
        lua_rawset(L, -3);

        _lua_light_push_fields(L, l); // TODO: store in Object registry when accessed
    }
    lua_setmetatable(L, -2);

    if (lua_g_object_addIndexEntry(L, t, -1) == false) {
        vxlog_error("Failed to add Lua Light to Object registry");
        lua_pop(L, 1); // pop Lua Light
        LUAUTILS_STACK_SIZE_ASSERT(L, 0)
        return false;
    }

    scene_register_managed_transform(game_get_scene(g->getCGame()), t);

    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
    return true;
}

bool lua_light_getOrCreateInstance(lua_State * const L, Light *l) {
    if (lua_g_object_getIndexEntry(L, light_get_transform(l))) {
        return true;
    } else {
        return lua_light_pushNewInstance(L, l);
    }
}

bool lua_light_isLight(lua_State * const L, const int idx) {
    vx_assert(L != nullptr);
    return lua_utils_getObjectType(L, idx) == ITEM_TYPE_LIGHT; // currently no lua Light extensions
}

Light *lua_light_getLightPtr(lua_State * const L, const int idx) {
    light_userdata *ud = static_cast<light_userdata*>(lua_touserdata(L, idx));
    return ud->light;
}

int lua_light_snprintf(lua_State *L, char *result, size_t maxLen, bool spacePrefix) {
    vx_assert(L != nullptr);
    Transform *root; lua_object_getTransformPtr(L, -1, &root);
    if (root == nullptr) {
        return snprintf(result, maxLen, "%s[Light (destroyed)]", spacePrefix ? " " : "");
    }
    return snprintf(result, maxLen, "%s[Light %d Name:%s] (instance)", spacePrefix ? " " : "",
                    transform_get_id(root), transform_get_name(root));
}

// MARK: - Exposed internal functions -

void _lua_light_push_fields(lua_State * const L, Light *l) {
    const float3 *color = light_get_color(l);

    lua_pushstring(L, LUA_LIGHT_FIELD_COLOR);
    lua_color_create_and_push_table(L,
                                    static_cast<uint8_t>(255 * color->x),
                                    static_cast<uint8_t>(255 * color->y),
                                    static_cast<uint8_t>(255 * color->z),
                                    static_cast<uint8_t>(255),
                                    false,
                                    false);

    lua_color_setHandlers(L, -1,
                          _light_set_color_handler,
                          _light_get_color_handler,
                          _light_color_handler_free_userdata,
                          transform_get_and_retain_weakptr(light_get_transform(l)));
    lua_rawset(L, -3);
}

bool _lua_light_metatable_index(lua_State * const L, Light *l, const char *key) {
    if (strcmp(key, LUA_LIGHT_FIELD_TYPE) == 0) {
        lua_lightType_pushTable(L, light_get_type(l));
        return true;
    } else if (strcmp(key, LUA_LIGHT_FIELD_COLOR) == 0) {
        LUA_GET_METAFIELD_AND_RETURN_TYPE_PUSHING_NIL_IF_NOT_FOUND(L, 1, LUA_LIGHT_FIELD_COLOR);
        return true;
    } else if (strcmp(key, LUA_LIGHT_FIELD_RANGE) == 0 || strcmp(key, LUA_LIGHT_FIELD_RADIUS) == 0) {
        lua_pushnumber(L, lua_Number(light_get_range(l)));
        return true;
    } else if (strcmp(key, LUA_LIGHT_FIELD_HARDNESS) == 0) {
        lua_pushnumber(L, lua_Number(light_get_hardness(l)));
        return true;
    } else if (strcmp(key, LUA_LIGHT_FIELD_ANGLE) == 0) {
        lua_pushnumber(L, lua_Number(light_get_angle(l)));
        return true;
    } else if (strcmp(key, LUA_LIGHT_FIELD_PRIORITY) == 0) {
        lua_pushinteger(L, light_get_priority(l));
        return true;
    } else if (strcmp(key, LUA_LIGHT_FIELD_ENABLED) == 0) {
        lua_pushboolean(L, light_is_enabled(l));
        return true;
    } else if (strcmp(key, LUA_LIGHT_FIELD_LAYERS) == 0) {
        lua_utils_pushMaskAsTable(L, light_get_layers(l), CAMERA_LAYERS_MASK_API_BITS);
        return true;
    } else if (strcmp(key, LUA_LIGHT_FIELD_SHADOWS) == 0) {
        lua_pushboolean(L, light_is_shadow_caster(l));
        return true;
    } else if (strcmp(key, LUA_LIGHT_FIELD_INTENSITY) == 0) {
        if (light_get_intensity(l) >= 0) {
            lua_pushnumber(L, static_cast<double>(light_get_intensity(l)));
        } else {
            // retrieve Game from Lua state
            vx::Game *g = nullptr;
            if (lua_utils_getGameCppWrapperPtr(L, &g) == false || g == nullptr) {
                LUAUTILS_INTERNAL_ERROR(L)
                // return false;
            }
            vx::GameRendererState_SharedPtr grs = g->getGameRendererState();
            lua_pushnumber(L, static_cast<double>(grs->lightsIntensity));
        }
        return true;
    }
    return false;
}

bool _lua_light_metatable_newindex(lua_State * const L, Light *l, const char *key) {
    if (strcmp(key, LUA_LIGHT_FIELD_TYPE) == 0) {
        if (lua_utils_getObjectType(L, 3) != ITEM_TYPE_LIGHTTYPE) {
            LUAUTILS_ERROR(L, "Light.Type must be a LightType");
        }
        light_set_type(l, lua_lightType_get(L, 3));
        return true;
    } else if (strcmp(key, LUA_LIGHT_FIELD_COLOR) == 0) {
        if (lua_utils_getObjectType(L, 3) != ITEM_TYPE_COLOR) {
            LUAUTILS_ERROR(L, "Light.Color must be a Color");
        }

        LUAUTILS_STACK_SIZE(L)
        {
            if (LUA_GET_METAFIELD_AND_RETURN_TYPE(L, 1, LUA_LIGHT_FIELD_COLOR) == LUA_TNIL) {
                LUAUTILS_INTERNAL_ERROR(L); // returns
            }

            uint8_t cr, cg, cb;
            lua_color_getRGBAL(L, 3, &cr, &cg, &cb, nullptr, nullptr);
            lua_color_setRGBAL(L, -1, &cr, &cg, &cb, nullptr, nullptr);
            lua_pop(L, 1); // pop Color
        }
        LUAUTILS_STACK_SIZE_ASSERT(L, 0)

        return true;
    } else if (strcmp(key, LUA_LIGHT_FIELD_RANGE) == 0 || strcmp(key, LUA_LIGHT_FIELD_RADIUS) == 0) {
        if (lua_isnumber(L, 3) == false) {
            LUAUTILS_ERROR_F(L, "Light.%s must be a number", key);
        }
        light_set_range(l, lua_tonumber(L, 3));
        return true;
    } else if (strcmp(key, LUA_LIGHT_FIELD_HARDNESS) == 0) {
        if (lua_isnumber(L, 3) == false) {
            LUAUTILS_ERROR(L, "Light.Hardness must be a number");
        }
        light_set_hardness(l, lua_tonumber(L, 3));
        return true;
    } else if (strcmp(key, LUA_LIGHT_FIELD_ANGLE) == 0) {
        if (lua_isnumber(L, 3) == false) {
            LUAUTILS_ERROR(L, "Light.Angle must be a number");
        }
        light_set_angle(l, lua_tonumber(L, 3));
        return true;
    } else if (strcmp(key, LUA_LIGHT_FIELD_PRIORITY) == 0) {
        if (lua_utils_isIntegerStrict(L, 3) == false) {
            LUAUTILS_ERROR(L, "Light.PriorityGroup must be an integer (0-255)");
        }
        light_set_priority(l, CLAMP(lua_tointeger(L, 3), 0, 255));
        return true;
    } else if (strcmp(key, LUA_LIGHT_FIELD_ENABLED) == 0) {
        if (lua_isboolean(L, 3) == false) {
            LUAUTILS_ERROR(L, "Light.Enabled must be a boolean");
        }
        light_set_enabled(l, lua_toboolean(L, 3));
        return true;
    } else if (strcmp(key, LUA_LIGHT_FIELD_LAYERS) == 0) {
        uint16_t mask = 0;
        if (lua_utils_getMask(L, 3, &mask, CAMERA_LAYERS_MASK_API_BITS) == false) {
            LUAUTILS_ERROR_F(L, "Light.%s cannot be set (new value should be one or a table of integers between 1 and %d)", key, CAMERA_LAYERS_MASK_API_BITS)
        }
        light_set_layers(l, mask);
        return true;
    } else if (strcmp(key, LUA_LIGHT_FIELD_SHADOWS) == 0) {
        if (lua_isboolean(L, 3) == false) {
            lua_pushliteral(L, "Light.CastsShadows must be a boolean");
            lua_error(L);
        }
        light_set_shadow_caster(l, lua_toboolean(L, 3));
        return true;
    } else if (strcmp(key, LUA_LIGHT_FIELD_INTENSITY) == 0) {
        if (lua_isnumber(L, 3) == false) {
            lua_pushliteral(L, "Light.Intensity must be a number");
            lua_error(L);
        }
        light_set_intensity(l, lua_tonumber(L, 3));
        return true;
    }
    return false; // nothing was set
}

// MARK: - Private functions -

static int _g_light_metatable_index(lua_State *L) {
    vx_assert(L != nullptr);
    LUAUTILS_STACK_SIZE(L)

    // retrieve Game from Lua state
    vx::Game *g = nullptr;
    if (lua_utils_getGameCppWrapperPtr(L, &g) == false || g == nullptr) {
        LUAUTILS_INTERNAL_ERROR(L)
        // return 0;
    }

    vx::GameRendererState_SharedPtr grs = g->getGameRendererState();

    const char *key = lua_tostring(L, 2);
    if (strcmp(key, LUA_G_LIGHT_FIELD_INTENSITY) == 0) {
        lua_pushnumber(L, static_cast<double>(grs->lightsIntensity));
    } else if (strcmp(key, LUA_G_LIGHT_FIELD_AMBIENT) == 0) {
        lua_g_ambient_createAndPush(L);
    } else {
        lua_pushnil(L);
    }

    LUAUTILS_STACK_SIZE_ASSERT(L, 1);
    return 1;
}

static int _g_light_metatable_newindex(lua_State *L) {
    vx_assert(L != nullptr);
    LUAUTILS_STACK_SIZE(L)

    // retrieve Game from Lua state
    vx::Game *g = nullptr;
    if (lua_utils_getGameCppWrapperPtr(L, &g) == false || g == nullptr) {
        LUAUTILS_INTERNAL_ERROR(L)
        // return 0;
    }

    vx::GameRendererState_SharedPtr grs = g->getGameRendererState();

    const char *key = lua_tostring(L, 2);
    if (strcmp(key, LUA_LIGHT_FIELD_INTENSITY) == 0) {
        if (lua_isnumber(L, 3)) {
            grs->lightsIntensity = lua_tonumber(L, 3);
        } else {
            LUAUTILS_ERROR(L, "Light.Intensity should be a number");
        }
    }
#if DEBUG_RENDERER
    else if (strcmp(key, LUA_G_LIGHT_FIELD_SHADOWBIAS) == 0) {
        if (lua_isnumber(L, 3)) {
            vx::GameRenderer::getGameRenderer()->_debug_shadowBias = lua_tonumber(L, 3);
        } else {
            LUAUTILS_ERROR(L, "Light.SB should be a number");
        }
    } else if (strcmp(key, LUA_G_LIGHT_FIELD_SHADOWOFFSET) == 0) {
        if (lua_isnumber(L, 3)) {
            vx::GameRenderer::getGameRenderer()->_debug_shadowOffset = lua_tonumber(L, 3);
        } else {
            LUAUTILS_ERROR(L, "Light.SNO should be a number");
        }
    }
#endif

    LUAUTILS_STACK_SIZE_ASSERT(L, 0);
    return 0;
}

static int _g_light_metatable_call(lua_State *L) {
    vx_assert(L != nullptr);
    LUAUTILS_STACK_SIZE(L)

    Light *l = light_new();
    if (lua_light_pushNewInstance(L, l) == false) {
        lua_pushnil(L); // GC will release light
    }
    light_release(l); // now owned by lua Light

    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
    return 1;
}

static int _light_metatable_index(lua_State * const L) {
    vx_assert(L != nullptr);
    LUAUTILS_ASSERT_ARGS_COUNT(L, 2)
    LUAUTILS_ASSERT_ARG_TYPE(L, 1, ITEM_TYPE_LIGHT)
    LUAUTILS_STACK_SIZE(L)

    const char *key = lua_tostring(L, 2);
    if (_lua_object_metatable_index(L, key) == false) {
        // no field found in Object
        // retrieve underlying Light struct
        Light *l = lua_light_getLightPtr(L, 1);
        if (l == nullptr) {
            lua_pushnil(L);
        } else if (_lua_light_metatable_index(L, l, key) == false) {
            // key is not known and starts with an uppercase letter
            lua_pushnil(L);
        }
    }

    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
    return 1;
}

static int _light_metatable_newindex(lua_State * const L) {
    vx_assert(L != nullptr);
    LUAUTILS_ASSERT_ARGS_COUNT(L, 3)
    LUAUTILS_ASSERT_ARG_TYPE(L, 1, ITEM_TYPE_LIGHT)
    LUAUTILS_STACK_SIZE(L)

    // retrieve underlying Light struct
    Light *l = lua_light_getLightPtr(L, 1);
    if (l == nullptr) {
        lua_pushnil(L);
        LUAUTILS_STACK_SIZE_ASSERT(L, 1)
        return 1;
    }
    Transform *t = light_get_transform(l);

    const char *key = lua_tostring(L, 2);

    if (_lua_object_metatable_newindex(L, t, key) == false) {
        // the key is not from Object
        if (_lua_light_metatable_newindex(L, l, key) == false) {
            // key not found
            LUAUTILS_ERROR_F(L, "Light: %s field is not settable", key);
        }
    }

    LUAUTILS_STACK_SIZE_ASSERT(L, 0)
    return 0;
}

void lua_light_release_transform(void *_ud) {
    light_userdata *ud = static_cast<light_userdata*>(_ud);
    Light *l = ud->light;
    if (l != nullptr) {
        light_release(l);
        ud->light = nullptr;
    }
}

static void _light_gc(void *_ud) {
    light_userdata *ud = static_cast<light_userdata*>(_ud);
    Light *l = ud->light;
    if (l != nullptr) {
        light_release(l);
        ud->light = nullptr;
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

void _light_set_color_handler(const uint8_t *r, const uint8_t *g, const uint8_t *b, const uint8_t *a,
                              const bool *light, lua_State *L, void *userdata) {
    Transform *t = static_cast<Transform *>(weakptr_get(static_cast<Weakptr*>(userdata)));
    if (t == nullptr) {
        LUAUTILS_ERROR(L, "[Light.Color] original Light does not exist anymore"); // returns
    }
    Light *l = static_cast<Light *>(transform_get_ptr(t));

    float3 color = *light_get_color(l);
    if (r != nullptr) { color.x = static_cast<float>(*r) / 255; }
    if (g != nullptr) { color.y = static_cast<float>(*g) / 255; }
    if (b != nullptr) { color.z = static_cast<float>(*b) / 255; }
    light_set_color(l, color.x, color.y, color.z);
}

void _light_get_color_handler(uint8_t *r, uint8_t *g, uint8_t *b, uint8_t *a, bool *light, lua_State *L,
                              const void *userdata) {
    Transform *t = static_cast<Transform *>(weakptr_get(static_cast<const Weakptr*>(userdata)));
    if (t == nullptr) {
        LUAUTILS_ERROR(L, "[Light.Color] original Light does not exist anymore"); // returns
    }
    Light *l = static_cast<Light *>(transform_get_ptr(t));

    const float3 *color = light_get_color(l);
    if (r != nullptr) { *r = static_cast<uint8_t>(255 * color->x); }
    if (g != nullptr) { *g = static_cast<uint8_t>(255 * color->y); }
    if (b != nullptr) { *b = static_cast<uint8_t>(255 * color->z); }

    // unused by light color
    if (a != nullptr) { *a = static_cast<uint8_t>(255); }
    if (light != nullptr) { *light = false; }
}

void _light_color_handler_free_userdata(void *userdata) {
    weakptr_release(static_cast<Weakptr*>(userdata));
}
