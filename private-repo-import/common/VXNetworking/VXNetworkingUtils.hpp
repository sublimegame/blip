//
//  VXNetworkingUtils.hpp
//  Cubzh
//
//  Created by Gaetan de Villele on 20/10/2020.
//

#pragma once

// C++
#include <cstdint>

#define GRPC_MAX_SEND_MESSAGE_SIZE (5*1024*1024)
#define GRPC_MAX_RECEIVE_MESSAGE_SIZE (5*1024*1024)

namespace vx {

class VXNetworkingUtils {
    
public:
    
    static uint32_t getSteadyClockTimeMs();
    
};

}
