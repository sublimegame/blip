//
//  VXHubPrivateClient.cpp
//  Cubzh
//
//  Created by Gaetan de Villele on 11/09/2020.
//

#include "VXHubPrivateClient.hpp"

#include "VXHubClient.hpp"

#include "VXConfig.hpp"

// xptools
#include "xptools.h"
#include "OperationQueue.hpp"

using namespace vx;

std::shared_ptr<HubPrivateClientRequest> HubPrivateClientRequest::make(HubPrivateClientRequest_SharedPtr sender, HubPrivateClientRequest::Callback callback) {
    HubPrivateClientRequest *r = new HubPrivateClientRequest(callback);
    HubPrivateClientRequest_SharedPtr sp = HubPrivateClientRequest_SharedPtr(r);

    return sp;
}

HubPrivateClientRequest::HubPrivateClientRequest(HubPrivateClientRequest::Callback callback) : _callback(callback) {}

vx::HubPrivateClient::HubPrivateClient(const std::string& serverAddr,
                                       const uint16_t& serverPort,
                                       const bool& serverSecure) :
_serverAddress(serverAddr),
_serverPort(serverPort),
_serverSecure(serverSecure) {}

vx::HubPrivateClient::~HubPrivateClient() {}

void vx::HubPrivateClient::updateGameServerInfo(const std::string &gameServerID,
                                                const int& players,
                                                const std::string &hubAuthToken,
                                                bool &success,
                                                bool &shouldExit,
                                                std::string &err) {

    const std::string path = PRIVATE_API_PATH_PREFIX + "/server/" + gameServerID + "/info";

    HttpRequest_SharedPtr request = HttpRequest::make("POST",
                                                      this->_serverAddress,
                                                      this->_serverPort,
                                                      path,
                                                      QueryParams(),
                                                      this->_serverSecure);
    request->setHeaders({
        { "Authorization", "Bearer " + hubAuthToken }
    });

    cJSON *bodyJson = cJSON_CreateObject();
    json::writeStringField(bodyJson, "id", gameServerID);
    json::writeIntField(bodyJson, "players", players);

    char *s = cJSON_PrintUnformatted(bodyJson);
    const std::string bodyStr = std::string(s);
    free(s);

    cJSON_Delete(bodyJson);

    request->setBodyBytes(bodyStr);

    request->sendSync();

    // Process response
    HttpResponse& response = request->getResponse();
    success = response.getSuccess() && response.getStatusCode() == 200;

    // if (success == false) {
    //     err = request->getResponse().getBytes();
    //     vxlog_error("updateGameServerInfo failed with code: %d - %s", request->getResponse().getStatusCode(), err.c_str());
    // }
}

void vx::HubPrivateClient::postChatMessage(const std::string& senderUserID,
                                           const std::vector<uint8_t>& recipients,
                                           const std::string& chatMessage,
                                           const std::string& serverID,
                                           const std::string& hubAuthToken,
                                           HttpRequestCallback callback) {

    HttpRequest_SharedPtr request = HttpRequest::make("POST",
                                                      this->_serverAddress,
                                                      this->_serverPort,
                                                      PRIVATE_API_PATH_PREFIX + "/chat/message",
                                                      QueryParams(),
                                                      this->_serverSecure);
    request->setHeaders({
        { "Authorization", "Bearer " + hubAuthToken }
    });

    cJSON *bodyJson = cJSON_CreateObject();
    json::writeStringField(bodyJson, "authorID", senderUserID);
    json::writeStringField(bodyJson, "serverID", serverID);
    json::writeStringField(bodyJson, "content", chatMessage);

    char *s = cJSON_PrintUnformatted(bodyJson);
    const std::string bodyStr = std::string(s);
    free(s);

    cJSON_Delete(bodyJson);

    request->setBodyBytes(bodyStr);
    request->setCallback(callback);
    request->sendAsync();
}

void vx::HubPrivateClient::notifyGameServerExit(const std::string &gameServerID,
                                                const std::string &hubAuthToken,
                                                bool &success,
                                                std::string &err) {

    const std::string path = PRIVATE_API_PATH_PREFIX + "/server/" + gameServerID + "/exit";

    HttpRequest_SharedPtr request = HttpRequest::make("POST",
                                                      this->_serverAddress,
                                                      this->_serverPort,
                                                      path,
                                                      QueryParams(),
                                                      this->_serverSecure);
    request->setHeaders({
        { "Authorization", "Bearer " + hubAuthToken }
    });

    const std::string bodyStr = "{}"; // empty json

    request->setBodyBytes(bodyStr);

    request->sendSync();

    // Process response
    HttpResponse& response = request->getResponse();
    std::string allBytes;
    success = (response.getSuccess() &&
               response.getStatusCode() == 200 &&
               response.readAllBytes(allBytes));
    if (success == false) {
        vxlog_error("notifyGameServerExit failed with code: %d", response.getStatusCode());
    }
}
