//
//  lua_dev.cpp
//  Cubzh
//
//  Created by Adrien Duermael on 30/06/2021.
//

#include "lua_dev.hpp"

// Lua
#include "lua.hpp"
#include "lua_logs.hpp"
#include "lua_utils.hpp"
#include "lua_constants.h"
#include "lua_object.hpp"

// vx
#include "GameCoordinator.hpp"
#include "process.hpp"
#include "VXApplication.hpp"
#include "VXGame.hpp"
#include "VXGameRenderer.hpp"
#include "VXHubClient.hpp"
#include "VXPrefs.hpp"

// xptools
#include "vxlog.h"

#define LUA_DEV_FIELD_SET_GAME_THUMBNAIL "SetGameThumbnail"     // read-only function
#define LUA_DEV_FIELD_DISPLAY_BOXES "DisplayBoxes"              // boolean
#define LUA_DEV_FIELD_DISPLAY_COLLIDERS "DisplayColliders"      // boolean
#define LUA_DEV_FIELD_DISPLAY_FPS "DisplayFPS"                  // boolean
#define LUA_DEV_FIELD_MEMORY_USAGE_MB "MemoryUsageMb"           // number
#define LUA_DEV_FIELD_COPY_TO_CLIPBOARD "CopyToClipboard"       // function
#define LUA_DEV_FIELD_GET_FROM_CLIPBOARD "GetFromClipboard"     // function
#define LUA_DEV_FIELD_WORLD_URL "WorldURL"                      // string
#define LUA_DEV_FIELD_SERVER_URL "ServerURL"                    // string
#define LUA_DEV_FIELD_CAN_RUN_COMMANDS "CanRunCommands"         // boolean (read-only)
#define LUA_DEV_FIELD_EXECUTIONLIMITER "ExecutionLimiter"       // read-only function

static int _dev_metatable_index(lua_State *L);
static int _dev_metatable_newindex(lua_State *L);

static int _dev_setGameThumbnail(lua_State *L);
static int _dev_copyToClipboard(lua_State *L);
static int _dev_getFromClipboard(lua_State *L);
static int _dev_executionLimiter(lua_State * const L);

static std::unordered_set<Weakptr*> _displayColliderRegistry;
static std::unordered_set<Weakptr*> _displayBoxRegistry;

void _set_dev_metatable(lua_State *L) {
    
    lua_newtable(L); // metatable
    
    lua_pushstring(L, "__index");
    lua_pushcfunction(L, _dev_metatable_index, "");
    lua_rawset(L, -3);
    
    lua_pushstring(L, "__newindex");
    lua_pushcfunction(L, _dev_metatable_newindex, "");
    lua_rawset(L, -3);
    
    lua_pushstring(L, "__metatable");
    lua_pushboolean(L, false);
    lua_rawset(L, -3);

    lua_pushstring(L, P3S_LUA_OBJ_METAFIELD_TYPE); // used to log tables
    lua_pushinteger(L, ITEM_TYPE_DEV);
    lua_rawset(L, -3);
    
    lua_setmetatable(L, -2);
}

void lua_dev_create_and_push(lua_State *L) {
    LUAUTILS_STACK_SIZE(L)
    lua_newtable(L);
    _set_dev_metatable(L);
    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
}

