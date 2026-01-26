//
//  sbs_print.c
//  sbs
//
//  Created by Adrien Duermael on 8/19/19.
//  Copyright © 2019 Voxowl Inc. All rights reserved.
//

#include "sbs_print.hpp"

// C
#include <stdlib.h>

#include "config.h"

// lua
#include "lua.hpp"
#include "lua_utils.hpp"

// sandbox
#include "lua_constants.h"

typedef struct {
    LuaSnPrintfFuncPtr block;
    LuaSnPrintfFuncPtr block_face;
    LuaSnPrintfFuncPtr block_properties;
    LuaSnPrintfFuncPtr g_box;
    LuaSnPrintfFuncPtr box;
    LuaSnPrintfFuncPtr map;
    LuaSnPrintfFuncPtr world;
    LuaSnPrintfFuncPtr color;
    LuaSnPrintfFuncPtr camera;
    LuaSnPrintfFuncPtr g_number3;
    LuaSnPrintfFuncPtr g_point;
    LuaSnPrintfFuncPtr player;
    LuaSnPrintfFuncPtr number3;
    LuaSnPrintfFuncPtr point;
    LuaSnPrintfFuncPtr server;
    LuaSnPrintfFuncPtr g_clouds;
    LuaSnPrintfFuncPtr g_fog;
    LuaSnPrintfFuncPtr g_mutableShape;
    LuaSnPrintfFuncPtr mutableShape;
    LuaSnPrintfFuncPtr g_shape;
    LuaSnPrintfFuncPtr shape;
    LuaSnPrintfFuncPtr g_object;
    LuaSnPrintfFuncPtr object;
    LuaSnPrintfFuncPtr g_palette;
    LuaSnPrintfFuncPtr palette;
    LuaSnPrintfFuncPtr g_ray;
    LuaSnPrintfFuncPtr ray;
    LuaSnPrintfFuncPtr impact;
    LuaSnPrintfFuncPtr g_collisionGroups;
    LuaSnPrintfFuncPtr collisionGroups;
    LuaSnPrintfFuncPtr g_kvstore;
    LuaSnPrintfFuncPtr kvstore;
    LuaSnPrintfFuncPtr g_event;
    LuaSnPrintfFuncPtr event;
    LuaSnPrintfFuncPtr g_light;
    LuaSnPrintfFuncPtr light;
    LuaSnPrintfFuncPtr lightType;
    LuaSnPrintfFuncPtr g_http;
    LuaSnPrintfFuncPtr g_json;
    LuaSnPrintfFuncPtr g_url;
    LuaSnPrintfFuncPtr g_file;
    LuaSnPrintfFuncPtr projectionMode;
    LuaSnPrintfFuncPtr g_text, text, textType, font;
    LuaSnPrintfFuncPtr g_number2, number2;
    LuaSnPrintfFuncPtr g_assets;
    LuaSnPrintfFuncPtr assetType;
    LuaSnPrintfFuncPtr physicsMode;
    LuaSnPrintfFuncPtr g_quad;
    LuaSnPrintfFuncPtr quad;
    LuaSnPrintfFuncPtr g_sky;
    LuaSnPrintfFuncPtr g_animation;
    LuaSnPrintfFuncPtr animation;
    LuaSnPrintfFuncPtr g_rotation;
    LuaSnPrintfFuncPtr rotation;
    LuaSnPrintfFuncPtr g_mesh;
    LuaSnPrintfFuncPtr mesh;

    LuaPrintOutputFuncPtr output;
} PrintFunctions;

static PrintFunctions *print_functions(void) {
    static PrintFunctions p = {nullptr, nullptr};
    return &p;
}

void sbs_print_set_block_print(LuaSnPrintfFuncPtr printFunc) {
    print_functions()->block = printFunc;
}

void sbs_print_set_block_face_print(LuaSnPrintfFuncPtr printFunc) {
    print_functions()->block_face = printFunc;
}

void sbs_print_set_block_properties(LuaSnPrintfFuncPtr printFunc) {
    print_functions()->block_properties = printFunc;
}

