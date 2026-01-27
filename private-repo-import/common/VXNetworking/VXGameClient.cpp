//
//  VXGameClient.cpp
//  Cubzh
//
//  Created by Gaetan de Villele on 08/09/2020.
//

#include "VXGameClient.hpp"

// xptools
#include "xptools.h"
#include "HttpClient.hpp"
#include "LocalConnection.hpp"
#include "WSConnection.hpp"
#include "OperationQueue.hpp"

// VXNetworking
#include "VXGameMessage.hpp"
#include "GameCoordinator.hpp"
#include "VXNetworkingUtils.hpp"

#define RETRY_DELAY_MS 500
#define RETRIES 10

using namespace vx;

/// Creates a client for a remote server
/// ws://<addr>:<port>
/// wss://<addr>:<port>
GameClient_SharedPtr GameClient::make(const std::string& urlStr,
                                      const std::string& userID,
                                      const std::string& userName,
                                      GameClientDelegate_SharedPtr delegate) {
    const vx::URL url = vx::URL::make(urlStr, "wss");
    if (url.isValid() == false) {
        return nullptr;
    }
    
    WSConnection_SharedPtr conn = WSConnection::make(url.scheme(), url.host(), url.port(), url.path());
    GameClient *client = new GameClient(Mode::REMOTE_SERVER, delegate, conn, userID, userName);
    
    GameClient_SharedPtr sp = GameClient_SharedPtr(client);
    client->_weakSelf = GameClient_WeakPtr(sp);
    return sp;
}

// client for local server
GameClient_SharedPtr GameClient::make(GameServer& localGameServer,
                                      const std::string &userID,
                                      const std::string &userName,
                                      GameClientDelegate_SharedPtr delegate) {
    
    // obtain a net::Connection instance from the local GameServer
    Connection_SharedPtr conn = localGameServer.connectJoin();
    
    // alloc GameClient instance
    GameClient *client = new GameClient(Mode::LOCAL_SERVER, delegate, conn, userID, userName);
    
    GameClient_SharedPtr sp = GameClient_SharedPtr(client);
    client->_weakSelf = GameClient_WeakPtr(sp);
    return sp;
}

GameClient::GameClient(const Mode mode,
                       GameClientDelegate_SharedPtr delegate,
                       Connection_SharedPtr serverConnection,
                       const std::string &userID,
                       const std::string &userName):
_delegate(),
_mode(mode),
_serverConnection(serverConnection),
_state(State::IDLE),
_playerID(PLAYER_ID_NOT_ATTRIBUTED),
_userID(userID),
_userName(userName),
_connectionRetries(RETRIES) {
    if (delegate != nullptr) {
        _delegate = GameClientDelegate_WeakPtr(delegate);
    }
    vxlog_trace("[GameClient] allocated");
}

GameClient::~GameClient() {
    vxlog_trace("[GameClient] deleted");
}

void GameClient::connect() {
    if (_state != State::IDLE && _state != State::DISCONNECTED) {
        vxlog_error("client can't connect, already connected, or connecting");
        return;
    }
    _state = State::CONNECTING;
    
    // GameClient becomes the connection delegate
    _serverConnection->setDelegate(_weakSelf);
    _serverConnection->connect();
}

void GameClient::stop(bool error) {
    vx_assert(_serverConnection != nullptr);
    if (error) {
        _serverConnection->closeOnError();
    } else {
        _serverConnection->close();
    }
}

void GameClient::sendGameMsg(GameMessage_SharedPtr msg) {
    if (_state != State::CONNECTED) {
        vxlog_warning("message not sent, not connected to server");
        return;
    }
    
    msg->step("GameClient::sendGameMsg");
    // vxlog_info("📧 from %d", _playerID);
    
    Connection::Payload_SharedPtr p = msg->unloadPayload();
    _serverConnection->pushPayloadToWrite(p);
}

/// Sends a "publish game script" message
void GameClient::sendReloadWorldMessage() {
    this->sendGameMsg(GameMessage::makeReloadWorldMsg());
}

uint8_t GameClient::getPlayerID() {
    return this->_playerID;
}

void GameClient::connectionDidEstablish(Connection& conn) {
    vx_assert(&conn == _serverConnection.get());
}

