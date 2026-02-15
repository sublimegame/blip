//
//  VXConnectionHandlerJoin.hpp
//  Cubzh
//
//  Created by Gaetan de Villele on 15/02/2022.
//

#pragma once

// C++
#include <cstdint>

// vx
#include "VXGameMessage.hpp"
#include "VXConnectionHandler.hpp"

namespace vx {

class ConnectionHandlerJoin final :
public ConnectionHandler,
public ConnectionDelegate {
    
public:
    
    static std::shared_ptr<ConnectionHandlerJoin> make(GameServer* gameserver,
                                                       Connection_SharedPtr connection,
                                                       const uint8_t playerID);
    
    ///
    ~ConnectionHandlerJoin() override;
    
    // NOTE: request may not be attached to a player ID
    // Like "Ping" that happens before the connection is even established
    inline const uint8_t& getPlayerID() const { return _playerID; }
    
    // inherited from ConnectionHandler
    void closeConnection() override;
    bool isConnectionClosed() const override;
    void willBeRemovedFromServer() override;
    
    // ConnectionDelegate interface implementation
    void connectionDidEstablish(Connection& conn) override;
    void connectionDidReceive(Connection& conn, const Connection::Payload_SharedPtr& payload) override;
    void connectionDidClose(Connection& conn) override;
    
    virtual void write(const Connection::Payload_SharedPtr& p) override;
    
private:
    
    /// Let's implement a tiny state machine with the following states.
    enum class ConnectionStatus {
        INIT,       // client just connected, let's send a player ID
        CLOCKSYNC,  // syncing clocks
        PROCESS,    //
        FINISH      //
    };
    
    /// Constructor
    ConnectionHandlerJoin();
    
    ///
    void init(const std::shared_ptr<ConnectionHandlerJoin>& ref,
              GameServer* gameserver,
              Connection_SharedPtr connection,
              const uint8_t playerID);
    
    ///
    std::weak_ptr<ConnectionHandlerJoin> _weakSelf;
    
    ///
    std::string _statusReadToStr(const ConnectionStatus& status) const;
    
    /// connection to the client
    Connection_SharedPtr _conn;
    
    /// The current state of the connection
    ConnectionStatus _status;
    
    ///
    std::string _messageBeingReceived;
    
    /// Player for this connection.
    /// This is a weak reference.
    /// It may represent no player (PLAYER_ID_NONE)
    uint8_t _playerID;
};

typedef std::shared_ptr<ConnectionHandlerJoin> ConnectionHandlerJoin_SharedPtr;
typedef std::weak_ptr<ConnectionHandlerJoin> ConnectionHandlerJoin_WeakPtr;

} // namespace vx