void sbs_print_set_box(LuaSnPrintfFuncPtr printFunc, LuaSnPrintfFuncPtr instance) {
    print_functions()->g_box = printFunc;
    print_functions()->box = instance;
}

void sbs_print_set_map_print(LuaSnPrintfFuncPtr printFunc) {
    print_functions()->map = printFunc;
}

void sbs_print_set_world_print(LuaSnPrintfFuncPtr printFunc) {
    print_functions()->world = printFunc;
}

void sbs_print_set_color_print(LuaSnPrintfFuncPtr printFunc) {
    print_functions()->color = printFunc;
}

void sbs_print_set_g_camera_print(LuaSnPrintfFuncPtr printFunc) {
    print_functions()->camera = printFunc;
}

void sbs_print_set_g_number3_print(LuaSnPrintfFuncPtr printFunc) {
    print_functions()->g_number3 = printFunc;
}

void sbs_print_set_g_point_print(LuaSnPrintfFuncPtr printFunc) {
    print_functions()->g_point = printFunc;
}

void sbs_print_set_player_print(LuaSnPrintfFuncPtr printFunc) {
    print_functions()->player = printFunc;
}

void sbs_print_set_number3_print(LuaSnPrintfFuncPtr printFunc) {
    print_functions()->number3 = printFunc;
}

void sbs_print_set_point_print(LuaSnPrintfFuncPtr printFunc) {
    print_functions()->point = printFunc;
}

void sbs_print_set_server_print(LuaSnPrintfFuncPtr printFunc) {
    print_functions()->server = printFunc;
}

void sbs_print_set_g_clouds_print(LuaSnPrintfFuncPtr printFunc) {
    print_functions()->g_clouds = printFunc;
}

void sbs_print_set_g_fog_print(LuaSnPrintfFuncPtr printFunc) {
    print_functions()->g_fog = printFunc;
}

void sbs_print_set_g_sky_print(LuaSnPrintfFuncPtr printFunc) {
    print_functions()->g_sky = printFunc;
}

void sbs_print_set_mutableShape_print(LuaSnPrintfFuncPtr global, LuaSnPrintfFuncPtr instance) {
    print_functions()->g_mutableShape = global;
    print_functions()->mutableShape = instance;
}

void sbs_print_set_shape_print(LuaSnPrintfFuncPtr global, LuaSnPrintfFuncPtr instance) {
    print_functions()->g_shape = global;
    print_functions()->shape = instance;
}

void sbs_print_set_object_print(LuaSnPrintfFuncPtr global, LuaSnPrintfFuncPtr instance) {
    print_functions()->g_object = global;
    print_functions()->object = instance;
}

void sbs_print_set_palette_print(LuaSnPrintfFuncPtr global, LuaSnPrintfFuncPtr instance) {
    print_functions()->g_palette = global;
    print_functions()->palette = instance;
}

void sbs_print_set_ray_print(LuaSnPrintfFuncPtr global, LuaSnPrintfFuncPtr instance) {
    print_functions()->g_ray = global;
    print_functions()->ray = instance;
}

void sbs_print_set_impact_print(LuaSnPrintfFuncPtr instance) {
    print_functions()->impact = instance;
}

void sbs_print_set_rkvstore_print(LuaSnPrintfFuncPtr global, LuaSnPrintfFuncPtr instance) {
    print_functions()->g_kvstore = global;
    print_functions()->kvstore = instance;
}

void sbs_print_set_collision_groups_print(LuaSnPrintfFuncPtr global, LuaSnPrintfFuncPtr instance) {
    print_functions()->g_collisionGroups = global;
    print_functions()->collisionGroups = instance;
}

void sbs_print_set_event_print(LuaSnPrintfFuncPtr global, LuaSnPrintfFuncPtr instance) {
    print_functions()->g_event = global;
    print_functions()->event = instance;
}

void sbs_print_set_light_print(LuaSnPrintfFuncPtr global, LuaSnPrintfFuncPtr instance) {
    print_functions()->g_light = global;
    print_functions()->light = instance;
}

