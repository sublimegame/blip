//
//  VXGameMessage.hpp
//  Cubzh
//
//  Created by Gaetan de Villele on 24/09/2020.
//

#pragma once

// C++
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

// vx
#include "Connection.hpp"

namespace vx {

enum ChatMessageStatus : uint8_t {
    OK = 0,
    ERR = 1,
    MODERATED = 2, // previously "REPORTED"
};

enum ChatMessageModerationReason : uint8_t {
    NONE = 0,
    // inappropriate
    VIOLENCE = 1,
    SEXUAL_CONTENT = 2,
    VERBAL_ABUSE = 3,
    IDENTITY_HATE = 4,
    PROFANITY = 5,
    DRUGS = 6,
    SELF_HARM = 7,
    // link sharing not allowed
    LINK_SHARING = 8,
    // spam
    SPAM = 9,
    // warning, do not share personnal info!
    PII = 10,
};

class GameMessage;
typedef std::shared_ptr<vx::GameMessage> GameMessage_SharedPtr;
typedef std::weak_ptr<vx::GameMessage> GameMessage_WeakPtr;

class GameMessage final {
public:
    
    enum Type {
        TYPE_INVALID = 0,              // message is invalid
        // TYPE_DISCONNECT = 1,
        TYPE_ASSIGN_PLAYER_ID = 2,     // from server to client
        TYPE_CHAT_MESSAGE = 3,         //
        TYPE_CLIENT_HANDSHAKE = 4,     // from client : handshake containing userID and userName
        TYPE_CLIENT_GAMEREADY = 5,     // from client : tells server it has loaded the game and is ready for simulation
        TYPE_SERVER_PLAYERJOINED = 6,  // from server : tells client that a player has joined the game
        TYPE_SERVER_PLAYERLEFT = 7,    // from server : tells client that a player has left the game
        
        TYPE_RELOAD_WORLD = 8,         // tells recipient world needs to be reloaded
        // TYPE_CLOCKTIME_REQ = 8,        // from client : ask for server time
        // TYPE_CLOCKTIME_RESP = 9,       // from server : response with server time
        
        // TYPE_PUBLISH_SCRIPT = 10,      // from client : publishing new game script
        // TYPE_GAME_SCRIPT = 11,         // from server : provided Lua script to the joining client
        // TYPE_PUBLISH_ERROR = 12,
        // TYPE_SERVER_STARTGAME = 13,    // from server to client : tells the client to start (OnStart Lua function)
        
        TYPE_GAME_EVENT_SMALL = 14,    // from server or from client : event message (255 bytes max)
        TYPE_GAME_EVENT = 15,          // from server or from client : event message (65535 bytes max)
        TYPE_BIG_GAME_EVENT_BIG = 16,  // from server or from client : event message (5000000 bytes max)
        
        TYPE_HEARTBEAT = 17,           // connection heartbeat (includes sender ID)
        
        // TYPE_PLAYER_AUTH = 18,         // provides user ID & name
        
        TYPE_CLIENT_HANDSHAKE_SERVER_APPROVAL = 19, // from server : true if handshake is accepted, establishing connection

        TYPE_CHAT_MESSAGE_ACK = 20,

        TYPE_MAX = 20
    };
    
    static inline Type numToType(const uint32_t& num) {
        Type t = Type(num);
        return t;
    }
    
    static inline std::string typeToStr(const Type& type) {
        switch (type) {
            case TYPE_INVALID:
                return "TYPE_INVALID";
            case TYPE_ASSIGN_PLAYER_ID:
                return "TYPE_ASSIGN_PLAYER_ID";
            case TYPE_CLIENT_HANDSHAKE:
                return "TYPE_CLIENT_HANDSHAKE";
            case TYPE_CLIENT_GAMEREADY:
                return "TYPE_CLIENT_GAMEREADY";
            case TYPE_SERVER_PLAYERJOINED:
                return "TYPE_SERVER_PLAYERJOINED";
            case TYPE_SERVER_PLAYERLEFT:
                return "TYPE_SERVER_PLAYERLEFT";
            case TYPE_RELOAD_WORLD:
                return "TYPE_RELOAD_WORLD";
            case TYPE_GAME_EVENT:
                return "TYPE_GAME_EVENT";
            case TYPE_GAME_EVENT_SMALL:
                return "TYPE_GAME_EVENT_SMALL";
            case TYPE_BIG_GAME_EVENT_BIG:
                return "TYPE_BIG_GAME_EVENT_BIG";
            case TYPE_HEARTBEAT:
                return "TYPE_HEARTBEAT";
            case TYPE_CHAT_MESSAGE:
                return "TYPE_CHAT_MESSAGE";
            case TYPE_CHAT_MESSAGE_ACK:
                return "TYPE_CHAT_MESSAGE_ACK";
            case TYPE_CLIENT_HANDSHAKE_SERVER_APPROVAL:
                return "TYPE_CLIENT_HANDSHAKE_SERVER_APPROVAL";
        }
        return "TYPE_UNKNOWN";
    }
    
    enum PublishErrorType: uint8_t {
        UNKNOWN = 0,
        PROTOCOL = 1,
        LUA_ERROR = 2,
    };
    
    // player ID
    
    static GameMessage_SharedPtr makeAssignPlayerIdMsg(const uint8_t ID);
    static bool decodePlayerID(GameMessage_SharedPtr msg, uint8_t& ID);
    
