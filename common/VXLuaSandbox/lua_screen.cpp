//
//  lua_screen.cpp
//  Cubzh
//
//  Created by Adrien Duermael on 14/11/2021.
//

#include "lua_screen.hpp"

// C++
#include <cstring>

#include "config.h"

#include "lua.hpp"
#include "lua_utils.hpp"
#include "lua_constants.h"
#include "lua_number2.hpp"
#include "lua_number3.hpp"
#include "lua_logs.hpp"
#include "lua_local_event.hpp"
#include "lua_menu.hpp"

#include "VXGame.hpp"
#include "VXNode.hpp"
#include "VXGameRenderer.hpp"
#include "VXApplication.hpp"

#define LUA_SCREEN_FIELD_WIDTH    "Width"    // number (read-only)
#define LUA_SCREEN_FIELD_HEIGHT   "Height"   // number (read-only)
#define LUA_SCREEN_FIELD_RENDERWIDTH    "RenderWidth"    // number (read-only)
#define LUA_SCREEN_FIELD_RENDERHEIGHT   "RenderHeight"   // number (read-only)
#define LUA_SCREEN_FIELD_SIZE     "Size"     // Number2 (read-only)
#define LUA_SCREEN_FIELD_RENDERSIZE "RenderSize" // Number2 (read-only)
#define LUA_SCREEN_FIELD_DENSITY  "Density"  // number (read-only)
#define LUA_SCREEN_FIELD_SAFEAREA "SafeArea" // table, contains Top, Bottom, Left & Right values (NOTE: should make a proper "Area" type maybe)
#define LUA_SCREEN_FIELD_CAPTURE  "Capture"
#define LUA_SCREEN_FIELD_ORIENTATION "Orientation" // "all", "landscape", "portrait"

#define LUA_SCREEN_FIELD_DID_RESIZE_LISTENER  "_drl" // LocalEvent listener

static int _g_lua_screen_index(lua_State * const L);
static int _g_lua_screen_newindex(lua_State * const L);
int _lua_screen_g_capture(lua_State * const L);

void lua_g_screen_create_and_push(lua_State * const L) {
    vx_assert(L != nullptr);
    LUAUTILS_STACK_SIZE(L)

    lua_newtable(L); // Screen
    lua_newtable(L); // metatable
    {
        lua_pushstring(L, "__index");
        lua_pushcfunction(L, _g_lua_screen_index, "");
        lua_rawset(L, -3);

        lua_pushstring(L, "__newindex");
        lua_pushcfunction(L, _g_lua_screen_newindex, "");
        lua_rawset(L, -3);

        lua_pushstring(L, LUA_SCREEN_FIELD_DID_RESIZE);
        lua_pushnil(L);
        lua_rawset(L, -3);

        lua_pushstring(L, "__metatable");
        lua_pushboolean(L, false);
        lua_rawset(L, -3);

        lua_pushstring(L, P3S_LUA_OBJ_METAFIELD_TYPE);
        lua_pushinteger(L, ITEM_TYPE_G_SCREEN);
        lua_rawset(L, -3);
    }
    lua_setmetatable(L, -2);

    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
}