void sbs_print_set_lightType_print(LuaSnPrintfFuncPtr instance) {
    print_functions()->lightType = instance;
}

void sbs_print_set_projectionMode_print(LuaSnPrintfFuncPtr instance) {
    print_functions()->projectionMode = instance;
}

void sbs_print_set_text_print(LuaSnPrintfFuncPtr global, LuaSnPrintfFuncPtr instance) {
    print_functions()->g_text = global;
    print_functions()->text = instance;
}

void sbs_print_set_textType_print(LuaSnPrintfFuncPtr instance) {
    print_functions()->textType = instance;
}

void sbs_print_set_font_print(LuaSnPrintfFuncPtr instance) {
    print_functions()->font = instance;
}

void sbs_print_set_number2_print(LuaSnPrintfFuncPtr global, LuaSnPrintfFuncPtr instance) {
    print_functions()->g_number2 = global;
    print_functions()->number2 = instance;
}

void sbs_print_set_g_assets_print(LuaSnPrintfFuncPtr global) {
    print_functions()->g_assets = global;
}

void sbs_print_set_assetType_print(LuaSnPrintfFuncPtr instance) {
    print_functions()->assetType = instance;
}

void sbs_print_set_g_palette_print(LuaSnPrintfFuncPtr global) {
    print_functions()->g_palette = global;
}

void sbs_print_set_print_output(LuaPrintOutputFuncPtr f) {
    print_functions()->output = f;
}

void sbs_print_set_g_http_print(LuaSnPrintfFuncPtr printFunc) {
    print_functions()->g_http = printFunc;
}

void sbs_print_set_g_json_print(LuaSnPrintfFuncPtr printFunc) {
    print_functions()->g_json = printFunc;
}

void sbs_print_set_g_url_print(LuaSnPrintfFuncPtr printFunc) {
    print_functions()->g_url = printFunc;
}

void sbs_print_set_file_print(LuaSnPrintfFuncPtr global) {
    print_functions()->g_file = global;
}

void sbs_print_set_physicsMode_print(LuaSnPrintfFuncPtr instance) {
    print_functions()->physicsMode = instance;
}

void sbs_print_set_quad_print(LuaSnPrintfFuncPtr global, LuaSnPrintfFuncPtr instance) {
    print_functions()->g_quad = global;
    print_functions()->quad = instance;
}

void sbs_print_set_animation_print(LuaSnPrintfFuncPtr global, LuaSnPrintfFuncPtr instance) {
    print_functions()->g_animation = global;
    print_functions()->animation = instance;
}

void sbs_print_set_rotation_print(LuaSnPrintfFuncPtr global, LuaSnPrintfFuncPtr instance) {
    print_functions()->g_rotation = global;
    print_functions()->rotation = instance;
}

void sbs_print_set_mesh_print(LuaSnPrintfFuncPtr global, LuaSnPrintfFuncPtr instance) {
    print_functions()->g_mesh = global;
    print_functions()->mesh = instance;
}

