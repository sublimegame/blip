//
//  sbs_print.h
//  sbs
//
//  Created by Adrien Duermael on 8/19/19.
//  Copyright © 2019 Voxowl Inc. All rights reserved.
//

#pragma once

#include <cstddef>

// forward declarations
typedef struct lua_State lua_State;

typedef int (*LuaSnPrintfFuncPtr)(lua_State *L, char* result, size_t maxLen, bool spacePrefix);
typedef void (*LuaPrintOutputFuncPtr)(lua_State *L, const char *logStr);

void sbs_print_set_block_print(LuaSnPrintfFuncPtr printFunc);
void sbs_print_set_block_face_print(LuaSnPrintfFuncPtr printFunc);
void sbs_print_set_block_properties(LuaSnPrintfFuncPtr printFunc);
void sbs_print_set_box(LuaSnPrintfFuncPtr printFunc, LuaSnPrintfFuncPtr instance);
void sbs_print_set_map_print(LuaSnPrintfFuncPtr printFunc);
void sbs_print_set_world_print(LuaSnPrintfFuncPtr printFunc);
void sbs_print_set_color_print(LuaSnPrintfFuncPtr printFunc);
void sbs_print_set_g_camera_print(LuaSnPrintfFuncPtr printFunc);
void sbs_print_set_g_number3_print(LuaSnPrintfFuncPtr printFunc);
void sbs_print_set_g_point_print(LuaSnPrintfFuncPtr printFunc);
void sbs_print_set_player_print(LuaSnPrintfFuncPtr printFunc);
void sbs_print_set_number3_print(LuaSnPrintfFuncPtr printFunc);
void sbs_print_set_point_print(LuaSnPrintfFuncPtr printFunc);
void sbs_print_set_server_print(LuaSnPrintfFuncPtr printFunc);
void sbs_print_set_time_cycle_print(LuaSnPrintfFuncPtr printFunc);
void sbs_print_set_time_cycle_colors_print(LuaSnPrintfFuncPtr printFunc);
void sbs_print_set_time_g_cycle_mark_print(LuaSnPrintfFuncPtr printFunc);
void sbs_print_set_time_cycle_mark_print(LuaSnPrintfFuncPtr printFunc);
void sbs_print_set_g_clouds_print(LuaSnPrintfFuncPtr printFunc);
void sbs_print_set_g_fog_print(LuaSnPrintfFuncPtr printFunc);
void sbs_print_set_g_sky_print(LuaSnPrintfFuncPtr printFunc);
void sbs_print_set_g_http_print(LuaSnPrintfFuncPtr printFunc);
void sbs_print_set_g_json_print(LuaSnPrintfFuncPtr printFunc);
void sbs_print_set_g_url_print(LuaSnPrintfFuncPtr printFunc);
void sbs_print_set_mutableShape_print(LuaSnPrintfFuncPtr global, LuaSnPrintfFuncPtr instance);
void sbs_print_set_shape_print(LuaSnPrintfFuncPtr global, LuaSnPrintfFuncPtr instance);
void sbs_print_set_object_print(LuaSnPrintfFuncPtr global, LuaSnPrintfFuncPtr instance);
void sbs_print_set_palette_print(LuaSnPrintfFuncPtr global, LuaSnPrintfFuncPtr instance);
void sbs_print_set_ray_print(LuaSnPrintfFuncPtr global, LuaSnPrintfFuncPtr instance);
void sbs_print_set_impact_print(LuaSnPrintfFuncPtr instance);
void sbs_print_set_collision_groups_print(LuaSnPrintfFuncPtr global, LuaSnPrintfFuncPtr instance);
void sbs_print_set_rkvstore_print(LuaSnPrintfFuncPtr global, LuaSnPrintfFuncPtr instance);
void sbs_print_set_event_print(LuaSnPrintfFuncPtr global, LuaSnPrintfFuncPtr instance);
void sbs_print_set_light_print(LuaSnPrintfFuncPtr global, LuaSnPrintfFuncPtr instance);
void sbs_print_set_lightType_print(LuaSnPrintfFuncPtr instance);
void sbs_print_set_file_print(LuaSnPrintfFuncPtr global);
void sbs_print_set_projectionMode_print(LuaSnPrintfFuncPtr instance);
void sbs_print_set_text_print(LuaSnPrintfFuncPtr global, LuaSnPrintfFuncPtr instance);
void sbs_print_set_textType_print(LuaSnPrintfFuncPtr instance);
void sbs_print_set_font_print(LuaSnPrintfFuncPtr instance);
void sbs_print_set_number2_print(LuaSnPrintfFuncPtr global, LuaSnPrintfFuncPtr instance);
void sbs_print_set_g_assets_print(LuaSnPrintfFuncPtr global);
void sbs_print_set_g_palette_print(LuaSnPrintfFuncPtr global);
void sbs_print_set_assetType_print(LuaSnPrintfFuncPtr instance);
void sbs_print_set_physicsMode_print(LuaSnPrintfFuncPtr instance);
void sbs_print_set_quad_print(LuaSnPrintfFuncPtr global, LuaSnPrintfFuncPtr instance);
void sbs_print_set_animation_print(LuaSnPrintfFuncPtr global, LuaSnPrintfFuncPtr instance);
void sbs_print_set_rotation_print(LuaSnPrintfFuncPtr global, LuaSnPrintfFuncPtr instance);
void sbs_print_set_mesh_print(LuaSnPrintfFuncPtr global, LuaSnPrintfFuncPtr instance);

void sbs_print_set_print_output(LuaPrintOutputFuncPtr f);

int sbs_print(lua_State *L);
