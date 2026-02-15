//
//  lua_assets.cpp
//  Cubzh
//
//  Created by Corentin Cailleaud on 25/10/2022.
//  Copyright � 2020 Voxowl Inc. All rights reserved.
//

#include "lua_assets.hpp"

#include <vector>
#include <queue>

// Lua
#include "lua.hpp"
#include "lua_utils.hpp"

#include "lua_constants.h"
#include "lua_logs.hpp"
#include "lua_object.hpp"
#include "lua_shape.hpp"
#include "lua_mutableShape.hpp"
#include "lua_palette.hpp"
#include "lua_camera.hpp"
#include "lua_light.hpp"
#include "lua_assetType.hpp"
#include "lua_http.hpp"
#include "lua_data.hpp"
#include "lua_mesh.hpp"

#include "VXGame.hpp"
#include "VXResource.hpp"

#include "serialization.h"
#include "fileutils.h"
#include "cache.h"
#include "game.h"
#include "vxconfig.h"
#include "camera.h"
#include "light.h"
#include "mesh.h"

// xptools
#include "OperationQueue.hpp"

#define LUA_G_ASSETS_LOAD "Load"                              // function
#define LUA_G_ASSETS_PENDING_REQUESTS "_pr"
#define LUA_G_ASSETS_REQUEST_CALLBACKS "_rc"

#define LUA_ASSETS_REQUEST_CANCEL "Cancel"
#define LUA_ASSETS_REQUEST_STATUS "Status"

typedef struct assetsRequest_userdata {
    // Userdata instance requirements
    vx::UserDataStore *store;
    assetsRequest_userdata *next;
    assetsRequest_userdata *previous;

    vx::HttpRequest_SharedPtr *req;
    uint32_t id;
    int status;
} assetsRequest_userdata;

static void _g_assets_load_item(vx::Game_WeakPtr weakGame, int requestIndex, std::string repo,
                                std::string name, bool fromAppBundle, bool appBundleFallback,
                                ShapeSettings shapeSettings, ASSET_MASK_T filter, bool async);
static void _g_assets_load_data(vx::Game_WeakPtr weakGame, int requestIndex, const void *data,
                                size_t dataSize, ShapeSettings shapeSettings, ASSET_MASK_T filter,
                                bool async);
static int _g_assets_load(lua_State * const L);

void _lua_assets_request_create_and_push(lua_State * const L,
                                         const int gAssetsIdx,
                                         const int callbackIdx,
                                         int *requestID);

void _lua_assets_request_attach_http_request(lua_State * const L, 
                                             const int assetsRequestIdx,
                                             const vx::HttpRequest_SharedPtr& ptr);

static int _assets_request_index(lua_State * const L);
static void _assets_request_gc(void *_ud);
static int _assets_request_cancel(lua_State * const L);

static std::queue<int> availableRequestIDs;
static int nextRequestID = 1;

bool lua_g_assets_create_and_push(lua_State * const L) {
    vx_assert(L != nullptr);
    LUAUTILS_STACK_SIZE(L)

    lua_newuserdata(L, 1);
    lua_newtable(L); // metatable
    {
        lua_pushstring(L, "__index");
        lua_newtable(L); // table for read-only keys
        {
            lua_pushstring(L, LUA_G_ASSETS_LOAD);
            lua_pushcfunction(L, _g_assets_load, "");
            lua_rawset(L, -3);
        }
        lua_rawset(L, -3);
        
        lua_pushstring(L, "__metatable");
        lua_pushboolean(L, false);
        lua_rawset(L, -3);

        lua_pushstring(L, LUA_G_ASSETS_PENDING_REQUESTS);
        lua_newtable(L);
        lua_rawset(L, -3);

        lua_pushstring(L, LUA_G_ASSETS_REQUEST_CALLBACKS);
        lua_newtable(L);
        lua_rawset(L, -3);

        lua_pushstring(L, "__imt"); // instance metatable
        lua_newtable(L);
        {
            lua_pushstring(L, "__index");
            lua_pushcfunction(L, _assets_request_index, "");
            lua_rawset(L, -3);

            lua_pushstring(L, "__metatable");
            lua_pushboolean(L, false);
            lua_rawset(L, -3);

            lua_pushstring(L, "__type");
            lua_pushstring(L, "AssetsRequest");
            lua_rawset(L, -3);

            lua_pushstring(L, "__typen");
            lua_pushinteger(L, ITEM_TYPE_ASSETS_REQUEST);
            lua_rawset(L, -3);
        }
        lua_rawset(L, -3);

        lua_pushstring(L, "__type");
        lua_pushstring(L, "AssetsInterface");
        lua_rawset(L, -3);

        lua_pushstring(L, "__typen");
        lua_pushinteger(L, ITEM_TYPE_G_ASSETS);
        lua_rawset(L, -3);
    }
    lua_setmetatable(L, -2);

    LUAUTILS_STACK_SIZE_ASSERT(L, 1);
    return true;
}

