//
//  VXGame.cpp
//  Cubzh
//
//  Created by Adrien Duermael on 21/05/2021.
//

#include "VXGame.hpp"

// C++
#include <sstream>

#include "VXGameMessage.hpp"
#include "VXNotifications.hpp"
#include "VXAccount.hpp"
#include "VXGameRenderer.hpp"
#include "VXResource.hpp"
#include "VXRootNodeCodeEditor.hpp"
#include "VXNetworkingUtils.hpp"
#include "GameCoordinator.hpp"
#include "URL.hpp"
#include "scripting.hpp"
#include "scripting_server.hpp"
#include "ThreadManager.hpp"
#include "process.hpp"
#include "lua.hpp"
#include "lua_logs.hpp"
#include "lua_utils.hpp"
#include "lua_pointer_event.hpp"
#include "lua_local_event.hpp"
#include "lua_menu.hpp"
#include "lua_sandbox.hpp"
#include "lua_http.hpp"
#include "lua_players.hpp"
#include "lua_require.hpp"
#include "lua_constants.h"
#include "lua_assets.hpp"
#include "strings.hpp"

// xptools
#include "xptools.h"
#include "BZMD5.hpp"
#include "OperationQueue.hpp"
#include "WSConnection.hpp"
#include "encoding_base64.hpp"

#include "config.h"
#include "game.h"
#include "game_config.h"

#include "vxlog.h"
#include "VXHubClient.hpp"
#include "VXLatencyTester.hpp"

#include "lua_object.hpp"
#include "lua_shape.hpp"
#include "lua_audioSource.hpp"

#define CONSOLE_MESSAGES_HISTORY_SIZE 100

// use only for local gameserver
#define HUB_PRIVATE_SERVER_ADDR "api.cu.bzh"
#define HUB_PRIVATE_SERVER_PORT 10083 // TODO: 1443
#define HUB_PRIVATE_SERVER_SECURE true

#define HEARTBEAT_INTERVAL_SEC 10.0 // 10 sec
#define RAMCHECK_INTERVAL_SEC 10.0 // 10 sec

#define NEEDS_RELOAD_RETRY_INTERVAL_SEC 3.0 // 3 sec

using namespace vx;

ConsoleMessage::ConsoleMessage(const uint64_t& time,
                               const Level& level,
                               const std::string& username,
                               const std::string& message) :
time(static_cast<uint32_t>(time)),
level(level),
sender(username),
message(message) {}

Game::Game() :
HubRequestSender(),
userdataStoreForObjects(new UserDataStore()),
userdataStoreForShapes(new UserDataStore()),
userdataStoreForQuads(new UserDataStore()),
userdataStoreForTexts(new UserDataStore()),
userdataStoreForCameras(new UserDataStore()),
userdataStoreForLights(new UserDataStore()),
userdataStoreForBlocks(new UserDataStore()),
userdataStoreForBlockProperties(new UserDataStore()),
userdataStoreForPalettes(new UserDataStore()),
userdataStoreForAudioSources(new UserDataStore()),
userdataStoreForAssetsRequests(new UserDataStore()),
userdataStoreForTimers(new UserDataStore()),
userdataStoreForMeshes(new UserDataStore()),
_state(created),
_scheduledStatesMutex(),
_scheduledStates(),
_connectionState(connectionState_not_required),
_gameRendererState(nullptr),
_mutex(),
_fetchInfo(nullptr),
_isAuthor(false),
_forcePointerShown(false),
_requestedServerAddr(""),
_serverAddr(""),
_data(nullptr),
_dataLength(0),
_dataLoaded(0),
_extraItemsToLoad(),
_serverDevMode(false),
_uploadingThumbnail(false),
_client(nullptr),
_messagesToSend(),
_cgame(nullptr),
_localGameServer(nullptr),
_rootNode(nullptr),
_previousCommandIndex(0),
_emptyString(""),
_weakSelf(),
_worldDataRequest(nullptr),
_nbDepsToLoad(0),
_nbDepsLoaded(0),
_depRequests(),
_depRequestsMutex(),
_nbModulesToLoad(0),
_nbModulesLoaded(0),
_nbModulesFailed(0),
_moduleRequests(),
_moduleRequestsMutex(),
_lookForServerRequest(nullptr),
_luaExecutionLimiter(),
_ramCheckTimerSec(RAMCHECK_INTERVAL_SEC),
_L(nullptr),
_nbPlayers(0),
_localPlayerAdded(false),
_localPlayerID(PLAYER_ID_NOT_ATTRIBUTED),
_needsReload(false),
_needsReloadRetryTimer(0.0)
{}

void Game::_setState(State s) {
    const std::lock_guard<std::mutex> locker(_scheduledStatesMutex);
    _scheduledStates.push(s);
}

bool Game::isInServerMode() {
    switch (_mode) {
        case GameMode_Client:
        case GameMode_ClientOffline:
            return false;
        case GameMode_Server:
            return true;
    }
    return false;
}

bool Game::isInClientMode() {
    return isInServerMode() == false;
}

bool Game::isConnected() {
    return _localPlayerID != PLAYER_ID_NOT_ATTRIBUTED;
}

lua_State* Game::getLuaState() {
    return _L;
}

Game::~Game() {
    vxlog_debug("DELETE GAME (Lua state: %p)", _L);

    if (_client != nullptr) {
        _client->stop(false);
        _client = nullptr; // release GameClient shared_ptr
    }

    if (_localGameServer != nullptr) {
        _localGameServer->stop();
        delete _localGameServer;
    }

    GameMessage_SharedPtr msg = nullptr;
    while (_messagesToSend.pop(msg) == true) {
        msg = nullptr;
    }

    // free the GameRendererState we stored in the C Game
    vx_assert(_gameRendererState != nullptr);
    _gameRendererState = nullptr; // release shared ptr

//    vxlog_debug("GAME CLEANUP REMAINING RESOURCES (1):\n"
//                "\tObjects: %d\n"
//                "\tShapes: %d\n"
//                "\tQuads: %d\n"
//                "\tTexts: %d\n"
//                "\tCameras: %d\n"
//                "\tLights: %d\n"
//                "\tBlocks: %d\n"
//                "\tBlockProperties: %d\n"
//                "\tPalettes: %d\n"
//                "\tAudioSources: %d\n"
//                "\tAssetsRequests: %d\n"
//                "\tTimers: %d\n"
//                "\tMeshes: %d",
//                userdataStoreForObjects->count(),
//                userdataStoreForShapes->count(),
//                userdataStoreForQuads->count(),
//                userdataStoreForTexts->count(),
//                userdataStoreForCameras->count(),
//                userdataStoreForLights->count(),
//                userdataStoreForBlocks->count(),
//                userdataStoreForBlockProperties->count(),
//                userdataStoreForPalettes->count(),
//                userdataStoreForAudioSources->count(),
//                userdataStoreForAssetsRequests->count(),
//                userdataStoreForTimers->count(),
//                userdataStoreForMeshes->count());


    // close Lua state
    if (_L != nullptr) {
        lua_close(_L);
        _L = nullptr;
    }

//    vxlog_debug("GAME CLEANUP REMAINING RESOURCES (2):\n"
//                "\tObjects: %d\n"
//                "\tShapes: %d\n"
//                "\tQuads: %d\n"
//                "\tTexts: %d\n"
//                "\tCameras: %d\n"
//                "\tLights: %d\n"
//                "\tBlocks: %d\n"
//                "\tBlockProperties: %d\n"
//                "\tPalettes: %d\n"
//                "\tAudioSources: %d\n"
//                "\tAssetsRequests: %d\n"
//                "\tTimers: %d\n"
//                "\tMeshes: %d",
//                userdataStoreForObjects->count(),
//                userdataStoreForShapes->count(),
//                userdataStoreForQuads->count(),
//                userdataStoreForTexts->count(),
//                userdataStoreForCameras->count(),
//                userdataStoreForLights->count(),
//                userdataStoreForBlocks->count(),
//                userdataStoreForBlockProperties->count(),
//                userdataStoreForPalettes->count(),
//                userdataStoreForAudioSources->count(),
//                userdataStoreForAssetsRequests->count(),
//                userdataStoreForTimers->count(),
//                userdataStoreForMeshes->count());

    delete userdataStoreForShapes;
    delete userdataStoreForQuads;
    delete userdataStoreForTexts;
    delete userdataStoreForCameras;
    delete userdataStoreForLights;
    delete userdataStoreForBlocks;
    delete userdataStoreForBlockProperties;
    delete userdataStoreForPalettes;
    delete userdataStoreForAudioSources;
    delete userdataStoreForObjects;
    delete userdataStoreForAssetsRequests;
    delete userdataStoreForTimers;
    delete userdataStoreForMeshes;

    game_recycle_managedTransformIDsToRemove(_cgame);
    game_free(_cgame);
    _cgame = nullptr;
}

