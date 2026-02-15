//
//  lua_type.c
//  Cubzh
//
//  Created by Gaetan de Villele on 08/01/2020.
//  Copyright © 2020 Voxowl Inc. All rights reserved.
//

#include "lua_type.hpp"

#include <string.h>

#include "config.h"

#include "lua.hpp"
#include "lua_utils.hpp"

// sandbox
#include "lua_constants.h"

// Type names

// lua types keys

const char *sbs_luaType_type_name_lua_novalue = "no value";

static const char *_type_key_lua_nil = "nil";
const char *sbs_luaType_type_name_lua_nil = "nil";

static const char *_type_key_lua_boolean = "boolean";
const char *sbs_luaType_type_name_lua_boolean = "boolean";

static const char *_type_key_lua_userdata = "userdata";
const char *sbs_luaType_type_name_lua_userdata = "userdata";

static const char *_type_key_lua_number = "number";
const char *sbs_luaType_type_name_lua_number = "number";

static const char *_type_key_lua_integer = "integer";
const char *sbs_luaType_type_name_lua_integer= "integer";

static const char *_type_key_lua_string = "string";
const char *sbs_luaType_type_name_lua_string = "string";

static const char *_type_key_lua_table = "table";
const char *sbs_luaType_type_name_lua_table = "table";

static const char *_type_key_lua_function = "function";
const char *sbs_luaType_type_name_lua_function = "function";

static const char *_type_key_lua_thread = "thread";
const char *sbs_luaType_type_name_lua_thread = "thread";

// Blip types keys

static const char *_type_key_p3s_anchor = "Anchor";
const char *sbs_luaType_type_name_p3s_anchor = "Anchor";

static const char *_type_key_p3s_audioListener = "AudioListener";
const char *sbs_luaType_type_name_p3s_audioListener = "AudioListener";

static const char *_type_key_p3s_audioSource = "AudioSource";
const char *sbs_luaType_type_name_p3s_audioSource = "AudioSource";

static const char *_type_key_p3s_block = "Block";
const char *sbs_luaType_type_name_p3s_block = "Block";

static const char *_type_key_p3s_face = "Face";
const char *sbs_luaType_type_name_p3s_face = "Face";

static const char *_type_key_p3s_blockProperties = "BlockProperties";
const char *sbs_luaType_type_name_p3s_blockProperties = "BlockProperties";

static const char *_type_key_p3s_box = "Box";
const char *sbs_luaType_type_name_p3s_box = "Box";

static const char *_type_key_p3s_button = "Button";
const char *sbs_luaType_type_name_p3s_button = "Button";

static const char *_type_key_p3s_camera = "Camera";
const char *sbs_luaType_type_name_p3s_camera = "Camera";

static const char *_type_key_p3s_clouds = "Clouds";
const char *sbs_luaType_type_name_p3s_clouds = "Clouds";

static const char *_type_key_p3s_color= "Color";
const char *sbs_luaType_type_name_p3s_color= "Color";

static const char *_type_key_p3s_colorPicker_class= "ColorPicker_Class";
const char *sbs_luaType_type_name_p3s_colorPicker_class= "ColorPicker_Class";

static const char *_type_key_p3s_colorPicker= "ColorPicker";
const char *sbs_luaType_type_name_p3s_colorPicker= "ColorPicker";

static const char *_type_key_p3s_event = "Event";
const char *sbs_luaType_type_name_p3s_event = "Event";

static const char *_type_key_p3s_fog = "Fog";
const char *sbs_luaType_type_name_p3s_fog = "Fog";

static const char *_type_key_p3s_impact = "Impact";
const char *sbs_luaType_type_name_p3s_impact = "Impact";

static const char *_type_key_p3s_label = "Label";
const char *sbs_luaType_type_name_p3s_label = "Label";

static const char *_type_key_p3s_map = "Map";
const char *sbs_luaType_type_name_p3s_map = "Map";

static const char *_type_key_p3s_world = "World";
const char *sbs_luaType_type_name_p3s_world = "World";

static const char *_type_key_p3s_mutableShape = "MutableShape";
const char *sbs_luaType_type_name_p3s_mutableShape = "MutableShape";

static const char *_type_key_p3s_number3 = "Number3";
const char *sbs_luaType_type_name_p3s_number3 = "Number3";

static const char *_type_key_p3s_object = "Object";
const char *sbs_luaType_type_name_p3s_object = "Object";

static const char *_type_key_p3s_player = "Player";
const char *sbs_luaType_type_name_p3s_player = "Player";

