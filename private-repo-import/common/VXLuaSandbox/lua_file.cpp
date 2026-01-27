//
//  lua_file.cpp
//  Cubzh
//
//  Created by Xavier Legland on 07/06/2022.
//  Copyright 2022 Voxowl Inc. All rights reserved.
//

#include "lua_file.hpp"

// Lua
#include "lua.hpp"
#include "lua_constants.h"
#include "lua_data.hpp"
#include "lua_logs.hpp"
#include "lua_utils.hpp"

#include "config.h"

#include "device.hpp"
#include "filesystem.hpp"
#include "GameCoordinator.hpp"
#include "VXHubClient.hpp"
#include "VXResource.hpp"
#include "cache.h"
#include "fileutils.h"
#include "serialization_vox.h"

// xptools
#include "OperationQueue.hpp"

using namespace vx;

#define LUA_G_FILE_FIELD_OPEN_AND_READ_ALL "OpenAndReadAll" // function
#define LUA_G_FILE_FIELD_EXPORT_ITEM "ExportItem" // function
#define LUA_G_FILE_PRIVATE_FIELD_IMPORT_CALLBACK "importCallback"
#define LUA_G_FILE_PRIVATE_FIELD_EXPORT_CALLBACK "exportCallback"

typedef void (*openCallbackFunction)(lua_State *L, lua_Integer callback);

// --------------------------------------------------
//
// MARK: - Static functions' prototypes -
//
// --------------------------------------------------

static int _g_file_newindex(lua_State *const L);
static int _g_file_open_and_read_all(lua_State *const L);
static int _g_file_export_item(lua_State *const L);

// --------------------------------------------------
//
// MARK: - Exposed functions -
//
// --------------------------------------------------

bool lua_g_file_create_and_push(lua_State *L) {
    vx_assert(L != nullptr);

    LUAUTILS_STACK_SIZE(L)

    lua_newtable(L);
    lua_newtable(L);
    {
        lua_pushstring(L, "__index");
        lua_newtable(L); // table for read-only keys
        {
            lua_pushstring(L, LUA_G_FILE_FIELD_OPEN_AND_READ_ALL);
            lua_pushcfunction(L, _g_file_open_and_read_all, "");
            lua_rawset(L, -3);

            lua_pushstring(L, LUA_G_FILE_FIELD_EXPORT_ITEM);
            lua_pushcfunction(L, _g_file_export_item, "");
            lua_rawset(L, -3);
        }
        lua_rawset(L, -3);

        lua_pushstring(L, "__newindex");
        lua_pushcfunction(L, _g_file_newindex, "");
        lua_rawset(L, -3);

        lua_pushstring(L, "__metatable");
        lua_pushboolean(L, false);
        lua_settable(L, -3);

        lua_pushstring(L, P3S_LUA_OBJ_METAFIELD_TYPE);
        lua_pushinteger(L, ITEM_TYPE_G_FILE);
        lua_settable(L, -3);
    }
    lua_setmetatable(L, -2);

    LUAUTILS_STACK_SIZE_ASSERT(L, 1);
    return true;
}

int lua_g_file_snprintf(lua_State *L, char *result, size_t maxLen, bool spacePrefix) {
    LUAUTILS_STACK_SIZE(L)
    LUAUTILS_STACK_SIZE_ASSERT(L, 0)
    return snprintf(result, maxLen, "%s[File]", spacePrefix ? " " : "");
}

// --------------------------------------------------
//
// MARK: - static functions
//
// --------------------------------------------------

static int _g_file_newindex(lua_State *L) {
    LUAUTILS_ERROR(L, "File is read-only")
    return 0;
}

