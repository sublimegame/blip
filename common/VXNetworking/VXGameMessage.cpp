//
//  VXGameMessage.cpp
//  Cubzh
//
//  Created by Gaetan de Villele on 24/09/2020.
//

#include "VXGameMessage.hpp"

#define MAX_RECIPIENTS 255 // because nbRecipients is uint8_t

#define GAME_EVENT_DATA_MAX_SIZE 65535
#define GAME_EVENT_DATA_SIZE_TYPE uint16_t

#define GAME_EVENT_SMALL_DATA_MAX_SIZE 255
#define GAME_EVENT_SMALL_DATA_SIZE_TYPE uint8_t

#define GAME_EVENT_BIG_DATA_MAX_SIZE 5000000 // ~5MB (arbitrary)
#define GAME_EVENT_BIG_DATA_SIZE_TYPE uint32_t

// C++
#include <string>       // std::string
#include <sstream>      // std::stringstream
#include <cstring>

// xptools
#include "config.h"
#include "xptools.h"

using namespace vx;

GameMessage::GameMessage() :
_payload(nullptr) {}

GameMessage::~GameMessage() {
    if (_payload != nullptr) {
        _payload = nullptr;
    }
}

uint8_t GameMessage::msgtype() const {
    if (_payload == nullptr || _payload->getContent() == nullptr || _payload->contentSize() == 0) { return TYPE_INVALID; }
    const uint8_t type = reinterpret_cast<uint8_t*>(_payload->getContent())[0];
    if (type > TYPE_MAX) { return TYPE_INVALID; }
    return type;
}

void GameMessage::debug() {
    if(_payload != nullptr) {
        _payload->debug();
    }
}

void GameMessage::step(const std::string& str) {
    if(_payload != nullptr) {
        _payload->step(str);
    }
}

GameMessage_SharedPtr GameMessage::makeAssignPlayerIdMsg(const uint8_t ID) {
    GameMessage_SharedPtr msg = GameMessage_SharedPtr(new vx::GameMessage);
    
    size_t len = sizeof(uint8_t) * 2;
    char *bytes = static_cast<char*>(malloc(len));
    msg->_payload = Connection::Payload::create(bytes, len);
    
    char *cursor = bytes;
    uint8_t type = TYPE_ASSIGN_PLAYER_ID;
    
    memcpy(cursor, &type, sizeof(uint8_t));
    cursor += sizeof(uint8_t);
    memcpy(cursor, &ID, sizeof(uint8_t));
    
    return msg;
}

bool GameMessage::decodePlayerID(GameMessage_SharedPtr msg, uint8_t &ID) {
    if (msg->msgtype() != TYPE_ASSIGN_PLAYER_ID) { return false; }
    
    char *cursor = msg->_payload->getContent();
    cursor += sizeof(uint8_t); // skip type
    
    ID = *(reinterpret_cast<uint8_t*>(cursor));
    return true;
}

GameMessage_SharedPtr GameMessage::makeHandshakeMsg(const uint8_t &senderPlayerID,
                                                    const std::string& userID,
                                                    const std::string& userName) {
    
    const uint32_t userIDSize = static_cast<const uint32_t>(userID.size());
    const uint32_t userNameSize = static_cast<const uint32_t>(userName.size());
    
    GameMessage_SharedPtr msg = GameMessage_SharedPtr(new vx::GameMessage);
    
    size_t len = sizeof(uint8_t) + // type
    sizeof(uint8_t) + // sender player ID
    sizeof(uint32_t) + // userID size
    userIDSize + // userID
    sizeof(uint32_t) + // username size
    userNameSize; // username
    
    char *bytes = static_cast<char*>(malloc(len));
    msg->_payload = Connection::Payload::create(bytes, len);
    
    char *cursor = bytes;
    uint8_t type = TYPE_CLIENT_HANDSHAKE;
    
    memcpy(cursor, &type, sizeof(uint8_t)); // type
    cursor += sizeof(uint8_t);
    
    memcpy(cursor, &senderPlayerID, sizeof(uint8_t)); // sender player ID
    cursor += sizeof(uint8_t);
    
    memcpy(cursor, &userIDSize, sizeof(uint32_t)); // userID size
    cursor += sizeof(uint32_t);
    
    memcpy(cursor, userID.c_str(), userIDSize); // userID
    cursor += userIDSize;
    
    memcpy(cursor, &userNameSize, sizeof(uint32_t)); // userName size
    cursor += sizeof(uint32_t);
    
    memcpy(cursor, userName.c_str(), userNameSize); // userName
    // cursor += userNameSize;
    
    return msg;
}