int lua_g_assets_snprintf(lua_State * const L, char *result, const size_t maxLen, const bool spacePrefix) {
    LUAUTILS_STACK_SIZE(L)
    LUAUTILS_STACK_SIZE_ASSERT(L, 0)
    return snprintf(result, maxLen, "%s[Assets]", spacePrefix ? " " : "");
}

void g_assets_process_objects(vx::Game_WeakPtr weakGame,
                              lua_Integer requestIndex,
                              DoublyLinkedList *assets,
                              ShapeSettings shapeSettings,
                              bool async) {

    auto stepFunc_setAtlas = [weakGame, requestIndex, assets, shapeSettings, async](){
        vx::Game_SharedPtr strongGame = weakGame.lock();
        if (strongGame == nullptr) {
            if (assets != nullptr) {
                doubly_linked_list_flush(assets, serialization_assets_free_func);
                doubly_linked_list_free(assets);
            }
            return;
        }
        CGame *cgame = strongGame->getCGame();
        if (cgame == nullptr) {
            if (assets != nullptr) {
                doubly_linked_list_flush(assets, serialization_assets_free_func);
                doubly_linked_list_free(assets);
            }
            return;
        }

        ColorAtlas *game_ca = game_get_color_atlas(cgame);
        DoublyLinkedListNode *n = doubly_linked_list_first(assets);
        while (n != nullptr) {
            Asset *asset = static_cast<Asset *>(doubly_linked_list_node_pointer(n));
            switch (asset->type) {
                case AssetType_Shape:
                    shape_set_color_palette_atlas(static_cast<Shape *>(asset->ptr), game_ca);
                    break;
                case AssetType_Palette:
                    color_palette_set_atlas(static_cast<ColorPalette *>(asset->ptr), game_ca);
                    break;
                default:
                    break;
            }
            n = doubly_linked_list_node_next(n);
        }

        auto stepFunc_buildShapes = [weakGame, requestIndex, assets, shapeSettings, async]{
            vx::Game_SharedPtr strongGame = weakGame.lock();
            if (strongGame == nullptr) {
                if (assets != nullptr) {
                    doubly_linked_list_flush(assets, serialization_assets_free_func);
                    doubly_linked_list_free(assets);
                }
                return;
            }

            DoublyLinkedListNode *n = doubly_linked_list_first(assets);
            while (n != nullptr) {
                Asset *asset = static_cast<Asset *>(doubly_linked_list_node_pointer(n));
                if (asset->type == AssetType_Shape) {
                    shape_refresh_all_vertices(static_cast<Shape *>(asset->ptr));
                }
                n = doubly_linked_list_node_next(n);
            }

            auto stepFunc_output = [weakGame, requestIndex, assets, shapeSettings](){
                vx::Game_SharedPtr strongGame = weakGame.lock();
                if (strongGame == nullptr) {
                    if (assets != nullptr) {
                        doubly_linked_list_flush(assets, serialization_assets_free_func);
                        doubly_linked_list_free(assets);
                    }
                    return;
                }
                CGame *cgame = strongGame->getCGame();
                if (cgame == nullptr) {
                    if (assets != nullptr) {
                        doubly_linked_list_flush(assets, serialization_assets_free_func);
                        doubly_linked_list_free(assets);
                    }
                    return;
                }
                lua_State *L = strongGame->getLuaState();
                if (L == nullptr) {
                    if (assets != nullptr) {
                        doubly_linked_list_flush(assets, serialization_assets_free_func);
                        doubly_linked_list_free(assets);
                    }
                    return;
                }

                LUAUTILS_STACK_SIZE(L)

                // mark request as done + trigger callback
                int type = LUA_GET_GLOBAL_AND_RETURN_TYPE(L, LUA_G_ASSETS);
                vx_assert(type == LUA_TUSERDATA);

                // Assets: -1

                LUA_GET_METAFIELD(L, -1, LUA_G_ASSETS_PENDING_REQUESTS);
                vx_assert(lua_type(L, -1) == LUA_TTABLE);

                // Pending Requests: -1
                // Assets: -2

                lua_rawgeti(L, -1, requestIndex);
                if (lua_isnil(L, -1)) {
                    // request is already gone, cancelled certainly
                    // just free assets and return
                    if (assets != nullptr) {
                        doubly_linked_list_flush(assets, serialization_assets_free_func);
                        doubly_linked_list_free(assets);
                    }
                    lua_pop(L, 3); // pop nil, pending requests and assets
                    LUAUTILS_STACK_SIZE_ASSERT(L, 0)
                    return;
                }

                vx_assert(lua_utils_getObjectType(L, -1) == ITEM_TYPE_ASSETS_REQUEST);

                // -1: request
                // -2: pending requests
                // -3: assets

                // remove request from pending requests
                lua_pushnil(L);
                lua_rawseti(L, -3, requestIndex);

                lua_remove(L, -2); // remove pending requests

                // -1: request
                // -2: Assets

                vx_assert(lua_utils_getObjectType(L, -1) == ITEM_TYPE_ASSETS_REQUEST);

                LUA_GET_METAFIELD(L, -2, LUA_G_ASSETS_REQUEST_CALLBACKS);
                vx_assert(lua_type(L, -1) == LUA_TTABLE);

                // -1: callbacks
                // -2: request
                // -3: Assets

                lua_rawgeti(L, -1, requestIndex);
                if (lua_isnil(L, -1)) {
                    // no callback, free assets and return
                    if (assets != nullptr) {
                        doubly_linked_list_flush(assets, serialization_assets_free_func);
                        doubly_linked_list_free(assets);
                    }
                    lua_pop(L, 4); // pop nil, callbacks, request and assets
                    LUAUTILS_STACK_SIZE_ASSERT(L, 0)
                    return;
                }

                vx_assert(lua_type(L, -1) == LUA_TFUNCTION);

                // -1: callback function
                // -2: callbacks
                // -3: request
                // -4: Assets
                LUAUTILS_STACK_SIZE_ASSERT(L, 4)

                // set callback to nil to release function and upvalues as soon as possible.
                // (request callback can only be called once)
                lua_pushnil(L);
                lua_rawseti(L, -3, requestIndex);

                // -1: callback function
                // -2: callbacks
                // -3: request
                // -4: Assets
                LUAUTILS_STACK_SIZE_ASSERT(L, 4)

                lua_remove(L, -2); // remove callbacks
                lua_remove(L, -2); // remove request
                lua_remove(L, -2); // remove Assets

                // -1: callback
                LUAUTILS_STACK_SIZE_ASSERT(L, 1)

                if (assets == nullptr) {
                    lua_pushnil(L);
                } else {
                    uint32_t nbItems = static_cast<uint32_t>(doubly_linked_list_node_count(assets));
                    DoublyLinkedListNode *node = doubly_linked_list_first(assets);
                    lua_createtable(L, nbItems, 0);
                    for (uint32_t i = 0; i < nbItems; ++i) {
                        Asset *asset = static_cast<Asset *>(doubly_linked_list_node_pointer(node));
                        switch (asset->type) {
                            case AssetType_Object: {
                                Transform *t = static_cast<Transform *>(asset->ptr);
                                if (lua_object_pushNewInstance(L, t) == false) {
                                    lua_pushnil(L);
                                }
                                transform_release(t); // owned by lua Object or freed if error
                                break;
                            }
                            case AssetType_Shape: {
                                Shape *s = static_cast<Shape *>(asset->ptr);
                                bool created = false;
                                if (shapeSettings.isMutable) {
                                    created = lua_mutableShape_pushNewInstance(L, s);
                                } else {
                                    created = lua_shape_pushNewInstance(L, s);
                                }
                                if (created == false) {
                                    lua_pushnil(L);
                                }
                                shape_release(s); // owned by lua Mutable/Shape or freed if error
                                break;
                            }
                            case AssetType_Palette: {
                                ColorPalette *p = static_cast<ColorPalette *>(asset->ptr);
                                if (lua_palette_create_and_push_table(L, p, nullptr) == false) {
                                    lua_pushnil(L);
                                }
                                color_palette_release(p); // owned by lua Palette or freed if error
                                break;
                            }
                            case AssetType_Camera: {
                                Camera *c = static_cast<Camera *>(asset->ptr);
                                if (lua_camera_pushNewInstance(L, c) == false) {
                                    lua_pushnil(L);
                                }
                                camera_release(c); // owned by lua Camera or freed if error
                                break;
                            }
                            case AssetType_Light: {
                                Light *l = static_cast<Light *>(asset->ptr);
                                if (lua_light_pushNewInstance(L, l) == false) {
                                    lua_pushnil(L);
                                }
                                light_release(l); // owned by lua Light or freed if error
                                break;
                            }
                            case AssetType_Mesh: {
                                Mesh *m = static_cast<Mesh *>(asset->ptr);
                                if (lua_mesh_pushNewInstance(L, m) == false) {
                                    lua_pushnil(L);
                                }
                                mesh_release(m); // owned by lua Object or freed if error
                                break;
                            }
                            default:
                                vxlog_error("unknown asset, possible memory leak (asset->ptr)");
                                lua_pushnil(L);
                                break;
                        }
                        lua_rawseti(L, -2, i + 1);
                        // lua_seti(L, -2, i + 1);
                        free(asset);
                        node = doubly_linked_list_node_next(node);
                    }
                    doubly_linked_list_free(assets);
                }
                LUAUTILS_STACK_SIZE_ASSERT(L, 2);

                // callback function is on the top of the stack
                const int state = lua_pcall(L, 1, 0, 0);
                if (state != LUA_OK) {
                    if (lua_utils_isStringStrict(L, -1)) {
                        lua_log_error_CStr(L, lua_tostring(L, -1));
                    } else {
                        lua_log_error(L, "Assets:Load callback error");
                    }
                    lua_pop(L, 1); // pops the error
                }

                LUAUTILS_STACK_SIZE_ASSERT(L, 0)
            };
            if (async) {
                vx::OperationQueue::getMain()->dispatch(stepFunc_output);
            } else {
                stepFunc_output();
            }
        };
        if (async) {
            vx::OperationQueue::getSlowBackground()->dispatch(stepFunc_buildShapes);
        } else {
            stepFunc_buildShapes();
        }
    };
    if (async) {
        vx::OperationQueue::getMain()->dispatch(stepFunc_setAtlas);
    } else {
        stepFunc_setAtlas();
    }
}

