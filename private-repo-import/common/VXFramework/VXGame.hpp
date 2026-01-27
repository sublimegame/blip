//
//  VXGame.hpp
//  Cubzh
//
//  Created by Adrien Duermael on 21/05/2021.
//

#pragma once

// C++
#include <memory>
#include <deque>
#include <queue>
#include <unordered_set>

// Cubzh
#include "VXCodeEditor.hpp"
#include "VXGameClient.hpp"
#include "VXGameServer.hpp"
#include "VXHubClient.hpp"
#include "VXHubRequestSender.hpp"
#include "VXKeyboard.hpp"
#include "VXLuaExecutionLimiter.hpp"

// engine
#include "game.h"
#include "weakptr.h"
#include "inputs.h"
#include "textinput.hpp"
#include "filo_list_uint32.h"

// xptools
#include "WSConnection.hpp"

typedef struct lua_State lua_State;

namespace vx {

class Game;
typedef std::shared_ptr<Game> Game_SharedPtr;
typedef std::weak_ptr<Game> Game_WeakPtr;

class Node;
typedef std::shared_ptr<Node> Node_SharedPtr;

class RootNodeCodeEditor;
typedef std::shared_ptr<RootNodeCodeEditor> RootNodeCodeEditor_SharedPtr;

class GameRendererState;
typedef std::shared_ptr<GameRendererState> GameRendererState_SharedPtr;
typedef std::weak_ptr<GameRendererState> GameRendererState_WeakPtr;

// A Games holds a UserDataStore for each type of Luau userdata types.
class UserDataStore final {
public:
    UserDataStore() {
        _active = nullptr;
        _count = 0;
        _removed = filo_list_uint32_new();
    }

    ~UserDataStore() {
        filo_list_uint32_free(_removed);
    }

    FiloListUInt32 *getRemovedIDs() { return _removed; }

    void *getFirst() { return _active; }

    // Remove function should be called after the userdata node is done
    // removing itself from the list, connecting previous and next nodes.
    // Userdata entries are malloced C structures, freed by Luau,
    // it's not ideal but can't use C++ types / inheritance in this context.
    void remove(void *ud, void *next, uint32_t id) {
        if (_active == ud) {
            _active = next;
        }
        --_count;
        filo_list_uint32_push(_removed, id);
    }

    // In some cases, IDs participate in an other lifecycle.
    // Like in the case of Objects/tramsforms. Even though an Object
    // loses its last reference in Lua doesn't mean its associated
    // Lua data should be cleaned up right away as the corresponding
    // transform might still exist in the scene hierarchy.
    void removeWithoutKeepingID(void *ud, void *next) {
        if (_active == ud) {
            _active = next;
        }
        --_count;
    }

    // added userdata becomes the new _active reference
    // while `next` is returned and caller is supposed to make the connection
    // (ud->next = next, next->previous = ud)
    void* add(void *ud) {
        void *next = _active;
        _active = ud;
        ++_count;
        return next;
    }

    size_t count() {
        return _count;
    }

private:
    void *_active; // userdata entries stored as doubly linked list for fast removal
    // Each userdata entry is supposed to have an id.
    // Ids listed here are from freed userdata entries, they can be accessed
    // to cleanup associated Lua metadatas before the list is flushed.
    FiloListUInt32 *_removed = nullptr;
    size_t _count;
};

///
class ConsoleMessage final {
public:
    typedef enum {
        debug = 0,
        info = 1,
        warning = 2,
        error = 3,
        severe = 4,
        chat = 5, // for chat messages
    } Level;
    
    ConsoleMessage(const uint64_t& time,
                   const Level& level,
                   const std::string& sender, // empty means "system"
                   const std::string& message);

    uint32_t time;
    Level level;
    std::string sender;
    std::string message;
private:
};

class GameData;
typedef std::shared_ptr<GameData> GameData_SharedPtr;
typedef std::weak_ptr<GameData> GameData_WeakPtr;

class GameData final {
public:
    static GameData_SharedPtr make(const std::string& script,
                                   const std::string& mapBase64,
                                   const std::string& map3zh,
                                   const int maxPlayers) {
        GameData_SharedPtr g(new GameData);
        g->_script = script;
        g->_mapBase64 = mapBase64;
        g->_map3zh = map3zh;
        g->_maxPlayers = maxPlayers;
        return g;
    }
    
