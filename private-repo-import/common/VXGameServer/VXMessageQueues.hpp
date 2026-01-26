//
//  VXMessageQueues.hpp
//  Cubzh
//
//  Created by Gaetan de Villele on 08/10/2020.
//

#pragma once

// C++
#include <mutex>
#include <queue>

// VX Networking
#include "VXGameMessage.hpp"

namespace vx {

// TODO: gdevillele: make this a template class with <typename T>
class MessageQueues final {
    
public:
    
    MessageQueues();
    ~MessageQueues();
    
    void pushOut(const uint8_t &playerID, const GameMessage_SharedPtr& msg);
    GameMessage_SharedPtr popOut(const uint8_t &playerID);
    
    void pushIn(const uint8_t &playerID, const GameMessage_SharedPtr& msg);
    void toggleIn();
    GameMessage_SharedPtr popIn(const uint8_t &playerID);
    
    void flushQueuesForPlayerID(const uint8_t &playerID);
    
private:
    
    /// 16 "out" queues
    std::queue<GameMessage_SharedPtr> _outMsgQueues[16];
    
    /// 16 "in" queues
    std::queue<GameMessage_SharedPtr> **_inMsgReceivingQueues;
    std::queue<GameMessage_SharedPtr> **_inMsgConsumedQueues;
    
    ///
    std::mutex _outMsgQueueMutexes[16];
    
    ///
    std::mutex _inMsgQueueMutex;
};

} // namespace vx