Game_SharedPtr Game::make(GameMode gameMode,
                          const WorldFetchInfo_SharedPtr& fetchInfo,
                          const GameData_SharedPtr& data,
                          const std::string& serverAddr,
                          const bool gameDevMode,
                          const bool isAuthor,
                          std::vector<std::string> itemsToLoad) {
    Game_SharedPtr g(new Game());
    g->_init(g, gameMode, fetchInfo, data, serverAddr, gameDevMode, isAuthor, itemsToLoad);
    return g;
}

void Game::_init(const Game_SharedPtr& ref,
                 GameMode gameMode,
                 const WorldFetchInfo_SharedPtr& fetchInfo,
                 const GameData_SharedPtr& data,
                 const std::string& serverAddr,
                 const bool gameDevMode,
                 const bool isAuthor,
                 std::vector<std::string> itemsToLoad) {

    this->_weakSelf = ref;

    this->_fetchInfo = fetchInfo;
    this->_mode = gameMode;
    this->_isAuthor = isAuthor;
    this->_requestedServerAddr = serverAddr;
    this->_serverDevMode = gameDevMode;
    this->_data = data;
    this->_extraItemsToLoad = itemsToLoad;

    // init a GameRendererState stored in the Game C++ wrapper
    _gameRendererState = GameRendererState_SharedPtr(new GameRendererState());

    // create local game server if needed
    if (_mode == GameMode_ClientOffline && _data != nullptr && _data->getMaxPlayers() > 1) {
        // NOTES:
        // - client hosted game servers for multiplayer not really rested yet
        // - flow should be reviewed
        vx_assert(false); // failing for now, not used anyway
        _localGameServer = GameServer::newLocalServerWithScript(data,
                                                                HUB_PRIVATE_SERVER_ADDR,
                                                                HUB_PRIVATE_SERVER_PORT,
                                                                HUB_PRIVATE_SERVER_SECURE,
                                                                1); // 1 player
        _localGameServer->start();
    }

    // IF NOT A SERVER
    if (_mode != GameMode_Server) {
        this->_rootNode = Node::make();
        this->_rootNode->setMaxTouches(2);
        this->_rootNode->setPassthrough(false);
        this->_rootNode->setColor(MenuConfig::shared().colorTransparent);
    }

    // create CGame (may be temporary)
    _cgame = game_new(this);
    _setCGameFunctionPointers();
}

void Game::tickUntilLoaded() {
    StateChange gameStateChange = StateChange::none;
    while (gameStateChange != running && gameStateChange != failed) {
        gameStateChange = tick(0.0);
    }
}

