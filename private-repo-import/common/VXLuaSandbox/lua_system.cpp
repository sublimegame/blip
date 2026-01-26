//
//  lua_system.cpp
//  Cubzh
//
//  Created by Corentin Cailleaud on 24/05/2023.
//

#include "lua_system.hpp"

// C++
#include <cstring>

// Cubzh Core
#include "config.h"
#include "BZMD5.hpp"

// xptools
#include "xptools.h"
#include "crypto.hpp"
#include "notifications.hpp"
#include "OperationQueue.hpp"
#include "ThreadManager.hpp"
#include "passkey.hpp"
#include "iap.hpp"
#include "xp_http.hpp"

// Lua sandbox
#include "lua_http.hpp"
#include "lua_data.hpp"
#include "lua_json.hpp"
#include "lua_utils.hpp"
#include "lua_logs.hpp"
#include "lua_player.hpp"
#include "lua_shape.hpp"
#include "lua_mutableShape.hpp"
#include "lua_collision_groups.hpp"
#include "lua_object.hpp"
#include "lua_safe_local_store.hpp"
#include "utils.h"
#include "lua_constants.h"
#include "lua_local_event.hpp"
#include "VXNode.hpp"
#include "VXPrefs.hpp"
#include "VXApplication.hpp"
#include "VXGameRenderer.hpp"

#include "cache.h"
#include "fileutils.h"

#if defined(ONLINE_GAMESERVER)
#define GAMESERVER_API_HOST "api.particubes.com"
#define GAMESERVER_API_PORT 10083
#endif

// NOTIFICATIONS

#define LUA_SYSTEM_FIELD_NOTIFICATION_COUNT "NotificationCount" // read-write integer (0 hides the badge)
// read-only function - returns "underdetermined", "denied", "authorized"
#define LUA_SYSTEM_FIELD_NOTIFICATION_GET_STATUS "NotificationGetStatus"
// read-only function(callback(response)) - possible responses: "authorized", "denied", "error, "not_supported"
#define LUA_SYSTEM_FIELD_REMOTE_NOTIF_REQUEST_ACCESS "NotificationRequestAuthorization"
#define LUA_SYSTEM_FIELD_REMOTE_NOTIF_REFRESH_PUSH_TOKEN "NotificationRefreshPushToken"
#define LUA_SYSTEM_FIELD_REMOTE_NOTIF_POSTPONE_AUTHORIZATION "NotificationPostponeAuthorization"
// read-only function
#define LUA_SYSTEM_FIELD_OPEN_APP_SETTINGS "OpenAppSettings"

// HTTP fields
#define LUA_SYSTEM_FIELD_HTTP_GET "HttpGet"                                     // read-only function
#define LUA_SYSTEM_FIELD_HTTP_POST "HttpPost"                                   // read-only function
#define LUA_SYSTEM_FIELD_HTTP_PATCH "HttpPatch"                                 // read-only function
#define LUA_SYSTEM_FIELD_HTTP_DELETE "HttpDelete"                               // read-only function
#define LUA_SYSTEM_FIELD_OPEN_URL "OpenURL"                                     // read-only function

// KVS HTTP Requests
#define LUA_SYSTEM_FIELD_KVS_GET "KvsGet"                                       // read-only function
#define LUA_SYSTEM_FIELD_KVS_SET "KvsSet"                                       // read-only function
#define LUA_SYSTEM_FIELD_KVS_REMOVE "KvsRemove"                                 // read-only function

// Elevated API fields
#define LUA_SYSTEM_FIELD_WORLD "World"
#define LUA_SYSTEM_FIELD_SETLAYERS "SetLayersElevated"
#define LUA_SYSTEM_FIELD_GETLAYERS "GetLayersElevated"
#define LUA_SYSTEM_FIELD_GETGROUPS "GetGroupsElevated"
#define LUA_SYSTEM_FIELD_SETGROUPS "SetCollisionGroupsElevated"
#define LUA_SYSTEM_FIELD_SETCOLLIDESWITH "SetCollidesWithGroupsElevated"
#define LUA_SYSTEM_FIELD_MULTILINEINPUT "MultilineInput"
#define LUA_SYSTEM_FIELD_LOGOUT "Logout"                                        // read-only function
#define LUA_SYSTEM_FIELD_LOGOUT_AND_EXIT "LogoutAndExit"                        // read-only function
#define LUA_SYSTEM_FIELD_EXECCOMMAND "ExecCommand"                              // read-only function
#define LUA_SYSTEM_FIELD_DEBUGEVENT "DebugEvent"                                // read-only function
#define LUA_SYSTEM_FIELD_HASENVIRONMENTTOLAUNCH "HasEnvironmentToLaunch"        // read-only function
#define LUA_SYSTEM_FIELD_LAUNCHENVIRONMENT "LaunchEnvironment"                  // read-only function
#define LUA_SYSTEM_FIELD_CONNECT_TO_SERVER "ConnectToServer"                    // read-only function
#define LUA_SYSTEM_FIELD_LOG "Log"

#define LUA_SYSTEM_FIELD_IS_HOME_APP_RUNNING "IsHomeAppRunning" // read-only boolean

// CONNECTED ACCOUNT INFO

#define LUA_SYSTEM_FIELD_HAS_CREDENTIALS "HasCredentials"                       // boolean (read-only)
#define LUA_SYSTEM_FIELD_IS_USER_UNDER_13 "IsUserUnder13"                       // boolean (read-only)
#define LUA_SYSTEM_FIELD_IS_PARENT_APPROVED "IsParentApproved"                  // boolean
#define LUA_SYSTEM_FIELD_IS_CHAT_ENABLED "IsChatEnabled"                        // boolean
#define LUA_SYSTEM_FIELD_USER_ID "UserID"                                       // string (read-only)
#define LUA_SYSTEM_FIELD_REMOVE_CREDENTIALS "RemoveCredentials"                 // function (read-only)
#define LUA_SYSTEM_FIELD_UNDER_13_DISCLAIMER_NEEDS_APPROVAL "Under13DisclaimerNeedsApproval" // boolean (read-only)
#define LUA_SYSTEM_FIELD_APPROVE_UNDER_13_DISCLAIMER "ApproveUnder13Disclaimer" // function (read-only)

#define LUA_SYSTEM_FIELD_AUTHENTICATED "Authenticated"                          // boolean (read-write)
#define LUA_SYSTEM_FIELD_ASKEDFORMAGICKEY "AskedForMagicKey"                    // boolean (read-write)
#define LUA_SYSTEM_FIELD_HAS_VERIFIED_PHONE_NUMBER "HasVerifiedPhoneNumber"     // boolean (read-write)
#define LUA_SYSTEM_FIELD_HAS_UNVERIFIED_PHONE_NUMBER "HasUnverifiedPhoneNumber" // boolean (read-write)
#define LUA_SYSTEM_FIELD_HAS_EMAIL "HasEmail"                                   // boolean (read-write)
#define LUA_SYSTEM_FIELD_IS_PHONE_EXEMPTED "IsPhoneExempted"                    // boolean (read-write)
#define LUA_SYSTEM_FIELD_HAS_DOB "HasDOB"                                       // boolean (read-write)
#define LUA_SYSTEM_FIELD_HAS_ESTIMATED_DOB "HasEstimatedDOB"                    // boolean (read-write)
#define LUA_SYSTEM_FIELD_USERNAME "Username"                                    // boolean (read-write)
#define LUA_SYSTEM_FIELD_SAVEDUSERNAMEOREMAIL "SavedUsernameOrEmail"            // string (read-write)
#define LUA_SYSTEM_FIELD_BLOCKED_USERS "BlockedUsers"

#define LUA_SYSTEM_FIELD_STORECREDENTIALS "StoreCredentials"                    // function (read-only)
#define LUA_SYSTEM_FIELD_DATAFROMBUNDLE "DataFromBundle"                        // function (read-only)
#define LUA_SYSTEM_FIELD_POINTERFORCESHOWN "PointerForceShown"                  //
#define LUA_SYSTEM_FIELD_JOINWORLD "JoinWorld"                                  //
#define LUA_SYSTEM_FIELD_EDITWORLD "EditWorld"                                  //
#define LUA_SYSTEM_FIELD_EDITWORLDCODE "EditWorldCode"                          //
#define LUA_SYSTEM_FIELD_GETMETATABLE "GetMetatable"                            //
#define LUA_SYSTEM_FIELD_ISCPPMENUACTIVE "IsCppMenuActive"                      //
#define LUA_SYSTEM_FIELD_OPENWEBMODAL "OpenWebModal"                            //

#define LUA_SYSTEM_FIELD_SCRIPT "Script"
#define LUA_SYSTEM_FIELD_PUBLISH_SCRIPT "PublishScript"
#define LUA_SYSTEM_FIELD_ROLLBACK "Rollback"

// Commands History fields
#define LUA_SYSTEM_FIELD_ADD_COMMAND_IN_HISTORY "AddCommandInHistory"           // read-only function
#define LUA_SYSTEM_FIELD_GET_COMMAND_FROM_HISTORY "GetCommandFromHistory"       // read-only function
#define LUA_SYSTEM_FIELD_NB_COMMANDS_IN_HISTORY "NbCommandsInHistory"           // read-only integer

#define LUA_SYSTEM_FIELD_TERMINATE "Terminate"                                  // read-only function

#define LUA_SYSTEM_FIELD_MASTER_VOLUME "MasterVolume"                           // number

#define LUA_SYSTEM_FIELD_SEND_CHAT_MESSAGE "SendChatMessage"

#define LUA_SYSTEM_FIELD_CRASH "Crash" // function

#define LUA_SYSTEM_FIELD_WORLD_ID "WorldID"

#define LUA_SYSTEM_WORLD_EDITOR_PLAY "WorldEditorPlay"
#define LUA_SYSTEM_WORLD_EDITOR_STOP "WorldEditorStop"

// Badges
#define LUA_SYSTEM_FIELD_COMPUTEBADGESIGNATURE "ComputeBadgeSignature"

// Networking
#define LUA_SYSTEM_FIELD_CLEARCACHE "ClearCache"
#define LUA_SYSTEM_FIELD_CLEARHTTPCACHEFORURL "ClearHttpCacheForUrl"

// Signup / Login
#define LUA_SYSTEM_FIELD_PASSKEYSUPPORTED "PasskeySupported"
#define LUA_SYSTEM_FIELD_PASSKEYCREATE "PasskeyCreate"
#define LUA_SYSTEM_FIELD_PASSKEYLOGIN "PasskeyLogin"

#define LUA_SYSTEM_IAP_IS_AVAILABLE "IAPIsAvailable"
#define LUA_SYSTEM_IAP_PURCHASE "IAPPurchase"
#define LUA_SYSTEM_IAP_PRODUCTS "IAPProducts"

using namespace vx;

extern "C" {

static int clearCache(lua_State *L);
static int readCode(lua_State *L);
static int editCode(lua_State *L);
static int goHome(lua_State *L);
static int editWorld(lua_State *L);
static int editWorldCode(lua_State *L);
static int getMetatable(lua_State *L);
static int openWebModal(lua_State *L);
static int joinWorld(lua_State *L);
static int dataFromBundle(lua_State *L); // DEPRECATED, use Data:FromBundle
static int launchItemEditor(lua_State *L);
static int storeCredentials(lua_State *L);
static int removeCredentials(lua_State *L);

static int clearCache(lua_State *L) {
    LUAUTILS_STACK_SIZE(L)

    // clear the app's own cache systems
    fs::removeStorageDirectoryRecurse("cache");
    fs::removeStorageDirectoryRecurse("http_cache");
    fs::removeStorageDirectoryRecurse("modules");

    // clear the operating system HTTP cache for the app
    vx::http::clearSystemHttpCache();

    LUAUTILS_STACK_SIZE_ASSERT(L, 0)
    return 0;
}

// arguments:
// - worldId
// - badgeTag
static int computeBadgeSignature(lua_State *L) {
    LUAUTILS_STACK_SIZE(L)

    // ensure the function is called with `:`
    const int argCount = lua_gettop(L);
    if (argCount < 1 || lua_utils_getObjectType(L, 1) != ITEM_TYPE_SYSTEM) {
        LUAUTILS_ERROR(L, "System:ComputeBadgeSignature - function should be called with `:`");
    }

    // check arguments count
    if (argCount < 3) {
        LUAUTILS_ERROR(L, "System:ComputeBadgeSignature expects a URL");
    }

    // ensure the 2nd and 3rd arguments are strings
    if (lua_isstring(L, 2) == false || lua_isstring(L, 3) == false) {
        LUAUTILS_ERROR(L, "System.ComputeBadgeSignature wrong arguments (string, string)");
    }

    // get arguments' values
    std::string worldId(lua_tostring(L, 2));
    std::string badgeTag(lua_tostring(L, 3));

    // compute signature
    std::string s = md5("item:" + worldId + ":" + badgeTag + "2019");

    lua_pushstring(L, s.c_str());

    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
    return 1;
}

static int clearHttpCacheForUrl(lua_State *L) {
    LUAUTILS_STACK_SIZE(L)

    // ensure the function is called with `:`
    const int argCount = lua_gettop(L);
    if (argCount < 1 || lua_utils_getObjectType(L, 1) != ITEM_TYPE_SYSTEM) {
        LUAUTILS_ERROR(L, "System:ClearHttpCacheForUrl - function should be called with `:`");
    }

    // ensure the function is called with 2 arguments
    if (argCount < 2) {
        LUAUTILS_ERROR(L, "System:ClearHttpCacheForUrl expects a URL");
    }

    // ensure the second argument is a string
    if (lua_isstring(L, 2) == false) {
        LUAUTILS_ERROR(L, "System.ClearHttpCacheForUrl expects a string argument");
    }

    std::string urlStr(lua_tostring(L, 2));
    // vx::URL urlObj = vx::URL::make(urlStr);

    vx::http::clearSystemHttpCacheForURL(urlStr);

    LUAUTILS_STACK_SIZE_ASSERT(L, 0)
    return 0;
}

static int readCode(lua_State *L) {
    LUAUTILS_STACK_SIZE(L)
    GameCoordinator::getInstance()->setState(State::activeWorldCodeReader);
    LUAUTILS_STACK_SIZE_ASSERT(L, 0)
    return 0;
}

static int editCode(lua_State *L) {
    LUAUTILS_STACK_SIZE(L)
    GameCoordinator::getInstance()->setState(State::activeWorldCodeEditor);
    LUAUTILS_STACK_SIZE_ASSERT(L, 0)
    return 0;
}

static int goHome(lua_State *L) {
    LUAUTILS_STACK_SIZE(L)
    GameCoordinator::getInstance()->exitWorldEditor();
    GameCoordinator::getInstance()->loadHome("", false, false, std::unordered_map<std::string, std::string>());
    LUAUTILS_STACK_SIZE_ASSERT(L, 0)
    return 0;
}

static int editWorld(lua_State *L) {
    LUAUTILS_STACK_SIZE(L)

    const int argCount = lua_gettop(L);
    if (argCount < 1) { return 0; }

    if (lua_isstring(L, 1) == false) {
        vxlog_error("editWorld expects a game ID (string)");
        return 0;
    }

    std::string gameID = std::string(lua_tostring(L, 1));

    GameCoordinator::getInstance()->quickGameJoin(gameID,
                                                  true, // isAuthor
                                                  GAME_LAUNCH_DEV_MODE,
                                                  GAME_LAUNCH_DISTANT_SERVER);
    LUAUTILS_STACK_SIZE_ASSERT(L, 0)
    return 0;
}

static int editWorldCode(lua_State *L) {
    LUAUTILS_STACK_SIZE(L)

    const int argCount = lua_gettop(L);
    if (argCount < 1) { return 0; }

    if (lua_isstring(L, 1) == false) {
        vxlog_error("System.EditWorldCode expects a World ID (string)");
        return 0;
    }

    vx::Game *cppGame = nullptr;
    if (lua_utils_getGameCppWrapperPtr(L, &cppGame) == false) {
        LUAUTILS_INTERNAL_ERROR(L);
    }

    Game_WeakPtr sender = cppGame->getWeakPtr();

    std::string worldID = std::string(lua_tostring(L, 1));

    HubClient::getInstance().getWorldData(sender,
                                          worldID,
                                          [worldID, sender](const bool &success,
                                                            const HTTPStatus &status,
                                                            const std::string &script,
                                                            const std::string &mapBase64,
                                                            const std::string &map3zh,
                                                            const int maxPlayers,
                                                            std::unordered_map<std::string, std::string> env) {

        if (success == false) {
            return;
        }

        Game_SharedPtr game = sender.lock();
        if (game == nullptr) {
            return;
        }

        OperationQueue::getMain()->dispatch([worldID, script](){
            GameCoordinator::getInstance()->setupCodeEditor(worldID, script, false);
            GameCoordinator::getInstance()->setState(State::worldCodeEditor);
        });
    }, nullptr);

    LUAUTILS_STACK_SIZE_ASSERT(L, 0)
    return 0;
}

static int getMetatable(lua_State *L) {
    LUAUTILS_STACK_SIZE(L)

    const int argCount = lua_gettop(L);
    if (argCount < 1) { return 0; }

    if (lua_isuserdata(L, 1) == false && lua_istable(L, 1) == false) {
        vxlog_error("System.GetMetatable expects a table argument");
        return 0;
    }

    if (lua_getmetatable(L, 1) == 0) {
        LUAUTILS_STACK_SIZE_ASSERT(L, 0)
        return 0;
    }

    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
    return 1;
}

static int openWebModal(lua_State *L) {
    LUAUTILS_STACK_SIZE(L)

    const int argCount = lua_gettop(L);
    if (argCount < 1 || lua_utils_getObjectType(L, 1) != ITEM_TYPE_SYSTEM) {
        LUAUTILS_ERROR(L, "System:OpenWebModal - function should be called with `:`");
    }

    if (argCount < 2) {
        LUAUTILS_ERROR(L, "System:OpenWebModal expects a URL");
    }

    if (lua_isstring(L, 2) == false) {
        LUAUTILS_ERROR(L, "System.OpenWebModal expects a URL");
    }

    std::string url = std::string(lua_tostring(L, 2));
    vx::Web::openModal(url);

    LUAUTILS_STACK_SIZE_ASSERT(L, 0)
    return 0;
}

static int joinWorld(lua_State *L) {
    LUAUTILS_STACK_SIZE(L)

    const int argCount = lua_gettop(L);
    if (argCount < 1 || argCount > 2) { return 0; }

    if (lua_isstring(L, 1) == false) {
        vxlog_error("joinWorld expects a game ID (string)");
        return 0;
    }

    std::string gameID = std::string(lua_tostring(L, 1));

    auto itemsToLoad = std::vector<std::string>(); // no loaded items
    auto env = std::unordered_map<std::string, std::string>(); // no specific env

    if (lua_isstring(L, 2)) {
        std::string serverAddress = std::string(lua_tostring(L, 2));

        if (gameID == WORLD_HOME_ID) {
            GameCoordinator::getInstance()->loadHome(serverAddress, false, false, env);
            LUAUTILS_STACK_SIZE_ASSERT(L, 0)
            return 0;
        }

        GameCoordinator::getInstance()->loadWorld(GameMode_Client,
                                                  nullptr, // no local data
                                                  WorldFetchInfo::makeWithID(gameID),
                                                  serverAddress,
                                                  false,
                                                  false,
                                                  itemsToLoad,
                                                  env);
        LUAUTILS_STACK_SIZE_ASSERT(L, 0)
        return 0;
    }

    if (gameID == WORLD_HOME_ID) {
        GameCoordinator::getInstance()->loadHome("", false, false, env);
        LUAUTILS_STACK_SIZE_ASSERT(L, 0)
        return 0;
    }

    GameCoordinator::getInstance()->quickGameJoin(gameID,
                                                  false, // isAuthor
                                                  GAME_LAUNCH_NOT_DEV_MODE,
                                                  GAME_LAUNCH_DISTANT_SERVER);
    LUAUTILS_STACK_SIZE_ASSERT(L, 0)
    return 0;
}

static int dataFromBundle(lua_State *L) {
    LUAUTILS_STACK_SIZE(L)

    const int argCount = lua_gettop(L);

    if (argCount < 1 || lua_isstring(L, 1) == false) {
        LUAUTILS_ERROR(L, "System.DataFromBundle expects parameter to be a string")
    }

    const char *filepath = lua_tostring(L, 1);

    FILE *file = vx::fs::openBundleFile(filepath);
    if (file == nullptr) {
        // can't open file (not found?), don't return anything
        LUAUTILS_STACK_SIZE_ASSERT(L, 0)
        return 0;
    }
    size_t len;
    void *content = vx::fs::getFileContent(file, &len);

    lua_data_pushNewTable(L, content, len);

    free(content); // copied by lua_data_pushNewTable

    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
    return 1;
}

static int launchItemEditor(lua_State *L) {
    LUAUTILS_STACK_SIZE(L)

    const int argCount = lua_gettop(L);
    if (argCount < 1) { return 0; }

    if (lua_isstring(L, 1) == false) {
        vxlog_error("launchItemEditor expects item fullname string parameter");
        return 0;
    }

    std::string itemFullName = std::string(lua_tostring(L, 1));
    std::string category = "";

    if (argCount >= 2 && lua_isstring(L, 2)) {
        category = std::string(lua_tostring(L, 2));
    }

    std::vector<std::string> parts = vx::str::splitString(itemFullName, ".");
    if (parts.size() != 2) {
        LUAUTILS_ERROR(L, "launchItemEditor - item name should be of the form repo.name");
        return 0;
    }

    GameCoordinator::getInstance()->launchLocalItemEditor(parts[0], parts[1], category);
    LUAUTILS_STACK_SIZE_ASSERT(L, 0)
    return 0;
}

static int storeCredentials(lua_State *L) {
    LUAUTILS_STACK_SIZE(L)

    const int argCount = lua_gettop(L);
    if (argCount < 1 || lua_utils_getObjectType(L, 1) != ITEM_TYPE_SYSTEM) {
        LUAUTILS_ERROR(L, "System:StoreCredentials - function should be called with `:`");
    }

    if (argCount < 2 || lua_isstring(L, 2) == false) {
        LUAUTILS_ERROR(L, "System:StoreCredentials(userID, token) - userID should be a string");
    }

    if (argCount < 3 || lua_isstring(L, 3) == false) {
        LUAUTILS_ERROR(L, "System:StoreCredentials(userID, token) - token should be a string");
    }

    std::string userID(lua_tostring(L, 2));
    std::string token(lua_tostring(L, 3));

    HubClient::getInstance().setCredentials(&userID, &token);

    // Update local player's username
    Game *game = nullptr;
    if (lua_utils_getGameCppWrapperPtr(L, &game) == false || game == nullptr) {
        LUAUTILS_INTERNAL_ERROR(L)
    }
    game->addOrUpdateLocalPlayer(nullptr, nullptr, &userID);

    LUAUTILS_STACK_SIZE_ASSERT(L, 0)
    return 0;
}

static int removeCredentials(lua_State *L) {
    LUAUTILS_STACK_SIZE(L)

    const int argCount = lua_gettop(L);
    if (argCount < 1 || lua_utils_getObjectType(L, 1) != ITEM_TYPE_SYSTEM) {
        LUAUTILS_ERROR(L, "System:RemoveCredentials - function should be called with `:`");
    }

    std::string emptyString = "";
    HubClient::getInstance().setCredentials(&emptyString, &emptyString);
    LUAUTILS_STACK_SIZE_ASSERT(L, 0)
    return 0;
}

}

