//
//  lua_camera.cpp
//  Cubzh
//
//  Created by Gaetan de Villele on 05/05/2020.
//  Copyright © 2020 Voxowl Inc. All rights reserved.
//

#include "lua_camera.hpp"

// Lua / Sandbox
#include "lua.hpp"
#include "lua_logs.hpp"
#include "lua_object.hpp"
#include "lua_sandbox.hpp"
#include "lua_block.hpp"
#include "lua_constants.h"
#include "lua_player.hpp"
#include "lua_number3.hpp"
#include "lua_number2.hpp"
#include "lua_impact.hpp"
#include "lua_utils.hpp"
#include "lua_type.hpp"
#include "lua_shape.hpp"
#include "lua_mutableShape.hpp"
#include "lua_projectionMode.hpp"
#include "lua_color.hpp"
#include "lua_box.hpp"
#include "lua_ray.hpp"
#include "lua_mesh.hpp"

// xptools
#include "xptools.h"

// vx
#include "VXGameRenderer.hpp"
#include "VXNode.hpp"
#include "VXConfig.hpp"

// engine
#include "camera.h"
#include "game.h"

// Camera instance fields

// Functions
#define LUA_CAMERA_FIELD_FITTOSCREEN           "FitToScreen"
#define LUA_CAMERA_FIELD_SCREENTORAY           "ScreenToRay"
#define LUA_CAMERA_FIELD_WORLDTOSCREEN         "WorldToScreen"

// Obsolete functions
#define LUA_CAMERA_FIELD_SHOW                  "Show"
#define LUA_CAMERA_FIELD_HIDE                  "Hide"

// Functions defined in default scripts
// Legacy camera modes (now using "Behavior")
#define LUA_CAMERA_FIELD_SETMODEFIRSTPERSON    "SetModeFirstPerson"
#define LUA_CAMERA_FIELD_SETMODETHIRDPERSON    "SetModeThirdPerson"
#define LUA_CAMERA_FIELD_SETMODETHIRDPERSONRPG "SetModeThirdPersonRPG"
#define LUA_CAMERA_FIELD_SETMODESATELLITE      "SetModeSatellite"
#define LUA_CAMERA_FIELD_SETMODEFREE           "SetModeFree"

// Allows to define how the camera behaves in a declarative way
// setting camera.Behavior = nil unsets it
#define LUA_CAMERA_FIELD_BEHAVIOR "Behavior"

#define LUA_CAMERA_FIELD_CASTRAY               "CastRay"

// Properties
#define LUA_CAMERA_FIELD_FOV                   "FieldOfView"           // number
#define LUA_CAMERA_FIELD_FOV2                  "FOV"                   // number
#define LUA_CAMERA_FIELD_VFOV                  "VerticalFOV"           // number (read-only)
#define LUA_CAMERA_FIELD_HFOV                  "HorizontalFOV"         // number (read-only)
#define LUA_CAMERA_FIELD_LAYERS                "Layers"                // table of integers, or single integer
#define LUA_CAMERA_FIELD_WIDTH                 "Width"                 // number
#define LUA_CAMERA_FIELD_HEIGHT                "Height"                // number
#define LUA_CAMERA_FIELD_TARGET_X              "TargetX"               // number
#define LUA_CAMERA_FIELD_TARGET_Y              "TargetY"               // number
#define LUA_CAMERA_FIELD_TARGET_WIDTH          "TargetWidth"           // number
#define LUA_CAMERA_FIELD_TARGET_HEIGHT         "TargetHeight"          // number
#define LUA_CAMERA_FIELD_NEAR                  "Near"                  // number
#define LUA_CAMERA_FIELD_FAR                   "Far"                   // number
#define LUA_CAMERA_FIELD_MODE                  "Projection"            // enum
#define LUA_CAMERA_FIELD_COLOR                 "Color"                 // Color
#define LUA_CAMERA_FIELD_ALPHA                 "Alpha"                 // number
#define LUA_CAMERA_FIELD_ORDER                 "ViewOrder"             // number
#define LUA_CAMERA_FIELD_ENABLED               "On"                    // boolean

typedef struct camera_userdata {
    // Userdata instance requirements
    vx::UserDataStore *store;
    camera_userdata *next;
    camera_userdata *previous;

    Camera *camera;
} camera_userdata;

// MARK: Metatable

static int _g_camera_metatable_index(lua_State *L);
static int _g_camera_metatable_newindex(lua_State *L);
static int _g_camera_metatable_call(lua_State *L);
static int _camera_metatable_index(lua_State *L);
static int _camera_metatable_newindex(lua_State *L);
static void _camera_gc(void *_ud);

// MARK: Lua fields

void _camera_set_color_handler(const uint8_t *r, const uint8_t *g, const uint8_t *b, const uint8_t *a,
                               const bool *light, lua_State *L, void *userdata);
void _camera_get_color_handler(uint8_t *r, uint8_t *g, uint8_t *b, uint8_t *a, bool *light, lua_State *L,
                               const void *userdata);
void _camera_color_handler_free_userdata(void *userdata);

// MARK: Lua functions

// MARK: Internal

