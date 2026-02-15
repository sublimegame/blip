//
//  VXGameClient.hpp
//  Cubzh
//
//  Created by Gaetan de Villele on 08/09/2020.
//

#pragma once

// C++
#include <queue>
#include <string>
#include <mutex>

// vx
#include "Connection.hpp"
#include "VXGameMessage.hpp"

namespace vx {

class GameServer;

class GameClientDelegate;
typedef std::shared_ptr<GameClientDelegate> GameClientDelegate_SharedPtr;
typedef std::weak_ptr<GameClientDelegate> GameClientDelegate_WeakPtr;

class GameClient;
typedef std::shared_ptr<GameClient> GameClient_SharedPtr;
typedef std::weak_ptr<GameClient> GameClient_WeakPtr;

class GameClient final : public ConnectionDelegate {
    
public:
    
    enum class Mode {
        LOCAL_SERVER,
        REMOTE_SERVER
    };
    
    enum class State {
        IDLE,
        CONNECTING,
        HANDSHAKE_SENT,
        CONNECTED,
        DISCONNECTED
    };
    
    /// Creates a client for a remote server
    static GameClient_SharedPtr make(const std::string &host,
                                            const std::string &userID,
                                            const std::string &userName,
                                            GameClientDelegate_SharedPtr delegate);
    
    /// Creates a client for a local server
    static GameClient_SharedPtr make(GameServer& localGameServer,
                                            const std::string &userID,
                                            const std::string &userName,
                                            GameClientDelegate_SharedPtr delegate);
    
    /// Destructor
    virtual ~GameClient();
    
    /// Initiate connection
    void connect();
    
    /// When calling this, the client disconnects the stream
    /// and removes itself. No need to keep a reference on it
    /// after calling stop().
    void stop(bool error);
    
    /// Sends a game message to the server.
    void sendGameMsg(GameMessage_SharedPtr msg);
    
    /// Sends a "publish game script" message
    void sendReloadWorldMessage();
    
    ///
    uint8_t getPlayerID();
    
    // ---------------------------------------------
    // ConnectionDelegate interface
    // ---------------------------------------------
    void connectionDidEstablish(Connection& conn) override;
    void connectionDidReceive(Connection& conn, const Connection::Payload_SharedPtr& payload) override;
    void connectionDidClose(Connection& conn) override;

private:
    
    /// Private constructor
    GameClient(const Mode mode,
               GameClientDelegate_SharedPtr delegate,
               Connection_SharedPtr serverConnection,
               const std::string &userID,
               const std::string &userName);
    
    ///
    GameClient_WeakPtr _weakSelf;
    
    /// GameClient's delegate
    /// Game is usually its GameClient's delegate
    GameClientDelegate_WeakPtr _delegate;
    
    /// Mode of the GameClient (connected to a local or remote server)
    Mode _mode;
    
    /// Connection to the server
    Connection_SharedPtr _serverConnection;
    
    /// Only accessed in _thread (no need for a mutex)
    State _state;
    
    uint8_t _playerID; /// 0 means "no ID"
    std::string _userID;
    std::string _userName;
    
    uint8_t _connectionRetries;
};

class GameClientDelegate {
public:
    /// Triggered when connnection is fully established
    /// (including handshake, clocksync, etc.)
    virtual void gameClientDidConnect() = 0;
    
    /// Triggered when receiving some Payload
    virtual void gameClientDidReceivePayload(const Connection::Payload_SharedPtr& payload) = 0;
    
    /// Triggered if the game client fails to connect.
    /// Connection never established.
    virtual void gameClientFailedToConnect() = 0;
    
    /// Triggered when losing connection (not expected)
    virtual void gameClientLostConnection() = 0;
    
    /// Triggered when the connection is closed, as expected
    virtual void gameClientDidCloseConnection() = 0;
};

} // namespace vx