static int _newindex_readonly(lua_State * const L);

static int _getFromClipboard(lua_State *L);
static int _system_index(lua_State *L);
static int _system_newindex(lua_State *L);

// Notifications
static int _remoteNotifGetStatus(lua_State * const L);
static int _remoteNotifRequestAccess(lua_State * const L);
static int _remoteNotifRefreshPushToken(lua_State * const L);
static int _remoteNotifPostponeAuthorization(lua_State * const L);
static int _openAppSettings(lua_State * const L);

// CHAT
static int _system_send_chat_message(lua_State * const L);

// CRASH
static int _system_crash(lua_State * const L);

// WORLD EDITOR
static int _system_world_editor_play(lua_State * const L);
static int _system_world_editor_stop(lua_State * const L);

// HTTP requests
static int _system_http_get(lua_State * const L);
static int _system_http_post(lua_State * const L);
static int _system_http_patch(lua_State * const L);
static int _system_http_delete(lua_State * const L);
static int _system_open_url(lua_State * const L);

static int _system_publish_script(lua_State * const L);
static int _system_rollback(lua_State * const L);
static int _system_iap_purchase(lua_State * const L);
static int _system_iap_get_products(lua_State * const L);

// KVS
static int _system_kvs_get(lua_State * const L);
static int _system_kvs_set(lua_State * const L);
static int _system_kvs_remove(lua_State * const L);
static int _kvs_get_callback(lua_State * const L, vx::HttpRequest_SharedPtr const &req, vx::HttpResponse const &resp);
static int _kvs_set_callback(lua_State * const L, vx::HttpRequest_SharedPtr const &req, vx::HttpResponse const &resp);

// Elevated API functions
static int _set_layers_elevated(lua_State * const L);
static int _get_layers_elevated(lua_State * const L);
static int _get_groups_elevated(lua_State * const L);
static int _set_groups_elevated(lua_State * const L);
static int _set_collideswith_elevated(lua_State * const L);
static int _multilineinput(lua_State * const L);
static int _logout(lua_State * const L);
static int _logoutAndExit(lua_State * const L);
static int _execCommand(lua_State * const L);
static int _debugEvent(lua_State * const L);
static int _launchEnvironment(lua_State * const L);
static int _connectToServer(lua_State * const L);
static int _log(lua_State * const L);

// Elevated API functions
static int _add_command_in_history(lua_State *L);
static int _get_command_from_history(lua_State *L);
static int _terminate(lua_State *L);

// Signup / Login
static int _system_passkey_create(lua_State *L);
static int _system_passkey_login(lua_State *L);

static int _approveUnder13Disclaimer(lua_State * const L);

void lua_init_root_and_system_globals(lua_State * const L) {
    LUAUTILS_STACK_SIZE(L)

    lua_pushvalue(L, LUA_GLOBALSINDEX);
    lua_utils_setRootGlobalsInRegistry(L, -1); // save reference on root globals
    lua_pop(L, 1);

    // create new table (System Globals), wrapping Global table and inserting `System` field
    lua_newtable(L);
    lua_newtable(L); // metatable, proxying read to root globals
    {
        lua_pushstring(L, "__index");
        lua_pushvalue(L, LUA_GLOBALSINDEX);
        lua_rawset(L, -3);
        // lua_setfield(L, -2, "__index");
        lua_setreadonly(L, -1, true);
    }
    lua_setmetatable(L, -2);

    // create `System` table
    lua_pushstring(L, P3S_LUA_G_SYSTEM);
    lua_newtable(L);
    lua_newtable(L); // System's metatable
    {
        lua_pushstring(L, "__index");
        lua_pushcfunction(L, _system_index, "");
        lua_rawset(L, -3);

        lua_pushstring(L, "__newindex");
        lua_pushcfunction(L, _system_newindex, "");
        lua_rawset(L, -3);

        lua_pushstring(L, "__metatable");
        lua_pushboolean(L, false);
        lua_rawset(L, -3);

        lua_pushstring(L, P3S_LUA_OBJ_METAFIELD_TYPE);
        lua_pushinteger(L, ITEM_TYPE_SYSTEM);
        lua_rawset(L, -3);
    }
    lua_setmetatable(L, -2);
    lua_rawset(L, -3);

    lua_utils_setSystemGlobalsInRegistry(L, -1);
    lua_pop(L, 1); // pop System Globals

    LUAUTILS_STACK_SIZE_ASSERT(L, 0)
}

void lua_sandbox_globals(lua_State * const L) {
    lua_newtable(L);

    lua_newtable(L);
    lua_utils_pushRootGlobalsFromRegistry(L);
    lua_setfield(L, -2, "__index");
    lua_setreadonly(L, -1, true);

    lua_setmetatable(L, -2);

    // we can set safeenv now although it's important to set it to false if code is loaded twice into the thread
    lua_replace(L, LUA_GLOBALSINDEX);
    // lua_setsafeenv(L, LUA_GLOBALSINDEX, true); // TODO: study this
}

void lua_sandbox_system_globals(lua_State * const L) {
    lua_newtable(L);

    lua_newtable(L);
    lua_utils_pushSystemGlobalsFromRegistry(L);
    lua_setfield(L, -2, "__index");
    lua_setreadonly(L, -1, true);

    lua_setmetatable(L, -2);

    // we can set safeenv now although it's important to set it to false if code is loaded twice into the thread
    lua_replace(L, LUA_GLOBALSINDEX);
    // lua_setsafeenv(L, LUA_GLOBALSINDEX, true); // TODO: study this
}

void lua_system_table_push(lua_State * const L) {
    LUAUTILS_STACK_SIZE(L)
    lua_utils_pushSystemGlobalsFromRegistry(L);
    lua_pushstring(L, P3S_LUA_G_SYSTEM);
    lua_rawget(L, -2);
    lua_remove(L, -2);
    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
}

static int _newindex_readonly(lua_State * const L) {
    vx_assert(L != nullptr);
    LUAUTILS_ASSERT_ARGS_COUNT(L, 3)
    LUAUTILS_STACK_SIZE(L)
    LUAUTILS_ERROR_F(L, "can't set System.%s (table is read-only)", lua_tostring(L, 2))
    LUAUTILS_STACK_SIZE_ASSERT(L, 0)
    return 0;
}

