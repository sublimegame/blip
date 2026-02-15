// -------------------------------------------------------------
//  Cubzh
//  lua_quad.cpp
//  Created by Arthur Cormerais on May 4, 2023.
// -------------------------------------------------------------

#include "lua_quad.hpp"

#include <cstring>

#include "VXConfig.hpp"
#include "VXGame.hpp"
#include "crypto.hpp"

#include "lua_utils.hpp"
#include "lua_constants.h"
#include "lua_object.hpp"
#include "lua_color.hpp"
#include "lua_number2.hpp"
#include "lua_data.hpp"

#include "config.h"
#include "weakptr.h"

#define LUA_QUAD_FIELD_COLOR       "Color"         // Color
#define LUA_QUAD_FIELD_SIZE        "Size"          // Number2
#define LUA_QUAD_FIELD_WIDTH       "Width"         // number
#define LUA_QUAD_FIELD_HEIGHT      "Height"        // number
#define LUA_QUAD_FIELD_ANCHOR      "Anchor"        // Number2
#define LUA_QUAD_FIELD_TILING      "Tiling"        // Number2
#define LUA_QUAD_FIELD_OFFSET      "Offset"        // Number2
#define LUA_QUAD_FIELD_LAYERS      "Layers"        // integer or table of integers (0 to 8)
#define LUA_QUAD_FIELD_DOUBLESIDED "IsDoubleSided" // boolean
#define LUA_QUAD_FIELD_SHADOW      "Shadow"        // boolean
#define LUA_QUAD_FIELD_UNLIT       "IsUnlit"       // boolean
#define LUA_QUAD_FIELD_IMAGE       "Image"         // setter: Data if compatible ; getter: boolean
#define LUA_QUAD_FIELD_MASK        "IsMask"        // boolean
#define LUA_QUAD_FIELD_SORTORDER   "SortOrder"     // integer
#define LUA_QUAD_FIELD_DRAWMODE    "DrawMode"      // table

typedef struct quad_userdata {
    // Userdata instance requirements
    vx::UserDataStore *store;
    quad_userdata *next;
    quad_userdata *previous;

    Quad *quad;
} quad_userdata;

// MARK: Metatable

static int _g_quad_metatable_index(lua_State *L);
static int _g_quad_metatable_newindex(lua_State *L);
static int _g_quad_metatable_call(lua_State *L);
static int _quad_metatable_index(lua_State *L);
static int _quad_metatable_newindex(lua_State *L);
static void _quad_gc(void *_ud);

// MARK: Data registry

static std::unordered_map<uint32_t, Weakptr*> _dataRegistry;
Weakptr *_quad_data_get_weakptr(const void *data, size_t size);

// MARK: Lua fields

bool _quad_get_or_create_field_color(lua_State *L, int idx, const Quad* q);
void _quad_set_color_handler(const uint8_t *r, const uint8_t *g, const uint8_t *b, const uint8_t *a,
                             const bool *light, lua_State *L, void *userdata);
void _quad_get_color_handler(uint8_t *r, uint8_t *g, uint8_t *b, uint8_t *a, bool *light, lua_State *L,
                             const void *userdata);
void _quad_color_handler_free_userdata(void *userdata);
bool _quad_get_or_create_field_anchor(lua_State *L, int idx, const Quad* q);
void _quad_set_anchor_handler(const float *x, const float *y, lua_State *L, const number2_C_handler_userdata *userdata);
void _quad_get_anchor_handler(float *x, float *y, lua_State *L, const number2_C_handler_userdata *userdata);
bool _quad_get_or_create_field_tiling(lua_State *L, int idx, const Quad* q);
void _quad_set_tiling_handler(const float *x, const float *y, lua_State *L, const number2_C_handler_userdata *userdata);
void _quad_get_tiling_handler(float *x, float *y, lua_State *L, const number2_C_handler_userdata *userdata);
bool _quad_get_or_create_field_offset(lua_State *L, int idx, const Quad* q);
void _quad_set_offset_handler(const float *x, const float *y, lua_State *L, const number2_C_handler_userdata *userdata);
void _quad_get_offset_handler(float *x, float *y, lua_State *L, const number2_C_handler_userdata *userdata);

// MARK: - Quad global table -

void lua_g_quad_createAndPush(lua_State * const L) {
    vx_assert(L != nullptr);
    LUAUTILS_STACK_SIZE(L)

    lua_newuserdata(L, 1);
    lua_newtable(L); // metatable
    {
        lua_pushstring(L, "__index");
        lua_pushcfunction(L, _g_quad_metatable_index, "");
        lua_rawset(L, -3);

        lua_pushstring(L, "__newindex");
        lua_pushcfunction(L, _g_quad_metatable_newindex, "");
        lua_rawset(L, -3);

        lua_pushstring(L, "__call");
        lua_pushcfunction(L, _g_quad_metatable_call, "");
        lua_rawset(L, -3);

        lua_pushstring(L, "__metatable");
        lua_pushboolean(L, false);
        lua_rawset(L, -3);

        lua_pushstring(L, "__type");
        lua_pushstring(L, "QuadInterface");
        lua_rawset(L, -3);

        lua_pushstring(L, "__typen");
        lua_pushinteger(L, ITEM_TYPE_G_QUAD);
        lua_rawset(L, -3);
    }
    lua_setmetatable(L, -2);

    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
}

int lua_g_quad_snprintf(lua_State *L, char *result, size_t maxLen, bool spacePrefix) {
    vx_assert(L != nullptr);
    return snprintf(result, maxLen, "%s[Quad] (global)", spacePrefix ? " " : "");
}

// MARK: - Quad instances tables -