Camera* _lua_camera_getCameraFromGame(lua_State * const L, CGame **g = nullptr);
int _lua_camera_FitToScreen(lua_State * const L);
int _lua_camera_ScreenToRay(lua_State * const L);
int _lua_camera_WorldToScreen(lua_State * const L);
bool _lua_g_camera_set_func(lua_State * const L, const char *key);
void _lua_camera_get_and_push_global_func(lua_State * const L, const char *key);

// MARK: - Camera global table -

void lua_g_camera_createAndPush(lua_State * const L) {
    vx_assert(L != nullptr);
    LUAUTILS_STACK_SIZE(L)

    vx::Game *g = nullptr;
    if (lua_utils_getGameCppWrapperPtr(L, &g) == false) {
        LUAUTILS_INTERNAL_ERROR(L);
    }

    Camera *c = game_get_camera(g->getCGame());

    if (c == nullptr) {
        lua_pushstring(L, "failed to get main camera");
        LUAUTILS_INTERNAL_ERROR(L)
    }

    // global "Camera"
    // not linking destructor nor adding to store as default camera is released on game destruction.
    camera_userdata *ud = static_cast<camera_userdata*>(lua_newuserdata(L, sizeof(camera_userdata)));
    ud->camera = c;

    lua_newtable(L); // metatable
    {
        lua_pushstring(L, "__index");
        lua_pushcfunction(L, _g_camera_metatable_index, "");
        lua_rawset(L, -3);

        lua_pushstring(L, "__newindex");
        lua_pushcfunction(L, _g_camera_metatable_newindex, "");
        lua_rawset(L, -3);

        lua_pushstring(L, "__call");
        lua_pushcfunction(L, _g_camera_metatable_call, "");
        lua_rawset(L, -3);

        // hide the metatable from the Lua sandbox user
        lua_pushstring(L, "__metatable");
        lua_pushboolean(L, false);
        lua_rawset(L, -3);

        lua_pushstring(L, "__type");
        lua_pushstring(L, "Camera");
        lua_rawset(L, -3);

        lua_pushstring(L, "__typen");
        lua_pushinteger(L, ITEM_TYPE_CAMERA);
        lua_rawset(L, -3);

        // private fields
        _lua_camera_push_fields(L, c);
    }
    lua_setmetatable(L, -2);

    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
}

// MARK: - Camera instances tables -

bool lua_camera_pushNewInstance(lua_State * const L, Camera *c) {
    vx_assert(L != nullptr);
    vx_assert(c != nullptr);

    Transform *t = camera_get_view_transform(c);

    // lua Camera takes ownership
    if (transform_retain(t) == false) {
        LUAUTILS_INTERNAL_ERROR(L);
        // return false;
    }

    LUAUTILS_STACK_SIZE(L)

    vx::Game *g = nullptr;
    if (lua_utils_getGameCppWrapperPtr(L, &g) == false) {
        LUAUTILS_INTERNAL_ERROR(L);
    }

    camera_userdata *ud = static_cast<camera_userdata*>(lua_newuserdatadtor(L, sizeof(camera_userdata), _camera_gc));
    ud->camera = c;

    // connect to userdata store
    ud->store = g->userdataStoreForCameras;
    ud->previous = nullptr;
    camera_userdata* next = static_cast<camera_userdata*>(ud->store->add(ud));
    ud->next = next;
    if (next != nullptr) {
        next->previous = ud;
    }

    lua_newtable(L); // metatable
    {
        lua_pushstring(L, "__index");
        lua_pushcfunction(L, _camera_metatable_index, "");
        lua_rawset(L, -3);

        lua_pushstring(L, "__newindex");
        lua_pushcfunction(L, _camera_metatable_newindex, "");
        lua_rawset(L, -3);

        // hide the metatable from the Lua sandbox user
        lua_pushstring(L, "__metatable");
        lua_pushboolean(L, false);
        lua_rawset(L, -3);

        lua_pushstring(L, "__type");
        lua_pushstring(L, "Camera");
        lua_rawset(L, -3);

        lua_pushstring(L, "__typen");
        lua_pushinteger(L, ITEM_TYPE_CAMERA);
        lua_rawset(L, -3);

        // private fields
        _lua_camera_push_fields(L, c); // TODO: store in registry when accessed
    }
    lua_setmetatable(L, -2);

    if (lua_g_object_addIndexEntry(L, t, -1) == false) {
        vxlog_error("Failed to add Lua Camera to Object registry");
        lua_pop(L, 1); // pop Lua Camera
        LUAUTILS_STACK_SIZE_ASSERT(L, 0)
        return false;
    }

    scene_register_managed_transform(game_get_scene(g->getCGame()), t);

    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
    return true;
}

Camera *lua_camera_getCameraPtr(lua_State * const L, const int idx) {
    vx_assert(L != nullptr);
    LUAUTILS_STACK_SIZE(L)
    // The object in the Lua stack is a Camera, get the camera ptr directly
    if (lua_utils_getObjectType(L, idx) == ITEM_TYPE_CAMERA) {
        camera_userdata *ud = static_cast<camera_userdata*>(lua_touserdata(L, idx));
        return ud->camera;
    }
    LUAUTILS_STACK_SIZE_ASSERT(L, 0)
    return nullptr;
}

