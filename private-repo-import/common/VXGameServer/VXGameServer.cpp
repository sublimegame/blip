//
//  VXGameServer.cpp
//  Cubzh
//
//  Created by Gaetan de Villele on 2020-09-03.
//  Copyright © 2020 voxowl. All rights reserved.
//

#include "VXGameServer.hpp"

// C++
#include <chrono>

// xptools
#include "xptools.h"
#include "LocalConnection.hpp"

// lua sandbox
#include "scripting_server.hpp"
#include "lua_server.hpp"
#include "scripting.hpp"
#include "lua_event.hpp"

// vx
#include "VXConnectionHandlerJoin.hpp"
#include "VXGameRenderer.hpp"
#include "VXHubPrivateClient.hpp"
#include "VXGame.hpp"
#include "VXGameMessage.hpp"
#include "VXMessageQueues.hpp"
#include "VXResource.hpp"
#include "VXNetworkingUtils.hpp"
#include "gameserver_config.h"

// engine
#include "cache.h"
#include "game_config.h"

// xptools
#include "OperationQueue.hpp"
#include "ThreadManager.hpp"

using namespace vx;

// --------------------------------------------------
//
// MARK: - Static functions -
//
// --------------------------------------------------

#ifndef CLIENT_ENV
GameServer *GameServer::newServer(const uint16_t& listeningPort,
                                  const bool& secure,
                                  const Mode mode,
                                  const std::string& gameID,
                                  const std::string& serverID,
                                  const std::string& hubAuthToken,
                                  const std::string& hubPrivateAddr,
                                  const uint16_t& hubPrivatePort,
                                  const bool& hubPrivateSecure,
                                  const int& maxPlayers) {
    return new GameServer(listeningPort,
                          secure,
                          mode,
                          gameID,
                          serverID,
                          hubAuthToken,
                          hubPrivateAddr,
                          hubPrivatePort,
                          hubPrivateSecure,
                          maxPlayers);
}
#endif

GameServer *GameServer::newLocalServerWithScript(const GameData_SharedPtr &data,
                                                 const std::string& hubPrivateAddr,
                                                 const uint16_t& hubPrivatePort,
                                                 const bool& hubPrivateSecure,
                                                 const int& maxPlayers) {
    return new GameServer(data,
                          hubPrivateAddr,
                          hubPrivatePort,
                          hubPrivateSecure,
                          maxPlayers);
}

std::string GameServer::modeToString(const Mode mode) {
    switch (mode) {
        case Mode::Play:
            return "play";
        case Mode::Dev:
            return "dev";
    }
    // The CI complains without this return, saying function should return
    // something, but all cases are handled
    return "play";
}

// --------------------------------------------------
//
// MARK: - Public Functions -
//
// --------------------------------------------------

#ifndef CLIENT_ENV
// Constructor for standalone GameServer, listening on a given port for
// incoming connections
GameServer::GameServer(const uint16_t& listeningPort,
                       const bool& secure,
                       const Mode mode,
                       const std::string& gameID,
                       const std::string& serverID,
                       const std::string& hubAuthToken,
                       const std::string& hubPrivateAddr,
                       const uint16_t& hubPrivatePort,
                       const bool& hubPrivateSecure,
                       const int& maxPlayers) :
_serverType(ServerType::Online),
_messageQueues(),
_state(State::Boot),
_stateMutex(),
_gameID(gameID),
_hubAuthToken(hubAuthToken),
_listeningPort(listeningPort),
_mode(mode),
_serverID(serverID),
_wsServer(nullptr),
_maxPlayers(maxPlayers),
_cppGame(nullptr),
_gameThread(),
_networkingThread(),
_loadingThread(),
_chanToLoadingThreadMutex(),
_chanToLoadingThread(),
_chanToGameThreadMutex(),
_chanToGameThread(),
_playerIDsAvailable(),
_playerIDsAvailableMutex(),
_playerCreationRequest(),
_playerCreationRequestMutex(),
_playersConnected(),
_playersPlaying(),
_playersWaitingToBeRemoved(),
_playerRemovalRequests(),
_playerRemovalRequestMutex(),
_shutDownTimer(SHUTDOWN_DELAY),
_hubUpdateTimer(HUB_UPDATE_DELAY),
_shouldExit(false),
_connectionHandlers(),
_connectionHandlersMutex(),
_connectionsToRemove(),
_connectionsToRemoveMutex() {
    
    vx_assert(_maxPlayers <= GAME_PLAYER_COUNT_MAX);
    
    // create websocket server
    _wsServer = new WSServer(listeningPort, secure, "", "");
    _wsServer->setDelegate(this);
    
    _cppGame = vx::Game::make(GameMode_Server,
                              vx::WorldFetchInfo::makeWithID(gameID),
                              nullptr, // no data
                              "",
                              mode == Mode::Dev,
                              false,
                              std::vector<std::string>());

    // create a client for using the "Hub Private" API
    _hubPrivateClient = new HubPrivateClient(hubPrivateAddr,
                                             hubPrivatePort,
                                             hubPrivateSecure);

    // make all player IDs available
    {
        const std::lock_guard<std::mutex> locker(_playerIDsAvailableMutex);
        for (uint8_t id = GAME_PLAYERID_MIN; id < _maxPlayers; id++) {
            _playerIDsAvailable.push_back(id);
        }
        printf("INIT _playerIDsAvailable:");
        for (uint8_t i : _playerIDsAvailable) {
            printf(" %d", i);
        }
        printf("\n");
    }
}
#endif

// creates a local GameServer with a Lua script
GameServer::GameServer(const GameData_SharedPtr& data,
                       const std::string& hubPrivateAddr,
                       const uint16_t& hubPrivatePort,
                       const bool& hubPrivateSecure,
                       const int& maxPlayers):