static const char *_type_key_p3s_shape = "Shape";
const char *sbs_luaType_type_name_p3s_shape = "Shape";

static const char *_type_key_p3s_time = "Time";
const char *sbs_luaType_type_name_p3s_time = "Time";

static const char *_type_key_p3s_timer = "Timer";
const char *sbs_luaType_type_name_p3s_timer = "Timer";

static const char *_type_key_p3s_item = "Item";
const char *sbs_luaType_type_name_p3s_item = "Item";

static const char *_type_key_p3s_items = "Items";
const char *sbs_luaType_type_name_p3s_items = "Items";

static const char *_type_key_p3s_config = "Config";
const char *sbs_luaType_type_name_p3s_config = "Config";

static const char *_type_key_p3s_palette = "Palette";
const char *sbs_luaType_type_name_p3s_palette = "Palette";

static const char *_type_key_p3s_ray = "Ray";
const char *sbs_luaType_type_name_p3s_ray = "Ray";

static const char *_type_key_p3s_collision_groups = "CollisionGroups";
const char *sbs_luaType_type_name_p3s_collision_groups = "CollisionGroups";

static const char *_type_key_p3s_light = "Light";
const char *sbs_luaType_type_name_p3s_light = "Light";

static const char *_type_key_p3s_lightType = "LightType";
const char *sbs_luaType_type_name_p3s_lightType = "LightType";

static const char *_type_key_p3s_data = "Data";
const char *sbs_luaType_type_name_p3s_data = "Data";

static const char *_type_key_p3s_file = "File";
const char *sbs_luaType_type_name_p3s_file = "File";

static const char *_type_key_p3s_projectionMode = "ProjectionMode";
const char *sbs_luaType_type_name_p3s_projectionMode = "ProjectionMode";

static const char *_type_key_p3s_text = "Text";
const char *sbs_luaType_type_name_p3s_text = "Text";

static const char *_type_key_p3s_textType = "TextType";
const char *sbs_luaType_type_name_p3s_textType = "TextType";

const char *sbs_luaType_type_name_p3s_font = "Font";

static const char *_type_key_p3s_number2 = "Number2";
const char *sbs_luaType_type_name_p3s_number2 = "Number2";

static const char *_type_key_p3s_assets = "Assets";
const char *sbs_luaType_type_name_p3s_assets = "Assets";

static const char *_type_key_p3s_assetType = "AssetType";
const char *sbs_luaType_type_name_p3s_assetType = "AssetType";

static const char *_type_key_p3s_physicsMode = "PhysicsMode";
const char *sbs_luaType_type_name_p3s_physicsMode = "PhysicsMode";

static const char *_type_key_p3s_quad = "Quad";
const char *sbs_luaType_type_name_p3s_quad = "Quad";

static const char *_type_key_p3s_sky = "Sky";
const char *sbs_luaType_type_name_p3s_sky = "Sky";

static const char *_type_key_p3s_animation = "Animation";
const char *sbs_luaType_type_name_p3s_animation = "Animation";

static const char *_type_key_p3s_rotation = "Rotation";
const char *sbs_luaType_type_name_p3s_rotation = "Rotation";

static const char *_type_key_p3s_mesh = "Mesh";
const char *sbs_luaType_type_name_p3s_mesh = "Mesh";

// --------------------------------------------------
//
// MARK: - Static functions prototypes -
//
// --------------------------------------------------
static int _type_metatable_index(lua_State *L);

/// This adds the "Type" global table containing the type names for lua types and Blip object types.
void sbs_luaType_setup(lua_State *L) {
    vx_assert(L != nullptr);
    LUAUTILS_STACK_SIZE(L)
    
    // add "Type" table to the lua global environment
    lua_newtable(L);
    lua_newtable(L); // metatable
    {
        lua_pushstring(L, "__index");
        lua_pushcfunction(L, _type_metatable_index, "");
        lua_settable(L, -3);
        
        // hide the metatable from the Lua sandbox user
        lua_pushstring(L, "__metatable");
        lua_pushboolean(L, false);
        lua_settable(L, -3);
        
        // lua_pushstring(L, P3S_LUA_OBJ_METAFIELD_TYPE); // used to log tables
        // lua_pushinteger(L, ITEM_TYPE_G_TYPE);
        // lua_settable(L, -3);
    }
    lua_setmetatable(L, -2);
    
    lua_setglobal(L, P3S_LUA_G_TYPE);
    
    LUAUTILS_STACK_SIZE_ASSERT(L, 0)
}

