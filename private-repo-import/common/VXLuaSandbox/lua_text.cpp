// -------------------------------------------------------------
//  Cubzh
//  lua_text.cpp
//  Created by Arthur Cormerais on September 27, 2022.
// -------------------------------------------------------------

#include "lua_text.hpp"

#include <cstring>

#include "lua_utils.hpp"
#include "lua_constants.h"
#include "lua_object.hpp"
#include "lua_color.hpp"
#include "lua_textType.hpp"
#include "lua_number2.hpp"
#include "VXNode.hpp"
#include "VXApplication.hpp"
#include "VXGameRenderer.hpp"
#include "lua_font.hpp"

#define LUA_G_TEXT_FIELD_FONTSIZE_DEFAULT  "FontSizeDefault"     // number (deprecated, returns "default")
#define LUA_G_TEXT_FIELD_FONTSIZE_SMALL    "FontSizeSmall"       // number (deprecated, returns "small")
#define LUA_G_TEXT_FIELD_FONTSIZE_BIG      "FontSizeBig"         // number (deprecated, returns "big")
#define LUA_G_TEXT_FIELD_CHARACTERSIZE     "GetCharacterSize"    // function (read-only)

#define LUA_TEXT_FIELD_TEXT        "Text"              // string
#define LUA_TEXT_FIELD_COLOR       "Color"             // Color
#define LUA_TEXT_FIELD_BGCOLOR     "BackgroundColor"   // Color
#define LUA_TEXT_FIELD_ANCHOR      "Anchor"            // Number2
#define LUA_TEXT_FIELD_MAXDISTANCE "MaxDistance"       // number
#define LUA_TEXT_FIELD_TAIL        "Tail"              // boolean
#define LUA_TEXT_FIELD_UNLIT       "IsUnlit"           // boolean
#define LUA_TEXT_FIELD_WIDTH       "Width"             // number (read-only)
#define LUA_TEXT_FIELD_HEIGHT      "Height"            // number (read-only)
#define LUA_TEXT_FIELD_RAWWIDTH    "RawWidth"          // number (read-only)
#define LUA_TEXT_FIELD_RAWHEIGHT   "RawHeight"         // number (read-only)
#define LUA_TEXT_FIELD_MAXWIDTH    "MaxWidth"          // number
#define LUA_TEXT_FIELD_PADDING     "Padding"           // number
#define LUA_TEXT_FIELD_FONTSIZE    "FontSize"          // number
#define LUA_TEXT_FIELD_LAYERS      "Layers"            // integer or table of integers (0 to 8)
#define LUA_TEXT_FIELD_TYPE        "Type"              // enum
#define LUA_TEXT_FIELD_LOCALTOCURSOR    "LocalToCursor" // function (read-only)
#define LUA_TEXT_FIELD_CHARINDEXTOCURSOR    "CharIndexToCursor" // function (read-only)
#define LUA_TEXT_FIELD_FONT        "Font"              // enum
#define LUA_TEXT_FIELD_SORTORDER    "SortOrder"         // integer
#define LUA_TEXT_FIELD_DOUBLESIDED  "IsDoubleSided"     // boolean
#define LUA_TEXT_FIELD_FORMAT       "Format"            // table
#define LUA_TEXT_FIELD_DRAWMODE     "DrawMode"          // table

typedef struct text_userdata {
    // Userdata instance requirements
    vx::UserDataStore *store;
    text_userdata *next;
    text_userdata *previous;

    WorldText *text;
} text_userdata;

// MARK: - Private functions prototypes -

// MARK: Metatable

static int _g_text_metatable_index(lua_State *L);
static int _g_text_metatable_newindex(lua_State *L);
static int _g_text_metatable_call(lua_State *L);
static int _text_metatable_index(lua_State *L);
static int _text_metatable_newindex(lua_State *L);
static void _text_gc(void *_ud);

// MARK: Lua fields

void _text_set_color_handler(const uint8_t *r, const uint8_t *g, const uint8_t *b, const uint8_t *a,
                             const bool *light, lua_State *L, void *userdata);
void _text_get_color_handler(uint8_t *r, uint8_t *g, uint8_t *b, uint8_t *a, bool *light, lua_State *L,
                             const void *userdata);
void _text_color_handler_free_userdata(void *userdata);
void _text_set_background_color_handler(const uint8_t *r, const uint8_t *g, const uint8_t *b, const uint8_t *a,
                                        const bool *light, lua_State *L, void *userdata);
void _text_get_background_color_handler(uint8_t *r, uint8_t *g, uint8_t *b, uint8_t *a, bool *light, lua_State *L,
                                        const void *userdata);
void _text_set_anchor_handler(const float *x, const float *y, lua_State *L, const number2_C_handler_userdata *userdata);
void _text_get_anchor_handler(float *x, float *y, lua_State *L, const number2_C_handler_userdata *userdata);
void _text_refresh_and_get_cached_size(WorldText *wt, float *outWidth, float *outHeight, bool points=true);

// MARK: Lua functions

static int _g_text_getCharacterSize(lua_State *L);
static int _text_localToCursor(lua_State *L);
static int _text_charIndexToCursor(lua_State *L);

// MARK: - Text global table -