bool GameMessage::decodeHandshakeMsg(const GameMessage_SharedPtr msg,
                                     uint8_t &senderPlayerID,
                                     std::string& userID,
                                     std::string& userName) {
    
    if (msg->msgtype() != TYPE_CLIENT_HANDSHAKE) { return false; }
    
    char *cursor = msg->_payload->getContent();
    cursor += sizeof(uint8_t); // skip type
    
    memcpy(&senderPlayerID, cursor, sizeof(uint8_t));
    cursor += sizeof(uint8_t);
    
    uint32_t userIDSize = 0;
    memcpy(&userIDSize, cursor, sizeof(uint32_t));
    cursor += sizeof(uint32_t);
    
    userID.assign(cursor, userIDSize);
    cursor += userIDSize;
    
    uint32_t userNameSize = 0;
    memcpy(&userNameSize, cursor, sizeof(uint32_t));
    cursor += sizeof(uint32_t);
    
    userName.assign(cursor, userNameSize);
    
    return true;
}

GameMessage_SharedPtr GameMessage::makeHandshakeServerApprovalMsg(const bool approved) {
    GameMessage_SharedPtr msg = GameMessage_SharedPtr(new vx::GameMessage);
    
    size_t len = sizeof(uint8_t) + // type
    sizeof(uint8_t); // 0: false, 1: true
    
    char *bytes = static_cast<char*>(malloc(len));
    msg->_payload = Connection::Payload::create(bytes, len);
    
    char *cursor = bytes;
    uint8_t type = TYPE_CLIENT_HANDSHAKE_SERVER_APPROVAL;
    
    memcpy(cursor, &type, sizeof(uint8_t)); // type
    cursor += sizeof(uint8_t);
    
    uint8_t b = approved ? 1 : 0;
    memcpy(cursor, &b, sizeof(uint8_t));
    
    return msg;
}

bool GameMessage::decodeHandshakeServerApprovalMsg(const GameMessage_SharedPtr msg, bool &approved) {
    if (msg->msgtype() != TYPE_CLIENT_HANDSHAKE_SERVER_APPROVAL) { return false; }
    
    char *cursor = msg->_payload->getContent();
    cursor += sizeof(uint8_t); // skip type
    
    uint8_t b;
    memcpy(&b, cursor, sizeof(uint8_t));
    
    approved = (b == 1);
    
    return true;
}

GameMessage_SharedPtr GameMessage::makeReloadWorldMsg() {
    
    GameMessage_SharedPtr msg = GameMessage_SharedPtr(new vx::GameMessage);
    
    size_t len = sizeof(uint8_t); // tye
    
    char *bytes = static_cast<char*>(malloc(len));
    msg->_payload = Connection::Payload::create(bytes, len);
    
    char *cursor = bytes;
    uint8_t type = TYPE_RELOAD_WORLD;
    
    memcpy(cursor, &type, sizeof(uint8_t)); // type
    
    return msg;
}

bool GameMessage::decodeReloadWorldMsg(GameMessage_SharedPtr msg) {
    if (msg->msgtype() != TYPE_RELOAD_WORLD) { return false; }
    return true;
}

