//
//  lua_type.h
//  Cubzh
//
//  Created by Gaetan de Villele on 08/01/2020.
//  Copyright © 2020 Voxowl Inc. All rights reserved.
//

#pragma once

// forward declarations
typedef struct lua_State lua_State;

// Type names

// lua types

extern const char *sbs_luaType_type_name_lua_novalue;
extern const char *sbs_luaType_type_name_lua_nil;
extern const char *sbs_luaType_type_name_lua_boolean;
extern const char *sbs_luaType_type_name_lua_userdata;
extern const char *sbs_luaType_type_name_lua_number;
extern const char *sbs_luaType_type_name_lua_integer;
extern const char *sbs_luaType_type_name_lua_string;
extern const char *sbs_luaType_type_name_lua_table;
extern const char *sbs_luaType_type_name_lua_function;
extern const char *sbs_luaType_type_name_lua_thread;

// Blip types

extern const char *sbs_luaType_type_name_p3s_anchor;
extern const char *sbs_luaType_type_name_p3s_audioListener;
extern const char *sbs_luaType_type_name_p3s_audioSource;
extern const char *sbs_luaType_type_name_p3s_block;
extern const char *sbs_luaType_type_name_p3s_face;
extern const char *sbs_luaType_type_name_p3s_blockProperties;
extern const char *sbs_luaType_type_name_p3s_box;
extern const char *sbs_luaType_type_name_p3s_button;
extern const char *sbs_luaType_type_name_p3s_camera;
extern const char *sbs_luaType_type_name_p3s_clouds;
extern const char *sbs_luaType_type_name_p3s_color;
extern const char *sbs_luaType_type_name_p3s_colorPicker_class;
extern const char *sbs_luaType_type_name_p3s_colorPicker;
extern const char *sbs_luaType_type_name_p3s_event;
extern const char *sbs_luaType_type_name_p3s_fog;
extern const char *sbs_luaType_type_name_p3s_impact;
extern const char *sbs_luaType_type_name_p3s_label;
extern const char *sbs_luaType_type_name_p3s_map;
extern const char *sbs_luaType_type_name_p3s_world;
extern const char *sbs_luaType_type_name_p3s_mutableShape;
extern const char *sbs_luaType_type_name_p3s_number3;
extern const char *sbs_luaType_type_name_p3s_object;
extern const char *sbs_luaType_type_name_p3s_player;
extern const char *sbs_luaType_type_name_p3s_shape;
extern const char *sbs_luaType_type_name_p3s_time;
extern const char *sbs_luaType_type_name_p3s_timer;
extern const char *sbs_luaType_type_name_p3s_item;
extern const char *sbs_luaType_type_name_p3s_items;
extern const char *sbs_luaType_type_name_p3s_config;
extern const char *sbs_luaType_type_name_p3s_palette;
extern const char *sbs_luaType_type_name_p3s_ray;
extern const char *sbs_luaType_type_name_p3s_collision_groups;
extern const char *sbs_luaType_type_name_p3s_light;
extern const char *sbs_luaType_type_name_p3s_lightType;
extern const char *sbs_luaType_type_name_p3s_data;
extern const char *sbs_luaType_type_name_p3s_file;
extern const char *sbs_luaType_type_name_p3s_projectionMode;
extern const char *sbs_luaType_type_name_p3s_text;
extern const char *sbs_luaType_type_name_p3s_textType;
extern const char *sbs_luaType_type_name_p3s_number2;
extern const char *sbs_luaType_type_name_p3s_assets;
extern const char *sbs_luaType_type_name_p3s_assetType;


///
void sbs_luaType_setup(lua_State *L);

///
const char *sbs_luaType_typeof(lua_State *L, const int idx);