static void _g_assets_load_item(vx::Game_WeakPtr weakGame,
                                int requestIndex,
                                std::string repo,
                                std::string name,
                                bool fromAppBundle,
                                bool appBundleFallback,
                                ShapeSettings shapeSettings,
                                ASSET_MASK_T filter,
                                bool async) {

    auto func = [weakGame, requestIndex, repo, name, fromAppBundle, appBundleFallback, shapeSettings, filter, async](){
        std::string itemFilePath = repo + "/" + name + ".3zh";
        std::string itemFullname = repo + "." + name;

        DoublyLinkedList *assets = nullptr;

        if (fromAppBundle == false) {
            assets = serialization_load_assets_from_cache(itemFilePath.c_str(), itemFullname.c_str(), filter, nullptr, &shapeSettings);
        }
        if (assets == nullptr && (appBundleFallback || fromAppBundle)) {
            itemFilePath = "cache/" + itemFilePath;
            assets = serialization_load_assets_from_assets(itemFilePath.c_str(), itemFullname.c_str(), filter, nullptr, &shapeSettings);
        }

        g_assets_process_objects(weakGame, requestIndex, assets, shapeSettings, async);
    };
    if (async) {
        vx::OperationQueue::getSlowBackground()->dispatch(func);
    } else {
        func();
    }
}