void lua_g_text_createAndPush(lua_State * const L) {
    vx_assert(L != nullptr);
    LUAUTILS_STACK_SIZE(L)

    lua_newuserdata(L, 1); // global "Text"
    lua_newtable(L); // metatable
    {
        lua_pushstring(L, "__index");
        lua_pushcfunction(L, _g_text_metatable_index, "");
        lua_rawset(L, -3);

        lua_pushstring(L, "__newindex");
        lua_pushcfunction(L, _g_text_metatable_newindex, "");
        lua_rawset(L, -3);

        lua_pushstring(L, "__call");
        lua_pushcfunction(L, _g_text_metatable_call, "");
        lua_rawset(L, -3);

        // hide the metatable from the Lua sandbox user
        lua_pushstring(L, "__metatable");
        lua_pushboolean(L, false);
        lua_rawset(L, -3);

        lua_pushstring(L, "__type");
        lua_pushstring(L, "TextInterface");
        lua_rawset(L, -3);

        lua_pushstring(L, "__typen");
        lua_pushinteger(L, ITEM_TYPE_G_TEXT);
        lua_rawset(L, -3);
    }
    lua_setmetatable(L, -2);

    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
}

int lua_g_text_snprintf(lua_State *L, char *result, size_t maxLen, bool spacePrefix) {
    vx_assert(L != nullptr);
    return snprintf(result, maxLen, "%s[Text] (global)", spacePrefix ? " " : "");
}

// MARK: - Text instances tables -

bool lua_text_pushNewInstance(lua_State * const L, WorldText *wt) {
    vx_assert(L != nullptr);
    vx_assert(wt != nullptr);

    Transform *t = world_text_get_transform(wt);

    // lua Text takes ownership
    if (transform_retain(t) == false) {
        LUAUTILS_INTERNAL_ERROR(L);
        // return false;
    }

    LUAUTILS_STACK_SIZE(L)

    vx::Game *g = nullptr;
    if (lua_utils_getGameCppWrapperPtr(L, &g) == false) {
        LUAUTILS_INTERNAL_ERROR(L);
    }

    text_userdata *ud = static_cast<text_userdata *>(lua_newuserdatadtor(L, sizeof(text_userdata), _text_gc));
    ud->text = wt;

    // connect to userdata store
    ud->store = g->userdataStoreForTexts;
    ud->previous = nullptr;
    text_userdata* next = static_cast<text_userdata*>(ud->store->add(ud));
    ud->next = next;
    if (next != nullptr) {
        next->previous = ud;
    }

    lua_newtable(L); // metatable
    {
        lua_pushstring(L, "__index");
        lua_pushcfunction(L, _text_metatable_index, "");
        lua_rawset(L, -3);

        lua_pushstring(L, "__newindex");
        lua_pushcfunction(L, _text_metatable_newindex, "");
        lua_rawset(L, -3);

        // hide the metatable from the Lua sandbox user
        lua_pushstring(L, "__metatable");
        lua_pushboolean(L, false);
        lua_rawset(L, -3);

        lua_pushstring(L, "__type");
        lua_pushstring(L, "Text");
        lua_rawset(L, -3);

        lua_pushstring(L, "__typen");
        lua_pushinteger(L, ITEM_TYPE_TEXT);
        lua_rawset(L, -3);

        // private fields
        _lua_text_push_fields(L, wt); // TODO: store in registry / share metatable
    }
    lua_setmetatable(L, -2);

    if (lua_g_object_addIndexEntry(L, t, -1) == false) {
        vxlog_error("Failed to add Lua Text to Object registry");
        lua_pop(L, 1); // pop Lua Text
        LUAUTILS_STACK_SIZE_ASSERT(L, 0)
        return false;
    }

    // manage resources in lua object storage
    scene_register_managed_transform(game_get_scene(g->getCGame()), t);

    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
    return true;
}

bool lua_text_getOrCreateInstance(lua_State * const L, WorldText *wt) {
    if (lua_g_object_getIndexEntry(L, world_text_get_transform(wt))) {
        return true;
    } else {
        return lua_text_pushNewInstance(L, wt);
    }
}

bool lua_text_isText(lua_State * const L, const int idx) {
    vx_assert(L != nullptr);

    return lua_utils_getObjectType(L, idx) == ITEM_TYPE_TEXT; // currently no lua Text extensions
}

WorldText *lua_text_getWorldTextPtr(lua_State * const L, const int idx) {
    text_userdata *ud = static_cast<text_userdata*>(lua_touserdata(L, idx));
    return ud->text;
}

int lua_text_snprintf(lua_State *L, char *result, size_t maxLen, bool spacePrefix) {
    vx_assert(L != nullptr);
    Transform *root; lua_object_getTransformPtr(L, -1, &root);
    if (root == nullptr) {
        return snprintf(result, maxLen, "%s[Text (destroyed)]", spacePrefix ? " " : "");
    }
    return snprintf(result, maxLen, "%s[Text %d Name:%s] (instance)", spacePrefix ? " " : "",
                    transform_get_id(root), transform_get_name(root));
}

// MARK: - Exposed internal functions -

void _lua_text_push_fields(lua_State * const L, WorldText *wt) {
    const RGBAColor color = uint32_to_color(world_text_get_color(wt));
    const RGBAColor bgColor = uint32_to_color(world_text_get_background_color(wt));

    lua_pushstring(L, LUA_TEXT_FIELD_COLOR);
    lua_color_create_and_push_table(L, color.r, color.g, color.b, color.a, false, false);
    lua_color_setHandlers(L, -1,
                          _text_set_color_handler,
                          _text_get_color_handler,
                          _text_color_handler_free_userdata,
                          transform_get_and_retain_weakptr(world_text_get_transform(wt)));
    lua_rawset(L, -3);

    lua_pushstring(L, LUA_TEXT_FIELD_BGCOLOR);
    lua_color_create_and_push_table(L, bgColor.r, bgColor.g, bgColor.b, bgColor.a, false, false);
    lua_color_setHandlers(L, -1,
                          _text_set_background_color_handler,
                          _text_get_background_color_handler,
                          _text_color_handler_free_userdata,
                          transform_get_and_retain_weakptr(world_text_get_transform(wt)));
    lua_rawset(L, -3);

    lua_pushstring(L, LUA_TEXT_FIELD_ANCHOR);
    lua_number2_pushNewObject(L, world_text_get_anchor_x(wt), world_text_get_anchor_y(wt));
    number2_C_handler_userdata userdata = number2_C_handler_userdata_zero;
    userdata.ptr = world_text_get_and_retain_weakptr(wt);
    lua_number2_setHandlers(L, -1,
                            _text_set_anchor_handler,
                            _text_get_anchor_handler,
                            userdata, true);
    lua_rawset(L, -3);
}

