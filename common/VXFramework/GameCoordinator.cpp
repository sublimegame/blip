//
//  GameCoordinator.cpp
//  Cubzh
//
//  Created by Gaetan de Villele on 07/02/2020.
//  Copyright © 2020 voxowl. All rights reserved.
//

#include "GameCoordinator.hpp"

// engine
#include "cache.h"
#include "game.h"

// xptools
#include "xptools.h"
#include "OperationQueue.hpp"
#include "strings.hpp"
#include "textinput.h"
#include "notifications.hpp"

// Cubzh
#include "VXAccount.hpp"
#include "VXApplication.hpp"
#include "VXConfig.hpp"
#include "VXGameMessage.hpp"
#include "VXHubClient.hpp"
#include "VXMenuConfig.hpp"
#include "VXPrefs.hpp"
#include "VXResource.hpp"

// Root nodes
#include "VXRootNodeLoading.hpp"
#include "VXRootNodeCodeEditor.hpp"

// Sandbox
#include "lua.hpp"
#include "lua_player.hpp"
#include "lua_node.hpp"
#include "lua_sandbox.hpp"
#include "lua_system.hpp"
#include "scripting.hpp"
#include "lua_menu.hpp"
#include "lua_require.hpp"

// Gameserver
#include "VXGameServer.hpp"

// Networking
#include "VXNetworkingUtils.hpp"

using namespace vx;

const std::string GameCoordinator::ENV_FLAG_WORLD_ID = "worldID";
// where to load the script from
// supported:
// - github.com/repo/name/path/to/dir
// - github.com/repo/name/path/to/dir:COMMIT
const std::string GameCoordinator::ENV_FLAG_SCRIPT = "script";

/// --------------------------------------------------
///
/// MARK: - constructor/destructor -
///
/// --------------------------------------------------

static void keyboard_input_callback(void *userdata, uint32_t charCode, Input input, uint8_t modifiers, KeyState state) {
    Game_SharedPtr g = vx::GameCoordinator::getInstance()->getActiveGame();
    if (g == nullptr) { return; }
    g->sendKeyboardInput(charCode, input, modifiers, state);
}

static void pointer_event_callback(void *userdata, PointerID ID, PointerEventType type, float x, float y, float dx, float dy) {
    Game_SharedPtr g = vx::GameCoordinator::getInstance()->getActiveGame();
    if (g == nullptr) { return; }
    g->sendPointerEvent(ID, type, x, y, dx, dy);
}

/// Constructor
GameCoordinator::GameCoordinator() : TickListener(),
_states(),
_scheduledState(nullptr),
_currentRootNode(nullptr),
_currentRootNodeState(nullptr),
_codeEditorNode(nullptr),
_backgroundGameFilepath(nullptr),
_backgroundGameID(),
_backgroundGameMutex(),
_stateMutex(),
_isBackgroundGameShown(false),
_popupToShow(nullptr),
_quitResumePopup(nullptr),
_keyboardSize(),
_previousCommands(),
_loadingWorld(nullptr),
_loadedWorld(nullptr),
_worldEditorState(WorldEditorState::none),
_editorWorld(nullptr),
_systemFileModalOpened(false),
_envToLaunch(),
_lastLoadedGameInfo(nullptr),
_keyboardInputListener(nullptr),
_pointerEventListener(nullptr),
_worldIDToEdit(),
_codeToEdit() {

    // THINGS THAT SHOULD GO AWAY
    // ONCE DONE WITH GAME SLOTS
    this->_backgroundGameID = ""; // TODO: shouldn't be required
    this->_backgroundGameFilepath = nullptr; // TODO: remove

    // game slots
    this->_loadingWorld = nullptr;
    this->_loadedWorld = nullptr;

    // GENERAL STATE

    this->_states = std::stack<State_SharedPtr>();

    this->_scheduledState = nullptr;

    this->_keyboardSize = Size(0.0f, 0.0f);

    // NODES

    this->_currentRootNode = nullptr;
    this->_currentRootNodeState = nullptr;

    // MAIN MENU STATES AND VARIABLES

    _resetMainMenuStates();

    // listeners

    NotificationCenter::shared().addListener(this, NotificationName::didResize);

    vx::textinput::textInputRegisterDelegate([](std::string str, bool strDidChange, size_t cursorStart, size_t cursorEnd) { // update
        Node_WeakPtr focusedWeak = Node::Manager::shared()->focused();
        Node_SharedPtr focused = focusedWeak.lock();
        if (focused != nullptr) {
            CodeEditor_SharedPtr codeEditor = std::static_pointer_cast<CodeEditor>(focused);
            if (codeEditor != nullptr) {
                codeEditor->update(str, strDidChange, true, cursorStart, cursorEnd);
            }
            return;
        }
        Game_SharedPtr g = vx::GameCoordinator::getInstance()->getActiveGame();
        if (g == nullptr) { return; }
        g->sendTextInputUpdate(str, cursorStart, cursorEnd);
    }, [](){ // close
        Game_SharedPtr g = vx::GameCoordinator::getInstance()->getActiveGame();
        if (g == nullptr) { return; }
        g->sendTextInputClose();
    }, [](){ // done
        Game_SharedPtr g = vx::GameCoordinator::getInstance()->getActiveGame();
        if (g == nullptr) { return; }
        g->sendTextInputDone();
    }, [](){ // next
        Game_SharedPtr g = vx::GameCoordinator::getInstance()->getActiveGame();
        if (g == nullptr) { return; }
        g->sendTextInputNext();
    });
}