    inline void setScript(const std::string &str) { _script.assign(str); }
    inline std::string& getScript() { return _script; }
    
    inline std::string& getMapBase64() { return _mapBase64; }
    
    inline int getMaxPlayers() {
        return _maxPlayers;
    }
    
private:
    GameData() :
    _script(""),
    _mapBase64(""),
    _map3zh(""),
    _maxPlayers(1) {}

    std::string _script;
    std::string _mapBase64;
    std::string _map3zh;
    int _maxPlayers;
};

class WorldFetchInfo;
typedef std::shared_ptr<WorldFetchInfo> WorldFetchInfo_SharedPtr;
typedef std::weak_ptr<WorldFetchInfo> WorldFetchInfo_WeakPtr;

class WorldFetchInfo final {
public:
    static WorldFetchInfo_SharedPtr makeWithID(const std::string& worldID) {
        WorldFetchInfo_SharedPtr fetchInfo(new WorldFetchInfo);
        fetchInfo->_worldID = worldID;
        return fetchInfo;
    }

    static WorldFetchInfo_SharedPtr makeWithGithubInfo(const std::string& repo,
                                                       const std::string& path,
                                                       const std::string& ref) {
        WorldFetchInfo_SharedPtr fetchInfo(new WorldFetchInfo);
        fetchInfo->_githubRepo = repo;
        fetchInfo->_githubPath = path;
        fetchInfo->_githubRef = ref;
        return fetchInfo;
    }

    static WorldFetchInfo_SharedPtr makeWithHuggingFaceInfo(const std::string& space,
                                                            const std::string& ref) {
        WorldFetchInfo_SharedPtr fetchInfo(new WorldFetchInfo);
        fetchInfo->_huggingFaceSpace = space;
        fetchInfo->_huggingFaceRef = ref;
        return fetchInfo;
    }

    inline std::string& getWorldID() { return _worldID; }

    inline std::string getScriptOrigin() {
        std::string scriptOrigin = "";
        if (_githubRepo.empty() == false) {
            scriptOrigin = "github.com/" + _githubRepo;
            if (_githubPath.empty() == false) {
                scriptOrigin += "/" + _githubPath;
            }
            if (_githubRef.empty() == false) {
                scriptOrigin += ":" + _githubRef;
            }
        }
        if (_huggingFaceSpace.empty() == false) {
            scriptOrigin = "huggingface.co/spaces/" + _huggingFaceSpace;
            if (_huggingFaceRef.empty() == false) {
                // TODO
            }
        }
        return scriptOrigin;
    }

    inline std::string& getGithubRepo() { return _githubRepo; }
    inline std::string& getGithubPath() { return _githubPath; }
    inline std::string& getGithubRef() { return _githubRef; }
    
    inline std::string& getHuggingFaceSpace() { return _huggingFaceSpace; }
    inline std::string& getHuggingFaceRef() { return _huggingFaceRef; }

    inline bool isValid() { return (_worldID.empty() == false || _githubRepo.empty() == false || _huggingFaceSpace.empty() == false); }

private:
    WorldFetchInfo() :
    _worldID(""),
    _githubRepo(""),
    _githubPath(""),
    _githubRef(""),
    _huggingFaceSpace(""),
    _huggingFaceRef("") {}

    std::string _worldID;

    std::string _githubRepo;
    std::string _githubPath;
    std::string _githubRef;
    std::string _huggingFaceSpace;
    std::string _huggingFaceRef;
};


// Game is a wrapper for the C Game structure
// It holds extra attributes that only are useful at the C++ level.
class Game final : public HubRequestSender, public GameClientDelegate, public ConnectionDelegate {

public:
    
    // Describes a high level state change for the game
    enum StateChange {
        none,
        loading,
        running,
        failed,
        minorChange // minor internal game state change
    };
    