GameMessage_SharedPtr GameMessage::makePlayerJoined(const uint8_t& playerID,
                                                    const std::string& userID,
                                                    const std::string& userName) {
    
    const uint32_t userIDSize = static_cast<const uint32_t>(userID.size());
    const uint32_t userNameSize = static_cast<const uint32_t>(userName.size());
    
    GameMessage_SharedPtr msg = GameMessage_SharedPtr(new vx::GameMessage);
    
    size_t len = sizeof(uint8_t) + // type
    sizeof(uint8_t) + // player ID
    sizeof(uint32_t) + // userIDSize
    userIDSize + // userID
    sizeof(uint32_t) + // userNameSize
    userNameSize; // userName
    
    char *bytes = static_cast<char*>(malloc(len));
    msg->_payload = Connection::Payload::create(bytes, len);
    
    char *cursor = bytes;
    uint8_t type = TYPE_SERVER_PLAYERJOINED;
    
    memcpy(cursor, &type, sizeof(uint8_t)); // type
    cursor += sizeof(uint8_t);
    
    memcpy(cursor, &playerID, sizeof(uint8_t)); // player ID
    cursor += sizeof(uint8_t);
    
    memcpy(cursor, &userIDSize, sizeof(uint32_t)); // userID size
    cursor += sizeof(uint32_t);
    
    memcpy(cursor, userID.c_str(), userIDSize); // userID
    cursor += userIDSize;
    
    memcpy(cursor, &userNameSize, sizeof(uint32_t)); // userName size
    cursor += sizeof(uint32_t);
    
    memcpy(cursor, userName.c_str(), userNameSize); // userName
    // cursor += userNameSize;
    
    return msg;
}

bool GameMessage::decodePlayerJoined(GameMessage_SharedPtr msg,
                                     uint8_t& playerID,
                                     std::string& userID,
                                     std::string& userName) {
    
    if (msg->msgtype() != TYPE_SERVER_PLAYERJOINED) { return false; }
    
    char *cursor = msg->_payload->getContent();
    cursor += sizeof(uint8_t); // skip type
    
    memcpy(&playerID, cursor, sizeof(uint8_t));
    cursor += sizeof(uint8_t);
    
    uint32_t userIDSize = 0;
    memcpy(&userIDSize, cursor, sizeof(uint32_t));
    cursor += sizeof(uint32_t);
    
    userID.assign(cursor, userIDSize);
    cursor += userIDSize;
    
    uint32_t userNameSize = 0;
    memcpy(&userNameSize, cursor, sizeof(uint32_t));
    cursor += sizeof(uint32_t);
    
    userName.assign(cursor, userNameSize);
    // cursor += userNameSize;
    
    return true;
}

/// server : server left

GameMessage_SharedPtr GameMessage::makePlayerLeft(const uint8_t& playerID) {

    GameMessage_SharedPtr msg = GameMessage_SharedPtr(new vx::GameMessage);
    
    size_t len = sizeof(uint8_t) + // type
    sizeof(uint8_t); // player ID
    
    char *bytes = static_cast<char*>(malloc(len));
    msg->_payload = Connection::Payload::create(bytes, len);
    
    char *cursor = bytes;
    uint8_t type = TYPE_SERVER_PLAYERLEFT;
    
    memcpy(cursor, &type, sizeof(uint8_t)); // type
    cursor += sizeof(uint8_t);
    
    memcpy(cursor, &playerID, sizeof(uint8_t)); // player ID
    
    return msg;
}

bool GameMessage::decodePlayerLeft(GameMessage_SharedPtr msg, uint8_t& playerID) {
    
    if (msg->msgtype() != TYPE_SERVER_PLAYERLEFT) { return false; }
    
    char *cursor = msg->_payload->getContent();
    cursor += sizeof(uint8_t); // skip type
    
    memcpy(&playerID, cursor, sizeof(uint8_t));
    // cursor += sizeof(uint8_t);
    
    return true;
}