static int _g_file_open_and_read_all(lua_State *L) {
    LUAUTILS_STACK_SIZE(L)

    const int argCount = lua_gettop(L);

    if (argCount < 1 || lua_utils_getObjectType(L, 1) != ITEM_TYPE_G_FILE) {
        LUAUTILS_ERROR(L, "File:OpenAndReadAll - function should be called with `:`")
    }

    if (argCount > 2 || lua_isfunction(L, 2) == false) {
        LUAUTILS_ERROR(L, "File:OpenAndReadAll - missing callback argument")
    }
    
    if (lua_getmetatable(L, 1) == 0) {
        LUAUTILS_INTERNAL_ERROR(L)
    }

    const int type = lua_getfield(L, -1, LUA_G_FILE_PRIVATE_FIELD_IMPORT_CALLBACK);
    if (type != LUA_TNIL) {
        lua_pop(L, 2); // pop field & metatable
        LUAUTILS_STACK_SIZE_ASSERT(L, 0)
        LUAUTILS_ERROR(L, "File:OpenAndReadAll - can't be called several times in parallel")
    }
    lua_pop(L, 1); // pop nil
    
    lua_pushstring(L, LUA_G_FILE_PRIVATE_FIELD_IMPORT_CALLBACK);
    lua_pushvalue(L, 2); // callback function
    lua_rawset(L, -3);
    
    lua_pop(L, 1); // pop metatable
    
    vx::Game *cppGame = nullptr;
    if (lua_utils_getGameCppWrapperPtr(L, &cppGame) == false) {
        LUAUTILS_ERROR(L, "HTTP:Get - failed to retrieve Game from lua state");
    }
    vx::Game_WeakPtr weakPtr = cppGame->getWeakPtr();

    vx::GameCoordinator::getInstance()->setSystemFileModalOpened(true);

    vx::fs::importFile([weakPtr](vx::fs::ImportFileCallbackStatus status, std::string fileBytes) {

        vx::GameCoordinator::getInstance()->setSystemFileModalOpened(false);

        vx::Game_SharedPtr cppGame = weakPtr.lock();
        if (cppGame == nullptr) {
            return;
        }
        
        vx::OperationQueue *mainQueue = cppGame->isInClientMode()
        ? vx::OperationQueue::getMain()
        : vx::OperationQueue::getServerMain();

        mainQueue->dispatch([weakPtr, fileBytes, status]() {
            vx::Game_SharedPtr cppGame = weakPtr.lock();
            if (cppGame == nullptr) {
                return;
            }
            
            lua_State *L = cppGame->getLuaState();
            
            LUAUTILS_STACK_SIZE(L)

            // retrieve the Lua function
            lua_getglobal(L, LUA_G_FILE);
            
            if (lua_getmetatable(L, -1) == 0) {
                LUAUTILS_INTERNAL_ERROR(L)
            }
            
            const int type = lua_getfield(L, -1, LUA_G_FILE_PRIVATE_FIELD_IMPORT_CALLBACK);
            if (type != LUA_TFUNCTION) {
                lua_pop(L, 3); // pop field, metatable and global File
                LUAUTILS_STACK_SIZE_ASSERT(L, 0)
                LUAUTILS_INTERNAL_ERROR(L)
            }
            
            // stack:
            // - callback function (-1)
            // - metatable (-2)
            // - global File (-3)

            if (status == vx::fs::ImportFileCallbackStatus::CANCELLED) {
                // CANCELLED: callback(true, nil)
                lua_pushboolean(L, true);
                lua_pushnil(L);
                if (lua_pcall(L, 2, 0, 0) != LUA_OK) {
                    if (lua_utils_isStringStrict(L, -1)) {
                        lua_log_error_CStr(L, lua_tostring(L, -1));
                    } else {
                        lua_log_error_CStr(L, "File:OpenAndReadAll - error in the callback");
                    }
                    lua_pop(L, 1); // pop error
                }
                
            } else if (status == vx::fs::ImportFileCallbackStatus::ERR) {
                // ERROR: callback(false, nil)
                lua_pushboolean(L, false);
                lua_pushnil(L);
                if (lua_pcall(L, 2, 0, 0) != LUA_OK) {
                    if (lua_utils_isStringStrict(L, -1)) {
                        lua_log_error_CStr(L, lua_tostring(L, -1));
                    } else {
                        lua_log_error_CStr(L, "File:OpenAndReadAll - error in the callback");
                    }
                    lua_pop(L, 1); // pop error
                }
                
            } else { // status == OK
                lua_pushboolean(L, true);
                if (lua_data_pushNewTable(L, fileBytes.c_str(), fileBytes.size()) == false) {
                    // error, could not push table -> callback(false, nil)
                    lua_pop(L, 1); // pop true
                    lua_pushboolean(L, false);
                    lua_pushnil(L);
                    if (lua_pcall(L, 2, 0, 0) != LUA_OK) {
                        if (lua_utils_isStringStrict(L, -1)) {
                            lua_log_error_CStr(L, lua_tostring(L, -1));
                        } else {
                            lua_log_error_CStr(L, "File:OpenAndReadAll - error in the callback");
                        }
                        lua_pop(L, 1); // pop error
                    }
                } else {
                    // all good -> callback(true, data)
                    if (lua_pcall(L, 2, 0, 0) != LUA_OK) {
                        if (lua_utils_isStringStrict(L, -1)) {
                            lua_log_error_CStr(L, lua_tostring(L, -1));
                        } else {
                            lua_log_error_CStr(L, "File:OpenAndReadAll - error in the callback");
                        }
                        lua_pop(L, 1); // pop error
                    }
                }
            }
            
            // stack:
            // - metatable (-1)
            // - global File (-2)
            
            lua_pushstring(L, LUA_G_FILE_PRIVATE_FIELD_IMPORT_CALLBACK);
            lua_pushnil(L);
            lua_rawset(L, -3);
            
            lua_pop(L, 2); // pop metatable & global File
           
            LUAUTILS_STACK_SIZE_ASSERT(L, 0)
            
        }); // main queue dispatch
    });

    LUAUTILS_STACK_SIZE_ASSERT(L, 0)
    return 0;
}