bool lua_quad_pushNewInstance(lua_State * const L, Quad *q) {
    vx_assert(L != nullptr);
    vx_assert(q != nullptr);

    Transform *t = quad_get_transform(q);

    // lua Quad takes ownership
    if (transform_retain(t) == false) {
        LUAUTILS_INTERNAL_ERROR(L);
    }

    LUAUTILS_STACK_SIZE(L)

    vx::Game *g = nullptr;
    if (lua_utils_getGameCppWrapperPtr(L, &g) == false) {
        LUAUTILS_INTERNAL_ERROR(L);
    }

    quad_userdata *ud = static_cast<quad_userdata *>(lua_newuserdatadtor(L, sizeof(quad_userdata), _quad_gc));
    ud->quad = q;

    // connect to userdata store
    ud->store = g->userdataStoreForQuads;
    ud->previous = nullptr;
    quad_userdata* next = static_cast<quad_userdata*>(ud->store->add(ud));
    ud->next = next;
    if (next != nullptr) {
        next->previous = ud;
    }

    // TODO: there should be only one shared Quad metatable
    // but it's not possible yet as functions still access it to store field specific to each Quad instance
    lua_newtable(L); // metatable
    {
        lua_pushstring(L, "__index");
        lua_pushcfunction(L, _quad_metatable_index, "");
        lua_rawset(L, -3);

        lua_pushstring(L, "__newindex");
        lua_pushcfunction(L, _quad_metatable_newindex, "");
        lua_rawset(L, -3);

        // hide the metatable from the Lua sandbox user
        lua_pushstring(L, "__metatable");
        lua_pushboolean(L, false);
        lua_rawset(L, -3);

        lua_pushstring(L, "__type");
        lua_pushstring(L, "Quad");
        lua_rawset(L, -3);

        lua_pushstring(L, "__typen");
        lua_pushinteger(L, ITEM_TYPE_QUAD);
        lua_rawset(L, -3);
    }
    lua_setmetatable(L, -2);

    if (lua_g_object_addIndexEntry(L, t, -1) == false) {
        vxlog_error("failed to add Lua Quad to Object registry");
        lua_pop(L, 1); // pop Lua Quad
        LUAUTILS_STACK_SIZE_ASSERT(L, 0)
        return false;
    }

    scene_register_managed_transform(game_get_scene(g->getCGame()), t);

    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
    return true;
}

bool lua_quad_getOrCreateInstance(lua_State * const L, Quad *q) {
    if (lua_g_object_getIndexEntry(L, quad_get_transform(q))) {
        return true;
    } else {
        return lua_quad_pushNewInstance(L, q);
    }
}

bool lua_quad_isQuad(lua_State * const L, const int idx) {
    vx_assert(L != nullptr);

    return lua_utils_getObjectType(L, idx) == ITEM_TYPE_QUAD; // currently no lua Quad extensions
}

Quad *lua_quad_getQuadPtr(lua_State * const L, const int idx) {
    vx_assert(L != nullptr);
    LUAUTILS_STACK_SIZE(L)
    if (lua_utils_getObjectType(L, idx) == ITEM_TYPE_QUAD) {
        quad_userdata *ud = static_cast<quad_userdata *>(lua_touserdata(L, idx));
        return ud->quad;
    }
    LUAUTILS_STACK_SIZE_ASSERT(L, 0)
    return nullptr;
}

int lua_quad_snprintf(lua_State *L, char *result, size_t maxLen, bool spacePrefix) {
    vx_assert(L != nullptr);
    Transform *root; lua_object_getTransformPtr(L, -1, &root);
    if (root == nullptr) {
        return snprintf(result, maxLen, "%s[Quad (destroyed)]", spacePrefix ? " " : "");
    }
    return snprintf(result, maxLen, "%s[Quad %d Name:%s] (instance)", spacePrefix ? " " : "",
                    transform_get_id(root), transform_get_name(root));
}

// MARK: - Exposed internal functions -