    enum State {
        state_none,
        created,
        downloadingWorldData,
        downloadedWorldData,
        loadingWorldData,
        failedToLoadWorldData, // a single `failed` state could suffice
        loadedWorldData,
        startLoadingModules,
        loadingModules, // englobes download + load (one module may reference other modules)
        loadedModules,
        startDownloadingOtherDependencies,
        downloadingOtherDependencies,
        downloadedOtherDependencies,
        starting,
        started, // can go back to `downloadingWorldData` when receiving new script
        exited,
    };
    
    enum ConnectionState {
        connectionState_not_required,
        connectionState_connecting,
        connectionState_failed,
    };
    
    /**
     * Factory method.
     * games can only be manipulated as shared pointers.
     * Makes it easier because they can be referenced in different places.
     */
    static Game_SharedPtr make(GameMode gameMode,
                               const WorldFetchInfo_SharedPtr& fetchInfo,
                               const GameData_SharedPtr& data,
                               const std::string& serverAddr,
                               const bool gameDevMode,
                               const bool isAuthor,
                               std::vector<std::string> itemsToLoad);

    /// Destructor
    ~Game() override;
    
    //
    StateChange tick(const double dt);
    void tickUntilLoaded();

    // returns C Game pointer
    CGame *getCGame();
    
    // Returns current game state (from _cgame)
    inline State getState() { return _state; }

    inline int getDataLength() { return _dataLength; }
    inline void setDataLength( int l ) { _dataLength = l; }

    inline int getDataLoaded() { return _dataLoaded; }
    inline void setDataLoaded( int l ) { _dataLoaded = l; }

    inline size_t getModulesLoaded() { return _nbModulesLoaded; }
    inline size_t getModulesToLoad() { return _nbModulesToLoad; }
    inline size_t getModulesFailed() { return _nbModulesFailed; }

    //
    GameData_SharedPtr getData();

    //
    void addOrUpdateLocalPlayer(const uint8_t *playerID,
                                const std::string *username,
                                const std::string *userid);
    
    //
    void addPlayer(const uint8_t playerID,
                   const std::string& username,
                   const std::string& userid,
                   const bool isLocalPlayer);
    
    // Calls OnPlayerLeave for player then removes is from Players table
    // When targetting local Player, OnPlayerLeave event is sent,
    // but Player remains in Players table.
    void removePlayer(const uint8_t playerID);
    
    // Calls OnPlayerLeave for all Players
    // and remove them all except for the local one.
    void removeAllPlayers();
    
    // Starts connection process
    // Requesting server or using provided address.
    void connect();
    
    //
    void sendReloadWorldMessage();
    
    // Creates a new instance of the same World
    // replacing active one when loaded
    void reload();
    void setNeedsReload(bool b);
    bool needsReload();

    //
    bool isSolo();
    
    ///
    void sendMessage(const GameMessage_SharedPtr& msg);

    ///
    void sendReceivedEnvToLaunch();

    // Returns true if game sever is running in dev mode.
    bool serverIsInDevMode();
    
    // Returns if the local user is a contributor.
    bool localUserIsContributor();
    
    /// Returns the root Node displayed while playing
    Node_SharedPtr getRootNode();

    ///
    void setGlobalCallback(const std::string& name, int (*callback)(lua_State *));

    ///
    void enableHTTP();

    ///
    void doString(const std::string& str);
    
    ///
    void setFetchInfo(const WorldFetchInfo_SharedPtr& fetchInfo);
    
    ///
    const std::string& getID();
    
    const WorldFetchInfo_SharedPtr& getFetchInfo();

    ///
    bool localUserIsAuthor();
    
    ///
    void forceShowPointer(bool b);
    
    ///
    bool pointerForceShown();
    
    /// Returns a shared pointer, built from internal weak pointer
    Game_SharedPtr getSharedPtr();
    
    /// Returns internal weak pointer
    Game_WeakPtr getWeakPtr();
    
    /// returns false if thumbnail upload can't start
    bool startTakingAndUploadingThumbnail();
    
    /// NOTE: call startTakingAndUploadingThumbnail prior
    /// to uploadThumbnail to make sure it's ok to do it.
    void uploadThumbnail(void* data, size_t dataSize);
    
    ///
    void resetPreviousCommandIndex();
    