static void _g_assets_load_data(vx::Game_WeakPtr weakGame,
                                int requestIndex,
                                const void *data,
                                size_t dataSize,
                                ShapeSettings shapeSettings,
                                ASSET_MASK_T filter,
                                bool async) {

    // Make a copy of data, because original lua Data object may be GCed in the meantime
    void* dataCopy = malloc(dataSize);
    memcpy(dataCopy, data, dataSize);

    auto func = [weakGame, requestIndex, dataCopy, dataSize, shapeSettings, filter, async](){
        DoublyLinkedList *assets = nullptr;
        serialization_load_data(dataCopy, dataSize, filter, &shapeSettings, reinterpret_cast<void **>(&assets));
        free(dataCopy);

        g_assets_process_objects(weakGame, requestIndex, assets, shapeSettings, async);
    };
    if (async) {
        vx::OperationQueue::getSlowBackground()->dispatch(func);
    } else {
        func();
    }
}

// ARGS:
// 1: Assets (Global)
// 2: asset: item name (string) or Data
// 3: callback: function(assets)
// 4: [filter]: AssetType or mask of AssetType as integer
// 5: [config]: table
//      useLocal = false
//      useCache = true
//      fromAppBundle = false
//      appBundleFallback = false
//      mutable = false
//      bakedLight = false
//      async = true
static int _g_assets_load(lua_State *L) {
    vx_assert(L != nullptr);
    LUAUTILS_STACK_SIZE(L)

    // if data provided, skip to processing data directly
    size_t dataSize = 0;
    const void *data = nullptr;

    // fetch options
    bool useLocal = false; // get local assets if found (no request)
    bool useCache = true;
    bool fromAppBundle = false;
    bool appBundleFallback = false;

    // data processing options
    ASSET_MASK_T filter = AssetType_Any;
    ShapeSettings shapeSettings;
    shapeSettings.lighting = false;
    shapeSettings.isMutable = false;
    bool async = true;

    const int argCount = lua_gettop(L);
    if (argCount < 1 || lua_utils_getObjectType(L, 1) != ITEM_TYPE_G_ASSETS) {
        LUAUTILS_STACK_SIZE_ASSERT(L, 0);
        LUAUTILS_ERROR(L, "Assets:Load(asset, callback, [type, config]) - function should be called with `:`")
        return 0;
    }

    if (argCount < 3 || argCount > 5) {
        LUAUTILS_STACK_SIZE_ASSERT(L, 0);
        LUAUTILS_ERROR(L, "Assets:Load(asset, callback, [type, config]) - incorrect number of arguments")
        return 0;
    }

    if (lua_utils_getObjectType(L, 2) == ITEM_TYPE_DATA) {
        data = lua_data_getBuffer(L, 2, &dataSize);
    } else if (lua_utils_isStringStrict(L, 2) == false) {
        LUAUTILS_STACK_SIZE_ASSERT(L, 0);
        LUAUTILS_ERROR(L, "Assets:Load(asset, callback, [type, config]) - asset must be a string or Data")
        return 0;
    }
    const bool useData = data != nullptr;

    if (lua_isfunction(L, 3) == false) {
        LUAUTILS_STACK_SIZE_ASSERT(L, 0);
        LUAUTILS_ERROR(L, "Assets:Load(asset, callback, [type, config]) - callback must be a function")
        return 0;
    }

    if (argCount >= 4) {
        if (lua_utils_isIntegerStrict(L, 4)) {
            filter = static_cast<ASSET_MASK_T>(lua_tointeger(L, 4));
        } else if (lua_utils_getObjectType(L, 4) == ITEM_TYPE_ASSETTYPE) {
            filter = lua_assetType_get(L, 4);
        } else {
            LUAUTILS_STACK_SIZE_ASSERT(L, 0);
            LUAUTILS_ERROR(L, "Assets:Load(asset, callback, [type, config]) - type must be an AssetType or integer mask of AssetType")
            return 0;
        }
    }

    if (argCount == 5) { // parse config
        if (lua_isnil(L, 5) == false) {
            if (lua_istable(L, 5) == false) {
                LUAUTILS_STACK_SIZE_ASSERT(L, 0);
                LUAUTILS_ERROR(L, "Assets:Load(asset, callback, [type, config]) - config must be a table")
                return 0;
            }
            int type = lua_getfield(L, 5, "useLocal");
            if (type == LUA_TNIL) {
                lua_pop(L, 1); // pop nil
            } else if (type == LUA_TBOOLEAN) {
                useLocal = lua_toboolean(L, -1);
                lua_pop(L, 1);
            } else {
                lua_pop(L, 1);
                LUAUTILS_STACK_SIZE_ASSERT(L, 0);
                LUAUTILS_ERROR(L, "Assets:Load(asset, callback, [type, config]) - config.local must be a boolean")
                return 0;
            }

            type = lua_getfield(L, 5, "useCache");
            if (type == LUA_TNIL) {
                lua_pop(L, 1); // pop nil
            } else if (type == LUA_TBOOLEAN) {
                useCache = lua_toboolean(L, -1);
                lua_pop(L, 1);
            } else {
                lua_pop(L, 1);
                LUAUTILS_STACK_SIZE_ASSERT(L, 0);
                LUAUTILS_ERROR(L, "Assets:Load(asset, callback, [type, config]) - config.useCache must be a boolean")
                return 0;
            }
            
            type = lua_getfield(L, 5, "fromAppBundle");
            if (type == LUA_TNIL) {
                lua_pop(L, 1); // pop nil
            } else if (type == LUA_TBOOLEAN) {
                fromAppBundle = lua_toboolean(L, -1);
                lua_pop(L, 1);
            } else {
                lua_pop(L, 1);
                LUAUTILS_STACK_SIZE_ASSERT(L, 0);
                LUAUTILS_ERROR(L, "Assets:Load(asset, callback, [type, config]) - config.fromAppBundle must be a boolean")
                return 0;
            }
            
            type = lua_getfield(L, 5, "appBundleFallback");
            if (type == LUA_TNIL) {
                lua_pop(L, 1); // pop nil
            } else if (type == LUA_TBOOLEAN) {
                appBundleFallback = lua_toboolean(L, -1);
                lua_pop(L, 1);
            } else {
                lua_pop(L, 1);
                LUAUTILS_STACK_SIZE_ASSERT(L, 0);
                LUAUTILS_ERROR(L, "Assets:Load(asset, callback, [type, config]) - config.appBundleFallback must be a boolean")
                return 0;
            }
            
            type = lua_getfield(L, 5, "mutable");
            if (type == LUA_TNIL) {
                lua_pop(L, 1); // pop nil
            } else if (type == LUA_TBOOLEAN) {
                shapeSettings.isMutable = lua_toboolean(L, -1);
                lua_pop(L, 1);
            } else {
                lua_pop(L, 1);
                LUAUTILS_STACK_SIZE_ASSERT(L, 0);
                LUAUTILS_ERROR(L, "Assets:Load(asset, callback, [type, config]) - config.mutable must be a boolean")
                return 0;
            }

            type = lua_getfield(L, 5, "bakedLight");
            if (type == LUA_TNIL) {
                lua_pop(L, 1); // pop nil
            } else if (type == LUA_TBOOLEAN) {
                shapeSettings.lighting = lua_toboolean(L, -1);
                lua_pop(L, 1);
            } else {
                lua_pop(L, 1);
                LUAUTILS_STACK_SIZE_ASSERT(L, 0);
                LUAUTILS_ERROR(L, "Assets:Load(asset, callback, [type, config]) - config.bakedLight must be a boolean")
                return 0;
            }

            type = lua_getfield(L, 5, "async");
            if (type == LUA_TNIL) {
                lua_pop(L, 1); // pop nil
            } else if (type == LUA_TBOOLEAN) {
                async = lua_toboolean(L, -1);
                lua_pop(L, 1);
            } else {
                lua_pop(L, 1);
                LUAUTILS_STACK_SIZE_ASSERT(L, 0);
                LUAUTILS_ERROR(L, "Assets:Load(asset, callback, [type, config]) - config.async must be a boolean")
                return 0;
            }
        }
    }

    // get Game
    vx::Game *cppGame = nullptr;
    if (lua_utils_getGameCppWrapperPtr(L, &cppGame) == false || cppGame == nullptr) {
        LUAUTILS_STACK_SIZE_ASSERT(L, 0);
        LUAUTILS_INTERNAL_ERROR(L);
        // return 0;
    }
    vx::Game_WeakPtr weakGame = cppGame->getWeakPtr();

    // create AssetsRequest
    int requestID = 0;
    _lua_assets_request_create_and_push(L, 1, 3, &requestID);

    LUAUTILS_STACK_SIZE_ASSERT(L, 1)

    // skip to processing data if provided
    if (useData) {
        if (useLocal) {
            lua_log_warning(L, "Assets:Load(asset, callback, [type, config]) - useLocal ignored when passing asset as Data");
        }
        if (useCache == false) {
            lua_log_warning(L, "Assets:Load(asset, callback, [type, config]) - useCache ignored when passing asset as Data");
        }
        if (fromAppBundle) {
            lua_log_warning(L, "Assets:Load(asset, callback, [type, config]) - fromAppBundle ignored when passing asset as Data");
        }
        if (appBundleFallback) {
            lua_log_warning(L, "Assets:Load(asset, callback, [type, config]) - appBundleFallback ignored when passing asset as Data");
        }

        _g_assets_load_data(weakGame, requestID, data, dataSize, shapeSettings, filter, async);
    } else {
        // get name of the item to load
        const std::string itemName(lua_tostring(L, 2));
        std::vector<std::string> elements = vx::str::splitString(itemName, ".");

        if (elements.size() != 2) {
            LUAUTILS_STACK_SIZE_ASSERT(L, 0);
            LUAUTILS_ERROR(L, "Assets:Load(asset, [type, callback, config]) - name must be of the form <repo>.<name>")
            return 0;
        }

        for (std::string elem : elements) {
            if (elem.empty()) {
                LUAUTILS_STACK_SIZE_ASSERT(L, 0);
                LUAUTILS_ERROR(L, "Assets:Load(asset, [type, callback, config]) - name must be of the form <repo>.<name>")
                return 0;
            }
        }

        const std::string repo = elements[0];
        const std::string name = elements[1];

        if (useLocal || fromAppBundle) {
            _g_assets_load_item(weakGame, requestID, repo, name, fromAppBundle, appBundleFallback, shapeSettings, filter, async);
            LUAUTILS_STACK_SIZE_ASSERT(L, 1)
            return 1;
        }

        // read local ETag value
        std::string etag = "";
#if defined(CLIENT_ENV)
        if (useCache) {
            int32_t creationTime = 0;
            bool cacheAvailable = cacheCPP_getItemCacheInfo(repo, name, etag, creationTime);
            if (cacheAvailable) {
                if (vx::device::timestampApple() - creationTime < DEFAULT_MAX_AGE) {
                    // get file from disk without request
                    _g_assets_load_item(weakGame, requestID, repo, name, fromAppBundle, appBundleFallback, shapeSettings, filter, async);
                    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
                    return 1;
                }
            }
        }
#endif

        // Request item from the API, considering the local cache
        vx::HubClient& client = vx::HubClient::getInstance();
        vx::HttpRequest_SharedPtr req = client.getItemModel(client.getUniversalSender(),
                                                          repo,
                                                          name,
                                                          false, /* forceCacheRevalidate */
                                                          [weakGame,
                                                           requestID,
                                                           filter,
                                                           fromAppBundle,
                                                           shapeSettings,
                                                           appBundleFallback,
                                                           async](const bool &success,
                                                                  vx::HttpRequest_SharedPtr req,
                                                                  const std::string& itemRepo,
                                                                  const std::string& itemName,
                                                                  const std::string& content,
                                                                  const bool& etagValid){

            auto func = [weakGame, requestID, success, itemRepo, itemName, content, etagValid, filter,
                fromAppBundle, shapeSettings, appBundleFallback, async](){

                // TODO: success true/false is not enough detail
                // a 404 shouldn't be handled like a network error
                if (success) {
                    if (etagValid == false && content.empty() == false) {
#if defined(CLIENT_ENV)
                        // .3zh bytes have been received, let's store it in local cache
                        if (saveItemInCache(itemRepo, itemName, content) == false) {
                            vxlog_debug("Assets:Load : failed to write file");
                            // TODO: error callback?
                        }
#endif
                    }
                }

                _g_assets_load_item(weakGame, requestID, itemRepo, itemName, fromAppBundle, appBundleFallback,
                                    shapeSettings, filter, async);
            };
            if (async) {
                vx::OperationQueue::getSlowBackground()->dispatch(func);
            } else {
                func();
            }
        });

        _lua_assets_request_attach_http_request(L, -1, req);
    }

    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
    return 1;
}

