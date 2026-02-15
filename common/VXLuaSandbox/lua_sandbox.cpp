//
//  lua_sandbox.cpp
//  Cubzh
//
//  Created by Adrien Duermael on 01/02/2020.
//  Copyright © 2020 Voxowl Inc. All rights reserved.
//

#include "lua_sandbox.hpp"

// Lua
#include "lua.hpp"

#include "VXHubClient.hpp"

// sandbox
#ifndef P3S_CLIENT_HEADLESS
#include "lua_audioSource.hpp"
#include "lua_audioListener.hpp"
#endif
#include "lua_face.hpp"
#include "lua_camera.hpp"
#include "lua_client.hpp"
#include "lua_color.hpp"
#include "lua_constants.h"
#include "lua_dev.hpp"
#include "lua_inputs.hpp"
#include "lua_mutableShape.hpp"
#include "lua_nil.hpp"
#include "lua_nilSilentExtra.hpp"
#include "lua_number3.hpp"
#include "lua_rotation.hpp"
#include "lua_items.hpp"
#include "lua_time.hpp"
#include "lua_config.hpp"
#include "lua_modules.hpp"
#include "sbs_print.hpp"
#include "lua_type.hpp"
#include "lua_utils.hpp"
#include "lua_pointer.hpp"
#include "lua_block.hpp"
#include "lua_block_properties.hpp"
#include "lua_color.hpp"
#include "lua_time.hpp"
#include "lua_box.hpp"
#include "lua_map.hpp"
#include "lua_event.hpp"
#include "lua_player.hpp"
#include "lua_players.hpp"
#include "lua_private.hpp"
#include "lua_ray.hpp"
#include "lua_server.hpp"
#include "lua_shape.hpp"
#include "lua_clouds.hpp"
#include "lua_collision_groups.hpp"
#include "lua_fog.hpp"
#include "lua_object.hpp"
#include "lua_point.hpp"
#include "lua_timer.hpp"
#include "lua_logs.hpp"
#include "lua_screen.hpp"
#include "scripting.hpp"
#include "scripting_server.hpp"
#include "serialization.h"
#include "lua_world.hpp"
#include "lua_light.hpp"
#include "lua_lightType.hpp"
#include "lua_http.hpp"
#include "lua_json.hpp"
#include "lua_url.hpp"
#include "lua_data.hpp"
#include "lua_environment.hpp"
#include "lua_file.hpp"
#include "lua_projectionMode.hpp"
#include "lua_require.hpp"
#include "lua_default_colors.hpp"
#include "lua_impact.hpp"
#include "lua_text.hpp"
#include "lua_textType.hpp"
#include "lua_number2.hpp"
#include "lua_assets.hpp"
#include "lua_assetType.hpp"
#include "lua_physicsMode.hpp"
#include "lua_quad.hpp"
#include "lua_ai.hpp"
#include "lua_pointer_event.hpp"
#include "lua_sky.hpp"
#include "lua_animation.hpp"
#include "lua_rotation.hpp"
#include "lua_deprecated.hpp"
#include "lua_mesh.hpp"

// xptools
#include "xptools.h"

// vx
#include "GameCoordinator.hpp"

// xptools
#include "OperationQueue.hpp"
#include "lua_font.hpp"

// --------------------------------------------------
//
// MARK: - Static functions prototypes -
//
// --------------------------------------------------

/// Adds Lua libs in the sandbox
static void openLibs(lua_State * const L);

///
static int _global_metatable_index(lua_State * const L);
static int _global_metatable_newindex(lua_State * const L);

static int lua_collectgarbage(lua_State* L) {
    const char* option = luaL_optstring(L, 1, "collect");
    if (strcmp(option, "collect") == 0) {
        lua_gc(L, LUA_GCCOLLECT, 0);
        return 0;
    }
    if (strcmp(option, "count") == 0) {
        int c = lua_gc(L, LUA_GCCOUNT, 0);
        lua_pushnumber(L, c);
        return 1;
    }
    luaL_error(L, "collectgarbage must be called with 'count' or 'collect'");
}