void GameCoordinator::_resetMainMenuStates() {
    this->_popupToShow = nullptr;
    this->_quitResumePopup = nullptr;
}

/// Destructor
GameCoordinator::~GameCoordinator() {
    NotificationCenter::shared().removeListener(this);

    input_keyboard_listener_free(_keyboardInputListener, nullptr);

    if (_lastLoadedGameInfo != nullptr) {
        delete _lastLoadedGameInfo;
    }
}

GameCoordinator *GameCoordinator::getInstance() {
    if (_instance == nullptr) {
        _instance = new GameCoordinator();
    }
    return _instance;
}

State_SharedPtr GameCoordinator::getState() {
    const std::lock_guard<std::mutex> locker(_stateMutex);
    if (this->_states.empty()) { return nullptr; }
    return this->_states.top();
}

const Node_SharedPtr& GameCoordinator::getCurrentRootNode() {

    State_SharedPtr currentState = getState();

    if (currentState == _currentRootNodeState) {
        return this->_currentRootNode;
    }

    State::Value v = currentState->getValue();

    switch (v) {
        case State::Value::startupLoading:
            this->_currentRootNode = RootNodeLoading::make();
            break;
        case State::Value::noActiveGame:
            this->_currentRootNode = RootNodeLoading::make();
            break;
        case State::Value::gameRunning:
            this->_currentRootNode = this->_loadedWorld->getRootNode();
            break;
        case State::Value::activeWorldCodeEditor:
        case State::Value::activeWorldCodeReader:
        case State::Value::worldCodeEditor:
            if (_codeEditorNode == nullptr) {
                _codeEditorNode = RootNodeCodeEditor::make(_worldIDToEdit, _codeToEdit, v == State::Value::activeWorldCodeReader);
            }

            RootNodeCodeEditor_WeakPtr weakCodeEditor = RootNodeCodeEditor_WeakPtr(_codeEditorNode);

            if (v == State::Value::worldCodeEditor) {
                _codeEditorNode->setCancelLabelAndCallback("Close", [](){
                    GameCoordinator::getInstance()->setState(State::gameRunning);
                });
                _codeEditorNode->setPublishLabelAndCallback("Save", [](){
                    GameCoordinator::getInstance()->setState(State::gameRunning);
                });
            } else {
                Game_SharedPtr g = getActiveGame();
                if (g != nullptr && g->getID() == _codeEditorNode->getWorldID()) {
                    _codeEditorNode->setCancelLabelAndCallback("Cancel", [](){
                        GameCoordinator::getInstance()->setState(State::gameRunning);
                    });
                    _codeEditorNode->setPublishLabelAndCallback("Save & Restart", [weakCodeEditor](){
                        RootNodeCodeEditor_SharedPtr codeEditor = weakCodeEditor.lock();
                        if (codeEditor == nullptr) { return; }
                        GameCoordinator *gc = GameCoordinator::getInstance();
                        Game_SharedPtr g = gc->getActiveGame();
                        if (g != nullptr && g->getID() == codeEditor->getWorldID()) {
                            if (g->isSolo()) {
                                g->reload(); // reload right away
                            } else {
                                // sends message to gameserver, requesting reload
                                // retrying in loop in case connection can't be established
                                g->setNeedsReload(true);
                            }
                        } else {
                            g = gc->getLoadingWorld();
                            if (g != nullptr && g->getID() == codeEditor->getWorldID()) {
                                if (g->isSolo()) {
                                    g->reload();
                                } else {
                                    g->setNeedsReload(true);
                                }
                            }
                        }
                    });
                    
                } else {
                    _codeEditorNode->setCancelLabelAndCallback("Cancel", [](){
                        GameCoordinator::getInstance()->setState(State::gameRunning);
                    });
                    _codeEditorNode->setPublishLabelAndCallback("Save", [](){
                        GameCoordinator::getInstance()->setState(State::gameRunning);
                    });
                }
            }



            this->_currentRootNode = _codeEditorNode;
            break;
    }

    _currentRootNodeState = currentState;

    vx_assert(this->_currentRootNode != nullptr);
    return this->_currentRootNode;
}