static void _g_file_export_callback(lua_State *L, bool error, std::string const& message) {
    lua_pushboolean(L, error);
    lua_pushstring(L, message.c_str());
    if (lua_pcall(L, 2, 0, 0) != LUA_OK) {
        if (lua_utils_isStringStrict(L, -1)) {
            lua_log_error_CStr(L, lua_tostring(L, -1));
        } else {
            lua_log_error_CStr(L, "File:ExportFile - error in the callback");
        }
        lua_pop(L, 1); // pop error
    }

    lua_pushstring(L, LUA_G_FILE_PRIVATE_FIELD_EXPORT_CALLBACK);
    lua_pushnil(L);
    lua_rawset(L, -3);
    
    lua_pop(L, 2); // pop metatable & global File
}

static int _g_file_export_item(lua_State *L) {
    LUAUTILS_STACK_SIZE(L)

    const int argCount = lua_gettop(L);

    if (argCount < 2 || lua_utils_getObjectType(L, 1) != ITEM_TYPE_G_FILE) {
        LUAUTILS_ERROR(L, "File:ExportItem - function should be called with `:`");
    }
    
    if (argCount < 5 || lua_isstring(L, 2) == false || lua_isstring(L, 3) == false || lua_isstring(L, 4) == false || lua_isfunction(L, 5) == false) {
        LUAUTILS_ERROR(L, "File:ExportItem - function should be called with 3 texts and a callback File:ExportItem(repo, name, fileType, callback)");
    }
    
    // Get parameters and save callback
    std::string repo = lua_tostring(L, 2);
    std::string name = lua_tostring(L, 3);
    std::string fileTypeStr = lua_tostring(L, 4);
    
    if (lua_getmetatable(L, 1) == 0) {
        LUAUTILS_INTERNAL_ERROR(L)
    }

    const int type = lua_getfield(L, -1, LUA_G_FILE_PRIVATE_FIELD_EXPORT_CALLBACK);
    if (type != LUA_TNIL) {
        lua_pop(L, 2); // pop field & metatable
        LUAUTILS_STACK_SIZE_ASSERT(L, 0)
        LUAUTILS_ERROR(L, "File:ExportItem - can't be called several times in parallel")
    }
    lua_pop(L, 1); // pop nil
    
    lua_pushstring(L, LUA_G_FILE_PRIVATE_FIELD_EXPORT_CALLBACK);
    lua_pushvalue(L, 5); // callback function
    lua_rawset(L, -3);
    
    lua_pop(L, 1); // pop metatable

    // Handle etag (cache info)
    bool cacheInfoAvailable = false;
    md5CStr cacheEtag = md5CStrZero;
    {
        std::string cacheInfoFile = repo + "/" + name + ".3zh";
        const bool ok = cache_getCacheInfoForItem(cacheInfoFile.c_str(), &cacheEtag, nullptr);
        if (ok) {
            cacheInfoAvailable = true;
        }
    }

    std::string etag;
    if (cacheInfoAvailable) {
        etag.assign(cacheEtag);
    }

    std::string path = repo + "." + name;
    path = std::string(cache_ensure_path_format(path.c_str(), true));
    path = "cache/" + path;

    vx::Game *cppGame = nullptr;
    if (lua_utils_getGameCppWrapperPtr(L, &cppGame) == false) {
        LUAUTILS_ERROR(L, "HTTP:Get - failed to retrieve Game from lua state");
    }
    vx::Game_WeakPtr weakPtr = cppGame->getWeakPtr();

    // Get Item
    HubClient::getInstance().getItemModel(HubClient::getInstance().getUniversalSender(),
                                        repo, name, true /* forceCacheRevalidate */,
                                        [path, fileTypeStr, weakPtr]
                                        (const bool &success,
                                         HttpRequest_SharedPtr req,
                                         const std::string &itemRepo,
                                         const std::string &itemName,
                                         const std::string &content,
                                         const bool &etagValid) {
        vx::Game_SharedPtr cppGame = weakPtr.lock();
        if (cppGame == nullptr) {
            return;
        }
        
        lua_State *L = cppGame->getLuaState();
        
        // retrieve the Lua function
        lua_getglobal(L, LUA_G_FILE);
        
        if (lua_getmetatable(L, -1) == 0) {
            LUAUTILS_INTERNAL_ERROR(L)
        }
        
        const int type = lua_getfield(L, -1, LUA_G_FILE_PRIVATE_FIELD_EXPORT_CALLBACK);
        if (type != LUA_TFUNCTION) {
            lua_pop(L, 3); // pop field, metatable and global File
            LUAUTILS_INTERNAL_ERROR(L)
            return;
        }
        
        if (!success) {
            _g_file_export_callback(L, true, VXPOPUP_ERR_EXPORT_BODY);
            return;
        }

#ifdef CLIENT_ENV
        // update the local file if needed
        if (etagValid == false) {
            // update the file in cache
            if (saveItemInCache(itemRepo, itemName, content) == false) {
                _g_file_export_callback(L, true, VXPOPUP_ERR_EXPORT_BODY);
                return;
            }
        }
#endif
        std::string exportPath = "";
        fs::FileType fileType = fs::FileType::NONE;
        if (fileTypeStr == "3zh") {
            fileType = fs::FileType::CUBZH;
        } else if (fileTypeStr == "vox") {
            fileType = fs::FileType::VOX;
        }

        // Note: we do not remove exported file as it needs to be accessed by system once the action
        // is chosen by user, on Android for example, this seems to be asynchronous
        switch (fileType) {
        case fs::FileType::CUBZH:
        {
            // the pcubes is stored on the local storage
            FILE *localFile = fs::openStorageFile(path);
            if (localFile == nullptr) {
                vxlog_error("[File:ExportItem] failed open cubzh file to export");
                _g_file_export_callback(L, true, VXPOPUP_ERR_EXPORT_BODY);
                return;
            }

            size_t byteCount = 0;
            void* bytes = fs::getFileContent(localFile, &byteCount);
            if (bytes == nullptr) {
                vxlog_error("[File:ExportItem] failed open pcubes file to export (bytes is NULL)");
                _g_file_export_callback(L, true, VXPOPUP_ERR_EXPORT_BODY);
                return;
            }
            std::string localPcubes(reinterpret_cast<char*>(bytes), byteCount);
            free(bytes);
            bytes = nullptr;

            // copy it from cache/username/ to exports/ which is a declared path for file provider
            exportPath = "exports/" + itemName + ".3zh";
            FILE *fd = fs::openStorageFile(exportPath, "wb", byteCount);
            bool written = false;
            if (fd != nullptr) {
                written = fwrite(localPcubes.c_str(), sizeof(char), byteCount, fd) == byteCount;
                fclose(fd);
            } else {
                vxlog_error("[File:ExportItem] failed to write export file");
            }

            if (written == false) {
                _g_file_export_callback(L, true, VXPOPUP_ERR_EXPORT_BODY);
                return;
            }
            break;
        }
        case vx::fs::FileType::VOX:
        {
            // create file in exports/ which is a declared path for file provider
            exportPath = "exports/" + itemName + ".vox";
            std::string repoAndName = itemRepo + "." + itemName;

            ColorAtlas *ca = color_atlas_new();
            ShapeSettings shapeSettings;
            shapeSettings.lighting = false;
            shapeSettings.isMutable = false;
            Shape *shape = serialization_load_shape_from_cache(repoAndName.c_str(),
                                                               repoAndName.c_str(),
                                                               ca,
                                                               &shapeSettings);

            FILE *voxFile = fs::openStorageFile(exportPath, "wb");
            const bool ok = serialization_vox_save(shape, voxFile);
            fclose(voxFile);

            shape_release(shape);
            color_atlas_free(ca);

            if (ok == false) {
                fs::removeStorageFileOrDirectory(exportPath);

//                // check if the shape is too long
//                SHAPE_SIZE_INT3_T shape_size = shape_get_allocated_size(shape);
//                PopupText_SharedPtr popup = nullptr;
//                if (shape_size.x > 256 || shape_size.y > 256 || shape_size.z > 256) {
//                    popup = PopupText::make(VXPOPUP_ERR_GENERIC_TITLE,
//                                            VXPOPUP_ERR_EXPORT_BODY_TOO_BIG);
//                } else {
//                    popup = PopupText::make(VXPOPUP_ERR_GENERIC_TITLE, VXPOPUP_ERR_EXPORT_BODY);
//                }
//                GameCoordinator::getInstance()->showPopup(popup);
                return;
            }
            break;
        }
        default:
            vxlog_error("Export: type not supported");
            _g_file_export_callback(L, true, VXPOPUP_ERR_EXPORT_FILE_FORMAT);
            return;
        }

        OperationQueue::getMain()->dispatch([weakPtr, exportPath, itemName, fileType]() {
            vx::Game_SharedPtr cppGame = weakPtr.lock();
            if (cppGame == nullptr) {
                return;
            }
            lua_State *L = cppGame->getLuaState();
            fs::shareFile(exportPath, "Export Item", itemName, fileType);
            _g_file_export_callback(L, false, "✅ Done Exporting!");
        });
    });
   
    LUAUTILS_STACK_SIZE_ASSERT(L, 0)
    return 0;
}