    ///
    void onStart();
    
    ///
    const std::string& getPreviousCommand();
    
    ///
    const std::string& getNextCommand();
    
    /// LocalEvents
    void sendKeyboardInput(uint32_t charCode, Input input, uint8_t modifiers, KeyState state);
    void sendPointerEvent(PointerID ID, PointerEventType type, float x, float y, float dx, float dy);
    void sendVirtualKeyboardShownEvent(float keyboardHeight);
    void sendVirtualKeyboardHiddenEvent();
    void sendScreenDidResize(const float width, const float height);
    void sendFailedToLoadWorld();
    void sendWorldRequested();

    void sendTextInputUpdate(const std::string& str, size_t cursorStart, size_t cursorEnd);
    void sendTextInputClose();
    void sendTextInputDone();
    void sendTextInputNext();

    void sendAppDidBecomeActive();
    void sendDidReceivePushNotification(const std::string& title,
                                        const std::string& body,
                                        const std::string& category,
                                        uint32_t badge);

    /// We're still using a few C++ menus, Lua menu needs to know when
    /// a CPP menu is active to capture some events.
    void sendCPPMenuStateChanged();
    
    /// 
    void setEnvironment(const std::unordered_map<std::string, std::string>& environment);
    void mergeEnvironment(const std::unordered_map<std::string, std::string>& environment);
    const std::unordered_map<std::string, std::string>& getEnvironment() const;
    void appendToEnvironment(const std::string& key, const std::string& value);
    
    // Loads world data
    bool loadWorldData();

    ///
    Channel<GameMessage_SharedPtr>& getMessagesToSend();
    
    LuaExecutionLimiter& getLuaExecutionLimiter();
    
    const GameRendererState_SharedPtr& getGameRendererState() const;

    inline void setActiveKeyboard(KeyboardType t) {
        if (t == KeyboardTypeNone) {
            vx::textinput::textInputAction(TextInputAction_Close);
        } else {
            vx::textinput::textInputRequest("", 0, 0, false, TextInputKeyboardType_Default, TextInputReturnKeyType_Default, false);
        }
    }

    // --------------------
    // GameClientDelegate
    // --------------------
    
    void gameClientDidConnect() override;
    void gameClientDidReceivePayload(const Connection::Payload_SharedPtr& payload) override;
    void gameClientFailedToConnect() override;
    void gameClientLostConnection() override;
    void gameClientDidCloseConnection() override;
    
    lua_State* getLuaState();

    //
    bool isInServerMode();
    bool isInClientMode();

    //
    bool isConnected();

    //
    int getLocalPlayerID();

    // NOTE: we may not need all these data stores in the end.
    // When we'll be done making sure there are no memory leaks,
    // all types with no Lua metadata to cleanup after destruction
    // can be simplified.
    UserDataStore *userdataStoreForObjects;
    UserDataStore *userdataStoreForShapes;
    UserDataStore *userdataStoreForQuads;
    UserDataStore *userdataStoreForTexts;
    UserDataStore *userdataStoreForCameras;
    UserDataStore *userdataStoreForLights;
    UserDataStore *userdataStoreForBlocks;
    UserDataStore *userdataStoreForBlockProperties;
    UserDataStore *userdataStoreForPalettes;
    UserDataStore *userdataStoreForAudioSources;
    UserDataStore *userdataStoreForAssetsRequests;
    UserDataStore *userdataStoreForTimers;
    UserDataStore *userdataStoreForMeshes;

    // TMP: used for external editor connections
    void connectionDidEstablish(Connection& conn) override;
    void connectionDidReceive(Connection& conn, const Connection::Payload_SharedPtr& payload) override;
    void connectionDidClose(Connection& conn) override ;

private:
    /// Constructor
    Game();

    ///
    State _state;

    ///
    std::mutex _scheduledStatesMutex;
    
    ///
    std::queue<State> _scheduledStates;
    
    ///
    void _setState(State s);
    
    ///
    ConnectionState _connectionState;
    
    ///
    GameRendererState_SharedPtr _gameRendererState;
    
    // for things that need to be thread safe
    std::mutex _mutex;
    