static int _dev_metatable_index(lua_State *L) {
    vx_assert(L != nullptr);
    LUAUTILS_ASSERT_ARGS_COUNT(L, 2)
    LUAUTILS_ASSERT_ARG_TYPE(L, 1, ITEM_TYPE_DEV)
    LUAUTILS_STACK_SIZE(L)
    
    // TODO: make sure user is a dev
    
    // get 2nd argument: key string
    if (lua_utils_isStringStrict(L, 2) == false) {
        // Dev does not support non-string keys
        // just return nil
        lua_pushnil(L);
        LUAUTILS_STACK_SIZE_ASSERT(L, 1)
        return 1;
    }
    
    // retrieve Game from Lua state
    vx::Game *g;
    if (lua_utils_getGameCppWrapperPtr(L, &g) == false || g == nullptr) {
        LUAUTILS_RETURN_INTERNAL_ERROR(L)
    }

    const char *key = lua_tostring(L, 2);
    LUAUTILS_STACK_SIZE_ASSERT(L, 0)

    if (strcmp(key, LUA_DEV_FIELD_SET_GAME_THUMBNAIL) == 0) {
        lua_pushcfunction(L, _dev_setGameThumbnail, "");
    } else if (strcmp(key, LUA_DEV_FIELD_EXECUTIONLIMITER) == 0) {
        lua_pushcfunction(L, _dev_executionLimiter, "");
    } else if (strcmp(key, LUA_DEV_FIELD_COPY_TO_CLIPBOARD) == 0) {
        lua_pushcfunction(L, _dev_copyToClipboard, "");
    } else if (strcmp(key, LUA_DEV_FIELD_GET_FROM_CLIPBOARD) == 0) {
        lua_pushcfunction(L, _dev_getFromClipboard, "");
    } else if (strcmp(key, LUA_DEV_FIELD_DISPLAY_BOXES) == 0) {
        vx::GameRendererState_SharedPtr grs = g->getGameRendererState();
        lua_pushboolean(L, grs->debug_displayBoxes);
    } else if (strcmp(key, LUA_DEV_FIELD_DISPLAY_COLLIDERS) == 0) {
        vx::GameRendererState_SharedPtr grs = g->getGameRendererState();
        lua_pushboolean(L, grs->debug_displayColliders);
    } else if (strcmp(key, LUA_DEV_FIELD_DISPLAY_FPS) == 0) {
        vx::GameRendererState_SharedPtr grs = g->getGameRendererState();
        lua_pushboolean(L, grs->debug_displayFPS);
    } else if (strcmp(key, LUA_DEV_FIELD_MEMORY_USAGE_MB) == 0) {
        lua_pushinteger(L, static_cast<double>(vx::Process::getUsedMemory() / 1048576));
    } else if (strcmp(key, LUA_DEV_FIELD_WORLD_URL) == 0) {
        vx::WorldFetchInfo_SharedPtr fetchInfo = g->getFetchInfo();
        if (fetchInfo->getWorldID().empty() == false) {
            std::string url = "https://app.cu.bzh/?worldID=" + fetchInfo->getWorldID();
            lua_pushstring(L, url.c_str());
        } else if (fetchInfo->getScriptOrigin().empty() == false) {
            std::string url = "https://app.cu.bzh/?script=" + fetchInfo->getScriptOrigin();
            lua_pushstring(L, url.c_str());
        } else {
            lua_pushnil(L);
        }
    } else if (strcmp(key, LUA_DEV_FIELD_SERVER_URL) == 0) {
        // TEMPORARY, WE NEED REAL SERVER URLS
        vx::WorldFetchInfo_SharedPtr fetchInfo = g->getFetchInfo();
        if (fetchInfo->getWorldID().empty() == false) {
            std::string url = "https://app.cu.bzh/?worldID=" + fetchInfo->getWorldID();
            lua_pushstring(L, url.c_str());
        } else if (fetchInfo->getScriptOrigin().empty() == false) {
            std::string url = "https://app.cu.bzh/?script=" + fetchInfo->getScriptOrigin();
            lua_pushstring(L, url.c_str());
        } else {
            lua_pushnil(L);
        }

    } else if (strcmp(key, LUA_DEV_FIELD_CAN_RUN_COMMANDS) == 0) {
        vx::Game *game;
        if (lua_utils_getGameCppWrapperPtr(L, &game) == false) {
            LUAUTILS_RETURN_INTERNAL_ERROR(L);
        }
        lua_pushboolean(L, game->localUserIsAuthor() && game->serverIsInDevMode());
    } else {
        lua_pushnil(L); // if key is unknown, return nil
    }
    
    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
    return 1;
}