GameMessage_SharedPtr GameMessage::makeGameEvent(const uint8_t& eventType,
                                                 const uint8_t& senderID,
                                                 const std::vector<uint8_t>& recipientIDs,
                                                 const uint8_t *data,
                                                 const size_t size) {
    
    if (recipientIDs.size() > MAX_RECIPIENTS) {
        vxlog_error("trying to send event to too many recipients");
        return nullptr;
    }
    
    if (size > GAME_EVENT_BIG_DATA_MAX_SIZE) {
        vxlog_error("trying to send too much data in one event");
        return nullptr;
    }
    
    bool big = size > GAME_EVENT_DATA_MAX_SIZE;
    bool small = size <= GAME_EVENT_SMALL_DATA_MAX_SIZE;
    
    // NOTE: Maybe one day we'll have to make sure there aren't more than 255 recipients here...
    const uint8_t nbRecipients = static_cast<const uint8_t>(recipientIDs.size());
    
    GameMessage_SharedPtr msg(new GameMessage);
    
    size_t len = (sizeof(uint8_t) + // GameMessage type
                  sizeof(uint8_t) + // eventType
                  sizeof(uint8_t) + // senderID
                  sizeof(uint8_t) + // nbRecipients (number of recipients)
                  sizeof(uint8_t) * nbRecipients + // recipient IDs
                  (big ? sizeof(GAME_EVENT_BIG_DATA_SIZE_TYPE) :
                   small ? sizeof(GAME_EVENT_SMALL_DATA_SIZE_TYPE) :
                   sizeof(GAME_EVENT_DATA_SIZE_TYPE)) + // data size
                  size); // data
    
    char *bytes = static_cast<char*>(malloc(len));
    
    if (eventType == EVENT_TYPE_FROM_SCRIPT_WITH_DEBUG) {
        msg->_payload = Connection::Payload::create(bytes, len,
                                                    Connection::Payload::Includes::PayloadID |
                                                    Connection::Payload::Includes::CreatedAt |
                                                    Connection::Payload::Includes::TravelHistory);
        msg->_payload->step("create event from script");
    } else {
        msg->_payload = Connection::Payload::create(bytes, len);
    }
    
    char *cursor = bytes;
    uint8_t type = (big ? TYPE_BIG_GAME_EVENT_BIG :
                    small ? TYPE_GAME_EVENT_SMALL :
                    TYPE_GAME_EVENT);
    
    memcpy(cursor, &type, sizeof(uint8_t)); // type
    cursor += sizeof(uint8_t);
    
    if (eventType == EVENT_TYPE_FROM_SCRIPT_WITH_DEBUG) {
        
        uint8_t eventType = EVENT_TYPE_FROM_SCRIPT;
        memcpy(cursor, &eventType, sizeof(uint8_t)); // eventType
        cursor += sizeof(uint8_t);
        
    } else {
        
        memcpy(cursor, &eventType, sizeof(uint8_t)); // eventType
        cursor += sizeof(uint8_t);
    }
    
    memcpy(cursor, &senderID, sizeof(uint8_t)); // senderID
    cursor += sizeof(uint8_t);
    
    memcpy(cursor, &nbRecipients, sizeof(uint8_t)); // nbRecipients
    cursor += sizeof(uint8_t);
    
    uint8_t recipientID = 0;
    for (int i = 0 ; i < nbRecipients ; ++i) {
        recipientID = recipientIDs.at(i);
        memcpy(cursor, &recipientID, sizeof(uint8_t)); // recipient
        cursor += sizeof(uint8_t);
    }
    
    if (big) {
        const GAME_EVENT_BIG_DATA_SIZE_TYPE dataSize = static_cast<GAME_EVENT_BIG_DATA_SIZE_TYPE>(size);
        memcpy(cursor, &dataSize, sizeof(GAME_EVENT_BIG_DATA_SIZE_TYPE)); // data size
        cursor += sizeof(GAME_EVENT_BIG_DATA_SIZE_TYPE);
    } else if (small) {
        const GAME_EVENT_SMALL_DATA_SIZE_TYPE dataSize = static_cast<GAME_EVENT_SMALL_DATA_SIZE_TYPE>(size);
        memcpy(cursor, &dataSize, sizeof(GAME_EVENT_SMALL_DATA_SIZE_TYPE)); // data size
        cursor += sizeof(GAME_EVENT_SMALL_DATA_SIZE_TYPE);
    } else {
        const GAME_EVENT_DATA_SIZE_TYPE dataSize = static_cast<GAME_EVENT_DATA_SIZE_TYPE>(size);
        memcpy(cursor, &dataSize, sizeof(GAME_EVENT_DATA_SIZE_TYPE)); // data size
        cursor += sizeof(GAME_EVENT_DATA_SIZE_TYPE);
    }
    
    memcpy(cursor, data, size); // data
    // cursor += sizeof(dataSize);
    
    return msg;
}