// generic setup part (for both client and server)
// Called as soon as the Lua sandbox is created.
// Before the script is parsed.
void lua_sandbox_setup_pre_load(lua_State * const L) {
    vxlog_debug("SANDBOX PRE LOAD for state %p", L);
    vx_assert(L != nullptr);
    LUAUTILS_STACK_SIZE(L)

    // openLibs(L); // same as luaL_openlibs(lstate)
    luaL_openlibs(L);

    LUAUTILS_STACK_SIZE_ASSERT(L, 0)

    // setup / override existing primitives like `nil` or `type`
    lua_nilSilentExtra_setupRegistry(L);
    lua_nil_setupRegistry(L);
    sbs_luaType_setup(L);

    LUAUTILS_STACK_SIZE_ASSERT(L, 0)

    // setup print functions
    // NOTE: maybe this could be simplified, using __tostring / __name metafields

    sbs_print_set_print_output(lua_log_info_CStr);
    sbs_print_set_block_print(lua_block_snprintf);
    sbs_print_set_block_face_print(lua_face_snprintf);
    sbs_print_set_block_properties(lua_block_properties_snprintf);
    sbs_print_set_box(lua_g_box_snprintf, lua_box_snprintf);
    sbs_print_set_color_print(lua_color_snprintf);
    sbs_print_set_g_camera_print(lua_camera_snprintf);
    sbs_print_set_g_number3_print(lua_g_number3_snprintf);
    sbs_print_set_map_print(lua_map_snprintf);
    sbs_print_set_world_print(lua_world_snprintf);
    sbs_print_set_player_print(lua_player_snprintf);
    sbs_print_set_number3_print(lua_number3_snprintf);
    sbs_print_set_point_print(lua_point_snprintf);
    sbs_print_set_server_print(lua_server_snprintf);
    sbs_print_set_g_clouds_print(lua_g_clouds_snprintf);
    sbs_print_set_g_fog_print(lua_g_fog_snprintf);
    sbs_print_set_g_sky_print(lua_g_sky_snprintf);
    sbs_print_set_mutableShape_print(lua_g_mutableShape_snprintf, lua_mutableShape_snprintf);
    sbs_print_set_shape_print(lua_g_shape_snprintf, lua_shape_snprintf);
    sbs_print_set_object_print(lua_g_object_snprintf, lua_object_snprintf);
    sbs_print_set_palette_print(lua_g_palette_snprintf, lua_palette_snprintf);
    sbs_print_set_ray_print(lua_g_ray_snprintf, lua_ray_snprintf);
    sbs_print_set_impact_print(lua_impact_snprintf);
    sbs_print_set_collision_groups_print(lua_g_collision_groups_snprintf, lua_collision_groups_snprintf);
    sbs_print_set_event_print(lua_g_event_snprintf, lua_event_snprintf);
    sbs_print_set_light_print(lua_g_light_snprintf, lua_light_snprintf);
    sbs_print_set_lightType_print(lua_lightType_snprintf);
    sbs_print_set_g_http_print(lua_g_http_snprintf);
    sbs_print_set_g_json_print(lua_g_json_snprintf);
    sbs_print_set_g_url_print(lua_g_url_snprintf);
    sbs_print_set_file_print(lua_g_file_snprintf);
    sbs_print_set_projectionMode_print(lua_projectionMode_snprintf);
    sbs_print_set_text_print(lua_g_text_snprintf, lua_text_snprintf);
    sbs_print_set_textType_print(lua_textType_snprintf);
    sbs_print_set_font_print(lua_font_snprintf);
    sbs_print_set_number2_print(lua_g_number2_snprintf, lua_number2_snprintf);
    sbs_print_set_g_assets_print(lua_g_assets_snprintf);
    sbs_print_set_assetType_print(lua_assetType_snprintf);
    sbs_print_set_physicsMode_print(lua_physicsMode_snprintf);
    sbs_print_set_quad_print(lua_g_quad_snprintf, lua_quad_snprintf);
    sbs_print_set_animation_print(lua_g_animation_snprintf, lua_animation_snprintf);
    sbs_print_set_rotation_print(lua_g_rotation_snprintf, lua_rotation_snprintf);
    sbs_print_set_mesh_print(lua_g_mesh_snprintf, lua_mesh_snprintf);

    lua_pushcfunction(L, sbs_print, "print");
    lua_setglobal(L, "print");

    // exposing collectgarbage here as it doesn't seem to be done by default with Luau.
    lua_pushcfunction(L, lua_collectgarbage, "collectgarbage");
    lua_setglobal(L, "collectgarbage");

    LUAUTILS_STACK_SIZE_ASSERT(L, 0)

    lua_utils_pushRootGlobalsFromRegistry(L);

    LUAUTILS_STACK_SIZE_ASSERT(L, 1)

    // Set Global's metatable
    lua_newtable(L); // metatable
    {
        lua_pushstring(L, "__index");
        lua_pushcfunction(L, _global_metatable_index, "");
        lua_rawset(L, -3);

        lua_pushstring(L, "__newindex");
        lua_pushcfunction(L, _global_metatable_newindex, "");
        lua_rawset(L, -3);

        lua_pushstring(L, "__metatable");
        lua_pushboolean(L, false);
        lua_rawset(L, -3);
    }
    lua_setmetatable(L, -2);

    LUAUTILS_STACK_SIZE_ASSERT(L, 1)

    const int r = lua_getmetatable(L, -1);
    vx_assert(r == 1);

    LUAUTILS_STACK_SIZE_ASSERT(L, 2)

    // Store all fields in Global's metatable
    // TODO: some variables could be created on demand, when not found
    // (within _global_metatable_index)

    lua_pushstring(L, P3S_LUA_G_NUMBER3);
    lua_g_number3_create_and_push(L);
    lua_rawset(L, -3);

    lua_pushstring(L, P3S_LUA_G_ROTATION);
    lua_g_rotation_create_and_push(L);
    lua_rawset(L, -3);

    lua_pushstring(L, P3S_LUA_G_BLOCK);
    lua_g_block_create_and_push(L);
    lua_rawset(L, -3);

    lua_pushstring(L, P3S_LUA_G_COLOR);
    lua_g_color_create_and_push(L);
    lua_rawset(L, -3);

    lua_pushstring(L, P3S_LUA_G_TIME);
    lua_g_time_create_and_push(L);
    lua_rawset(L, -3);

    // Timer
    lua_pushstring(L, P3S_LUA_G_TIMER);
    lua_g_timer_create_and_push(L);
    lua_rawset(L, -3);

    // set Config
    lua_pushstring(L, LUA_GLOBAL_FIELD_CONFIG);
    lua_config_create_default_and_push(L);
    lua_rawset(L, -3);

    // set Modules
    lua_pushstring(L, LUA_G_MODULES);
    lua_modules_createAndPush(L);
    lua_rawset(L, -3);

    // set Items (== Config.Items)
    lua_pushstring(L, LUA_GLOBAL_FIELD_ITEMS);
    lua_getfield(L, -2, LUA_GLOBAL_FIELD_CONFIG);
    lua_config_push_items(L);
    lua_remove(L, -2); // remove Config
    lua_rawset(L, -3);

    // set Dev
    lua_pushstring(L, P3S_LUA_G_DEV);
    lua_dev_create_and_push(L);
    lua_rawset(L, -3);

    // set Private
    lua_pushstring(L, P3S_LUA_G_PRIVATE);
    lua_private_create_and_push(L);
    lua_rawset(L, -3);

    // DefaultColors
    lua_pushstring(L, P3S_LUA_G_DEFAULT_COLORS);
    lua_default_colors_create_and_push(L);
    lua_rawset(L, -3);

    // Object
    lua_pushstring(L, P3S_LUA_G_OBJECT);
    lua_g_object_createAndPush(L);
    lua_rawset(L, -3);

    // Shape
    lua_pushstring(L, P3S_LUA_G_SHAPE);
    lua_g_shape_createAndPush(L);
    lua_rawset(L, -3);

    // MutableShape
    lua_pushstring(L, P3S_LUA_G_MUTABLESHAPE);
    lua_g_mutableShape_createAndPush(L);
    lua_rawset(L, -3);

    // Face
    lua_pushstring(L, P3S_LUA_G_FACE);
    lua_g_face_createAndPush(L);
    lua_rawset(L, -3);

    // Light
    lua_pushstring(L, P3S_LUA_G_LIGHT);
    lua_g_light_createAndPush(L);
    lua_rawset(L, -3);

    // LightType
    lua_pushstring(L, P3S_LUA_G_LIGHTTYPE);
    lua_g_lightType_createAndPush(L);
    lua_rawset(L, -3);

    // Camera
    lua_pushstring(L, LUA_G_CAMERA);
    lua_g_camera_createAndPush(L);
    lua_rawset(L, -3);

    // ProjectionMode
    lua_pushstring(L, LUA_G_PROJECTIONMODE);
    lua_g_projectionMode_createAndPush(L);
    lua_rawset(L, -3);

    // Text
    lua_pushstring(L, LUA_G_TEXT);
    lua_g_text_createAndPush(L);
    lua_rawset(L, -3);

    // TextType
    lua_pushstring(L, LUA_G_TEXTTYPE);
    lua_g_textType_createAndPush(L);
    lua_rawset(L, -3);

    // Font
    lua_pushstring(L, LUA_G_FONT);
    lua_g_font_createAndPush(L);
    lua_rawset(L, -3);

    // Number2
    lua_pushstring(L, LUA_G_NUMBER2);
    lua_g_number2_createAndPush(L);
    lua_rawset(L, -3);

    // require
    lua_pushstring(L, LUA_G_REQUIRE);
    lua_g_require_create_and_push(L);
    lua_rawset(L, -3);

    // Resources
    lua_pushstring(L, LUA_G_ASSETS);
    lua_g_assets_create_and_push(L);
    lua_rawset(L, -3);

    // Palette
    lua_pushstring(L, LUA_G_PALETTE);
    lua_g_palette_create_and_push(L);
    lua_rawset(L, -3);

    // AssetType
    lua_pushstring(L, LUA_G_ASSETTYPE);
    lua_g_assetType_createAndPush(L);
    lua_rawset(L, -3);

    // PhysicsMode
    lua_pushstring(L, LUA_G_PHYSICSMODE);
    lua_g_physicsMode_createAndPush(L);
    lua_rawset(L, -3);

    // LocalEvent
    lua_pushstring(L, LUA_G_LOCALEVENT);
    LUAUTILS_STACK_SIZE_ASSERT(L, 3)
    if (luau_dostring(L, R"""(
    return require("localevent")
    )""", "LocalEvent") == false) {
        lua_pushnil(L);
    }
    LUAUTILS_STACK_SIZE_ASSERT(L, 4)
    lua_rawset(L, -3);

    // TimeCycle
    lua_pushstring(L, LUA_G_TIMECYCLE);
    if (luau_dostring(L, R"""(
    return require("empty_table"):create("TimeCycle does not exist anymore")
    )""", "TimeCycle") == false) {
        lua_pushnil(L);
    }
    lua_rawset(L, -3);

    // TimeCycleMark
    lua_pushstring(L, LUA_G_TIME_CYCLE_MARK);
    if (luau_dostring(L, R"""(
    return require("empty_table"):create("TimeCycleMark does not exist anymore")
    )""", "TimeCycleMark") == false) {
        lua_pushnil(L);
    }
    lua_rawset(L, -3);

    // Quad
    lua_pushstring(L, LUA_G_QUAD);
    lua_g_quad_createAndPush(L);
    lua_rawset(L, -3);

    // Deprecated
    lua_pushstring(L, P3S_LUA_G_DEPRECATED);
    lua_deprecated_create_and_push(L);
    lua_rawset(L, -3);

    // Ray
    lua_pushstring(L, P3S_LUA_G_RAY);
    lua_g_ray_create_and_push(L);
    lua_rawset(L, -3);

    // CollisionGroups
    lua_pushstring(L, P3S_LUA_G_COLLISIONGROUPS);
    lua_g_collision_groups_create_and_push(L);
    lua_rawset(L, -3);

    // Box
    lua_pushstring(L, P3S_LUA_G_BOX);
    lua_g_box_create_and_push(L);
    lua_rawset(L, -3);

    // Mesh
    lua_pushstring(L, LUA_G_MESH);
    lua_g_mesh_createAndPush(L);
    lua_rawset(L, -3);

    LUAUTILS_STACK_SIZE_ASSERT(L, 2)

    lua_pop(L, 1); // pop metatable
    lua_replace(L, LUA_GLOBALSINDEX); // pops global table

    // lua_pop(L, 2); // pop metatable & global table

    LUAUTILS_STACK_SIZE_ASSERT(L, 0)
}

