//
//  VXHubRequestSender.hpp
//  Cubzh
//
//  Created by Gaetan de Villele on 30/04/2020.
//

#pragma once

// C++
#include <memory>

namespace vx {

class HubRequestSender;
typedef std::shared_ptr<HubRequestSender> HubRequestSender_SharedPtr;
typedef std::weak_ptr<HubRequestSender> HubRequestSender_WeakPtr;

class HubRequestSender {
    
public:
    
    HubRequestSender();
    virtual ~HubRequestSender();
    
};

} // namespace vx