/// Wraps the whole Assets::Load process: optional HTTP request AND load from disk async operations.
/// Created request ID is returned through requestID pointer parameter.
/// /!\ Automatically registers AssetsRequest within Assets' pending requests.
/// @param gAssetsIdx stack index pointing to global Assets object
/// @param callbackIdx can be 0 if there's no Lua callback
/// @param requestID internal ID for this AssetsRequest
void _lua_assets_request_create_and_push(lua_State * const L,
                                         const int gAssetsIdx,
                                         const int callbackIdx,
                                         int *requestID) {
    vx_assert(L != nullptr);
    vx_assert(lua_utils_getObjectType(L, gAssetsIdx) == ITEM_TYPE_G_ASSETS);
    LUAUTILS_STACK_SIZE(L)

    vx::Game *g = nullptr;
    if (lua_utils_getGameCppWrapperPtr(L, &g) == false) {
        LUAUTILS_INTERNAL_ERROR(L);
    }

    if (availableRequestIDs.empty() == false) {
        *requestID = availableRequestIDs.front();
        availableRequestIDs.pop();
    } else {
        *requestID = nextRequestID;
        ++nextRequestID;
    }

    assetsRequest_userdata* ud = static_cast<assetsRequest_userdata*>(lua_newuserdatadtor(L, sizeof(assetsRequest_userdata), _assets_request_gc));
    ud->id = *requestID;

    // connect to userdata store
    ud->store = g->userdataStoreForAssetsRequests;
    ud->previous = nullptr;
    assetsRequest_userdata* next = static_cast<assetsRequest_userdata*>(ud->store->add(ud));
    ud->next = next;
    if (next != nullptr) {
        next->previous = ud;
    }

    ud->status = vx::HttpRequest::Status::PROCESSING;
    ud->req = nullptr;

    // -1: request

    // set metatable
    LUA_GET_METAFIELD(L, gAssetsIdx, "__imt");
    lua_setmetatable(L, -2);

    // store Request in the pending requests table
    LUA_GET_METAFIELD(L, gAssetsIdx, LUA_G_ASSETS_PENDING_REQUESTS);
    // -1: pending requests table
    // -2: request
    lua_pushvalue(L, -2);
    lua_rawseti(L, -2, *requestID);
    lua_pop(L, 1); // pop pending requests table

    if (callbackIdx > 0) {
        // store Callback
        LUA_GET_METAFIELD(L, gAssetsIdx, LUA_G_ASSETS_REQUEST_CALLBACKS);
        // -1: callbacks
        // -2: request
        lua_pushvalue(L, callbackIdx);
        lua_rawseti(L, -2, *requestID);
        lua_pop(L, 1); // pop callbacks
    }

    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
}