///
static int _dev_metatable_newindex(lua_State *L) {
    vx_assert(L != nullptr);
    LUAUTILS_ASSERT_ARGS_COUNT(L, 3)
    LUAUTILS_ASSERT_ARG_TYPE(L, 1, ITEM_TYPE_DEV)
    LUAUTILS_STACK_SIZE(L)
    
    // TODO: make sure user is a dev
    
    // setting non-string or non-capitalized string keys is allowed
    if (lua_utils_isStringStrictStartingWithUppercase(L, 2) == false) {
        // store the key/value pair
        lua_pushvalue(L, 2); // key
        lua_pushvalue(L, 3); // value
        lua_rawset(L, 1);
        LUAUTILS_STACK_SIZE_ASSERT(L, 0)
        return 0;
    }
    
    // retrieve Game from Lua state
    vx::Game *g;
    if (lua_utils_getGameCppWrapperPtr(L, &g) == false || g == nullptr) {
        LUAUTILS_RETURN_INTERNAL_ERROR(L)
    }

    // key is a capitalized string
    const char *key = lua_tostring(L, 2);
    
    if (strcmp(key, LUA_DEV_FIELD_DISPLAY_BOXES) == 0) {
        if (lua_isboolean(L, 3)) {
            const bool value = lua_toboolean(L, 3);
            vx::GameRendererState_SharedPtr grs = g->getGameRendererState();
            grs->debug_displayBoxes = DEBUG_RENDERER_DISPLAY_BOXES || value;

            if (value == false) {
                // reset individual objects previously flagged
                for (Weakptr *wptr : _displayBoxRegistry) {
                    if (weakptr_get(wptr) != nullptr) {
                        debug_transform_set_display_box((Transform*)weakptr_get(wptr), false);
                    }
                    weakptr_release(wptr);
                }
                _displayBoxRegistry.clear();
            }
        } else if (lua_object_isObject(L, 3)) {
            Transform *t; lua_object_getTransformPtr(L, 3, &t);
            if (t != nullptr) {
                debug_transform_set_display_box(t, true);

                Weakptr *wptr = transform_get_and_retain_weakptr(t);
                if (_displayBoxRegistry.insert(wptr).second == false) {
                    weakptr_release(wptr); // already in registry
                }
            }
        } else if (lua_istable(L, 3)) {
            bool isObject = false;
            int idx = 0;
            do {
                lua_rawgeti(L, 3, ++idx);
                isObject = lua_object_isObject(L, -1);
                if (isObject) {
                    Transform *t; lua_object_getTransformPtr(L, -1, &t);
                    if (t != nullptr) {
                        debug_transform_set_display_box(t, true);

                        Weakptr *wptr = transform_get_and_retain_weakptr(t);
                        if (_displayBoxRegistry.insert(wptr).second == false) {
                            weakptr_release(wptr); // already in registry
                        }
                    }
                }
                lua_pop(L, 1);
            } while(isObject);
        } else {
            LUAUTILS_ERROR_F(L, "Dev.%s cannot be set (argument must be boolean, Object, or array of Objects)", key);
        }
    } else if (strcmp(key, LUA_DEV_FIELD_DISPLAY_COLLIDERS) == 0) {
        if (lua_isboolean(L, 3)) {
            const bool value = lua_toboolean(L, 3);
            vx::GameRendererState_SharedPtr grs = g->getGameRendererState();
            grs->debug_displayColliders = DEBUG_RENDERER_DISPLAY_COLLIDERS || value;

            if (value == false) {
                // reset individual objects previously flagged
                for (Weakptr *wptr : _displayColliderRegistry) {
                    if (weakptr_get(wptr) != nullptr) {
                        debug_transform_set_display_collider((Transform*)weakptr_get(wptr), false);
                    }
                    weakptr_release(wptr);
                }
                _displayColliderRegistry.clear();
            }
        } else if (lua_object_isObject(L, 3)) {
            Transform *t; lua_object_getTransformPtr(L, 3, &t);
            if (t != nullptr) {
                debug_transform_set_display_collider(t, true);

                Weakptr *wptr = transform_get_and_retain_weakptr(t);
                if (_displayColliderRegistry.insert(wptr).second == false) {
                    weakptr_release(wptr); // already in registry
                }
            }
        } else if (lua_istable(L, 3)) {
            bool isObject = false;
            int idx = 0;
            do {
                lua_rawgeti(L, 3, ++idx);
                isObject = lua_object_isObject(L, -1);
                if (isObject) {
                    Transform *t; lua_object_getTransformPtr(L, -1, &t);
                    if (t != nullptr) {
                        debug_transform_set_display_collider(t, true);

                        Weakptr *wptr = transform_get_and_retain_weakptr(t);
                        if (_displayColliderRegistry.insert(wptr).second == false) {
                            weakptr_release(wptr); // already in registry
                        }
                    }
                }
                lua_pop(L, 1);
            } while(isObject);
        } else {
            LUAUTILS_ERROR_F(L, "Dev.%s cannot be set (argument must be boolean, Object, or array of Objects)", key);
        }
    } else if (strcmp(key, LUA_DEV_FIELD_DISPLAY_FPS) == 0) {
        if (lua_isboolean(L, 3) == false) {
            LUAUTILS_ERROR_F(L, "Dev.%s cannot be set (argument is not a boolean)", key);
        }
        vx::GameRendererState_SharedPtr grs = g->getGameRendererState();
        grs->debug_displayFPS = DEBUG_RENDERER_DISPLAY_FPS || lua_toboolean(L, 3);
    } else {
        // field is not settable
        vxlog_error("Dev.%s is not settable.", key);
    }

    LUAUTILS_STACK_SIZE_ASSERT(L, 0)
    return 0;
}