_serverType(ServerType::Local),
_messageQueues(),
_state(State::Boot),
_stateMutex(),
#ifndef CLIENT_ENV
_listeningPort(0), // not used for a local GameServer (maybe later)
_mode(Mode::Play),
_gameID(), // remains empty
_serverID(), // remains empty
_wsServer(nullptr),
#endif
_maxPlayers(maxPlayers),
_data(data),
_cppGame(nullptr),
_gameThread(),
_networkingThread(),
_loadingThread(),
_chanToLoadingThread(),
_chanToGameThread(),
_playerIDsAvailable(),
_playerIDsAvailableMutex(),
_playerCreationRequest(),
_playerCreationRequestMutex(),
_playersConnected(),
_playersPlaying(),
_playerRemovalRequests(),
_playerRemovalRequestMutex(),
_shutDownTimer(SHUTDOWN_DELAY),
_hubUpdateTimer(HUB_UPDATE_DELAY),
_shouldExit(false),
_connectionHandlers(),
_connectionHandlersMutex(),
_connectionsToRemove(),
_connectionsToRemoveMutex() {
    
    vx_assert(_maxPlayers <= GAME_PLAYER_COUNT_MAX);
    
    _cppGame = vx::Game::make(GameMode_Server, 
                              WorldFetchInfo::makeWithID(""),
                              data,
                              "",
                              false,
                              false,
                              std::vector<std::string>());

    // create a client for using the "Hub Private" API
    _hubPrivateClient = new HubPrivateClient(hubPrivateAddr,
                                             hubPrivatePort,
                                             hubPrivateSecure);
    
    // make all player IDs available
    {
        const std::lock_guard<std::mutex> locker(_playerIDsAvailableMutex);
        for (uint8_t id = GAME_PLAYERID_MIN; id < _maxPlayers; id++) {
            _playerIDsAvailable.push_back(id);
        }
        printf("INIT _playerIDsAvailable:");
        for (uint8_t i : _playerIDsAvailable) {
            printf(" %d", i);
        }
        printf("\n");
    }
}

GameServer::~GameServer() {
#ifndef CLIENT_ENV
    if (_wsServer != nullptr) {
        _wsServer->setDelegate(nullptr);
    }
#endif
}

void GameServer::start() {
    
    // NOTE: maybe _loadingThreadFunction should not even start when
    // in play mode. Users can't send new script in that mode.
    
    switch (_serverType) {
        case ServerType::Local: {
            vxlog_trace("📡 starting... (local server mode)");
            // local server
            this->_gameThread = std::thread(&GameServer::_gameThreadFunction, this);
            this->_loadingThread = std::thread(&GameServer::_loadingThreadFunction, this);
            this->_networkingThread = std::thread(&GameServer::_networkingThreadFunction, this);
            break;
        }
        case ServerType::Online: {
            vxlog_info("📡 starting... (online server mode)");
            
            // online server
            this->_gameThread = std::thread(&GameServer::_gameThreadFunction, this);
            this->_loadingThread = std::thread(&GameServer::_loadingThreadFunction, this);
            this->_networkingThreadFunction(); // keep gRPC handling in main thread
            
            // stop server gracefully
            this->stop();
            break;
        }
    }
}

void GameServer::stop() {
    switch (_serverType) {
        case ServerType::Local: {
            vxlog_trace("📡 stopping... (local server mode)");
            // local server
            {
                const std::lock_guard<std::mutex> locker(_shouldExitMutex);
                _shouldExit = true;
            }

            _gameThread.join();
            vxlog_trace("📡 game thread stopped");
            _loadingThread.join();
            vxlog_trace("📡 loading thread stopped");
            _networkingThread.join();
            vxlog_trace("📡 networking thread stopped");
            
            break;
        }
        case ServerType::Online: {
            vxlog_trace("📡 stopping... (online server mode)");
            // online server
            {
                const std::lock_guard<std::mutex> locker(_shouldExitMutex);
                _shouldExit = true;
            }

            _gameThread.join();
            vxlog_trace("📡 game thread stopped");
            _loadingThread.join();
            vxlog_trace("📡 loading thread stopped");
            
            break;
        }
    }
}

void GameServer::removeConnection(ConnectionHandler_SharedPtr handler) {
    vxlog_trace("✂️ remove connection: %p", handler.get());
    if (handler == nullptr) {
        vxlog_error("connection:  is null");
        return;
    }
    vx_assert(handler->isConnectionClosed());
    
    // downcast into ConnectionHandlerJoin
    ConnectionHandlerJoin_SharedPtr handlerJoin = std::dynamic_pointer_cast<ConnectionHandlerJoin>(handler);
    if (handlerJoin != nullptr) {
        {
            std::lock_guard<std::mutex> lock(_connectionsToRemoveMutex);
            _connectionsToRemove.insert(handlerJoin);
        }
        _connectionHandlers.erase(handlerJoin->getPlayerID());
    } else {
        vxlog_error("[GameServer::removeConnection] unsupported cast");
        vx_assert(false);
    }
}

// This is expected to be used only in RPC thread.
bool GameServer::popAvailablePlayerID(uint8_t &playerID) {
    const std::lock_guard<std::mutex> locker(_playerIDsAvailableMutex);
    if (_playerIDsAvailable.empty()) {
        vxlog_warning("_playerIDsAvailable is empty!");
        return false; // failure : no ID is available
    } else {
        playerID = _playerIDsAvailable.front();
        _playerIDsAvailable.pop_front();
        printf("POP %d _playerIDsAvailable:", playerID);
        for (uint8_t i : _playerIDsAvailable) {
            printf(" %d", i);
        }
        printf("\n");
        return true; // success
    }
}

void GameServer::pushPlayerCreationRequest(const uint8_t &playerID, const std::string &userID, const std::string &userName) {
    const std::lock_guard<std::mutex> locker(_playerCreationRequestMutex);
    _playerCreationRequest.push_back(new PlayerInfo(playerID, userID, userName));
}

