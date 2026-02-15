//
//  VXGameServer.hpp
//  Cubzh
//
//  Created by Gaetan de Villele on 2020-09-03.
//  Copyright © 2020 voxowl. All rights reserved.
//

#pragma once

// C++
#include <list>
#include <map>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>
#include <unordered_set>

// VX
#include "VXGameMessage.hpp"
#include "VXMessageQueues.hpp"
#include "VXConnectionHandler.hpp"

// xptools
#include "Connection.hpp"
#include "WSServer.hpp"

// forward declarations

typedef struct _CGame CGame;

namespace vx {

class HubPrivateClient;

class Game;
typedef std::shared_ptr<Game> Game_SharedPtr;

class GameData;
typedef std::shared_ptr<GameData> GameData_SharedPtr;

class PlayerCreationRequest;

///
struct GameServerMsg {
    uint8_t playerID;
    uint32_t msgType;
    std::string bytes;
};

class PlayerInfo final {
public:
    PlayerInfo(const uint8_t id,
               const std::string &userID,
               const std::string &userName) :
    userID(userID),
    userName(userName),
    id(id) {}
    
    std::string userID;
    std::string userName;
    uint8_t id;

    char pad[7];
};

///
class GameServer final : public WSServerDelegate {
    
public:
    
    /// Type of the GameServer.
    /// A GameServer can be local or online.
    enum class ServerType {
        Local,
        Online,
    };
    
    /// Mode of the game running.
    /// Dev  : clients can update the game script.
    /// Play : clients cannot update the game script, but only play.
    enum class Mode {
        Dev,
        Play,
    };
    
    /// The different states the GameServer can be in.
    enum class State {
        Boot, // launching
        LoadGame,
        Run,
        Invalid,
    };
    
    ///
    enum class PublishMsgType {
        ReloadWorld,    // game       -> background
        GameReady,      // background -> game
        Error,          // background -> game
    };
    
    /// use for dialog between Game and Loading threads
    typedef struct {
        PublishMsgType type;
        Game_SharedPtr cppGame;
    } PublishMsg;
    
    /// type of connection between game client and server
    enum class ConnectionType {
        Player,
        // Spectator
    };
    
#ifndef CLIENT_ENV
    /// Description
    /// @param listeningPort ...
    /// @param secure indicates whether the GameServer uses secure transport
    /// @param mode ...
    /// @param gameID ...
    static GameServer *newServer(const uint16_t& listeningPort,
                                 const bool& secure,
                                 const Mode mode,
                                 const std::string& gameID,
                                 const std::string& serverID,
                                 const std::string& hubAuthToken,
                                 const std::string& hubPrivateAddr,
                                 const uint16_t& hubPrivatePort,
                                 const bool& hubPrivateSecure,
                                 const int& maxPlayers);
#endif
    
    /// Creates a local GameServer using the provided Lua script.
    static GameServer *newLocalServerWithScript(const GameData_SharedPtr& data,
                                                const std::string& hubPrivateAddr,
                                                const uint16_t& hubPrivatePort,
                                                const bool& hubPrivateSecure,
                                                const int& maxPlayers);
    
    ///
    static std::string modeToString(const Mode mode);
    
#ifndef CLIENT_ENV
    /// Constructor for online GameServer
    GameServer(// const std::string& listeningAddr, // LATER
               const uint16_t& listeningPort,
               const bool& secure,
               const Mode mode,
               const std::string& gameID,
               const std::string& serverID,
               const std::string& hubAuthToken,
               const std::string& hubPrivateAddr,
               const uint16_t& hubPrivatePort,
               const bool& hubPrivateSecure,
               const int& maxPlayers);
#endif
    
    /// Constructor for local GameServer using the provided script
    GameServer(const GameData_SharedPtr& data,
               const std::string& hubPrivateAddr,
               const uint16_t& hubPrivatePort,
               const bool& hubPrivateSecure,
               const int& maxPlayers);
    
    /// Destructor
    virtual ~GameServer() override;
    
    /// Starts the server, make it listen for client connections.
    void start();
    
    ///
    void stop();
    
    /// Called to ask for a connection to be removed
    void removeConnection(ConnectionHandler_SharedPtr connectionHandler);
    
    /// This is expected to be used only in RPC thread.
    bool popAvailablePlayerID(uint8_t &playerID);
    
    ///
    void pushPlayerCreationRequest(const uint8_t &playerID, const std::string &userID, const std::string &userName);
    
    ///
    void pushPlayerRemovalRequest(const uint8_t &playerID);
    
    /// Returns a new connection to the GameServer.
    /// (used for in-memory connections to local game servers)
    Connection_SharedPtr connectJoin();
        
    ///
    MessageQueues& getMessageQueues();
    
    // WSServerDelegate
    bool didEstablishNewConnection(Connection_SharedPtr newIncomingConn) override;
    
private:
    
    // private functions
    
    ///
    void _setupGameFunctionPointers(CGame * const g) const;
    
    ///
    ServerType _getServerType() const;
    