// generic setup part (for both client and server)
// Called right after script parsing (Config loaded).
void lua_sandbox_setup_post_load(lua_State * const L, CGame *g) {
    vx_assert(L != nullptr);

    Shape *mapShape = game_get_map_shape(g);
    if (mapShape == nullptr) {
        vxlog_error("game has no Map");
        return;
    }

    Scene *s = game_get_scene(g);
    if (s == nullptr) {
        vxlog_error("game has no Scene");
        return;
    }

    LUAUTILS_STACK_SIZE(L)

    // lua_pushvalue(L, LUA_GLOBALSINDEX);
    lua_utils_pushRootGlobalsFromRegistry(L);
    lua_getmetatable(L, -1);
    lua_setreadonly(L, -1, false);
    {
        // PointerEvent
        lua_pushstring(L, LUA_G_POINTER_EVENT);
        lua_g_pointer_event_create_and_push(L);
        lua_rawset(L, -3);

        // JSON
        lua_pushstring(L, LUA_G_JSON);
        lua_g_json_create_and_push(L);
        lua_rawset(L, -3);

        // Data
        lua_pushstring(L, LUA_G_DATA);
        lua_g_data_createAndPush(L);
        lua_rawset(L, -3);

        // World
        lua_pushstring(L, P3S_LUA_G_WORLD);
        if(lua_g_world_create_and_push(L, s)) {
            lua_rawset(L, -3);
        } else {
            vxlog_error("failed to create Lua World");
            lua_pop(L, 1); // pop string
        }

        // Map
        lua_pushstring(L, P3S_LUA_G_MAP);
        if (lua_g_map_create_and_push(L, mapShape)) {
            lua_rawset(L, -3);
        } else {
            vxlog_error("failed to create Lua Map");
            lua_pop(L, 1); // pop string
        }

        // AI
        lua_pushstring(L, LUA_G_AI);
        lua_g_ai_create_and_push(L);
        lua_rawset(L, -3);

        // Animation
        lua_pushstring(L, LUA_G_ANIMATION);
        lua_g_animation_create_and_push(L);
        lua_rawset(L, -3);

        // Menu
        lua_pushstring(L, LUA_G_MENU);
        if (luau_dostring(L, R"""(
        return require("menu")
        )""", "Menu") == false) {
            lua_pushnil(L);
        }
        lua_rawset(L, -3);
        LUAUTILS_STACK_SIZE_ASSERT(L, 2)

        // KeyValueStore
        lua_pushstring(L, P3S_LUA_G_KEYVALUESTORE);
        if (luau_dostring(L, R"""(
        return require("kvs")
        )""", "KeyValueStore") == false) {
            lua_pushnil(L);
        }
        lua_rawset(L, -3);

        // KeyValueStore
        lua_pushstring(L, P3S_LUA_G_LEADERBOARD);
        if (luau_dostring(L, R"""(
        return require("leaderboard")
        )""", "Leaderboard") == false) {
            lua_pushnil(L);
        }
        lua_rawset(L, -3);

    }
    lua_setreadonly(L, -1, true);
    lua_pop(L, 2); // pop metatable & global table

    LUAUTILS_STACK_SIZE_ASSERT(L, 0)
}

