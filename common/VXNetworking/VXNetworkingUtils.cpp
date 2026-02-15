//
//  VXNetworkingUtils.cpp
//  Cubzh
//
//  Created by Gaetan de Villele on 20/10/2020.
//

#include "VXNetworkingUtils.hpp"

// C++
#include <chrono>

using namespace vx;

/// Returns the number of ms since server boot.
uint32_t VXNetworkingUtils::getSteadyClockTimeMs() {
    auto now_ms = std::chrono::time_point_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now());
    auto value = now_ms.time_since_epoch();
    return static_cast<uint32_t>(value.count());
}