Game::StateChange Game::tick(const double dt) {
    // Game::tick can be called in a different thread for games being loaded
    // vx_assert(vx::ThreadManager::shared().isMainThread());

    State newState = state_none;
    StateChange stateChange = none;

    TICK_DELTA_MS_T dtMS = static_cast<TICK_DELTA_MS_T>(dt * 1000);
    TICK_DELTA_MS_T limitedDtMS = dtMS > MAX_TICK_DELTA_MS ? MAX_TICK_DELTA_MS : dtMS;
    TICK_DELTA_SEC_T limitedDtS = limitedDtMS / 1000.0;

    {
        const std::lock_guard<std::mutex> locker(_scheduledStatesMutex);
        if (_scheduledStates.empty() == false) {
            newState = _scheduledStates.front();
            _scheduledStates.pop();
        }
    }

    if (newState != state_none) {
        _state = newState;
        switch (_state) {
            case downloadingWorldData:
                stateChange = StateChange::loading;
                vxlog_info("📍 downloading world data (%s)", this->getID().c_str());
                break;
            case downloadedWorldData:
                stateChange = StateChange::minorChange;
                vxlog_info("📍 downloaded world data (%s)", this->getID().c_str());
                break;
            case loadingWorldData:
                stateChange = StateChange::minorChange;
                vxlog_info("📍 loading world data (%s)", this->getID().c_str());
                break;
            case failedToLoadWorldData:
                // TODO: review error message
                stateChange = StateChange::failed;
                vxlog_info("❌ failed to load world data (%s)", this->getID().c_str());
                break;
            case loadedWorldData:
                stateChange = StateChange::minorChange;
                vxlog_info("📍 loaded world data (%s)", this->getID().c_str());
                break;
            case loadingModules:
                stateChange = StateChange::minorChange;
                vxlog_info("📍 loading modules (%s)", this->getID().c_str());
                break;
            case loadedModules:
                stateChange = StateChange::minorChange;
                vxlog_info("📍 loaded modules (%s)", this->getID().c_str());
                break;
            case startDownloadingOtherDependencies:
                stateChange = StateChange::minorChange;
                vxlog_info("📍 start downloading other dependencies (%s)", this->getID().c_str());
                break;
            case downloadingOtherDependencies:
                stateChange = StateChange::minorChange;
                vxlog_info("📍 downloading other dependencies (%s)", this->getID().c_str());
                break;
            case downloadedOtherDependencies:
                stateChange = StateChange::minorChange;
                vxlog_info("📍 downloaded other dependencies (%s)", this->getID().c_str());
                break;
            case starting:
                stateChange = StateChange::minorChange;
                vxlog_info("📍 starting (%s)", this->getID().c_str());
                break;
            case started:
                stateChange = StateChange::running;
                vxlog_info("📍 started (%s)", this->getID().c_str());
                break;
            default:
                break;
        }
    }

    switch (_state) {
        case created:
        {
            if (_data != nullptr) {
                _setState(downloadedWorldData);
            } else {
                // START DOWNLOADING WORLD DATA
                _setState(downloadingWorldData);

                Game_WeakPtr sender = _weakSelf;

                vx::HubClient& client = vx::HubClient::getInstance();

                if (_fetchInfo->getWorldID().empty() == false) {

                    if (isInServerMode()) {
                        _worldDataRequest = client.privateAPIGetWorldScriptAndMaxPlayers(sender,
                                                                                         _fetchInfo->getWorldID(),
                                                                                         [sender](const bool &success,
                                                                                                  const HTTPStatus &status,
                                                                                                  const std::string &script,
                                                                                                  const int maxPlayers){
                            Game_SharedPtr game = sender.lock();
                            if (game == nullptr) {
                                return;
                            }

                            if (success == false) {
                                game->_setState(failedToLoadWorldData);
                                return;
                            }

                            game->_data = GameData::make(script, "", "", maxPlayers);
                            game->_setState(downloadedWorldData);
                        });
                    } else {
                        _worldDataRequest = client.getWorldData(sender, _fetchInfo->getWorldID(),
                                                                [sender](const bool &success,
                                                                         const HTTPStatus &status,
                                                                         const std::string &script,
                                                                         const std::string &mapBase64,
                                                                         const std::string &map3zh,
                                                                         const int maxPlayers,
                                                                         std::unordered_map<std::string, std::string> env) {
                            Game_SharedPtr game = sender.lock();
                            if (game == nullptr) {
                                return;
                            }

                            if (success == false) {
                                game->_setState(failedToLoadWorldData);
                                return;
                            }

                            game->_data = GameData::make(script, mapBase64, map3zh, maxPlayers);
                            game->mergeEnvironment(env);
                            game->_setState(downloadedWorldData);
                        },
                        [sender](int contentLength, int loadedContent) {

                            Game_SharedPtr game = sender.lock();
                            if (game == nullptr) {
                                return;
                            }

                            game->setDataLength(contentLength);
                            game->setDataLoaded(loadedContent);

                            // DEBUG
                            //double loaded = static_cast<double>(loadedContent);
                            //if (contentLength == 0) {
                            //    vxlog_debug("NO CONTENT LENGTH!");
                            //    if (loaded >= 1048576) { // > 1MB, display in MB
                            //        vxlog_debug("LOADED %.2fMB", loaded / 1048576.0);
                            //    } else { // display in KB
                            //        vxlog_debug("LOADED %.2fKB", loaded / 1024.0);
                            //    }
                            //    return;
                            //}
                            //double progression = (loaded / static_cast<double>(contentLength)) * 100.0;
                            //if (loaded >= 1048576) { // display in MB
                            //    vxlog_debug("LOADED %.2f%% - (%.2fMB)", progression, loaded / 1048576.0);
                            //} else { // display in KB
                            //    vxlog_debug("LOADED %.2f%% - (%.2fKB)", progression, loaded / 1024.0);
                            //}

                            vx::NotificationCenter::shared().notify(vx::NotificationName::loadingGameStateChanged);
                        });
                    }
                } else if (_fetchInfo->getGithubRepo().empty() == false) {

                    _worldDataRequest = client.getWorldDataFromGithub(sender,
                                                                      _fetchInfo->getGithubRepo(),
                                                                      _fetchInfo->getGithubPath(),
                                                                      _fetchInfo->getGithubRef(),
                                                                      [sender](const bool &success,
                                                                               const HTTPStatus &status,
                                                                               const std::string &script,
                                                                               const std::string &mapBase64,
                                                                               const std::string &map3zh,
                                                                               const int maxPlayers,
                                                                               std::unordered_map<std::string, std::string> env) {

                        Game_SharedPtr game = sender.lock();
                        if (game == nullptr) {
                            return;
                        }

                        if (success == false) {
                            game->_setState(failedToLoadWorldData);
                            return;
                        }

                        game->_data = GameData::make(script, mapBase64, map3zh, maxPlayers);
                        game->mergeEnvironment(env);
                        game->_setState(downloadedWorldData);
                    });

                } else if (_fetchInfo->getHuggingFaceSpace().empty() == false) {

                    _worldDataRequest = client.getWorldDataFromHuggingFaceSpace(sender,
                                                                                _fetchInfo->getHuggingFaceSpace(),
                                                                                _fetchInfo->getHuggingFaceRef(),
                                                                                [sender](const bool &success,
                                                                                         const HTTPStatus &status,
                                                                                         const std::string &script,
                                                                                         const std::string &mapBase64,
                                                                                         const std::string &map3zh,
                                                                                         const int maxPlayers,
                                                                                         std::unordered_map<std::string, std::string> env) {

                        Game_SharedPtr game = sender.lock();
                        if (game == nullptr) {
                            return;
                        }

                        if (success == false) {
                            game->_setState(failedToLoadWorldData);
                            return;
                        }

                        game->_data = GameData::make(script, mapBase64, map3zh, maxPlayers);
                        game->mergeEnvironment(env);
                        game->_setState(downloadedWorldData);
                    });
                }
            }
            break;
        }
        case downloadingWorldData:
        {
            // TODO: track progression (content-length)
            break;
        }
        case downloadedWorldData:
        {
            // this->_data has been populated
            if (_mode == GameMode_Client && _isAuthor == true && _serverDevMode == true) {
                vxlog_debug("⭐️ GAME DATA: script length: %d", this->_data->getScript().length());
                _editorConn = WSConnection::make("ws", "127.0.0.1", 3429, "");
                _editorConn->setDelegate(_weakSelf);
                _editorConn->connect();
            }

            _setState(loadingWorldData);
            break;
        }
        case loadingWorldData:
        {
            loadWorldData();
            break;
        }
        case loadedWorldData:
        {
            _setState(startLoadingModules);
            break;
        }
        case startLoadingModules:
        {
            _setState(loadingModules);

            lua_State *L = getLuaState();
            lua_pushvalue(L, LUA_GLOBALSINDEX);
            // -1: globals

            lua_getfield(L, -1, "Modules");
            vx_assert(lua_istable(L, -1));
            vx_assert(lua_utils_getObjectType(L, -1) == ITEM_TYPE_G_MODULES);
            // -1: Modules
            // -2: env

            LUA_GET_METAFIELD(L, -1, "__index");
            // -1: Modules's index
            // -2: Modules
            // -3: env

            _nbModulesToLoad = 0;
            _nbModulesLoaded = 0;
            _nbModulesFailed = 0;

            // loop once to count
            lua_pushnil(L);
            while (lua_next(L, -2) != 0) {
                // key at -2, value at -1
                vxlog_info("MODULE: %s", lua_tostring(L, -1));
                ++_nbModulesToLoad;
                lua_pop(L, 1); // pop value, keep key
            }

            if (_nbModulesToLoad == 0) {
                _setState(loadedModules);
            } else {
                Game_WeakPtr gameWeakPtr = _weakSelf;

                lua_pushnil(L);
                std::string moduleName;
                while (lua_next(L, -2) != 0) {
                    // key at -2, value at -1
                    moduleName = std::string(lua_tostring(L, -1));

                    const bool isPublicModule = vx::str::hasPrefix(moduleName, "github.com");

                    if (isPublicModule == false) { // local module
                        bool done = false;
                        {
                            const std::lock_guard<std::mutex> locker(_moduleRequestsMutex);
                            ++_nbModulesLoaded;

                            done = (_nbModulesLoaded + _nbModulesFailed) == _nbModulesToLoad;
                            vxlog_info("📍 MODULE LOADED %d / %d (%s)",
                                       _nbModulesLoaded + _nbModulesFailed,
                                       _nbModulesToLoad,
                                       this->getID().c_str()
                                       );
                        }
                        if (done) {
                            _setState(loadedModules);
                        } else {
                            vx::NotificationCenter::shared().notify(vx::NotificationName::loadingGameStateChanged);
                        }
                    } else {

                        std::string _url = std::string(Config::apiHost());
                        _url += "/module?src=" + moduleName;

                        std::string ref = ".latest";

                        std::vector<std::string> elements = vx::str::splitString(moduleName, ":");
                        if (elements.size() >= 2) {
                            ref = elements[elements.size() - 1];
                            moduleName = vx::str::trimSuffix(moduleName, ":" + ref);
                        }

                        const std::unordered_map<std::string, std::string> headers = {
                            {HTTP_HEADER_CUBZH_CLIENT_VERSION_V2, vx::device::appVersionCached()},
                            {HTTP_HEADER_CUBZH_CLIENT_BUILDNUMBER_V2, vx::device::appBuildNumberCached()},
                        };

                        const vx::URL url = vx::URL::make(_url);
                        // TODO: push req in pool / cancel on Game destruction
                        HttpRequest_SharedPtr req = HttpRequest::make("GET", url.host(), url.port(), url.path(), url.queryParams(), url.scheme() == "https");
                        req->setHeaders(headers);
                        req->setCallback([gameWeakPtr, _url, moduleName, ref](HttpRequest_SharedPtr req){

                            Game_SharedPtr game = gameWeakPtr.lock();
                            if (game == nullptr) {
                                return;
                            }

                            uint16_t statusCode = req->getResponse().getStatusCode();
                            vxlog_info("%s -> %d", _url.c_str(), statusCode);

                            std::string responseBytes;
                            const bool ok = req->getResponse().readAllBytes(responseBytes);
                            if (ok == false) {
                                return;
                            }

                            bool done = false;
                            {
                                const std::lock_guard<std::mutex> locker(game->_moduleRequestsMutex);
                                if (statusCode == 200) {
                                    ++game->_nbModulesLoaded;

                                    FILE *fp = vx::fs::openStorageFile("modules/" + moduleName + "/.ref/" + ref + "/script.lua", "wb");

                                    if (fp == nullptr) {
                                        vxlog_error("MODULE: failed to write file");
                                    } else {
                                        fwrite(responseBytes.c_str(), 1, responseBytes.size(), fp);
                                        fclose(fp);
                                    }

                                    fp = vx::fs::openStorageFile("modules/" + moduleName + "/.ref/" + ref + "/.bzhash", "wb");
                                    if (fp == nullptr) {
                                        vxlog_error("MODULE: failed to write hash");
                                    } else {
                                        // SALT + "::" +  FULL MODULE NAME + "::" + SCRIPT
                                        std::string hash = md5(std::string("nanskip") + "::" + moduleName + ":" + ref + "::" + responseBytes);
                                        fwrite(hash.c_str(), 1, hash.size(), fp);
                                        fclose(fp);
                                    }

                                } else {
                                    // retry?
                                    ++game->_nbModulesFailed;
                                }
                                done = (game->_nbModulesLoaded + game->_nbModulesFailed) == game->_nbModulesToLoad;
                                vxlog_info("📍 MODULE LOADED %d / %d", game->_nbModulesLoaded + game->_nbModulesFailed, game->_nbModulesToLoad);
                            }

                            if (done) {
                                game->_setState(loadedModules);
                            } else {
                                vx::NotificationCenter::shared().notify(vx::NotificationName::loadingGameStateChanged);
                            }
                        });
                        req->sendAsync();
                    }

                    lua_pop(L, 1); // pop value, keep key
                }
            }

            lua_pop(L, 3);
            break;
        }
        case loadingModules:
        {
            break;
        }
        case loadedModules:
        {
            _setState(startDownloadingOtherDependencies);
            break;
        }
        case startDownloadingOtherDependencies:
        {
            _setState(downloadingOtherDependencies);

            // server does not download other dependencies (for now)
            if (isInServerMode()) {
                _setState(downloadedOtherDependencies);
                break;
            }

            // LOAD BASE64 MAP IF ANY
            if (_data->getMapBase64().empty() == false) {
                std::string outputStr;
                const std::string err = vx::encoding::base64::decode(_data->getMapBase64(), outputStr);
                if (err.empty()) {
                    const char *output = outputStr.c_str();
                    output += 1; // skip version (one byte)
                    const uint8_t chunkID = *reinterpret_cast<const uint8_t*>(output);
                    if (chunkID == 0) {
                        output += 1;
                        // do not consider config's map if there's a map from the world editor
                        GameConfig *config = game_getConfig(_cgame);
                        if (config != nullptr) {
                            game_config_set_map(config, nullptr);
                        }
                        // map chunk is always the first chunk (when it exists)
                        double scale;
                        memcpy(&scale, &output, sizeof(double));
                        uint32_t mapNameLen;
                        memcpy(&mapNameLen, &output[sizeof(double)], sizeof(uint32_t));
                        char mapName[1000];
                        memset(mapName, 0, 1000);
                        if (mapNameLen < 1000) {
                            memcpy(mapName, &output[sizeof(double) + sizeof(uint32_t)], mapNameLen);
                            game_config_set_map(config, mapName);
                        }
                    }
                }
            }

            _nbDepsToLoad = 0;
            _nbDepsLoaded = 0;

            GameConfig *config = game_getConfig(getCGame());
            if (config != nullptr) {
                vx::HubClient& client = vx::HubClient::getInstance();
                Game_WeakPtr sender = _weakSelf;
                Item *item;
                std::string emptyItemID = "";

                int nbConfigItems = game_config_items_len(config);
                int nbItemsToLoad = nbConfigItems;

                Item *map = game_config_get_map(config);
                if (map != nullptr) {
                    ++nbItemsToLoad;
                }

                // HACK: do not load anything when loading home world,
                // all assets are supposed to be in the bundle
                if (getID() == WORLD_HOME_ID) {
                    nbConfigItems = 0;
                    nbItemsToLoad = 0;
                }

                if (nbItemsToLoad == 0) {
                    _setState(downloadedOtherDependencies);
                } else {
                    {
                        const std::lock_guard<std::mutex> locker(_depRequestsMutex);
                        _nbDepsToLoad = static_cast<size_t>(nbItemsToLoad);
                    }

                    if (_mode == GameMode_ClientOffline) {
                        _nbDepsLoaded = _nbDepsToLoad;
                        _setState(downloadedOtherDependencies);
                    } else {
                        for (int i = 0; i < nbItemsToLoad; ++i) {

                            if (i < nbConfigItems) {
                                item = game_config_items_get_at(config, i);
                            } else {
                                vx_assert(map != nullptr);
                                item = map;
                            }

                            const std::string repo(item->repo);
                            const std::string name(item->name);

                            HttpRequest_SharedPtr req = client.getItemModel(sender,
                                                                            repo,
                                                                            name,
                                                                            false,
                                                                            [sender]
                                                                            (const bool &success,
                                                                             HttpRequest_SharedPtr req,
                                                                             const std::string& repo,
                                                                             const std::string& name,
                                                                             const std::string& content,
                                                                             const bool& etagValid) {

                                Game_SharedPtr game = sender.lock();
                                if (game == nullptr) {
                                    return;
                                }


                                if (success == true) {
                                    // Requested the item .3zh file successfully.
                                    // If the ETag is valid, we did not get the file bytes.
                                    if (etagValid) {
                                        // no need to update the .3zh file in the cache,
                                        // but we need to update the cache info (max-age)
                                        // If ETag arg is `nullptr`, ETag value is not updated in CacheInfo.
#ifdef CLIENT_ENV
                                        if (updateItemCacheInfo(repo, name, nullptr, vx::device::timestampApple())) {
                                            // UPDATED
                                        } else {
                                            // ERROR
                                        }
#endif
                                    } else {
#ifdef CLIENT_ENV
                                        // update the cache with .3zh file
                                        if (saveItemInCache(repo, name, content)) {
                                            // SAVED
                                        } else {
                                            // ERROR
                                        }
#endif
                                    }
                                } else {
                                    vxlog_error("failed to download item: %s.%s", repo.c_str(), name.c_str());
                                    // FAILED TO DOWNLOAD
                                }

                                // TODO: handle errors

                                bool done = false;
                                {
                                    const std::lock_guard<std::mutex> locker(game->_depRequestsMutex);
                                    ++game->_nbDepsLoaded;
                                    done = game->_nbDepsLoaded == game->_nbDepsToLoad;
                                    vxlog_info("📍 ITEM LOADED %d / %d", game->_nbDepsLoaded, game->_nbDepsToLoad);
                                }

                                if (done) {
                                    game->_setState(downloadedOtherDependencies);
                                }
                            });

                            _depRequests.push_back(req);
                        }
                    }
                }
            }
            break;
        }
        case downloadingOtherDependencies:
        {
            break;
        }
        case downloadedOtherDependencies:
        {
            _setState(starting);

            if (isInClientMode()) {
                game_set_map_from_game_config(_cgame, true /* bake light */);
                _gameRendererState->setDefaultsFromMap(game_get_map_shape(_cgame));

                lua_sandbox_client_setup_post_load(_L, this);
            } else { // server
                lua_sandbox_server_setup_post_load(_L, this);

                // TODO: call at ths end
                // scriptingServer_onStart(this);
            }

            // LOAD MODULES
            lua_State *L = getLuaState();
            LUAUTILS_STACK_SIZE(L)
            lua_pushvalue(L, LUA_GLOBALSINDEX);
            // -1: globals

            lua_getfield(L, -1, "Modules");
            vx_assert(lua_istable(L, -1));
            vx_assert(lua_utils_getObjectType(L, -1) == ITEM_TYPE_G_MODULES);
            // -1: Modules
            // -2: globals

            LUA_GET_METAFIELD(L, -1, "__index");
            // -1: Modules's index
            // -2: Modules
            // -3: globals

            lua_remove(L, -2);
            // -1: Modules's index
            // -2: globals

            // loop once to count
            lua_pushnil(L);
            while (lua_next(L, -2) != 0) {
                // key at -2, value at -1
                vxlog_info("MODULE LOAD: %s", lua_tostring(L, -1));
                if (lua_isstring(L, -1)) {
                    if (lua_require(L, lua_tostring(L, -1))) {
                        // -1: module
                        // -2: path
                        // -3: key
                        // -4: modules's index
                        // -5: globals
                        if (lua_isstring(L, -3)) { // set global variable in user's env
                            lua_pushvalue(L, -3);
                            lua_pushvalue(L, -2);
                            lua_rawset(L, -7);
                        }
                        lua_pop(L, 1); // pop required module
                    }
                }

                // LUA_REQUIRE

                lua_pop(L, 1); // pop value, keep key
            }

            // -1: Modules's index
            // -2: globals
            lua_pop(L, 2);
            LUAUTILS_STACK_SIZE_ASSERT(L, 0)
            // LOAD MODULES END

            if (isInClientMode()) {
                addOrUpdateLocalPlayer(nullptr, nullptr, nullptr);

                GameRenderer::setTemporaryCameraProj(this);

                scripting_loadWorld_onStart(this);
            } else {
                scriptingServer_onStart(this);
            }

            break;
        }
        case starting:
        {
            break;
        }
        case started:
        {
            game_tick(_cgame, limitedDtS);
            scripting_tick(this, _cgame, limitedDtS);
            game_tick_end(_cgame, limitedDtS);

            if (isInClientMode()) {

                if (_needsReload) {
                    _needsReloadRetryTimer -= dt;
                    if (_needsReloadRetryTimer <= 0.0) {
                        sendReloadWorldMessage();
                        _needsReloadRetryTimer = NEEDS_RELOAD_RETRY_INTERVAL_SEC;
                    }
                }

                // mem usage check
                _ramCheckTimerSec -= dt;
                if (_ramCheckTimerSec <= 0.0) {

                    const unsigned int memLimit = vx::Process::getMemoryUsageLimitMB();

                    if (_luaExecutionLimiter.isAllFuncsDisabled() == false && vx::Process::getUsedMemoryMB() >= memLimit) {
                        vxlog_info("MemoryUsage is above RAM threshold, calling collectgarbage");

                        luau_dostring(_L, "collectgarbage()", "collectgarbage()");
                        // If used memory is still above the threshold, block all functions
                        if (vx::Process::getUsedMemoryMB() >= memLimit) {
                            vxlog_info("Memory usage is above threshold. Pausing all functions.");
                            _luaExecutionLimiter.setBlockAllFuncs(true);
                            std::ostringstream oss;
                            oss << "Memory usage is above threshold (" << vx::Process::getMemoryUsageLimitMB() << "MB). Pausing all functions.";
                            lua_log_error_with_game(this, oss.str());
                        }
                    }
                    _ramCheckTimerSec = RAMCHECK_INTERVAL_SEC;
                }
            }
            break;
        }
        default:
            break;
    }

    return stateChange;
}