///
const char *sbs_luaType_typeof(lua_State *L, const int idx) {
    vx_assert(L != nullptr);

    // get lua type and typename of the first argument
    const int luaType = lua_type(L, idx);
    const char *luaTypename = lua_typename(L, luaType);
    
    if (luaType == LUA_TTABLE || luaType == LUA_TUSERDATA) {
        // looking for __type metafield if instance is a table.
        // __type can be an integer or string
        // when an integer, string name is associated using ITEM_TYPE_<name> enum.
        const int type = LUA_GET_METAFIELD_AND_RETURN_TYPE(L, idx, P3S_LUA_OBJ_METAFIELD_TYPE);
        if (type == LUA_TNIL) {
            // table doesn't have a metatable or a __type metafield
            // we return the standard lua type name ("table", here)
            return luaTypename;
        } else {
            if (type == LUA_TNUMBER) {
                // __type value is on the top of the lua stack
                const long long typeIntValue = lua_tointeger(L, -1);
                lua_pop(L, 1);
                switch (typeIntValue) {
                    case ITEM_TYPE_ANCHOR:
                        return sbs_luaType_type_name_p3s_anchor;
                    case ITEM_TYPE_G_AUDIOLISTENER:
                        return sbs_luaType_type_name_p3s_audioListener;
                    case ITEM_TYPE_G_AUDIOSOURCE:
                        return sbs_luaType_type_name_p3s_audioSource;
                    case ITEM_TYPE_BLOCK:
                        return sbs_luaType_type_name_p3s_block;
                    case ITEM_TYPE_FACE:
                        return sbs_luaType_type_name_p3s_face;
                    case ITEM_TYPE_BLOCKPROPERTIES:
                        return sbs_luaType_type_name_p3s_blockProperties;
                    case ITEM_TYPE_BOX:
                        return sbs_luaType_type_name_p3s_box;
                    case ITEM_TYPE_BUTTON:
                        return sbs_luaType_type_name_p3s_button;
                    case ITEM_TYPE_CAMERA:
                        return sbs_luaType_type_name_p3s_camera;
                    case ITEM_TYPE_G_CLOUDS:
                        return sbs_luaType_type_name_p3s_clouds;
                    case ITEM_TYPE_COLOR:
                        return sbs_luaType_type_name_p3s_color;
                    case ITEM_TYPE_G_COLORPICKER:
                        return sbs_luaType_type_name_p3s_colorPicker_class;
                    case ITEM_TYPE_COLORPICKER:
                        return sbs_luaType_type_name_p3s_colorPicker;
                    case ITEM_TYPE_EVENT:
                        return sbs_luaType_type_name_p3s_event;
                    case ITEM_TYPE_G_FOG:
                        return sbs_luaType_type_name_p3s_fog;
                    case ITEM_TYPE_IMPACT:
                        return sbs_luaType_type_name_p3s_impact;
                    case ITEM_TYPE_LABEL:
                        return sbs_luaType_type_name_p3s_label;
                    case ITEM_TYPE_MAP:
                        return sbs_luaType_type_name_p3s_map;
                    case ITEM_TYPE_WORLD:
                        return sbs_luaType_type_name_p3s_world;
                    case ITEM_TYPE_MUTABLESHAPE:
                        return sbs_luaType_type_name_p3s_mutableShape;
                    case ITEM_TYPE_NUMBER3:
                        return sbs_luaType_type_name_p3s_number3;
                    case ITEM_TYPE_OBJECT:
                        return sbs_luaType_type_name_p3s_object;
                    case ITEM_TYPE_PLAYER:
                        return sbs_luaType_type_name_p3s_player;
                    case ITEM_TYPE_SHAPE:
                        return sbs_luaType_type_name_p3s_shape;
                    case ITEM_TYPE_TIMER:
                        return sbs_luaType_type_name_p3s_timer;
                    case ITEM_TYPE_ITEM:
                        return sbs_luaType_type_name_p3s_item;
                    case ITEM_TYPE_ITEMS:
                        return sbs_luaType_type_name_p3s_items;
                    case ITEM_TYPE_CONFIG:
                        return sbs_luaType_type_name_p3s_config;
                    case ITEM_TYPE_PALETTE:
                    case ITEM_TYPE_G_PALETTE:
                        return sbs_luaType_type_name_p3s_palette;
                    case ITEM_TYPE_RAY:
                        return sbs_luaType_type_name_p3s_ray;
                    case ITEM_TYPE_COLLISIONGROUPS:
                        return sbs_luaType_type_name_p3s_collision_groups;
                    case ITEM_TYPE_G_LIGHT:
                    case ITEM_TYPE_LIGHT:
                        return sbs_luaType_type_name_p3s_light;
                    case ITEM_TYPE_G_LIGHTTYPE:
                    case ITEM_TYPE_LIGHTTYPE:
                        return sbs_luaType_type_name_p3s_lightType;
                    case ITEM_TYPE_DATA:
                        return sbs_luaType_type_name_p3s_data;
                    case ITEM_TYPE_G_FILE:
                        return sbs_luaType_type_name_p3s_file;
                    case ITEM_TYPE_G_PROJECTIONMODE:
                    case ITEM_TYPE_PROJECTIONMODE:
                        return sbs_luaType_type_name_p3s_projectionMode;
                    case ITEM_TYPE_G_TEXT:
                    case ITEM_TYPE_TEXT:
                        return sbs_luaType_type_name_p3s_text;
                    case ITEM_TYPE_G_TEXTTYPE:
                    case ITEM_TYPE_TEXTTYPE:
                        return sbs_luaType_type_name_p3s_textType;
                    case ITEM_TYPE_G_NUMBER2:
                    case ITEM_TYPE_NUMBER2:
                        return sbs_luaType_type_name_p3s_number2;
                    case ITEM_TYPE_G_ASSETS:
                        return sbs_luaType_type_name_p3s_assets;
                    case ITEM_TYPE_G_ASSETTYPE:
                    case ITEM_TYPE_ASSETTYPE:
                        return sbs_luaType_type_name_p3s_assetType;
                    case ITEM_TYPE_G_PHYSICSMODE:
                    case ITEM_TYPE_PHYSICSMODE:
                        return sbs_luaType_type_name_p3s_physicsMode;
                    case ITEM_TYPE_G_QUAD:
                    case ITEM_TYPE_QUAD:
                        return sbs_luaType_type_name_p3s_quad;
                    case ITEM_TYPE_G_SKY:
                        return sbs_luaType_type_name_p3s_sky;
                    case ITEM_TYPE_G_ANIMATION:
                    case ITEM_TYPE_ANIMATION:
                        return sbs_luaType_type_name_p3s_animation;
                    case ITEM_TYPE_G_ROTATION:
                    case ITEM_TYPE_ROTATION:
                        return sbs_luaType_type_name_p3s_rotation;
                    case ITEM_TYPE_G_FONT:
                    case ITEM_TYPE_FONT:
                        return sbs_luaType_type_name_p3s_font;
                    case ITEM_TYPE_G_MESH:
                    case ITEM_TYPE_MESH:
                        return sbs_luaType_type_name_p3s_mesh;
                    default:
                        // __type is defined to unrecognized type number
                        // return underlying lua type name
                        return luaTypename;
                }
            } else if (type == LUA_TSTRING) {
                // __type defined as string
                // just return it
                const char* typeName = lua_tostring(L, -1);
                lua_pop(L, 1);
                return typeName;
            }
        }
    } else if (luaType == LUA_TNUMBER && lua_utils_isIntegerStrict(L, idx)) {
        // integers are a subtype for numbers
        return sbs_luaType_type_name_lua_integer;
    }
    
    // lua value is not a table, we return the standard lua type name
    return luaTypename;
}