bool GameMessage::decodeGameEvent(GameMessage_SharedPtr msg,
                                  uint8_t& eventType,
                                  uint8_t& senderID,
                                  std::vector<uint8_t>& recipientIDs,
                                  uint8_t **data,
                                  size_t *size) {
    
    vx_assert(recipientIDs.empty() == true);
    vx_assert(data != nullptr);
    
    uint8_t msgType = msg->msgtype();
    
    if (msgType != TYPE_GAME_EVENT &&
        msgType != TYPE_GAME_EVENT_SMALL &&
        msgType != TYPE_BIG_GAME_EVENT_BIG) { return false; }
    
    
    char *cursor = msg->_payload->getContent();
    cursor += sizeof(uint8_t); // skip GameMessage type
    
    memcpy(&eventType, cursor, sizeof(uint8_t)); // event type
    cursor += sizeof(uint8_t);
    
    memcpy(&senderID, cursor, sizeof(uint8_t)); // sender ID
    cursor += sizeof(uint8_t);
    
    uint8_t nbRecipients = 0;
    memcpy(&nbRecipients, cursor, sizeof(uint8_t)); // nbRecipients
    cursor += sizeof(uint8_t);
    
    uint8_t recipientID = 0;
    for (int i = 0; i < nbRecipients; ++i) {
        memcpy(&recipientID, cursor, sizeof(uint8_t)); // recipient ID
        cursor += sizeof(uint8_t);
        recipientIDs.push_back(recipientID);
    }
    
    if (msgType == TYPE_GAME_EVENT) {
        GAME_EVENT_DATA_SIZE_TYPE dataSize = 0;
        memcpy(&dataSize, cursor, sizeof(GAME_EVENT_DATA_SIZE_TYPE)); // data size
        cursor += sizeof(GAME_EVENT_DATA_SIZE_TYPE);
        *size = static_cast<size_t>(dataSize);
    } else if (msgType == TYPE_GAME_EVENT_SMALL) {
        GAME_EVENT_SMALL_DATA_SIZE_TYPE dataSize = 0;
        memcpy(&dataSize, cursor, sizeof(GAME_EVENT_SMALL_DATA_SIZE_TYPE)); // data size
        cursor += sizeof(GAME_EVENT_SMALL_DATA_SIZE_TYPE);
        *size = static_cast<size_t>(dataSize);
    } else { // big
        GAME_EVENT_BIG_DATA_SIZE_TYPE dataSize = 0;
        memcpy(&dataSize, cursor, sizeof(GAME_EVENT_BIG_DATA_SIZE_TYPE)); // data size
        cursor += sizeof(GAME_EVENT_BIG_DATA_SIZE_TYPE);
        *size = static_cast<size_t>(dataSize);
    }
    
    // data
    if (*data != nullptr) { free(*data); }
    *data = static_cast<uint8_t*>(malloc(*size));
    memcpy(*data, cursor, *size); // data
    
    return true;
}

bool GameMessage::decodeGameEventMeta(GameMessage_SharedPtr msg,
                                      uint8_t& eventType,
                                      uint8_t& senderID,
                                      std::vector<uint8_t>& recipientIDs) {
    
    vx_assert(recipientIDs.empty() == true);
    
    uint8_t msgType = msg->msgtype();
    
    if (msgType != TYPE_GAME_EVENT &&
        msgType != TYPE_GAME_EVENT_SMALL &&
        msgType != TYPE_BIG_GAME_EVENT_BIG) { return false; }
    
    char *cursor = msg->_payload->getContent();
    cursor += sizeof(uint8_t); // skip GameMessage type
    
    memcpy(&eventType, cursor, sizeof(uint8_t)); // event type
    cursor += sizeof(uint8_t);
    
    memcpy(&senderID, cursor, sizeof(uint8_t)); // sender ID
    cursor += sizeof(uint8_t);
    
    uint8_t nbRecipients = 0;
    memcpy(&nbRecipients, cursor, sizeof(uint8_t)); // nbRecipients
    cursor += sizeof(uint8_t);
    
    uint8_t recipientID = 0;
    for (int i = 0; i < nbRecipients; ++i) {
        memcpy(&recipientID, cursor, sizeof(uint8_t)); // recipient ID
        cursor += sizeof(uint8_t);
        recipientIDs.push_back(recipientID);
    }
    
    return true;
}