    // game ID
    WorldFetchInfo_SharedPtr _fetchInfo;

    //
    GameMode _mode;
    
    // true if game launched by its author
    bool _isAuthor;
    
    // false by default, true when forcing pointer to be shown
    bool _forcePointerShown;
    
    // set when game explicitely asked one specific server
    // NOTE: will be replaced by a proper server link at some point
    std::string _requestedServerAddr;

    // server address
    // empty by default, until it server is found 
    // or set to _requestedServerAddr if not empty
    std::string _serverAddr;
    
    // World Data
    GameData_SharedPtr _data;
    int _dataLength;
    int _dataLoaded;

    // Extra items to load, not listed in World Data
    // TODO: get rid of this, currently used by item editor,
    // but item could be loaded within script.
    std::vector<std::string> _extraItemsToLoad;
    
    /// Indicates if gameserver is in dev mode
    bool _serverDevMode;
    
    /// true while uploading thumbnail
    /// to avoid uploading several thumbnails too quickly
    bool _uploadingThumbnail;
    
    // Sets bindings for CGame to be able to call functions defined in C++
    void _setCGameFunctionPointers();
    
    /// client for game's server
    /// Can be NULL, like when the game is itself a server.
    GameClient_SharedPtr _client;
    
    /// Only used when in server mode
    /// Because there's no _client, the _gameThreadFunction
    /// pulls out those messages to dispatch them.
    Channel<GameMessage_SharedPtr> _messagesToSend;
    
    /// Underlying C game structure.
    CGame *_cgame;
    
    /// not NULL when the game is connected to a local server
    GameServer *_localGameServer;
    
    /// game's root node
    Node_SharedPtr _rootNode;
    
    // Previous command cursor.
    // 0 when not active (default value).
    // Previous commands are stored by
    // the Game Coordinator.
    size_t _previousCommandIndex;
    
    /// Used to return string& to non local temporary object
    std::string _emptyString;
    
    ///
    Game_WeakPtr _weakSelf;
    
    ///
    HttpRequest_SharedPtr _worldDataRequest;

    /// Stores all requests for dependencies
    /// This step will certainly be removed when we'll
    /// be able to rely solely on World data bundles.
    /// But still used currently to load items listed in Config.
    size_t _nbDepsToLoad;
    size_t _nbDepsLoaded;
    std::deque<HttpRequest_SharedPtr> _depRequests;
    std::mutex _depRequestsMutex;

    size_t _nbModulesToLoad;
    size_t _nbModulesLoaded;
    size_t _nbModulesFailed;
    std::deque<HttpRequest_SharedPtr> _moduleRequests;
    std::mutex _moduleRequestsMutex;

    void _init(const Game_SharedPtr& ref,
               GameMode gameMode,
               const WorldFetchInfo_SharedPtr& fetchInfo,
               const GameData_SharedPtr& data,
               const std::string& serverAddr,
               const bool gameDevMode,
               const bool isAuthor,
               std::vector<std::string> itemsToLoad);
    
    HubClientRequest_SharedPtr _lookForServerRequest;
    
    void _lookForServer();
    
    void _connect();
    
    // ------------------
    // FUNCTION VARIABLES
    // A bunch of variables used within functions.
    // Should not rely on what's in them when the
    // function start.
    // It's better to have them here compared to static
    // declarations. Because 2 games could end up using
    // them in parallel.
    // It's safer to scope this to each Game instance.
    // ------------------
    
    GameMessage_SharedPtr _func_msg;
    std::string _func_userID;
    std::string _func_userName;
    std::string _func_senderName;
    std::string _func_msgText;
    
    ///
    vx::LuaExecutionLimiter _luaExecutionLimiter;

    /// Delay until next ram check, in seconds
    double _ramCheckTimerSec;

    ///
    std::unordered_map<std::string, std::string> _environment;
    
    /// World's Lua state of the sandbox.
    /// (weak reference)
    lua_State *_L;

    int _nbPlayers;
    bool _localPlayerAdded;
    uint8_t _localPlayerID;

    bool _needsReload;
    double _needsReloadRetryTimer;

    WSConnection_SharedPtr _editorConn;
};
}