std::string GameCoordinator::getIdealLoadingMessage() {

    Game_SharedPtr g = this->getLoadingWorld();
    if (g == nullptr) {
        return "";
    }
    Game::State state = g->getState();
    switch (state) {
        case Game::State::downloadingWorldData:
        case Game::State::downloadedWorldData:
        case Game::State::loadingWorldData:
        case Game::State::loadedWorldData:
        {
            double loaded = static_cast<double>(g->getDataLoaded());
            std::string s = std::string("Loading World");
            int dataLen = g->getDataLength();
            if (dataLen > 0) {
                char buffer[50];
                double p = (loaded / static_cast<double>(dataLen)) * 100.0;
                if (loaded >= 1048576) { // > 1MB, display in MB
                    // vxlog_debug("LOADED %.2fMB", loaded / 1048576.0);
                    std::snprintf(buffer, sizeof(buffer), " %.2f%% (%.2fMB)", p, loaded / 1048576.0);
                } else { // display in KB
                    // vxlog_debug("LOADED %.2fKB", loaded / 1024.0);
                    std::snprintf(buffer, sizeof(buffer), " %.2f%% (%.2fKB)", p, loaded / 1024.0);
                }
                s = s + buffer;
            } else {
                char buffer[50];
                if (loaded >= 1048576) { // > 1MB, display in MB
                    // vxlog_debug("LOADED %.2fMB", loaded / 1048576.0);
                    std::snprintf(buffer, sizeof(buffer), " (%.2fMB)", loaded / 1048576.0);
                } else { // display in KB
                    // vxlog_debug("LOADED %.2fKB", loaded / 1024.0);
                    std::snprintf(buffer, sizeof(buffer), " (%.2fKB)", loaded / 1024.0);
                }
                s = s + buffer;
            }
            return s;
        }
        case Game::State::loadingModules:
        case Game::State::loadedModules:
        {
            std::string s = "Loading Modules";
            size_t modulesToLoad = g->getModulesToLoad();
            if (modulesToLoad > 0) {
                size_t modulesLoaded = g->getModulesLoaded();
                size_t modulesFailed = g->getModulesFailed();
                char buffer[50];
                std::snprintf(buffer, sizeof(buffer), " %lu/%lu", modulesLoaded + modulesFailed, modulesToLoad);
                s = s + buffer;
            }
            return std::string(s);
        }
        case Game::State::downloadingOtherDependencies:
        case Game::State::downloadedOtherDependencies:
            return "Loading Dependencies";
        case Game::State::starting:
        case Game::State::started:
            return "Starting";
        default:
            break;
    }

    return "Loading";
}

bool GameCoordinator::isPlayerGameState(const State::Value& value) {
    // 'loadingGame' is included here as a player-selected game state, because the background game
    // should be unloaded when in any of these states
    return value == State::Value::gameRunning
    // || value == State::Value::gamePaused
    || value == State::Value::activeWorldCodeEditor
    || value == State::Value::activeWorldCodeReader
    || value == State::Value::worldCodeEditor;
}

bool GameCoordinator::shouldMouseCursorBeHidden() {
    Game_SharedPtr g = vx::GameCoordinator::getInstance()->getActiveGame();
    if (g == nullptr) { return false; }
    if (g->pointerForceShown()) { return false; }

    CGame *cgame = g->getCGame();
    vx_assert(cgame != nullptr);
    return (vx::GameCoordinator::getInstance()->getState()->is(State::Value::gameRunning)  &&
            game_ui_state_get_isPointerVisible(cgame) == false);
}

void GameCoordinator::setState(const State::Value& newState) {
    setState(State::make(newState));
}

void GameCoordinator::setState(State_SharedPtr newState) {

    const std::lock_guard<std::mutex> locker(_stateMutex);

    if (this->_scheduledState != nullptr) {
        if (this->_states.empty() == false) {
            State_SharedPtr currentState = this->_states.top();
            vxlog_error("state can't change twice during same tick (%s -> %s)",
                        currentState->toString().c_str(),
                        newState->toString().c_str());
        } else {
            vxlog_error("state can't change twice during same tick (-> %s)",
                        newState->toString().c_str());
        }
        return;
    }

    if (this->_states.empty()) {
        _scheduledState = newState;
        return;
    }

    State_SharedPtr currentState = this->_states.top();
    if (currentState->canTransitionTo(newState->getValue()) == false) {
        vxlog_error("invalid state change (%s -> %s)", currentState->toString().c_str(), newState->toString().c_str());
        return;
    }

    _scheduledState = newState;
}

void GameCoordinator::initCurrentState() {

    State_SharedPtr currentState = this->getState();

    if (this->_pointerEventListener != nullptr) {
        pointer_event_listener_free(this->_pointerEventListener, nullptr);
        this->_pointerEventListener = nullptr;
    }

    if (this->_keyboardInputListener != nullptr) {
        input_keyboard_listener_free(this->_keyboardInputListener, nullptr);
        this->_keyboardInputListener = nullptr;
    }

    switch (currentState->getValue()) {
        case State::Value::startupLoading:
        {
            this->_startupLoading();
            break;
        }
        case State::Value::gameRunning:
        {
            this->_keyboardInputListener = input_keyboard_listener_new(nullptr, keyboard_input_callback);
            this->_pointerEventListener = pointer_event_listener_new(nullptr, pointer_event_callback);
            break;
        }
        default:
            break;
    }
}

///
void GameCoordinator::applyScheduledState() {
    {
        const std::lock_guard<std::mutex> locker(_stateMutex);
        if (_scheduledState == nullptr) return;

        if (_states.empty() == false) {
            _states.pop();
        }
        _states.push(this->_scheduledState);

        _scheduledState = nullptr;
    }

    initCurrentState();
    vx::NotificationCenter::shared().notify(vx::NotificationName::stateChanged);

    vx::OperationQueue::getMain()->dispatch([](){
        Game_SharedPtr g = GameCoordinator::getInstance()->getActiveGame();
        if (g == nullptr) return;
        g->sendCPPMenuStateChanged();
    });
}