CGame *Game::getCGame() {
    vx_assert(_cgame != nullptr);
    return _cgame;
}

GameData_SharedPtr Game::getData() {
    return _data;
}

void Game::onStart() {
    // NOTE: "OnStart" event is the same on client and server
    LOCAL_EVENT_SEND_PUSH_WITH_SYSTEM(_L, LOCAL_EVENT_NAME_OnStart)
    LOCAL_EVENT_SEND_CALL_WITH_SYSTEM(_L, 0)

    std::unordered_map<std::string, std::string> properties;
    if (_fetchInfo != nullptr) {
        properties["world-id"] = _fetchInfo->getWorldID();
        if (_fetchInfo->getGithubRepo().empty() == false) {
            properties["github-repo"] = _fetchInfo->getGithubRepo();
            properties["github-path"] = _fetchInfo->getGithubPath();
            properties["github-ref"] = _fetchInfo->getGithubRef();
        }
        if (_fetchInfo->getHuggingFaceSpace().empty() == false) {
            properties["hf-space"] = _fetchInfo->getHuggingFaceSpace();
            properties["hf-ref"] = _fetchInfo->getHuggingFaceRef();
        }
    }

    vx::tracking::TrackingClient::shared().trackEvent("World starts", properties);
    _setState(started);
}