void GameClient::connectionDidReceive(Connection& conn, const Connection::Payload_SharedPtr& payload) {
    
    // Payload received in parameter should be freed by the end
    // of the function, or responsability to do so should be
    // handed to something.
    
    // vxlog_debug("[GameClient::connectionDidReceive]");
     if (payload == nullptr) { return; }
    
    // de-serialize game message
    // bytes will be freed by the GameMessage destructor
    GameMessage_SharedPtr msgReceived = GameMessage::make(payload);
    if (msgReceived == nullptr) {
        vxlog_error("[GameClient::connectionDidReceive] failed to deserialize GameMessage");
        return;
    }

    // process game message
    
    // TODO: ADD CHALLENGE
    // a random hash that should be "signed", using loaded Lua script's hash.
    
    if (_state == State::CONNECTING) {
        
        // we have received a playerID
        const bool ok = GameMessage::decodePlayerID(msgReceived, _playerID);
        if (ok == false) {
            vxlog_error("GameClient::connectionDidReceive - failed to decode GameMessage for player ID");
            stop(true);
            return;
        }
        
        GameMessage_SharedPtr msgToSend = GameMessage::makeHandshakeMsg(_playerID, _userID, _userName);
        Connection::Payload_SharedPtr p = msgToSend->unloadPayload();
        _serverConnection->pushPayloadToWrite(p);
        
        _state = State::HANDSHAKE_SENT;
        
    } else if (_state == State::HANDSHAKE_SENT) {

        _state = State::DISCONNECTED; // set to CONNECTED if everything goes well

        const uint8_t msgType = msgReceived->msgtype();
        if (msgType != GameMessage::TYPE_CLIENT_HANDSHAKE_SERVER_APPROVAL) {
            vxlog_error("expected TYPE_CLIENT_HANDSHAKE_SERVER_APPROVAL message type, got %d", msgType);
            stop(true);
            return;
        }
        
        bool approved;
        const bool ok = GameMessage::decodeHandshakeServerApprovalMsg(msgReceived, approved);
        if (ok == false) {
            vxlog_error("failed to decode TYPE_CLIENT_HANDSHAKE_SERVER_APPROVAL message");
            stop(true);
            return;
        }
        
        if (approved == false) {
            vxlog_error("client not approved by server");
            stop(true);
            return;
        }
        
        // inform delegate
        GameClientDelegate_SharedPtr delegate = _delegate.lock();
        if (delegate != nullptr) {
            delegate->gameClientDidConnect();
        }
        
        // update client state
        _state = State::CONNECTED;
        
    } else if (_state == State::CONNECTED) {
        // push message to queue
        GameClientDelegate_SharedPtr delegate = _delegate.lock();
        if (delegate != nullptr) {
            delegate->gameClientDidReceivePayload(msgReceived->unloadPayload());
        }
        
    } else {
        vxlog_error("GameClient::connectionDidReceive - unsupported state");
    }
}

void GameClient::connectionDidClose(Connection& conn) {
    vx_assert(&conn == _serverConnection.get());
    vx_assert(_mode == Mode::REMOTE_SERVER);
    
    Connection::Status status = conn.getStatus();
    
    if (_connectionRetries > 0 && status == Connection::Status::CLOSED_INITIAL_CONNECTION_FAILURE) {
        
        --_connectionRetries;
        _serverConnection->reset();
        
        // retry every after 100ms
        GameClient_WeakPtr wp = _weakSelf;
        OperationQueue::getBackground()->schedule([wp](){
            GameClient_SharedPtr sp = wp.lock();
            if (sp == nullptr) return;
            sp->_serverConnection->connect();
        }, RETRY_DELAY_MS);
    } else { // CONNECTION LOST / WILL NEVER BE ESTABLISHED
        _state = State::DISCONNECTED;
        
        GameClientDelegate_SharedPtr delegate = _delegate.lock();
        if (delegate != nullptr) {
            if (status == Connection::Status::CLOSED_ON_ERROR) {
                delegate->gameClientLostConnection();
            } else if (status == Connection::Status::CLOSED_INITIAL_CONNECTION_FAILURE) {
                delegate->gameClientFailedToConnect();
            } else if (status == Connection::Status::CLOSED) { // EXPECTED
                delegate->gameClientDidCloseConnection();
            }
        }
    }
}