void _lua_assets_request_attach_http_request(lua_State * const L,
                                             const int assetsRequestIdx,
                                             const vx::HttpRequest_SharedPtr& ptr) {
    vx_assert(L != nullptr);
    vx_assert(lua_utils_getObjectType(L, assetsRequestIdx) == ITEM_TYPE_ASSETS_REQUEST);
    assetsRequest_userdata *ud = static_cast<assetsRequest_userdata*>(lua_touserdata(L, assetsRequestIdx));
    vx_assert(ud->req == nullptr);
    ud->req = new vx::HttpRequest_SharedPtr(ptr);
}

static int _assets_request_index(lua_State * const L) {
    vx_assert(L != nullptr);
    LUAUTILS_STACK_SIZE(L)

    vx::Game *cppGame = nullptr;
    if (lua_utils_getGameCppWrapperPtr(L, &cppGame) == false || cppGame == nullptr) {
        LUAUTILS_STACK_SIZE_ASSERT(L, 0);
        LUAUTILS_INTERNAL_ERROR(L);
        // return 0;
    }

    const char *key = lua_tostring(L, 2);
    if (strcmp(key, LUA_ASSETS_REQUEST_CANCEL) == 0) {
        lua_pushcfunction(L, _assets_request_cancel, "");
    } else if (strcmp(key, LUA_ASSETS_REQUEST_STATUS) == 0) {
        assetsRequest_userdata *ud = static_cast<assetsRequest_userdata*>(lua_touserdata(L, 1));
        lua_pushinteger(L, ud->status);
    } else {
        lua_pushnil(L);
    }
    LUAUTILS_STACK_SIZE_ASSERT(L, 1);
    return 1;
}