void Game::connect() {
    if (vx::Account::active().isAuthenticated() == false) {
        vxlog_error("can't connect before authentication");
        return;
    }

    LOCAL_EVENT_SEND_PUSH_WITH_SYSTEM(_L, LOCAL_EVENT_NAME_ServerConnectionStart)
    LOCAL_EVENT_SEND_CALL_WITH_SYSTEM(_L, 0)

    _serverAddr = _requestedServerAddr;

    // look for server or connect world is for more than 1 player
    if (_data->getMaxPlayers() > 1) {
        if (_localGameServer == nullptr && _serverAddr.empty() &&
            Config::debugGameServer().empty()) {
            _lookForServer();
        } else {
            _connect();
        }
    }
}

void Game::addOrUpdateLocalPlayer(const uint8_t *playerID,
                                  const std::string *username,
                                  const std::string *userid) {

    if (_localPlayerAdded == false) {
        if (playerID != nullptr) {
            _localPlayerID = *playerID;
        }
        std::string uname = username != nullptr ? *username : vx::Account::active().username();
        std::string uid = userid != nullptr ? *userid : vx::Account::active().userID();
        addPlayer(_localPlayerID, uname, uid, true);
        _localPlayerAdded = true;
        return;
    }

    // update
    if (playerID != nullptr || username != nullptr || userid != nullptr) {
        lua_g_players_updatePlayer(_L, _localPlayerID, playerID, username, userid);
        if (playerID != nullptr) {
            _localPlayerID = *playerID;
        }
    }
}

void Game::addPlayer(const uint8_t playerID,
                     const std::string& username,
                     const std::string& userid,
                     const bool isLocalPlayer) {

    vx_assert(_L != nullptr);

    if (_nbPlayers >= GAME_PLAYER_COUNT_MAX) {
        vxlog_error("Reached max amount of players (%d).", _nbPlayers);
        return;
    }

    std::string uName = username;
    if (uName.empty()) {
        uName = PLAYER_DEFAULT_USERNAME;
    }

    // update the players count
    ++_nbPlayers;

    // add Lua Player to Players
    if (scripting_addPlayer(this, playerID, uName, userid, isLocalPlayer) == false) {
        vxlog_error("error when adding player");
        return;
    }

    // Client/Server.OnPlayerJoin called separately
}

void Game::removePlayer(const uint8_t playerID) {

    if (game_isPlayerIDValid(playerID) == false) {
        vxlog_error("[%s:%d] playerID is not valid", __func__, __LINE__);
        return;
    }

    LUAUTILS_STACK_SIZE(_L)

    LOCAL_EVENT_SEND_PUSH_WITH_SYSTEM(_L, LOCAL_EVENT_NAME_OnPlayerLeave)
    lua_g_players_pushPlayerTable(_L, playerID);

    // LOCAL_EVENT_SEND_CALL_WITH_SYSTEM(_L, 1) // not using macro currently to debug a crash
    if (lua_pcall(_L, 1 + 3, 0, 0) != LUA_OK) {
        if (lua_utils_isStringStrict(_L, -1)) {
            lua_log_error_CStr(_L, lua_tostring(_L, -1));
        } else {
            lua_log_error(_L, "LocalEvent.Send callback error");
        }
        lua_pop(_L, 1);
    }


    if (playerID != _localPlayerID) {
        --_nbPlayers;

        // removes the player table from the "Players" array
        if (lua_g_players_removePlayerTable(_L, playerID) == false) {
            vxlog_error("Failed to remove player from Players");
            return;
        }

        if (isInClientMode()) {
            // removes the player table from the "OtherPlayers" array
            if (lua_g_otherPlayers_removePlayerTable(_L, playerID) == false) {
                vxlog_error("Failed to remove player from OtherPlayers");
                return;
            }
        }
    }

    LUAUTILS_STACK_SIZE_ASSERT(_L, 0)
}

void Game::removeAllPlayers() {
    std::vector<uint8_t> ids = lua_g_players_getPlayerIDs(_L);
    for (uint8_t id : ids) {
        removePlayer(id);
    }
}

bool Game::serverIsInDevMode() {
    return this->_serverDevMode;
}

bool Game::localUserIsContributor() {
    // TODO: implement
    return true;
}

Node_SharedPtr Game::getRootNode() {
    return this->_rootNode;
}

void Game::setGlobalCallback(const std::string &name, int (*callback)(lua_State *)) {
    vx_assert(_L != nullptr);
    LUAUTILS_STACK_SIZE(_L)
    lua_pushcclosure(_L, callback, "", 0);
    lua_setglobal(_L, name.c_str());
    LUAUTILS_STACK_SIZE_ASSERT(_L, 0)
}

void Game::enableHTTP() {
    vx_assert(_L != nullptr);
    LUAUTILS_STACK_SIZE(_L)

    lua_pushvalue(_L, LUA_GLOBALSINDEX);
    LUAUTILS_STACK_SIZE_ASSERT(_L, 1)

    if (lua_getmetatable(_L, -1) != 1) {
        vxlog_error("Couldn't get globals table metatable");
        LUAUTILS_INTERNAL_ERROR(_L)
    }
    LUAUTILS_STACK_SIZE_ASSERT(_L, 2)

    // HTTP global
    lua_pushstring(_L, LUA_G_HTTP);
    LUAUTILS_STACK_SIZE_ASSERT(_L, 3)
    lua_g_http_create_and_push(_L);
    LUAUTILS_STACK_SIZE_ASSERT(_L, 4)
    lua_rawset(_L, -3);
    LUAUTILS_STACK_SIZE_ASSERT(_L, 2)
    lua_pop(_L, 2);

    LUAUTILS_STACK_SIZE_ASSERT(_L, 0)
}

void Game::doString(const std::string& str) {
    scripting_do_string_called_from_game(this, str.c_str());
}

void Game::setFetchInfo(const WorldFetchInfo_SharedPtr& fetchInfo) {
    this->_fetchInfo = fetchInfo;
}

const std::string& Game::getID() {
    return this->_fetchInfo->getWorldID();
}

const WorldFetchInfo_SharedPtr& Game::getFetchInfo() {
    return this->_fetchInfo;
}

bool Game::localUserIsAuthor() {
    return this->_isAuthor;
}

void Game::forceShowPointer(bool b) {
    _forcePointerShown = b;
}

bool Game::pointerForceShown() {
    return _forcePointerShown;
}

Game_SharedPtr Game::getSharedPtr() {
    Game_SharedPtr g = this->_weakSelf.lock();
    vx_assert(g != nullptr);
    return g;
}

Game_WeakPtr Game::getWeakPtr() {
    return this->_weakSelf;
}