///
void lua_sandbox_client_setup_pre_load(lua_State *const L,
                                       const std::unordered_map<std::string, std::string> &environment) {
    vx_assert(L != nullptr);
    LUAUTILS_STACK_SIZE(L)

    lua_sandbox_setup_pre_load(L);

    LUAUTILS_STACK_SIZE_ASSERT(L, 0)

    lua_utils_pushRootGlobalsFromRegistry(L);

    const int r = lua_getmetatable(L, -1);
    vx_assert(r == 1);

    lua_pushstring(L, P3S_LUA_G_IS_CLIENT);
    lua_pushboolean(L, true);
    lua_rawset(L, -3);

    lua_pushstring(L, P3S_LUA_G_IS_SERVER);
    lua_pushboolean(L, false);
    lua_rawset(L, -3);

    // Client
    lua_pushstring(L, P3S_LUA_G_CLIENT);
    lua_client_createAndPush(L);
    lua_rawset(L, -3);
    {
        // Shortcuts to Client fields

        // Pointer = Client.Inputs.Pointer
        lua_pushstring(L, LUA_INPUTS_FIELD_POINTER);
        lua_client_pushField(L, LUA_CLIENT_FIELD_INPUTS);
        lua_g_inputs_pushField(L, -1, LUA_INPUTS_FIELD_POINTER);
        lua_remove(L, -2); // pop Client.Inputs
        lua_rawset(L, -3);

        // Screen = Client.Screen
        lua_pushstring(L, LUA_CLIENT_FIELD_SCREEN);
        lua_client_pushField(L, LUA_CLIENT_FIELD_SCREEN);
        lua_rawset(L, -3);
    }

    lua_pushstring(L, P3S_LUA_G_SERVER);
    lua_server_createAndPushForClient(L);
    lua_rawset(L, -3);

    // TODO: What follows should be in post load

    // OtherPlayers
    lua_pushstring(L, P3S_LUA_G_OTHER_PLAYERS);
    lua_g_otherPlayers_createAndPush(L);
    lua_rawset(L, -3);

    // Players
    lua_pushstring(L, P3S_LUA_G_PLAYERS);
    lua_g_players_createAndPush(L);
    lua_rawset(L, -3);

    // Event
    lua_pushstring(L, P3S_LUA_G_EVENT);
    lua_g_event_create_and_push(L);
    lua_rawset(L, -3);

    // URL
    lua_pushstring(L, LUA_G_URL);
    lua_g_url_create_and_push(L);
    lua_rawset(L, -3);

    // Environment
    lua_pushstring(L, LUA_G_ENVIRONMENT);
    lua_g_environment_create_and_push(L);
    lua_rawset(L, -3);

    // File
    lua_pushstring(L, LUA_G_FILE);
    lua_g_file_create_and_push(L);
    lua_rawset(L, -3);

#ifndef P3S_CLIENT_HEADLESS
    // AudioSource
    lua_pushstring(L, LUA_G_AUDIOSOURCE);
    lua_g_audioSource_createAndPush(L);
    lua_rawset(L, -3);
    // AudioListener
    lua_pushstring(L, LUA_G_AUDIOLISTENER);
    lua_g_audiolistener_createAndPush(L);
    lua_rawset(L, -3);
#endif

    // accepts only cubzh api call from client
    lua_pushstring(L, LUA_G_HTTP);
    lua_g_http_create_and_push(L);
    lua_rawset(L, -3);

    LUAUTILS_STACK_SIZE_ASSERT(L, 2)

    lua_pop(L, 2); // pop metatable & globals

    LUAUTILS_STACK_SIZE_ASSERT(L, 0)
}