void GameServer::pushPlayerRemovalRequest(const uint8_t &playerID) {
    bool removedFromCreationRequests = false;
    
    // could still a creation request
    // if so, remove it and skip _playerRemovalRequests insertion
    {
        const std::lock_guard<std::mutex> locker(_playerCreationRequestMutex);
        
        for (std::list<PlayerInfo*>::iterator it = _playerCreationRequest.begin(); it != _playerCreationRequest.end();) {
            // find in playing players, remove from here
            if ((*it)->id == playerID) {
                removedFromCreationRequests = true;
                delete (*it);
                it = _playerCreationRequest.erase(it);
                vxlog_warning("🧹 player removed with status CREATION REQUEST: %d", playerID);
            } else {
                ++it;
            }
        }
    }
    
    if (removedFromCreationRequests == false) {
        const std::lock_guard<std::mutex> locker(_playerRemovalRequestMutex);
        _playerRemovalRequests.push_back(playerID);
    }
}

MessageQueues& GameServer::getMessageQueues() {
    return _messageQueues;
}

// --------------------------------------------------
// Receiving an incoming connection as Local GameServer
// --------------------------------------------------

// only available in local GameServer
Connection_SharedPtr GameServer::connectJoin() {
    vx_assert(this->_getServerType() == ServerType::Local);
    
    // allocate connection and handler
    LocalConnection_SharedPtr clientConn(new LocalConnection);
    LocalConnection_SharedPtr serverConn(new LocalConnection);
    
    // connect the 2 connections together
    clientConn->setPeerConnection(serverConn);
    serverConn->setPeerConnection(clientConn);
    
    // HACK : call delegate function directly for now
    this->didEstablishNewConnection(serverConn);
    
    return std::move(clientConn);
}

// --------------------------------------------------
// WSServer Delegate functions
// --------------------------------------------------

bool GameServer::didEstablishNewConnection(Connection_SharedPtr newConn) {
    // create new handler for this connection
    uint8_t playerID = PLAYER_ID_NONE;
    const bool ok = popAvailablePlayerID(playerID);
    vxlog_debug("GameServer::didEstablishNewConnection | playerID %d", playerID);
    if (ok) {
        ConnectionHandlerJoin_SharedPtr handler = ConnectionHandlerJoin::make(this, newConn, playerID);
        std::lock_guard<std::mutex> lock(_connectionHandlersMutex);
        // make sure there isn't already a connection handler for playerID
        if (_connectionHandlers.find(playerID) == _connectionHandlers.end()) {
            // not found
            _connectionHandlers.emplace(playerID, handler);
            return true;
        } else {
            vxlog_error("[GameServer::didEstablishNewConnection] there is already a handler for this player ID");
            // TODO: this is not supposed to happen, but maybe it's ok to consider server is full
        }
    } else {
        // TODO: set is full
    }
    return false;
}


// --------------------------------------------------
//
// MARK: - Private functions -
//
// --------------------------------------------------

void GameServer::_sendMessageToAllConnections(const GameMessage_SharedPtr& msg) {
    std::vector<ConnectionHandler_SharedPtr> handlers;
    
    {
        const std::lock_guard<std::mutex> locker(_connectionHandlersMutex);
        for (std::pair<uint8_t, ConnectionHandler_SharedPtr> e : _connectionHandlers) {
            handlers.push_back(e.second);
        }
    }
    
    for (ConnectionHandler_SharedPtr h : handlers) {
        Connection::Payload_SharedPtr p = msg->copyPayload();
        h->write(p);
    }
}

void GameServer::_sendMessageToAllConnectionsButOne(const GameMessage_SharedPtr& msg, const uint8_t playerID) {
    std::vector<ConnectionHandler_SharedPtr> handlers;
    
    {
        const std::lock_guard<std::mutex> locker(_connectionHandlersMutex);
        for (std::pair<uint8_t, ConnectionHandler_SharedPtr> e : _connectionHandlers) {
            if (e.first != playerID) {
                handlers.push_back(e.second);
            }
        }
    }
    
    for (ConnectionHandler_SharedPtr h : handlers) {
        Connection::Payload_SharedPtr p = msg->copyPayload();
        h->write(p);
    }
}

void GameServer::_sendMessageToOneConnection(const GameMessage_SharedPtr& msg, const uint8_t playerID) {
    ConnectionHandler_SharedPtr h = nullptr;
    
    {
        const std::lock_guard<std::mutex> locker(_connectionHandlersMutex);
        std::unordered_map<uint8_t, ConnectionHandler_SharedPtr>::iterator it = _connectionHandlers.find(playerID);
        if (it != _connectionHandlers.end()) {
            h = it->second;
        }
    }
    
    if (h == nullptr) {
        // handler not found
        return;
    }
    
    Connection::Payload_SharedPtr p = msg->unloadPayload();
    h->write(p);
}

void GameServer::_sendMessageToAllConnectionsPlaying(const GameMessage_SharedPtr& msg) {
    std::vector<ConnectionHandler_SharedPtr> handlers;
    
    {
        const std::lock_guard<std::mutex> locker(_connectionHandlersMutex);
        for (PlayerInfo *info : _playersPlaying) {
            uint8_t id = info->id;
            std::unordered_map<uint8_t, ConnectionHandler_SharedPtr>::iterator it = _connectionHandlers.find(id);
            if (it != _connectionHandlers.end()) {
                handlers.push_back(it->second);
            } else {
                // not found
                vxlog_error("[GameServer::_sendMessageToAllConnectionsPlaying] failed to retrieve handler");
            }
        }
    }
    
    for (ConnectionHandler_SharedPtr h : handlers) {
        Connection::Payload_SharedPtr p = msg->copyPayload();
        h->write(p);
    }
}

void GameServer::_sendMessageToAllConnectionsButOnePlaying(const GameMessage_SharedPtr& msg, const uint8_t playerID) {
    std::vector<ConnectionHandler_SharedPtr> handlers;
    
    {
        const std::lock_guard<std::mutex> locker(_connectionHandlersMutex);
        for (PlayerInfo *info : _playersPlaying) {
            uint8_t id = info->id;
            if (id != playerID) {
                std::unordered_map<uint8_t, ConnectionHandler_SharedPtr>::iterator it = _connectionHandlers.find(id);
                if (it != _connectionHandlers.end()) {
                    handlers.push_back(it->second);
                }
            }
        }
    }
    
    for (ConnectionHandler_SharedPtr h : handlers) {
        Connection::Payload_SharedPtr p = msg->copyPayload();
        h->write(p);
    }
}