bool Game::startTakingAndUploadingThumbnail() {
    const std::lock_guard<std::mutex> locker(_mutex);
    if (this->_uploadingThumbnail == true) {
        return false;
    }
    this->_uploadingThumbnail = true;
    return true;
}

void Game::uploadThumbnail(void* data, size_t dataSize) {

    std::string worldID = _fetchInfo->getWorldID();
    if (worldID.empty()) {
        vxlog_error("can't set thumbnail if world has no ID");
        return;
    }

    vx::HubClient& client = vx::HubClient::getInstance();
    client.setWorldThumbnail(this->_weakSelf, worldID,
                             data, dataSize, [](const bool& success,
                                                HubClientRequest_SharedPtr request) {

        vx::HubRequestSender_SharedPtr sharedPtr = request->sender().lock();
        if (sharedPtr == nullptr) {
            // sender is gone, no need to do anything
            return;
        }
        vx::Game *game = static_cast<vx::Game*>(sharedPtr.get());

        {
            // set _uploadingThumbnail back to false, for
            // startTakingAndUploadingThumbnail to return true.
            const std::lock_guard<std::mutex> locker(game->_mutex);
            game->_uploadingThumbnail = false;
        }

        Game_WeakPtr wp = game->getWeakPtr();
        vx::OperationQueue::getMain()->dispatch([wp, success]() {
            Game_SharedPtr g = wp.lock();
            if (g == nullptr) { return; }

            lua_State *L = g->getLuaState();
            vx_assert(L != nullptr);
            lua_log_warning(L, success ? "thumbnail uploaded!" : "thumbnail could not be uploaded.");
        });
    });
}

void Game::resetPreviousCommandIndex() {
    _previousCommandIndex = 0;
}

const std::string& Game::getPreviousCommand() {
    _previousCommandIndex += 1;
    size_t nbCommands = GameCoordinator::getInstance()->getNbPreviousCommands();
    // NOTE: getPreviousCommand uses indexes starting from 1 (not 0)
    if (_previousCommandIndex > nbCommands) {
        _previousCommandIndex = nbCommands;
    }

    if (_previousCommandIndex == 0) {
        return _emptyString;
    }

    return GameCoordinator::getInstance()->getPreviousCommand(_previousCommandIndex);
}

const std::string& Game::getNextCommand() {
    if (_previousCommandIndex > 0) {
        _previousCommandIndex -= 1;
    }

    if (_previousCommandIndex == 0) {
        return _emptyString;
    }

    vx_assert(_previousCommandIndex <= GameCoordinator::getInstance()->getNbPreviousCommands());

    return GameCoordinator::getInstance()->getPreviousCommand(_previousCommandIndex);
}

void Game::sendCPPMenuStateChanged() {
    LOCAL_EVENT_SEND_PUSH_WITH_SYSTEM(_L, LOCAL_EVENT_NAME_CppMenuStateChanged)
    LOCAL_EVENT_SEND_CALL_WITH_SYSTEM(_L, 0)
}

void Game::sendKeyboardInput(uint32_t charCode, Input input, uint8_t modifiers, KeyState state) {
    LOCAL_EVENT_SEND_PUSH_WITH_SYSTEM(_L, LOCAL_EVENT_NAME_KeyboardInput)
    if (charCode != 0) {
        char buf[5];
        input_char_code_to_string(buf, charCode);
        lua_pushstring(_L, buf); // char
    } else {
        lua_pushstring(_L, ""); // char
    }
    lua_pushinteger(_L, input); // key code
    lua_pushinteger(_L, modifiers); // modifiers mask
    lua_pushboolean(_L, state == KeyState::KeyStateDown ? true : false);
    LOCAL_EVENT_SEND_CALL_WITH_SYSTEM(_L, 4)
}

void Game::sendTextInputUpdate(const std::string& str, size_t cursorStart, size_t cursorEnd) {
    LOCAL_EVENT_SEND_PUSH_WITH_SYSTEM(_L, LOCAL_EVENT_NAME_ActiveTextInputUpdate)
    lua_pushstring(_L, str.c_str());
    lua_pushinteger(_L, static_cast<int>(cursorStart + 1)); // C -> Lua indexes
    lua_pushinteger(_L, static_cast<int>(cursorEnd + 1));
    LOCAL_EVENT_SEND_CALL_WITH_SYSTEM(_L, 3)
}

void Game::sendTextInputClose() {
    LOCAL_EVENT_SEND_PUSH_WITH_SYSTEM(_L, LOCAL_EVENT_NAME_ActiveTextInputClose)
    LOCAL_EVENT_SEND_CALL_WITH_SYSTEM(_L, 0)
}

void Game::sendTextInputDone() {
    LOCAL_EVENT_SEND_PUSH_WITH_SYSTEM(_L, LOCAL_EVENT_NAME_ActiveTextInputDone)
    LOCAL_EVENT_SEND_CALL_WITH_SYSTEM(_L, 0)
}

void Game::sendTextInputNext() {
    LOCAL_EVENT_SEND_PUSH_WITH_SYSTEM(_L, LOCAL_EVENT_NAME_ActiveTextInputNext)
    LOCAL_EVENT_SEND_CALL_WITH_SYSTEM(_L, 0)
}

void Game::sendAppDidBecomeActive() {
    LOCAL_EVENT_SEND_PUSH_WITH_SYSTEM(_L, LOCAL_EVENT_NAME_AppDidBecomeActive)
    LOCAL_EVENT_SEND_CALL_WITH_SYSTEM(_L, 0)
}

void Game::sendDidReceivePushNotification(const std::string& title,
                                          const std::string& body,
                                          const std::string& category,
                                          uint32_t badge) {
    LOCAL_EVENT_SEND_PUSH_WITH_SYSTEM(_L, LOCAL_EVENT_NAME_DidReceivePushNotification)
    lua_pushstring(_L, title.c_str());
    lua_pushstring(_L, body.c_str());
    lua_pushstring(_L, category.c_str());
    lua_pushinteger(_L, static_cast<int>(badge));
    LOCAL_EVENT_SEND_CALL_WITH_SYSTEM(_L, 4)

    LOCAL_EVENT_SEND_PUSH(_L, LOCAL_EVENT_NAME_NotificationCountDidChange)
    LOCAL_EVENT_SEND_CALL(_L, 0)

}

void Game::sendPointerEvent(PointerID ID, PointerEventType type, float x, float y, float dx, float dy) {
    if (vx::GameCoordinator::getInstance()->isSystemFileModalOpened()) {
        return;
    }

    // pixels -> unsigned normalized (unorm, from 0 to 1) screen coordinates
    float relX = x / Screen::widthInPixels;
    float relY = y / Screen::heightInPixels;

    // pixels -> points
    float pdx = dx / Screen::nbPixelsInOnePoint;
    float pdy = dy / Screen::nbPixelsInOnePoint;

    switch (type) {
        case PointerEventTypeDown:
        {
            float3 o, v;
            camera_unorm_screen_to_ray(game_get_camera(this->getCGame()), relX, relY, &o, &v);
            LOCAL_EVENT_SEND_PUSH(_L, LOCAL_EVENT_NAME_PointerDown)
            lua_pointer_event_create_and_push_table(_L, relX, relY, 0.0f, 0.0f, &o, &v, true, ID);
            LOCAL_EVENT_SEND_CALL(_L, 1)
            break;
        }
        case PointerEventTypeUp:
        {
            float3 o, v;
            camera_unorm_screen_to_ray(game_get_camera(this->getCGame()), relX, relY, &o, &v);
            LOCAL_EVENT_SEND_PUSH(_L, LOCAL_EVENT_NAME_PointerUp)
            lua_pointer_event_create_and_push_table(_L, relX, relY, 0.0f, 0.0f, &o, &v, false, ID);
            LOCAL_EVENT_SEND_CALL(_L, 1)
            break;
        }
        case PointerEventTypeMove:
        {
            if (ID == PointerIDMouse) {
                // If the ID is just "Mouse" (no specific button), it's a PointerMove event, not a PointerDrag
                float3 o, v;
                camera_unorm_screen_to_ray(game_get_camera(this->getCGame()), relX, relY, &o, &v);
                LOCAL_EVENT_SEND_PUSH(_L, LOCAL_EVENT_NAME_PointerMove)
                lua_pointer_event_create_and_push_table(_L, relX, relY, pdx, pdy, &o, &v, true, PointerIDMouse);
                LOCAL_EVENT_SEND_CALL(_L, 1)

            } else {
                float3 o, v;
                camera_unorm_screen_to_ray(game_get_camera(this->getCGame()), relX, relY, &o, &v);
                LOCAL_EVENT_SEND_PUSH(_L, LOCAL_EVENT_NAME_PointerDrag)
                lua_pointer_event_create_and_push_table(_L, relX, relY, pdx, pdy, &o, &v, true, ID);
                LOCAL_EVENT_SEND_CALL(_L, 1)
            }
            break;
        }
        case PointerEventTypeWheel:
        {
            LOCAL_EVENT_SEND_PUSH(_L, LOCAL_EVENT_NAME_PointerWheel)
            lua_pushnumber(_L, static_cast<double>(-dy));
            LOCAL_EVENT_SEND_CALL(_L, 1)
            break;
        }
        case PointerEventTypeCancel:
        {
            LOCAL_EVENT_SEND_PUSH(_L, LOCAL_EVENT_NAME_PointerCancel)
            lua_pushinteger(_L, ID);
            LOCAL_EVENT_SEND_CALL(_L, 1)
            break;
        }
    }
}