static int _system_index(lua_State *L) {
    // vxlog_debug("_system_index");
    vx_assert(L != nullptr);
    LUAUTILS_ASSERT_ARGS_COUNT(L, 2)
    LUAUTILS_ASSERT_ARG_TYPE(L, 1, ITEM_TYPE_SYSTEM)
    LUAUTILS_STACK_SIZE(L)

    const char *key = lua_tostring(L, 2);
    LUAUTILS_STACK_SIZE_ASSERT(L, 0)

    if (strcmp(key, "GetFromClipboard") == 0) {
        lua_pushcfunction(L, _getFromClipboard, "");
    } else if (strcmp(key, "SafeAreaTop") == 0) {
        lua_pushnumber(L, static_cast<double>(Screen::safeAreaInsets.top));
    } else if (strcmp(key, "Sensitivity") == 0) {
        lua_pushnumber(L, static_cast<double>(Prefs::shared().sensitivity()));
    } else if (strcmp(key, "ZoomSensitivity") == 0) {
        lua_pushnumber(L, static_cast<double>(Prefs::shared().zoomSensitivity()));
    } else if (strcmp(key, LUA_SYSTEM_FIELD_MASTER_VOLUME) == 0) {
        lua_pushnumber(L, static_cast<double>(Prefs::shared().masterVolume()));
    } else if (strcmp(key, LUA_SYSTEM_FIELD_WORLD_ID) == 0) {
        Game *g = nullptr;
        if (lua_utils_getGameCppWrapperPtr(L, &g) == false || g == nullptr) {
            LUAUTILS_ERROR_F(L, "System.%s - failed to retrieve Game from lua state", key)
            return 0;
        }
        lua_pushstring(L, g->getID().c_str());
    } else if (strcmp(key, LUA_SYSTEM_FIELD_SEND_CHAT_MESSAGE) == 0) {
        lua_pushcfunction(L, _system_send_chat_message, "");
    } else if (strcmp(key, LUA_SYSTEM_FIELD_CRASH) == 0) {
        lua_pushcfunction(L, _system_crash, "");
    } else if (strcmp(key, LUA_SYSTEM_WORLD_EDITOR_PLAY) == 0) {
        lua_pushcfunction(L, _system_world_editor_play, "");
    } else if (strcmp(key, LUA_SYSTEM_WORLD_EDITOR_STOP) == 0) {
        lua_pushcfunction(L, _system_world_editor_stop, "");
    } else if (strcmp(key, LUA_SYSTEM_FIELD_COMPUTEBADGESIGNATURE) == 0) {
        lua_pushcfunction(L, computeBadgeSignature, "");
    } else if (strcmp(key, "RenderQualityTiersAvailable") == 0) {
        bool b = true;
        if (GameRenderer::getGameRenderer()->isFallbackEnabled()) {
            b = false;
        }
        lua_pushboolean(L, b);
    } else if (strcmp(key, "RenderQualityTier") == 0) {
        const QualityTier qt = GameRenderer::getGameRenderer()->getQualityTier();
        lua_pushinteger(L, static_cast<lua_Integer>(qt) + 1);
    } else if (strcmp(key, "MaxRenderQualityTier") == 0) {
        lua_pushinteger(L, RENDER_QUALITY_TIER_MAX);
    } else if (strcmp(key, "MinRenderQualityTier") == 0) {
        lua_pushinteger(L, RENDER_QUALITY_TIER_MIN);
    } else if (strcmp(key, LUA_SYSTEM_FIELD_CLEARCACHE) == 0) {
        lua_pushcfunction(L, clearCache, "");
    } else if (strcmp(key, LUA_SYSTEM_FIELD_CLEARHTTPCACHEFORURL) == 0) {
        lua_pushcfunction(L, clearHttpCacheForUrl, "");
    } else if (strcmp(key, "HapticFeedbackEnabled") == 0) {
        lua_pushboolean(L, Prefs::shared().canVibrate());
    } else if (strcmp(key, "Fullscreen") == 0) {
        lua_pushboolean(L, Prefs::shared().fullscreen());
    } else if (strcmp(key, "LaunchItemEditor") == 0) {
        lua_pushcfunction(L, launchItemEditor, "");
    } else if (strcmp(key, "ServerIsInDevMode") == 0) {
        Game *g = nullptr;
        if (lua_utils_getGameCppWrapperPtr(L, &g) == false || g == nullptr) {
            LUAUTILS_ERROR_F(L, "System.%s - failed to retrieve Game from lua state", key)
            return 0;
        }
        lua_pushboolean(L, g->serverIsInDevMode());
    } else if (strcmp(key, "LocalUserIsAuthor") == 0) {
        Game *g = nullptr;
        if (lua_utils_getGameCppWrapperPtr(L, &g) == false || g == nullptr) {
            LUAUTILS_ERROR_F(L, "System.%s - failed to retrieve Game from lua state", key)
            return 0;
        }
        lua_pushboolean(L, g->localUserIsAuthor());
    } else if (strcmp(key, "ReadCode") == 0) {
        lua_pushcfunction(L, readCode, "");
    } else if (strcmp(key, "EditCode") == 0) {
        lua_pushcfunction(L, editCode, "");
    } else if (strcmp(key, "GoHome") == 0) {
        lua_pushcfunction(L, goHome, "");
    } else if (strcmp(key, LUA_SYSTEM_FIELD_HAS_CREDENTIALS) == 0) {
        vx::HubClient& client = HubClient::getInstance();
        HubClient::UserCredentials credentials = client.getCredentials();
        lua_pushboolean(L, credentials.token != "" && credentials.ID != "");
    } else if (strcmp(key, LUA_SYSTEM_FIELD_REMOVE_CREDENTIALS) == 0) {
        lua_pushcfunction(L, removeCredentials, "");
    } else if (strcmp(key, LUA_SYSTEM_FIELD_AUTHENTICATED) == 0) {
        lua_pushboolean(L, vx::Account::active().isAuthenticated());
    } else if (strcmp(key, LUA_SYSTEM_FIELD_USER_ID) == 0) {
        vx::HubClient& client = HubClient::getInstance();
        HubClient::UserCredentials credentials = client.getCredentials();
        lua_pushstring(L, credentials.ID.c_str());
    } else if (strcmp(key, LUA_SYSTEM_FIELD_HAS_EMAIL) == 0) {
        lua_pushboolean(L, vx::Account::active().hasEmail());
    } else if (strcmp(key, LUA_SYSTEM_FIELD_IS_PHONE_EXEMPTED) == 0) {
        lua_pushboolean(L, vx::Account::active().isPhoneNumberExempted());
    } else if (strcmp(key, LUA_SYSTEM_FIELD_HAS_DOB) == 0) {
        lua_pushboolean(L, vx::Account::active().hasDOB());
    } else if (strcmp(key, LUA_SYSTEM_FIELD_HAS_ESTIMATED_DOB) == 0) {
        lua_pushboolean(L, vx::Account::active().hasEstimatedDOB());
    } else if (strcmp(key, LUA_SYSTEM_FIELD_USERNAME) == 0) {
        lua_pushstring(L, vx::Account::active().username().c_str());
    } else if (strcmp(key, LUA_SYSTEM_FIELD_HAS_VERIFIED_PHONE_NUMBER) == 0) {
        lua_pushboolean(L, vx::Account::active().hasVerifiedPhoneNumber());
    } else if (strcmp(key, LUA_SYSTEM_FIELD_HAS_UNVERIFIED_PHONE_NUMBER) == 0) {
        lua_pushboolean(L, vx::Account::active().hasUnverifiedPhoneNumber());
    } else if (strcmp(key, LUA_SYSTEM_FIELD_BLOCKED_USERS) == 0) {
        lua_newtable(L);
        const std::vector<std::string>& blockedUsers = vx::Account::active().blockedUsers();
        int i = 1;
        for (std::string userID : blockedUsers) {
            lua_pushstring(L, userID.c_str());
            lua_rawseti(L, -2, i);
            ++i;
        }
    } else if (strcmp(key, LUA_SYSTEM_FIELD_IS_HOME_APP_RUNNING) == 0) {
        Game *g = nullptr;
        if (lua_utils_getGameCppWrapperPtr(L, &g) == false || g == nullptr) {
            LUAUTILS_ERROR_F(L, "System.%s - failed to retrieve Game from lua state", key)
            return 0;
        }
        lua_pushboolean(L, g->getID() == WORLD_HOME_ID);
    } else if (strcmp(key, LUA_SYSTEM_FIELD_IS_USER_UNDER_13) == 0) {
        lua_pushboolean(L, Account::active().under13());
    } else if (strcmp(key, LUA_SYSTEM_FIELD_IS_PARENT_APPROVED) == 0) {
        lua_pushboolean(L, Account::active().parentApproved());
    } else if (strcmp(key, LUA_SYSTEM_FIELD_IS_CHAT_ENABLED) == 0) {
        lua_pushboolean(L, Account::active().chatEnabled());
    } else if (strcmp(key, LUA_SYSTEM_FIELD_UNDER_13_DISCLAIMER_NEEDS_APPROVAL) == 0) {
        lua_pushboolean(L, Account::active().under13DisclaimerRequired());
    } else if (strcmp(key, LUA_SYSTEM_FIELD_APPROVE_UNDER_13_DISCLAIMER) == 0) {
        lua_pushcfunction(L, _approveUnder13Disclaimer, "");
    } else if (strcmp(key, LUA_SYSTEM_FIELD_ASKEDFORMAGICKEY) == 0) {
        lua_pushboolean(L, Account::connecting().askedForMagicKey());
    } else if (strcmp(key, LUA_SYSTEM_FIELD_SAVEDUSERNAMEOREMAIL) == 0) {
        lua_pushstring(L, Account::connecting().usernameOrEmail().c_str());
    } else if (strcmp(key, LUA_SYSTEM_FIELD_STORECREDENTIALS) == 0) {
        lua_pushcfunction(L, storeCredentials, "");
    } else if (strcmp(key, LUA_SYSTEM_FIELD_DATAFROMBUNDLE) == 0) {
        lua_pushcfunction(L, dataFromBundle, "");
    } else if (strcmp(key, LUA_SYSTEM_FIELD_POINTERFORCESHOWN) == 0) {
        Game *g = nullptr;
        if (lua_utils_getGameCppWrapperPtr(L, &g) == false || g == nullptr) {
            LUAUTILS_ERROR_F(L, "System.%s - failed to retrieve Game from lua state", key)
            return 0;
        }
        lua_pushboolean(L, g->pointerForceShown());
    } else if (strcmp(key, LUA_SYSTEM_FIELD_JOINWORLD) == 0) {
        lua_pushcfunction(L, joinWorld, "");
    } else if (strcmp(key, LUA_SYSTEM_FIELD_EDITWORLD) == 0) {
        lua_pushcfunction(L, editWorld, "");
    } else if (strcmp(key, LUA_SYSTEM_FIELD_EDITWORLDCODE) == 0) {
        lua_pushcfunction(L, editWorldCode, "");
    } else if (strcmp(key, LUA_SYSTEM_FIELD_GETMETATABLE) == 0) {
        lua_pushcfunction(L, getMetatable, "");
    } else if (strcmp(key, LUA_SYSTEM_FIELD_ISCPPMENUACTIVE) == 0) {
        State_SharedPtr state = GameCoordinator::getInstance()->getState();
        bool b = GameCoordinator::getInstance()->isPopupShown() || state->isNot(State::Value::gameRunning);
        lua_pushboolean(L, b);
    } else if (strcmp(key, LUA_SYSTEM_FIELD_OPENWEBMODAL) == 0) {
        lua_pushcfunction(L, openWebModal, "");
    } else if (strcmp(key, LUA_SYSTEM_FIELD_REMOTE_NOTIF_REQUEST_ACCESS) == 0) {
        lua_pushcfunction(L, _remoteNotifRequestAccess, "");
    } else if (strcmp(key, LUA_SYSTEM_FIELD_REMOTE_NOTIF_REFRESH_PUSH_TOKEN) == 0) {
        lua_pushcfunction(L, _remoteNotifRefreshPushToken, "");
    } else if (strcmp(key, LUA_SYSTEM_FIELD_REMOTE_NOTIF_POSTPONE_AUTHORIZATION) == 0) {
        lua_pushcfunction(L, _remoteNotifPostponeAuthorization, "");
    } else if (strcmp(key, LUA_SYSTEM_FIELD_NOTIFICATION_GET_STATUS) == 0) {
        lua_pushcfunction(L, _remoteNotifGetStatus, "");
    } else if (strcmp(key, LUA_SYSTEM_FIELD_NOTIFICATION_COUNT) == 0) {
        LUAUTILS_ERROR_F(L, "System.%s is write-only", key)
    } else if (strcmp(key, LUA_SYSTEM_FIELD_OPEN_APP_SETTINGS) == 0) {
        lua_pushcfunction(L, _openAppSettings, "");
    } else if (strcmp(key, LUA_SYSTEM_FIELD_HTTP_GET) == 0) {
        lua_pushcfunction(L, _system_http_get, "");
    } else if (strcmp(key, LUA_SYSTEM_FIELD_HTTP_POST) == 0) {
        lua_pushcfunction(L, _system_http_post, "");
    } else if (strcmp(key, LUA_SYSTEM_FIELD_HTTP_PATCH) == 0) {
        lua_pushcfunction(L, _system_http_patch, "");
    } else if (strcmp(key, LUA_SYSTEM_FIELD_HTTP_DELETE) == 0) {
        lua_pushcfunction(L, _system_http_delete, "");
    } else if (strcmp(key, LUA_SYSTEM_FIELD_OPEN_URL) == 0) {
        lua_pushcfunction(L, _system_open_url, "");
    } else if (strcmp(key, LUA_SYSTEM_FIELD_KVS_GET) == 0) {
        lua_pushcfunction(L, _system_kvs_get, "");
    } else if (strcmp(key, LUA_SYSTEM_FIELD_KVS_SET) == 0) {
        lua_pushcfunction(L, _system_kvs_set, "");
    } else if (strcmp(key, LUA_SYSTEM_FIELD_KVS_REMOVE) == 0) {
        lua_pushcfunction(L, _system_kvs_remove, "");
    } else if (strcmp(key, LUA_SYSTEM_FIELD_WORLD) == 0) {
        CGame *game = nullptr;
        if (lua_utils_getGamePtr(L, &game) == false || game == nullptr) {
            LUAUTILS_INTERNAL_ERROR(L)
        }
        lua_object_pushNewInstance(L, scene_get_system_root(game_get_scene(game)));
    } else if (strcmp(key, LUA_SYSTEM_FIELD_SETLAYERS) == 0) {
        lua_pushcfunction(L, _set_layers_elevated, "");
    } else if (strcmp(key, LUA_SYSTEM_FIELD_GETLAYERS) == 0) {
        lua_pushcfunction(L, _get_layers_elevated, "");
    } else if (strcmp(key, LUA_SYSTEM_FIELD_GETGROUPS) == 0) {
        lua_pushcfunction(L, _get_groups_elevated, "");
    } else if (strcmp(key, LUA_SYSTEM_FIELD_SETGROUPS) == 0) {
        lua_pushcfunction(L, _set_groups_elevated, "");
    } else if (strcmp(key, LUA_SYSTEM_FIELD_SETCOLLIDESWITH) == 0) {
        lua_pushcfunction(L, _set_collideswith_elevated, "");
    } else if (strcmp(key, LUA_SYSTEM_FIELD_MULTILINEINPUT) == 0) {
        lua_pushcfunction(L, _multilineinput, "");
    } else if (strcmp(key, LUA_SYSTEM_FIELD_LOGOUT) == 0) {
        lua_pushcfunction(L, _logout, "");
    } else if (strcmp(key, LUA_SYSTEM_FIELD_LOGOUT_AND_EXIT) == 0) {
        lua_pushcfunction(L, _logoutAndExit, "");
    } else if (strcmp(key, LUA_SYSTEM_FIELD_EXECCOMMAND) == 0) {
        lua_pushcfunction(L, _execCommand, "");
    } else if (strcmp(key, LUA_SYSTEM_FIELD_DEBUGEVENT) == 0) {
        lua_pushcfunction(L, _debugEvent, "");
    } else if (strcmp(key, LUA_SYSTEM_FIELD_HASENVIRONMENTTOLAUNCH) == 0) {
        lua_pushboolean(L, vx::GameCoordinator::getInstance()->hasEnvironmentToLaunch());
    } else if (strcmp(key, LUA_SYSTEM_FIELD_LAUNCHENVIRONMENT) == 0) {
        lua_pushcfunction(L, _launchEnvironment, "");
    } else if (strcmp(key, LUA_SYSTEM_FIELD_CONNECT_TO_SERVER) == 0) {
        lua_pushcfunction(L, _connectToServer, "");
    } else if (strcmp(key, LUA_SYSTEM_FIELD_LOG) == 0) {
        lua_pushcfunction(L, _log, "");
    } else if (strcmp(key, LUA_SYSTEM_FIELD_ADD_COMMAND_IN_HISTORY) == 0) {
        lua_pushcfunction(L, _add_command_in_history, "");
    } else if (strcmp(key, LUA_SYSTEM_FIELD_GET_COMMAND_FROM_HISTORY) == 0) {
        lua_pushcfunction(L, _get_command_from_history, "");
    } else if (strcmp(key, LUA_SYSTEM_FIELD_NB_COMMANDS_IN_HISTORY) == 0) {
        lua_pushinteger(L, static_cast<int>(GameCoordinator::getInstance()->getNbPreviousCommands()));
    } else if (strcmp(key, LUA_SYSTEM_FIELD_SCRIPT) == 0) {
        vx::Game *g = nullptr;
        if (lua_utils_getGameCppWrapperPtr(L, &g) == false) {
            LUAUTILS_INTERNAL_ERROR(L);
        }
        lua_pushstring(L, g->getData()->getScript().c_str());
    } else if (strcmp(key, LUA_SYSTEM_FIELD_PUBLISH_SCRIPT) == 0) {
        lua_pushcfunction(L, _system_publish_script, "");
    } else if (strcmp(key, LUA_SYSTEM_FIELD_ROLLBACK) == 0) {
        lua_pushcfunction(L, _system_rollback, "");
    } else if (strcmp(key, LUA_SYSTEM_FIELD_TERMINATE) == 0) {
        lua_pushcfunction(L, _terminate, "");
    } else if (strcmp(key, LUA_SYSTEM_FIELD_PASSKEYSUPPORTED) == 0) {
        bool available = vx::auth::PassKey::IsAvailable();
        lua_pushboolean(L, available);
    } else if (strcmp(key, LUA_SYSTEM_FIELD_PASSKEYCREATE) == 0) {
        lua_pushcfunction(L, _system_passkey_create, "");
    } else if (strcmp(key, LUA_SYSTEM_FIELD_PASSKEYLOGIN) == 0) {
        lua_pushcfunction(L, _system_passkey_login, "");
    } else if (strcmp(key, LUA_SYSTEM_IAP_IS_AVAILABLE) == 0) {
#if defined(__VX_DISTRIBUTION_APPSTORE) || defined(__VX_DISTRIBUTION_TESTFLIGHT)
        lua_pushboolean(L, vx::IAP::isAvailable());
#else
        lua_pushboolean(L, false);
#endif
    } else if (strcmp(key, LUA_SYSTEM_IAP_PURCHASE) == 0) {
        lua_pushcfunction(L, _system_iap_purchase, "");
    } else if (strcmp(key, LUA_SYSTEM_IAP_PRODUCTS) == 0) {
        lua_pushcfunction(L, _system_iap_get_products, "");
    } else {
        lua_pushnil(L); // if key is unknown, return nil
    }

    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
    return 1;
}

static int _system_newindex(lua_State * const L) {
    vx_assert(L != nullptr);
    LUAUTILS_ASSERT_ARGS_COUNT(L, 3)
    LUAUTILS_STACK_SIZE(L)

    const char *key = lua_tostring(L, 2);
    LUAUTILS_STACK_SIZE_ASSERT(L, 0)

    if (strcmp(key, "Sensitivity") == 0) {
        if (lua_isnumber(L, 3) == false) {
            LUAUTILS_ERROR_F(L, "System.%s should be a number", key)
        }
        Prefs::shared().setSensitivity(static_cast<float>(lua_tonumber(L, 3)));
    } else if (strcmp(key, "ZoomSensitivity") == 0) {
        if (lua_isnumber(L, 3) == false) {
            LUAUTILS_ERROR_F(L, "System.%s should be a number", key)
        }
        Prefs::shared().setZoomSensitivity(static_cast<float>(lua_tonumber(L, 3)));

    } else if (strcmp(key, LUA_SYSTEM_FIELD_MASTER_VOLUME) == 0) {
        if (lua_isnumber(L, 3) == false) {
            LUAUTILS_ERROR_F(L, "System.%s should be a number", key)
        }
        const float volume = static_cast<float>(lua_tonumber(L, 3));
        Prefs::shared().setMasterVolume(volume);
#if !defined(P3S_CLIENT_HEADLESS)
        const bool ok = audio::AudioEngine::shared()->setVolume(volume);
        if (ok == false) {
            LUAUTILS_ERROR(L, "System.MasterVolume: number should be between 0.0 and 1.0")
        }
#endif

    } else if (strcmp(key, "RenderQualityTier") == 0) {
        if (lua_utils_isIntegerStrict(L, 3) == false) {
            LUAUTILS_ERROR_F(L, "System.%s should be an integer", key)
        }
        uint8_t tier = static_cast<uint8_t>(lua_tointeger(L, 3));
        if (tier < RENDER_QUALITY_TIER_MIN or tier > RENDER_QUALITY_TIER_MAX) {
            LUAUTILS_ERROR_F(L, "System.%s - tier should be between 1 and 4 (included)", key)
        }

        Prefs::shared().setRenderQualityTier(tier);

        const QualityTier qt = static_cast<QualityTier>(tier - 1);
        if (GameRenderer::getGameRenderer()->reload(qt)) {
            // Values returned by Screen may have changed, notify Lua script
            vx::NotificationCenter::shared().notify(vx::NotificationName::didResize);
        }

    } else if (strcmp(key, "HapticFeedbackEnabled") == 0) {
        if (lua_isboolean(L, 3) == false) {
            LUAUTILS_ERROR_F(L, "System.%s should be an boolean", key)
        }
        Prefs::shared().setCanVibrate(lua_toboolean(L, 3));
    } else if (strcmp(key, "Fullscreen") == 0) {
        if (lua_isboolean(L, 3) == false) {
            LUAUTILS_ERROR_F(L, "System.%s should be an boolean", key)
        }
        Prefs::shared().setFullscreen(lua_toboolean(L, 3));

    } else if (strcmp(key, "PointerForceShown") == 0) {
        if (lua_isboolean(L, 3) == false) {
            LUAUTILS_ERROR_F(L, "System.%s should be an boolean", key)
        }
        Game *g = nullptr;
        if (lua_utils_getGameCppWrapperPtr(L, &g) == false || g == nullptr) {
            LUAUTILS_ERROR_F(L, "System.%s - failed to retrieve Game from lua state", key)
            return 0;
        }
        g->forceShowPointer(lua_toboolean(L, 3));
    } else if (strcmp(key, LUA_SYSTEM_FIELD_AUTHENTICATED) == 0) {
        if (lua_isboolean(L, 3) == false) {
            LUAUTILS_ERROR_F(L, "System.%s should be an boolean", key)
        }
        vx::Account::active().setAuthenticated(lua_toboolean(L, 3));
    } else if (strcmp(key, LUA_SYSTEM_FIELD_HAS_EMAIL) == 0) {
        if (lua_isboolean(L, 3) == false) {
            LUAUTILS_ERROR_F(L, "System.%s should be an boolean", key)
        }
        vx::Account::active().setHasEmail(lua_toboolean(L, 3));
    } else if (strcmp(key, LUA_SYSTEM_FIELD_IS_PHONE_EXEMPTED) == 0) {
        if (lua_isboolean(L, 3) == false) {
            LUAUTILS_ERROR_F(L, "System.%s should be an boolean", key)
        }
        vx::Account::active().setIsPhoneNumberExempted(lua_toboolean(L, 3));
    } else if (strcmp(key, LUA_SYSTEM_FIELD_HAS_DOB) == 0) {
        if (lua_isboolean(L, 3) == false) {
            LUAUTILS_ERROR_F(L, "System.%s should be an boolean", key)
        }
        vx::Account::active().setHasDOB(lua_toboolean(L, 3));
    } else if (strcmp(key, LUA_SYSTEM_FIELD_HAS_ESTIMATED_DOB) == 0) {
        if (lua_isboolean(L, 3) == false) {
            LUAUTILS_ERROR_F(L, "System.%s should be an boolean", key)
        }
        vx::Account::active().setHasEstimatedDOB(lua_toboolean(L, 3));
    } else if (strcmp(key, LUA_SYSTEM_FIELD_IS_USER_UNDER_13) == 0) {
        if (lua_isboolean(L, 3) == false) {
            LUAUTILS_ERROR_F(L, "System.%s should be an boolean", key)
        }
        vx::Account::active().setUnder13(lua_toboolean(L, 3));
    } else if (strcmp(key, LUA_SYSTEM_FIELD_IS_PARENT_APPROVED) == 0) {
        if (lua_isboolean(L, 3) == false) {
            LUAUTILS_ERROR_F(L, "System.%s should be an boolean", key)
        }
        vx::Account::active().setParentApproved(lua_toboolean(L, 3));
    } else if (strcmp(key, LUA_SYSTEM_FIELD_IS_CHAT_ENABLED) == 0) {
        if (lua_isboolean(L, 3) == false) {
            LUAUTILS_ERROR_F(L, "System.%s should be an boolean", key)
        }
        vx::Account::active().setChatEnabled(lua_toboolean(L, 3));
    } else if (strcmp(key, LUA_SYSTEM_FIELD_HAS_VERIFIED_PHONE_NUMBER) == 0) {
        if (lua_isboolean(L, 3) == false) {
            LUAUTILS_ERROR_F(L, "System.%s should be an boolean", key)
        }
        vx::Account::active().setHasVerifiedPhoneNumber(lua_toboolean(L, 3));
    } else if (strcmp(key, LUA_SYSTEM_FIELD_HAS_UNVERIFIED_PHONE_NUMBER) == 0) {
        if (lua_isboolean(L, 3) == false) {
            LUAUTILS_ERROR_F(L, "System.%s should be an boolean", key)
        }
        vx::Account::active().setHasUnverifiedPhoneNumber(lua_toboolean(L, 3));
    } else if (strcmp(key, LUA_SYSTEM_FIELD_BLOCKED_USERS) == 0) {
        if (lua_istable(L, 3) == false) {
            LUAUTILS_ERROR_F(L, "System.%s should be an table", key)
        }
        vx::Account::active().resetBlockedUsers();
        lua_pushvalue(L, 3);
        lua_pushnil(L);
        while (lua_next(L, -2) != 0) {
            // key at -2, value at -1
            if (lua_isstring(L, -1)) {
                std::string userID = lua_tostring(L,-1);
                vx::Account::active().addBlockedUser(userID);
            }
            lua_pop(L, 1); // pop value, keep key
        }
        lua_pop(L, 1); // pop table

    } else if (strcmp(key, LUA_SYSTEM_FIELD_USERNAME) == 0) {
        if (lua_isstring(L, 3) == false) {
            LUAUTILS_ERROR_F(L, "System.%s should be a string", key)
        }
        const std::string userName = std::string(lua_tostring(L, 3));

        vx::Account::active().setUsername(userName);

        // Update local player's username
        Game *game = nullptr;
        if (lua_utils_getGameCppWrapperPtr(L, &game) == false || game == nullptr) {
            LUAUTILS_INTERNAL_ERROR(L)
            // return 0;
        }
        game->addOrUpdateLocalPlayer(nullptr, &userName, nullptr);
    } else if (strcmp(key, LUA_SYSTEM_FIELD_ASKEDFORMAGICKEY) == 0) {
        if (lua_isboolean(L, 3) == false) {
            LUAUTILS_ERROR_F(L, "System.%s should be an boolean", key)
        }
        vx::Account::connecting().setAskedForMagicKey(lua_toboolean(L, 3));

    } else if (strcmp(key, LUA_SYSTEM_FIELD_SAVEDUSERNAMEOREMAIL) == 0) {
        if (lua_isstring(L, 3) == false) {
            LUAUTILS_ERROR_F(L, "System.%s should be a string", key)
        }
        const std::string value(lua_tostring(L, 3));
        vx::Account::connecting().setUsernameOrEmail(value);
        return 0;
    } else if (strcmp(key, LUA_SYSTEM_FIELD_NOTIFICATION_COUNT) == 0) {
        if (lua_utils_isIntegerStrict(L, 3) == false) {
            LUAUTILS_ERROR_F(L, "System.%s should be an integer", key)
        }
        vx::notification::setBadgeCount(lua_tointeger(L, 3));
    }

    LUAUTILS_STACK_SIZE_ASSERT(L, 0)
    return 0;
}