int lua_camera_snprintf(lua_State * const L, char *result, size_t maxLen, bool spacePrefix) {
    RETURN_VALUE_IF_NULL(L, 0)
    Transform *root; lua_object_getTransformPtr(L, -1, &root);
    if (root == nullptr) {
        return snprintf(result, maxLen, "%s[Camera (destroyed)]", spacePrefix ? " " : "");
    }
    return snprintf(result, maxLen, "%s[Camera %d Name:%s] (instance)", spacePrefix ? " " : "",
                    transform_get_id(root), transform_get_name(root));
}

// MARK: - Exposed internal functions -

void _lua_camera_push_fields(lua_State * const L, Camera *c) {
    uint8_t rgba[4]; utils_rgba_to_uint8(camera_get_color(c), rgba);

    lua_pushstring(L, LUA_CAMERA_FIELD_COLOR);
    lua_color_create_and_push_table(L, rgba[0], rgba[1], rgba[2], rgba[3], false, false);

    lua_color_setHandlers(L, -1,
                          _camera_set_color_handler,
                          _camera_get_color_handler,
                          _camera_color_handler_free_userdata,
                          transform_get_and_retain_weakptr(camera_get_view_transform(c)));
    lua_rawset(L, -3);
}

void _lua_camera_update_fields(lua_State * const L, Camera *c) {
    lua_getfield(L, -1, LUA_CAMERA_FIELD_COLOR);
    lua_color_setHandlers(L, -1,
                          _camera_set_color_handler,
                          _camera_get_color_handler,
                          _camera_color_handler_free_userdata,
                          transform_get_and_retain_weakptr(camera_get_view_transform(c)));
    lua_pop(L, 1);
}

bool _lua_camera_metatable_index(lua_State * const L, Camera *c, const char *key) {
    // Functions defined in default script are set onto Camera global table
    if (strcmp(key, LUA_CAMERA_FIELD_SETMODEFIRSTPERSON) == 0 ||
        strcmp(key, LUA_CAMERA_FIELD_SETMODETHIRDPERSON) == 0 ||
        strcmp(key, LUA_CAMERA_FIELD_SETMODETHIRDPERSONRPG) == 0 ||
        strcmp(key, LUA_CAMERA_FIELD_SETMODESATELLITE) == 0 ||
        strcmp(key, LUA_CAMERA_FIELD_SETMODEFREE) == 0 ||
        strcmp(key, LUA_CAMERA_FIELD_CASTRAY) == 0 ) {
        _lua_camera_get_and_push_global_func(L, key);
        return true;
    }

    // Obsolete functions
    else if (strcmp(key, LUA_CAMERA_FIELD_SHOW) == 0) {
        LUAUTILS_ERROR(L, "Camera:Show(object) is obsolete, use Camera.On instead")
    } else if (strcmp(key, LUA_CAMERA_FIELD_HIDE) == 0) {
        LUAUTILS_ERROR(L, "Camera:Hide(object) is obsolete, use Camera.On instead")
    }

    // Functions & fields from Camera instance
    else if (strcmp(key, LUA_CAMERA_FIELD_FITTOSCREEN) == 0) {
        lua_pushcfunction(L, _lua_camera_FitToScreen, "");
        return true;
    } else if (strcmp(key, LUA_CAMERA_FIELD_SCREENTORAY) == 0) {
        lua_pushcfunction(L, _lua_camera_ScreenToRay, "");
        return true;
    } else if (strcmp(key, LUA_CAMERA_FIELD_WORLDTOSCREEN) == 0) {
        lua_pushcfunction(L, _lua_camera_WorldToScreen, "");
        return true;
    } else if (strcmp(key, LUA_CAMERA_FIELD_FOV) == 0 || strcmp(key, LUA_CAMERA_FIELD_FOV2) == 0) {
        lua_pushnumber(L, static_cast<double>(camera_get_fov(c)));
        return true;
    } else if (strcmp(key, LUA_CAMERA_FIELD_VFOV) == 0) {
        const float vfov_rad = camera_utils_get_vertical_fov2(c, vx::Screen::widthInPoints, vx::Screen::heightInPoints);
        lua_pushnumber(L, static_cast<double>(utils_rad2Deg(vfov_rad)));
        return true;
    } else if (strcmp(key, LUA_CAMERA_FIELD_HFOV) == 0) {
        const float hfov_rad = camera_utils_get_horizontal_fov2(c, vx::Screen::widthInPoints, vx::Screen::heightInPoints);
        lua_pushnumber(L, static_cast<double>(utils_rad2Deg(hfov_rad)));
        return true;
    } else if (strcmp(key, LUA_CAMERA_FIELD_LAYERS) == 0) {
        lua_utils_pushMaskAsTable(L, camera_get_layers(c), CAMERA_LAYERS_MASK_API_BITS);
        return true;
    } else if (strcmp(key, LUA_CAMERA_FIELD_WIDTH) == 0) {
        lua_pushnumber(L, static_cast<double>(camera_get_width(c)));
        return true;
    } else if (strcmp(key, LUA_CAMERA_FIELD_HEIGHT) == 0) {
        lua_pushnumber(L, static_cast<double>(camera_get_height(c)));
            return true;
    } else if (strcmp(key, LUA_CAMERA_FIELD_TARGET_X) == 0) {
        lua_pushnumber(L, static_cast<double>(camera_get_target_x(c)));
        return true;
    } else if (strcmp(key, LUA_CAMERA_FIELD_TARGET_Y) == 0) {
        lua_pushnumber(L, static_cast<double>(camera_get_target_y(c)));
        return true;
    } else if (strcmp(key, LUA_CAMERA_FIELD_TARGET_WIDTH) == 0) {
        lua_pushnumber(L, static_cast<double>(camera_get_target_width(c)));
        return true;
    } else if (strcmp(key, LUA_CAMERA_FIELD_TARGET_HEIGHT) == 0) {
        lua_pushnumber(L, static_cast<double>(camera_get_target_height(c)));
        return true;
    } else if (strcmp(key, LUA_CAMERA_FIELD_NEAR) == 0) {
        lua_pushnumber(L, static_cast<double>(camera_get_near(c)));
        return true;
    } else if (strcmp(key, LUA_CAMERA_FIELD_FAR) == 0) {
        lua_pushnumber(L, static_cast<double>(camera_get_far(c)));
        return true;
    } else if (strcmp(key, LUA_CAMERA_FIELD_MODE) == 0) {
        lua_projectionMode_pushTable(L, camera_get_mode(c));
        return true;
    } else if (strcmp(key, LUA_CAMERA_FIELD_COLOR) == 0) {
        LUA_GET_METAFIELD_AND_RETURN_TYPE_PUSHING_NIL_IF_NOT_FOUND(L, 1, LUA_CAMERA_FIELD_COLOR);
        return true;
    } else if (strcmp(key, LUA_CAMERA_FIELD_ALPHA) == 0) {
        lua_pushnumber(L, static_cast<double>(camera_get_alpha_f(c)));
        return true;
    } else if (strcmp(key, LUA_CAMERA_FIELD_ORDER) == 0) {
        lua_pushinteger(L, camera_get_order(c));
        return true;
    } else if (strcmp(key, LUA_CAMERA_FIELD_ENABLED) == 0) {
        lua_pushboolean(L, camera_is_enabled(c));
        return true;
    }
    return false;
}