bool GameMessage::decodeGameEventBytes(GameMessage_SharedPtr msg,
                                       uint8_t **data,
                                       size_t *size) {
    vx_assert(data != nullptr);
    
    uint8_t msgType = msg->msgtype();
    
    if (msgType != TYPE_GAME_EVENT &&
        msgType != TYPE_GAME_EVENT_SMALL &&
        msgType != TYPE_BIG_GAME_EVENT_BIG) { return false; }
    
    char *cursor = msg->_payload->getContent();
    cursor += sizeof(uint8_t); // skip GameMessage type
    cursor += sizeof(uint8_t); // skip event type
    cursor += sizeof(uint8_t); // skip sender ID
    
    uint8_t nbRecipients = 0;
    memcpy(&nbRecipients, cursor, sizeof(uint8_t)); // nbRecipients
    cursor += sizeof(uint8_t);
    
    cursor += sizeof(uint8_t) * nbRecipients; // skip recipients
    
    if (msgType == TYPE_GAME_EVENT) {
        GAME_EVENT_DATA_SIZE_TYPE dataSize = 0;
        memcpy(&dataSize, cursor, sizeof(GAME_EVENT_DATA_SIZE_TYPE)); // data size
        cursor += sizeof(GAME_EVENT_DATA_SIZE_TYPE);
        *size = static_cast<size_t>(dataSize);
    } else if (msgType == TYPE_GAME_EVENT_SMALL) {
        GAME_EVENT_SMALL_DATA_SIZE_TYPE dataSize = 0;
        memcpy(&dataSize, cursor, sizeof(GAME_EVENT_SMALL_DATA_SIZE_TYPE)); // data size
        cursor += sizeof(GAME_EVENT_SMALL_DATA_SIZE_TYPE);
        *size = static_cast<size_t>(dataSize);
    } else { // big
        GAME_EVENT_BIG_DATA_SIZE_TYPE dataSize = 0;
        memcpy(&dataSize, cursor, sizeof(GAME_EVENT_BIG_DATA_SIZE_TYPE)); // data size
        cursor += sizeof(GAME_EVENT_BIG_DATA_SIZE_TYPE);
        *size = static_cast<size_t>(dataSize);
    }
    
    // data
    if (*data != nullptr) { free(*data); }
    *data = static_cast<uint8_t*>(malloc(*size));
    memcpy(*data, cursor, *size); // data
    
    return true;
}

// Chat Message
GameMessage_SharedPtr GameMessage::makeChatMessage(const uint8_t& senderID,
                                                   const std::string& senderUserID,
                                                   const std::string& senderUserName,
                                                   const std::string& localUUID,
                                                   // assigned by server, provide "00000000-0000-0000-0000-000000000000" when creating message from client
                                                   const std::string& UUID,
                                                   const std::vector<uint8_t>& recipientIDs,
                                                   const std::string& chatMessage) {

    if (recipientIDs.size() > MAX_RECIPIENTS) {
        vxlog_error("trying to send event to too many recipients");
        return nullptr;
    }

    if (senderUserID.size() != 36) {
        vxlog_error("userID's size should be 36");
        return nullptr;
    }

    if (senderUserName.size() > 255) {
        vxlog_error("username's size shouldn't be greather than 255");
        return nullptr;
    }

    if (localUUID.size() != 36) {
        vxlog_error("localUUID's size should be 36");
        return nullptr;
    }

    if (UUID.size() != 36) {
        vxlog_error("UUID's size should be 36");
        return nullptr;
    }

    if (chatMessage.size() > 65535) {
        vxlog_error("chat message's size shouldn't be greather than 65535");
        return nullptr;
    }

    const uint8_t senderUserNameSize = static_cast<const uint8_t>(senderUserName.size());
    const uint16_t chatMessageSize = static_cast<const uint16_t>(chatMessage.size());

    vx::GameMessage *gm = new vx::GameMessage();
    if (gm == nullptr) {
        vxlog_error("can't malloc GameMessage");
        return nullptr;
    }
    GameMessage_SharedPtr msg = GameMessage_SharedPtr(gm);

    const uint8_t nbRecipients = static_cast<const uint8_t>(recipientIDs.size());

    size_t len = (sizeof(uint8_t) + // GameMessage type
                  sizeof(uint8_t) + // senderID
                  36 + // userID size
                  sizeof(uint8_t) + // username size
                  senderUserNameSize + // username
                  36 + // local UUID size
                  36 + // UUID size
                  sizeof(uint8_t) + // nbRecipients (number of recipients)
                  sizeof(uint8_t) * nbRecipients + // recipient IDs
                  sizeof(uint16_t) + // chat message size
                  chatMessageSize // chat message size
                  ); // data

    char *bytes = static_cast<char*>(malloc(len));
    msg->_payload = Connection::Payload::create(bytes, len);

    char *cursor = bytes;
    uint8_t type = TYPE_CHAT_MESSAGE;

    memcpy(cursor, &type, sizeof(uint8_t)); // type
    cursor += sizeof(uint8_t);

    memcpy(cursor, &senderID, sizeof(uint8_t));
    cursor += sizeof(uint8_t);

    memcpy(cursor, senderUserID.c_str(), 36);
    cursor += 36;

    memcpy(cursor, &senderUserNameSize, sizeof(uint8_t));
    cursor += sizeof(uint8_t);

    memcpy(cursor, senderUserName.c_str(), senderUserNameSize);
    cursor += senderUserNameSize;

    memcpy(cursor, localUUID.c_str(), 36);
    cursor += 36;

    memcpy(cursor, UUID.c_str(), 36);
    cursor += 36;

    memcpy(cursor, &nbRecipients, sizeof(uint8_t)); // nbRecipients
    cursor += sizeof(uint8_t);

    uint8_t recipientID = 0;
    for (int i = 0 ; i < nbRecipients ; ++i) {
        recipientID = recipientIDs.at(i);
        memcpy(cursor, &recipientID, sizeof(uint8_t)); // recipient
        cursor += sizeof(uint8_t);
    }

    memcpy(cursor, &chatMessageSize, sizeof(uint16_t));
    cursor += sizeof(uint16_t);

    memcpy(cursor, chatMessage.c_str(), chatMessageSize);

    return msg;
}