void GameServer::_sendMessageToOneConnectionPlaying(const GameMessage_SharedPtr& msg, const uint8_t playerID) {
    ConnectionHandler_SharedPtr h = nullptr;
    
    {
        const std::lock_guard<std::mutex> locker(_connectionHandlersMutex);
        for (PlayerInfo *info : _playersPlaying) {
            
            uint8_t id = info->id;
            if (id == playerID) {
                std::unordered_map<uint8_t, ConnectionHandler_SharedPtr>::iterator it = _connectionHandlers.find(id);
                if (it != _connectionHandlers.end()) {
                    h = it->second;
                    break;
                }
            }
        }
    }
    
    if (h != nullptr) {
        Connection::Payload_SharedPtr p = msg->copyPayload();
        h->write(p);
    }
}

/// Game simulation is done here
void GameServer::_gameThreadFunction() {

    if (this->_getServerType() == ServerType::Online) {
        vx::ThreadManager::shared().setMainThread();
    }
    
    // NOTE: World servers do not download items defined in Config
    // (Config.Items, Config.Map)
    // Servers can use Object:Load for now.
    // Many servers don't need any assets, so it's best to start as fast as possible.
    
    uint32_t now = VXNetworkingUtils::getSteadyClockTimeMs();
    uint32_t lastNow = now;
    uint32_t nextTick = now + (FRAME_DURATION_MS - now % FRAME_DURATION_MS);
    uint32_t currentTick; // exact 50ms tick
    uint32_t elapsedMilliseconds = 0;

    uint8_t playerID;
    uint8_t id = 0;
    std::string userID;
    std::string userName;

    bool erased;
    
    std::string senderName;
    std::string msgContent;

    bool foundPlayerToRemove = false;
    bool exit = false;

    std::list<PlayerInfo*>::iterator it;
    std::list<uint8_t>::iterator itPlayerID;

    bool moveToNextFrame = false;

    GameMessage_SharedPtr msg_sharedPtr;
    uint8_t msg_type;

    PublishMsg *publishMsg = nullptr;

    vx::Game::State gameState;

    while (true) {
        {
            const std::lock_guard<std::mutex> locker(_shouldExitMutex);
            exit = this->_shouldExit;
        }
        if (exit) { break; }
        
        vx::OperationQueue::getServerMain()->callFirstDispatchedBlocks(10);

        now = VXNetworkingUtils::getSteadyClockTimeMs();

        moveToNextFrame = now >= nextTick;

        currentTick = now - (now % FRAME_DURATION_MS);

        // NOTE: Make sure not to use MessageQueues::popIn in wrong conditional blocks.
        // Messages not popped at the end of the loop are lost when the next one starts.

        // NOTE: we run simulation only if moving to next frame.
        if (moveToNextFrame) {

            nextTick = currentTick + FRAME_DURATION_MS;
            elapsedMilliseconds = currentTick - lastNow;
            lastNow = currentTick;

            if (this->_shutDownTimer > 0) {
                // Decrement _shutDownTimer and shut down it reaches 0.
                this->_shutDownTimer -= elapsedMilliseconds;
                // vxlog_info("_shutDownTimer: %d", this->_shutDownTimer);
                if (this->_shutDownTimer <= 0) {
                    vxlog_info("shuting down, %dms without connection", SHUTDOWN_DELAY);
                    break;
                }
            } else {
                // Init _shutDownTimer if no one is playing.
                if (_playersPlaying.empty() && _playersConnected.empty()) {
                    this->_shutDownTimer = SHUTDOWN_DELAY;
                }
            }

            _cppGame->tick(FRAME_DURATION_S);
        }

        gameState = _cppGame->getState();

        // get ready to pop messages from In queues without using mutexes
        _messageQueues.toggleIn();

        // ----------------------------------
        // Process Player removal requests.
        // ----------------------------------

        {
            std::list<uint8_t> playersToRemove;
            {
                // make copy, clear _playerRemovalRequests and release mutex
                const std::lock_guard<std::mutex> lockerPlayerRemovalRequest(_playerRemovalRequestMutex);
                playersToRemove = _playerRemovalRequests;
                _playerRemovalRequests.clear();
            }

            if (playersToRemove.empty() == false) {
                this->_hubUpdateTimer = 0.0f; // inform hub as soon as possible
            }

            itPlayerID = playersToRemove.begin();
            while (itPlayerID != playersToRemove.end()) {
                // indicates whether we managed to remove the player

                foundPlayerToRemove = false;

                playerID = *itPlayerID;

                // look in _playersConnected
                for (it = _playersConnected.begin(); it != _playersConnected.end();) {
                    // found in connected players, remove from here
                    if ((*it)->id == playerID) {
                        foundPlayerToRemove = true;
                        delete (*it);
                        it = _playersConnected.erase(it);
                        vxlog_info("🧹 player removed with status CONNECTED: %d", playerID);
                    } else {
                        ++it;
                    }
                }

                // look in _playersJoining
                if (foundPlayerToRemove == false) {
                    for (it = _playersJoining.begin(); it != _playersJoining.end();) {
                        // found in joining players, remove from here
                        if ((*it)->id == playerID) {
                            foundPlayerToRemove = true;
                            delete (*it);
                            it = _playersJoining.erase(it);
                            vxlog_info("🧹 player removed with status JOINING: %d", playerID);
                        } else {
                            ++it;
                        }
                    }
                }

                // look in _playersPlaying
                if (foundPlayerToRemove == false) {
                    for (std::list<PlayerInfo*>::iterator it = _playersPlaying.begin(); it != _playersPlaying.end();) {
                        // find in playing players, remove from here
                        if ((*it)->id == playerID) {
                            foundPlayerToRemove = true;
                            delete (*it);
                            it = _playersPlaying.erase(it);
                            vxlog_info("🧹 player removed with status PLAYING: %d", playerID);
                        } else {
                            ++it;
                        }
                    }
                }
                
                // look in _playersWaitingToBeRemoved
                bool userFromPreviousGame = false;
                if (foundPlayerToRemove == false) {
                    for (std::list<PlayerInfo*>::iterator it = _playersWaitingToBeRemoved.begin(); it != _playersWaitingToBeRemoved.end();) {
                        // find in playing players, remove from here
                        if ((*it)->id == playerID) {
                            foundPlayerToRemove = true;
                            userFromPreviousGame = true;
                            delete (*it);
                            it = _playersWaitingToBeRemoved.erase(it);
                            vxlog_info("🧹 player removed with status WAITING TO BE REMOVED: %d", playerID);
                        } else {
                            ++it;
                        }
                    }
                }

                if (foundPlayerToRemove == true) {
                    itPlayerID = playersToRemove.erase(itPlayerID);

                    if (userFromPreviousGame == false) {
                        // remove from game simulation
                        _cppGame->removePlayer(playerID);
                        vxlog_info("🧹 player removed: %d", playerID);

                        // notify all other players than a player has left
                        GameMessage_SharedPtr m = GameMessage::makePlayerLeft(playerID);
                        _sendMessageToAllConnectionsButOne(m, playerID);
                    }

                    // re-add player ID in pool of available IDs
                    {
                        const std::lock_guard<std::mutex> lockerPlayerIDsAvailable(_playerIDsAvailableMutex);
                        _playerIDsAvailable.push_back(playerID);
                        printf("PUSH %d _playerIDsAvailable:", playerID);
                        for (uint8_t i : _playerIDsAvailable) {
                            printf(" %d", i);
                        }
                        printf("\n");
                    }
                } else {
                    vxlog_warning("player could not be removed: %d, not found in any list", playerID);
                    ++itPlayerID;
                }
            } // player removal
        }
        
        // ----------------------------------
        // See if game has messages to send
        // ----------------------------------
        
        GameMessage_SharedPtr msg = nullptr;
        while (_cppGame->getMessagesToSend().pop(msg) == true) {
            uint8_t eventType;
            uint8_t senderID;
            std::vector<uint8_t> recipients;
            
            if (GameMessage::decodeGameEventMeta(msg, eventType, senderID, recipients)) {
                
                for (uint8_t r : recipients) {
                    if (r == PLAYER_ID_ALL) {
                        _sendMessageToAllConnectionsPlaying(msg);
                        
                    } else if (r == PLAYER_ID_ALL_BUT_SELF) {
                        _sendMessageToAllConnectionsButOnePlaying(msg, senderID);
                        
                    } else if (game_isRecipientIDValid(r)) {
                        // players (0-15)
                        _sendMessageToOneConnectionPlaying(msg, r);
                        
                    } else {
                        vxlog_error("this should not happen [%s:%d]", __FILE_NAME__, __LINE__);
                    }
                }
            }
        }
        
        // ----------------------------------
        // Process Player creation requests.
        // _playerCreationRequest -> _playersConnected
        // ----------------------------------

        {
            const std::lock_guard<std::mutex> locker(_playerCreationRequestMutex);
            while (_playerCreationRequest.empty() == false) {
                PlayerInfo *playerInfo = _playerCreationRequest.front();
                _playerCreationRequest.pop_front();
                _playersConnected.push_back(playerInfo);

                // approve handshake (TODO: challenge)
                GameMessage_SharedPtr m = GameMessage::makeHandshakeServerApprovalMsg(true);
                _sendMessageToOneConnection(m, playerInfo->id);
            }
        }

        // ----------------------------------
        // Loop over players connected,
        // see if they're ready to join.
        // _playersConnected -> _playersJoining
        // ----------------------------------

        if (gameState == vx::Game::State::started) {
            for (it = _playersConnected.begin(); it != _playersConnected.end();) {
                PlayerInfo *playerInfo = (*it);

                // NOTE: not waiting for message anymore, going straight to
                // _playersJoining. Maybe we could simplify these pools.

                // client is ready, we move it from "connected" to "joining"
                it = _playersConnected.erase(it);
                erased = true;
                _playersJoining.push_back(playerInfo);

                if (erased == false) {
                    it++;
                }
            }
        }

        // ----------------------------------
        // Loop over players joining,
        // they may now be ready to play.
        // _playersJoining -> _playersPlaying
        // ----------------------------------

        if (_playersJoining.empty() == false) {
            this->_hubUpdateTimer = 0.0f; // inform hub as soon as possible
        }
        
        for (it = _playersJoining.begin(); it != _playersJoining.end();) {
            PlayerInfo *playerInfo = (*it);

            it = _playersJoining.erase(it);
            erased = true;
            _playersPlaying.push_back(playerInfo);
            
            
            _cppGame->addPlayer(playerInfo->id,
                                playerInfo->userName,
                                playerInfo->userID,
                                false); // there are no local player on the gameserver
            scripting_onPlayerJoin(_cppGame, playerInfo->id);
            
            GameMessage_SharedPtr m = GameMessage::makePlayerJoined(playerInfo->id,
                                                                    playerInfo->userID,
                                                                    playerInfo->userName);
            
            // notify all players
            // (those connected or joining will receive an event 
            // for each player when moved to _playersPlaying)
            _sendMessageToAllConnectionsPlaying(m);

            // notify new player of the existing players in the game
            for (std::list<PlayerInfo*>::iterator it = _playersPlaying.begin(); it != _playersPlaying.end(); it++) {
                id = (*it)->id;
                if (id != playerInfo->id) {
                    GameMessage_SharedPtr m = GameMessage::makePlayerJoined(id,
                                                                            (*it)->userID,
                                                                            (*it)->userName);
                    _sendMessageToOneConnection(m, playerInfo->id);
                };
            }
            
            if (erased == false) {
                itPlayerID++;
            }
        }

        // ----------------------------------
        // Loop over players and process
        // messages they sent to the server.
        // ----------------------------------

        for (PlayerInfo *playerInfo : _playersPlaying) {

            uint8_t id = playerInfo->id;
            msg_sharedPtr = _messageQueues.popIn(id);

            while (msg_sharedPtr != nullptr) {
                
                msg_type = msg_sharedPtr->msgtype();

                if (msg_type == GameMessage::TYPE_RELOAD_WORLD) {
                    
                    vxlog_trace("world needs to be reloaded!");
   
#ifndef CLIENT_ENV
                    if (this->_mode != Mode::Dev) {
                        vxlog_warning("⚠️ server refuses to reload world as it's not in DEV mode.");
                    } else {
                        const std::lock_guard<std::mutex> locker(_chanToLoadingThreadMutex);
                        PublishMsg *newScriptMsg = new PublishMsg();
                        newScriptMsg->type = PublishMsgType::ReloadWorld;
                        newScriptMsg->cppGame = nullptr;
                        _chanToLoadingThread.push(newScriptMsg);
                    }
#endif
                } else if (msg_type == GameMessage::TYPE_CHAT_MESSAGE) {

                    uint8_t senderID = 0;
                    std::string senderUserID;
                    std::string senderUsername;
                    std::string localUUID;
                    std::string uuid;
                    std::vector<uint8_t> recipients;
                    std::string chatMessage;

                    bool ok = GameMessage::decodeChatMessage(msg_sharedPtr, 
                                                             senderID,
                                                             senderUserID,
                                                             senderUsername,
                                                             localUUID,
                                                             uuid,
                                                             recipients,
                                                             chatMessage);
                    if (ok) {
#if defined(CLIENT_ENV)
                        const std::string serverID = "";
#else
                        const std::string serverID = this->_serverID;
#endif

                        vx::Game_WeakPtr weakPtr = _cppGame->getWeakPtr();
                        this->_hubPrivateClient->postChatMessage(senderUserID, 
                                                                 recipients,
                                                                 chatMessage,
                                                                 serverID,
                                                                 _hubAuthToken,
                                                                 [weakPtr,
                                                                  recipients,
                                                                  msg_sharedPtr,
                                                                  senderID,
                                                                  localUUID,
                                                                  uuid,
                                                                  this](HttpRequest_SharedPtr req){
                            Game_SharedPtr game = weakPtr.lock();
                            if (game == nullptr) {
                                return;
                            }

                            HttpResponse& response = req->getResponse();
                            std::string allBytes;

                            const bool success = (response.getSuccess() &&
                                                  response.getStatusCode() == 200 &&
                                                  response.readAllBytes(allBytes));
                            if (success == false) {
                                vxlog_error("CHAT MESSAGE RESPONSE STATUS: %d", response.getStatusCode());
                                GameMessage_SharedPtr ack = GameMessage::makeChatMessageACK(localUUID, uuid, ChatMessageStatus::ERR);
                                OperationQueue::getServerMain()->dispatch([ack, senderID, this](){
                                    _sendMessageToOneConnection(ack, senderID);
                                });
                                return;
                            }

                            cJSON *json = cJSON_Parse(allBytes.c_str());

                            std::string msgUUID = uuid;
                            uint8_t status = 0;

                            if (vx::json::readStringField(json, "msgUUID", msgUUID, false) == false) {
                                vxlog_debug("JSON parsing failed. (msgUUID)");
                                cJSON_Delete(json);
                                GameMessage_SharedPtr ack = GameMessage::makeChatMessageACK(localUUID, uuid, ChatMessageStatus::ERR);
                                OperationQueue::getServerMain()->dispatch([ack, senderID, this](){
                                    _sendMessageToOneConnection(ack, senderID);
                                });
                                return;
                            }

                            if (vx::json::readUInt8Field(json, "status", status, false) == false) {
                                vxlog_debug("JSON parsing failed. (status)");
                                cJSON_Delete(json);
                                GameMessage_SharedPtr ack = GameMessage::makeChatMessageACK(localUUID, uuid, ChatMessageStatus::ERR);
                                OperationQueue::getServerMain()->dispatch([ack, senderID, this](){
                                    _sendMessageToOneConnection(ack, senderID);
                                });
                                return;
                            }

                            cJSON_Delete(json);
                            GameMessage::chatMessageAttachUUID(msg_sharedPtr, msgUUID);

                            GameMessage_SharedPtr ack = GameMessage::makeChatMessageACK(localUUID, msgUUID, status);
                            OperationQueue::getServerMain()->dispatch([ack, senderID, this](){
                                _sendMessageToOneConnection(ack, senderID);
                            });

                            // Only transmit message if OK
                            // TODO: transmit modified messages when reason is not too bad
                            if (status == ChatMessageStatus::OK) {
                                for (uint8_t id : recipients) {
                                    if (id == PLAYER_ID_SERVER) {
                                        // TODO: distribute chat message to server

                                    } else if (id == PLAYER_ID_ALL_BUT_SELF) {
                                        OperationQueue::getServerMain()->dispatch([msg_sharedPtr, senderID, this](){
                                            _sendMessageToAllConnectionsButOnePlaying(msg_sharedPtr, senderID);
                                        });
                                    } else if (id == PLAYER_ID_ALL) {
                                        OperationQueue::getServerMain()->dispatch([msg_sharedPtr, this](){
                                            _sendMessageToAllConnectionsPlaying(msg_sharedPtr);
                                        });
                                    } else {
                                        OperationQueue::getServerMain()->dispatch([msg_sharedPtr, id, this](){
                                            _sendMessageToOneConnection(msg_sharedPtr, id);
                                        });
                                    }
                                }
                            }
                        });
                    }

                } else if (msg_type == GameMessage::TYPE_GAME_EVENT ||
                           msg_type == GameMessage::TYPE_GAME_EVENT_SMALL ||
                           msg_type == GameMessage::TYPE_BIG_GAME_EVENT_BIG) {
                    
                    // received a Game Event from the client, let's decode it and
                    // call the Lua callback Server.DidReceiveEvent(event)
                    
                    msg_sharedPtr->step("decode game event (server)");
                    
                    uint8_t senderID = 0;
                    uint8_t eventType = 0;
                    std::vector<uint8_t> recipients;
                    
                    bool ok = GameMessage::decodeGameEventMeta(msg_sharedPtr, eventType, senderID, recipients);
                    if (ok) {
                        for (uint8_t id : recipients) {
                            if (id == PLAYER_ID_SERVER) {
                                // vxlog_info("📧 %d -> Server", senderID);
                                uint8_t *data = nullptr;
                                size_t size = 0;
                                
                                ok = GameMessage::decodeGameEventBytes(msg_sharedPtr, &data, &size);
                                if (ok) {
                                    // NOTE: data & recupientList are freed after DidReceiveEvent is called
                                    DoublyLinkedListUint8 *recipientList = doubly_linked_list_uint8_new();
                                    for (uint8_t recipient : recipients) {
                                        doubly_linked_list_uint8_push_back(recipientList, recipient);
                                    }
                                    scriptingServer_didReceiveEvent(_cppGame, eventType, senderID, recipientList, data, size);
                                    doubly_linked_list_uint8_free(recipientList);
                                }
                                
                                if (data != nullptr) {
                                    free(data);
                                }
                                
                            } else if (id == PLAYER_ID_ALL_BUT_SELF) {
                                // vxlog_info("📧 %d -> OtherPlayers", senderID);
                                _sendMessageToAllConnectionsButOnePlaying(msg_sharedPtr, senderID);
                                
                            } else if (id == PLAYER_ID_ALL) {
                                // vxlog_info("📧 %d -> Players", senderID);
                                _sendMessageToAllConnectionsPlaying(msg_sharedPtr);
                                
                            } else {
                                // vxlog_info("📧 %d -> %d", senderID, id);
                                _sendMessageToOneConnection(msg_sharedPtr, id);
                            }
                        }
                    }
                } else if (msg_type == GameMessage::TYPE_HEARTBEAT) {
                    // Game server received an heartbeat. We do nothing.
                } else {
                    vxlog_warning("🌎 received unsupported message type: %d", msg_sharedPtr->msgtype());
                }
                
                // read next message
                msg_sharedPtr = _messageQueues.popIn(id);
                
            } // end of while loop for player id
        }

        // ----------------------------------
        // Turn shutdown timer on/off.
        // ----------------------------------

        if (this->_shutDownTimer >= 0 && (_playersPlaying.empty() == false || _playersJoining.empty() == false || _playersConnected.empty() == false)) {
            this->_shutDownTimer = -1;
        } else if (this->_shutDownTimer < 0 && _playersPlaying.empty() && _playersJoining.empty() && _playersConnected.empty()) {
            this->_shutDownTimer = SHUTDOWN_DELAY;
        }

        // ----------------------------------
        // Send server info updates to Hub
        // every HUB_UPDATE_DELAY seconds.
        // ----------------------------------

#if !defined(CLIENT_ENV)
        // do it only when moving to next frame because
        if (moveToNextFrame && _serverType == ServerType::Online) {
            this->_hubUpdateTimer -= elapsedMilliseconds;
            if (this->_hubUpdateTimer <= 0) {

                this->_hubUpdateTimer = HUB_UPDATE_DELAY;

                int nbPlayers = static_cast<const int>(_playersConnected.size() + _playersJoining.size() + _playersPlaying.size());

                vx::OperationQueue::getBackground()->dispatch([this, nbPlayers]() {
                    bool success = false;
                    bool shouldExit = false;
                    std::string err = "";
                    this->_hubPrivateClient->updateGameServerInfo(this->_serverID,
                                                                  nbPlayers,
                                                                  _hubAuthToken,
                                                                  success,
                                                                  shouldExit,
                                                                  err);

                    if (success == false) {
#if !defined(LOCAL_DEBUG_GAMESERVER)
                        vxlog_error("can't send server info: %s", err.c_str());
                        if (shouldExit) {
                            vxlog_error("exiting because not recognized by the hub 😢");
                            {
                                const std::lock_guard<std::mutex> locker(_shouldExitMutex);
                                this->_shouldExit = true;
                            }
                        }
#endif
                    }
                });
            }
        }
#endif

        // ----------------------------------
        // Check for message in channel
        // ----------------------------------
        
        publishMsg = nullptr;
        {
            const std::lock_guard<std::mutex> locker(_chanToGameThreadMutex);
            if (_chanToGameThread.empty() == false) {
                publishMsg = _chanToGameThread.front();
                _chanToGameThread.pop();
            }
        }

        // Note: channel is unbuffered so it has only 1 message
        if (publishMsg != nullptr) {

            switch (publishMsg->type) {
                case PublishMsgType::GameReady: {
                    
                    // clients are all expexted to reconnect when receiving
                    // the "reload world" message.
                    // Moving them to _playersWaitingToBeRemoved in the meantime
                    this->_moveClientsToWaitingForRemoval();

                    // swap old and new games
                    this->_swapGame(publishMsg->cppGame);

                    // send a message to all client for them to reload world
                    GameMessage_SharedPtr m = GameMessage::makeReloadWorldMsg();
                    _sendMessageToAllConnections(m);
                    vxlog_info("sent world reload message to all clients");
                    break;
                }
                case PublishMsgType::Error: {
                    vxlog_info("error, world not loaded correctly");
                    break;
                }
                default:
                    vxlog_warning("[🐞][🕹] unsupported message type %d", publishMsg->type);
                    break;
            }
            
            delete publishMsg;
        }

        // ----------------------------------
        // Things happen real fast when there's no frame to compute.
        // Sleeping for 1ms to play nice with the CPU :)
        // ----------------------------------

        if (moveToNextFrame == false) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }

    vxlog_trace("📡 exiting game thread");

    // exit == false means thread function is exiting, but
    // _shouldExit may not be true (to stop grpc thread)
    // so let's do it here.
    if (exit == false) {
        const std::lock_guard<std::mutex> locker(_shouldExitMutex);
        this->_shouldExit = true;
    }
}