bool _lua_camera_metatable_newindex(lua_State * const L, Camera *c, const char *key) {
    if (strcmp(key, LUA_CAMERA_FIELD_FOV) == 0 || strcmp(key, LUA_CAMERA_FIELD_FOV2) == 0) {
        if (lua_isnumber(L, 3) == false) {
            LUAUTILS_ERROR(L, "Camera.FieldOfView must be a number")
        }
        camera_set_fov(c, lua_tonumber(L, 3));
        return true;
    } else if (strcmp(key, LUA_CAMERA_FIELD_LAYERS) == 0) {
        uint16_t mask = 0;
        if (lua_utils_getMask(L, 3, &mask, CAMERA_LAYERS_MASK_API_BITS) == false) {
            LUAUTILS_ERROR_F(L, "Camera.%s cannot be set (new value should be one or a table of integers between 1 and %d)", key, CAMERA_LAYERS_MASK_API_BITS)
        }
        camera_set_layers(c, mask);
        return true;
    } else if (strcmp(key, LUA_CAMERA_FIELD_WIDTH) == 0) {
        if (lua_isnumber(L, 3) == false) {
            LUAUTILS_ERROR(L, "Camera.Width must be a number")
        }
        const float width = lua_tonumber(L, 3);
        if (width <= 0.0f) {
            camera_toggle_auto_projection_width(c, true);
        } else {
            camera_set_width(c, width, true);
            camera_toggle_auto_projection_width(c, false);
        }
        return true;
    } else if (strcmp(key, LUA_CAMERA_FIELD_HEIGHT) == 0) {
        if (lua_isnumber(L, 3) == false) {
            LUAUTILS_ERROR(L, "Camera.Height must be a number")
        }
        const float height = lua_tonumber(L, 3);
        if (height <= 0.0f) {
            camera_toggle_auto_projection_height(c, true);
        } else {
            camera_set_height(c, height, true);
            camera_toggle_auto_projection_height(c, false);
        }
        return true;
    } else if (strcmp(key, LUA_CAMERA_FIELD_TARGET_X) == 0) {
        if (lua_isnumber(L, 3) == false) {
            LUAUTILS_ERROR(L, "Camera.TargetX must be a number")
        }
        camera_set_target_x(c, lua_tonumber(L, 3));
        return true;
    } else if (strcmp(key, LUA_CAMERA_FIELD_TARGET_Y) == 0) {
        if (lua_isnumber(L, 3) == false) {
            LUAUTILS_ERROR(L, "Camera.TargetY must be a number")
        }
        camera_set_target_y(c, lua_tonumber(L, 3));
        return true;
    } else if (strcmp(key, LUA_CAMERA_FIELD_TARGET_WIDTH) == 0) {
        if (lua_isnumber(L, 3) == false) {
            LUAUTILS_ERROR(L, "Camera.TargetWidth must be a number")
        }
        camera_set_target_width(c, lua_tonumber(L, 3));
        return true;
    } else if (strcmp(key, LUA_CAMERA_FIELD_TARGET_HEIGHT) == 0) {
        if (lua_isnumber(L, 3) == false) {
            LUAUTILS_ERROR(L, "Camera.TargetHeight must be a number")
        }
        camera_set_target_height(c, lua_tonumber(L, 3));
        return true;
    } else if (strcmp(key, LUA_CAMERA_FIELD_NEAR) == 0) {
        if (lua_isnumber(L, 3) == false) {
            LUAUTILS_ERROR(L, "Camera.Near must be a number")
        }
        camera_set_near(c, lua_tonumber(L, 3));
        return true;
    } else if (strcmp(key, LUA_CAMERA_FIELD_FAR) == 0) {
        if (lua_isnumber(L, 3) == false) {
            LUAUTILS_ERROR(L, "Camera.Far must be a number")
        }
        camera_set_far(c, lua_tonumber(L, 3));
        return true;
    } else if (strcmp(key, LUA_CAMERA_FIELD_MODE) == 0) {
        if (lua_utils_getObjectType(L, 3) != ITEM_TYPE_PROJECTIONMODE) {
            LUAUTILS_ERROR(L, "Camera.Projection must be a ProjectionMode")
        }
        camera_set_mode(c, lua_projectionMode_get(L, 3));
        return true;
    } else if (strcmp(key, LUA_CAMERA_FIELD_COLOR) == 0) {
        if (lua_utils_getObjectType(L, 3) != ITEM_TYPE_COLOR) {
            LUAUTILS_ERROR(L, "Camera.Color must be a Color")
        }

        LUAUTILS_STACK_SIZE(L)
        {
            if (LUA_GET_METAFIELD_AND_RETURN_TYPE(L, 1, LUA_CAMERA_FIELD_COLOR) == LUA_TNIL) {
                LUAUTILS_INTERNAL_ERROR(L); // returns
            }

            uint8_t cr, cg, cb, ca;
            lua_color_getRGBAL(L, 3, &cr, &cg, &cb, &ca, nullptr);
            lua_color_setRGBAL(L, -1, &cr, &cg, &cb, &ca, nullptr);
            lua_pop(L, 1); // pop Color
        }
        LUAUTILS_STACK_SIZE_ASSERT(L, 0)

        return true;
    } else if (strcmp(key, LUA_CAMERA_FIELD_ALPHA) == 0) {
        if (lua_isnumber(L, 3) == false) {
            LUAUTILS_ERROR(L, "Camera.Alpha must be a number")
        }
        camera_set_alpha_f(c, lua_tonumber(L, 3));
        return true;
    } else if (strcmp(key, LUA_CAMERA_FIELD_ORDER) == 0) {
        if (lua_utils_isIntegerStrict(L, 3) == false) {
            LUAUTILS_ERROR(L, "Camera.ViewOrder must be an integer between 1 and 255")
        }
        const int order = static_cast<int>(lua_tointeger(L, 3));
        if (order < CAMERA_ORDER_MIN || order > CAMERA_ORDER_MAX) {
            LUAUTILS_ERROR(L, "Camera.ViewOrder must be an integer between 1 and 255")
        }
        camera_set_order(c, order);
        return true;
    } else if (strcmp(key, LUA_CAMERA_FIELD_ENABLED) == 0) {
        if (lua_isboolean(L, 3) == false) {
            LUAUTILS_ERROR(L, "Camera.On must be a boolean")
        }
        camera_set_enabled(c, lua_toboolean(L, 3));
        return true;
    } else if (strcmp(key, LUA_CAMERA_FIELD_BEHAVIOR) == 0) {
        if (lua_isnil(L, 3)) {
            std::string s = R"""(
return function(camera)
    require("camera"):unsetBehavior(camera)
end
)""";

            if (luau_loadstring(L, s, "load Camera.Behavior setter") == false) {
                std::string err = lua_tostring(L, -1);
                lua_pop(L, 1);
                LUAUTILS_ERROR_F(L, "%s", err.c_str());
            }

            if (lua_pcall(L, 0, LUA_MULTRET, 0) != 0) {
                std::string err = "";
                if (lua_isstring(L, -1)) {
                    err = lua_tostring(L, -1);
                } else {
                    err = "unspecified error";
                }
                lua_pop(L, 1);
                LUAUTILS_ERROR_F(L, "%s", err.c_str());
            }

            // function now at -1
            lua_pushvalue(L, 1); // push camera (first parameter)

            if (lua_pcall(L, 1, LUA_MULTRET, 0) != 0) {
                std::string err = "";
                if (lua_isstring(L, -1)) {
                    err = lua_tostring(L, -1);
                } else {
                    err = "unspecified error";
                }
                lua_log_error_CStr(L, err.c_str());
                lua_pop(L, 1);
                LUAUTILS_ERROR_F(L, "%s", err.c_str());
            }

        } else {

            std::string s = R"""(
return function(camera, behavior)
    behavior.camera = camera
    require("camera"):setBehavior(behavior)
end
)""";

            if (luau_loadstring(L, s, "load Camera.Behavior setter") == false) {
                std::string err = lua_tostring(L, -1);
                lua_pop(L, 1);
                LUAUTILS_ERROR_F(L, "%s", err.c_str());
            }

            if (lua_pcall(L, 0, LUA_MULTRET, 0) != 0) {
                std::string err = "";
                if (lua_isstring(L, -1)) {
                    err = lua_tostring(L, -1);
                } else {
                    err = "unspecified error";
                }
                lua_pop(L, 1);
                LUAUTILS_ERROR_F(L, "%s", err.c_str());
            }

            // function now at -1
            lua_pushvalue(L, 1); // push camera (first parameter)
            lua_pushvalue(L, 3); // push behavior (second parameter)

            if (lua_pcall(L, 2, LUA_MULTRET, 0) != 0) {
                std::string err = "";
                if (lua_isstring(L, -1)) {
                    err = lua_tostring(L, -1);
                } else {
                    err = "unspecified error";
                }
                lua_log_error_CStr(L, err.c_str());
                lua_pop(L, 1);
                LUAUTILS_ERROR_F(L, "%s", err.c_str());
            }
        }

        return true;
    }
    return false;
}