static int _g_lua_screen_index(lua_State * const L) {
    vx_assert(L != nullptr);
    LUAUTILS_ASSERT_ARGS_COUNT(L, 2)
    LUAUTILS_ASSERT_ARG_TYPE(L, 1, ITEM_TYPE_G_SCREEN)
    LUAUTILS_STACK_SIZE(L)
    
    const char *key = lua_tostring(L, 2);

    if (strcmp(key, LUA_SCREEN_FIELD_WIDTH) == 0) {
        lua_pushnumber(L, static_cast<lua_Number>(vx::Screen::widthInPoints));

    } else if (strcmp(key, LUA_SCREEN_FIELD_HEIGHT) == 0) {

        lua_pushnumber(L, static_cast<lua_Number>(vx::Screen::heightInPoints));

    } else if (strcmp(key, LUA_SCREEN_FIELD_RENDERWIDTH) == 0) {
        lua_pushnumber(L, static_cast<lua_Number>(vx::GameRenderer::getGameRenderer()->getRenderWidth()));
    } else if (strcmp(key, LUA_SCREEN_FIELD_RENDERHEIGHT) == 0) {
        lua_pushnumber(L, static_cast<lua_Number>(vx::GameRenderer::getGameRenderer()->getRenderHeight()));
    } else if (strcmp(key, LUA_SCREEN_FIELD_SIZE) == 0) {

        lua_number2_pushNewObject(L, 
                                  static_cast<const float>(vx::Screen::widthInPoints),
                                  static_cast<const float>(vx::Screen::heightInPoints));
    } else if (strcmp(key, LUA_SCREEN_FIELD_RENDERSIZE) == 0) {
        lua_number2_pushNewObject(L,
                                  static_cast<const float>(vx::GameRenderer::getGameRenderer()->getRenderWidth()),
                                  static_cast<const float>(vx::GameRenderer::getGameRenderer()->getRenderHeight()));
    }  else if (strcmp(key, LUA_SCREEN_FIELD_SAFEAREA) == 0) {
        
        vx::Game *game = nullptr;
        if (lua_utils_getGameCppWrapperPtr(L, &game) == false) {
            LUAUTILS_INTERNAL_ERROR(L) // returns
        }
        vx_assert(game != nullptr);
        
        lua_newtable(L);
        
        lua_pushstring(L, "Top");
        lua_pushnumber(L, static_cast<double>(vx::Screen::safeAreaInsets.top));
        lua_rawset(L, -3);
        
        lua_pushstring(L, "Bottom");
        lua_pushnumber(L, static_cast<double>(vx::Screen::safeAreaInsets.bottom));
        lua_rawset(L, -3);
        
        lua_pushstring(L, "Left");
        lua_pushnumber(L, static_cast<double>(vx::Screen::safeAreaInsets.left));
        lua_rawset(L, -3);
        
        lua_pushstring(L, "Right");
        lua_pushnumber(L, static_cast<double>(vx::Screen::safeAreaInsets.right));
        lua_rawset(L, -3);

    } else if (strcmp(key, LUA_SCREEN_FIELD_CAPTURE) == 0) {
        lua_pushcfunction(L, _lua_screen_g_capture, "");
    } else if (strcmp(key, LUA_SCREEN_FIELD_ORIENTATION) == 0) {
        lua_pushstring(L, vx::device::getScreenAllowedOrientation().c_str());
    } else if (strcmp(key, LUA_SCREEN_FIELD_DENSITY) == 0) {
        lua_pushnumber(L, static_cast<double>(vx::Screen::nbPixelsInOnePoint));
    } else {
        LUA_GET_METAFIELD_PUSHING_NIL_IF_NOT_FOUND(L, 1, key);
    }
    
    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
    return 1;
}