void Game::sendVirtualKeyboardShownEvent(float keyboardHeight) {
    LOCAL_EVENT_SEND_PUSH(_L, LOCAL_EVENT_NAME_VirtualKeyboardShown)
    lua_pushnumber(_L, static_cast<double>(keyboardHeight));
    LOCAL_EVENT_SEND_CALL(_L, 1)
}

void Game::sendVirtualKeyboardHiddenEvent() {
    LOCAL_EVENT_SEND_PUSH(_L, LOCAL_EVENT_NAME_VirtualKeyboardHidden)
    LOCAL_EVENT_SEND_CALL(_L, 0)
}

void Game::sendFailedToLoadWorld() {
    LOCAL_EVENT_SEND_PUSH(_L, LOCAL_EVENT_NAME_FailedToLoadWorld)
    LOCAL_EVENT_SEND_CALL(_L, 0)
}

void Game::sendWorldRequested() {
    LOCAL_EVENT_SEND_PUSH(_L, LOCAL_EVENT_NAME_WorldRequested)
    LOCAL_EVENT_SEND_CALL(_L, 0)
}

void Game::sendScreenDidResize(const float width, const float height) {
    LOCAL_EVENT_SEND_PUSH(_L, LOCAL_EVENT_NAME_ScreenDidResize)
    lua_pushnumber(_L, static_cast<const double>(width));
    lua_pushnumber(_L, static_cast<const double>(height));
    LOCAL_EVENT_SEND_CALL(_L, 2)
}

Channel<GameMessage_SharedPtr>& Game::getMessagesToSend() {
    return _messagesToSend;
}

LuaExecutionLimiter& Game::getLuaExecutionLimiter() {
    return _luaExecutionLimiter;
}

const GameRendererState_SharedPtr& Game::getGameRendererState() const {
    return this->_gameRendererState;
}

void Game::_lookForServer() {

    std::string worldID = _fetchInfo->getWorldID();
    if (worldID.empty()) {
        vxlog_debug("can't get server if world has no ID");
        Game_WeakPtr wp = getWeakPtr();
        OperationQueue::getMain()->dispatch([wp](){
            Game_SharedPtr g = wp.lock();
            if (g == nullptr) { return; }
            g->_connectionState = g->connectionState_failed;
        });
        return;
    }

    vx::GameServer::Mode gameserverMode = _serverDevMode ?
    vx::GameServer::Mode::Dev :
    vx::GameServer::Mode::Play;

    // returned request can be used to cancel gracefully
    const Region::Type region = LatencyTester::shared().getPreferredRegion();
    _lookForServerRequest = vx::HubClient::getInstance().getRunningServer(_weakSelf,
                                                                          worldID,
                                                                          Config::shared().gameServerTag(),
                                                                          gameserverMode,
                                                                          region,
                                                                          [](const bool& success,
                                                                             const HubClientError& errType,
                                                                             HubClientRequest_SharedPtr request,
                                                                             hub::GameServer server) {
        HubRequestSender_SharedPtr sharedPtr = request->sender().lock();
        if (sharedPtr == nullptr) { return; }
        Game *g = static_cast<Game*>(sharedPtr.get());

        if (success == false) {
            Game_WeakPtr wp = g->getWeakPtr();
            vxlog_debug("failed to get gameserver address");
            OperationQueue::getMain()->dispatch([wp](){
                Game_SharedPtr g = wp.lock();
                if (g == nullptr) { return; }
                g->_connectionState = g->connectionState_failed;
            });
            return;
        }

        g->_serverAddr = server.address;
        g->_connect();
    });
}

void Game::_connect() {
    // TODO: check client state

    const std::string userID = vx::Account::active().userID();
    const std::string userName = vx::Account::active().username();

    Game_SharedPtr g = _weakSelf.lock();
    vx_assert(g != nullptr);

    if (_localGameServer != nullptr) {
        vxlog_info("🌐 joining local server");
        _client = GameClient::make(*_localGameServer, userID, userName, g);

    } else if (Config::debugGameServer().empty() == false) {
        vxlog_info("🌐 joining local debug server");
        _client = GameClient::make(Config::debugGameServer(), userID, userName, g);

    } else if (_serverAddr != "") {
        // the server address has already been set
        vxlog_info("🌐 joining %s", _serverAddr.c_str());
        _client = GameClient::make(_serverAddr, userID, userName, g);
    }

    // error
    if (_client == nullptr) {
        vxlog_error("client has no server to connect to");
        _connectionState = connectionState_failed;
        return;
    }

    _client->connect();
}

void Game::sendReloadWorldMessage() {
    if (_client == nullptr) return;
    _client->sendReloadWorldMessage();
}

void Game::reload() {
    _needsReload = false;
    RootNodeCodeEditor_SharedPtr editorRootNode = nullptr;
    GameCoordinator::getInstance()->loadWorld(_mode,
                                              nullptr, // no data
                                              _fetchInfo,
                                              _requestedServerAddr,
                                              _serverDevMode,
                                              _isAuthor,
                                              _extraItemsToLoad,
                                              std::unordered_map<std::string, std::string>());
}

void Game::setNeedsReload(bool b) {
    _needsReload = b;
    if (_needsReload) {
        _needsReloadRetryTimer = 0.0;
    }
}

bool Game::needsReload() {
    return _needsReload;
}

bool Game::isSolo() {
    if (_data == nullptr) return false;
    return _data->getMaxPlayers() == 1;
}

void Game::sendMessage(const GameMessage_SharedPtr& msg) {
    if (isInServerMode()) {
        _messagesToSend.push(msg);
    } else {
        if (_client == nullptr) return;
        _client->sendGameMsg(msg);
    }
}

int Game::getLocalPlayerID() {
    return _localPlayerID;
}

void Game::sendReceivedEnvToLaunch() {
    LOCAL_EVENT_SEND_PUSH_WITH_SYSTEM(_L, LOCAL_EVENT_NAME_ReceivedEnvironmentToLaunch)
    LOCAL_EVENT_SEND_CALL_WITH_SYSTEM(_L, 0)
}

void Game::_setCGameFunctionPointers() {
    game_set_object_tick_func(_cgame, &scripting_object_tick);
    game_setFuncPtr_ScriptingOnCollision(_cgame, &scripting_object_onCollision);
    game_setFuncPtr_ScriptingOnDestroy(&scripting_object_onDestroy);
}

#pragma mark GameClientDelegate

// --------------------
// GameClientDelegate
// --------------------