// MARK: - Private functions -

static int _g_camera_metatable_index(lua_State *L) {
    return _camera_metatable_index(L);
}

static int _g_camera_metatable_newindex(lua_State *L) {
    vx_assert(L != nullptr);
    LUAUTILS_ASSERT_ARGS_COUNT(L, 3)
    LUAUTILS_STACK_SIZE(L)

    // retrieve underlying Camera struct
    Camera *c = lua_camera_getCameraPtr(L, 1);
    if (c == nullptr) {
        lua_pushnil(L);
        LUAUTILS_STACK_SIZE_ASSERT(L, 1)
        return 1;
    }

    const char *key = lua_tostring(L, 2);

    // Functions defined in default script are set onto Camera global table
    bool done = false;

    if (strcmp(key, LUA_CAMERA_FIELD_SETMODEFIRSTPERSON) == 0 ||
        strcmp(key, LUA_CAMERA_FIELD_SETMODETHIRDPERSON) == 0 ||
        strcmp(key, LUA_CAMERA_FIELD_SETMODETHIRDPERSONRPG) == 0 ||
        strcmp(key, LUA_CAMERA_FIELD_SETMODESATELLITE) == 0 ||
        strcmp(key, LUA_CAMERA_FIELD_SETMODEFREE) == 0 ||
        strcmp(key, LUA_CAMERA_FIELD_CASTRAY) == 0) {
        if (_lua_g_camera_set_func(L, key) == false) {
            LUAUTILS_INTERNAL_ERROR(L)
        }
        done = true;
    }

    if (done == false && _lua_object_metatable_newindex(L, camera_get_view_transform(c), key) == false) {
        // the key is not from Object
        if (_lua_camera_metatable_newindex(L, c, key) == false) {
            // key not found
            LUAUTILS_ERROR_F(L, "Camera: %s field is not settable", key)
        }
    }

    LUAUTILS_STACK_SIZE_ASSERT(L, 0)
    return 0;
}