static int _dev_copyToClipboard(lua_State *L) {
    RETURN_VALUE_IF_NULL(L, 0)
    
    LUAUTILS_STACK_SIZE(L)
    
    const int argCount = lua_gettop(L);
    
    if (argCount < 2 || lua_utils_getObjectType(L, 1) != ITEM_TYPE_DEV) {
        LUAUTILS_ERROR(L, "Dev:CopyToClipboard - function should be called with `:`");
    }
    
    if (lua_isstring(L, 2) == false) {
        LUAUTILS_ERROR(L, "Dev:CopyToClipboard - function should be called with a text as parameter");
    }
    
    const char *text = lua_tostring(L, 2);
    vx::device::setClipboardText(text);

    LUAUTILS_STACK_SIZE_ASSERT(L, 0)
    return 0;
}

static int _dev_getFromClipboard(lua_State *L) {
    RETURN_VALUE_IF_NULL(L, 0)
    LUAUTILS_STACK_SIZE(L)
    
    const int argCount = lua_gettop(L);
    
    if (argCount < 1 || lua_utils_getObjectType(L, 1) != ITEM_TYPE_DEV) {
        LUAUTILS_ERROR(L, "Dev:GetFromClipboard - function should be called with `:`");
    }
    
    lua_pushstring(L, vx::device::getClipboardText().c_str());

    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
    return 1;
}

