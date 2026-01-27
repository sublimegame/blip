//
//  VXConnectionHandler.hpp
//  Cubzh
//
//  Created by Gaetan de Villele on 09/02/2022.
//

#pragma once

// xptools
#include "Connection.hpp"

namespace vx {

class GameServer;

///
class ConnectionHandler {
public:
    
    /// Constructor
    ConnectionHandler(GameServer* gameserver);
    
    /// Destructor
    virtual ~ConnectionHandler();
    
    ///
    virtual void closeConnection() = 0;
    
    ///
    virtual bool isConnectionClosed() const = 0;
    
    /// Notify handler it is about the be deleted and removed entirely from
    /// the GameServer. Handler should free all local resources and notify
    /// the GameServer (to tell him a Player ID is leaving for instance)
    virtual void willBeRemovedFromServer() = 0;
    
    /// Writes in connection
    /// ConnectionHandler if responsible to free given bytes.
    virtual void write(const Connection::Payload_SharedPtr& p) = 0;
    
protected:
    
    /// The "parent" GameServer.
    /// This is a weak reference.
    GameServer* _gameServer;
    
private:
    
};

typedef std::shared_ptr<ConnectionHandler> ConnectionHandler_SharedPtr;
typedef std::weak_ptr<ConnectionHandler> ConnectionHandler_WeakPtr;

} // namespace vx