bool _lua_quad_metatable_index(lua_State * const L, Quad *q, const char *key) {
    if (strcmp(key, LUA_QUAD_FIELD_COLOR) == 0) {
        if (_quad_get_or_create_field_color(L, 1, q) == false) {
            lua_pushnil(L);
        }
        return true;
    } else if (strcmp(key, LUA_QUAD_FIELD_SIZE) == 0) {
        lua_number2_pushNewObject(L, quad_get_width(q), quad_get_height(q));
        return true;
    } else if (strcmp(key, LUA_QUAD_FIELD_WIDTH) == 0) {
        lua_pushnumber(L, static_cast<double>(quad_get_width(q)));
        return true;
    } else if (strcmp(key, LUA_QUAD_FIELD_HEIGHT) == 0) {
        lua_pushnumber(L, static_cast<double>(quad_get_height(q)));
        return true;
    } else if (strcmp(key, LUA_QUAD_FIELD_ANCHOR) == 0) {
        if (_quad_get_or_create_field_anchor(L, 1, q) == false) {
            lua_pushnil(L);
        }
        return true;
    } else if (strcmp(key, LUA_QUAD_FIELD_TILING) == 0) {
        if (_quad_get_or_create_field_tiling(L, 1, q) == false) {
            lua_pushnil(L);
        }
        return true;
    } else if (strcmp(key, LUA_QUAD_FIELD_OFFSET) == 0) {
        if (_quad_get_or_create_field_offset(L, 1, q) == false) {
            lua_pushnil(L);
        }
        return true;
    } else if (strcmp(key, LUA_QUAD_FIELD_LAYERS) == 0) {
        lua_utils_pushMaskAsTable(L, quad_get_layers(q), CAMERA_LAYERS_MASK_API_BITS);
        return true;
    } else if (strcmp(key, LUA_QUAD_FIELD_DOUBLESIDED) == 0) {
        lua_pushboolean(L, quad_is_doublesided(q));
        return true;
    } else if (strcmp(key, LUA_QUAD_FIELD_SHADOW) == 0) {
        lua_pushboolean(L, quad_has_shadow(q));
        return true;
    } else if (strcmp(key, LUA_QUAD_FIELD_UNLIT) == 0) {
        lua_pushboolean(L, quad_is_unlit(q));
        return true;
    } else if (strcmp(key, LUA_QUAD_FIELD_IMAGE) == 0) {
        lua_pushboolean(L, quad_get_data_size(q) > 0);
        return true;
    } else if (strcmp(key, LUA_QUAD_FIELD_MASK) == 0) {
        lua_pushboolean(L, quad_is_mask(q));
        return true;
    } else if (strcmp(key, LUA_QUAD_FIELD_SORTORDER) == 0) {
        lua_pushinteger(L, quad_get_sort_order(q));
        return true;
    } else if (strcmp(key, LUA_QUAD_FIELD_DRAWMODE) == 0) {
        if (quad_uses_drawmodes(q)) {
            lua_newtable(L);
            {
                const BillboardMode billboard = quad_drawmode_get_billboard(q);

                if (billboard != BillboardMode_None) {
                    lua_pushstring(L, "billboard");
                    switch(billboard) {
                        case BillboardMode_Screen:
                            lua_pushstring(L, "screen");
                            break;
                        case BillboardMode_Horizontal:
                            lua_pushstring(L, "horizontal");
                            break;
                        case BillboardMode_Vertical:
                            lua_pushstring(L, "vertical");
                            break;
                        case BillboardMode_ScreenX:
                            lua_pushstring(L, "screenX");
                            break;
                        case BillboardMode_ScreenY:
                            lua_pushstring(L, "screenY");
                            break;
                        case BillboardMode_ScreenZ:
                            lua_pushstring(L, "screenZ");
                            break;
                    }
                    lua_rawset(L, -3);
                }

                if (quad_drawmode_get_blending(q) != BlendingMode_Default) {
                    lua_pushstring(L, "blending");
                    if (quad_drawmode_get_blending_factor(q) > 1) {
                        lua_newtable(L);

                        lua_pushstring(L, "mode");
                    }
                    switch(quad_drawmode_get_blending(q)) {
                        case BlendingMode_Additive:
                            lua_pushstring(L, "additive");
                            break;
                        case BlendingMode_Multiplicative:
                            lua_pushstring(L, "multiplicative");
                            break;
                        case BlendingMode_Subtractive:
                            lua_pushstring(L, "subtractive");
                            break;
                    }
                    lua_rawset(L, -3);

                    lua_pushstring(L, "factor");
                    lua_pushnumber(L, quad_drawmode_get_blending_factor(q));
                    lua_rawset(L, -3);

                    if (quad_drawmode_get_blending_factor(q) > 1) {
                        lua_rawset(L, -3);
                    }
                }
            }
        } else {
            lua_pushnil(L);
        }
        return true;
    }
    return false;
}