void lua_sandbox_client_setup_post_load(lua_State * const L, vx::Game *g) {
    vx_assert(L != nullptr);
    LUAUTILS_STACK_SIZE(L)

    // generic setup (client/server)
    lua_sandbox_setup_post_load(L, g->getCGame());

    // define shortcuts
    lua_sandbox_set_shortcuts_post_load(L, false);

    LUAUTILS_STACK_SIZE_ASSERT(L, 0)
}

void lua_sandbox_server_setup_pre_load(lua_State * const L) {
    vx_assert(L != nullptr);
    LUAUTILS_STACK_SIZE(L)

    lua_sandbox_setup_pre_load(L);

    LUAUTILS_STACK_SIZE_ASSERT(L, 0)

    lua_utils_pushRootGlobalsFromRegistry(L);
    lua_getmetatable(L, -1);
    {
        lua_pushstring(L, P3S_LUA_G_IS_CLIENT);
        lua_pushboolean(L, false);
        lua_rawset(L, -3);

        lua_pushstring(L, P3S_LUA_G_IS_SERVER);
        lua_pushboolean(L, true);
        lua_rawset(L, -3);

        lua_pushstring(L, P3S_LUA_G_SERVER);
        lua_server_createAndPushForServer(L);
        lua_rawset(L, -3);

        // on Server, Client is just a completely silent nil
        lua_pushstring(L, P3S_LUA_G_CLIENT);
        lua_nilSilentExtra_push(L);
        lua_rawset(L, -3);

        lua_pushstring(L, LUA_CLIENT_FIELD_PLAYER);
        lua_nilSilentExtra_push(L);
        lua_rawset(L, -3);

        lua_pushstring(L, LUA_CLIENT_FIELD_CLOUDS);
        lua_nilSilentExtra_push(L);
        lua_rawset(L, -3);

        lua_pushstring(L, LUA_CLIENT_FIELD_FOG);
        lua_nilSilentExtra_push(L);
        lua_rawset(L, -3);

        lua_pushstring(L, LUA_CLIENT_FIELD_SKY);
        lua_nilSilentExtra_push(L);
        lua_rawset(L, -3);

        lua_pushstring(L, LUA_CLIENT_FIELD_SCREEN);
        lua_nilSilentExtra_push(L);
        lua_rawset(L, -3);

        lua_pushstring(L, LUA_CLIENT_FIELD_INPUTS);
        lua_nilSilentExtra_push(L);
        lua_rawset(L, -3);

        lua_pushstring(L, LUA_INPUTS_FIELD_POINTER);
        lua_nilSilentExtra_push(L);
        lua_rawset(L, -3);

        // TODO: What follows should be in post load

        // Event
        lua_pushstring(L, P3S_LUA_G_EVENT);
        lua_g_event_create_and_push(L);
        lua_rawset(L, -3);

        // Players
        lua_pushstring(L, P3S_LUA_G_PLAYERS);
        lua_g_players_createAndPush(L);
        lua_rawset(L, -3);

        // HTTP
        lua_pushstring(L, LUA_G_HTTP);
        lua_g_http_create_and_push(L);
        lua_rawset(L, -3);
    }
    lua_pop(L, 2); // pop metatable & global table

    LUAUTILS_STACK_SIZE_ASSERT(L, 0)
}