static int _dev_setGameThumbnail(lua_State *L) {
    RETURN_VALUE_IF_NULL(L, 0)
    
    LUAUTILS_STACK_SIZE(L)
    
    const int argCount = lua_gettop(L);

    if (argCount < 1 || lua_utils_getObjectType(L, 1) != ITEM_TYPE_DEV) {
        LUAUTILS_ERROR(L, "Dev:SetGameThumbnail - function should be called with `:`");
    }
    
    vx::Game *game;
    if (lua_utils_getGameCppWrapperPtr(L, &game) == false) {
        // lua state should always have a pointer to the C++ Game
        vx_assert(false);
        LUAUTILS_STACK_SIZE_ASSERT(L, 0)
        return 0;
    }
    
    if (game->localUserIsAuthor() == false) {
        lua_log_info(L, "Only the author of a game can update its thumbnail.");
        LUAUTILS_STACK_SIZE_ASSERT(L, 0)
        return 0;
    }
    
    if (game->startTakingAndUploadingThumbnail() == false) {
        lua_log_info(L, "can't take thumbnail while one is being uploaded.");
        LUAUTILS_STACK_SIZE_ASSERT(L, 0)
        return 0;
    }
    
    // take screenshot
    
    size_t thumbnailBytesCount;
    void *thumbnailBytes;

    vx::GameRenderer *renderer = vx::GameRenderer::getGameRenderer();
    renderer->screenshot(&thumbnailBytes,
                         nullptr,
                         nullptr,
                         &thumbnailBytesCount,
                         "", // don't write to local file
                         true, // using PNG bytes
                         false, // background included
                         800,
                         800,
                         false,
                         false,
                         vx::GameRenderer::EnforceRatioStrategy::crop,
                         1.0f);
    
    lua_log_info(L, "uploading game thumbnail...");
    
    game->uploadThumbnail(thumbnailBytes, thumbnailBytesCount);
    
    LUAUTILS_STACK_SIZE_ASSERT(L, 0)
    return 0;
}

// args
// 1 : funcToCall (function)
// return values
// 1 : error (string or nil)
// 2,3,4 : return values of `funcToCall` (only if error is nil)
static int _dev_executionLimiter(lua_State * const L) {
    RETURN_VALUE_IF_NULL(L, 0)
    LUAUTILS_STACK_SIZE(L)

    // validate arguments
    {
        const int argCount = lua_gettop(L);
        if (argCount < 1 || lua_utils_getObjectType(L, 1) != ITEM_TYPE_DEV) {
            LUAUTILS_ERROR(L, "Dev:ExecutionLimiter - function should be called with `:`");
        }
        if (argCount != 2 || lua_isfunction(L, 2) == false) {
            LUAUTILS_ERROR(L, "Dev:ExecutionLimiter - 1 function argument expected");
        }
    }

    const int stackSizeBefore = lua_gettop(L);

    // push function argument onto the stack
    lua_pushvalue(L, 2);

    // call Lua function
    // retrieve Game from Lua state
    vx::Game *g;
    if (lua_utils_getGameCppWrapperPtr(L, &g) == false || g == nullptr) {
        LUAUTILS_RETURN_INTERNAL_ERROR(L)
    }
    g->getLuaExecutionLimiter().startLimitation(L, "");

    const int status = lua_pcall(L, 0, LUA_MULTRET, 0);
    
    g->getLuaExecutionLimiter().cancelLimitation(L);

    if (status != LUA_OK) { // pops function + arguments
        // error has been pushed onto the stack
        if (lua_utils_isStringStrict(L, -1)) {
            // error is a Lua string
        } else {
            // replace non-string error with a string
            lua_pop(L, 1);
            lua_pushstring(L, "error while executing function with limiter");
        }
        // failure
        LUAUTILS_STACK_SIZE_ASSERT(L, 1)
        return 1;
    }
    const int returnValuesCount = lua_gettop(L) - stackSizeBefore;

    // push error first
    lua_pushnil(L); // success: err is nil
    // then push return values of the pcall-ed function
    if (returnValuesCount > 0) {
        for (int i = 0; i < returnValuesCount; ++i) {
            lua_pushvalue(L, -returnValuesCount - 1);
            lua_remove(L, -returnValuesCount - 2);
        }
    }

    const int luaRetValCount = 1 + returnValuesCount;
    LUAUTILS_STACK_SIZE_ASSERT(L, luaRetValCount)
    return luaRetValCount;
}