static int _g_lua_screen_newindex(lua_State * const L) {
    vx_assert(L != nullptr);
    LUAUTILS_ASSERT_ARGS_COUNT(L, 3)
    LUAUTILS_ASSERT_ARG_TYPE(L, 1, ITEM_TYPE_G_SCREEN)
    LUAUTILS_STACK_SIZE(L)

    const char *key = lua_tostring(L, 2);

    if (strcmp(key, LUA_SCREEN_FIELD_DID_RESIZE) == 0) {

        // make sure the new value is a function or nil
        if (lua_isfunction(L, 3) == false && lua_isnil(L, 3) == false) {
            LUAUTILS_ERROR_F(L, "Screen.%s can only be a function or nil", key); // returns
        }

        lua_getmetatable(L, 1);

        // Remove LocalEvent Listener if set
        lua_pushstring(L, LUA_SCREEN_FIELD_DID_RESIZE_LISTENER);
        lua_rawget(L, -2);
        if (lua_isnil(L, -1)) {
            lua_pop(L, 1); // pop nil
        } else {
            LOCAL_EVENT_LISTENER_CALL_REMOVE(L, -1)
        }

        lua_pushstring(L, LUA_SCREEN_FIELD_DID_RESIZE_LISTENER);
        if (lua_isnil(L, 3) == false) {
            LOCAL_EVENT_LISTEN(L, LOCAL_EVENT_NAME_ScreenDidResize, 3); // callbackIdx: 3
        } else {
            lua_pushnil(L);
        }
        lua_rawset(L, -3);

        lua_pushstring(L, LUA_SCREEN_FIELD_DID_RESIZE);
        lua_pushvalue(L, 3);
        lua_rawset(L, -3);

        lua_pop(L, 1); // pop metatable

    } else if (strcmp(key, LUA_SCREEN_FIELD_ORIENTATION) == 0) {
        if (lua_isstring(L, 3) == false) {
            LUAUTILS_ERROR(L, "Screen.Orientation can only be a string (\"all\", \"portrait\", \"landscape\")"); // returns
        }
        std::string s = std::string(lua_tostring(L, 3));

        if (s == "all" || s == "portrait" || s == "landscape") {
            vx::device::setScreenAllowedOrientation(s);
        } else  {
            LUAUTILS_ERROR(L, "Screen.Orientation supported values: \"all\", \"portrait\", \"landscape\""); // returns
        }

    } else {
        LUAUTILS_ERROR_F(L, "Screen.%s can't be set", key); // returns
    }

    LUAUTILS_STACK_SIZE_ASSERT(L, 0)
    return 0;
}

/// Screen:Capture(self, <filename>, <noBackground>)
/// Takes a screenshot w/o UI, saved as "screenshot.png" in app storage
int _lua_screen_g_capture(lua_State * const L) {
    RETURN_VALUE_IF_NULL(L, 0)
    LUAUTILS_ASSERT_ARG_TYPE(L, 1, ITEM_TYPE_G_SCREEN)
    LUAUTILS_STACK_SIZE(L)

    // retrieve Game from Lua state
    CGame *g = nullptr;
    if (lua_utils_getGamePtr(L, &g) == false) {
        const char *error = "🔥 _lua_camera_g_Screenshot : failed to retrieve Game from lua state";
        lua_pushstring(L, error);
        LUAUTILS_STACK_SIZE_ASSERT(L, 1)
        lua_error(L); // returns
    }
    vx_assert(g != nullptr);

    const int argCount = lua_gettop(L);
    int arg = 2;

    // optional argument: filename
    std::string filename = "screenshot";
    if (arg <= argCount && lua_isstring(L, arg)) {
        filename.assign(lua_tostring(L, arg));
        arg++;
    }

    // optional argument: transparent background
    bool noBackground = false;
    if (arg <= argCount && lua_isboolean(L, arg)) {
        noBackground = lua_toboolean(L, arg);
    }

    // screenshot needs to happen immediately for line-by-line usage in Lua (i.e. setting up objects,
    // calling screenshot, resetting objects all in one go), making a full refresh necessary
    scene_standalone_refresh(game_get_scene(g));

    // take screenshot & save to app storage
    const std::string pngFileStoragePath = "screenshots/" + filename;
    uint16_t w, h;
    size_t thumbnailBytesCount = 0;
    vx::GameRenderer *renderer = vx::GameRenderer::getGameRenderer();
    renderer->screenshot(nullptr,
                         &w,
                         &h,
                         &thumbnailBytesCount,
                         pngFileStoragePath,
                         false, // not using PNG bytes
                         noBackground, // transparent background
                         2048,
                         2048,
                         false, // unflip
                         true, // trim
                         vx::GameRenderer::EnforceRatioStrategy::none); // strategy to enforce ratio

    vx::fs::shareFile(pngFileStoragePath + ".png", "Save image", filename, vx::fs::FileType::PNG);

    LUAUTILS_STACK_SIZE_ASSERT(L, 0)
    return 0;
}