/// new Game loading thread
void GameServer::_loadingThreadFunction() {
    vxlog_trace("🧵 _loadingThreadFunction starting...");
    bool exit = false;
    PublishMsg *msg = nullptr;

    while (true) {
        {
            const std::lock_guard<std::mutex> locker(_shouldExitMutex);
            exit = this->_shouldExit;
        }
        if (exit) { break; }

        msg = nullptr;
        {
            const std::lock_guard<std::mutex> locker(_chanToLoadingThreadMutex);
            if (_chanToLoadingThread.empty() == false) {
                msg = _chanToLoadingThread.front();
                _chanToLoadingThread.pop();
            }
        }

        if (msg != nullptr) {
            
            switch (msg->type) {
                case PublishMsgType::ReloadWorld:
                {
                    vxlog_trace("reloading world...");
                    Game_SharedPtr loadingGame = Game::make(GameMode_Server,
                                                            vx::WorldFetchInfo::makeWithID(_gameID),
                                                            nullptr, // no data
                                                            "", 
                                                            false,
                                                            false,
                                                            std::vector<std::string>());

                    bool exit = false;
                    while(true) {
                        loadingGame->tick(FRAME_DURATION_S);

                        switch (loadingGame->getState()) {
                            case Game::State::failedToLoadWorldData:
                            case Game::State::exited:
                            {
                                PublishMsg *msgToGame = new PublishMsg();
                                msgToGame->type = PublishMsgType::Error;
                                msgToGame->cppGame = nullptr;
                                _chanToGameThread.push(msgToGame);
                                exit = true;
                                break;
                            }
                            case Game::State::started:
                            {
                                PublishMsg *msgToGame = new PublishMsg();
                                msgToGame->type = PublishMsgType::GameReady;
                                msgToGame->cppGame = loadingGame;
                                _chanToGameThread.push(msgToGame);
                                exit = true;
                                break;
                            }
                            default:
                                break;
                        }

                        if (exit) {
                            break;
                        } else {
                            std::this_thread::sleep_for(std::chrono::milliseconds(FRAME_DURATION_MS));
                        }
                    }
                    break;
                }
                default:
                {
                    vxlog_error("[🐞][⚙️] unsupported message type %d", msg->type);
                    break;
                }
            }
            
            delete msg;
            
        } else { // no message
            // wait half a sec before checking for other messages
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        }        
    }
}