    ///
    void _sendMessageToAllConnections(const GameMessage_SharedPtr& msg);
    void _sendMessageToAllConnectionsButOne(const GameMessage_SharedPtr& msg, const uint8_t playerID);
    void _sendMessageToOneConnection(const GameMessage_SharedPtr& msg, const uint8_t playerID);
    
    ///
    void _sendMessageToAllConnectionsPlaying(const GameMessage_SharedPtr& msg);
    void _sendMessageToAllConnectionsButOnePlaying(const GameMessage_SharedPtr& msg, const uint8_t playerID);
    void _sendMessageToOneConnectionPlaying(const GameMessage_SharedPtr& msg, const uint8_t playerID);
    
    /// Indicates whether the GameServer is running locally with the Game,
    /// or online as a standalone server.
    ServerType _serverType;
        
    ///
    MessageQueues _messageQueues;
    
    /// The current state of the server.
    State _state;
    std::mutex _stateMutex;
    
    ///
    std::string _gameID;

    ///
    std::string _hubAuthToken;
    
#if defined(__VX_USE_LIBWEBSOCKETS)

    /// [only for online server]
    // std::string _listeningAddr; // LATER
    
    /// [only for online server]
    uint16_t _listeningPort;
    
    /// [only for online server]
    /// Used only in _gameThreadFunction.
    Mode _mode;
    
    /// [only for online server]
    std::string _serverID;
    
    /// [only for online server]
    WSServer* _wsServer;

#endif
    
    /// number of supported connexions
    int _maxPlayers;
    
    /// Lua script of the game
    GameData_SharedPtr _data;
    
    /// C game reference
    /// Note: a GameServer will certainly end up hosting several games
    Game_SharedPtr _cppGame;
    
    /// HubPrivate service client
    vx::HubPrivateClient *_hubPrivateClient;
    
    ///
    // GameServiceImpl _gameService;
        
    /// Game simulation thread
    std::thread _gameThread;
    void _gameThreadFunction();
    
    /// networking thread (handling connections)
    std::thread _networkingThread;
    void _networkingThreadFunction();
    
    /// new Game loading thread
    /// ------------------------------------------------------------------------
    std::thread _loadingThread;
    void _loadingThreadFunction();
    
    std::mutex _chanToLoadingThreadMutex;
    std::queue<PublishMsg*> _chanToLoadingThread;
    std::mutex _chanToGameThreadMutex;
    std::queue<PublishMsg*> _chanToGameThread;

    // Only used when a new game is published!
    // All clients are expected to re-connect,
    // so moving connections to from where they are to
    // _playersWaitingToBeRemoved.
    void _moveClientsToWaitingForRemoval();
    void _swapGame(Game_SharedPtr& newGame);
    
    /// connections handling function (server's main loop)
    void _handleConnections();
    
    // players --------------------------------
    
    /// player IDs available
    /// /!\ this should be used only in the RPC thread
    std::list<uint8_t> _playerIDsAvailable;
    std::mutex _playerIDsAvailableMutex;

    /// requests for player creation
    /// (shared between RPC and simulation threads)
    std::list<PlayerInfo*> _playerCreationRequest;
    std::mutex _playerCreationRequestMutex;
    
    /// list of players currently connected to the server,
    /// but not playing yet. They are downloading assets until they send their
    /// "game ready" message.
    /// This is used exclusively in the game thread.
    std::list<PlayerInfo*> _playersConnected;
    
    /// Almost playing, waiting for next tick to actually add the player
    /// This is used exclusively in the game thread.
    std::list<PlayerInfo*> _playersJoining;
    
    /// list of players currently playing
    /// (used only by simulation thread)
    std::list<PlayerInfo*> _playersPlaying;

    /// No removal request yet received for those players,
    /// but we know they're about to leave or re-connect.
    /// (happens during publish)
    /// Moving them back to `_playersConnected` would lead do undesired onJoin events.
    /// Removing them right away would trigger a warning when the connection is in fact lost.
    std::list<PlayerInfo*> _playersWaitingToBeRemoved;

    /// Players temporary Transform id into real server id correspondance table
    std::map<uint16_t, uint16_t> tmpIDToID[16];

    /// requests for player removal
    /// (shared between RPC and simulation threads)
    std::list<uint8_t> _playerRemovalRequests;
    std::mutex _playerRemovalRequestMutex;
    
    /// timer used to shutdown game server when not player
    /// is connected for a defined amount of time. (in milliseconds)
    int32_t _shutDownTimer;
    
    /// timer used to trigger server info updates for the hub.
    int32_t _hubUpdateTimer;
    
    /// list of players currently playing
    bool _shouldExit;
    std::mutex _shouldExitMutex;
    
    // client connections
    // they can represent connections for any kind of request
    // even short ones like "Ping".
    std::unordered_map<uint8_t, ConnectionHandler_SharedPtr> _connectionHandlers;
    std::mutex _connectionHandlersMutex;

    // used only in a single thread, no mutex needed
    std::unordered_set<ConnectionHandler_SharedPtr> _connectionsToRemove;
    std::mutex _connectionsToRemoveMutex;
};

} // namespace vx