static void _assets_request_gc(void *_ud) {
    assetsRequest_userdata *ud = static_cast<assetsRequest_userdata*>(_ud);

    if (ud->req != nullptr) {
        (*(ud->req))->cancel();
        delete ud->req;
        ud->req = nullptr;
    }

    // disconnect from userdata store
    {
        if (ud->previous != nullptr) {
            ud->previous->next = ud->next;
        }
        if (ud->next != nullptr) {
            ud->next->previous = ud->previous;
        }
        ud->store->remove(ud, ud->next, ud->id);
    }
}

void lua_assets_request_gc_finalize(vx::UserDataStore *store, lua_State * const L) {
    FiloListUInt32 *l = store->getRemovedIDs();
    if (filo_list_uint32_is_empty(l)) { return; }

    if (L != nullptr) {
        int type = LUA_GET_GLOBAL_AND_RETURN_TYPE(L, LUA_G_ASSETS);
        vx_assert(type == LUA_TUSERDATA);
        LUA_GET_METAFIELD(L, -1, LUA_G_ASSETS_REQUEST_CALLBACKS);
    }

    uint32_t id;
    while (filo_list_uint32_pop(l, &id)) {
        if (L != nullptr) {
            lua_pushnil(L);
            lua_rawseti(L, -2, id);
        }
        availableRequestIDs.push(id);
    }

    if (L != nullptr) {
        lua_pop(L, 2); // pop request callbacks and Assets
    }
}