static int _getFromClipboard(lua_State *L) {
    RETURN_VALUE_IF_NULL(L, 0)
    LUAUTILS_STACK_SIZE(L)
    const int argCount = lua_gettop(L);
    if (argCount < 1 || lua_utils_getObjectType(L, 1) != ITEM_TYPE_SYSTEM) {
        LUAUTILS_ERROR(L, "System:GetFromClipboard - function should be called with `:`");
    }
    lua_pushstring(L, device::getClipboardText().c_str());
    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
    return 1;
}

static int _remoteNotifGetStatus(lua_State * const L) {
    RETURN_VALUE_IF_NULL(L, 0)
    LUAUTILS_STACK_SIZE(L)

    const int argsCount = lua_gettop(L);

    int callbackIndex = 0;
    std::string callbackKey = "";
    bool callbackStoreSuccess = false;

    if (argsCount < 1 || lua_utils_getObjectType(L, 1) != ITEM_TYPE_SYSTEM) {
        LUAUTILS_ERROR(L, "System:NotificationGetStatus(callback) - function should be called with `:`");
    }
    if (argsCount < 2) {
        LUAUTILS_ERROR(L, "System:NotificationGetStatus(callback) - callback should not be nil");
    }
    if (lua_isfunction(L, 2) == false) {
        LUAUTILS_ERROR(L, "System:NotificationGetStatus(callback) - should be a function");
    }

    SAFE_LOCAL_STORE_SET(L, 2, callbackIndex, callbackKey, callbackStoreSuccess);
    if (callbackStoreSuccess == false) {
        // couldn't store function, not supposed to happen
        LUAUTILS_INTERNAL_ERROR(L);
    }

    vx::Game *cppGame = nullptr;
    if (lua_utils_getGameCppWrapperPtr(L, &cppGame) == false) {
        LUAUTILS_INTERNAL_ERROR(L)
    }
    vx::Game_WeakPtr weakPtr = cppGame->getWeakPtr();

    vx::notification::remotePushAuthorizationStatus([weakPtr, callbackIndex, callbackKey](vx::notification::NotificationAuthorizationStatus status){
        vx::Game_SharedPtr cppGame = weakPtr.lock();
        if (cppGame == nullptr) {
            return;
        }

        lua_State *L = cppGame->getLuaState();
        vx_assert(L != nullptr);

        bool success = false;
        SAFE_LOCAL_STORE_GET(L, callbackIndex, callbackKey, success);

        if (success == false) {
            return;
        }

        SAFE_LOCAL_STORE_REMOVE(L, callbackIndex, callbackKey, success)
        if (success == false) {
            // couldn't removed stored callback
            // not supposed to happen, but continue anyway since function has been retrieved
            vxlog_error("System:NotificationGetStatus(callback) - couldn't remove stored callback");
        }

        if (lua_isfunction(L, -1) == false) {
            return;
        }

        switch (status) {
            case vx::notification::NotificationAuthorizationStatus_NotDetermined:
                lua_pushstring(L, "underdetermined");
                break;
            case vx::notification::NotificationAuthorizationStatus_Denied:
                lua_pushstring(L, "denied");
                break;
            case vx::notification::NotificationAuthorizationStatus_Authorized:
                lua_pushstring(L, "authorized");
                break;
            case vx::notification::NotificationAuthorizationStatus_Postponed:
                lua_pushstring(L, "postponed");
                break;
            case vx::notification::NotificationAuthorizationStatus_NotSupported:
                lua_pushstring(L, "notsupported");
                break;
                //            default:
                //                // this should not happen
                //                lua_pushstring(L, "notsupported");
                //                break;
        }

        if (lua_pcall(L, 1, 0, 0) != LUA_OK) {
            lua_pop(L, 1);
        }
    });

    LUAUTILS_STACK_SIZE_ASSERT(L, 0)
    return 0;
}

static int _remoteNotifRefreshPushToken(lua_State * const L) {
    if (vx::notification::needsToPushToken()) {
        vx::notification::requestRemotePushToken();
        vx::notification::setNeedsToPushToken(false);
    }
    return 0;
}

static int _remoteNotifRequestAccess(lua_State * const L) {
    RETURN_VALUE_IF_NULL(L, 0)
    LUAUTILS_STACK_SIZE(L)

    const int argsCount = lua_gettop(L);

    int callbackIndex = 0;
    std::string callbackKey = "";
    bool callbackStoreSuccess = false;

    if (argsCount < 1 || lua_utils_getObjectType(L, 1) != ITEM_TYPE_SYSTEM) {
        LUAUTILS_ERROR(L, "System:NotificationRequestAuthorization(callback) - function should be called with `:`");
    }
    if (argsCount > 2) {
        LUAUTILS_ERROR(L, "System:NotificationRequestAuthorization(callback) - too many arguments");
    }
    if (argsCount == 2 && lua_isfunction(L, 2) == false) {
        LUAUTILS_ERROR(L, "System:NotificationRequestAuthorization(callback) - should be a function");
    }

    if (argsCount == 2) {
        SAFE_LOCAL_STORE_SET(L, 2, callbackIndex, callbackKey, callbackStoreSuccess);
        if (callbackStoreSuccess == false) {
            // couldn't store function, not supposed to happen
            LUAUTILS_INTERNAL_ERROR(L);
        }
    }

    vx::Game *cppGame = nullptr;
    if (lua_utils_getGameCppWrapperPtr(L, &cppGame) == false) {
        LUAUTILS_INTERNAL_ERROR(L)
    }
    vx::Game_WeakPtr weakPtr = cppGame->getWeakPtr();

    vx::notification::requestRemotePushAuthorization([weakPtr, callbackIndex, callbackKey](vx::notification::NotificationAuthorizationResponse r){
        vx::Game_SharedPtr cppGame = weakPtr.lock();
        if (cppGame == nullptr) {
            return;
        }

        lua_State *L = cppGame->getLuaState();
        vx_assert(L != nullptr);

        bool success = false;
        SAFE_LOCAL_STORE_GET(L, callbackIndex, callbackKey, success);

        if (success == false) {
            return;
        }

        SAFE_LOCAL_STORE_REMOVE(L, callbackIndex, callbackKey, success)
        if (success == false) {
            // couldn't removed stored callback
            // not supposed to happen, but continue anyway since function has been retrieved
            vxlog_error("System:NotificationRequestAuthorization(callback) - couldn't remove stored callback");
        }

        if (lua_isfunction(L, -1) == false) {
            return;
        }

        switch (r) {
            case vx::notification::NotificationAuthorizationResponse_Error:
                lua_pushstring(L, "error");
                break;
            case vx::notification::NotificationAuthorizationResponse_Authorized:
                vx::notification::setNeedsToPushToken(false); // already pushed by requestRemotePushAuthorization
                lua_pushstring(L, "authorized");
                break;
            case vx::notification::NotificationAuthorizationResponse_Denied:
                lua_pushstring(L, "denied");
                break;
            case vx::notification::NotificationAuthorizationResponse_NotSupported:
                lua_pushstring(L, "not_supported");
                break;
            case vx::notification::NotificationAuthorizationResponse_Postponed:
                lua_pushstring(L, "postponed");
                break;
        }

        if (lua_pcall(L, 1, 0, 0) != LUA_OK) {
            lua_pop(L, 1);
        }
    });
    LUAUTILS_STACK_SIZE_ASSERT(L, 0)
    return 0;
}

static int _remoteNotifPostponeAuthorization(lua_State * const L) {
    RETURN_VALUE_IF_NULL(L, 0)
    LUAUTILS_STACK_SIZE(L)

    const int argsCount = lua_gettop(L);
    if (argsCount < 1 || lua_utils_getObjectType(L, 1) != ITEM_TYPE_SYSTEM) {
        LUAUTILS_ERROR(L, "System:NotificationPostponeAuthorization() - function should be called with `:`");
    }

    vx::notification::postponeRemotePushAuthorization();

    LUAUTILS_STACK_SIZE_ASSERT(L, 0)
    return 0;
}

static int _openAppSettings(lua_State * const L) {
    RETURN_VALUE_IF_NULL(L, 0)
    LUAUTILS_STACK_SIZE(L)
    vx::device::openApplicationSettings();
    LUAUTILS_STACK_SIZE_ASSERT(L, 0)
    return 0;
}

// Chat

static int _system_send_chat_message(lua_State * const L) {
    LUAUTILS_STACK_SIZE(L)
    const int argsCount = lua_gettop(L);
    if (argsCount < 1 || lua_utils_getObjectType(L, 1) != ITEM_TYPE_SYSTEM) {
        LUAUTILS_ERROR(L, "System:SendChatMessage(msg, recipients) - function should be called with `:`");
    }
    if (argsCount < 3 || argsCount > 4) {
        LUAUTILS_ERROR(L, "System:SendChatMessage(msg, recipients) - incorrect argument count");
    }
    if (lua_isstring(L, 2) == false) {
        LUAUTILS_ERROR(L, "System:SendChatMessage(msg, recipients) - msg should be a string");
    }
    if (lua_istable(L, 3) == false) {
        LUAUTILS_ERROR(L, "System:SendChatMessage(msg, recipients) - recipients should be Players, OtherPlayers, Server or a table");
    }

    std::string message = std::string(lua_tostring(L, 2));
    if (message.size() > 65535) {
        LUAUTILS_ERROR(L, "System:SendChatMessage(msg, recipients) - msg is too long")
    }

    vx::Game *cppGame = nullptr;
    if (lua_utils_getGameCppWrapperPtr(L, &cppGame) == false) {
        LUAUTILS_ERROR(L, "HTTP:Get - failed to retrieve Game from lua state");
    }

    int localPlayerID = cppGame->getLocalPlayerID();
    // localPlayerID here could be PLAYER_ID_NOT_ATTRIBUTED

    // chat messages are attributed by userID
    // but client ID could be used to select the right player
    // when connected several times with same account

    std::string userID = vx::Account::active().userID();
    std::string username = vx::Account::active().username();

    std::vector<uint8_t> recipientIds = std::vector<uint8_t>();

    const int type = lua_utils_getObjectType(L, 3);
    if (type == ITEM_TYPE_SERVER) {
        recipientIds.push_back(PLAYER_ID_SERVER);

    } else if (type == ITEM_TYPE_PLAYER) {
        uint8_t playerId = 0;
        if (lua_player_getIdFromLuaPlayer(L, 3, &playerId)) {
            recipientIds.push_back(playerId);
        }

    } else if (type == ITEM_TYPE_G_PLAYERS) {
        recipientIds.push_back(PLAYER_ID_ALL);

    } else if (type == ITEM_TYPE_G_OTHERPLAYERS) {
        recipientIds.push_back(PLAYER_ID_ALL_BUT_SELF);

    } else {
        // Argument table is not a Server nor a Player, let's loop over its
        // items and look for Server or Player objects.
        lua_pushnil(L); // first key
        while (lua_next(L, 3) != 0) {
            // +2
            // 'key' is at index -2 and 'value' is at index -1
            const int itemType = lua_utils_getObjectType(L, -1);

            if (itemType == ITEM_TYPE_SERVER) {
                recipientIds.push_back(PLAYER_ID_SERVER);

            } else if (itemType == ITEM_TYPE_PLAYER) {
                uint8_t playerId = 0;
                if (lua_player_getIdFromLuaPlayer(L, -1, &playerId)) {
                    recipientIds.push_back(playerId);
                }
            }
            // pops value. Keeps 'key' for next iteration
            lua_pop(L, 1);
        }
    }

    std::string localUUID = vx::crypto::generate_uuid_v4();
    std::string emptyUUID = vx::crypto::empty_uuid_v4();

    vx::GameMessage_SharedPtr msg = vx::GameMessage::makeChatMessage(localPlayerID, userID, username, localUUID, vx::crypto::empty_uuid_v4(), recipientIds, message);

    if (msg == nullptr) {
        LUAUTILS_INTERNAL_ERROR(L) // return
    } else {
        cppGame->sendMessage(msg);
    }

    LOCAL_EVENT_SEND_PUSH_WITH_SYSTEM(L, LOCAL_EVENT_NAME_ChatMessage)
    lua_pushstring(L, message.c_str());
    lua_pushinteger(L, localPlayerID); // sender connection ID when message is sent
    lua_pushstring(L, userID.c_str());
    lua_pushstring(L, username.c_str());
    lua_pushstring(L, "pending");
    lua_pushstring(L, emptyUUID.c_str());
    lua_pushstring(L, localUUID.c_str());
    LOCAL_EVENT_SEND_CALL_WITH_SYSTEM(L, 7)

    lua_pushboolean(L, true);
    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
    return 1;
}

static int _system_crash(lua_State * const L) {
    int a = 1, b = 2;
    assert(a==b);
    return 0;
}

static int _system_world_editor_play(lua_State * const L) {
    GameCoordinator::getInstance()->worldEditorPlay();
    return 0;
}

static int _system_world_editor_stop(lua_State * const L) {
    GameCoordinator::getInstance()->worldEditorStop();
    return 0;
}

// MARK: - HTTP functions' implementation -

static int _system_http_get(lua_State * const L) {
    LUAUTILS_STACK_SIZE(L)

    const int argsCount = lua_gettop(L);
    const bool hasHeaders = argsCount == 4;
    const int callbackIndex = hasHeaders ? 4 : 3;

    if (argsCount < 1 || lua_utils_getObjectType(L, 1) != ITEM_TYPE_SYSTEM) {
        LUAUTILS_ERROR(L, "HTTP:Get - function should be called with `:`");
    }
    if (argsCount < 3 || argsCount > 4) {
        LUAUTILS_ERROR(L, "HTTP:Get - incorrect argument count");
    }

    if (lua_isstring(L, 2) == false) {
        LUAUTILS_ERROR(L, "HTTP:Get - argument 1 should be a string");
    }

    // Parse headers provided from Lua
    std::unordered_map<std::string, std::string> headers;
    if (hasHeaders) {
        lua_http_parse_headers(headers, L, 3);
    }

    if (lua_isfunction(L, callbackIndex) == false) {
        LUAUTILS_ERROR_F(L,
                         "HTTP:Get - %s argument should be a function",
                         hasHeaders ? "third" : "second");
    }

    const char *urlCStr = lua_tostring(L, 2);
    const vx::URL url = vx::URL::make(std::string(urlCStr));

    if (url.isValid() == false) {
        LUAUTILS_ERROR(L, "HTTP:Get - URL is not valid");
    }

    // If URL targets Cubzh' API server, then add the user's credentials to the HTTP headers
    {
        if (url.host() == CUBZH_API_SERVER_URL || url.host() == CUBZH_API_SERVER_DEV_ADDR) {
#ifdef CLIENT_ENV
            const vx::HubClient::UserCredentials creds = vx::HubClient::getInstance().getCredentials();
            headers.emplace(HTTP_HEADER_CUBZH_CLIENT_VERSION_V2, vx::device::appVersionCached());
            headers.emplace(HTTP_HEADER_CUBZH_CLIENT_BUILDNUMBER_V2, vx::device::appBuildNumberCached());
            headers.emplace(HTTP_HEADER_CUBZH_USER_ID, creds.ID);
            headers.emplace(HTTP_HEADER_CUBZH_USER_TOKEN, creds.token);
#elif defined(ONLINE_GAMESERVER)
            headers.emplace(HTTP_HEADER_CUBZH_GAME_SERVER_TOKEN, HTTP_HEADER_CUBZH_GAME_SERVER_TOKEN_VALUE);
#endif
        }
    }

    vx::Game *cppGame = nullptr;
    if (lua_utils_getGameCppWrapperPtr(L, &cppGame) == false) {
        LUAUTILS_ERROR(L, "HTTP:Get - failed to retrieve Game from lua state");
    }
    vx::Game_WeakPtr weakPtr = cppGame->getWeakPtr();

    vx::HttpRequestOpts opts;
    opts.setSendNow(false);

    vx::HttpRequest_SharedPtr req = vx::HttpClient::shared().GET(url,
                                                                 headers,
                                                                 opts,
                                                                 [weakPtr](vx::HttpRequest_SharedPtr req) {
        if (req->getStatus() == vx::HttpRequest::Status::CANCELLED) { return; }

        vx::Game_SharedPtr cppGame = weakPtr.lock();
        if (cppGame == nullptr) {
            return;
        }

        vx::OperationQueue *mainQueue = cppGame->isInClientMode()
        ? vx::OperationQueue::getMain()
        : vx::OperationQueue::getServerMain();

        mainQueue->dispatch([weakPtr, req]() {
            if (req->getStatus() == vx::HttpRequest::Status::CANCELLED) {
                return;
            }

            // Get L from weak game ptr
            vx::Game_SharedPtr cppGame = weakPtr.lock();
            if (cppGame == nullptr)
                return;

            lua_State *L = cppGame->getLuaState();
            vx_assert(L != nullptr);

            lua_http_httprequest_callback(L, req, req->getResponse());
        });
    });

    lua_http_request_create_and_push(L, callbackIndex, req);

    req->sendAsync();

    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
    return 1;
}

