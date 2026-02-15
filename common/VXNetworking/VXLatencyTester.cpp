//
//  VXLatencyTester.cpp
//  Cubzh
//
//  Created by Gaetan de Villele on 17/01/2023.
//

#include "VXLatencyTester.hpp"

// C++


// vx
#include "VXHubClient.hpp"

// xptools
#include "OperationQueue.hpp"

#define DEFAULT_REGION Region::Europe
#define NB_REGIONS 2
#define LATENCY_INVALID -1

using namespace vx;

// MARK: - static -

LatencyTester& LatencyTester::shared() {
    static LatencyTester lt;
    return lt;
}

// MARK: - constructor / destructor -

LatencyTester::LatencyTester() :
_mutex(),
_pingRegionCount(0),
_pingCountEurope(0),
_pingSumEurope(0),
_latencyMsEurope(LATENCY_INVALID),
_pingCountUSA(0),
_pingSumUSA(0),
_latencyMsUSA(LATENCY_INVALID),
_preferredRegion(DEFAULT_REGION),
_preferredLatency(LATENCY_INVALID) {}

LatencyTester::~LatencyTester() {}

bool LatencyTester::ping() {
    // if ping process is already running we return false
    if (this->_setPingRegionCountIfZero(NB_REGIONS) == false) {
        return false;
    }

    // Reset ping count/sum
    
    _pingCountEurope = 0;
    _pingSumEurope = 0;

    _pingCountUSA = 0;
    _pingSumUSA = 0;

    // 🌍 ping Europe region
    {
        // send request asynchronously
        // 1st ping
        HubClient::getInstance().pingRegion(HubClient::getInstance().getUniversalSender(), Region::Europe, [this](const bool& ok, const uint32_t& latency, const HubClientError& err) {
            if (ok && err == HubClientError::NONE) {
                _pingCountEurope += 1;
                this->_pingSumEurope += latency;
            }
            // 2nd ping
            HubClient::getInstance().pingRegion(HubClient::getInstance().getUniversalSender(), Region::Europe, [this](const bool& ok, const uint32_t& latency, const HubClientError& err) {
                if (ok && err == HubClientError::NONE) {
                    _pingCountEurope += 1;
                    this->_pingSumEurope += latency;
                }
                // 3rd ping
                HubClient::getInstance().pingRegion(HubClient::getInstance().getUniversalSender(), Region::Europe, [this](const bool& ok, const uint32_t& latency, const HubClientError& err) {
                    if (ok && err == HubClientError::NONE) {
                        _pingCountEurope += 1;
                        this->_pingSumEurope += latency;
                    }
                    this->_setAverageLatencyForRegion(Region::Europe, _pingCountEurope > 0 ? _pingSumEurope / _pingCountEurope : LATENCY_INVALID);
                });
            });
        });
    }

    // 🌍 ping USA region
    {
        // send request asynchronously
        // 1st ping
        HubClient::getInstance().pingRegion(HubClient::getInstance().getUniversalSender(), Region::USA, [this](const bool& ok, const uint32_t& latency, const HubClientError& err) {
            if (ok && err == HubClientError::NONE) {
                _pingCountUSA += 1;
                this->_pingSumUSA += latency;
            }
            // 2nd ping
            HubClient::getInstance().pingRegion(HubClient::getInstance().getUniversalSender(), Region::USA, [this](const bool& ok, const uint32_t& latency, const HubClientError& err) {
                if (ok && err == HubClientError::NONE) {
                    _pingCountUSA += 1;
                    this->_pingSumUSA += latency;
                }
                // 3rd ping
                HubClient::getInstance().pingRegion(HubClient::getInstance().getUniversalSender(), Region::USA, [this](const bool& ok, const uint32_t& latency, const HubClientError& err) {
                    if (ok && err == HubClientError::NONE) {
                        _pingCountUSA += 1;
                        this->_pingSumUSA += latency;
                    }
                    this->_setAverageLatencyForRegion(Region::USA, _pingCountUSA > 0 ? _pingSumUSA / _pingCountUSA : LATENCY_INVALID);
                });
            });
        });
    }

    // 🌍 ping Singapore region
    {
        // send request asynchronously
        // 1st ping
        HubClient::getInstance().pingRegion(HubClient::getInstance().getUniversalSender(), Region::Singapore, [this](const bool& ok, const uint32_t& latency, const HubClientError& err) {
            if (ok && err == HubClientError::NONE) {
                _pingCountSG += 1;
                this->_pingSumSG += latency;
            }
            // 2nd ping
            HubClient::getInstance().pingRegion(HubClient::getInstance().getUniversalSender(), Region::Singapore, [this](const bool& ok, const uint32_t& latency, const HubClientError& err) {
                if (ok && err == HubClientError::NONE) {
                    _pingCountSG += 1;
                    this->_pingSumSG += latency;
                }
                // 3rd ping
                HubClient::getInstance().pingRegion(HubClient::getInstance().getUniversalSender(), Region::Singapore, [this](const bool& ok, const uint32_t& latency, const HubClientError& err) {
                    if (ok && err == HubClientError::NONE) {
                        _pingCountSG += 1;
                        this->_pingSumSG += latency;
                    }
                    this->_setAverageLatencyForRegion(Region::Singapore, _pingCountSG > 0 ? _pingSumSG / _pingCountSG : LATENCY_INVALID);
                });
            });
        });
    }

    return true;
}

Region::Type LatencyTester::getPreferredRegion() {
    Region::Type value = DEFAULT_REGION;
    _mutex.lock();
    value = _preferredRegion;
    _mutex.unlock();
    return value;
}

bool LatencyTester::_setPingRegionCountIfZero(const uint8_t nbRegions) {
    bool ret = false;
    _mutex.lock();
    if (_pingRegionCount == 0) {
        // reset _preferredLatency
        _preferredLatency = LATENCY_INVALID;
        _pingRegionCount = nbRegions;
        ret = true;
    }
    _mutex.unlock();
    return ret;
}

void LatencyTester::_setAverageLatencyForRegion(Region::Type regionUpdated, int avgLatency) {
    _mutex.lock();
    switch (regionUpdated) {
        case Region::Europe:
            _latencyMsEurope = avgLatency;
            if (_latencyMsEurope != LATENCY_INVALID && (_preferredLatency == LATENCY_INVALID || _latencyMsEurope < _preferredLatency)) {
                _preferredRegion = Region::Europe;
                _preferredLatency = _latencyMsEurope;
            }
            break;
        case Region::USA:
            _latencyMsUSA = avgLatency;
            if (_latencyMsUSA != LATENCY_INVALID && (_preferredLatency == LATENCY_INVALID || _latencyMsUSA < _preferredLatency)) {
                _preferredRegion = Region::USA;
                _preferredLatency = _latencyMsUSA;
            }
            break;
        case Region::Singapore:
            _latencyMsSG = avgLatency;
            if (_latencyMsSG != LATENCY_INVALID && (_preferredLatency == LATENCY_INVALID || _latencyMsSG < _preferredLatency)) {
                _preferredRegion = Region::Singapore;
                _preferredLatency = _latencyMsSG;
            }
            break;
        // OTHER REGIONS...
        // default:
        //     vxlog_error("unsupported region!");
        //     return;
    }
    
//    vxlog_info("PING: EU: %d - US: %d - SG: %d - preferred: %d",
//               _latencyMsEurope,
//               _latencyMsUSA,
//               _latencyMsSG,
//               _preferredLatency);
    
    _pingRegionCount -= 1;
    _mutex.unlock();
}
