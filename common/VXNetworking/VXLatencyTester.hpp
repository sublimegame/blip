//
//  VXLatencyTester.hpp
//  Cubzh
//
//  Created by Gaetan de Villele on 17/01/2023.
//

#pragma once

// C++
#include <cstdint>
#include <mutex>
#include <condition_variable>

//
#include "VXHubClient.hpp"

namespace vx {

class LatencyTester final {

public:

    ///
    static LatencyTester& shared();

    /// Destructor
    ~LatencyTester();

    ///
    bool ping();

    ///
    Region::Type getPreferredRegion();

private:

    // private constructor
    LatencyTester();

    /// Called when starting to measure ping.
    /// Used to wait for all regions to be considered.
    bool _setPingRegionCountIfZero(const uint8_t increment);

    /// Called when done measuring ping for one region.
    /// Decreases ping region count.
    void _setAverageLatencyForRegion(Region::Type regionUpdated, int avgLatency);

    // Fields

    // count of regions currently being ping-ed
    std::mutex _mutex;
    uint8_t _pingRegionCount;

    // Europe
    uint8_t _pingCountEurope;
    uint32_t _pingSumEurope;
    int _latencyMsEurope;

    // USA
    uint8_t _pingCountUSA;
    uint32_t _pingSumUSA;
    int _latencyMsUSA;

    // Singapore
    uint8_t _pingCountSG;
    uint32_t _pingSumSG;
    int _latencyMsSG;

    // other regions ...

    // Favorite region
    Region::Type _preferredRegion;
    int _preferredLatency;
};

} // namespace vx