static int _system_http_post(lua_State * const L) {
    LUAUTILS_STACK_SIZE(L)

    const int argsCount = lua_gettop(L);
    const bool hasHeaders = argsCount == 5;
    const int bodyIndex = hasHeaders ? 4 : 3;
    const int callbackIndex = hasHeaders ? 5 : 4;

    if (argsCount < 1 || lua_utils_getObjectType(L, 1) != ITEM_TYPE_SYSTEM) {
        LUAUTILS_ERROR(L, "HTTP:Post - function should be called with `:`");
    }

    if (argsCount < 4 || argsCount > 5) {
        LUAUTILS_ERROR(L, "HTTP:Post - incorrect argument count");
    }

    if (lua_isstring(L, 2) == false) {
        LUAUTILS_ERROR(L, "HTTP:Post - argument 1 should be a string");
    }

    std::unordered_map<std::string, std::string> headers;
    if (hasHeaders) {
        lua_http_parse_headers(headers, L, 3);
    }

    std::string body;
    if (lua_utils_getObjectType(L, bodyIndex) == ITEM_TYPE_DATA) {
        size_t len = 0;
        const void *bytes = lua_data_getBuffer(L, bodyIndex, &len);
        body = std::string(static_cast<const char *>(bytes), len);

    } else if (lua_istable(L, bodyIndex)) {
        std::ostringstream streamEncoded;
        lua_table_to_json_string(streamEncoded, L, bodyIndex);
        body = streamEncoded.str();

    } else if (lua_isstring(L, bodyIndex)) {
        body = lua_tostring(L, bodyIndex);

    } else {
        LUAUTILS_ERROR(L, "HTTP:Post - body parameter should be a table, string or data");
    }

    if (lua_isfunction(L, callbackIndex) == false) {
        LUAUTILS_ERROR_F(L,
                         "HTTP:Post - %s argument should be a function",
                         hasHeaders ? "fourth" : "third");
    }

    const char *urlCStr = lua_tostring(L, 2);
    const std::string urlStr(urlCStr);
    const vx::URL url = vx::URL::make(urlStr);

    if (url.isValid() == false) {
        LUAUTILS_ERROR(L, "HTTP:Post - URL is not valid");
    }

    // If URL targets Cubzh' API server, then add the user's credentials to the HTTP headers
    {
        if (url.host() == CUBZH_API_SERVER_URL || url.host() == CUBZH_API_SERVER_DEV_ADDR) {
#ifdef CLIENT_ENV
            const vx::HubClient::UserCredentials creds = vx::HubClient::getInstance().getCredentials();
            headers.emplace(HTTP_HEADER_CUBZH_CLIENT_VERSION_V2, vx::device::appVersionCached());
            headers.emplace(HTTP_HEADER_CUBZH_CLIENT_BUILDNUMBER_V2, vx::device::appBuildNumberCached());
            headers.emplace(HTTP_HEADER_CUBZH_USER_ID, creds.ID);
            headers.emplace(HTTP_HEADER_CUBZH_USER_TOKEN, creds.token);
#elif defined(ONLINE_GAMESERVER)
            headers.emplace(HTTP_HEADER_CUBZH_GAME_SERVER_TOKEN, HTTP_HEADER_CUBZH_GAME_SERVER_TOKEN_VALUE);
#endif
        }
    }

    vx::Game *cppGame = nullptr;
    if (lua_utils_getGameCppWrapperPtr(L, &cppGame) == false) {
        LUAUTILS_ERROR(L, "HTTP:Post - failed to retrieve Game from lua state");
    }
    vx::Game_WeakPtr weakPtr = cppGame->getWeakPtr();

    vx::HttpRequest_SharedPtr req = vx::HttpClient::shared().POST(
                                                                  urlStr,
                                                                  headers,
                                                                  nullptr,
                                                                  body,
                                                                  false /*don't send*/,
                                                                  [weakPtr](vx::HttpRequest_SharedPtr req) {
                                                                      if (req->getStatus() == vx::HttpRequest::Status::CANCELLED) { return; }

                                                                      vx::Game_SharedPtr cppGame = weakPtr.lock();
                                                                      if (cppGame == nullptr)
                                                                          return;

                                                                      vx::OperationQueue *mainQueue = cppGame->isInClientMode()
                                                                      ? vx::OperationQueue::getMain()
                                                                      : vx::OperationQueue::getServerMain();

                                                                      mainQueue->dispatch([weakPtr, req]() {
                                                                          if (req->getStatus() == vx::HttpRequest::Status::CANCELLED) { return; }

                                                                          // Get L from weak game ptr
                                                                          vx::Game_SharedPtr cppGame = weakPtr.lock();
                                                                          if (cppGame == nullptr)
                                                                              return;

                                                                          lua_State *L = cppGame->getLuaState();
                                                                          vx_assert(L != nullptr);

                                                                          vxlog_debug("POST: %s %s %d",
                                                                                      req->getHost().c_str(),
                                                                                      req->getPath().c_str(),
                                                                                      req->getResponse().getStatusCode());
                                                                          lua_http_httprequest_callback(L, req, req->getResponse());
                                                                      });
                                                                  });
    lua_http_request_create_and_push(L, callbackIndex, req);
    req->sendAsync();

    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
    return 1;
}

static int _system_http_patch(lua_State * const L) {
    LUAUTILS_STACK_SIZE(L)

    const int argsCount = lua_gettop(L);
    const bool hasHeaders = argsCount == 5;
    const int bodyIndex = hasHeaders ? 4 : 3;
    const int callbackIndex = hasHeaders ? 5 : 4;

    if (argsCount < 1 || lua_utils_getObjectType(L, 1) != ITEM_TYPE_SYSTEM) {
        LUAUTILS_ERROR(L, "HTTP:Patch - function should be called with `:`");
    }

    if (argsCount < 4 || argsCount > 5) {
        LUAUTILS_ERROR(L, "HTTP:Patch - incorrect argument count");
    }

    if (lua_isstring(L, 2) == false) {
        LUAUTILS_ERROR(L, "HTTP:Patch - argument 1 should be a string");
    }

    std::unordered_map<std::string, std::string> headers;
    if (hasHeaders) {
        lua_http_parse_headers(headers, L, 3);
    }

    std::string body;
    if (lua_utils_getObjectType(L, bodyIndex) == ITEM_TYPE_DATA) {
        size_t len = 0;
        const void *bytes = lua_data_getBuffer(L, bodyIndex, &len);
        body = std::string(static_cast<const char *>(bytes), len);

    } else if (lua_istable(L, bodyIndex)) {
        std::ostringstream streamEncoded;
        lua_table_to_json_string(streamEncoded, L, bodyIndex);
        body = streamEncoded.str();

    } else if (lua_isstring(L, bodyIndex)) {
        body = lua_tostring(L, bodyIndex);

    } else {
        LUAUTILS_ERROR(L, "HTTP:Patch - body parameter should be a table, string or data");
    }

    if (lua_isfunction(L, callbackIndex) == false) {
        LUAUTILS_ERROR_F(L,
                         "HTTP:Patch - %s argument should be a function",
                         hasHeaders ? "fourth" : "third");
    }

    const char *urlCStr = lua_tostring(L, 2);
    const std::string urlStr(urlCStr);
    const vx::URL url = vx::URL::make(urlStr);

    if (url.isValid() == false) {
        LUAUTILS_ERROR(L, "HTTP:Patch - URL is not valid");
    }

    // If URL targets Cubzh' API server, then add the user's credentials to the HTTP headers
    {
        if (url.host() == CUBZH_API_SERVER_URL || url.host() == CUBZH_API_SERVER_DEV_ADDR) {
#ifdef CLIENT_ENV
            const vx::HubClient::UserCredentials creds = vx::HubClient::getInstance().getCredentials();
            headers.emplace(HTTP_HEADER_CUBZH_CLIENT_VERSION_V2, vx::device::appVersionCached());
            headers.emplace(HTTP_HEADER_CUBZH_CLIENT_BUILDNUMBER_V2, vx::device::appBuildNumberCached());
            headers.emplace(HTTP_HEADER_CUBZH_USER_ID, creds.ID);
            headers.emplace(HTTP_HEADER_CUBZH_USER_TOKEN, creds.token);
#elif defined(ONLINE_GAMESERVER)
            headers.emplace(HTTP_HEADER_CUBZH_GAME_SERVER_TOKEN, HTTP_HEADER_CUBZH_GAME_SERVER_TOKEN_VALUE);
#endif
        }
    }

    vx::Game *cppGame = nullptr;
    if (lua_utils_getGameCppWrapperPtr(L, &cppGame) == false) {
        LUAUTILS_ERROR(L, "HTTP:Patch - failed to retrieve Game from lua state");
    }
    vx::Game_WeakPtr weakPtr = cppGame->getWeakPtr();

    vx::HttpRequest_SharedPtr req = vx::HttpClient::shared().PATCH(
                                                                   urlStr,
                                                                   headers,
                                                                   nullptr,
                                                                   body,
                                                                   false /*don't send*/,
                                                                   [weakPtr](vx::HttpRequest_SharedPtr req) {
                                                                       if (req->getStatus() == vx::HttpRequest::Status::CANCELLED) { return; }

                                                                       vx::Game_SharedPtr cppGame = weakPtr.lock();
                                                                       if (cppGame == nullptr)
                                                                           return;

                                                                       vx::OperationQueue *mainQueue = cppGame->isInClientMode()
                                                                       ? vx::OperationQueue::getMain()
                                                                       : vx::OperationQueue::getServerMain();

                                                                       mainQueue->dispatch([weakPtr, req]() {
                                                                           if (req->getStatus() == vx::HttpRequest::Status::CANCELLED) { return; }

                                                                           // Get L from weak game ptr
                                                                           vx::Game_SharedPtr cppGame = weakPtr.lock();
                                                                           if (cppGame == nullptr)
                                                                               return;

                                                                           lua_State *L = cppGame->getLuaState();
                                                                           vx_assert(L != nullptr);

                                                                           lua_http_httprequest_callback(L, req, req->getResponse());
                                                                       });
                                                                   });
    lua_http_request_create_and_push(L, callbackIndex, req);
    req->sendAsync();

    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
    return 1;
}

static int _system_http_delete(lua_State * const L) {
    LUAUTILS_STACK_SIZE(L)

    const int argsCount = lua_gettop(L);
    const bool hasHeaders = argsCount == 5;
    const int bodyIndex = hasHeaders ? 4 : 3;
    const int callbackIndex = hasHeaders ? 5 : 4;

    if (argsCount < 1 || lua_utils_getObjectType(L, 1) != ITEM_TYPE_SYSTEM) {
        LUAUTILS_ERROR(L, "HTTP:Delete - function should be called with `:`");
    }

    if (argsCount < 4 || argsCount > 5) {
        LUAUTILS_ERROR(L, "HTTP:Delete - incorrect argument count");
    }

    if (lua_isstring(L, 2) == false) {
        LUAUTILS_ERROR(L, "HTTP:Delete - argument 1 should be a string");
    }

    std::unordered_map<std::string, std::string> headers;
    if (hasHeaders) {
        lua_http_parse_headers(headers, L, 3);
    }

    std::string body;
    if (lua_utils_getObjectType(L, bodyIndex) == ITEM_TYPE_DATA) {
        size_t len = 0;
        const void *bytes = lua_data_getBuffer(L, bodyIndex, &len);
        body = std::string(static_cast<const char *>(bytes), len);

    } else if (lua_istable(L, bodyIndex)) {
        std::ostringstream streamEncoded;
        lua_table_to_json_string(streamEncoded, L, bodyIndex);
        body = streamEncoded.str();

    } else if (lua_isstring(L, bodyIndex)) {
        body = lua_tostring(L, bodyIndex);

    } else {
        LUAUTILS_ERROR(L, "HTTP:Delete - body parameter should be a table, string or data");
    }

    if (lua_isfunction(L, callbackIndex) == false) {
        LUAUTILS_ERROR_F(L,
                         "HTTP:Delete - %s argument should be a function",
                         hasHeaders ? "fourth" : "third");
    }

    const char *urlCStr = lua_tostring(L, 2);
    const std::string urlStr(urlCStr);
    const vx::URL url = vx::URL::make(urlStr);

    if (url.isValid() == false) {
        LUAUTILS_ERROR(L, "HTTP:Delete - URL is not valid");
    }

    // If URL targets Cubzh' API server, then add the user's credentials to the HTTP headers
    {
        if (url.host() == CUBZH_API_SERVER_URL || url.host() == CUBZH_API_SERVER_DEV_ADDR) {
#ifdef CLIENT_ENV
            const vx::HubClient::UserCredentials creds = vx::HubClient::getInstance().getCredentials();
            headers.emplace(HTTP_HEADER_CUBZH_CLIENT_VERSION_V2, vx::device::appVersionCached());
            headers.emplace(HTTP_HEADER_CUBZH_CLIENT_BUILDNUMBER_V2, vx::device::appBuildNumberCached());
            headers.emplace(HTTP_HEADER_CUBZH_USER_ID, creds.ID);
            headers.emplace(HTTP_HEADER_CUBZH_USER_TOKEN, creds.token);
#elif defined(ONLINE_GAMESERVER)
            headers.emplace(HTTP_HEADER_CUBZH_GAME_SERVER_TOKEN, HTTP_HEADER_CUBZH_GAME_SERVER_TOKEN_VALUE);
#endif
        }
    }

    vx::Game *cppGame = nullptr;
    if (lua_utils_getGameCppWrapperPtr(L, &cppGame) == false) {
        LUAUTILS_ERROR(L, "HTTP:Delete - failed to retrieve Game from lua state");
    }
    vx::Game_WeakPtr weakPtr = cppGame->getWeakPtr();

    vx::HttpRequest_SharedPtr req = vx::HttpClient::shared().Delete(
                                                                    urlStr,
                                                                    headers,
                                                                    nullptr,
                                                                    body,
                                                                    false /*don't send*/,
                                                                    [weakPtr](vx::HttpRequest_SharedPtr req) {
                                                                        if (req->getStatus() == vx::HttpRequest::Status::CANCELLED) { return; }

                                                                        vx::Game_SharedPtr cppGame = weakPtr.lock();
                                                                        if (cppGame == nullptr)
                                                                            return;

                                                                        vx::OperationQueue *mainQueue = cppGame->isInClientMode()
                                                                        ? vx::OperationQueue::getMain()
                                                                        : vx::OperationQueue::getServerMain();

                                                                        mainQueue->dispatch([weakPtr, req]() {
                                                                            if (req->getStatus() == vx::HttpRequest::Status::CANCELLED) { return; }

                                                                            // Get L from weak game ptr
                                                                            vx::Game_SharedPtr cppGame = weakPtr.lock();
                                                                            if (cppGame == nullptr)
                                                                                return;

                                                                            lua_State *L = cppGame->getLuaState();
                                                                            vx_assert(L != nullptr);

                                                                            lua_http_httprequest_callback(L, req, req->getResponse());
                                                                        });
                                                                    });
    lua_http_request_create_and_push(L, callbackIndex, req);
    req->sendAsync();

    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
    return 1;
}

static int _system_open_url(lua_State * const L) {
    LUAUTILS_STACK_SIZE(L)

    const int argsCount = lua_gettop(L);

    if (argsCount < 1 || lua_utils_getObjectType(L, 1) != ITEM_TYPE_SYSTEM) {
        LUAUTILS_ERROR(L, "System:OpenURL - function should be called with `:`");
    }

    if (argsCount != 2) {
        LUAUTILS_ERROR(L, "System:OpenURL - incorrect argument count");
    }

    if (lua_isstring(L, 2) == false) {
        LUAUTILS_ERROR(L, "System:OpenURL - argument 1 should be a string");
    }

    const char *url = lua_tostring(L, 2);
    if (url == nullptr || strlen(url) == 0) {
        LUAUTILS_ERROR(L, "System:OpenURL - url is empty");
    }

    vx::Web::openModal(url);

    LUAUTILS_STACK_SIZE_ASSERT(L, 0)
    return 0;
}

#include "VXRootNodeCodeEditor.hpp"

static int _system_publish_script(lua_State * const L) {
    LUAUTILS_STACK_SIZE(L)

    const int argsCount = lua_gettop(L);

    if (argsCount < 1 || lua_utils_getObjectType(L, 1) != ITEM_TYPE_SYSTEM) {
        LUAUTILS_ERROR(L, "System:PublishScript(script) - function should be called with `:`");
    }

    if (argsCount != 2) {
        LUAUTILS_ERROR(L, "System:PublishScript(script) - incorrect argument count");
    }

    if (lua_isstring(L, 2) == false) {
        LUAUTILS_ERROR(L, "System:PublishScript(script) - script should be a string");
    }

    const std::string script = std::string(lua_tostring(L, 2));

    vx::Game *g = nullptr;
    bool ok = lua_utils_getGameCppWrapperPtr(L, &g);
    if (ok == false || g == nullptr) {
        LUAUTILS_INTERNAL_ERROR(L);
    }
    vx::Game_WeakPtr wp = g->getWeakPtr();

    HubClient::getInstance().setWorldScript(HubClient::getInstance().getUniversalSender(),
                                            g->getID(),
                                            script,
                                            [wp, script](const bool &success, HttpRequest_SharedPtr request){
        vx::OperationQueue::getMain()->dispatch([wp, success, script](){
            if (success) {
                Game_SharedPtr g = wp.lock();
                if (g == nullptr) {
                    return;
                }

                //                RootNodeCodeEditor_SharedPtr editor = g->getEditorRootNode();
                //                if (editor != nullptr) {
                //                    editor->updateScript(script);
                //                }

                if (g->isSolo()) {
                    g->reload();
                } else {
                    g->setNeedsReload(true);
                }
            }
        });
    });

    LUAUTILS_STACK_SIZE_ASSERT(L, 0)
    return 0;
}

static int _system_rollback(lua_State * const L) {
    LUAUTILS_STACK_SIZE(L)

    const int argsCount = lua_gettop(L);

    if (argsCount < 1 || lua_utils_getObjectType(L, 1) != ITEM_TYPE_SYSTEM) {
        LUAUTILS_ERROR(L, "System:Rollback(hash) - function should be called with `:`");
    }

    if (argsCount != 2) {
        LUAUTILS_ERROR(L, "System:Rollback(hash) - incorrect argument count");
    }

    if (lua_isstring(L, 2) == false) {
        LUAUTILS_ERROR(L, "System:Rollback(hash) - hash should be a string");
    }

    const std::string hash = std::string(lua_tostring(L, 2));

    vx::Game *g = nullptr;
    bool ok = lua_utils_getGameCppWrapperPtr(L, &g);
    if (ok == false || g == nullptr) {
        LUAUTILS_INTERNAL_ERROR(L);
    }
    vx::Game_WeakPtr wp = g->getWeakPtr();

    HubClient::getInstance().rollbackWorld(HubClient::getInstance().getUniversalSender(),
                                           g->getID(),
                                           hash,
                                           [wp, hash](const bool &success, HttpRequest_SharedPtr request){
        vx::OperationQueue::getMain()->dispatch([wp, success, hash](){
            if (success) {
                Game_SharedPtr g = wp.lock();
                if (g == nullptr) {
                    return;
                }
                if (g->isSolo()) {
                    g->reload();
                } else {
                    g->setNeedsReload(true);
                }
            }
        });
    });

    LUAUTILS_STACK_SIZE_ASSERT(L, 0)
    return 0;
}