bool _lua_text_metatable_index(lua_State * const L, WorldText *wt, const char *key) {
    if (strcmp(key, LUA_TEXT_FIELD_TEXT) == 0) {
        lua_pushstring(L, world_text_get_text(wt));
        return true;
    } else if (strcmp(key, LUA_TEXT_FIELD_COLOR) == 0) {
        LUA_GET_METAFIELD_PUSHING_NIL_IF_NOT_FOUND(L, 1, LUA_TEXT_FIELD_COLOR);
        return true;
    } else if (strcmp(key, LUA_TEXT_FIELD_BGCOLOR) == 0) {
        LUA_GET_METAFIELD_PUSHING_NIL_IF_NOT_FOUND(L, 1, LUA_TEXT_FIELD_BGCOLOR);
        return true;
    } else if (strcmp(key, LUA_TEXT_FIELD_ANCHOR) == 0) {
        LUA_GET_METAFIELD_PUSHING_NIL_IF_NOT_FOUND(L, 1, LUA_TEXT_FIELD_ANCHOR);
        return true;
    } else if (strcmp(key, LUA_TEXT_FIELD_MAXDISTANCE) == 0) {
        lua_pushnumber(L, lua_Number(world_text_get_max_distance(wt)));
        return true;
    } else if (strcmp(key, LUA_TEXT_FIELD_TAIL) == 0) {
        lua_pushboolean(L, world_text_get_tail(wt));
        return true;
    } else if (strcmp(key, LUA_TEXT_FIELD_UNLIT) == 0) {
        lua_pushboolean(L, world_text_is_unlit(wt));
        return true;
    } else if (strcmp(key, LUA_TEXT_FIELD_WIDTH) == 0) {
        float width; _text_refresh_and_get_cached_size(wt, &width, nullptr, true);
        lua_pushnumber(L, lua_Number(width));
        return true;
    } else if (strcmp(key, LUA_TEXT_FIELD_HEIGHT) == 0) {
        float height; _text_refresh_and_get_cached_size(wt, nullptr, &height, true);
        lua_pushnumber(L, lua_Number(height));
        return true;
    } else if (strcmp(key, LUA_TEXT_FIELD_RAWWIDTH) == 0) {
        float width; _text_refresh_and_get_cached_size(wt, &width, nullptr, false);
        lua_pushnumber(L, lua_Number(width));
        return true;
    } else if (strcmp(key, LUA_TEXT_FIELD_RAWHEIGHT) == 0) {
        float height; _text_refresh_and_get_cached_size(wt, nullptr, &height, false);
        lua_pushnumber(L, lua_Number(height));
        return true;
    } else if (strcmp(key, LUA_TEXT_FIELD_MAXWIDTH) == 0) {
        lua_pushnumber(L, lua_Number(world_text_get_max_width(wt)));
        return true;
    } else if (strcmp(key, LUA_TEXT_FIELD_PADDING) == 0) {
        lua_pushnumber(L, lua_Number(world_text_get_padding(wt)));
        return true;
    } else if (strcmp(key, LUA_TEXT_FIELD_FONTSIZE) == 0) {
        lua_pushnumber(L, lua_Number(world_text_get_font_size(wt)));
        return true;
    } else if (strcmp(key, LUA_TEXT_FIELD_LAYERS) == 0) {
        lua_utils_pushMaskAsTable(L, world_text_get_layers(wt), CAMERA_LAYERS_MASK_API_BITS);
        return true;
    } else if (strcmp(key, LUA_TEXT_FIELD_TYPE) == 0) {
        lua_textType_pushTable(L, world_text_get_type(wt));
        return true;
    } else if (strcmp(key, LUA_TEXT_FIELD_LOCALTOCURSOR) == 0) {
        lua_pushcfunction(L, _text_localToCursor, "");
        return true;
    } else if (strcmp(key, LUA_TEXT_FIELD_CHARINDEXTOCURSOR) == 0) {
        lua_pushcfunction(L, _text_charIndexToCursor, "");
        return true;
    } else if (strcmp(key, LUA_TEXT_FIELD_FONT) == 0) {
        lua_font_pushTable(L, static_cast<FontName>(world_text_get_font_index(wt)));
        return true;
    } else if (strcmp(key, LUA_TEXT_FIELD_SORTORDER) == 0) {
        lua_pushinteger(L, world_text_get_sort_order(wt));
        return true;
    } else if (strcmp(key, LUA_TEXT_FIELD_DOUBLESIDED) == 0) {
        lua_pushboolean(L, world_text_is_doublesided(wt));
        return true;
    } else if (strcmp(key, LUA_TEXT_FIELD_FORMAT) == 0) {
        lua_newtable(L);
        {
            lua_pushstring(L, "alignment");
            switch (world_text_get_alignment(wt)) {
                case TextAlignment_Left:
                    lua_pushstring(L, "left");
                    break;
                case TextAlignment_Center:
                    lua_pushstring(L, "center");
                    break;
                case TextAlignment_Right:
                    lua_pushstring(L, "right");
                    break;
            }
            lua_rawset(L, -3);

            lua_pushstring(L, "weight");
            lua_pushnumber(L, static_cast<lua_Number>(world_text_get_weight(wt) * 2.0f)); // exposed as [0.0:2.0]
            lua_rawset(L, -3);

            lua_pushstring(L, "slant");
            lua_pushnumber(L, static_cast<lua_Number>(world_text_get_slant(wt) * 2.0f - 1.0f)); // exposed as [-1.0:1.0]
            lua_rawset(L, -3);
        }
        return true;
    } else if (strcmp(key, LUA_TEXT_FIELD_DRAWMODE) == 0) {
        if (world_text_uses_drawmodes(wt)) {
            lua_newtable(L);
            {
                const float outlineWeight = world_text_drawmode_get_outline_weight(wt);
                if (outlineWeight > EPSILON_ZERO) {
                    lua_pushstring(L, "outline");
                    lua_newtable(L);
                    {
                        const RGBAColor color = uint32_to_color(world_text_drawmode_get_outline_color(wt));

                        lua_pushstring(L, "color");
                        lua_color_create_and_push_table(L, color.r, color.g, color.b, 255, false, true);
                        lua_rawset(L, -3);

                        lua_pushstring(L, "weight");
                        lua_pushnumber(L, static_cast<lua_Number>(outlineWeight));
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

bool _lua_text_metatable_newindex(lua_State * const L, WorldText *wt, const char *key) {
    if (strcmp(key, LUA_TEXT_FIELD_TEXT) == 0) {
        if (lua_isstring(L, 3) == false) {
            LUAUTILS_ERROR_F(L, "Text.%s must be a string", key);
        }
        world_text_set_text(wt, lua_tostring(L, 3));
        return true;
    } else if (strcmp(key, LUA_TEXT_FIELD_COLOR) == 0) {
        if (lua_utils_getObjectType(L, 3) != ITEM_TYPE_COLOR) {
            LUAUTILS_ERROR(L, "Text.Color must be a Color");
        }

        LUAUTILS_STACK_SIZE(L)
        {
            if (LUA_GET_METAFIELD_AND_RETURN_TYPE(L, 1, LUA_TEXT_FIELD_COLOR) == LUA_TNIL) {
                LUAUTILS_INTERNAL_ERROR(L);
            }

            uint8_t cr, cg, cb, ca;
            lua_color_getRGBAL(L, 3, &cr, &cg, &cb, &ca, nullptr);
            lua_color_setRGBAL(L, -1, &cr, &cg, &cb, &ca, nullptr);
            lua_pop(L, 1); // pop Color
        }
        LUAUTILS_STACK_SIZE_ASSERT(L, 0)

        return true;
    } else if (strcmp(key, LUA_TEXT_FIELD_BGCOLOR) == 0) {
        if (lua_utils_getObjectType(L, 3) != ITEM_TYPE_COLOR) {
            LUAUTILS_ERROR(L, "Text.BackgroundColor must be a Color");
        }

        LUAUTILS_STACK_SIZE(L)
        {
            if (LUA_GET_METAFIELD_AND_RETURN_TYPE(L, 1, LUA_TEXT_FIELD_BGCOLOR) == LUA_TNIL) {
                LUAUTILS_INTERNAL_ERROR(L);
            }

            uint8_t cr, cg, cb, ca;
            lua_color_getRGBAL(L, 3, &cr, &cg, &cb, &ca, nullptr);
            lua_color_setRGBAL(L, -1, &cr, &cg, &cb, &ca, nullptr);
            lua_pop(L, 1); // pop Color
        }
        LUAUTILS_STACK_SIZE_ASSERT(L, 0)

        return true;
    } else if (strcmp(key, LUA_TEXT_FIELD_ANCHOR) == 0) {
        float x, y;
        if (lua_number2_or_table_getXY(L, 3, &x, &y) == false) {
            LUAUTILS_ERROR_F(L, "Text.%s must be a Number2", key);
        }
        world_text_set_anchor_x(wt, x);
        world_text_set_anchor_y(wt, y);
        return true;
    } else if (strcmp(key, LUA_TEXT_FIELD_MAXDISTANCE) == 0) {
        if (lua_isnumber(L, 3) == false) {
            LUAUTILS_ERROR_F(L, "Text.%s must be a number", key);
        }
        world_text_set_max_distance(wt, lua_tonumber(L, 3));
        return true;
    } else if (strcmp(key, LUA_TEXT_FIELD_TAIL) == 0) {
        if (lua_isboolean(L, 3) == false) {
            LUAUTILS_ERROR_F(L, "Text.%s must be a boolean", key);
        }
        world_text_set_tail(wt, lua_toboolean(L, 3));
        return true;
    } else if (strcmp(key, LUA_TEXT_FIELD_UNLIT) == 0) {
        if (lua_isboolean(L, 3) == false) {
            LUAUTILS_ERROR_F(L, "Text.%s must be a boolean", key);
        }
        world_text_set_unlit(wt, lua_toboolean(L, 3));
        return true;
    } else if (strcmp(key, LUA_TEXT_FIELD_MAXWIDTH) == 0) {
        if (lua_isnumber(L, 3) == false) {
            LUAUTILS_ERROR_F(L, "Text.%s must be a number", key);
        }
        float o = lua_tonumber(L, 3);

        world_text_set_max_width(wt, o);
        return true;
    } else if (strcmp(key, LUA_TEXT_FIELD_PADDING) == 0) {
        if (lua_isnumber(L, 3) == false) {
            LUAUTILS_ERROR_F(L, "Text.%s must be a number", key);
        }
        world_text_set_padding(wt, lua_tonumber(L, 3));
        return true;
    } else if (strcmp(key, LUA_TEXT_FIELD_FONTSIZE) == 0) {
        if (lua_isnumber(L, 3)) {
            world_text_set_font_size(wt, lua_tonumber(L, 3));
        } else if (lua_isstring(L, 3)) {
            const FontName font = static_cast<FontName>(world_text_get_font_index(wt));
            const char *preset = lua_tostring(L, 3);
            if (strcmp(preset, LUA_TEXT_FONT_SIZE_DEFAULT) == 0) {
                world_text_set_font_size(wt, vx::Font::shared()->getSizeForFontType(font, vx::Font::Type::body));
            } else if (strcmp(preset, LUA_TEXT_FONT_SIZE_BIG) == 0) {
                world_text_set_font_size(wt, vx::Font::shared()->getSizeForFontType(font, vx::Font::Type::title));
            } else if (strcmp(preset, LUA_TEXT_FONT_SIZE_SMALL) == 0) {
                world_text_set_font_size(wt, vx::Font::shared()->getSizeForFontType(font, vx::Font::Type::tiny));
            } else {
                LUAUTILS_ERROR_F(L, "Text.%s must be a number or a preset string (default, big, small)", key);
            }
        } else {
            LUAUTILS_ERROR_F(L, "Text.%s must be a number or a preset string (default, big, small)", key);
        }
        return true;
    } else if (strcmp(key, LUA_TEXT_FIELD_LAYERS) == 0) {
        uint16_t mask = 0;
        if (lua_utils_getMask(L, 3, &mask, CAMERA_LAYERS_MASK_API_BITS) == false) {
            LUAUTILS_ERROR_F(L, "Text.%s cannot be set (new value should be one or a table of integers between 1 and %d)", key, CAMERA_LAYERS_MASK_API_BITS)
        }
        world_text_set_layers(wt, mask);
        return true;
    } else if (strcmp(key, LUA_TEXT_FIELD_TYPE) == 0) {
        if (lua_utils_getObjectType(L, 3) != ITEM_TYPE_TEXTTYPE) {
            LUAUTILS_ERROR_F(L, "Text.%s must be a TextType", key);
        }
        world_text_set_type(wt, lua_textType_get(L, 3));
        return true;
    } else if (strcmp(key, LUA_TEXT_FIELD_FONT) == 0) {
        if (lua_utils_getObjectType(L, 3) != ITEM_TYPE_FONT) {
            LUAUTILS_ERROR_F(L, "Text.%s must be a Font", key);
        }
        world_text_set_font_index(wt, lua_font_get(L, 3));
        return true;
    } else if (strcmp(key, LUA_TEXT_FIELD_SORTORDER) == 0) {
        if (lua_utils_isIntegerStrict(L, 3) == false) {
            LUAUTILS_ERROR_F(L, "Text.%s must be an integer", key);
        }
        world_text_set_sort_order(wt, static_cast<uint8_t>(lua_tointeger(L, 3)));
        return true;
    } else if (strcmp(key, LUA_TEXT_FIELD_DOUBLESIDED) == 0) {
        if (lua_isboolean(L, 3) == false) {
            LUAUTILS_ERROR_F(L, "Text.%s must be a boolean", key);
        }
        world_text_set_doublesided(wt, lua_toboolean(L, 3));
        return true;
    } else if (strcmp(key, LUA_TEXT_FIELD_FORMAT) == 0) {
        if (lua_istable(L, 3)) {
            lua_getfield(L, 3, "alignment");
            if (lua_isstring(L, -1)) {
                const char *alignment = lua_tostring(L, -1);
                if (strcmp(alignment, "center") == 0) {
                    world_text_set_alignment(wt, TextAlignment_Center);
                } else if (strcmp(alignment, "right") == 0) {
                    world_text_set_alignment(wt, TextAlignment_Right);
                } else if (strcmp(alignment, "left") == 0) {
                    world_text_set_alignment(wt, TextAlignment_Left);
                } else {
                    LUAUTILS_ERROR_F(L, "Text.%s 'alignment' must be string 'left' (default), 'center', or 'right'", key);
                }
            } else if (lua_isnil(L, -1) == false) {
                LUAUTILS_ERROR_F(L, "Text.%s 'alignment' must be string 'left' (default), 'center', or 'right'", key);
            }
            lua_pop(L, 1);

            lua_getfield(L, 3, "weight");
            if (lua_isnumber(L, -1)) {
                const float luaWeight = static_cast<float>(lua_tonumber(L, -1));
                world_text_set_weight(wt, luaWeight * 0.5f);
            } else if (lua_isstring(L, -1)) {
                const char *preset = lua_tostring(L, -1);
                if (strcmp(preset, "bold") == 0) {
                    world_text_set_weight(wt, WORLDTEXT_DEFAULT_FONT_WEIGHT_BOLD);
                } else if (strcmp(preset, "light") == 0) {
                    world_text_set_weight(wt, WORLDTEXT_DEFAULT_FONT_WEIGHT_LIGHT);
                } else {
                    world_text_set_weight(wt, WORLDTEXT_DEFAULT_FONT_WEIGHT_REGULAR);
                }
            } else if (lua_isnil(L, -1) == false) {
                LUAUTILS_ERROR_F(L, "Text.%s 'weight' must be a number between 0.0 and 2.0 (default 1.0), or string 'regular' (default), 'bold', 'light'", key);
            }
            lua_pop(L, 1);

            lua_getfield(L, 3, "slant");
            if (lua_isnumber(L, -1)) {
                const float luaSlant = static_cast<float>(lua_tonumber(L, -1));
                world_text_set_slant(wt, (luaSlant + 1.0f) * 0.5f);
            } else if (lua_isstring(L, -1)) {
                const char *preset = lua_tostring(L, -1);
                if (strcmp(preset, "italic") == 0) {
                    world_text_set_slant(wt, WORLDTEXT_DEFAULT_FONT_SLANT_ITALIC);
                } else {
                    world_text_set_slant(wt, WORLDTEXT_DEFAULT_FONT_SLANT_REGULAR);
                }
            } else if (lua_isnil(L, -1) == false) {
                LUAUTILS_ERROR_F(L, "Text.%s 'slant' must be a number between -1.0 and 1.0 (default 0.0) or string 'regular' (default), 'italic'", key);
            }
            lua_pop(L, 1);
        } else if (lua_isnil(L, 3)) {
            world_text_set_alignment(wt, TextAlignment_Left);
            world_text_set_weight(wt, WORLDTEXT_DEFAULT_FONT_WEIGHT_REGULAR);
            world_text_set_slant(wt, WORLDTEXT_DEFAULT_FONT_SLANT_REGULAR);
        } else {
            LUAUTILS_ERROR_F(L, "Text.%s must be a table of format parameters, or nil to reset parameters", key);
        }
        return true;
    } else if (strcmp(key, LUA_TEXT_FIELD_DRAWMODE) == 0) {
        if (lua_istable(L, 3)) {
            lua_getfield(L, 3, "outline");
            if (lua_istable(L, -1)) {
                lua_getfield(L, -1, "color");
                RGBAColor color;
                if (lua_color_getRGBAL(L, -1, &color.r, &color.g, &color.b, nullptr, nullptr)) {
                    world_text_drawmode_set_outline_color(wt, color_to_uint32(&color));
                } else if (lua_isnil(L, -1) == false) {
                    LUAUTILS_ERROR_F(L, "Text.%s 'outline.color' must be a Color", key);
                }
                lua_pop(L, 1);

                lua_getfield(L, -1, "weight");
                if (lua_isnumber(L, -1)) {
                    world_text_drawmode_set_outline_weight(wt, static_cast<float>(lua_tonumber(L, -1)));
                } else if (lua_isnil(L, -1) == false) {
                    LUAUTILS_ERROR_F(L, "Text.%s 'outline.weight' must be a number between 0.0 and 1.0, or a negative value to disable", key);
                }
                lua_pop(L, 1);
            } else if (lua_isnil(L, -1) == false) {
                LUAUTILS_ERROR_F(L, "Text.%s 'outline' must be a table of any or all { color, weight }", key);
            }
            lua_pop(L, 1);
        } else if (lua_isnil(L, 3) || lua_isboolean(L, 3) && lua_toboolean(L, 3) == false) {
            world_text_toggle_drawmodes(wt, false);
        } else {
            LUAUTILS_ERROR_F(L, "Text.%s must be a table of draw mode parameters, or nil/false to reset parameters", key);
        }
        return true;
    }
    return false;
}

// MARK: - Private functions -

static int _g_text_metatable_index(lua_State *L) {
    vx_assert(L != nullptr);
    LUAUTILS_ASSERT_ARGS_COUNT(L, 2)
    LUAUTILS_ASSERT_ARG_TYPE(L, 1, ITEM_TYPE_G_TEXT)
    LUAUTILS_STACK_SIZE(L)

    const char *key = lua_tostring(L, 2);
    if (strcmp(key, LUA_G_TEXT_FIELD_FONTSIZE_DEFAULT) == 0) {
        lua_pushstring(L, LUA_TEXT_FONT_SIZE_DEFAULT);
    } else if (strcmp(key, LUA_G_TEXT_FIELD_FONTSIZE_SMALL) == 0) {
        lua_pushstring(L, LUA_TEXT_FONT_SIZE_SMALL);
    } else if (strcmp(key, LUA_G_TEXT_FIELD_FONTSIZE_BIG) == 0) {
        lua_pushstring(L, LUA_TEXT_FONT_SIZE_BIG);
    } else if (strcmp(key, LUA_G_TEXT_FIELD_CHARACTERSIZE) == 0) {
        lua_pushcfunction(L, _g_text_getCharacterSize, "");
    } else {
        lua_pushnil(L);
    }

    LUAUTILS_STACK_SIZE_ASSERT(L, 1);
    return 1;
}

static int _g_text_metatable_newindex(lua_State *L) {
    vx_assert(L != nullptr);
    LUAUTILS_ASSERT_ARGS_COUNT(L, 3)
    LUAUTILS_ASSERT_ARG_TYPE(L, 1, ITEM_TYPE_G_TEXT)
    LUAUTILS_STACK_SIZE(L)


    LUAUTILS_STACK_SIZE_ASSERT(L, 0);
    return 0;
}

static int _g_text_metatable_call(lua_State *L) {
    vx_assert(L != nullptr);
    LUAUTILS_STACK_SIZE(L)

    WorldText *wt = world_text_new();
    if (lua_text_pushNewInstance(L, wt) == false) {
        lua_pushnil(L); // GC will release world text
    }
    world_text_release(wt); // now owned by lua Text

    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
    return 1;
}

static int _text_metatable_index(lua_State *L) {
    vx_assert(L != nullptr);
    LUAUTILS_ASSERT_ARGS_COUNT(L, 2)
    LUAUTILS_ASSERT_ARG_TYPE(L, 1, ITEM_TYPE_TEXT)
    LUAUTILS_STACK_SIZE(L)

    // retrieve underlying WorldText struct
    WorldText *wt = lua_text_getWorldTextPtr(L, 1);
    if (wt == nullptr) {
        lua_pushnil(L);
        LUAUTILS_STACK_SIZE_ASSERT(L, 1)
        return 1;
    }

    const char *key = lua_tostring(L, 2);
    if (_lua_object_metatable_index(L, key)) {
        // a field from Object was pushed onto the stack
    } else if (_lua_text_metatable_index(L, wt, key) == false) {
        // nothing found at that key
        lua_pushnil(L);
    }

    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
    return 1;
}

static int _text_metatable_newindex(lua_State *L) {
    vx_assert(L != nullptr);
    LUAUTILS_ASSERT_ARGS_COUNT(L, 3)
    LUAUTILS_ASSERT_ARG_TYPE(L, 1, ITEM_TYPE_TEXT)
    LUAUTILS_STACK_SIZE(L)

    // retrieve underlying WorldText struct
    WorldText *wt = lua_text_getWorldTextPtr(L, 1);
    if (wt == nullptr) {
        lua_pushnil(L);
        LUAUTILS_STACK_SIZE_ASSERT(L, 1)
        return 1;
    }
    Transform *t = world_text_get_transform(wt);

    const char *key = lua_tostring(L, 2);

    if (_lua_object_metatable_newindex(L, t, key) == false) {
        // the key is not from Object
        if (_lua_text_metatable_newindex(L, wt, key) == false) {
            // key not found
            LUAUTILS_ERROR_F(L, "Text: %s field is not settable", key);
        }
    }

    LUAUTILS_STACK_SIZE_ASSERT(L, 0)
    return 0;
}

void lua_text_release_transform(void *_ud) {
    text_userdata *ud = static_cast<text_userdata*>(_ud);
    WorldText *wt = ud->text;
    if (wt != nullptr) {
        world_text_release(wt);
        ud->text = nullptr;
    }
}

static void _text_gc(void *_ud) {
    text_userdata *ud = static_cast<text_userdata*>(_ud);
    WorldText *wt = ud->text;
    if (wt != nullptr) {
        world_text_release(wt);
        ud->text = nullptr;
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

void _text_set_color_handler(const uint8_t *r, const uint8_t *g, const uint8_t *b, const uint8_t *a,
                             const bool *light, lua_State *L, void *userdata) {
    Transform *t = static_cast<Transform*>(weakptr_get(static_cast<Weakptr*>(userdata)));
    if (t == nullptr) {
        LUAUTILS_ERROR(L, "[Text.Color] original Text does not exist anymore");
    }
    WorldText *wt = static_cast<WorldText*>(transform_get_ptr(t));

    RGBAColor color = uint32_to_color(world_text_get_color(wt));
    if (r != nullptr) { color.r = *r; }
    if (g != nullptr) { color.g = *g; }
    if (b != nullptr) { color.b = *b; }
    if (a != nullptr) { color.a = *a; }
    world_text_set_color(wt, color_to_uint32(&color));
}

void _text_get_color_handler(uint8_t *r, uint8_t *g, uint8_t *b, uint8_t *a, bool *light, lua_State *L,
                             const void *userdata) {
    Transform *t = static_cast<Transform*>(weakptr_get(static_cast<const Weakptr*>(userdata)));
    if (t == nullptr) {
        LUAUTILS_ERROR(L, "[Text.Color] original Text does not exist anymore");
    }
    WorldText *wt = static_cast<WorldText*>(transform_get_ptr(t));

    RGBAColor color = uint32_to_color(world_text_get_color(wt));
    if (r != nullptr) { *r = color.r; }
    if (g != nullptr) { *g = color.g; }
    if (b != nullptr) { *b = color.b; }
    if (a != nullptr) { *a = color.a; }

    // unused by world text color
    if (light != nullptr) { *light = false; }
}

void _text_color_handler_free_userdata(void *userdata) {
    weakptr_release(static_cast<Weakptr*>(userdata));
}

void _text_set_background_color_handler(const uint8_t *r, const uint8_t *g, const uint8_t *b, const uint8_t *a,
                                        const bool *light, lua_State *L, void *userdata) {
    Transform *t = static_cast<Transform*>(weakptr_get(static_cast<Weakptr*>(userdata)));
    if (t == nullptr) {
        LUAUTILS_ERROR(L, "[Text.BackgroundColor] original Text does not exist anymore");
    }
    WorldText *wt = static_cast<WorldText*>(transform_get_ptr(t));

    RGBAColor color = uint32_to_color(world_text_get_background_color(wt));
    if (r != nullptr) { color.r = *r; }
    if (g != nullptr) { color.g = *g; }
    if (b != nullptr) { color.b = *b; }
    if (a != nullptr) { color.a = *a; }
    world_text_set_background_color(wt, color_to_uint32(&color));
}

void _text_get_background_color_handler(uint8_t *r, uint8_t *g, uint8_t *b, uint8_t *a, bool *light, lua_State *L,
                                        const void *userdata) {
    Transform *t = static_cast<Transform*>(weakptr_get(static_cast<const Weakptr*>(userdata)));
    if (t == nullptr) {
        LUAUTILS_ERROR(L, "[Text.BackgroundColor] original Text does not exist anymore");
    }
    WorldText *wt = static_cast<WorldText*>(transform_get_ptr(t));

    RGBAColor color = uint32_to_color(world_text_get_background_color(wt));
    if (r != nullptr) { *r = color.r; }
    if (g != nullptr) { *g = color.g; }
    if (b != nullptr) { *b = color.b; }
    if (a != nullptr) { *a = color.a; }

    // unused by world text color
    if (light != nullptr) { *light = false; }
}

void _text_set_anchor_handler(const float *x, const float *y, lua_State *L, const number2_C_handler_userdata *userdata) {
    WorldText *wt = static_cast<WorldText*>(weakptr_get(static_cast<Weakptr*>(userdata->ptr)));
    if (wt == nullptr) return;

    if (x != nullptr) {
        world_text_set_anchor_x(wt, *x);
    }
    if (y != nullptr) {
        world_text_set_anchor_y(wt, *y);
    }
}

void _text_get_anchor_handler(float *x, float *y, lua_State *L, const number2_C_handler_userdata *userdata) {
    WorldText *wt = static_cast<WorldText*>(weakptr_get(static_cast<Weakptr*>(userdata->ptr)));
    if (wt == nullptr) return;

    if (x != nullptr) {
        *x = world_text_get_anchor_x(wt);
    }
    if (y != nullptr) {
        *y = world_text_get_anchor_y(wt);
    }
}

void _text_refresh_and_get_cached_size(WorldText *wt, float *outWidth, float *outHeight, bool points) {
#if !defined(P3S_CLIENT_HEADLESS)
    vx::GameRenderer *renderer = vx::GameRenderer::getGameRenderer();
    float width, height;
    renderer->getOrComputeWorldTextSize(wt, &width, &height, points);

    if (outWidth != nullptr) {
        *outWidth = width;
    }
    if (outHeight != nullptr) {
        *outHeight = height;
    }
#else
    if (outWidth != nullptr) {
        *outWidth = 0.0f;
    }
    if (outHeight != nullptr) {
        *outHeight = 0.0f;
    }
#endif
}

/// size = Text:GetCharacterSize(font, character, [points=true])
static int _g_text_getCharacterSize(lua_State *L) {
    RETURN_VALUE_IF_NULL(L, 0)
    LUAUTILS_STACK_SIZE(L)

    if (lua_utils_getObjectType(L, 2) != ITEM_TYPE_FONT) {
        LUAUTILS_ERROR(L, "[Text:GetCharacterSize] first argument must be a Font");
    }
    const FontName font = lua_font_get(L, 3);

    if (lua_isstring(L, 3) == false) {
        LUAUTILS_ERROR(L, "[Text:GetCharacterSize] second argument must be a string");
    }
    const char *arg = lua_tostring(L, 3);

    bool points = true;
    if (lua_isboolean(L, 3)) {
        points = lua_toboolean(L, 3);
    }

#ifndef P3S_CLIENT_HEADLESS
    vx::GameRenderer *renderer = vx::GameRenderer::getGameRenderer();
    lua_pushnumber(L, static_cast<lua_Number>(renderer->getCharacterSize(font, arg, points)));
#else
    lua_pushnumber(L, 0.0f);
#endif

    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
    return 1;
}

/// cursorLocalPos, charIndex = text:LocalToCursor(localPos, [points=true])
static int _text_localToCursor(lua_State *L) {
    RETURN_VALUE_IF_NULL(L, 0)
    LUAUTILS_STACK_SIZE(L)

    WorldText *wt = lua_text_getWorldTextPtr(L, 1);
    if (wt == nullptr) {
        LUAUTILS_ERROR(L, "[Text:LocalToCursor] function should be called with `:`");
    }

    float x, y;
    if (lua_number2_or_table_getXY(L, 2, &x, &y) == false) {
        LUAUTILS_ERROR(L, "[Text:LocalToCursor] argument must be a Number2");
    }

    bool points = true;
    if (lua_isboolean(L, 3)) {
        points = lua_toboolean(L, 3);
    }

    _text_refresh_and_get_cached_size(wt, nullptr, nullptr, points);

    size_t idx = 0;
#ifndef P3S_CLIENT_HEADLESS
    vx::GameRenderer *renderer = vx::GameRenderer::getGameRenderer();
    idx = renderer->snapWorldTextCursor(wt, &x, &y, nullptr, points);
#endif

    lua_number2_pushNewObject(L, x, y);
    lua_pushinteger(L, static_cast<lua_Integer>(idx + 1));

    LUAUTILS_STACK_SIZE_ASSERT(L, 2)
    return 2;
}

/// cursorLocalPos, charIndex = text:CharIndexToCursor(charIndex, [points=true])
static int _text_charIndexToCursor(lua_State *L) {
    RETURN_VALUE_IF_NULL(L, 0)
    LUAUTILS_STACK_SIZE(L)

    WorldText *wt = lua_text_getWorldTextPtr(L, 1);
    if (wt == nullptr) {
        LUAUTILS_ERROR(L, "[Text:CharIndexToCursor] function should be called with `:`");
    }

    if (lua_utils_isIntegerStrict(L, 2) == false) {
        LUAUTILS_ERROR(L, "[Text:CharIndexToCursor] argument must be an integer");
    }
    size_t charIndex = lua_tointeger(L, 2);
    if (charIndex < 1) { charIndex = 1; }
    charIndex = charIndex - 1; // Lua -> C indexes

    bool points = true;
    if (lua_isboolean(L, 3)) {
        points = lua_toboolean(L, 3);
    }

    _text_refresh_and_get_cached_size(wt, nullptr, nullptr, points);

    size_t idx = 0;
    float x = 0.0f;
    float y = 0.0f;
#ifndef P3S_CLIENT_HEADLESS
    vx::GameRenderer *renderer = vx::GameRenderer::getGameRenderer();
    idx = renderer->snapWorldTextCursor(wt, &x, &y, &charIndex, points);

#endif

    lua_number2_pushNewObject(L, x, y);
    lua_pushinteger(L, static_cast<lua_Integer>(idx + 1));

    LUAUTILS_STACK_SIZE_ASSERT(L, 2)
    return 2;
}