bool GameMessage::chatMessageAttachUUID(GameMessage_SharedPtr msg, const std::string& uuid) {
    if (msg->msgtype() != TYPE_CHAT_MESSAGE) {
        return false;
    }

    char *cursor = msg->_payload->getContent();
    cursor += sizeof(uint8_t) * 2 + 36; // skip type + conn ID + sender userID

    uint8_t n = 0;
    memcpy(&n, cursor, sizeof(uint8_t));

    cursor += sizeof(uint8_t) + n + 36; // skip username size + username + local UUID

    memcpy(cursor, uuid.c_str(), 36); // write UUID

    return true;
}

bool GameMessage::decodeChatMessage(GameMessage_SharedPtr msg,
                                    uint8_t& senderID,
                                    std::string& senderUserID,
                                    std::string& senderUserName,
                                    std::string& localUUID,
                                    std::string& UUID,
                                    std::vector<uint8_t>& recipientIDs,
                                    std::string& chatMessage) {

    if (msg->msgtype() != TYPE_CHAT_MESSAGE) {
        return false;
    }

    uint8_t n = 0;

    char *cursor = msg->_payload->getContent();
    cursor += sizeof(uint8_t); // skip type

    memcpy(&senderID, cursor, sizeof(uint8_t)); // sender ID
    cursor += sizeof(uint8_t);

    senderUserID.assign(cursor, 36);
    cursor += 36;

    memcpy(&n, cursor, sizeof(uint8_t)); // sender username size
    cursor += sizeof(uint8_t);

    senderUserName.assign(cursor, n);
    cursor += n;

    localUUID.assign(cursor, 36);
    cursor += 36;

    UUID.assign(cursor, 36);
    cursor += 36;

    memcpy(&n, cursor, sizeof(uint8_t)); // nbRecipients
    cursor += sizeof(uint8_t);

    uint8_t recipientID = 0;
    for (int i = 0; i < n; ++i) {
        memcpy(&recipientID, cursor, sizeof(uint8_t)); // recipient ID
        cursor += sizeof(uint8_t);
        recipientIDs.push_back(recipientID);
    }

    uint16_t n16 = 0;
    memcpy(&n16, cursor, sizeof(uint16_t)); // chat message size
    cursor += sizeof(uint16_t);

    chatMessage.assign(cursor, n16);

    return true;
}