static int _system_iap_get_products(lua_State * const L) {
    const int argsCount = lua_gettop(L);
    if (argsCount < 1 || lua_utils_getObjectType(L, 1) != ITEM_TYPE_SYSTEM) {
        LUAUTILS_ERROR(L, "System:IAPProducts(callback) - function should be called with `:`");
    }

    if (argsCount != 2) {
        LUAUTILS_ERROR(L, "System:IAPProducts(callback) - incorrect argument count");
    }

    if (lua_isfunction(L, 2) == false) {
        LUAUTILS_ERROR(L, "System:IAPProducts(callback) - callback should be a function");
    }

    // store the Luau callback function to be able to retrieve it later
    int callbackIndex = 0;
    std::string callbackKey = "";
    bool callbackStoreSuccess = false;
    SAFE_LOCAL_STORE_SET(L, 2, callbackIndex, callbackKey, callbackStoreSuccess);
    if (callbackStoreSuccess == false) {
        // couldn't store function, not supposed to happen
        LUAUTILS_INTERNAL_ERROR(L);
    }

    vx::Game *g = nullptr;
    bool ok = lua_utils_getGameCppWrapperPtr(L, &g);
    if (ok == false) {
        LUAUTILS_INTERNAL_ERROR(L);
    }
    Game_WeakPtr gWeakPtr = g->getWeakPtr();

    std::vector<std::string> productIDs = {
        "blip.coins.1",
        "blip.coins.2",
        "blip.coins.3",
        "blip.coins.4",
        "blip.coins.5",
        "blip.coins.6",
        "blip.coins.7",
    };

    vx::IAP::getProducts(productIDs, [gWeakPtr, callbackIndex, callbackKey](std::unordered_map<std::string, vx::IAP::Product> products){
        OperationQueue::getMain()->dispatch([callbackIndex, callbackKey, gWeakPtr, products](){
            Game_SharedPtr g = gWeakPtr.lock(); if (g == nullptr) { return; }
            lua_State *L = g->getLuaState();

            // Retrieve the Luau callback function
            bool success = false;
            SAFE_LOCAL_STORE_GET(L, callbackIndex, callbackKey, success);
            if (success == false) { LUAUTILS_INTERNAL_ERROR(L); }
            SAFE_LOCAL_STORE_REMOVE(L, callbackIndex, callbackKey, success)
            if (success == false) { LUAUTILS_INTERNAL_ERROR(L); }
            if (lua_isfunction(L, -1) == false) { LUAUTILS_INTERNAL_ERROR(L); }
            // callback function is on top of the stack (index -1)

            lua_newtable(L);
            for (std::pair<std::string, vx::IAP::Product> entry : products) {
                lua_pushstring(L, entry.first.c_str());

                lua_newtable(L);
                {
                    lua_pushstring(L, "id");
                    lua_pushstring(L, entry.second.id.c_str());
                    lua_rawset(L, -3);

                    lua_pushstring(L, "title");
                    lua_pushstring(L, entry.second.title.c_str());
                    lua_rawset(L, -3);

                    lua_pushstring(L, "description");
                    lua_pushstring(L, entry.second.description.c_str());
                    lua_rawset(L, -3);

                    lua_pushstring(L, "price");
                    lua_pushnumber(L, static_cast<double>(entry.second.price));
                    lua_rawset(L, -3);

                    lua_pushstring(L, "displayPrice");
                    lua_pushstring(L, entry.second.displayPrice.c_str());
                    lua_rawset(L, -3);

                    lua_pushstring(L, "currency");
                    lua_pushstring(L, entry.second.currency.c_str());
                    lua_rawset(L, -3);

                    lua_pushstring(L, "currencySymbol");
                    lua_pushstring(L, entry.second.currencySymbol.c_str());
                    lua_rawset(L, -3);
                }
                lua_rawset(L, -3);
            }

            const int err = lua_pcall(L, 1, 0, 0);
            if (err != LUA_OK) {
                vxlog_debug("System:IAPProducts(callback) - failed to call callback function");
                if (lua_utils_isStringStrict(L, -1)) { vxlog_debug("%s", lua_tostring(L, -1));
                } else { vxlog_debug("lua error is not a string"); }
                lua_pop(L, 1);
            }

        });
    });

    return 0;
}

static int _system_iap_purchase(lua_State * const L) {
    // LUAUTILS_STACK_SIZE(L)

    const int argsCount = lua_gettop(L);

    if (argsCount < 1 || lua_utils_getObjectType(L, 1) != ITEM_TYPE_SYSTEM) {
        LUAUTILS_ERROR(L, "System:IAPPurchase(productID, callback) - function should be called with `:`");
    }

    if (argsCount != 3) {
        LUAUTILS_ERROR(L, "System:IAPPurchase(productID, callback) - incorrect argument count");
    }

    if (lua_isstring(L, 2) == false) {
        LUAUTILS_ERROR(L, "System:IAPPurchase(productID, callback) - productID should be a string");
    }

    if (lua_isfunction(L, 3) == false) {
        LUAUTILS_ERROR(L, "System:IAPPurchase(productID, callback) - callback should be a function");
    }

    // store the Luau callback function to be able to retrieve it later
    int callbackIndex = 0;
    std::string callbackKey = "";
    bool callbackStoreSuccess = false;
    SAFE_LOCAL_STORE_SET(L, 3, callbackIndex, callbackKey, callbackStoreSuccess);
    if (callbackStoreSuccess == false) {
        // couldn't store function, not supposed to happen
        LUAUTILS_INTERNAL_ERROR(L);
    }

    vx::Game *g = nullptr;
    bool ok = lua_utils_getGameCppWrapperPtr(L, &g);
    if (ok == false) {
        LUAUTILS_INTERNAL_ERROR(L);
    }
    Game_WeakPtr gWeakPtr = g->getWeakPtr();

    const vx::HubClient::UserCredentials creds = vx::HubClient::getInstance().getCredentials();
    const std::unordered_map<std::string, std::string> headers = {
        {HTTP_HEADER_CUBZH_USER_ID, creds.ID},
        {HTTP_HEADER_CUBZH_USER_TOKEN, creds.token},
        {HTTP_HEADER_CUBZH_CLIENT_VERSION_V2, vx::device::appVersionCached()},
        {HTTP_HEADER_CUBZH_CLIENT_BUILDNUMBER_V2, vx::device::appBuildNumberCached()},
    };

    const std::string productID = std::string(lua_tostring(L, 2));
    vx::IAP::purchase(productID, "https://api.cu.bzh/purchases/verify", headers,
                      [callbackIndex, callbackKey, gWeakPtr](const vx::IAP::Purchase_SharedPtr& purchase){

            switch (purchase->status) {
                case vx::IAP::Purchase::Status::Pending:
                    vxlog_debug("IAP PURCHASE PENDING FOR PRODUCT ID: %s", purchase->productID.c_str());
                    break;
                case vx::IAP::Purchase::Status::Success:
                    vxlog_debug("IAP PURCHASE SUCCESS: PRODUCT ID: %s - TRANSACTION ID: %s - RECEIPT: %s",
                                purchase->productID.c_str(),
                                purchase->transactionID.c_str(),
                                purchase->receiptData.substr(0, 50).c_str());
                    break;
                case vx::IAP::Purchase::Status::SuccessNotVerified:
                    vxlog_debug("IAP PURCHASE SUCCESS BUT NOT VERIFIED: PRODUCT ID: %s - TRANSACTION ID: %s - RECEIPT: %s",
                                purchase->productID.c_str(),
                                purchase->transactionID.c_str(),
                                purchase->receiptData.substr(0, 50).c_str());
                    // VERIFY
                    vx::HubClient::getInstance().verifyPurchase(purchase, [callbackIndex, callbackKey, gWeakPtr](bool iapSuccess, bool sandbox, std::string err, HttpRequest_SharedPtr req){
                        // vxlog_debug("VERIFY DONE, SUCCESS: %s", success ? "TRUE" : "FALSE");
                        vxlog_debug("VERIFY DONE, STATUS: %d", req->getResponse().getStatusCode());
                        vxlog_debug("SUCCESS: %s, SANDBOX: %s, ERR: %s",
                                    iapSuccess ? "TRUE" : "FALSE",
                                    sandbox ? "TRUE" : "FALSE",
                                    err.c_str());

                        OperationQueue::getMain()->dispatch([callbackIndex, callbackKey, gWeakPtr, iapSuccess, sandbox](){
                            Game_SharedPtr g = gWeakPtr.lock(); if (g == nullptr) { return; }
                            lua_State *L = g->getLuaState();
                            // Retrieve the Luau callback function
                            bool success = false;
                            SAFE_LOCAL_STORE_GET(L, callbackIndex, callbackKey, success);
                            if (success == false) { LUAUTILS_INTERNAL_ERROR(L); }
                            SAFE_LOCAL_STORE_REMOVE(L, callbackIndex, callbackKey, success)
                            if (success == false) { LUAUTILS_INTERNAL_ERROR(L); }
                            if (lua_isfunction(L, -1) == false) { LUAUTILS_INTERNAL_ERROR(L); }
                            // callback function is on top of the stack (index -1)

                            if (iapSuccess) {
                                lua_pushstring(L, "success");
                            } else {
                                lua_pushstring(L, "failure");
                            }
                            lua_pushboolean(L, sandbox);

                            const int err = lua_pcall(L, 2, 0, 0); // 0 args
                            if (err != LUA_OK) {
                                vxlog_debug("System:IAPPurchase(productID, callback) - failed to call callback function");
                                if (lua_utils_isStringStrict(L, -1)) { vxlog_debug("%s", lua_tostring(L, -1));
                                } else { vxlog_debug("lua error is not a string"); }
                                lua_pop(L, 1);
                            }
                        });

                    });
                    break;
                case vx::IAP::Purchase::Status::Failed:
                {
                    vxlog_debug("IAP PURCHASE FAILED FOR PRODUCT ID: %s", purchase->productID.c_str());

                    OperationQueue::getMain()->dispatch([callbackIndex, callbackKey, gWeakPtr](){
                        Game_SharedPtr g = gWeakPtr.lock(); if (g == nullptr) { return; }
                        lua_State *L = g->getLuaState();
                        // Retrieve the Luau callback function
                        bool success = false;
                        SAFE_LOCAL_STORE_GET(L, callbackIndex, callbackKey, success);
                        if (success == false) { LUAUTILS_INTERNAL_ERROR(L); }
                        SAFE_LOCAL_STORE_REMOVE(L, callbackIndex, callbackKey, success)
                        if (success == false) { LUAUTILS_INTERNAL_ERROR(L); }
                        if (lua_isfunction(L, -1) == false) { LUAUTILS_INTERNAL_ERROR(L); }
                        // callback function is on top of the stack (index -1)

                        lua_pushstring(L, "failed");
                        const int err = lua_pcall(L, 1, 0, 0); // 0 args
                        if (err != LUA_OK) {
                            vxlog_debug("System:IAPPurchase(productID, callback) - failed to call callback function");
                            if (lua_utils_isStringStrict(L, -1)) { vxlog_debug("%s", lua_tostring(L, -1));
                            } else { vxlog_debug("lua error is not a string"); }
                            lua_pop(L, 1);
                        }
                    });

                    break;
                }
                case vx::IAP::Purchase::Status::Cancelled:
                {
                    vxlog_debug("IAP PURCHASE CANCELLED FOR PRODUCT ID: %s", purchase->productID.c_str());

                    OperationQueue::getMain()->dispatch([callbackIndex, callbackKey, gWeakPtr](){
                        Game_SharedPtr g = gWeakPtr.lock(); if (g == nullptr) { return; }
                        lua_State *L = g->getLuaState();
                        // Retrieve the Luau callback function
                        bool success = false;
                        SAFE_LOCAL_STORE_GET(L, callbackIndex, callbackKey, success);
                        if (success == false) { LUAUTILS_INTERNAL_ERROR(L); }
                        SAFE_LOCAL_STORE_REMOVE(L, callbackIndex, callbackKey, success)
                        if (success == false) { LUAUTILS_INTERNAL_ERROR(L); }
                        if (lua_isfunction(L, -1) == false) { LUAUTILS_INTERNAL_ERROR(L); }
                        // callback function is on top of the stack (index -1)

                        lua_pushstring(L, "cancelled");
                        const int err = lua_pcall(L, 1, 0, 0); // 0 args
                        if (err != LUA_OK) {
                            vxlog_debug("System:IAPPurchase(productID, callback) - failed to call callback function");
                            if (lua_utils_isStringStrict(L, -1)) { vxlog_debug("%s", lua_tostring(L, -1));
                            } else { vxlog_debug("lua error is not a string"); }
                            lua_pop(L, 1);
                        }
                    });

                    break;
                }
                case vx::IAP::Purchase::Status::InvalidProduct:
                    vxlog_debug("IAP PURCHASE INVALID PRODUCT ID: %s", purchase->productID.c_str());
                    break;
            }
    });

    return 0;
}


// args
// 1 <System>
// 2 <string> store name
// 3 <[]string> keys
// 4 <function> callback
static int _system_kvs_get(lua_State * const L) {
    LUAUTILS_STACK_SIZE(L)

    const int callbackIndex = 4;

    // validate arguments
    const int argsCount = lua_gettop(L);
    if (argsCount < 1 || lua_utils_getObjectType(L, 1) != ITEM_TYPE_SYSTEM) {
        LUAUTILS_ERROR(L, "System:KvsGet - function should be called with `:`");
    }
    if (argsCount != 4) {
        LUAUTILS_ERROR(L, "System:KvsGet - incorrect argument count");
    }
    if (lua_utils_isStringStrict(L, 2) == false) {
        LUAUTILS_ERROR(L, "System:KvsGet - argument 1 (store) should be a string");
    }
    if (lua_istable(L, 3) == false) {
        LUAUTILS_ERROR(L, "System:KvsGet - argument 2 (keys) should be a table (array of strings)");
    }
    if (lua_isfunction(L, callbackIndex) == false) {
        LUAUTILS_ERROR(L, "System:KvsGet - argument 4 (callback) should be a function");
    }

    // retrieve game info
    vx::Game *cppGame = nullptr;
    bool ok = lua_utils_getGameCppWrapperPtr(L, &cppGame);
    if (ok == false || cppGame == nullptr) {
        LUAUTILS_ERROR(L, "KVS get failed. Internal error.")
        return 0;
    }
    vx::Game_WeakPtr weakPtr = cppGame->getWeakPtr();
    const std::string worldID = cppGame->getID();

    if (worldID.empty()) {
        LUAUTILS_ERROR(L, "KVS get failed, world has no ID")
    }

    // store name
    const std::string store(lua_tostring(L, 2));

    // keys
    std::unordered_set<std::string> keys;
    ok = lua_http_parse_string_set(L, 3, keys);
    if (ok == false) {
        LUAUTILS_ERROR(L, "System:KvsGet - failed to parse keys argument. It should be an array of strings");
    }

    // construct URL
    const std::string scheme = HubClient::getInstance().getHubServerScheme();
    std::string addr = HubClient::getInstance().getHubServerAddr();
    uint16_t port = HubClient::getInstance().getHubServerPort();
#if defined(ONLINE_GAMESERVER)
    port = GAMESERVER_API_PORT;
    addr = GAMESERVER_API_HOST;
#endif
    vx::URL url = vx::URL::make(scheme + "://" + addr + ":" + std::to_string(port) + "/kvs/world/"+worldID+"/store/"+store);
    url.setQuery(QueryParams({{"key", std::move(keys)}}));
    if (url.isValid() == false) {
        LUAUTILS_ERROR(L, "kvs get - URL is not valid");
    }

    std::unordered_map<std::string, std::string> headers;
    // If URL targets Cubzh' API server, then add the user's credentials to the HTTP headers
    {
#if defined(CLIENT_ENV)
        const vx::HubClient::UserCredentials creds = vx::HubClient::getInstance().getCredentials();
        headers.emplace(HTTP_HEADER_CUBZH_CLIENT_VERSION_V2, vx::device::appVersionCached());
        headers.emplace(HTTP_HEADER_CUBZH_CLIENT_BUILDNUMBER_V2, vx::device::appBuildNumberCached());
        headers.emplace(HTTP_HEADER_CUBZH_USER_ID, creds.ID);
        headers.emplace(HTTP_HEADER_CUBZH_USER_TOKEN, creds.token);
#elif defined(ONLINE_GAMESERVER)
        headers.emplace(HTTP_HEADER_CUBZH_GAME_SERVER_TOKEN, HTTP_HEADER_CUBZH_GAME_SERVER_TOKEN_VALUE);
#endif
    }

    HttpRequestOpts opts;
    opts.setForceCacheRevalidate(true);
    opts.setSendNow(false);

    vx::HttpRequest_SharedPtr req = vx::HttpClient::shared().GET(url,
                                                                 headers,
                                                                 opts,
                                                                 [weakPtr](vx::HttpRequest_SharedPtr req){
        if (req->getStatus() == vx::HttpRequest::Status::CANCELLED) {
            vxlog_error("[System.KvsGet] Request was cancelled (1)");
            return;
        }

        vx::Game_SharedPtr cppGame = weakPtr.lock();
        if (cppGame == nullptr) {
            vxlog_error("[System.KvsGet] C++ game is null (1)");
            return;
        }

        vx::OperationQueue *mainQueue = (cppGame->isInClientMode()
                                         ? vx::OperationQueue::getMain()
                                         : vx::OperationQueue::getServerMain());

        mainQueue->dispatch([weakPtr, req]() {
            if (req->getStatus() == vx::HttpRequest::Status::CANCELLED) {
                vxlog_error("[System.KvsGet] Request was cancelled (2)");
                return;
            }

            // Get L from weak game ptr
            vx::Game_SharedPtr cppGame = weakPtr.lock();
            if (cppGame == nullptr) {
                vxlog_error("[System.KvsGet] C++ game is null (2)");
                return;
            }
            lua_State * const L = cppGame->getLuaState();
            vx_assert(L != nullptr);

            // call callback function
            lua_http_kvs_get_callback(L, req);
        });
    });

    lua_http_request_create_and_push(L, callbackIndex, req);

    req->sendAsync();

    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
    return 1;
}