void GameCoordinator::setupCodeEditor(std::string worldID, std::string code, bool readOnly) {
    _worldIDToEdit = worldID;
    _codeToEdit = code;
    if (_codeEditorNode != nullptr) {
        if (_codeEditorNode->getWorldID() == _worldIDToEdit) { // same world, just update script
            _codeEditorNode->updateScript(_codeToEdit);
            _codeEditorNode->setReadOnly(readOnly);
            return;
        }

        // not same world ID

        if (_currentRootNode != _codeEditorNode) {
            _codeEditorNode = nullptr; // will be created when needed
            return;
        }

        _codeEditorNode = RootNodeCodeEditor::make(_worldIDToEdit, _codeToEdit, readOnly);
        _currentRootNode = _codeEditorNode;
    }
}

///
void GameCoordinator::quickGameJoin(const std::string& gameID,
                                    const bool isAuthor,
                                    const bool gameDevMode,
                                    const bool &useLocalServer) {

    std::unordered_map<std::string, std::string> env = _envToLaunch;
    _envToLaunch = std::unordered_map<std::string, std::string>();

    if (gameID == WORLD_HOME_ID) {
        loadHome("", gameDevMode, isAuthor, env);
        return;
    }

    this->loadWorld(GameMode_Client,
                    nullptr, // don't provide game data
                    WorldFetchInfo::makeWithID(gameID),
                    "",
                    gameDevMode,
                    isAuthor,
                    std::vector<std::string>(),
                    env);
}

void GameCoordinator::exitWorldEditor() {
    if (_worldEditorState != WorldEditorState::none) {
        _worldEditorState = WorldEditorState::exiting;
    }
}


void GameCoordinator::loadHome(const std::string serverAddr, 
                              const bool gameDevMode,
                              const bool isAuthor, 
                              std::unordered_map<std::string,std::string> env) {

    vx::OperationQueue::getMain()->dispatch([serverAddr, gameDevMode, isAuthor, env](){

        FILE *file = vx::fs::openBundleFile(HUB_SCRIPT_BUNDLE_PATH);
        vx_assert(file != nullptr);
        std::string script;
        bool ok;
        ok = vx::fs::getFileTextContentAsStringAndClose(file, script);
        vx_assert(ok);
        vx_assert(lua_require_verify_local_script(script, "cubzh.lua"));

//        FILE *fileMapBase64 = vx::fs::openBundleFile(HUB_MAPBASE64_BUNDLE_PATH);
//        std::string mapBase64;
//        ok = vx::fs::getFileTextContentAsStringAndClose(fileMapBase64, mapBase64);
//        vx_assert(ok);

        GameData_SharedPtr data = GameData::make(script, "", "", 1);

        GameCoordinator::getInstance()->loadWorld(GameMode_Client,
                                                  data,
                                                  WorldFetchInfo::makeWithID(WORLD_HOME_ID),
                                                  serverAddr,
                                                  gameDevMode,
                                                  isAuthor, // isAuthor
                                                  std::vector<std::string>(),
                                                  env);
    });
}

///
void GameCoordinator::launchLocalItemEditor(const std::string& repo, 
                                            const std::string& name,
                                            const std::string& itemCategory) {

    const std::string itemName = repo + "." + name;

    // invalidate local cache for this item
    {
        CacheErr err = cacheCPP_expireCacheForItem(repo, name);
        if (err != nullptr) {
            vxlog_warning("item editor launch warning: %s", err->c_str());
        }
    }

    vxlog_info("Launching local Item Editor for item [%s] [%s]", itemName.c_str(), itemCategory.c_str());
    bool ok = false;

    // read Item Editor Lua code from bundle
    FILE * const fd = ::vx::fs::openBundleFile("modules/item_editor.lua");
    // FILE *fd = ::vx::fs::openStorageFile("game_item_editor.lua"); // useful when working on the script
    if (fd == nullptr) {
        vxlog_error("FAILED TO OPEN ITEM EDITOR SCRIPT");
        return;
    }
    std::string luaScript;
    ok = vx::fs::getFileTextContentAsStringAndClose(fd, luaScript);
    if (ok == false) {
        vxlog_error("FAILED TO READ LUA SCRIPT FROM BUNDLE");
        return;
    }

    ok = vx::str::replaceStringInString(luaScript, "%item_name%", itemName);
    if (ok == false) {
        vxlog_error("FAILED TO CUSTOMIZE LUA SCRIPT FOR ITEM EDITOR");
        return;
    }

    std::vector<std::string> itemsToLoad;
    itemsToLoad.push_back(itemName);

    // add item category to the Lua environment
    std::unordered_map<std::string, std::string> env;
    env.emplace("itemFullname", itemName);
    env.emplace("itemName", name);
    env.emplace("itemRepo", repo);
    env.emplace("itemCategory", itemCategory);

    this->loadWorld(GameMode_ClientOffline,
                    GameData::make(luaScript, "", "", 1),
                    WorldFetchInfo::makeWithID(GAME_ID_FOR_LOCAL_ITEMEDITOR),
                    "",
                    false,
                    false,
                    itemsToLoad,
                    env);
}

bool GameCoordinator::hasEnvironmentToLaunch() {
    if (_envToLaunch.empty()) { return false; }

    const auto worldIdEntry = _envToLaunch.find(GameCoordinator::ENV_FLAG_WORLD_ID);
    if (worldIdEntry != _envToLaunch.end()) {
        return true;
    }

    return false;
}