bool _lua_quad_metatable_newindex(lua_State * const L, Quad *q, const char *key) {
    if (strcmp(key, LUA_QUAD_FIELD_COLOR) == 0) {
        if (lua_utils_getObjectType(L, 3) == ITEM_TYPE_COLOR) {
            RGBAColor color;
            lua_color_getRGBAL(L, 3, &color.r, &color.g, &color.b, &color.a, nullptr);
            quad_set_color(q, color_to_uint32(&color));

            return true;
        } else if (lua_istable(L, 3)) {
            // Can be a list of up to 4 colors, or a gradient helper { gradient="H"/"V" or "X"/"Y", from=color1, to=color2 }
            bool useHelper = true;

            bool vertical = true;
            lua_getfield(L, 3, "gradient");
            if (lua_isstring(L, -1)) {
                const char *grad = lua_tostring(L, -1);
                vertical = strcmp(grad, "V") == 0 || strcmp(grad, "Y") == 0;
            } else {
                useHelper = false;
            }
            lua_pop(L, 1);

            RGBAColor from, to;
            lua_getfield(L, 3, "from");
            if (lua_color_getRGBAL(L, -1, &from.r, &from.g, &from.b, &from.a, nullptr) == false) {
                useHelper = false;
            }
            lua_pop(L, 1);
            lua_getfield(L, 3, "to");
            if (lua_color_getRGBAL(L, -1, &to.r, &to.g, &to.b, &to.a, nullptr) == false) {
                useHelper = false;
            }
            lua_pop(L, 1);

            lua_getfield(L, 3, "alpha");
            if (lua_isboolean(L, -1)) {
                quad_set_alpha_blending(q, static_cast<float>(lua_toboolean(L, -1)));
            } else if (lua_isnil(L, -1) == false) {
                LUAUTILS_ERROR_F(L, "Quad.%s table key 'alpha' must be a boolean", key);
            }
            lua_pop(L, 1);

            if (useHelper) {
                if (vertical) {
                    quad_set_vertex_color(q, color_to_uint32(&from), 0);
                    quad_set_vertex_color(q, color_to_uint32(&from), 1);
                    quad_set_vertex_color(q, color_to_uint32(&to), 2);
                    quad_set_vertex_color(q, color_to_uint32(&to), 3);
                } else {
                    quad_set_vertex_color(q, color_to_uint32(&from), 0);
                    quad_set_vertex_color(q, color_to_uint32(&to), 1);
                    quad_set_vertex_color(q, color_to_uint32(&to), 2);
                    quad_set_vertex_color(q, color_to_uint32(&from), 3);
                }
            } else {
                const int count = static_cast<int>(lua_objlen(L, 3));

                RGBAColor color;
                for (uint8_t i = 0; i < count; ++i) {
                    lua_rawgeti(L, 3, i + 1); // note replaced lua_geti (5.3), see if it's important or not to trigger meta functions
                    if (lua_color_getRGBAL(L, -1, &color.r, &color.g, &color.b, &color.a, nullptr)) {
                        quad_set_vertex_color(q, color_to_uint32(&color), i);
                    } else {
                        break;
                    }
                    lua_pop(L, 1);
                }
                for (uint8_t i = count; i < 4; ++i) {
                    quad_set_vertex_color(q, color_to_uint32(&color), i);
                }
            }

            return true;
        } else {
            LUAUTILS_ERROR(L, "Quad.Color must be a Color, or a table of up to four Colors");
        }
    } else if (strcmp(key, LUA_QUAD_FIELD_SIZE) == 0) {
        float w, h;
        if (lua_isnumber(L, 3)) {
            w = lua_tonumber(L, 3);
            h = w;
        } else if (lua_number2_or_table_getXY(L, 3, &w, &h) == false) {
            LUAUTILS_ERROR_F(L, "Quad.%s must be a Number2 or number", key);
        }
        quad_set_width(q, w);
        quad_set_height(q, h);
        return true;
    } else if (strcmp(key, LUA_QUAD_FIELD_WIDTH) == 0) {
        if (lua_isnumber(L, 3) == false) {
            LUAUTILS_ERROR_F(L, "Quad.%s cannot be set (new value (%s) should be a number)", key, lua_typename(L,lua_type(L, 3)));
        }
        quad_set_width(q, lua_tonumber(L, 3));
        return true;
    } else if (strcmp(key, LUA_QUAD_FIELD_HEIGHT) == 0) {
        if (lua_isnumber(L, 3) == false) {
            LUAUTILS_ERROR_F(L, "Quad.%s cannot be set new value (%s) should be a number)", key, lua_typename(L,lua_type(L, 3)));
        }
        quad_set_height(q, lua_tonumber(L, 3));
        return true;
    } else if (strcmp(key, LUA_QUAD_FIELD_ANCHOR) == 0) {
        float x, y;
        if (lua_number2_or_table_getXY(L, 3, &x, &y) == false) {
            LUAUTILS_ERROR_F(L, "Quad.%s must be a Number2", key);
        }
        quad_set_anchor_x(q, x);
        quad_set_anchor_y(q, y);
        return true;
    } else if (strcmp(key, LUA_QUAD_FIELD_TILING) == 0) {
        if (quad_uses_9slice(q)) {
            LUAUTILS_ERROR_F(L, "Quad.%s cannot be used with 9-slice", key);
        }
        float x, y;
        if (lua_number2_or_table_getXY(L, 3, &x, &y) == false) {
            LUAUTILS_ERROR_F(L, "Quad.%s must be a Number2", key);
        }
        quad_set_tiling_u(q, x);
        quad_set_tiling_v(q, y);
        return true;
    } else if (strcmp(key, LUA_QUAD_FIELD_OFFSET) == 0) {
        if (quad_uses_9slice(q)) {
            LUAUTILS_ERROR_F(L, "Quad.%s cannot be used with 9-slice", key);
        }
        float x, y;
        if (lua_number2_or_table_getXY(L, 3, &x, &y) == false) {
            LUAUTILS_ERROR_F(L, "Quad.%s must be a Number2", key);
        }
        quad_set_offset_u(q, x);
        quad_set_offset_v(q, y);
        return true;
    } else if (strcmp(key, LUA_QUAD_FIELD_LAYERS) == 0) {
        uint16_t mask = 0;
        if (lua_utils_getMask(L, 3, &mask, CAMERA_LAYERS_MASK_API_BITS) == false) {
            LUAUTILS_ERROR_F(L, "Quad.%s cannot be set (new value should be one or a table of integers between 1 and %d)", key, CAMERA_LAYERS_MASK_API_BITS)
        }
        quad_set_layers(q, mask);
        return true;
    } else if (strcmp(key, LUA_QUAD_FIELD_DOUBLESIDED) == 0) {
        if (lua_isboolean(L, 3) == false) {
            LUAUTILS_ERROR_F(L, "Quad.%s must be a boolean", key);
        }
        quad_set_doublesided(q, lua_toboolean(L, 3));
        return true;
    } else if (strcmp(key, LUA_QUAD_FIELD_SHADOW) == 0) {
        if (lua_isboolean(L, 3) == false) {
            LUAUTILS_ERROR_F(L, "Quad.%s must be a boolean", key);
        }
        quad_set_shadow(q, lua_toboolean(L, 3));
        return true;
    } else if (strcmp(key, LUA_QUAD_FIELD_UNLIT) == 0) {
        if (lua_isboolean(L, 3) == false) {
            LUAUTILS_ERROR_F(L, "Quad.%s must be a boolean", key);
        }
        quad_set_unlit(q, lua_toboolean(L, 3));
        return true;
    } else if (strcmp(key, LUA_QUAD_FIELD_IMAGE) == 0) {
        if (lua_utils_getObjectType(L, 3) == ITEM_TYPE_DATA) {
            size_t dataSize = 0;
            const void *dataPtr = lua_data_getBuffer(L, 3, &dataSize);
            quad_set_data(q, _quad_data_get_weakptr(dataPtr, dataSize), static_cast<uint32_t>(dataSize));
        } else if (lua_istable(L, 3)) {
            lua_getfield(L, 3, "data");
            if (lua_utils_getObjectType(L, -1) == ITEM_TYPE_DATA) {
                size_t dataSize = 0;
                const void *dataPtr = lua_data_getBuffer(L, -1, &dataSize);
                quad_set_data(q, _quad_data_get_weakptr(dataPtr, dataSize), static_cast<uint32_t>(dataSize));
            } else if (lua_isnil(L, -1) == false) {
                LUAUTILS_ERROR_F(L, "Quad.%s table key 'data' must be an image Data", key);
            }
            lua_pop(L, 1);

            lua_getfield(L, 3, "alpha");
            if (lua_isboolean(L, -1)) {
                quad_set_alpha_blending(q, static_cast<float>(lua_toboolean(L, -1)));
            } else if (lua_isnil(L, -1) == false) {
                LUAUTILS_ERROR_F(L, "Quad.%s table key 'alpha' must be a boolean", key);
            }
            lua_pop(L, 1);

            lua_getfield(L, 3, "filtering");
            if (lua_isboolean(L, -1)) {
                quad_set_filtering(q, lua_toboolean(L, -1));
            } else if (lua_isnil(L, -1) == false) {
                LUAUTILS_ERROR_F(L, "Quad.%s table key 'filtering' must be a boolean", key);
            }
            lua_pop(L, 1);

            lua_getfield(L, 3, "slice9");
            if (lua_isboolean(L, -1)) {
                quad_set_9slice(q, lua_toboolean(L, -1));
            } else {
                float u, v;
                if (lua_number2_or_table_getXY(L, -1, &u, &v)) {
                    quad_set_9slice_uv(q, u, v);
                } else if (lua_isnil(L, -1) == false) {
                    LUAUTILS_ERROR_F(L, "Quad.%s table key 'slice9' must be a boolean or Number2", key);
                }
            }
            lua_pop(L, 1);

            lua_getfield(L, 3, "slice9Scale");
            if (lua_isnumber(L, -1)) {
                quad_set_9slice_scale(q, static_cast<float>(lua_tonumber(L, -1)));
            } else if (lua_isnil(L, -1) == false) {
                LUAUTILS_ERROR_F(L, "Quad.%s table key 'slice9Scale' must be a number", key);
            }
            lua_pop(L, 1);

            lua_getfield(L, 3, "slice9Width");
            if (lua_isnumber(L, -1)) {
                quad_set_9slice_corner_width(q, static_cast<float>(lua_tonumber(L, -1)));
            } else if (lua_isnil(L, -1) == false) {
                LUAUTILS_ERROR_F(L, "Quad.%s table key 'slice9Width' must be a number", key);
            }
            lua_pop(L, 1);

            lua_getfield(L, 3, "cutout");
            if (lua_isboolean(L, -1)) {
                if (lua_toboolean(L, -1)) {
                    quad_set_cutout(q, QUAD_CUTOUT_DEFAULT);
                } else {
                    quad_set_cutout(q, QUAD_CUTOUT_NONE);
                }
            } else if (lua_isnumber(L, -1)) {
                quad_set_cutout(q, static_cast<float>(lua_tonumber(L, -1)));
            } else if (lua_isnil(L, -1) == false) {
                LUAUTILS_ERROR_F(L, "Quad.%s table key 'cutout' must be a boolean or number", key);
            }
            lua_pop(L, 1);
        } else if (lua_isnil(L, 3) || (lua_isboolean(L, 3) && lua_toboolean(L, 3) == false)) {
            quad_set_data(q, nullptr, 0);
        } else {
            LUAUTILS_ERROR_F(L, "Quad.%s must be an image Data, a table { data, params }, or nil/false to remove image", key);
        }
        return true;
    } else if (strcmp(key, LUA_QUAD_FIELD_MASK) == 0) {
        if (lua_isboolean(L, 3) == false) {
            LUAUTILS_ERROR_F(L, "Quad.%s must be a boolean", key);
        }
        quad_set_mask(q, lua_toboolean(L, 3));
        return true;
    } else if (strcmp(key, LUA_QUAD_FIELD_SORTORDER) == 0) {
        if (lua_utils_isIntegerStrict(L, 3) == false) {
            LUAUTILS_ERROR_F(L, "Quad.%s must be an integer", key);
        }
        quad_set_sort_order(q, static_cast<uint8_t>(lua_tointeger(L, 3)));
        return true;
    } else if (strcmp(key, LUA_QUAD_FIELD_DRAWMODE) == 0) {
        if (lua_istable(L, 3)) {
            lua_getfield(L, 3, "billboard");
            if (lua_isboolean(L, -1)) {
                quad_drawmode_set_billboard(q, lua_toboolean(L, -1) ? BillboardMode_Screen : BillboardMode_None);
            } else if (lua_isstring(L, -1)) {
                const char *str = lua_tostring(L, -1);
                if (strcmp(str, "screen") == 0) {
                    quad_drawmode_set_billboard(q, BillboardMode_Screen);
                } else if (strcmp(str, "horizontal") == 0) {
                    quad_drawmode_set_billboard(q, BillboardMode_Horizontal);
                } else if (strcmp(str, "vertical") == 0) {
                    quad_drawmode_set_billboard(q, BillboardMode_Vertical);
                } else if (strcmp(str, "screenX") == 0) {
                    quad_drawmode_set_billboard(q, BillboardMode_ScreenX);
                } else if (strcmp(str, "screenY") == 0) {
                    quad_drawmode_set_billboard(q, BillboardMode_ScreenY);
                } else if (strcmp(str, "screenZ") == 0) {
                    quad_drawmode_set_billboard(q, BillboardMode_ScreenZ);
                } else {
                    LUAUTILS_ERROR_F(L, "Quad.%s 'billboard' must be one of: 'screen', 'horizontal', 'vertical', 'screenX', 'screenY', 'screenZ'", key);
                }
            } else if (lua_isnil(L, -1) == false) {
                LUAUTILS_ERROR_F(L, "Quad.%s 'billboard' must be a boolean or string", key);
            }
            lua_pop(L, 1);

            lua_getfield(L, 3, "blending");
            if (lua_istable(L, -1)) {
                lua_getfield(L, -1, "mode");
                if (lua_isstring(L, -1)) {
                    const char *str = lua_tostring(L, -1);
                    if (strcmp(str, "additive") == 0) {
                        quad_drawmode_set_blending(q, BlendingMode_Additive);
                    } else if (strcmp(str, "multiplicative") == 0) {
                        quad_drawmode_set_blending(q, BlendingMode_Multiplicative);
                    } else if (strcmp(str, "subtractive") == 0) {
                        quad_drawmode_set_blending(q, BlendingMode_Subtractive);
                    } else {
                        LUAUTILS_ERROR_F(L, "Quad.%s 'blending.mode' must be one of: 'additive', 'multiplicative', 'subtractive'", key);
                    }
                } else if (lua_isnil(L, -1) == false) {
                    LUAUTILS_ERROR_F(L, "Quad.%s 'blending.mode' must be a string", key);
                }
                lua_pop(L, 1);

                lua_getfield(L, -1, "factor");
                if (lua_isnumber(L, -1)) {
                    const uint8_t factor = static_cast<uint8_t>(CLAMP(lua_tonumber(L, -1), 0.0f, 255.0f));
                    quad_drawmode_set_blending_factor(q, factor);
                } else if (lua_isnil(L, -1) == false) {
                    LUAUTILS_ERROR_F(L, "Quad.%s 'blending.factor' must be an integer between 0 and 255", key);
                }
                lua_pop(L, 1);
            } else if (lua_isstring(L, -1)) {
                const char *str = lua_tostring(L, -1);
                if (strcmp(str, "additive") == 0) {
                    quad_drawmode_set_blending(q, BlendingMode_Additive);
                } else if (strcmp(str, "multiplicative") == 0) {
                    quad_drawmode_set_blending(q, BlendingMode_Multiplicative);
                } else if (strcmp(str, "subtractive") == 0) {
                    quad_drawmode_set_blending(q, BlendingMode_Subtractive);
                } else {
                    LUAUTILS_ERROR_F(L, "Quad.%s 'blending' must be one of: 'additive', 'multiplicative', 'subtractive'", key);
                }
            } else if (lua_isnil(L, -1) == false) {
                LUAUTILS_ERROR_F(L, "Quad.%s 'blending' must be a string", key);
            }
            lua_pop(L, 1);
        } else if (lua_isnil(L, 3) || lua_isboolean(L, 3) && lua_toboolean(L, 3) == false) {
            quad_toggle_drawmodes(q, false);
        } else {
            LUAUTILS_ERROR_F(L, "Quad.%s must be a table of draw mode parameters, or nil/false to reset parameters", key);
        }
        return true;
    }
    return false; // nothing was set
}