// args
// 1 <System>
// 2 <string> store name
// 3 <map[string]any> key-value map
// 4 <function> callback
static int _system_kvs_set(lua_State * const L) {
    LUAUTILS_STACK_SIZE(L)

    const int callbackIndex = 4;

    // validate arguments
    const int argsCount = lua_gettop(L);
    if (argsCount < 1 || lua_utils_getObjectType(L, 1) != ITEM_TYPE_SYSTEM) {
        LUAUTILS_ERROR(L, "System:KvsSet - function should be called with `:`");
    }
    if (argsCount != 4) {
        LUAUTILS_ERROR(L, "System:KvsSet - incorrect argument count");
    }
    if (lua_utils_isStringStrict(L, 2) == false) {
        LUAUTILS_ERROR(L, "System:KvsSet - argument 1 (store) should be a string");
    }
    if (lua_istable(L, 3) == false) {
        LUAUTILS_ERROR(L, "System:KvsSet - argument 2 (key-value pairs) should be a table map[string]any");
    }
    if (lua_isfunction(L, callbackIndex) == false) {
        LUAUTILS_ERROR(L, "System:KvsSet - argument 4 (callback) should be a function");
    }

    // retrieve game info
    vx::Game *cppGame = nullptr;
    bool ok = lua_utils_getGameCppWrapperPtr(L, &cppGame);
    if (ok == false || cppGame == nullptr) {
        LUAUTILS_ERROR(L, "KVS set failed. Internal error.")
        return 0;
    }
    vx::Game_WeakPtr weakPtr = cppGame->getWeakPtr();
    const std::string worldID = cppGame->getID();

    if (worldID.empty()) {
        LUAUTILS_ERROR(L, "KVS set failed, world has no ID")
    }

    // store name
    const std::string store(lua_tostring(L, 2));

    // key-value pairs
    std::unordered_map<std::string, std::string> keyValuePairs;
    ok = lua_http_encode_kv_map(L, 3, keyValuePairs);
    if (ok == false) {
        LUAUTILS_ERROR(L, "System:KvsSet - failed to parse key-value pairs argument. It should be a map[string]any");
    }

    // body : encode map[string]base64 into JSON
    std::string body = "";
    {
        cJSON *json = cJSON_CreateObject();
        for (std::pair<std::string, std::string> pair : keyValuePairs) {
            vx::json::writeStringField(json, pair.first, pair.second);
        }
        body.assign(cJSON_PrintUnformatted(json));
    }

    // construct URL
    const std::string scheme = HubClient::getInstance().getHubServerScheme();
    std::string addr = HubClient::getInstance().getHubServerAddr();
    uint16_t port = HubClient::getInstance().getHubServerPort();
#if defined(ONLINE_GAMESERVER)
    port = GAMESERVER_API_PORT;
    addr = GAMESERVER_API_HOST;
#endif
    vx::URL url = vx::URL::make(scheme + "://" + addr + ":" + std::to_string(port) + "/kvs/world/"+worldID+"/store/"+store);
    if (url.isValid() == false) {
        LUAUTILS_ERROR(L, "kvs set - URL is not valid");
    }

    std::unordered_map<std::string, std::string> headers;
    // If URL targets Cubzh' API server, then add the user's credentials to the HTTP headers
    {
#if defined(CLIENT_ENV)
        const vx::HubClient::UserCredentials creds = vx::HubClient::getInstance().getCredentials();
        headers.emplace(HTTP_HEADER_CUBZH_CLIENT_VERSION_V2, vx::device::appVersionCached());
        headers.emplace(HTTP_HEADER_CUBZH_CLIENT_BUILDNUMBER_V2, vx::device::appBuildNumberCached());
        headers.emplace(HTTP_HEADER_CUBZH_USER_ID, creds.ID);
        headers.emplace(HTTP_HEADER_CUBZH_USER_TOKEN, creds.token);
#elif defined(ONLINE_GAMESERVER)
        headers.emplace(HTTP_HEADER_CUBZH_GAME_SERVER_TOKEN, HTTP_HEADER_CUBZH_GAME_SERVER_TOKEN_VALUE);
#endif
    }

    HttpRequestOpts opts;
    opts.setForceCacheRevalidate(true);
    opts.setSendNow(false);

    vx::HttpRequest_SharedPtr req = vx::HttpClient::shared().PATCH(url,
                                                                   headers,
                                                                   body,
                                                                   opts,
                                                                   [weakPtr](vx::HttpRequest_SharedPtr req){
        if (req->getStatus() == vx::HttpRequest::Status::CANCELLED) { return; }

        vx::Game_SharedPtr cppGame = weakPtr.lock();
        if (cppGame == nullptr)
            return;

        vx::OperationQueue *mainQueue = (cppGame->isInClientMode()
                                         ? vx::OperationQueue::getMain()
                                         : vx::OperationQueue::getServerMain());

        mainQueue->dispatch([weakPtr, req]() {
            if (req->getStatus() == vx::HttpRequest::Status::CANCELLED) { return; }

            // Get L from weak game ptr
            vx::Game_SharedPtr cppGame = weakPtr.lock();
            if (cppGame == nullptr) {
                return;
            }
            lua_State * const L = cppGame->getLuaState();
            vx_assert(L != nullptr);

            // call callback function
            lua_http_kvs_set_callback(L, req);
        });
    });

    lua_http_request_create_and_push(L, callbackIndex, req);

    req->sendAsync();

    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
    return 1;
}

static int _system_kvs_remove(lua_State * const L) {
    LUAUTILS_STACK_SIZE(L)

    const int callbackIndex = 4;

    // validate arguments
    const int argsCount = lua_gettop(L);
    if (argsCount < 1 || lua_utils_getObjectType(L, 1) != ITEM_TYPE_SYSTEM) {
        LUAUTILS_ERROR(L, "System:KvsRemove - function should be called with `:`");
    }
    if (argsCount != 4) {
        LUAUTILS_ERROR(L, "System:KvsRemove - incorrect argument count");
    }
    if (lua_utils_isStringStrict(L, 2) == false) {
        LUAUTILS_ERROR(L, "System:KvsRemove - argument 1 (store) should be a string");
    }
    if (lua_istable(L, 3) == false) {
        LUAUTILS_ERROR(L, "System:KvsRemove - argument 2 (keys) should be a table (array of strings)");
    }
    if (lua_isfunction(L, callbackIndex) == false) {
        LUAUTILS_ERROR(L, "System:KvsRemove - argument 4 (callback) should be a function");
    }

    // retrieve game info
    vx::Game *cppGame = nullptr;
    bool ok = lua_utils_getGameCppWrapperPtr(L, &cppGame);
    if (ok == false || cppGame == nullptr) {
        LUAUTILS_ERROR(L, "KVS remove failed. Internal error.")
        return 0;
    }
    vx::Game_WeakPtr weakPtr = cppGame->getWeakPtr();
    const std::string worldID = cppGame->getID();

    if (worldID.empty()) {
        LUAUTILS_ERROR(L, "KVS remove failed, world has no ID")
    }

    // store name
    const std::string store(lua_tostring(L, 2));

    // keys
    std::unordered_set<std::string> keys;
    ok = lua_http_parse_string_set(L, 3, keys);
    if (ok == false) {
        LUAUTILS_ERROR(L, "System:KvsRemove - failed to parse keys argument. It should be an array of strings");
    }

    // body : encode map[string]base64 into JSON
    std::string body = "";
    {
        cJSON *json = cJSON_CreateObject();
        for (std::string key : keys) {
            vx::json::writeNullField(json, key);
        }
        body.assign(cJSON_PrintUnformatted(json));
    }

    // construct URL
    const std::string scheme = HubClient::getInstance().getHubServerScheme();
    std::string addr = HubClient::getInstance().getHubServerAddr();
    uint16_t port = HubClient::getInstance().getHubServerPort();
#if defined(ONLINE_GAMESERVER)
    port = GAMESERVER_API_PORT;
    addr = GAMESERVER_API_HOST;
#endif
    vx::URL url = vx::URL::make(scheme + "://" + addr + ":" + std::to_string(port) + "/kvs/world/"+worldID+"/store/"+store);
    if (url.isValid() == false) {
        LUAUTILS_ERROR(L, "kvs get - URL is not valid");
    }

    std::unordered_map<std::string, std::string> headers;
    // If URL targets Cubzh' API server, then add the user's credentials to the HTTP headers
    {
#if defined(CLIENT_ENV)
        const vx::HubClient::UserCredentials creds = vx::HubClient::getInstance().getCredentials();
        headers.emplace(HTTP_HEADER_CUBZH_CLIENT_VERSION_V2, vx::device::appVersionCached());
        headers.emplace(HTTP_HEADER_CUBZH_CLIENT_BUILDNUMBER_V2, vx::device::appBuildNumberCached());
        headers.emplace(HTTP_HEADER_CUBZH_USER_ID, creds.ID);
        headers.emplace(HTTP_HEADER_CUBZH_USER_TOKEN, creds.token);
#elif defined(ONLINE_GAMESERVER)
        headers.emplace(HTTP_HEADER_CUBZH_GAME_SERVER_TOKEN, HTTP_HEADER_CUBZH_GAME_SERVER_TOKEN_VALUE);
#endif
    }

    HttpRequestOpts opts;
    opts.setForceCacheRevalidate(true);
    opts.setSendNow(false);

    vx::HttpRequest_SharedPtr req = vx::HttpClient::shared().PATCH(url,
                                                                   headers,
                                                                   body,
                                                                   opts,
                                                                   [weakPtr](vx::HttpRequest_SharedPtr req){
        if (req->getStatus() == vx::HttpRequest::Status::CANCELLED) { return; }

        vx::Game_SharedPtr cppGame = weakPtr.lock();
        if (cppGame == nullptr)
            return;


        vx::OperationQueue *mainQueue = (cppGame->isInClientMode()
                                         ? vx::OperationQueue::getMain()
                                         : vx::OperationQueue::getServerMain());

        mainQueue->dispatch([weakPtr, req]() {
            if (req->getStatus() == vx::HttpRequest::Status::CANCELLED) { return; }

            // Get L from weak game ptr
            vx::Game_SharedPtr cppGame = weakPtr.lock();
            if (cppGame == nullptr) {
                return;
            }
            lua_State * const L = cppGame->getLuaState();
            vx_assert(L != nullptr);

            // call callback function
            lua_http_kvs_get_callback(L, req);
        });
    });

    lua_http_request_create_and_push(L, callbackIndex, req);

    req->sendAsync();

    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
    return 1;
}

// MARK: - Elevated API functions -

/// System:SetLayersElevated(object, layers)
static int _set_layers_elevated(lua_State * const L) {
    vx_assert(L != nullptr);
    LUAUTILS_STACK_SIZE(L)

    const int argsCount = lua_gettop(L);

    if (argsCount < 1 || lua_utils_getObjectType(L, 1) != ITEM_TYPE_SYSTEM) {
        LUAUTILS_ERROR(L, "System:SetLayersElevated - function should be called with `:`");
    }

    Transform *self; lua_object_getTransformPtr(L, 2, &self);
    if (self == nullptr) {
        LUAUTILS_ERROR(L, "[System:SetLayersElevated] first argument should have an Object component")
    }

    const uint8_t elevatedBits = CAMERA_LAYERS_MASK_API_BITS + CAMERA_LAYERS_MASK_SYSTEM_BITS;

    uint16_t mask = 0;
    if (lua_utils_getMask(L, 3, &mask, elevatedBits) == false) {
        LUAUTILS_ERROR_F(L,
                         "System.SetLayersElevated cannot be set (new value should be one or a table of integers between 1 and %d)",
                         elevatedBits)
    }

    switch (transform_get_type(self)) {
        case QuadTransform:
            quad_set_layers(static_cast<Quad *>(transform_get_ptr(self)), mask);
            break;
        case ShapeTransform:
            shape_set_layers(static_cast<Shape *>(transform_get_ptr(self)), mask);
            break;
        case WorldTextTransform:
            world_text_set_layers(static_cast<WorldText *>(transform_get_ptr(self)), mask);
            break;
        case CameraTransform:
            camera_set_layers(static_cast<Camera *>(transform_get_ptr(self)), mask);
            break;
        case LightTransform:
            light_set_layers(static_cast<Light *>(transform_get_ptr(self)), mask);
            break;
        case MeshTransform:
            mesh_set_layers(static_cast<Mesh *>(transform_get_ptr(self)), mask);
            break;
        default:
            break;
    }

    LUAUTILS_STACK_SIZE_ASSERT(L, 0)
    return 0;
}

static int _get_layers_elevated(lua_State * const L) {
    vx_assert(L != nullptr);
    LUAUTILS_ASSERT_ARG_TYPE(L, 1, ITEM_TYPE_SYSTEM)
    LUAUTILS_STACK_SIZE(L)

    Transform *self; lua_object_getTransformPtr(L, 2, &self);
    if (self == nullptr) {
        LUAUTILS_ERROR(L, "System:GetLayersElevated(object) - object should have an Object component")
    }

    uint16_t mask = 0;

    switch (transform_get_type(self)) {
        case QuadTransform:
            mask = quad_get_layers(static_cast<Quad *>(transform_get_ptr(self)));
            break;
        case ShapeTransform:
            mask = shape_get_layers(static_cast<Shape *>(transform_get_ptr(self)));
            break;
        case WorldTextTransform:
            mask = world_text_get_layers(static_cast<WorldText *>(transform_get_ptr(self)));
            break;
        case CameraTransform:
            mask = camera_get_layers(static_cast<Camera *>(transform_get_ptr(self)));
            break;
        case LightTransform:
            mask = light_get_layers(static_cast<Light *>(transform_get_ptr(self)));
            break;
        case MeshTransform:
            mask = mesh_get_layers(static_cast<Mesh *>(transform_get_ptr(self)));
            break;
        default:
            break;
    }

    const uint8_t elevatedBits = CAMERA_LAYERS_MASK_API_BITS + CAMERA_LAYERS_MASK_SYSTEM_BITS;


    lua_utils_pushMaskAsTable(L, mask, elevatedBits);
    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
    return 1;
}

static int _get_groups_elevated(lua_State * const L) {
    vx_assert(L != nullptr);
    LUAUTILS_ASSERT_ARG_TYPE(L, 1, ITEM_TYPE_SYSTEM)
    LUAUTILS_STACK_SIZE(L)

    const uint8_t elevatedBits = PHYSICS_GROUP_MASK_API_BITS + PHYSICS_GROUP_MASK_SYSTEM_BITS;

    uint16_t mask = 0;
    if (lua_utils_getMask(L, 2, &mask, elevatedBits) == false) {
        LUAUTILS_ERROR_F(L, "System.GetGroupsElevated failed (value should be one or a table of integers between 1 and %d)",
                         elevatedBits)
    }

    lua_collision_groups_create_and_push(L, mask);
    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
    return 1;
}

/// System:SetCollisionGroupsElevated(object, layers)
static int _set_groups_elevated(lua_State * const L) {
    vx_assert(L != nullptr);
    LUAUTILS_ASSERT_ARG_TYPE(L, 1, ITEM_TYPE_SYSTEM)
    LUAUTILS_STACK_SIZE(L)

    Transform *self; lua_object_getTransformPtr(L, 2, &self);
    if (self == nullptr) {
        LUAUTILS_ERROR(L, "System:SetCollisionGroupsElevated - first argument should have an Object component")
    }

    const uint8_t elevatedBits = PHYSICS_GROUP_MASK_API_BITS + PHYSICS_GROUP_MASK_SYSTEM_BITS;

    uint16_t mask = 0;
    if (lua_utils_getMask(L, 3, &mask, elevatedBits) == false) {
        LUAUTILS_ERROR_F(L, "System.SetCollisionGroupsElevated cannot be set (new value should be one or a table of integers between 1 and %d)",
                         elevatedBits)
    }

    RigidBody *rb = transform_get_rigidbody(self);
    if (rb != nullptr) {
        rigidbody_set_groups(rb, mask);
    }

    LUAUTILS_STACK_SIZE_ASSERT(L, 0)
    return 0;
}

/// System:SetCollidesWithGroupsElevated(object, layers)
static int _set_collideswith_elevated(lua_State * const L) {
    vx_assert(L != nullptr);
    LUAUTILS_ASSERT_ARG_TYPE(L, 1, ITEM_TYPE_SYSTEM)
    LUAUTILS_STACK_SIZE(L)

    Transform *self; lua_object_getTransformPtr(L, 2, &self);
    if (self == nullptr) {
        LUAUTILS_ERROR(L, "[System:SetCollidesWithGroupsElevated] first argument should have an Object component")
    }

    const uint8_t elevatedBits = PHYSICS_GROUP_MASK_API_BITS + PHYSICS_GROUP_MASK_SYSTEM_BITS;

    uint16_t mask = 0;
    if (lua_utils_getMask(L, 3, &mask, elevatedBits) == false) {
        LUAUTILS_ERROR_F(L, "System.SetCollidesWithGroupsElevated cannot be set (new value should be one or a table of integers between 1 and %d)",
                         elevatedBits)
    }

    RigidBody *rb = transform_get_rigidbody(self);
    if (rb != nullptr) {
        rigidbody_set_collides_with(rb, mask);
    }

    LUAUTILS_STACK_SIZE_ASSERT(L, 0)
    return 0;
}

static int _multilineinput(lua_State * const L) {
    LUAUTILS_STACK_SIZE(L)

    Game *game = nullptr;
    if (lua_utils_getGameCppWrapperPtr(L, &game) == false || game == nullptr) {
        vxlog_error("_multilineInput can't get game from registry");
        return 0;
    }
    Game_WeakPtr gameWP = game->getWeakPtr();

    const int argCount = lua_gettop(L);

    std::string content = "";
    std::string title = "";
    std::string hint = "";
    std::string regex = "";
    int maxChar = 10000;

    if (argCount >= 1 && lua_isstring(L, 1)) {
        content = std::string(lua_tostring(L, 1));
    }

    if (argCount >= 2 && lua_isstring(L, 2)) {
        title = std::string(lua_tostring(L, 2));
    }

    if (argCount >= 3 && lua_isstring(L, 3)) {
        hint = std::string(lua_tostring(L, 3));
    }

    if (argCount >= 4 && lua_isstring(L, 4)) {
        regex = std::string(lua_tostring(L, 4));
    }

    if (argCount >= 5 && lua_utils_isIntegerStrict(L, 5)) {
        maxChar = static_cast<int>(lua_tointeger(L, 5));
    }

    if (argCount >= 6 && lua_isfunction(L, 6)) {
        lua_utils_pushRootGlobalsFromRegistry(L); // TODO: use registry for this
        lua_pushstring(L, "__multiline_done__");
        lua_pushvalue(L, 6);
        lua_rawset(L, -3);
        lua_pop(L, 1); // pop global
    }

    if (argCount >= 7 && lua_isfunction(L, 7)) {
        lua_utils_pushRootGlobalsFromRegistry(L); // TODO: use registry for this
        lua_pushstring(L, "__multiline_cancel__");
        lua_pushvalue(L, 7);
        lua_rawset(L, -3);
        lua_pop(L, 1); // pop global
    }

    PopupEditText_SharedPtr popup = PopupEditText::makeMultiline(title,
                                                                 hint,
                                                                 content,
                                                                 regex,
                                                                 maxChar);

    popup->setDoneCallback([gameWP](PopupEditText_SharedPtr popup, const std::string text){
        Game_SharedPtr game = gameWP.lock();
        if (game == nullptr) return;

        lua_State *L = game->getLuaState();
        if (L == nullptr) return;

        LUAUTILS_STACK_SIZE(L)
        lua_utils_pushRootGlobalsFromRegistry(L); // TODO: use registry for this

        lua_pushstring(L, "__multiline_done__");
        if (lua_rawget(L, -2) != LUA_TFUNCTION) {
            lua_pop(L, 1);
        } else {
            lua_pushstring(L, text.c_str());

            if (lua_pcall(L, 1, 0, 0) != LUA_OK) { // pops function + arguments
                if (lua_utils_isStringStrict(L, -1)) {
                    lua_log_error_CStr(L, lua_tostring(L, -1));
                } else {
                    lua_log_error(L, "_multilineInput callback error");
                }
                lua_pop(L, 1); // pops the error
            }
        }

        lua_pushstring(L, "__multiline_done__");
        lua_pushnil(L);
        lua_rawset(L, -3);
        lua_pushstring(L, "__multiline_cancel__");
        lua_pushnil(L);
        lua_rawset(L, -3);

        lua_pop(L, 1); // pop global table

        LUAUTILS_STACK_SIZE_ASSERT(L, 0)
        popup->close();
    });

    popup->setCancelCallback([gameWP](PopupEditText_SharedPtr popup){
        Game_SharedPtr game = gameWP.lock();
        if (game == nullptr) return;

        lua_State *L = game->getLuaState();
        if (L == nullptr) return;

        LUAUTILS_STACK_SIZE(L)
        lua_utils_pushRootGlobalsFromRegistry(L); // TODO: use registry for this

        lua_pushstring(L, "__multiline_cancel__");
        if (lua_rawget(L, -2) != LUA_TFUNCTION) {
            lua_pop(L, 1);
        } else {
            if (lua_pcall(L, 0, 0, 0) != LUA_OK) { // pops function + arguments
                if (lua_utils_isStringStrict(L, -1)) {
                    lua_log_error_CStr(L, lua_tostring(L, -1));
                } else {
                    lua_log_error(L, "_multilineInput callback error");
                }
                lua_pop(L, 1); // pops the error
            }
        }

        lua_pushstring(L, "__multiline_done__");
        lua_pushnil(L);
        lua_rawset(L, -3);
        lua_pushstring(L, "__multiline_cancel__");
        lua_pushnil(L);
        lua_rawset(L, -3);

        lua_pop(L, 1); // pop global table

        LUAUTILS_STACK_SIZE_ASSERT(L, 0)
        popup->close();
    });

    GameCoordinator::getInstance()->showPopup(popup);
    LUAUTILS_STACK_SIZE_ASSERT(L, 0)
    return 0;
}