static int _g_camera_metatable_call(lua_State *L) {
    RETURN_VALUE_IF_NULL(L, 0)
    LUAUTILS_STACK_SIZE(L)

    Camera *c = camera_new();
    if (lua_camera_pushNewInstance(L, c) == false) {
        lua_pushnil(L); // GC will release camera
    }
    camera_release(c); // now owned by lua Camera

    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
    return 1;
}

static int _camera_metatable_index(lua_State *L) {
    vx_assert(L != nullptr);
    LUAUTILS_ASSERT_ARGS_COUNT(L, 2)
    LUAUTILS_ASSERT_ARG_TYPE(L, 1, ITEM_TYPE_CAMERA)
    LUAUTILS_STACK_SIZE(L)

    const char *key = lua_tostring(L, 2);
    if (_lua_object_metatable_index(L, key) == false) {
        // no field found in Object
        // retrieve underlying Camera struct
        Camera *c = lua_camera_getCameraPtr(L, 1);
        if (c == nullptr) {
            lua_pushnil(L);
        } else if (_lua_camera_metatable_index(L, c, key) == false) {
            // key is not known and starts with an uppercase letter
            lua_pushnil(L);
        }
    }

    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
    return 1;
}

static int _camera_metatable_newindex(lua_State *L) {
    vx_assert(L != nullptr);
    LUAUTILS_ASSERT_ARGS_COUNT(L, 3)
    LUAUTILS_ASSERT_ARG_TYPE(L, 1, ITEM_TYPE_CAMERA)
    LUAUTILS_STACK_SIZE(L)

    // retrieve underlying Camera struct
    Camera *c = lua_camera_getCameraPtr(L, 1);
    if (c == nullptr) {
        lua_pushnil(L);
        LUAUTILS_STACK_SIZE_ASSERT(L, 1)
        return 1;
    }

    const char *key = lua_tostring(L, 2);
    if (_lua_object_metatable_newindex(L, camera_get_view_transform(c), key) == false) {
        // the key is not from Object
        if (_lua_camera_metatable_newindex(L, c, key) == false) {
            // key not found
            LUAUTILS_ERROR_F(L, "Camera: %s field is not settable", key)
        }
    }

    LUAUTILS_STACK_SIZE_ASSERT(L, 0)
    return 0;
}