static int _assets_request_cancel(lua_State * const L) {
    LUAUTILS_STACK_SIZE(L)

    if (lua_gettop(L) < 1) {
        LUAUTILS_ERROR(L, "AssetsRequest:Cancel() should be called with :");
    } else if (lua_gettop(L) != 1) {
        LUAUTILS_ERROR(L, "AssetsRequest:Cancel() does not accept arguments");
    }

    // Cancel underlying HTTP request if there's one
    assetsRequest_userdata *ud = static_cast<assetsRequest_userdata*>(lua_touserdata(L, 1));
    if (ud->req != nullptr) {
        (*(ud->req))->cancel();
        delete ud->req;
        ud->req = nullptr;
    }
    ud->status = vx::HttpRequest::Status::CANCELLED;

    int type = LUA_GET_GLOBAL_AND_RETURN_TYPE(L, LUA_G_ASSETS);
    vx_assert(type == LUA_TUSERDATA);

    LUA_GET_METAFIELD(L, -1, LUA_G_ASSETS_REQUEST_CALLBACKS);
    vx_assert(lua_type(L, -1) == LUA_TTABLE);
    lua_pushnil(L);
    lua_rawseti(L, -2, ud->id);
    lua_pop(L, 1); // pop callbacks

    LUA_GET_METAFIELD(L, -1, LUA_G_ASSETS_PENDING_REQUESTS);
    vx_assert(lua_type(L, -1) == LUA_TTABLE);
    lua_pushnil(L);
    lua_rawseti(L, -2, ud->id);
    lua_pop(L, 1); // pop pending requests

    lua_pop(L, 1); // pop Assets

    LUAUTILS_STACK_SIZE_ASSERT(L, 0)
    return 0;
}