int sbs_print(lua_State *L) {
    vx_assert(L != nullptr);
    LUAUTILS_STACK_SIZE(L)

    const size_t maxLen = 1000;
    char* result = static_cast<char*>(malloc(sizeof(char) * (maxLen + 1)));

    int count = lua_gettop(L); // number of arguments

    int n;

    size_t cursor = 0;

    for (int i=1; i <= count; ++i) {
        if (lua_isnil(L, i)) {
            n = snprintf(result+cursor, maxLen-cursor, "%snil", i == 1 ? "" : " ");
            cursor += n;
        } else if (lua_isuserdata(L, i) || lua_istable(L, i)) {
            n = 0;
            const int type = lua_utils_getObjectType(L, i);

            if (type != ITEM_TYPE_NONE) {
                lua_pushvalue(L, i); // put value to print at the top of the stack

                switch (type) {
                    case ITEM_TYPE_BLOCK:
                    {
                        if (print_functions()->block != nullptr) {
                            n = print_functions()->block(L, result+cursor, maxLen-cursor, i > 1);
                        }
                        break;
                    }
                    case ITEM_TYPE_FACE:
                    {
                        if (print_functions()->block != nullptr) {
                            n = print_functions()->block_face(L, result+cursor, maxLen-cursor, i > 1);
                        }
                        break;
                    }
                    case ITEM_TYPE_BLOCKPROPERTIES:
                    {
                        if (print_functions()->block_properties != nullptr) {
                            n = print_functions()->block_properties(L, result+cursor, maxLen-cursor, i > 1);
                        }
                        break;
                    }
                    case ITEM_TYPE_G_BOX:
                    {
                        if (print_functions()->g_box != nullptr) {
                            n = print_functions()->g_box(L, result+cursor, maxLen-cursor, i > 1);
                        }
                        break;
                    }
                    case ITEM_TYPE_BOX:
                    {
                        if (print_functions()->box != nullptr) {
                            n = print_functions()->box(L, result+cursor, maxLen-cursor, i > 1);
                        }
                        break;
                    }
                    case ITEM_TYPE_COLOR:
                    {
                        if (print_functions()->color != nullptr) {
                            n = print_functions()->color(L, result+cursor, maxLen-cursor, i > 1);
                        }
                        break;
                    }
                    case ITEM_TYPE_MAP:
                    {
                        if (print_functions()->map != nullptr) {
                            n = print_functions()->map(L, result+cursor, maxLen-cursor, i > 1);
                        }
                        break;
                    }
                    case ITEM_TYPE_WORLD:
                    {
                        if (print_functions()->world != nullptr) {
                            n = print_functions()->world(L, result+cursor, maxLen-cursor, i > 1);
                        }
                        break;
                    }
                    case ITEM_TYPE_CAMERA:
                    {
                        if (print_functions()->camera != nullptr) {
                            n = print_functions()->camera(L, result + cursor, maxLen - cursor, i > 1);
                        }
                        break;
                    }
                    case ITEM_TYPE_G_NUMBER3:
                    {
                        if (print_functions()->g_number3 != nullptr) {
                            n = print_functions()->g_number3(L, result+cursor, maxLen-cursor, i > 1);
                        }
                        break;
                    }
                    case ITEM_TYPE_G_POINT:
                    {
                        if (print_functions()->g_point != nullptr) {
                            n = print_functions()->g_point(L, result+cursor, maxLen-cursor, i > 1);
                        }
                        break;
                    }
                    case ITEM_TYPE_PLAYER:
                    {
                        if (print_functions()->player != nullptr) {
                            n = print_functions()->player(L, result+cursor, maxLen-cursor, i > 1);
                        }
                        break;
                    }
                    case ITEM_TYPE_NUMBER3:
                    {
                        if (print_functions()->number3 != nullptr) {
                            n = print_functions()->number3(L, result+cursor, maxLen-cursor, i > 1);
                        }
                        break;
                    }
                    case ITEM_TYPE_POINT:
                    {
                        if (print_functions()->point != nullptr) {
                            n = print_functions()->point(L, result+cursor, maxLen-cursor, i > 1);
                        }
                        break;
                    }
                    case ITEM_TYPE_SERVER:
                    {
                        if (print_functions()->server != nullptr) {
                            n = print_functions()->server(L, result+cursor, maxLen-cursor, i > 1);
                        }
                        break;
                    }
                    case ITEM_TYPE_G_CLOUDS:
                    {
                        if (print_functions()->g_clouds != nullptr) {
                            n = print_functions()->g_clouds(L, result+cursor, maxLen-cursor, i > 1);
                        }
                        break;
                    }
                    case ITEM_TYPE_G_FOG:
                    {
                        if (print_functions()->g_fog != nullptr) {
                            n = print_functions()->g_fog(L, result+cursor, maxLen-cursor, i > 1);
                        }
                        break;
                    }
                    case ITEM_TYPE_G_OBJECT: {
                        if (print_functions()->g_object != nullptr) {
                            n = print_functions()->g_object(L, result+cursor, maxLen-cursor, i > 1);
                        }
                        break;
                    }
                    case ITEM_TYPE_OBJECT: {
                        if (print_functions()->object != nullptr) {
                            n = print_functions()->object(L, result+cursor, maxLen-cursor, i > 1);
                        }
                        break;
                    }
                    case ITEM_TYPE_G_PALETTE: {
                        if (print_functions()->g_palette != nullptr) {
                            n = print_functions()->g_palette(L, result+cursor, maxLen-cursor, i > 1);
                        }
                        break;
                    }
                    case ITEM_TYPE_PALETTE: {
                        if (print_functions()->palette != nullptr) {
                            n = print_functions()->palette(L, result+cursor, maxLen-cursor, i > 1);
                        }
                        break;
                    }
                    case ITEM_TYPE_G_RAY: {
                        if (print_functions()->g_ray != nullptr) {
                            n = print_functions()->g_ray(L, result+cursor, maxLen-cursor, i > 1);
                        }
                        break;
                    }
                    case ITEM_TYPE_RAY: {
                        if (print_functions()->ray != nullptr) {
                            n = print_functions()->ray(L, result+cursor, maxLen-cursor, i > 1);
                        }
                        break;
                    }
                    case ITEM_TYPE_G_COLLISIONGROUPS: {
                        if (print_functions()->g_collisionGroups != nullptr) {
                            n = print_functions()->g_collisionGroups(L, result+cursor, maxLen-cursor, i > 1);
                        }
                        break;
                    }
                    case ITEM_TYPE_COLLISIONGROUPS: {
                        if (print_functions()->collisionGroups != nullptr) {
                            n = print_functions()->collisionGroups(L, result+cursor, maxLen-cursor, i > 1);
                        }
                        break;
                    }
                    case ITEM_TYPE_G_SHAPE: {
                        if (print_functions()->g_shape != nullptr) {
                            n = print_functions()->g_shape(L, result+cursor, maxLen-cursor, i > 1);
                        }
                        break;
                    }
                    case ITEM_TYPE_SHAPE: {
                        if (print_functions()->shape != nullptr) {
                            n = print_functions()->shape(L, result+cursor, maxLen-cursor, i > 1);
                        }
                        break;
                    }
                    case ITEM_TYPE_G_MUTABLESHAPE: {
                        if (print_functions()->g_mutableShape != nullptr) {
                            n = print_functions()->g_mutableShape(L, result+cursor, maxLen-cursor, i > 1);
                        }
                        break;
                    }
                    case ITEM_TYPE_MUTABLESHAPE: {
                        if (print_functions()->mutableShape != nullptr) {
                            n = print_functions()->mutableShape(L, result+cursor, maxLen-cursor, i > 1);
                        }
                        break;
                    }
                    case ITEM_TYPE_IMPACT: {
                        if (print_functions()->impact != nullptr) {
                            n = print_functions()->impact(L, result+cursor, maxLen-cursor, i > 1);
                        }
                        break;
                    }
                    case ITEM_TYPE_G_KVSTORE: {
                        if (print_functions()->g_kvstore != nullptr) {
                            n = print_functions()->g_kvstore(L, result+cursor, maxLen-cursor, i > 1);
                        }
                        break;
                    }
                    case ITEM_TYPE_KVSTORE: {
                        if (print_functions()->kvstore != nullptr) {
                            n = print_functions()->kvstore(L, result+cursor, maxLen-cursor, i > 1);
                        }
                        break;
                    }
                    case ITEM_TYPE_G_EVENT: {
                        if (print_functions()->g_event != nullptr) {
                            n = print_functions()->g_event(L, result+cursor, maxLen-cursor, i > 1);
                        }
                        break;
                    }
                    case ITEM_TYPE_EVENT: {
                        if (print_functions()->event != nullptr) {
                            n = print_functions()->event(L, result+cursor, maxLen-cursor, i > 1);
                        }
                        break;
                    }
                    case ITEM_TYPE_G_LIGHT: {
                        if (print_functions()->g_light != nullptr) {
                            n = print_functions()->g_light(L, result+cursor, maxLen-cursor, i > 1);
                        }
                        break;
                    }
                    case ITEM_TYPE_LIGHT: {
                        if (print_functions()->light != nullptr) {
                            n = print_functions()->light(L, result+cursor, maxLen-cursor, i > 1);
                        }
                        break;
                    }
                    case ITEM_TYPE_G_LIGHTTYPE:
                    case ITEM_TYPE_LIGHTTYPE: {
                        if (print_functions()->lightType != nullptr) {
                            n = print_functions()->lightType(L, result+cursor, maxLen-cursor, i > 1);
                        }
                        break;
                    }
                    case ITEM_TYPE_G_JSON: {
                        if (print_functions()->g_json != nullptr) {
                            n = print_functions()->g_json(L, result+cursor, maxLen-cursor, i > 1);
                        }
                        break;
                    }
                    case ITEM_TYPE_G_HTTP: {
                        if (print_functions()->g_http != nullptr) {
                            n = print_functions()->g_http(L, result+cursor, maxLen-cursor, i > 1);
                        }
                        break;
                    }
                    case ITEM_TYPE_G_URL: {
                        if (print_functions()->g_url!= nullptr) {
                            n = print_functions()->g_url(L, result+cursor, maxLen-cursor, i > 1);
                        }
                        break;
                    }
                    case ITEM_TYPE_G_FILE: {
                        if (print_functions()->g_file!= nullptr) {
                            n = print_functions()->g_file(L, result+cursor, maxLen-cursor, i > 1);
                        }
                        break;
                    }
                    case ITEM_TYPE_G_PROJECTIONMODE:
                    case ITEM_TYPE_PROJECTIONMODE: {
                        if (print_functions()->projectionMode != nullptr) {
                            n = print_functions()->projectionMode(L, result+cursor, maxLen-cursor, i > 1);
                        }
                        break;
                    }
                    case ITEM_TYPE_G_TEXT: {
                        if (print_functions()->g_text != nullptr) {
                            n = print_functions()->g_text(L, result + cursor, maxLen - cursor, i > 1);
                        }
                        break;
                    }
                    case ITEM_TYPE_TEXT: {
                        if (print_functions()->text != nullptr) {
                            n = print_functions()->text(L, result + cursor, maxLen - cursor, i > 1);
                        }
                        break;
                    }
                    case ITEM_TYPE_G_TEXTTYPE:
                    case ITEM_TYPE_TEXTTYPE: {
                        if (print_functions()->textType != nullptr) {
                            n = print_functions()->textType(L, result+cursor, maxLen-cursor, i > 1);
                        }
                        break;
                    }
                    case ITEM_TYPE_G_FONT:
                    case ITEM_TYPE_FONT:{
                        if (print_functions()->font != nullptr) {
                            n = print_functions()->font(L, result+cursor, maxLen-cursor, i > 1);
                        }
                        break;
                    }
                    case ITEM_TYPE_G_NUMBER2: {
                        if (print_functions()->g_number2 != nullptr) {
                            n = print_functions()->g_number2(L, result + cursor, maxLen - cursor, i > 1);
                        }
                        break;
                    }
                    case ITEM_TYPE_NUMBER2: {
                        if (print_functions()->number2 != nullptr) {
                            n = print_functions()->number2(L, result + cursor, maxLen - cursor, i > 1);
                        }
                        break;
                    }
                    case ITEM_TYPE_G_ASSETS: {
                        if (print_functions()->g_assets != nullptr) {
                            n = print_functions()->g_assets(L, result+cursor, maxLen-cursor, i > 1);
                        }
                        break;
                    }
                    case ITEM_TYPE_G_ASSETTYPE:
                    case ITEM_TYPE_ASSETTYPE: {
                        if (print_functions()->assetType != nullptr) {
                            n = print_functions()->assetType(L, result+cursor, maxLen-cursor, i > 1);
                        }
                        break;
                    }
                    case ITEM_TYPE_G_PHYSICSMODE:
                    case ITEM_TYPE_PHYSICSMODE: {
                        if (print_functions()->physicsMode != nullptr) {
                            n = print_functions()->physicsMode(L, result+cursor, maxLen-cursor, i > 1);
                        }
                        break;
                    }
                    case ITEM_TYPE_G_QUAD: {
                        if (print_functions()->g_quad != nullptr) {
                            n = print_functions()->g_quad(L, result+cursor, maxLen-cursor, i > 1);
                        }
                        break;
                    }
                    case ITEM_TYPE_QUAD: {
                        if (print_functions()->quad != nullptr) {
                            n = print_functions()->quad(L, result+cursor, maxLen-cursor, i > 1);
                        }
                        break;
                    }
                    case ITEM_TYPE_G_SKY:
                    {
                        if (print_functions()->g_sky != nullptr) {
                            n = print_functions()->g_sky(L, result+cursor, maxLen-cursor, i > 1);
                        }
                        break;
                    }
                    case ITEM_TYPE_G_ANIMATION:
                    {
                        if (print_functions()->g_animation != nullptr) {
                            n = print_functions()->g_animation(L, result+cursor, maxLen-cursor, i > 1);
                        }
                        break;
                    }
                    case ITEM_TYPE_ANIMATION:
                    {
                        if (print_functions()->animation != nullptr) {
                            n = print_functions()->animation(L, result+cursor, maxLen-cursor, i > 1);
                        }
                        break;
                    }
                    case ITEM_TYPE_G_ROTATION: {
                        if (print_functions()->g_rotation != nullptr) {
                            n = print_functions()->g_rotation(L, result+cursor, maxLen-cursor, i > 1);
                        }
                        break;
                    }
                    case ITEM_TYPE_ROTATION: {
                        if (print_functions()->rotation != nullptr) {
                            n = print_functions()->rotation(L, result+cursor, maxLen-cursor, i > 1);
                        }
                        break;
                    }
                    case ITEM_TYPE_G_MESH: {
                        if (print_functions()->g_mesh != nullptr) {
                            n = print_functions()->g_mesh(L, result+cursor, maxLen-cursor, i > 1);
                        }
                        break;
                    }
                    case ITEM_TYPE_MESH: {
                        if (print_functions()->mesh != nullptr) {
                            n = print_functions()->mesh(L, result+cursor, maxLen-cursor, i > 1);
                        }
                        break;
                    }
                    default:
                        break;
                }
                lua_pop(L, 1); // pop value
            }

            // use default print
            if (n == 0) {
                const void* ptr = lua_topointer(L, i);
                n = snprintf(result+cursor, maxLen-cursor, "%s[Table %p]", i == 1 ? "" : " ", ptr);
            }
            cursor += n;
        } else if (lua_isstring(L, i)) {
            const char *s = lua_tostring(L, i);
            n = snprintf(result+cursor, maxLen-cursor, "%s%s", i == 1 ? "" : " ", s);
            cursor += n;
        } else if (lua_isboolean(L, i)) {
            const bool b = lua_toboolean(L, i);
            n = snprintf(result + cursor, maxLen - cursor, "%s%s", i == 1 ? "" : " ", b ? "true" : "false");
            cursor += n;
        } else if (lua_isfunction(L, i)) {
            const void *ptr = lua_topointer(L, i);
            n = snprintf(result + cursor, maxLen - cursor, "%s[Function %p]", i == 1 ? "" : " ", ptr);
            cursor += n;
        }

        if (cursor >= maxLen) { break; }
    }

    if (cursor < maxLen) {
        result[cursor] = 0; // null char at the end
    } else {
        result[cursor - 1] = 0; // null char at the end
    }

    // print_functions()->output should create a copy of the string,
    // therefore we can free result right after this block.
    if (print_functions()->output != nullptr) {
        print_functions()->output(L, result);
    } else { // default print
        vxlog_info("%s", result);
    }
    free(result);

    LUAUTILS_STACK_SIZE_ASSERT(L, 0)
    return 0;
}