static void _camera_gc(void *_ud) {
    camera_userdata *ud = static_cast<camera_userdata*>(_ud);
    Camera *c = ud->camera;
    if (c != nullptr) {
        camera_release(c);
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

void _camera_set_color_handler(const uint8_t *r, const uint8_t *g, const uint8_t *b, const uint8_t *a,
                               const bool *light, lua_State *L, void *userdata) {
    Transform *t = static_cast<Transform *>(weakptr_get(static_cast<Weakptr*>(userdata)));
    if (t == nullptr) {
        LUAUTILS_ERROR(L, "[Camera.Color] original Camera does not exist anymore")
    }
    Camera *c = static_cast<Camera *>(transform_get_ptr(t));

    uint8_t rgba[4]; utils_rgba_to_uint8(camera_get_color(c), rgba);
    if (r != nullptr) { rgba[0] = *r; }
    if (g != nullptr) { rgba[1] = *g; }
    if (b != nullptr) { rgba[2] = *b; }
    if (a != nullptr) { rgba[3] = *a; }
    camera_set_color(c, rgba[0], rgba[1], rgba[2], rgba[3]);
}

void _camera_get_color_handler(uint8_t *r, uint8_t *g, uint8_t *b, uint8_t *a, bool *light, lua_State *L,
                               const void *userdata) {
    Transform *t = static_cast<Transform *>(weakptr_get(static_cast<const Weakptr*>(userdata)));
    if (t == nullptr) {
        LUAUTILS_ERROR(L, "[Camera.Color] original Camera does not exist anymore")
    }
    Camera *c = static_cast<Camera *>(transform_get_ptr(t));

    uint8_t rgba[4]; utils_rgba_to_uint8(camera_get_color(c), rgba);
    if (r != nullptr) { *r = rgba[0]; }
    if (g != nullptr) { *g = rgba[1]; }
    if (b != nullptr) { *b = rgba[2]; }
    if (a != nullptr) { *a = rgba[3]; }

    // unused by Camera color
    if (light != nullptr) { *light = false; }
}

void _camera_color_handler_free_userdata(void *userdata) {
    weakptr_release(static_cast<Weakptr*>(userdata));
}

Camera* _lua_camera_getCameraFromGame(lua_State * const L, CGame **g) {
    CGame *game = nullptr;
    if (lua_utils_getGamePtr(L, &game) == false || game == nullptr) {
        return nullptr;
    }
    if (g != nullptr) {
        *g = game;
    }
    return game_get_camera(game);
}

/// Camera:FitToScreen(target, coverage)
/// Camera:FitToScreen(target, { coverage=n, mode=string, orientation=string })
/// - target: Shape, Mesh or Box
/// - coverage: 1.0 by default, use this to adjust the fit, increase/decrease to zoom/unzoom respectively
/// - mode: UNDOCUMENTED, passing "box" will force the use of box-fit for perspective cameras
///     TODO: if we make this official, we can allow for "sphere"/"box" to force a fit form regardless of camera projection,
///         by default (and currently), perspective uses sphere, and ortho uses box
/// - orientation: forces one orientation instead of choosing limiting dimension
int _lua_camera_FitToScreen(lua_State * const L) {
    RETURN_VALUE_IF_NULL(L, 0)
    LUAUTILS_STACK_SIZE(L)

    const int argCount = lua_gettop(L);

    if (argCount < 1) {
        LUAUTILS_ERROR(L, "Camera:FitToScreen(target[, options]) should be called with `:`");
    }

    Camera *c = lua_camera_getCameraPtr(L, 1);
    if (c == nullptr) {
        LUAUTILS_ERROR(L, "Camera:FitToScreen(target[, options]) should be called with `:`");
    }

    if (argCount < 2) {
        LUAUTILS_ERROR(L, "Camera:FitToScreen(target[, options]) - missing target parameter (Shape or Box)");
    }

    // get target (Shape, Mesh or Box)
    Box box;
    if (lua_shape_isShape(L, 2)) {
        Shape *s = lua_shape_getShapePtr(L, 2);
        if (s == nullptr) {
            LUAUTILS_INTERNAL_ERROR(L)
        }

        shape_apply_current_transaction(s, true);
        shape_get_world_aabb(s, &box, true);
    } else if (lua_mesh_isMesh(L, 2)) {
        Mesh *m = lua_mesh_getMeshPtr(L, 2);
        if (m == nullptr) {
            LUAUTILS_INTERNAL_ERROR(L)
        }

        mesh_get_world_aabb(m, &box, true);
    } else if (lua_box_get_box(L, 2, &box) == false) {
        LUAUTILS_ERROR(L, "Camera:FitToScreen(target[, options]) - target should be a Shape or a Box");
    }

    // optional parameters
    float coverage = 1.0f;
    bool forceBox = false;
    FitToScreen_Orientation forceOrientation = FitToScreen_Minimum;
    if (argCount >= 3) {
        if (lua_isnumber(L, 3)) { // optional coverage (number)
            coverage = lua_tonumber(L, 3);
        } else if (lua_istable(L, 3)) { // optional parameters table
            int type = lua_getfield(L, 3, "coverage");
            if (type == LUA_TNIL) {
                lua_pop(L, 1);
            } else if (type == LUA_TNUMBER) {
                coverage = lua_tonumber(L, -1);
                lua_pop(L, 1);
            } else {
                LUAUTILS_ERROR(L, "Camera:FitToScreen(target[, options]) - options.coverage should be a number");
            }

            type = lua_getfield(L, 3, "mode");
            if (type == LUA_TNIL) {
                lua_pop(L, 1);
            } else if (type == LUA_TSTRING) {
                if (strcmp(lua_tostring(L, -1), "box") == 0) {
                    forceBox = true;
                } else {
                    LUAUTILS_ERROR(L, "Camera:FitToScreen(target[, options]) - options.mode possible value: 'box'");
                }
                lua_pop(L, 1);
            } else {
                LUAUTILS_ERROR(L, "Camera:FitToScreen(target[, options]) - options.mode should be a string");
            }

            type = lua_getfield(L, 3, "orientation");
            if (type == LUA_TNIL) {
                lua_pop(L, 1);
            } else if (type == LUA_TSTRING) {
                if (strcmp(lua_tostring(L, -1), "vertical") == 0) {
                    forceOrientation = FitToScreen_Vertical;
                } else if (strcmp(lua_tostring(L, -1), "horizontal") == 0) {
                    forceOrientation = FitToScreen_Horizontal;
                } else {
                    LUAUTILS_ERROR(L, "Camera:FitToScreen(target[, options]) - options.orientation possible values: 'vertical', 'horizontal'");
                }
                lua_pop(L, 1);
            } else {
                LUAUTILS_ERROR(L, "Camera:FitToScreen(target[, options]) - options.orientation should be a string");
            }
        }
    }

    camera_utils_apply_fit_to_screen(c, &box, coverage,
                                     vx::Screen::widthInPoints,
                                     vx::Screen::heightInPoints,
                                     forceBox, forceOrientation);

    LUAUTILS_STACK_SIZE_ASSERT(L, 0)
    return 0;
}

/// Camera:ScreenToRay(screenPos)
/// - screenPos: Number2 or table of numbers, normalized screen coordinates
int _lua_camera_ScreenToRay(lua_State * const L) {
    RETURN_VALUE_IF_NULL(L, 0)
    LUAUTILS_STACK_SIZE(L)

    Camera *c = lua_camera_getCameraPtr(L, 1);
    if (c == nullptr) {
        LUAUTILS_ERROR(L, "[Camera:ScreenToRay] incorrect caller, expected Camera")
    }

    float x, y;
    if (lua_number2_or_table_or_numbers_getXY(L, 2, &x, &y) == false) {
        LUAUTILS_ERROR(L, "[Camera:ScreenToRay] argument should be either a Number2, a table of 2 numbers, or 2 individual numbers");
    }

    float3 o, v; camera_unorm_screen_to_ray(c, x, y, &o, &v);
    lua_ray_create_and_push(L, o, v);

    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
    return 1;
}

/// Camera:WorldToScreen(worldPos)
/// - worldPos: Number3 or table of numbers
/// returns the corresponding screen point (Number2) or nil if not within view
int _lua_camera_WorldToScreen(lua_State * const L) {
    RETURN_VALUE_IF_NULL(L, 0)
    LUAUTILS_STACK_SIZE(L)

    Camera *c = lua_camera_getCameraPtr(L, 1);
    if (c == nullptr) {
        LUAUTILS_ERROR(L, "[Camera:WorldToScreen] incorrect caller, expected Camera")
    }

    float x, y, z;
    if (lua_number3_or_table_getXYZ(L, 2, &x, &y, &z) == false) {
        LUAUTILS_ERROR(L, "[Camera:WorldToScreen] expects a Number3 or three numbers");
    }

    float screenX, screenY;
    if (camera_world_to_unorm_screen(c, x, y, z, &screenX, &screenY)) {
        lua_number2_pushNewObject(L, screenX, screenY);
    } else {
        lua_pushnil(L);
    }

    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
    return 1;
}

bool _lua_g_camera_set_func(lua_State * const L, const char *key) {
    const bool isNil = lua_isnil(L, 3);
    if (lua_isfunction(L, 3) == false && isNil == false) {
        lua_pushfstring(L, "Camera.%s cannot be set", key);
        return false;
    }

    if (lua_getmetatable(L, 1) == 0) {
        lua_pushfstring(L, "Camera.%s cannot be set", key);
        return false;
    }
    lua_pushstring(L, key);
    lua_pushvalue(L, 3);
    lua_settable(L, -3);
    lua_pop(L, 1); // pop metatable

    return true;
}

void _lua_camera_get_and_push_global_func(lua_State * const L, const char *key) {
    LUAUTILS_STACK_SIZE(L)

    lua_utils_pushRootGlobalsFromRegistry(L);
    if (lua_getmetatable(L, -1) != 1) {
        vxlog_error("Couldn't get globals table metatable");
        LUAUTILS_INTERNAL_ERROR(L)
    }
    if (lua_getfield(L, -1, LUA_G_CAMERA) != LUA_TUSERDATA) {
        vxlog_error("Couldn't get global table Camera");
        LUAUTILS_INTERNAL_ERROR(L)
    }
    LUA_GET_METAFIELD_AND_RETURN_TYPE_PUSHING_NIL_IF_NOT_FOUND(L, -1, key);
    lua_remove(L, -2); // remove global Camera
    lua_remove(L, -2); // remove globals metatable
    lua_remove(L, -2); // remove globals table

    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
}