void GameCoordinator::launchEnvironment() {

    vx::OperationQueue::getMain()->dispatch([](){

        GameCoordinator *gc = GameCoordinator::getInstance();

        if (gc->_envToLaunch.empty()) { return; }

        std::string worldID = "";

        const auto worldIdEntry = gc->_envToLaunch.find(GameCoordinator::ENV_FLAG_WORLD_ID);
        if (worldIdEntry != gc->_envToLaunch.end()) {
            worldID = worldIdEntry->second;
            vxlog_trace("LAUNCH WORLD : %s", worldID.c_str());
        }

        std::unordered_map<std::string, std::string> env = gc->_envToLaunch;
        gc->_envToLaunch.clear();

        gc->loadingWorldStateChange();

        if (worldID == "") { return; }
        if (worldID == WORLD_HOME_ID) {
            gc->loadHome("", GAME_LAUNCH_NOT_DEV_MODE, false, env); // TODO -> pass env
            return;
        }

        gc->loadWorld(GameMode_Client,
                      nullptr, // no game Data
                      WorldFetchInfo::makeWithID(worldID),
                      "",
                      GAME_LAUNCH_NOT_DEV_MODE,
                      false, // isAuthor
                      std::vector<std::string>(),
                      env);
    });
}

void GameCoordinator::loadingWorldStateChange() {
    if (_worldEditorState == WorldEditorState::editing) {
        return;
    }

    vx::OperationQueue::getMain()->dispatch([](){
        GameCoordinator *gc = GameCoordinator::getInstance();
        if (gc->_worldEditorState == WorldEditorState::editing) { return; }

        Game_SharedPtr g = gc->getActiveGame();
        if (g == nullptr) { return; }

        lua_State *L = g->getLuaState();
        if (L == nullptr) { return; }

        std::string loadingMessage = GameCoordinator::getInstance()->getIdealLoadingMessage();
        if (loadingMessage != "") {
            MENU_CALL_SYSTEM_LOADING(L, loadingMessage.c_str());
        }
    });
}

void GameCoordinator::showBackgroundGame() {
    this->_isBackgroundGameShown = true;
    NotificationCenter::shared().notify(NotificationName::toggleBackgroundGame);
}

void GameCoordinator::hideBackgroundGame() {
    this->_isBackgroundGameShown = false;
    NotificationCenter::shared().notify(NotificationName::toggleBackgroundGame);
}

bool GameCoordinator::isBackgroundGameShown() const {
    return this->_isBackgroundGameShown;
}

void GameCoordinator::showKeyboard(Size keyboardSize, float duration) {
    this->_keyboardSize = keyboardSize;
    vx::NotificationCenter::shared().notify(vx::NotificationName::showKeyboard);

    Game_SharedPtr game = this->getActiveGame();
    if (game != nullptr) {
        game->sendVirtualKeyboardShownEvent(keyboardSize.getHeight());
    }
}

void GameCoordinator::hideKeyboard(float duration) {
    this->_keyboardSize = Size(0.0f, 0.0f);
    vx::NotificationCenter::shared().notify(vx::NotificationName::hideKeyboard);

    Game_SharedPtr game = this->getActiveGame();
    if (game != nullptr) {
        game->setActiveKeyboard(KeyboardTypeNone);
        game->sendVirtualKeyboardHiddenEvent();
    }
}

Size GameCoordinator::keyboardSize() {
    return this->_keyboardSize;
}

void GameCoordinator::addPreviousCommand(std::string command) {
    if (this->_previousCommands.size() >= MenuConfig::shared().maxStoredCommands) {
        this->_previousCommands.pop_front();
    }

    if (this->_previousCommands.size() == 0) {
        this->_previousCommands.push_back(command);
        return;
    }

    std::string last = this->_previousCommands.back();
    if (last != command) {
        this->_previousCommands.push_back(command);
    }
}

const std::string& GameCoordinator::getPreviousCommand(size_t n) {
    vx_assert(n > 0);

    // gets stuck in the first command if out of range
    if (n >= this->_previousCommands.size()) {
        return *this->_previousCommands.begin();
    }

    n = this->_previousCommands.size() - n;
    std::list<std::string>::iterator it = this->_previousCommands.begin();
    while (n--) {
        ++it;
    }
    return *it;
}

size_t GameCoordinator::getNbPreviousCommands() {
    return this->_previousCommands.size();
}

