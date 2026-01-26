//
//  VXMessageQueues.cpp
//  Cubzh
//
//  Created by Gaetan de Villele on 08/10/2020.
//

#include "VXMessageQueues.hpp"

// xptools
#include "xptools.h"

// engine
#include "game.h"

/// example of use :
// messageQueues->pushOut(3, msg)
// messageQueues->popOut(3)
// messageQueues->pushIn(3, msg)
// messageQueues->popIn(3)

using namespace vx;

MessageQueues::MessageQueues() {
    
    _inMsgReceivingQueues = static_cast<std::queue<GameMessage_SharedPtr> **>(malloc(sizeof(std::queue<GameMessage_SharedPtr> *) * GAME_PLAYER_COUNT_MAX));
    
    for (int i = 0; i < GAME_PLAYER_COUNT_MAX; i++) {
        _inMsgReceivingQueues[i] = new std::queue<GameMessage_SharedPtr>();
    }
    
    _inMsgConsumedQueues = static_cast<std::queue<GameMessage_SharedPtr> **>(malloc(sizeof(std::queue<GameMessage_SharedPtr> *) * GAME_PLAYER_COUNT_MAX));
    
    for (int i = 0; i < GAME_PLAYER_COUNT_MAX; i++) {
        _inMsgConsumedQueues[i] = new std::queue<GameMessage_SharedPtr>();
    }
}

MessageQueues::~MessageQueues() {
    
    for (int i = 0; i < GAME_PLAYER_COUNT_MAX; i++) {
        delete _inMsgReceivingQueues[i];
    }
    free(_inMsgReceivingQueues);
    _inMsgReceivingQueues = nullptr;
    
    for (int i = 0; i < GAME_PLAYER_COUNT_MAX; i++) {
        delete _inMsgConsumedQueues[i];
    }
    free(_inMsgConsumedQueues);
    _inMsgConsumedQueues = nullptr;
}

///
void MessageQueues::pushOut(const uint8_t& playerID, const GameMessage_SharedPtr& msg) {
    if (game_isRecipientIDValid(playerID) == false) {
        vxlog_error("[%d] this should not happen", __LINE__);
        return;
    }
    
    {
        const std::lock_guard<std::mutex> locker(_outMsgQueueMutexes[playerID]);
        _outMsgQueues[playerID].push(msg);
    }
}

///
vx::GameMessage_SharedPtr MessageQueues::popOut(const uint8_t& playerID) {
    if (game_isRecipientIDValid(playerID) == false) {
        vxlog_error("[%d] this should not happen", __LINE__);
        return nullptr;
    }
    
    GameMessage_SharedPtr msg = nullptr;
    {
        const std::lock_guard<std::mutex> locker(_outMsgQueueMutexes[playerID]);
        if (_outMsgQueues[playerID].empty() == false) {
            msg = _outMsgQueues[playerID].front();
            _outMsgQueues[playerID].pop();
        }
    }
    return msg;
}

///
void MessageQueues::pushIn(const uint8_t& playerID, const GameMessage_SharedPtr& msg) {
    if (game_isRecipientIDValid(playerID) == false) {
        vxlog_error("[%d] this should not happen", __LINE__);
        return;
    }
    
    {
        const std::lock_guard<std::mutex> locker(_inMsgQueueMutex);
        _inMsgReceivingQueues[playerID]->push(msg);
    }
}

///
vx::GameMessage_SharedPtr MessageQueues::popIn(const uint8_t &playerID) {
    vx_assert(game_isRecipientIDValid(playerID) == true);
    
    GameMessage_SharedPtr msg = nullptr;
    if (_inMsgConsumedQueues[playerID]->empty() == false) {
        msg = _inMsgConsumedQueues[playerID]->front();
        _inMsgConsumedQueues[playerID]->pop();
    }
    return msg;
}

void MessageQueues::toggleIn() {
    static std::queue<GameMessage_SharedPtr> **tmp;
    const std::lock_guard<std::mutex> locker(_inMsgQueueMutex);
    tmp = _inMsgReceivingQueues;
    _inMsgReceivingQueues = _inMsgConsumedQueues;
    _inMsgConsumedQueues = tmp;
    // empty inMsgReceivingQueues
    for (int i = 0; i < GAME_PLAYER_COUNT_MAX; i++) {
        while (_inMsgReceivingQueues[i]->empty() == false) {
            _inMsgReceivingQueues[i]->pop();
        }
    }
}

// TODO: remove this function when using toggle logic for out queues
void MessageQueues::flushQueuesForPlayerID(const uint8_t &playerID) {
    vx_assert(game_isRecipientIDValid(playerID) == true);
    // flushing "out" queue
    {
        const std::lock_guard<std::mutex> locker(_outMsgQueueMutexes[playerID]);
        while (_outMsgQueues[playerID].empty() == false) {
            _outMsgQueues[playerID].pop();
        }
    }
    {
        const std::lock_guard<std::mutex> locker(_inMsgQueueMutex);
        while (_inMsgReceivingQueues[playerID]->empty() == false) {
            _inMsgReceivingQueues[playerID]->pop();
        }
    }
}
