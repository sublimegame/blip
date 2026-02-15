//
//  VXConnectionHandlerJoin.cpp
//  Cubzh
//
//  Created by Gaetan de Villele on 15/02/2022.
//

#include "VXConnectionHandlerJoin.hpp"

#include "VXGameServer.hpp"
#include "VXNetworkingUtils.hpp"

#include "xptools.h"
#include "config.h"

using namespace vx;

ConnectionHandlerJoin_SharedPtr ConnectionHandlerJoin::make(GameServer* gameserver,
                                                            Connection_SharedPtr connection,
                                                            const uint8_t playerID) {
    ConnectionHandlerJoin_SharedPtr ptr(new ConnectionHandlerJoin);
    ptr->init(ptr, gameserver, connection, playerID);
    return ptr;
}

ConnectionHandlerJoin::ConnectionHandlerJoin() :
ConnectionHandler(nullptr),
_conn(nullptr),
_status(ConnectionStatus::INIT),
_messageBeingReceived(),
_playerID(PLAYER_ID_NONE) {}

void ConnectionHandlerJoin::init(const ConnectionHandlerJoin_SharedPtr& ref,
                                 GameServer* gameserver,
                                 Connection_SharedPtr connection,
                                 const uint8_t playerID) {
    _weakSelf = ref;
    _gameServer = gameserver;
    _conn = connection;
    _playerID = playerID;
    _conn->setDelegate(_weakSelf);
}

ConnectionHandlerJoin::~ConnectionHandlerJoin() {
    
}

void ConnectionHandlerJoin::closeConnection() {
    _conn->close();
}

bool ConnectionHandlerJoin::isConnectionClosed() const {
    return _conn->isClosed();
}

void ConnectionHandlerJoin::willBeRemovedFromServer() {
    const uint8_t playerID = this->getPlayerID();
    vxlog_trace("handler will be removed. player ID: %d", playerID);
    
    // notify the GameServer the client / player is leaving
    if (playerID != PLAYER_ID_NONE) {
        this->_gameServer->pushPlayerRemovalRequest(playerID);
    }
}

void ConnectionHandlerJoin::connectionDidEstablish(Connection& conn) {
    vxlog_trace("connectionDidEstablish (_playerID: %d)", _playerID);
    vx_assert(_status == ConnectionStatus::INIT);
    
    GameMessage_SharedPtr msg = GameMessage::makeAssignPlayerIdMsg(_playerID);
    Connection::Payload_SharedPtr p = msg->unloadPayload();

    _status = ConnectionStatus::CLOCKSYNC;
    
    // send player ID to client
    _conn->pushPayloadToWrite(p);
}

void ConnectionHandlerJoin::connectionDidReceive(Connection& conn, const Connection::Payload_SharedPtr& payload) {
    // Payload received in parameter should be freed by the end
    // of the function, or responsability to do so should be
    // handed to something.

    // vxlog_debug("[📡][join] connectionDidReceive");
    
    if (payload == nullptr) {
        return;
    }
    
    switch (_status) {
        case ConnectionStatus::INIT: {
            vxlog_error("ConnectionStatus should never be INIT in connectionDidReceive");
            vx_assert(false);
            break;
        }
        case ConnectionStatus::CLOCKSYNC: {
            
            GameMessage_SharedPtr msg = GameMessage::make(payload);
            if (msg == nullptr) {
                vxlog_error("handshake msg is null (playerID: %d)", _playerID);
                break;
            }
            
            vxlog_trace("📩 from %d: %s", _playerID, GameMessage::typeToStr(GameMessage::numToType(msg->msgtype())).c_str());

            if (msg->msgtype() == GameMessage::Type::TYPE_CLIENT_HANDSHAKE) {

                // flush message queues for this new player ID
                _gameServer->getMessageQueues().flushQueuesForPlayerID(_playerID);
                _status = ConnectionStatus::PROCESS;

                uint8_t playerID;
                std::string userID;
                std::string userName;
                const bool ok = GameMessage::decodeHandshakeMsg(msg, playerID, userID, userName);
                if (ok == false) {
                    vxlog_error("failed to decode handshake message");
                    _conn->close();
                    return;
                }

                if (_playerID != playerID) {
                    vxlog_error("handshake does not contain correct playerID: %d (expected) != %d", _playerID, playerID);
                    _conn->close();
                    return;
                }

                vxlog_trace("player (%d): %s (id: %s)", playerID, userName.c_str(), userID.c_str());

                // request Player object to be allocated by the game simulation thread
                // ! \\ Player may not be authenticated here, userID and userName updated later
                _gameServer->pushPlayerCreationRequest(playerID, userID, userName);

                // update connection status
                _status = ConnectionStatus::PROCESS;

            } else {
                vxlog_trace("expected msgf type TYPE_CLOCKTIME_REQ but got %d", msg->msgtype());
            }
            break;
        }
        case ConnectionStatus::PROCESS: {

            GameMessage_SharedPtr msg = GameMessage::make(payload);
            if (msg == nullptr) {
                vxlog_error("PROCESS msg is null (playerID: %d)", _playerID);
                break;
            }
            
            // TODO: avoid buffering here
            _gameServer->getMessageQueues().pushIn(_playerID, msg);
            break;
        }
        case ConnectionStatus::FINISH: {
            vxlog_debug("ConnectionHandlerJoin - ConnectionStatus::FINISH");
            break;
        }
    }
}

void ConnectionHandlerJoin::connectionDidClose(Connection& conn) {
    vxlog_trace("[📡][join] connectionDidClose");
    vx_assert(_conn.get() == &conn);
    ConnectionHandlerJoin_SharedPtr strongSelf = _weakSelf.lock();
    if (strongSelf != nullptr) {
        _gameServer->removeConnection(strongSelf);
    }
}

void ConnectionHandlerJoin::write(const Connection::Payload_SharedPtr& p) {
    this->_conn->pushPayloadToWrite(p); // _conn will free Payload
}

std::string ConnectionHandlerJoin::_statusReadToStr(const ConnectionStatus& status) const {
    switch (status) {
        case ConnectionStatus::INIT:
            return "INIT";
        case ConnectionStatus::CLOCKSYNC:
            return "CLOCKSYNC";
        case ConnectionStatus::PROCESS:
            return "PROCESS";
        case ConnectionStatus::FINISH:
            return "FINISH";
    }
    return "";
}