// MARK: - Private functions -

static int _g_quad_metatable_index(lua_State *L) {
    vx_assert(L != nullptr);
    LUAUTILS_STACK_SIZE(L)

    lua_pushnil(L);

    LUAUTILS_STACK_SIZE_ASSERT(L, 1);
    return 1;
}

static int _g_quad_metatable_newindex(lua_State *L) {
    vx_assert(L != nullptr);
    return 0;
}

static int _g_quad_metatable_call(lua_State *L) {
    vx_assert(L != nullptr);
    LUAUTILS_STACK_SIZE(L)

    Quad *q = quad_new();
    if (lua_quad_pushNewInstance(L, q) == false) {
        lua_pushnil(L); // GC will release quad
    }
    quad_release(q); // now owned by lua Quad

    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
    return 1;
}

static int _quad_metatable_index(lua_State * const L) {
    vx_assert(L != nullptr);
    LUAUTILS_ASSERT_ARGS_COUNT(L, 2)
    LUAUTILS_ASSERT_ARG_TYPE(L, 1, ITEM_TYPE_QUAD)
    LUAUTILS_STACK_SIZE(L)

    const char *key = lua_tostring(L, 2);
    if (_lua_object_metatable_index(L, key) == false) {
        // no field found in Object
        // retrieve underlying Quad struct
        Quad *q = lua_quad_getQuadPtr(L, 1);
        if (q == nullptr) {
            lua_pushnil(L);
        } else if (_lua_quad_metatable_index(L, q, key) == false) {
            // key is not known and starts with an uppercase letter
            lua_pushnil(L);
        }
    }

    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
    return 1;
}