void GameCoordinator::loadWorld(GameMode gameMode,
                                GameData_SharedPtr data,
                                WorldFetchInfo_SharedPtr fetchInfo,
                                const std::string serverAddr,
                                const bool gameDevMode,
                                const bool isAuthor,
                                std::vector<std::string> itemsToLoad,
                                std::unordered_map<std::string,std::string> env) {

    env["worldId"] = fetchInfo->getWorldID(); // can be empty

    vx::OperationQueue::getMain()->dispatch([gameMode,
                                             data,
                                             fetchInfo,
                                             serverAddr,
                                             gameDevMode,
                                             isAuthor,
                                             itemsToLoad,
                                             env](){

        Game_SharedPtr g = GameCoordinator::getInstance()->getActiveGame();
        if (g != nullptr) {
            g->sendWorldRequested();
        }

        // fetchInfo should contains a way to retrieve world script
        vx_assert(fetchInfo->isValid());

        GameCoordinator *gc = GameCoordinator::getInstance();

        if (gc->_lastLoadedGameInfo != nullptr) {
            delete gc->_lastLoadedGameInfo;
            gc->_lastLoadedGameInfo = nullptr;
        }

        if (gameMode == GameMode_Client && gameDevMode && isAuthor) {
            if (gc->_editorWorld == nullptr) {
                std::string worldEditorScript = R"""(
Client.OnStart = function()
    require("world_editor")
end
)""";
                GameData_SharedPtr _data = GameData::make(worldEditorScript, "", "", 1);
                std::unordered_map<std::string,std::string> _env = std::unordered_map<std::string,std::string>();
                _env["EDITED_WORLD_ID"] = fetchInfo->getWorldID();
                _env["worldId"] = "world_editor";

                gc->_worldEditorState = WorldEditorState::loadingEditor;
                gc->_editorWorld = Game::make(gameMode,
                                              WorldFetchInfo::makeWithID(WORLD_EDITOR_ID),
                                              _data,
                                              "", // server address
                                              gameDevMode,
                                              isAuthor,
                                              std::vector<std::string>() // items to load
                                              );
                gc->_editorWorld->setEnvironment(_env);
                gc->_editorWorld->tickUntilLoaded();
                gc->_worldEditorState = WorldEditorState::editing;

                // set both other slots to nullptr
                // the edited world starts loading right after to support multiplayer connections
                // while the world is being edited.
                gc->_loadedWorld = nullptr;
                gc->_loadingWorld = nullptr;
                vx::NotificationCenter::shared().notify(vx::NotificationName::loadingGameStateChanged);
            }
        } else {
            gc->_editorWorld = nullptr; // when exiting local dev mode
        }


        gc->_lastLoadedGameInfo = new GameInfo();
        gc->_lastLoadedGameInfo->gameMode = gameMode;
        gc->_lastLoadedGameInfo->data = data;
        gc->_lastLoadedGameInfo->fetchInfo = fetchInfo;
        gc->_lastLoadedGameInfo->serverAddr = serverAddr;
        gc->_lastLoadedGameInfo->gameDevMode = gameDevMode;
        gc->_lastLoadedGameInfo->isAuthor = isAuthor;
        gc->_lastLoadedGameInfo->itemsToLoad = itemsToLoad;
        gc->_lastLoadedGameInfo->env = env;

        gc->_loadingWorld = Game::make(gameMode,
                                       fetchInfo,
                                       data,
                                       serverAddr,
                                       gameDevMode,
                                       isAuthor,
                                       itemsToLoad);

        gc->_loadingWorld->setEnvironment(env);

    });
}

void GameCoordinator::didRegisterForRemoteNotifications(const std::string& tokenTypeStr,
                                                        const std::string& tokenValue) {
    vxlog_debug("📣[PushNotifs] didRegisterForRemoteNotifications [%s] [%s]", tokenTypeStr.c_str(), tokenValue.c_str());

    // Convert token type from string to enum
    hub::PushToken::Type tokenType;
    if (tokenTypeStr == "apple") {
        tokenType = hub::PushToken::Type::Apple;
    } else if (tokenTypeStr == "firebase") {
        tokenType = hub::PushToken::Type::Firebase;
    } else {
        vxlog_error("unsupported push token type");
        return;
    }

    // Store token temporarily, before trying to upload it to the Hub API
    const bool saved = vx::Prefs::shared().setNotifPushTokenToUpload(tokenValue);
    if (saved == false) {
        vxlog_error("failed to save push token");
        return;
    }

    vx::OperationQueue::getMain()->dispatch([tokenType, tokenValue](){
        // Register token on the Hub API
        HubClient &hubClient = HubClient::getInstance();
        hubClient.postPushToken(hubClient.getUniversalSender(),
                                tokenType,
                                tokenValue,
                                [tokenValue](const bool& success, HubClientRequest_SharedPtr request){
            vxlog_debug("📣[PushNotifs] token saved on Hub API: %s", success ? "success" : "failure");

            if (success == false) {
                vxlog_debug("📣[PushNotifs] did register for remote notifications: failure");
                return;
            }

            // Token has been uploading successfully, we can remove it from temporary storage
            bool saved = Prefs::shared().setNotifPushTokenToUpload("");
            if (saved == false) {
                vxlog_debug("📣[PushNotifs] did register for remote notifications: failure");
                return;
            }

            // Save token in local preferences
            saved = vx::Prefs::shared().setNotifPushToken(tokenValue);

            // Notify of success or error
            vxlog_debug("📣[PushNotifs] did register for remote notifications: %s", saved ? "success" : "failure");
        });
    });
}