void lua_sandbox_server_setup_post_load(lua_State * const L, vx::Game *g) {
    vx_assert(L != nullptr);
    LUAUTILS_STACK_SIZE(L)

    // generic setup (client/server)
    lua_sandbox_setup_post_load(L, g->getCGame());

    // define shortcuts
    lua_sandbox_set_shortcuts_post_load(L, true);

    LUAUTILS_STACK_SIZE_ASSERT(L, 0)
}

void lua_sandbox_set_shortcuts_post_load(lua_State * const L, const bool isOnServer) {
    vx_assert(L != nullptr);
    LUAUTILS_STACK_SIZE(L)

    /*
     NOTE: shortcuts can't be set with doStrings because global
     table is already locked, not accepting variable with uppercase
     initial letters to be set.
     It can be done with lua_rawset.
     */

    if (isOnServer == false) {
        lua_utils_pushRootGlobalsFromRegistry(L);
        lua_getmetatable(L, -1);
        lua_setreadonly(L, -1, false);

        // Clouds = Client.Clouds
        lua_pushstring(L, LUA_CLIENT_FIELD_CLOUDS);
        lua_client_pushField(L, LUA_CLIENT_FIELD_CLOUDS);
        lua_rawset(L, -3);

        // Fog = Client.Fog
        lua_pushstring(L, LUA_CLIENT_FIELD_FOG);
        lua_client_pushField(L, LUA_CLIENT_FIELD_FOG);
        lua_rawset(L, -3);

        // Sky = Client.Sky
        lua_pushstring(L, LUA_CLIENT_FIELD_SKY);
        lua_client_pushField(L, LUA_CLIENT_FIELD_SKY);
        lua_rawset(L, -3);

        // Player = Client.Player
        lua_pushstring(L, LUA_CLIENT_FIELD_PLAYER);
        lua_client_pushField(L, LUA_CLIENT_FIELD_PLAYER);
        lua_rawset(L, -3);

        LUAUTILS_STACK_SIZE_ASSERT(L, 2)

        // Alias BlockFace = Face
        lua_pushstring(L, "BlockFace");
        lua_getfield(L, -2, P3S_LUA_G_FACE);
        lua_rawset(L, -3);

        // Alias UI (deprecated) = Deprecated
        lua_pushstring(L, "UI");
        lua_getfield(L, -2, P3S_LUA_G_DEPRECATED);
        lua_rawset(L, -3);

        lua_setreadonly(L, -1, true);
        lua_pop(L, 2); // pop _G and its metatable
    }

    // complete default script

    const char * script = R"""(
Camera.CastRay = function(camera, filterIn, filterOut)
    if camera == nil or typeof(camera) ~= "Camera" then
        error("Camera:CastRay should be called with `:`")
    end

    local ray = Ray(Camera.Position, Camera.Forward)
    local impact = ray:Cast(filterIn, filterOut)

    return impact
end

-- (1) target: Object, defaults to Player
-- (2) offset: offset to target (number or Number3), default (0, 2.25, 0)
Camera.SetModeFirstPerson = function(camera, target, offset)
	if camera ~= Camera then error("Camera:SetModeFirstPerson(camera, target, offset) should be called with `:`", 2) end
    if not target then target = Player end
    return require("camera_modes"):setFirstPerson({ camera = camera, target = target, offset = offset })
end

-- config defaults in camera_modes.setThirdPerson 
-- legacy parameters: target, minDist, maxDist, offset
Camera.SetModeThirdPerson = function(camera, configOrTarget, minDist, maxDist, offset)
	if typeof(camera) ~= "Camera" then error("Camera:SetModeThirdPerson(config) should be called with `:`", 2) end

    -- support legacy parameters (target, minDist, maxDist, offset)
    local config = configOrTarget
    local defaultDistance = 40
    if config == nil then
        config = { target = Player, distance = defaultDistance, minDistance = minDist, maxDistance = maxDist, offset = offset }
    elseif typeof(config) ~= "table" then
        local t = typeof(config)
        if t == "Object" or t == "Shape" or t == "MutableShape" or t == "Number3" or t == "Player" or t == "Quad" then
            config = { target = config, distance = defaultDistance, minDistance = minDist, maxDistance = maxDist, offset = offset }
        else
            error("Camera:SetModeThirdPerson(config) - config should be a table", 2)
        end
    end

    config.camera = camera
    return require("camera_modes"):setThirdPerson(config)
end

-- config defaults in camera_modes.setSatellite
-- legacy parameters: target, distance
Camera.SetModeSatellite = function(camera, configOrTarget, distance)
	if typeof(camera) ~= "Camera" then error("Camera:SetModeSatellite(config) should be called with `:`", 2) end

    -- support legacy parameters (target, distance)
    local config = configOrTarget
    local defaultDistance = 40
    if config == nil then
        config = { target = Player, distance = defaultDistance }
    elseif typeof(config) ~= "table" then
        local t = typeof(config)
        if t == "Object" or t == "Shape" or t == "MutableShape" or t == "Number3" or t == "Player" or t == "Quad" then
            config = { target = config, distance = defaultDistance }
        else
            error("Camera:SetModeSatellite(config) - config should be a table", 2)
        end
    end

    config.camera = camera
    return require("camera_modes"):setSatellite(config)
end

Camera.SetModeFree = function(camera)
	if typeof(camera) ~= "Camera" then error("Camera:SetModeFree() should be called with `:`", 2) end
    return require("camera_modes"):setFree({ camera = camera })
end

-- add global Object:IsObject(o) ?
isObject = function(o)
    return o ~= nil and o.Position ~= nil
end

dostring = function(s)
    load(s)()
end
)""";

    const int success = luau_dostring(L, script, "Post Load");
    vx_assert(success);

    LUAUTILS_STACK_SIZE_ASSERT(L, 0)
}