void GameServer::_moveClientsToWaitingForRemoval() {
    std::list<PlayerInfo*>::iterator it;
    bool erased;

    for (it = _playersConnected.begin(); it != _playersConnected.end();) {
        PlayerInfo *playerInfo = (*it);

        it = _playersConnected.erase(it);
        erased = true;
        _playersWaitingToBeRemoved.push_back(playerInfo);

        if (erased == false) {
            it++;
        }
    }

    for (it = _playersJoining.begin(); it != _playersJoining.end();) {
        PlayerInfo *playerInfo = (*it);

        it = _playersJoining.erase(it);
        erased = true;
        _playersWaitingToBeRemoved.push_back(playerInfo);

        if (erased == false) {
            it++;
        }
    }

    for (it = _playersPlaying.begin(); it != _playersPlaying.end();) {
        PlayerInfo *playerInfo = (*it);

        it = _playersPlaying.erase(it);
        erased = true;
        _playersWaitingToBeRemoved.push_back(playerInfo);

        if (erased == false) {
            it++;
        }
    }

    vxlog_trace("📦 PUBLISH: moved connected players to _playersWaitingToBeRemoved");
}

void GameServer::_swapGame(Game_SharedPtr& newGame) {
    vx_assert(_cppGame != nullptr);

    Game_SharedPtr oldGame = _cppGame;
    _cppGame = newGame;
    _data = _cppGame->getData();
    
    // Necessary currently when new game is created
    // with script, and not with ID.
    _cppGame->setFetchInfo(oldGame->getFetchInfo());

    oldGame.reset();

    vxlog_trace("📦 PUBLISH: swapped games");
}