void GameCoordinator::retryPushTokenUploadIfNeeded() {
    const std::string token = Prefs::shared().notifPushTokenToUpload();
    if (token.empty()) {
        return;
    }

    // try to upload the token
    hub::PushToken::Type tokenType;
#if defined(__VX_PLATFORM_ANDROID)
    tokenType = hub::PushToken::Type::Firebase;
#elif defined(__VX_PLATFORM_IOS)
    tokenType = hub::PushToken::Type::Apple;
#elif defined(__VX_PLATFORM_MACOS)
    tokenType = hub::PushToken::Type::Apple;
#elif defined(__VX_PLATFORM_WINDOWS)
    // push notifications are not supported yet on this platform
    return;
#elif defined(__VX_PLATFORM_WASM)
    // push notifications are not supported yet on this platform
    return;
#elif defined(__VX_PLATFORM_LINUX)
    // push notifications are not supported yet on this platform
    return;
#else
#error unsupported platform
#endif

    HubClient::getInstance().postPushToken(HubClient::getInstance().getUniversalSender(),
                                           tokenType,
                                           token, [token](const bool& success,
                                                          HubClientRequest_SharedPtr request){
        vxlog_debug("☀️ Push Token uploaded to Hub API: %s", success ? "SUCCESS" : "FAILURE");
        if (success) {
            // Token has been uploading successfully, we can remove it from temporary storage
            Prefs::shared().setNotifPushTokenToUpload("");
            Prefs::shared().setNotifPushToken(token);
        }
    });
}

GameCoordinator *GameCoordinator::_instance = nullptr;

void GameCoordinator::tick(const double dt) {

    Game::StateChange gameStateChange;
    Game_SharedPtr g;
    GameCoordinator *gc = GameCoordinator::getInstance();

    // ----------------------------
    // ACTIVE WORLD TICK
    // ----------------------------
    g = gc->getActiveGame();
    if (g != nullptr) {
        g->tick(dt);
    }

    // ----------------------------
    // LOADING WORLD TICK
    // ----------------------------

    g = gc->getLoadingWorld();
    if (g != nullptr) {
        gameStateChange = g->tick(dt);

        switch (gameStateChange) {
            case Game::none:
            {
                // nothing to do
                break;
            }
            case Game::loading:
            {
                this->loadingWorldStateChange();
                break;
            }
            case Game::running:
            {
                switch (_worldEditorState) {
                    case WorldEditorState::loadingWorld:
                        _worldEditorState = WorldEditorState::testingWorld;
                        break;
                    case WorldEditorState::exiting:
                        _worldEditorState = WorldEditorState::none;
                        break;
                    default:
                        break;
                }

                bool isLocalDev = g->localUserIsAuthor() && g->serverIsInDevMode();
                setupCodeEditor(g->getID(), g->getData()->getScript(), isLocalDev == false);

                this->_loadedWorld = g;
                this->_loadingWorld = nullptr;

                setState(State::gameRunning);
                vx::NotificationCenter::shared().notify(vx::NotificationName::didResize);
                break;
            }
            case Game::failed:
            {
                // nothing to do ?
                break;
            }
            case Game::minorChange:
            {
                // nothing to do
                break;
            }
        }

        if (gameStateChange != Game::none && this->_loadingWorld != nullptr) {
            this->loadingWorldStateChange();
            vx::NotificationCenter::shared().notify(vx::NotificationName::loadingGameStateChanged);
        }
    }
}

void GameCoordinator::setSystemFileModalOpened(bool isOpened) {
    this->_systemFileModalOpened = isOpened;
}

bool GameCoordinator::isSystemFileModalOpened() {
    return this->_systemFileModalOpened;
}

Game_SharedPtr GameCoordinator::getActiveGame() {
    if (this->_worldEditorState == WorldEditorState::editing ||
        this->_worldEditorState == WorldEditorState::loadingWorld) {
        return this->_editorWorld;
    }
    if (this->_loadedWorld != nullptr && this->_loadedWorld->getState() == Game::State::started) {
        return this->_loadedWorld;
    }
    return nullptr;
}

const Game_SharedPtr& GameCoordinator::getLoadedWorld() {
    return this->_loadedWorld;
}

const Game_SharedPtr& GameCoordinator::getLoadingWorld() {
    return this->_loadingWorld;
}

const Game_SharedPtr& GameCoordinator::getEditorWorld() {
    return this->_editorWorld;
}

void GameCoordinator::worldEditorPlay() {
    if (this->_worldEditorState != WorldEditorState::editing) {
        return;
    }
    this->_worldEditorState = WorldEditorState::loadingWorld;
    // Temporary way to force turning off key listener if a text input is in focus,
    // preventing key events to arrive correctly in the new active world.
    // TODO: dispatch DidBecomeActive and WillResignActive local events and use them in uikit to pause and resume text input listeners.
    if (this->_editorWorld != nullptr) {
        this->_editorWorld->setActiveKeyboard(KeyboardTypeNone);
    }
    if (_loadedWorld != nullptr) {
        _loadedWorld->reload();
    } else if (_loadingWorld != nullptr) {
        _loadingWorld->reload();
    }
    vx::NotificationCenter::shared().notify(vx::NotificationName::loadingGameStateChanged);
}

void GameCoordinator::worldEditorStop() {
    if (this->_worldEditorState != WorldEditorState::testingWorld) {
        return;
    }
    this->_worldEditorState = WorldEditorState::editing;

    if (this->_editorWorld != nullptr) {
        lua_State *L = this->_editorWorld->getLuaState();
        if (L != nullptr) {
            MENU_CALL_SYSTEM_HIDE_LOADING(L)
        }
    }

    // Temporary way to force turning off key listener if a text input is in focus,
    // preventing key events to arrive correctly in the new active world.
    // TODO: dispatch DidBecomeActive and WillResignActive local events and use them in uikit to pause and resume text input listeners.
    if (this->_loadedWorld != nullptr) {
        this->_loadedWorld->setActiveKeyboard(KeyboardTypeNone);
    }
    vx::NotificationCenter::shared().notify(vx::NotificationName::loadingGameStateChanged);
}