///
static void openLibs(lua_State * const L) {
    // we can simply comment the libs we don't want in the sandbox
    static const luaL_Reg loadedlibs[] = {
        {"_G", luaopen_base},
        // {LUA_LOADLIBNAME, luaopen_package}, // disable package (contains `require`)
        // {LUA_COLIBNAME, luaopen_coroutine}, // disable coroutine.*
        {LUA_TABLIBNAME, luaopen_table},
        // {LUA_IOLIBNAME, luaopen_io}, // disable io (allows to read/write in files)
        {LUA_OSLIBNAME, luaopen_os},
        {LUA_STRLIBNAME, luaopen_string},
        {LUA_MATHLIBNAME, luaopen_math},
        {LUA_UTF8LIBNAME, luaopen_utf8},
        // {LUA_DBLIBNAME, luaopen_debug}, // may not be a good idea to expose this
#if defined(LUA_COMPAT_BITLIB)
        {LUA_BITLIBNAME, luaopen_bit32},
#endif
        {nullptr, nullptr}
    };

    const luaL_Reg *lib;
    // "require" functions from 'loadedlibs' and set results to global table
    for (lib = loadedlibs; lib->func; lib++) {
        lib->func(L);  // Call the library opening function
        lua_pushvalue(L, -1);  // Duplicate the library table
        lua_setglobal(L, lib->name);  // Set it as a global

        // Lua 5.3:
        // luaL_requiref(L, lib->name, lib->func, 1);
        // lua_pop(L, 1);  // remove lib
    }
}