static int _quad_metatable_newindex(lua_State * const L) {
    vx_assert(L != nullptr);
    LUAUTILS_ASSERT_ARGS_COUNT(L, 3)
    LUAUTILS_ASSERT_ARG_TYPE(L, 1, ITEM_TYPE_QUAD)
    LUAUTILS_STACK_SIZE(L)

    // retrieve underlying Quad struct
    Quad *q = lua_quad_getQuadPtr(L, 1);
    if (q == nullptr) {
        lua_pushnil(L);
        LUAUTILS_STACK_SIZE_ASSERT(L, 1)
        return 1;
    }
    Transform *t = quad_get_transform(q);

    const char *key = lua_tostring(L, 2);

    if (_lua_object_metatable_newindex(L, t, key) == false) {
        // the key is not from Object
        if (_lua_quad_metatable_newindex(L, q, key) == false) {
            // key not found
            LUAUTILS_ERROR_F(L, "Quad: %s field is not settable", key);
        }
    }

    LUAUTILS_STACK_SIZE_ASSERT(L, 0)
    return 0;
}

void lua_quad_release_transform(void *_ud) {
    quad_userdata *ud = static_cast<quad_userdata*>(_ud);
    Quad *q = ud->quad;
    if (q != nullptr) {
        quad_release(q);
        ud->quad = nullptr;
    }
}

