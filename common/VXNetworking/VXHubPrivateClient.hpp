//
//  VXHubPrivateClient.hpp
//  Cubzh
//
//  Created by Gaetan de Villele on 11/09/2020.
//

#pragma once

// C++
#include <functional>
#include <string>
#include <vector>

// xptools
#include "xptools.h"

// VX
#include "VXTickDispatcher.hpp"
#include "VXHub.hpp"
#include "HttpRequest.hpp"

namespace vx {

class HubPrivateClientRequest;
typedef std::shared_ptr<HubPrivateClientRequest> HubPrivateClientRequest_SharedPtr;
typedef std::shared_ptr<HubPrivateClientRequest> HubPrivateClientRequest_WeakPtr;

class HubPrivateClientRequest {

public:

    typedef std::function<void(std::shared_ptr<HubPrivateClientRequest> request)> Callback;

    static std::shared_ptr<HubPrivateClientRequest> make(HubPrivateClientRequest_SharedPtr sender, HubPrivateClientRequest::Callback callback);

private:

    HubPrivateClientRequest(HubPrivateClientRequest::Callback callback);

    HubPrivateClientRequest::Callback _callback;

};


/// Client for the HubPrivate service
class HubPrivateClient final {

public:
    
    ///
    enum class ErrorType {
        NONE,
        NETWORK,
        UNKNOWN,
        BAD_REQUEST,
        INTERNAL,
        UNAUTHORIZED,
        NOT_FOUND,
    };

    /// Constructor
    HubPrivateClient(const std::string& serverAddr,
                     const uint16_t& port,
                     const bool &secure);
    
    /// HubPrivateClient(const HubPrivateClient &) = default;
    HubPrivateClient(HubPrivateClient &&) = default;
    
    /// HubPrivateClient& operator=(const HubPrivateClient &) = default;
    HubPrivateClient& operator=(HubPrivateClient &&) = default;
    
    /// Destructor
    ~HubPrivateClient();

    HubPrivateClientRequest_SharedPtr newRequest(HubPrivateClientRequest::Callback callback);
    
    // Update hub information about running game server.
    // Game server gets killed if not receiving updates for 2 minutes.
    void updateGameServerInfo(const std::string &gameServerID,
                              const int& players,
                              const std::string &hubAuthToken,
                              bool &success,
                              bool &shouldExit,
                              std::string &err);
    
    // Update hub information about running game server.
    // Game server gets killed if not receiving updates for 2 minutes.
    void notifyGameServerExit(const std::string &gameServerID,
                              const std::string &hubAuthToken,
                              bool &success,
                              std::string &err);

    void postChatMessage(const std::string& senderUserID,
                         const std::vector<uint8_t>& recipients,
                         const std::string& chatMessage,
                         const std::string& serverID,
                         const std::string& hubAuthToken,
                         HttpRequestCallback callback);

private:
    
    ///
    std::string _serverAddress;
    
    ///
    uint16_t _serverPort;
    
    ///
    bool _serverSecure;
};

} // namespace vx