// --------------------------------------------------
//
// MARK: - Static functions -
//
// --------------------------------------------------

/// _index metamethod of lua "Type" global table
static int _type_metatable_index(lua_State *L) {
    vx_assert(L != nullptr);
    LUAUTILS_STACK_SIZE(L)
    
    // validate arguments count
    if (lua_utils_assertArgsCount(L, 2) == false) {
        vxlog_error("Wrong argument count. Returning nil.");
        lua_pushnil(L);
        LUAUTILS_STACK_SIZE_ASSERT(L, 1)
        return 1;
    }
    
    // validate type of 1st argument
    if (lua_istable(L, 1) == false) {
        vxlog_error("Wrong 1st argument type. Returning nil.");
        lua_pushnil(L);
        LUAUTILS_STACK_SIZE_ASSERT(L, 1)
        return 1;
    }
    
    // validate type of 2nd argument
    if (lua_utils_isStringStrict(L, 2) == false) {
        vxlog_error("Wrong 2nd argument type. Returning nil.");
        lua_pushnil(L);
        LUAUTILS_STACK_SIZE_ASSERT(L, 1)
        return 1;
    }
    
    const char *key = lua_tostring(L, 2);
    
    // lua types
    if (strcmp(key, _type_key_lua_nil) == 0) {
        lua_pushstring(L, sbs_luaType_type_name_lua_nil);

    } else if (strcmp(key, _type_key_lua_boolean) == 0) {
        lua_pushstring(L, sbs_luaType_type_name_lua_boolean);

    } else if (strcmp(key, _type_key_lua_userdata) == 0) {
        lua_pushstring(L, sbs_luaType_type_name_lua_userdata);

    } else if (strcmp(key, _type_key_lua_number) == 0) {
        lua_pushstring(L, sbs_luaType_type_name_lua_number);

    } else if (strcmp(key, _type_key_lua_integer) == 0) {
        lua_pushstring(L, sbs_luaType_type_name_lua_integer);

    } else if (strcmp(key, _type_key_lua_string) == 0) {
        lua_pushstring(L, sbs_luaType_type_name_lua_string);

    } else if (strcmp(key, _type_key_lua_table) == 0) {
        lua_pushstring(L, sbs_luaType_type_name_lua_table);

    } else if (strcmp(key, _type_key_lua_function) == 0) {
        lua_pushstring(L, sbs_luaType_type_name_lua_function);

    } else if (strcmp(key, _type_key_lua_thread) == 0) {
        lua_pushstring(L, sbs_luaType_type_name_lua_thread);


    } else if (strcmp(key, _type_key_p3s_anchor) == 0) {
        lua_pushstring(L, sbs_luaType_type_name_p3s_anchor);
        
    } else if (strcmp(key, _type_key_p3s_audioListener) == 0) {
        lua_pushstring(L, sbs_luaType_type_name_p3s_audioListener);
        
    } else if (strcmp(key, _type_key_p3s_audioSource) == 0) {
        lua_pushstring(L, sbs_luaType_type_name_p3s_audioSource);

    } else if (strcmp(key, _type_key_p3s_block) == 0) {
        lua_pushstring(L, sbs_luaType_type_name_p3s_block);

    } else if (strcmp(key, _type_key_p3s_face) == 0) {
        lua_pushstring(L, sbs_luaType_type_name_p3s_face);

    } else if (strcmp(key, _type_key_p3s_blockProperties) == 0) {
        lua_pushstring(L, sbs_luaType_type_name_p3s_blockProperties);

    } else if (strcmp(key, _type_key_p3s_box) == 0) {
        lua_pushstring(L, sbs_luaType_type_name_p3s_box);

    } else if (strcmp(key, _type_key_p3s_button) == 0) {
        lua_pushstring(L, sbs_luaType_type_name_p3s_button);

    } else if (strcmp(key, _type_key_p3s_camera) == 0) {
        lua_pushstring(L, sbs_luaType_type_name_p3s_camera);

    } else if (strcmp(key, _type_key_p3s_clouds) == 0) {
        lua_pushstring(L, sbs_luaType_type_name_p3s_clouds);

    } else if (strcmp(key, _type_key_p3s_color) == 0) {
        lua_pushstring(L, sbs_luaType_type_name_p3s_color);
        
    } else if (strcmp(key, _type_key_p3s_colorPicker_class) == 0) {
        lua_pushstring(L, sbs_luaType_type_name_p3s_colorPicker_class);
        
    } else if (strcmp(key, _type_key_p3s_colorPicker) == 0) {
        lua_pushstring(L, sbs_luaType_type_name_p3s_colorPicker);

    } else if (strcmp(key, _type_key_p3s_event) == 0) {
        lua_pushstring(L, sbs_luaType_type_name_p3s_event);

    } else if (strcmp(key, _type_key_p3s_fog) == 0) {
        lua_pushstring(L, sbs_luaType_type_name_p3s_fog);

    } else if (strcmp(key, _type_key_p3s_impact) == 0) {
        lua_pushstring(L, sbs_luaType_type_name_p3s_impact);

    } else if (strcmp(key, _type_key_p3s_label) == 0) {
        lua_pushstring(L, sbs_luaType_type_name_p3s_label);

    } else if (strcmp(key, _type_key_p3s_map) == 0) {
        lua_pushstring(L, sbs_luaType_type_name_p3s_map);
    
    } else if (strcmp(key, _type_key_p3s_world) == 0) {
        lua_pushstring(L, sbs_luaType_type_name_p3s_world);

    } else if (strcmp(key, _type_key_p3s_mutableShape) == 0) {
        lua_pushstring(L, sbs_luaType_type_name_p3s_mutableShape);

    } else if (strcmp(key, _type_key_p3s_number3) == 0) {
        lua_pushstring(L, sbs_luaType_type_name_p3s_number3);

    } else if (strcmp(key, _type_key_p3s_object) == 0) {
        lua_pushstring(L, sbs_luaType_type_name_p3s_object);

    } else if (strcmp(key, _type_key_p3s_player) == 0) {
        lua_pushstring(L, sbs_luaType_type_name_p3s_player);

    } else if (strcmp(key, _type_key_p3s_shape) == 0) {
        lua_pushstring(L, sbs_luaType_type_name_p3s_shape);

    } else if (strcmp(key, _type_key_p3s_time) == 0) {
        lua_pushstring(L, sbs_luaType_type_name_p3s_time);

    } else if (strcmp(key, _type_key_p3s_timer) == 0) {
        lua_pushstring(L, sbs_luaType_type_name_p3s_timer);

    } else if (strcmp(key, _type_key_p3s_item) == 0) {
        lua_pushstring(L, sbs_luaType_type_name_p3s_item);

    } else if (strcmp(key, _type_key_p3s_items) == 0) {
        lua_pushstring(L, sbs_luaType_type_name_p3s_items);

    } else if (strcmp(key, _type_key_p3s_config) == 0) {
        lua_pushstring(L, sbs_luaType_type_name_p3s_config);

    } else if (strcmp(key, _type_key_p3s_palette) == 0) {
        lua_pushstring(L, sbs_luaType_type_name_p3s_palette);

    } else if (strcmp(key, _type_key_p3s_ray) == 0) {
        lua_pushstring(L, sbs_luaType_type_name_p3s_ray);

    } else if (strcmp(key, _type_key_p3s_collision_groups) == 0) {
        lua_pushstring(L, sbs_luaType_type_name_p3s_collision_groups);

    } else if (strcmp(key, _type_key_p3s_light) == 0) {
        lua_pushstring(L, sbs_luaType_type_name_p3s_light);

    } else if (strcmp(key, _type_key_p3s_lightType) == 0) {
        lua_pushstring(L, sbs_luaType_type_name_p3s_lightType);

    } else if (strcmp(key, _type_key_p3s_data) == 0) {
        lua_pushstring(L, sbs_luaType_type_name_p3s_data);

    } else if (strcmp(key, _type_key_p3s_file) == 0) {
        lua_pushstring(L, sbs_luaType_type_name_p3s_file);

    } else if (strcmp(key, _type_key_p3s_projectionMode) == 0) {
        lua_pushstring(L, sbs_luaType_type_name_p3s_projectionMode);

    } else if (strcmp(key, _type_key_p3s_text) == 0) {
        lua_pushstring(L, sbs_luaType_type_name_p3s_text);

    } else if (strcmp(key, _type_key_p3s_textType) == 0) {
        lua_pushstring(L, sbs_luaType_type_name_p3s_textType);
        
    } else if (strcmp(key, _type_key_p3s_number2) == 0) {
        lua_pushstring(L, sbs_luaType_type_name_p3s_number2);

    } else if (strcmp(key, _type_key_p3s_assets) == 0) {
        lua_pushstring(L, sbs_luaType_type_name_p3s_assets);

    } else if (strcmp(key, _type_key_p3s_assetType) == 0) {
        lua_pushstring(L, sbs_luaType_type_name_p3s_assetType);

    } else if (strcmp(key, _type_key_p3s_physicsMode) == 0) {
        lua_pushstring(L, sbs_luaType_type_name_p3s_physicsMode);

    } else if (strcmp(key, _type_key_p3s_quad) == 0) {
        lua_pushstring(L, sbs_luaType_type_name_p3s_quad);

    } else if (strcmp(key, _type_key_p3s_sky) == 0) {
        lua_pushstring(L, sbs_luaType_type_name_p3s_sky);

    } else if (strcmp(key, _type_key_p3s_animation) == 0) {
        lua_pushstring(L, sbs_luaType_type_name_p3s_animation);

    } else if (strcmp(key, _type_key_p3s_rotation) == 0) {
        lua_pushstring(L, sbs_luaType_type_name_p3s_rotation);

    } else if (strcmp(key, _type_key_p3s_mesh) == 0) {
        lua_pushstring(L, sbs_luaType_type_name_p3s_mesh);

    } else {
        lua_pushnil(L); // if key is unknown, return nil
    }
    
    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
    return 1;
}