GameServer::ServerType GameServer::_getServerType() const {
    return this->_serverType;
}

void GameServer::_networkingThreadFunction() {
    vxlog_trace("📡 network I/O thread starting...");
    
    switch (_serverType) {
        case ServerType::Local: {
            // local server
            _handleConnections();
            break;
        }
        case ServerType::Online: {
#ifndef CLIENT_ENV
            // online server
            vxlog_info("Server listening on %d", this->_listeningPort);
            _handleConnections();

            // notify Hub server we are stopping
            bool success = false;
            std::string err = "";
            _hubPrivateClient->notifyGameServerExit(this->_serverID,
                                                    this->_hubAuthToken,
                                                    success,
                                                    err);
            if (success == false) {
                vxlog_warning("can't tell hub server is exiting: %s", err.c_str());
            } else {
                vxlog_info("so long hub!");
            }
#endif
            break;
        }
    }
    vxlog_trace("📡 networking thread exited");
}

/// Connections handling function (server's main loop)
///
/// Important:
/// "_connectionHandlers" must only be used in the main thread (this function).
/// (it is not the case currently, we should improve this later and get rid of the mutex)
void GameServer::_handleConnections() {
    bool shouldExit = false;
    
    if (_serverType == ServerType::Online) {
#if defined(__VX_USE_LIBWEBSOCKETS)
        vx_assert(_wsServer != nullptr);
        _wsServer->listen();
#endif
    }
    
    while (shouldExit == false) {
        
        if (_serverType == ServerType::Online) {
#if defined(__VX_USE_LIBWEBSOCKETS)
            vx_assert(_wsServer != nullptr);
            _wsServer->process();
#endif
        } else if (_serverType == ServerType::Local) {
            
        }
        
        // cleanup connections
        {
            std::lock_guard<std::mutex> lock(_connectionsToRemoveMutex);
            while (_connectionsToRemove.empty() == false) {
                auto it = _connectionsToRemove.begin();
                
                // notify the handler it will be removed entirely from the server
                // (also notifies other players)
                (*it)->willBeRemovedFromServer();
                
                // destroy handler
                _connectionsToRemove.erase(it); // calls *it's destructor
            }
        }
        
        {
            const std::lock_guard<std::mutex> lock(_shouldExitMutex);
            shouldExit = _shouldExit;
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}