GameMessage_SharedPtr GameMessage::makeChatMessageACK(const std::string& localUUID,
                                                      const std::string& UUID,
                                                      const uint8_t& status) {

    if (localUUID.size() != 36) {
        vxlog_error("localUUID's size should be 36");
        return nullptr;
    }

    if (UUID.size() != 36) {
        vxlog_error("UUID's size should be 36");
        return nullptr;
    }

    vx::GameMessage *gm = new vx::GameMessage();
    if (gm == nullptr) {
        vxlog_error("can't malloc GameMessage");
        return nullptr;
    }
    GameMessage_SharedPtr msg = GameMessage_SharedPtr(gm);

    size_t len = (sizeof(uint8_t) + // GameMessage type
                  36 + // local UUID size
                  36 + // UUID size
                  sizeof(uint8_t) // status
                  ); // data

    char *bytes = static_cast<char*>(malloc(len));
    msg->_payload = Connection::Payload::create(bytes, len);

    char *cursor = bytes;
    uint8_t type = TYPE_CHAT_MESSAGE_ACK;

    memcpy(cursor, &type, sizeof(uint8_t)); // type
    cursor += sizeof(uint8_t);

    memcpy(cursor, localUUID.c_str(), 36);
    cursor += 36;

    memcpy(cursor, UUID.c_str(), 36);
    cursor += 36;

    memcpy(cursor, &status, sizeof(uint8_t)); // status

    return msg;
}

bool GameMessage::decodeChatMessageACK(GameMessage_SharedPtr msg,
                                       std::string& localUUID,
                                       std::string& UUID,
                                       uint8_t& status) {

    if (msg->msgtype() != TYPE_CHAT_MESSAGE_ACK) {
        return false;
    }

    char *cursor = msg->_payload->getContent();
    cursor += sizeof(uint8_t); // skip type

    localUUID.assign(cursor, 36);
    cursor += 36;

    UUID.assign(cursor, 36);
    cursor += 36;

    memcpy(&status, cursor, sizeof(uint8_t)); // status

    return true;
}

/// Allocates a new heartbeat message
GameMessage_SharedPtr GameMessage::makeHeartbeat(const uint8_t& senderPlayerID) {
    vx::GameMessage *gm = new vx::GameMessage();
    if (gm == nullptr) {
        vxlog_error("can't malloc GameMessage");
        return nullptr;
    }
    GameMessage_SharedPtr msg = GameMessage_SharedPtr(gm);
    
    const size_t len = (sizeof(uint8_t) + // GameMessage type
                        sizeof(uint8_t)); // senderID
    
    char *bytes = static_cast<char*>(malloc(len));
    if (bytes == nullptr) {
        // memory allocation failed
        // TODO: handle error...
    }
    
    msg->_payload = Connection::Payload::create(bytes, len);

    char *cursor = msg->_payload->getContent();

    const uint8_t type = TYPE_HEARTBEAT;
    memcpy(cursor, &type, sizeof(uint8_t));
    cursor += sizeof(uint8_t);
    
    memcpy(cursor, &senderPlayerID, sizeof(uint8_t)); // player ID
    // cursor += sizeof(uint8_t);
    
    return msg;
}

/// Decodes heartbeat message
bool GameMessage::decodeHeartbeatMessage(GameMessage_SharedPtr msg,
                                         uint8_t& senderID) {
    if (msg->msgtype() != TYPE_HEARTBEAT) {
        return false;
    }
    if (msg->_payload->contentSize() != (sizeof(uint8_t) * 2)) {
        vxlog_error("[decodeHeartbeatMessage] unexpected message length");
    }
    
    char *cursor = msg->_payload->getContent();
    cursor += sizeof(uint8_t); // skip type
    
    memcpy(&senderID, cursor, sizeof(uint8_t));
    return true;
}

Connection::Payload_SharedPtr GameMessage::unloadPayload() {
    if (_payload == nullptr) {
        vxlog_error("GameMessage: unloading NULL bytes");
        return nullptr;
    }
    
    Connection::Payload_SharedPtr pl = _payload;
    _payload = nullptr;
    
    return pl;
}

Connection::Payload_SharedPtr GameMessage::copyPayload() {
    if (_payload == nullptr) {
        vxlog_error("GameMessage: copying NULL payload");
        return nullptr;
    }
    return Connection::Payload::copy(_payload);
}

GameMessage_SharedPtr GameMessage::make(const Connection::Payload_SharedPtr& payload) {
    GameMessage_SharedPtr msg = GameMessage_SharedPtr(new vx::GameMessage);
    if (msg == nullptr) { return nullptr; }
    
    msg->_payload = payload;
    
    return msg;
}