void Game::gameClientDidReceivePayload(const Connection::Payload_SharedPtr& payload) {

    Game_WeakPtr ws = _weakSelf;

    OperationQueue::getMain()->dispatch([ws, payload](){

        Game_SharedPtr g = ws.lock();
        if (g == nullptr) {
            return;
        }

        CGame *cgame = g->getCGame();

        // NOTE: payload will be destroyed with the message
        GameMessage_SharedPtr msg = GameMessage::make(payload);

        switch (msg->msgtype()) {
            case GameMessage::TYPE_SERVER_PLAYERJOINED:
            {
                uint8_t playerID;
                std::string userID;
                std::string userName;

                const bool ok = GameMessage::decodePlayerJoined(msg, playerID, userID, userName);
                if (ok) {
                    const bool isLocalPlayer = (playerID == g->_client->getPlayerID());
                    if (isLocalPlayer) {
                        g->addOrUpdateLocalPlayer(&playerID, nullptr, nullptr);
                    } else {
                        g->addPlayer(playerID, userName, userID, isLocalPlayer);
                    }
                    // vxlog_info("player joined: %s, %d", userName.c_str(), playerID);
                    scripting_onPlayerJoin(g, playerID);
                }
                break;
            }
            case GameMessage::TYPE_SERVER_PLAYERLEFT:
            {
                uint8_t playerID;

                const bool ok = GameMessage::decodePlayerLeft(msg, playerID);
                if (ok) {
                    const bool isLocalPlayer = (playerID == g->_client->getPlayerID());
                    if (isLocalPlayer == false) {
                        g->removePlayer(playerID);
                    }
                }
                break;
            }
            case GameMessage::TYPE_CHAT_MESSAGE:
            {
                uint8_t senderID = 0;
                std::string senderUserID;
                std::string senderUsername;
                std::string localUUID;
                std::string uuid;
                std::vector<uint8_t> recipients;
                std::string chatMessage;

                bool ok = GameMessage::decodeChatMessage(msg,
                                                         senderID,
                                                         senderUserID,
                                                         senderUsername,
                                                         localUUID,
                                                         uuid,
                                                         recipients,
                                                         chatMessage);

                if (ok) {
                    scripting_client_didReceiveChatMessage(g, senderID, senderUserID, senderUsername, localUUID, uuid, recipients, chatMessage);
                }

                break;
            }
            case GameMessage::TYPE_CHAT_MESSAGE_ACK:
            {
                std::string localUUID;
                std::string uuid;
                uint8_t status;

                bool ok = GameMessage::decodeChatMessageACK(msg,
                                                            localUUID,
                                                            uuid,
                                                            status);

                if (ok) {
                    scripting_client_didReceiveChatMessageACK(g, localUUID, uuid, status);
                }

                break;
            }
            case GameMessage::TYPE_GAME_EVENT:
            case GameMessage::TYPE_GAME_EVENT_SMALL:
            case GameMessage::TYPE_BIG_GAME_EVENT_BIG:
            {
                // received a Game Event from the server, let's decode it and
                // call the Lua callback Client.DidReceiveEvent(event)

                uint8_t senderID = 0;
                uint8_t eventType = 0;
                std::vector<uint8_t> recipients;
                uint8_t *data = nullptr;
                size_t size = 0;

                msg->step("decode game event (client)");
                msg->debug();

                const bool ok = GameMessage::decodeGameEvent(msg,
                                                             eventType,
                                                             senderID,
                                                             recipients,
                                                             &data,
                                                             &size);
                if (ok) {
                    lua_State *L = g->getLuaState();
                    vx_assert(L != nullptr);

                    switch (eventType) {
                        case EVENT_TYPE_FROM_SCRIPT:
                        {
                            // NOTE: recipients & data are freed after DidReceiveEvent is called
                            DoublyLinkedListUint8 *recipientList = doubly_linked_list_uint8_new();
                            for (uint8_t recipient : recipients) {
                                doubly_linked_list_uint8_push_back(recipientList, recipient);
                            }
                            scripting_client_didReceiveEvent(cgame, eventType, senderID, recipientList, data, size);
                            doubly_linked_list_uint8_free(recipientList);
                            break;
                        }
                        case EVENT_TYPE_SERVER_LOG_INFO:
                        {
                            std::string msg(reinterpret_cast<const char *>(data), size);
                            lua_log_info(L, "[server] " + msg);
                            break;
                        }
                        case EVENT_TYPE_SERVER_LOG_WARNING:
                        {
                            std::string msg(reinterpret_cast<const char *>(data), size);
                            lua_log_warning(L, "[server] " + msg);
                            break;
                        }
                        case EVENT_TYPE_SERVER_LOG_ERROR:
                        {
                            std::string msg(reinterpret_cast<const char *>(data), size);
                            lua_log_error(L, "[server] " + msg);
                            break;
                        }
                        default:
                        {
                            vxlog_error("unhandled event type");
                            break;
                        }
                    }
                }

                if (data != nullptr) {
                    free(data);
                }
                break;
            }
            case GameMessage::TYPE_HEARTBEAT: {
                // do nothing
                break;
            }
            case GameMessage::TYPE_RELOAD_WORLD: {
                GameCoordinator::getInstance()->setState(vx::State::Value::gameRunning);
                g->reload();
                break;
            }
            default: {
                vxlog_error("🔥 received unsupported game message (type %d)", msg->msgtype());
                break;
            }
        }
    });
}

void Game::gameClientDidConnect() {
    Game_WeakPtr wp = _weakSelf;
    OperationQueue::getMain()->dispatch([wp](){
        Game_SharedPtr g = wp.lock();
        if (g != nullptr) {
            LOCAL_EVENT_SEND_PUSH_WITH_SYSTEM(g->_L, LOCAL_EVENT_NAME_ServerConnectionSuccess)
            LOCAL_EVENT_SEND_CALL_WITH_SYSTEM(g->_L, 0)
        }
    });
}

void Game::gameClientFailedToConnect() {
    Game_WeakPtr wp = _weakSelf;
    OperationQueue::getMain()->dispatch([wp](){
        Game_SharedPtr g = wp.lock();
        if (g != nullptr) {
            LOCAL_EVENT_SEND_PUSH_WITH_SYSTEM(g->_L, LOCAL_EVENT_NAME_ServerConnectionFailed)
            LOCAL_EVENT_SEND_CALL_WITH_SYSTEM(g->_L, 0)
        }
    });
}

void Game::gameClientLostConnection() {
    Game_WeakPtr wp = _weakSelf;
    OperationQueue::getMain()->dispatch([wp](){
        Game_SharedPtr g = wp.lock();

        g->removeAllPlayers();

        if (g != nullptr) {
            LOCAL_EVENT_SEND_PUSH_WITH_SYSTEM(g->_L, LOCAL_EVENT_NAME_ServerConnectionLost)
            LOCAL_EVENT_SEND_CALL_WITH_SYSTEM(g->_L, 0)
        }

        // reset local player ID to "not attributed"
        // after OnPlayerLeave LocalEvent has been sent
        uint8_t pid = PLAYER_ID_NOT_ATTRIBUTED;
        g->addOrUpdateLocalPlayer(&pid, nullptr, nullptr);
    });
}

void Game::gameClientDidCloseConnection() {}

void Game::setEnvironment(const std::unordered_map<std::string, std::string>& environment) {
    _environment = environment;
}

void Game::mergeEnvironment(const std::unordered_map<std::string, std::string>& environment) {
    for (std::pair<std::string, std::string> entry : environment) {
        _environment[entry.first] = entry.second;
    }
}

const std::unordered_map<std::string, std::string>& Game::getEnvironment() const {
    return _environment;
}

void Game::appendToEnvironment(const std::string& key, const std::string& value) {
    _environment.emplace(key, value);
}

bool Game::loadWorldData() {
    if (_state != loadingWorldData) {
        vxlog_error("World trying to load data while in wrong state (2)");
        _setState(failedToLoadWorldData);
        return false;
    }
    
    if (scripting_loadScript(this, &_L) == false) {
        _setState(failedToLoadWorldData);
        return false;
    } else {
        _setState(loadedWorldData);
    }

    return true;
}

void Game::connectionDidEstablish(Connection& conn) {
    vxlog_debug("🥑 CONNECTED TO CURSOR");

    WSConnection& c = static_cast<WSConnection&>(conn);

    std::string script = this->_data->getScript();

    size_t len = script.size();
    char *buf = static_cast<char*>(malloc(len));
    memcpy(buf, script.c_str(), len);
    c.writeBytes(buf, len);
}

void Game::connectionDidReceive(Connection& conn, const Connection::Payload_SharedPtr& payload) {
    vxlog_debug("🥑 RECEIVED MESSAGE FROM CURSOR");
    // vxlog_debug("🥑 CONTENT: %s", payload->getRawBytes().c_str());

    std::string gameID = getID();
    std::string script = this->_data->getScript();
    std::string newScript = payload->getRawBytes();

    if (newScript == script) {
        vxlog_debug("🥑 SCRIPTS ARE THE SAME, DON'T DO ANYTHING");
        return;
    }

    Game_WeakPtr gameWeakPtr = _weakSelf;

    HubClient::getInstance().setWorldScript(HubClient::getInstance().getUniversalSender(),
                                            gameID,
                                            newScript,
                                            [gameWeakPtr](const bool &success, HttpRequest_SharedPtr request) {

        vx::OperationQueue::getMain()->dispatch([gameWeakPtr, success](){
            if (success) {
                Game_SharedPtr g = gameWeakPtr.lock();
                if (g == nullptr) {
                    return;
                }
                if (g->isSolo()) {
                    g->reload();
                } else {
                    // sends message to gameserver, requesting reload
                    // retrying in loop in case connection can't be established
                    g->setNeedsReload(true);
                }
            }
        });
    });
}

void Game::connectionDidClose(Connection& conn) {
    vxlog_debug("🥑 LOST CURSOR CONNECTION");
}