void GameCoordinator::tickForCI(double dt) {
    vx::OperationQueue::getMain()->callFirstDispatchedBlocks(10);
    tick(dt);
}

void GameCoordinator::setEnvironmentToLaunch(const std::unordered_map<std::string,std::string>& environment) {
    _envToLaunch = environment;
}

void GameCoordinator::_startupLoading() {
    vx::OperationQueue::getMain()->dispatch([this]() {
        if (_loadedWorld == nullptr && _loadingWorld == nullptr) {

            std::string worldID = "";
            
            // IF `script` is provided
            // IF script is a github.com URL:
            std::string githubRepo = "";
            // can be a file (ends with .lua) or directory
            // if empty, looking for .lua file in root directory
            std::string githubPath = "";
            // looking branch by ref if non-empty
            std::string githubRef = "";

            std::string huggingFaceSpace = "";
            std::string huggingFaceRef = "";

            if (_envToLaunch.empty() == false) {
                auto entry = _envToLaunch.find(GameCoordinator::ENV_FLAG_WORLD_ID);
                if (entry != _envToLaunch.end()) {
                    worldID = entry->second;
                }

                entry = _envToLaunch.find(GameCoordinator::ENV_FLAG_SCRIPT);
                if (entry != _envToLaunch.end()) {
                    std::string scriptOrigin = entry->second;
                    std::vector<std::string> elements = vx::str::splitString(scriptOrigin, "/");

                    if (elements.size() > 0) {
                        if (elements[0] == "github.com" && elements.size() >= 3) {
                            // extract ref if provided
                            std::vector<std::string> lastElementAndRef = vx::str::splitString(elements[elements.size() - 1], ":");
                            if (lastElementAndRef.size() > 1) {
                                elements[elements.size() - 1] = lastElementAndRef[0];
                                githubRef = lastElementAndRef[1];
                            }
                            if (elements.size() >= 3) {
                                githubRepo = elements[1] + "/" + elements[2];
                            }
                            for (size_t i = 3; i < elements.size(); ++i) {
                                if (githubPath != "") {
                                    githubPath += "/";
                                }
                                githubPath += elements[i];
                            }
                        } else if (elements[0] == "huggingface.co" && elements.size() >= 4) {
                            // extract ref?
                            if (elements.size() >= 4) {
                                huggingFaceSpace = elements[2] + "/" + elements[3];
                            }
                            // no path with HF (yet?)
                        }
                    }
                }
            }

            if (githubRepo != "") {

                this->loadWorld(GameMode_Client,
                                nullptr, // don't provide game data
                                WorldFetchInfo::makeWithGithubInfo(githubRepo, githubPath, githubRef),
                                "", // server address
                                false, // dev mode
                                false, // not author
                                std::vector<std::string>(),
                                _envToLaunch); // no env

            } else if (huggingFaceSpace != "") {

                this->loadWorld(GameMode_Client,
                                nullptr, // don't provide game data
                                WorldFetchInfo::makeWithHuggingFaceInfo(huggingFaceSpace, huggingFaceRef),
                                "", // server address
                                false, // dev mode
                                false, // not author
                                std::vector<std::string>(),
                                _envToLaunch); // no env

            } else if (worldID != "") {
                this->quickGameJoin(worldID, false, false, false);
            } else {
                this->loadHome("", false, false, std::unordered_map<std::string, std::string>());
            }
        }
    });
}

void GameCoordinator::logout(bool quit) {
    vx::HubClient::getInstance().logout();

    // clear account
    Account::active().reset();

    this->_resetMainMenuStates();

    vx::notification::setNeedsToPushToken(true);

    if (quit) {
        exit(EXIT_SUCCESS);
    } else {
        this->loadHome("", false, false, std::unordered_map<std::string, std::string>());
    }
}

void GameCoordinator::showPopup(Popup_SharedPtr popup) {
    this->_popupToShow = popup;
    // just in case, no need to keep a "Loading..." label when a popup is shown
    vx::NotificationCenter::shared().notify(vx::NotificationName::hideLoadingLabel);
}

Popup_SharedPtr GameCoordinator::popPopupToShow() {
    Popup_SharedPtr result = this->_popupToShow;
    if (this->_popupToShow != nullptr) {
        this->_popupToShow = nullptr;
    }
    return result;
}

bool GameCoordinator::isPopupShown() {
    return Popup::getNbPopups() > 0;
}

void GameCoordinator::didReceiveNotification(const Notification& notification) {
    switch (notification.name) {
        case NotificationName::didResize:
        {
            Game_SharedPtr activeGame = this->getActiveGame();
            if (activeGame != nullptr) {
                activeGame->sendScreenDidResize(static_cast<float>(Screen::widthInPoints), static_cast<float>(Screen::heightInPoints));
            }
            break;
        }
        default:
            break;
    }
}