/// __index
static int _global_metatable_index(lua_State * const L) {
    vx_assert(L != nullptr);
    LUAUTILS_ASSERT_ARGS_COUNT(L, 2)
    LUAUTILS_STACK_SIZE(L)

    // System variables are stored in the metatable, get them from here.
    const char *key = lua_tostring(L, 2);
    // vxlog_debug("_global_metatable_index %s", key);

    // get value from global metatable, load user defined variables if not found
    if (LUA_GET_METAFIELD_AND_RETURN_TYPE(L, 1, key) != LUA_TNIL) {
        LUAUTILS_STACK_SIZE_ASSERT(L, 1)
        return 1;
    }

    // User defined variables, just return them.
    lua_pushvalue(L, 2); // push key
    lua_rawget(L, 1); // get value

    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
    return 1;
}

/// __newindex function loading sandbox _G metatable
/// arguments: table, key, value
/// This prevents setting values.
static int _global_metatable_newindex(lua_State * const L) {
    vx_assert(L != nullptr);
    LUAUTILS_ASSERT_ARGS_COUNT(L, 3)
    LUAUTILS_STACK_SIZE(L)

    // Trying to set a system variable, it's only allowed for a few of them.
    if (lua_utils_isStringStrictStartingWithUppercase(L, 2)) {

        const char *key = lua_tostring(L, 2);

        if (strcmp(key, LUA_G_MODULES) == 0) {
            // setting Config like this:
            // Modules = { ui = "uikit", "multi", pathfinding = "github.com/aduermael/pathfinding" }

            // push Modules onto stack
            if (LUA_GET_METAFIELD_AND_RETURN_TYPE(L, 1, LUA_G_MODULES) == LUA_TNIL) {
                LUAUTILS_INTERNAL_ERROR(L) // returns
            }

            // Modules should be at -1 when calling this
            lua_modules_set_from_table(L, 3);
            lua_pop(L, 1); // pop Modules

        } else if (strcmp(key, LUA_GLOBAL_FIELD_CONFIG) == 0) {
            // setting Config like this:
            // Config = { Items = {"foo", "bar"}, Map = "baz" }

            // push Config onto stack
            lua_getglobal(L, LUA_GLOBAL_FIELD_CONFIG);
            if (lua_utils_getObjectType(L, -1) != ITEM_TYPE_CONFIG) {
                LUAUTILS_INTERNAL_ERROR(L) // returns
            }

            // Config should be at -1 when calling this
            lua_config_set_from_table(L, 3 /* Config table idx */);
            lua_pop(L, 1); // pop Config

        } else {
            LUAUTILS_ERROR_F(L, "\"%s\" can't be set, global variables starting with an uppercase letter are reserved", key);
        }

    } else {
        // user defined global variable
        lua_pushvalue(L, 2); // key
        lua_pushvalue(L, 3); // value
        lua_rawset(L, 1);
    }

    LUAUTILS_STACK_SIZE_ASSERT(L, 0)
    return 0;
}