static int _logout(lua_State * const L) {
    LUAUTILS_STACK_SIZE(L)
    GameCoordinator::getInstance()->logout(false);
    LUAUTILS_STACK_SIZE_ASSERT(L, 0)
    return 0;
}

static int _logoutAndExit(lua_State * const L) {
    LUAUTILS_STACK_SIZE(L)
    GameCoordinator::getInstance()->logout(true);
    LUAUTILS_STACK_SIZE_ASSERT(L, 0)
    return 0;
}

// args:
// 1 : (System)
// 2 : (string) Lua command to execute
static int _execCommand(lua_State * const L) {
    // make sure this is running in main thread
    vx_assert(vx::ThreadManager::shared().isMainThread());

    LUAUTILS_STACK_SIZE(L)

    // 2 arguments are expected
    const int argCount = lua_gettop(L);
    if (argCount < 2) { return 0; }

    // TODO make sure argument 1 is `System`

    const char* luaCommandStr = lua_tostring(L, 2);
    if (luaCommandStr == nullptr || strlen(luaCommandStr) == 0) {
        LUAUTILS_STACK_SIZE_ASSERT(L, 0)
        return 0;
    }

    vx::Game *cppGame = nullptr;
    if (lua_utils_getGameCppWrapperPtr(L, &cppGame) == false) {
        LUAUTILS_INTERNAL_ERROR(L)
    }

    cppGame->doString(luaCommandStr);

    LUAUTILS_STACK_SIZE_ASSERT(L, 0)
    return 0;
}

// args:
// 1 : System
// 2 : (string) event name
static int _debugEvent(lua_State * const L) {
    LUAUTILS_STACK_SIZE(L)

    const int argCount = lua_gettop(L);
    if (argCount < 2) { return 0; }
    if (lua_isstring(L, 2) == false) {
        vxlog_error("debugEvent expects string parameter (event name)");
        return 0;
    }
    std::string eventName = std::string(lua_tostring(L, 2));

    std::unordered_map<std::string, std::string> properties;

    // optional properties
    if (argCount >= 3 && lua_istable(L, 3)) {
        lua_pushvalue(L, 3);
        lua_pushnil(L);
        while (lua_next(L, -2) != 0) {
            // key at -2, value at -1
            if (lua_isstring(L, -2) && lua_isstring(L, -1)) {
                properties[lua_tostring(L,-2)] = lua_tostring(L, -1);
            }
            lua_pop(L, 1); // pop value, keep key
        }
        lua_pop(L, 1); // pop properties table
    }


    vx::tracking::TrackingClient::shared().trackEvent(eventName, properties);

    LUAUTILS_STACK_SIZE_ASSERT(L, 0)
    return 0;
}

static int _launchEnvironment(lua_State * const L) {
    LUAUTILS_STACK_SIZE(L)
    vx::GameCoordinator::getInstance()->launchEnvironment();
    LUAUTILS_STACK_SIZE_ASSERT(L, 0)
    return 0;
}

static int _connectToServer(lua_State * const L) {
    LUAUTILS_STACK_SIZE(L)
    Game *g = nullptr;
    if (lua_utils_getGameCppWrapperPtr(L, &g) == false || g == nullptr) {
        LUAUTILS_INTERNAL_ERROR(L)
        return 0;
    }
    g->connect();
    LUAUTILS_STACK_SIZE_ASSERT(L, 0)
    return 0;
}

static int _log(lua_State * const L) {
    LUAUTILS_STACK_SIZE(L)
    const int argCount = lua_gettop(L);
    if (argCount < 1) { return 0; }
    if (lua_isstring(L, 1) == false) { return 0; }

    const char *s = lua_tostring(L, 1);
    vxlog_info("%s", s);

    LUAUTILS_STACK_SIZE_ASSERT(L, 0)
    return 0;
}

// MARK: - Commands History functions' implementation -

static int _add_command_in_history(lua_State *L) {
    LUAUTILS_STACK_SIZE(L)

    const int argsCount = lua_gettop(L);

    if (argsCount < 1 || lua_utils_getObjectType(L, 1) != ITEM_TYPE_SYSTEM) {
        LUAUTILS_ERROR(L, "System:AddCommandInHistory - function should be called with `:`");
    }
    if (argsCount != 2) {
        LUAUTILS_ERROR(L, "System:AddCommandInHistory - incorrect argument count");
    }

    std::string command = std::string(lua_tostring(L, 2));

    GameCoordinator::getInstance()->addPreviousCommand(command);

    LUAUTILS_STACK_SIZE_ASSERT(L, 0)
    return 0;
}
static int _get_command_from_history(lua_State *L) {
    LUAUTILS_STACK_SIZE(L)

    const int argsCount = lua_gettop(L);

    if (argsCount < 1 || lua_utils_getObjectType(L, 1) != ITEM_TYPE_SYSTEM) {
        LUAUTILS_ERROR(L, "System:GetCommandFromHistory - function should be called with `:`");
    }
    if (argsCount != 2) {
        LUAUTILS_ERROR(L, "System:GetCommandFromHistory - incorrect argument count");
    }

    size_t index = static_cast<size_t>(lua_tonumber(L, 2));

    if (index <= 0) {
        lua_pushnil(L);
        LUAUTILS_STACK_SIZE_ASSERT(L, 1)
        return 1;
    }

    size_t s = GameCoordinator::getInstance()->getNbPreviousCommands();
    if (s == 0) {
        lua_pushnil(L);
        LUAUTILS_STACK_SIZE_ASSERT(L, 1)
        return 1;
    }

    if (index > s) {
        index = s;
    }
    const std::string& command = GameCoordinator::getInstance()->getPreviousCommand(index);
    lua_pushstring(L, command.c_str());

    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
    return 1;
}

static int _terminate(lua_State *L) {
    LUAUTILS_STACK_SIZE(L)

    vx::device::terminate();

    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
    return 1;
}

// Arguments
// - userID
// - challenge
// - userName (nullable)
// - callback(error string)
//
// TODO: gaetan: get the challenge from the API, in this function
static int _system_passkey_create(lua_State * const L) {

    // make sure it's the main thread
    if (ThreadManager::shared().isMainThread() == false) {
        vxlog_error("[System:PasskeyCreate] NOT MAIN THREAD");
        return 0;
    }

    LUAUTILS_STACK_SIZE(L)

    // validate arguments
    if (lua_gettop(L) != 5 || lua_utils_getObjectType(L, 1) != ITEM_TYPE_SYSTEM) {
        LUAUTILS_ERROR(L, "System:PasskeyCreate - function should be called with `:`");
    }

    // retrieve arguments' values
    std::string userID = std::string(lua_tostring(L, 2));
    std::string challenge = std::string(lua_tostring(L, 3));
    std::string userName = std::string(lua_tostring(L, 4));
    // 5th argument is the callback
    if (lua_isfunction(L, 5) == false) {
        LUAUTILS_ERROR(L, "System:PasskeyCreate - callback should be a function");
    }

    vxlog_debug("System:PasskeyCreate - userID: %s, challenge: %s, userName: %s", userID.c_str(), challenge.c_str(), userName.c_str());

    // Store the Luau callback function
    // to be able to retrieve it later, asynchronously
    int callbackIndex = 0;
    std::string callbackKey = "";
    bool callbackStoreSuccess = false;
    SAFE_LOCAL_STORE_SET(L, 5, callbackIndex, callbackKey, callbackStoreSuccess);
    if (callbackStoreSuccess == false) {
        // couldn't store function, not supposed to happen
        LUAUTILS_INTERNAL_ERROR(L);
    }

    vx::Game *g = nullptr;
    bool ok = lua_utils_getGameCppWrapperPtr(L, &g);
    if (ok == false) {
        LUAUTILS_INTERNAL_ERROR(L);
    }
    Game_WeakPtr gWeakPtr = g->getWeakPtr();

    // ⚠️ TODO: gaetan: write a function that combines `device::createPasskey()` and `authorizePasskey()`

    // Create a PassKey using the native implementation
    vx::auth::PassKey::createPasskey("blip.game", challenge, userID, userName,
                                     [callbackIndex, callbackKey, gWeakPtr]
                                     (const vx::auth::PassKey::CreateResponse &response, const std::string &error){
        OperationQueue::getMain()->dispatch([callbackIndex, callbackKey, response, error, gWeakPtr](){
            vxlog_debug("vx::device::createPasskey DONE\n - error: %s", error.c_str());
            Game_SharedPtr g = gWeakPtr.lock(); if (g == nullptr) { return; }
            lua_State *L = g->getLuaState();

            // handle error
            if (error.empty() == false) {

                // Retrieve the Luau callback function
                bool success = false;
                SAFE_LOCAL_STORE_GET(L, callbackIndex, callbackKey, success);
                if (success == false) {
                    vxlog_error("System:PasskeyCreate(callback) - couldn't retrieve stored callback");
                }
                SAFE_LOCAL_STORE_REMOVE(L, callbackIndex, callbackKey, success)
                if (success == false) {
                    // couldn't removed stored callback, not supposed to happen
                    vxlog_error("System:PasskeyCreate(callback) - couldn't remove stored callback");
                }
                if (lua_isfunction(L, -1) == false) {
                    vxlog_error("System:PasskeyCreate(callback) - callback is not a function");
                }
                // callback function is on top of the stack (index -1)

                // call the callback with error
                lua_pushstring(L, error.c_str()); // push error string
                const int err = lua_pcall(L, 1, 0, 0);
                if (err != LUA_OK) {
                    vxlog_debug("System:PasskeyCreate(callback) - failed to call callback function");
                    // error string has been pushed onto the lua stack // +1
                    if (lua_utils_isStringStrict(L, -1)) {
                        vxlog_debug("%s", lua_tostring(L, -1));
                    } else {
                        vxlog_debug("lua error is not a string");
                    }
                    lua_pop(L, 1);
                }
                return;
            }

            // upload public key to API server
            HubClient::getInstance().authorizePasskey(response,
                                                      [callbackIndex, callbackKey, gWeakPtr]
                                                      (const bool &success,
                                                       HttpRequest_SharedPtr request){
                OperationQueue::getMain()->dispatch([callbackIndex, callbackKey, success, gWeakPtr](){
                    Game_SharedPtr g = gWeakPtr.lock(); if (g == nullptr) { return; }
                    lua_State *L = g->getLuaState();

                    vxlog_debug("System:PasskeyCreate(callback) - did upload passkey to API server...");
                    vxlog_debug("System:PasskeyCreate(callback) - %s", success ? "OK" : "ERROR");

                    // Retrieve the Luau callback function
                    bool ok = false;
                    SAFE_LOCAL_STORE_GET(L, callbackIndex, callbackKey, ok);
                    if (ok == false) {
                        vxlog_error("System:PasskeyCreate(callback) - couldn't retrieve stored callback");
                    }
                    SAFE_LOCAL_STORE_REMOVE(L, callbackIndex, callbackKey, ok)
                    if (ok == false) {
                        // couldn't removed stored callback, not supposed to happen
                        vxlog_error("System:PasskeyCreate(callback) - couldn't remove stored callback");
                    }
                    if (lua_isfunction(L, -1) == false) {
                        vxlog_error("System:PasskeyCreate(callback) - callback is not a function");
                    }
                    // callback function is on top of the stack (index -1)

                    if (success) {
                        lua_pushnil(L);
                    } else {
                        lua_pushstring(L, "failed to authorize passkey"); // push error string
                    }

                    // call the callback
                    const int err = lua_pcall(L, 1, 0, 0);
                    if (err != LUA_OK) {
                        vxlog_debug("System:PasskeyCreate(callback) - failed to call callback function");
                        // error string has been pushed onto the lua stack // +1
                        if (lua_utils_isStringStrict(L, -1)) {
                            vxlog_debug("%s", lua_tostring(L, -1));
                        } else {
                            vxlog_debug("lua error is not a string");
                        }
                        lua_pop(L, 1);
                    }
                });
            });
        });
    });

    LUAUTILS_STACK_SIZE_ASSERT(L, 0)
    return 0;
}


// Arguments
// - callback(result, error)
static int _system_passkey_login(lua_State * const L) {

    // make sure it's the main thread
    if (ThreadManager::shared().isMainThread() == false) {
        vxlog_error("[System:PasskeyLogin] NOT MAIN THREAD");
        return 0;
    }

    LUAUTILS_STACK_SIZE(L)

    const int ARG_COUNT = 2;
    const int ARG_INDEX_CALLBACK = 2;

    // validate arguments
    if (lua_gettop(L) != ARG_COUNT || lua_utils_getObjectType(L, 1) != ITEM_TYPE_SYSTEM) {
        LUAUTILS_ERROR(L, "System:PasskeyLogin - function should be called with `:`");
    }

    // argument #2 is the callback function
    if (lua_isfunction(L, ARG_INDEX_CALLBACK) == false) {
        LUAUTILS_ERROR(L, "System:PasskeyLogin - callback should be a function");
    }

    // store the Luau callback function to be able to retrieve it later, asynchronously
    int callbackIndex = 0;
    std::string callbackKey = "";
    bool callbackStoreSuccess = false;
    SAFE_LOCAL_STORE_SET(L, ARG_INDEX_CALLBACK, callbackIndex, callbackKey, callbackStoreSuccess);
    if (callbackStoreSuccess == false) {
        // couldn't store function, not supposed to happen
        LUAUTILS_INTERNAL_ERROR(L);
    }

    // get Game weak pointer
    vx::Game *cppGame = nullptr;
    if (lua_utils_getGameCppWrapperPtr(L, &cppGame) == false || cppGame == nullptr) {
        LUAUTILS_STACK_SIZE_ASSERT(L, 0);
        LUAUTILS_INTERNAL_ERROR(L);
    }
    vx::Game_WeakPtr weakGame = cppGame->getWeakPtr();

    vx::HubClient::getInstance().getPasskeyChallenge([weakGame, callbackIndex, callbackKey]
                                                     (const bool& success,
                                                      const std::string& challenge,
                                                      HttpRequest_SharedPtr request){
        OperationQueue::getMain()->dispatch([weakGame, success, callbackIndex, callbackKey, challenge](){

            vx::Game_SharedPtr strongGame = weakGame.lock();
            if (strongGame == nullptr) {
                // Game doesn't exist anymore. Do nothing.
                return;
            }
            lua_State *L = strongGame->getLuaState();

            // handle error
            if (success == false) {
                // Retrieve the Luau callback function
                bool storeOk = false;
                SAFE_LOCAL_STORE_GET(L, callbackIndex, callbackKey, storeOk);
                if (storeOk == false) {
                    vxlog_error("System:PasskeyLogin(callback) - couldn't retrieve stored callback");
                }
                SAFE_LOCAL_STORE_REMOVE(L, callbackIndex, callbackKey, storeOk)
                if (storeOk == false) {
                    // couldn't removed stored callback, not supposed to happen
                    vxlog_error("System:PasskeyLogin(callback) - couldn't remove stored callback");
                }
                if (lua_isfunction(L, -1) == false) {
                    vxlog_error("System:PasskeyLogin(callback) - callback is not a function");
                }
                // callback function is on top of the stack (index -1)

                // call the callback with error
                lua_pushstring(L, "failed to get challenge from API server"); // push error string
                const int err = lua_pcall(L, 1, 0, 0);
                if (err != LUA_OK) {
                    // error string has been pushed onto the lua stack // +1
                    if (lua_utils_isStringStrict(L, -1)) {
                        vxlog_debug("%s", lua_tostring(L, -1));
                    } else {
                        vxlog_debug("lua error is not a string");
                    }
                    lua_pop(L, 1);
                }
                return;
            }

            // We got a challenge for the user.
            // Now, let's start the passkey assertion flow.

            vx::auth::PassKey::loginWithPasskey("blip.game",
                                                challenge,
                                                [weakGame,
                                                 callbackIndex,
                                                 callbackKey]
                                                (const vx::auth::PassKey::LoginResponse_SharedPtr response,
                                                 const std::string &error){
                OperationQueue::getMain()->dispatch([weakGame,
                                                     callbackIndex,
                                                     callbackKey,
                                                     response,
                                                     error](){
                    if (error.empty() == false) {
                        // if (vx::str::contains(error, "error 1001")) { // Error 1001 : user cancelled
                        //     // user cancelled the Passkey login flow
                        // } else {
                        //     // something else
                        // }
                        // do nothing and exit
                        return;
                    }

                    vx::Game_SharedPtr strongGame = weakGame.lock();
                    if (strongGame == nullptr) {
                        // Game doesn't exist anymore. Do nothing.
                        return;
                    }
                    lua_State *L = strongGame->getLuaState();


                    // Retrieve the Luau callback function
                    bool ok = false;
                    SAFE_LOCAL_STORE_GET(L, callbackIndex, callbackKey, ok);
                    if (ok == false) {
                        vxlog_error("System:PasskeyLogin(callback) - couldn't retrieve stored callback");
                    }
                    SAFE_LOCAL_STORE_REMOVE(L, callbackIndex, callbackKey, ok)
                    if (ok == false) {
                        // couldn't removed stored callback, not supposed to happen
                        vxlog_error("System:PasskeyLogin(callback) - couldn't remove stored callback");
                    }
                    if (lua_isfunction(L, -1) == false) {
                        vxlog_error("System:PasskeyLogin(callback) - callback is not a function");
                    }
                    // callback function is on top of the stack (index -1)

                    // push callback arguments
                    lua_pushstring(L, response->getCredentialIDBase64().c_str());
                    lua_pushstring(L, response->getAuthenticatorDataBase64().c_str());
                    lua_pushstring(L, response->getRawClientDataJSONString().c_str());
                    lua_pushstring(L, response->getSignatureBase64().c_str());
                    lua_pushstring(L, response->getUserIDString().c_str());
                    lua_pushstring(L, error.c_str());

                    // call the callback
                    const int err = lua_pcall(L, 6, 0, 0);
                    if (err != LUA_OK) {
                        // error string has been pushed onto the lua stack // +1
                        if (lua_utils_isStringStrict(L, -1)) {
                            vxlog_debug("%s", lua_tostring(L, -1));
                        } else {
                            vxlog_debug("lua error is not a string");
                        }
                        lua_pop(L, 1);
                    }
                });
            });
        });
    });

    LUAUTILS_STACK_SIZE_ASSERT(L, 0)
    return 0;
}

static int _approveUnder13Disclaimer(lua_State * const L) {
    LUAUTILS_STACK_SIZE(L)
    vx::Account::active().approveUnder13Disclaimer();
    LUAUTILS_STACK_SIZE_ASSERT(L, 0)
    return 0;
}