static void _quad_gc(void *_ud) {
    quad_userdata *ud = static_cast<quad_userdata*>(_ud);
    Quad *q = ud->quad;
    if (q != nullptr) {
        quad_release(q);
        ud->quad = nullptr;
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

Weakptr *_quad_data_get_weakptr(const void *data, size_t size) {
    const uint32_t hash = vx::crypto::crc32(0, data, static_cast<uint32_t>(size));

    std::unordered_map<uint32_t, Weakptr*>::iterator itr = _dataRegistry.find(hash);

    if (itr != _dataRegistry.end()) {
        Weakptr *wptr = static_cast<Weakptr*>(itr->second);

        // data in registry, and in use by other quad(s)
        if (weakptr_get(wptr) != nullptr) {
            return wptr;
        }
        // data in registry, but no longer in use
        else {
            weakptr_release(wptr);
        }
    }

    // copy data and wrap it in new weakptr, data will be auto-freed when last quad using it is freed
    void *copy = malloc(size);
    memcpy(copy, data, size);
    _dataRegistry[hash] = weakptr_new_autofree(copy, 1); // 1: _dataRegistry's own reference

    return _dataRegistry[hash];
}

typedef struct {
    Weakptr *t;
    uint8_t vidx;
} _ColorHandlerData;

bool _quad_get_or_create_field_color(lua_State *L, int idx, const Quad* q) {
    const bool vcolors = quad_uses_vertex_colors(q);
    const int type = LUA_GET_METAFIELD_AND_RETURN_TYPE(L, idx, LUA_QUAD_FIELD_COLOR);
    if (type == LUA_TNIL || // no field, create it
        (vcolors && lua_utils_getObjectType(L, -1) == ITEM_TYPE_COLOR)) { // change from single color to table of colors

        if (lua_getmetatable(L, idx) == 0) {
            return false;
        }

        lua_pushstring(L, LUA_QUAD_FIELD_COLOR);
        if (vcolors) {
            lua_newtable(L);
            {
                _ColorHandlerData *userdata;
                for (uint8_t i = 0; i < 4; ++i) {
                    const RGBAColor color = uint32_to_color(quad_get_vertex_color(q, i));

                    userdata = static_cast<_ColorHandlerData*>(malloc(sizeof(_ColorHandlerData)));
                    userdata->t = transform_get_and_retain_weakptr(quad_get_transform(q));
                    userdata->vidx = i;

                    lua_pushinteger(L, i + 1);
                    lua_color_create_and_push_table(L, color.r, color.g, color.b, color.a, false, false);
                    lua_color_setHandlers(L, -1,
                                          _quad_set_color_handler,
                                          _quad_get_color_handler,
                                          _quad_color_handler_free_userdata,
                                          userdata);
                    lua_rawset(L, -3);
                }
            }
        } else {
            const RGBAColor color = uint32_to_color(quad_get_color(q));

            _ColorHandlerData *userdata = static_cast<_ColorHandlerData*>(malloc(sizeof(_ColorHandlerData)));
            userdata->t = transform_get_and_retain_weakptr(quad_get_transform(q));
            userdata->vidx = 4;

            lua_color_create_and_push_table(L, color.r, color.g, color.b, color.a, false, false);
            lua_color_setHandlers(L, -1,
                                  _quad_set_color_handler,
                                  _quad_get_color_handler,
                                  _quad_color_handler_free_userdata,
                                  userdata);
        }
        lua_rawset(L, -3);

        lua_pop(L, 1); // pop metatable

        if (LUA_GET_METAFIELD_AND_RETURN_TYPE(L, idx, LUA_QUAD_FIELD_COLOR) == LUA_TNIL) {
            return false;
        }
    }
    return true;
}

void _quad_set_color_handler(const uint8_t *r, const uint8_t *g, const uint8_t *b, const uint8_t *a,
                             const bool *light, lua_State *L, void *userdata) {
    _ColorHandlerData *data = static_cast<_ColorHandlerData*>(userdata);
    Transform *t = static_cast<Transform *>(weakptr_get(data->t));
    if (t == nullptr) {
        LUAUTILS_ERROR(L, "[Quad.Color] original Quad does not exist anymore"); // returns
    }
    Quad *q = static_cast<Quad *>(transform_get_ptr(t));

    RGBAColor color = uint32_to_color(data->vidx < 4 ? quad_get_vertex_color(q, data->vidx) : quad_get_color(q));
    if (r != nullptr) { color.r = *r; }
    if (g != nullptr) { color.g = *g; }
    if (b != nullptr) { color.b = *b; }
    if (a != nullptr) { color.a = *a; }
    if (data->vidx < 4) {
        quad_set_vertex_color(q, color_to_uint32(&color), data->vidx);
    } else {
        quad_set_color(q, color_to_uint32(&color));
    }
}

void _quad_get_color_handler(uint8_t *r, uint8_t *g, uint8_t *b, uint8_t *a, bool *light, lua_State *L,
                             const void *userdata) {
    const _ColorHandlerData *data = static_cast<const _ColorHandlerData*>(userdata);
    Transform *t = static_cast<Transform *>(weakptr_get(data->t));
    if (t == nullptr) {
        LUAUTILS_ERROR(L, "[Quad.Color] original Quad does not exist anymore"); // returns
    }
    Quad *q = static_cast<Quad *>(transform_get_ptr(t));

    RGBAColor color = uint32_to_color(data->vidx < 4 ? quad_get_vertex_color(q, data->vidx) : quad_get_color(q));
    if (r != nullptr) { *r = color.r; }
    if (g != nullptr) { *g = color.g; }
    if (b != nullptr) { *b = color.b; }
    if (a != nullptr) { *a = color.a; }

    // unused by quad color
    if (light != nullptr) { *light = false; }
}

void _quad_color_handler_free_userdata(void *userdata) {
    _ColorHandlerData *data = static_cast<_ColorHandlerData*>(userdata);
    weakptr_release(data->t);
    free(data);
}

bool _quad_get_or_create_field_anchor(lua_State *L, int idx, const Quad* q) {
    if (LUA_GET_METAFIELD_AND_RETURN_TYPE(L, idx, LUA_QUAD_FIELD_ANCHOR) == LUA_TNIL) {
        if (lua_getmetatable(L, idx) == 0) {
            return false;
        }

        lua_pushstring(L, LUA_QUAD_FIELD_ANCHOR);
        lua_number2_pushNewObject(L, quad_get_anchor_x(q), quad_get_anchor_y(q));
        number2_C_handler_userdata userdata = number2_C_handler_userdata_zero;
        userdata.ptr = transform_get_and_retain_weakptr(quad_get_transform(q));
        lua_number2_setHandlers(L, -1,
                                _quad_set_anchor_handler,
                                _quad_get_anchor_handler,
                                userdata, true);
        lua_rawset(L, -3);

        lua_pop(L, 1); // pop metatable

        if (LUA_GET_METAFIELD_AND_RETURN_TYPE(L, idx, LUA_QUAD_FIELD_ANCHOR) == LUA_TNIL) {
            return false;
        }
    }
    return true;
}

void _quad_set_anchor_handler(const float *x, const float *y, lua_State *L, const number2_C_handler_userdata *userdata) {
    Transform *t = static_cast<Transform *>(weakptr_get(static_cast<const Weakptr*>(userdata->ptr)));
    if (t == nullptr) {
        LUAUTILS_ERROR(L, "[Quad.Anchor] original Quad does not exist anymore"); // returns
    }
    Quad *q = static_cast<Quad *>(transform_get_ptr(t));

    if (x != nullptr) {
        quad_set_anchor_x(q, *x);
    }
    if (y != nullptr) {
        quad_set_anchor_y(q, *y);
    }
}

void _quad_get_anchor_handler(float *x, float *y, lua_State *L, const number2_C_handler_userdata *userdata) {
    Transform *t = static_cast<Transform *>(weakptr_get(static_cast<const Weakptr*>(userdata->ptr)));
    if (t == nullptr) {
        LUAUTILS_ERROR(L, "[Quad.Anchor] original Quad does not exist anymore"); // returns
    }
    Quad *q = static_cast<Quad *>(transform_get_ptr(t));

    if (x != nullptr) {
        *x = quad_get_anchor_x(q);
    }
    if (y != nullptr) {
        *y = quad_get_anchor_y(q);
    }
}

bool _quad_get_or_create_field_tiling(lua_State *L, int idx, const Quad* q) {
    if (LUA_GET_METAFIELD_AND_RETURN_TYPE(L, idx, LUA_QUAD_FIELD_TILING) == LUA_TNIL) {
        if (lua_getmetatable(L, idx) == 0) {
            return false;
        }

        lua_pushstring(L, LUA_QUAD_FIELD_TILING);
        lua_number2_pushNewObject(L, quad_get_tiling_u(q), quad_get_tiling_v(q));
        number2_C_handler_userdata userdata = number2_C_handler_userdata_zero;
        userdata.ptr = transform_get_and_retain_weakptr(quad_get_transform(q));
        lua_number2_setHandlers(L, -1,
                                _quad_set_tiling_handler,
                                _quad_get_tiling_handler,
                                userdata, true);
        lua_rawset(L, -3);

        lua_pop(L, 1); // pop metatable

        if (LUA_GET_METAFIELD_AND_RETURN_TYPE(L, idx, LUA_QUAD_FIELD_TILING) == LUA_TNIL) {
            return false;
        }
    }
    return true;
}

void _quad_set_tiling_handler(const float *x, const float *y, lua_State *L, const number2_C_handler_userdata *userdata) {
    Transform *t = static_cast<Transform *>(weakptr_get(static_cast<const Weakptr*>(userdata->ptr)));
    if (t == nullptr) {
        LUAUTILS_ERROR(L, "[Quad.Tiling] original Quad does not exist anymore"); // returns
    }
    Quad *q = static_cast<Quad *>(transform_get_ptr(t));

    if (quad_uses_9slice(q)) {
        LUAUTILS_ERROR(L, "Quad.Tiling cannot be used with 9-slice");
    }

    if (x != nullptr) {
        quad_set_tiling_u(q, *x);
    }
    if (y != nullptr) {
        quad_set_tiling_v(q, *y);
    }
}

void _quad_get_tiling_handler(float *x, float *y, lua_State *L, const number2_C_handler_userdata *userdata) {
    Transform *t = static_cast<Transform *>(weakptr_get(static_cast<const Weakptr*>(userdata->ptr)));
    if (t == nullptr) {
        LUAUTILS_ERROR(L, "[Quad.Tiling] original Quad does not exist anymore"); // returns
    }
    Quad *q = static_cast<Quad *>(transform_get_ptr(t));

    if (x != nullptr) {
        *x = quad_get_tiling_u(q);
    }
    if (y != nullptr) {
        *y = quad_get_tiling_v(q);
    }
}

bool _quad_get_or_create_field_offset(lua_State *L, int idx, const Quad* q) {
    if (LUA_GET_METAFIELD_AND_RETURN_TYPE(L, idx, LUA_QUAD_FIELD_OFFSET) == LUA_TNIL) {
        if (lua_getmetatable(L, idx) == 0) {
            return false;
        }

        lua_pushstring(L, LUA_QUAD_FIELD_OFFSET);
        lua_number2_pushNewObject(L, quad_get_offset_u(q), quad_get_offset_v(q));
        number2_C_handler_userdata userdata = number2_C_handler_userdata_zero;
        userdata.ptr = transform_get_and_retain_weakptr(quad_get_transform(q));
        lua_number2_setHandlers(L, -1,
                                _quad_set_offset_handler,
                                _quad_get_offset_handler,
                                userdata, true);
        lua_rawset(L, -3);

        lua_pop(L, 1); // pop metatable

        if (LUA_GET_METAFIELD_AND_RETURN_TYPE(L, idx, LUA_QUAD_FIELD_OFFSET) == LUA_TNIL) {
            return false;
        }
    }
    return true;
}

void _quad_set_offset_handler(const float *x, const float *y, lua_State *L, const number2_C_handler_userdata *userdata) {
    Transform *t = static_cast<Transform *>(weakptr_get(static_cast<const Weakptr*>(userdata->ptr)));
    if (t == nullptr) {
        LUAUTILS_ERROR(L, "[Quad.Offset] original Quad does not exist anymore"); // returns
    }
    Quad *q = static_cast<Quad *>(transform_get_ptr(t));

    if (quad_uses_9slice(q)) {
        LUAUTILS_ERROR(L, "Quad.Offset cannot be used with 9-slice");
    }

    if (x != nullptr) {
        quad_set_offset_u(q, *x);
    }
    if (y != nullptr) {
        quad_set_offset_v(q, *y);
    }
}

void _quad_get_offset_handler(float *x, float *y, lua_State *L, const number2_C_handler_userdata *userdata) {
    Transform *t = static_cast<Transform *>(weakptr_get(static_cast<const Weakptr*>(userdata->ptr)));
    if (t == nullptr) {
        LUAUTILS_ERROR(L, "[Quad.Offset] original Quad does not exist anymore"); // returns
    }
    Quad *q = static_cast<Quad *>(transform_get_ptr(t));

    if (x != nullptr) {
        *x = quad_get_offset_u(q);
    }
    if (y != nullptr) {
        *y = quad_get_offset_v(q);
    }
}