    // client handshake
    
    static GameMessage_SharedPtr makeHandshakeMsg(const uint8_t& senderPlayerID,
                                                  const std::string& senderUserID,
                                                  const std::string& senderUserName);
    
    static bool decodeHandshakeMsg(GameMessage_SharedPtr msg,
                                   uint8_t& senderPlayerID,
                                   std::string& senderUserID,
                                   std::string& senderUserName);
    
    static GameMessage_SharedPtr makeHandshakeServerApprovalMsg(const bool approved);
    
    static bool decodeHandshakeServerApprovalMsg(const GameMessage_SharedPtr msg, bool &approved);
    
    
    // client publishes new game script to the server
    
    static GameMessage_SharedPtr makeReloadWorldMsg();
    static bool decodeReloadWorldMsg(GameMessage_SharedPtr msg);
    
    /// server : notify clients of a new player who joined the game
    
    static GameMessage_SharedPtr makePlayerJoined(const uint8_t& playerID,
                                                  const std::string& userID,
                                                  const std::string& userName);
    static bool decodePlayerJoined(GameMessage_SharedPtr msg,
                                   uint8_t& playerID,
                                   std::string& userID,
                                   std::string& userName);
    
    /// server : server left
    static GameMessage_SharedPtr makePlayerLeft(const uint8_t& playerID);
    static bool decodePlayerLeft(GameMessage_SharedPtr msg, uint8_t& playerID);
    
    // Game Event
    static GameMessage_SharedPtr makeGameEvent(const uint8_t& eventType,
                                               const uint8_t& senderID,
                                               const std::vector<uint8_t>& recipientIDs,
                                               const uint8_t *data,
                                               const size_t size);

    // Decodes full Event message
    static bool decodeGameEvent(GameMessage_SharedPtr msg,
                                uint8_t& eventType,
                                uint8_t& senderID,
                                std::vector<uint8_t>& recipientIDs,
                                uint8_t **data,
                                size_t *size);

    // Decodes Event message metadata (type, sender, recipients)
    static bool decodeGameEventMeta(GameMessage_SharedPtr msg,
                                    uint8_t& eventType,
                                    uint8_t& senderID,
                                    std::vector<uint8_t>& recipientIDs);
    
    // Decodes Event bytes
    static bool decodeGameEventBytes(GameMessage_SharedPtr msg,
                                     uint8_t **data,
                                     size_t *size);

    // Chat Message
    static GameMessage_SharedPtr makeChatMessage(const uint8_t& senderID,
                                                 const std::string& senderUserID,
                                                 const std::string& senderUserName,
                                                 const std::string& localUUID,
                                                 const std::string& UUID, // assigned by server, provide "" when creating message from client
                                                 const std::vector<uint8_t>& recipientIDs,
                                                 const std::string& chatMessage);

    static bool chatMessageAttachUUID(GameMessage_SharedPtr msg, const std::string& uuid);

    static bool decodeChatMessage(GameMessage_SharedPtr msg,
                                  uint8_t& senderID,
                                  std::string& senderUserID,
                                  std::string& senderUserName,
                                  std::string& localUUID,
                                  std::string& UUID, // assigned by server
                                  std::vector<uint8_t>& recipientIDs,
                                  std::string& chatMessage);

    // sent by server to sender to indicate if chat message has been delivered
    // status: 0: OK, 1: error, 2: reported
    static GameMessage_SharedPtr makeChatMessageACK(const std::string& localUUID,
                                                    const std::string& UUID,
                                                    const uint8_t& status);

    static bool decodeChatMessageACK(GameMessage_SharedPtr msg,
                                     std::string& localUUID,
                                     std::string& UUID,
                                     uint8_t& status);

    /// Allocates a new heartbeat message
    static GameMessage_SharedPtr makeHeartbeat(const uint8_t& senderPlayerID);
    
    /// Decodes heartbeat message
    static bool decodeHeartbeatMessage(GameMessage_SharedPtr msg,
                                       uint8_t& senderID);
    
    
    // --------------------------------------------------
    // Serialization / Deserialization for network transport
    // --------------------------------------------------
    
    /// Returns GameMessage's _bytes/_len as Payload
    /// Setting _payload to NULL
    /// Returned Payload (& contained bytes) now under caller's responsability.
    Connection::Payload_SharedPtr unloadPayload();
    
    // Returns a copy of the GameMessage's _bytes
    Connection::Payload_SharedPtr copyPayload();
    
    /// Build GameMessage from raw bytes
    /// Takes pointer ownership, no need to free given bytes,
    /// and they should not be used after the GameMessage is made.
    static GameMessage_SharedPtr make(const Connection::Payload_SharedPtr& payload);
    
    // Instance methods
    
    GameMessage();
    ~GameMessage();
    
    // Returns the type of the message
    uint8_t msgtype() const;
    
    // Prints debug for underlying Payload
    void debug();
    
    // Adds debug step
    void step(const std::string& str);
    
private:
    // _bytes contains everything, the GameMessage is always ready to go
    // structure of _bytes:
    // - type (uint_8)
    // - custom content // (no need to write the size, it depends on each type)
    // char* _bytes;
    // size_t _bytesLen;
    Connection::Payload_SharedPtr _payload;
};

} // namespace vx
