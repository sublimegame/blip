//
//  VXHubClient.cpp
//  Cubzh
//
//  Created by Adrien Duermael on 3/16/20.
//

#include "VXHubClient.hpp"

#include "VXNotifications.hpp"
#include "VXApplication.hpp"
#include "VXConfig.hpp"
#include "VXResource.hpp"
#include "GameCoordinator.hpp"
#include "HttpClient.hpp"
#include "HttpCookie.hpp"

// xptools
#include "xptools.h"
#include "ThreadManager.hpp"
#include "strings.hpp"

// deps
#include "BZMD5.hpp"

#define HTTP_STATUS_CODE_OK 200
#define HTTP_STATUS_CODE_NOT_MODIFIED 304

#if !defined(__VX_USE_HTTP_CACHING)
#error __VX_USE_HTTP_CACHING not defined
#endif

using namespace vx;

std::string Region::URL(Type region) {
    switch (region) {
        case Europe:
            return "ping-eu.cu.bzh";
        case USA:
            return "ping-us.cu.bzh";
        case Singapore:
            return "ping-sg.cu.bzh";
    }
};
std::string Region::Name(Type region) {
    switch (region) {
        case Europe:
            return "eu";
        case Singapore:
            return "sg";
        case USA:
            return "us";
    }
}

// --------------------------------------------------
//
// MARK: - HubClientRequest -
//
// --------------------------------------------------

HubClientRequest_SharedPtr HubClientRequest::make(HubRequestSender_WeakPtr sender) {
    HubClientRequest* r = new HubClientRequest(sender);
    HubClientRequest_SharedPtr sp = HubClientRequest_SharedPtr(r);
    sp->_weakThis = std::weak_ptr<HubClientRequest>(sp);
    return sp;
}

HubClientRequest::HubClientRequest(HubRequestSender_WeakPtr sender) :
_state(State::idle),
_underlyingHttpRequestWeak(),
_sender(sender),
_success(false),
_err() {}

HubClientRequest::State HubClientRequest::getState() {
    const std::lock_guard<std::mutex> locker(_lock);
    return this->_state;
}

HubRequestSender_WeakPtr HubClientRequest::sender() {
    const std::lock_guard<std::mutex> locker(_lock);
    return this->_sender;
}

bool HubClientRequest::success() {
    const std::lock_guard<std::mutex> locker(_lock);
    return this->_success;
}

std::string HubClientRequest::err() {
    const std::lock_guard<std::mutex> locker(_lock);
    return this->_err;
}

bool HubClientRequest::setState(State s) {
    bool res = false;

    {
        const std::lock_guard<std::mutex> locker(_lock);

        switch (s) {
            case State::idle:
                // idle is initial state, can't set it twice
                break;
            case State::canceled:
                // it's always possible to cancel a request
                this->_state = s;
                res = true;
                break;
            case State::sent:
                if (this->_state == State::idle) {
                    this->_state = s;
                    res = true;
                }
                break;
            case State::done:
                if (this->_state == State::sent) {
                    this->_state = s;
                    res = true;
                }
                break;
        }
    }

    return res;
}

bool HubClientRequest::cancel() {
    HttpRequest_SharedPtr httpRequest = _underlyingHttpRequestWeak.lock();
    if (httpRequest != nullptr) {
        httpRequest->cancel();
    }
    return this->setState(State::canceled);
}

bool HubClientRequest::isCanceled() {
    return this->getState() == State::canceled;
}

bool HubClientRequest::done() {
    return this->setState(State::done);
}

bool HubClientRequest::isDone() {
    return this->getState() == State::done;
}

bool HubClientRequest::send() {
    const bool sent = this->setState(State::sent);
    return sent;
}

void HubClientRequest::setHttpRequest(const HttpRequest_SharedPtr& req) {
    this->_underlyingHttpRequestWeak = req;
}

// --------------------------------------------------
//
// MARK: - HubClient -
//
// --------------------------------------------------

HubClient::UserCredentials HubClient::_loadCredentialsFromDisk() {
    vx_assert(vx::ThreadManager::shared().isMainThreadOrMainThreadNotSet());

    HubClient::UserCredentials uc;

    if (fs::storageFileExists("/credentials.json") == false) {
        // credentials.json doesn't exist, and that's ok.
        return uc;
    }

    FILE *credsFile = fs::openStorageFile("/credentials.json", "rb");
    if (credsFile == nullptr) {
        return uc;
    }

    std::string content;
    const bool readOK = fs::getFileTextContentAsStringAndClose(credsFile, content);
    credsFile = nullptr;

    if (readOK == false) {
        vxlog_error("can't read credentials (1)");
        return uc;
    }

    cJSON * const jsonObj = cJSON_Parse(content.c_str());

    if (jsonObj == nullptr) {
        vxlog_error("can't read credentials (2)");
        return uc;
    }

    if (cJSON_IsObject(jsonObj) == false) {
        vxlog_error("credentials: top level object expected");
        cJSON_Delete(jsonObj);
        return uc;
    }

    // ID
    {
        const char* jsonKey = "id";
        if (cJSON_HasObjectItem(jsonObj, jsonKey) == false) {
            vxlog_error("credentials: missing id");
            cJSON_Delete(jsonObj);
            return uc;
        }
        const cJSON *value = cJSON_GetObjectItem(jsonObj, jsonKey);
        if (cJSON_IsString(value) == false) {
            vxlog_error("credentials: id should be a string");
            cJSON_Delete(jsonObj);
            return uc;
        }
        uc.ID = std::string(cJSON_GetStringValue(value));
    }

    // Token
    {
        const char* jsonKey = "token";
        if (cJSON_HasObjectItem(jsonObj, jsonKey) == false) {
            vxlog_error("credentials: missing token");
            cJSON_Delete(jsonObj);
            return uc;
        }
        const cJSON *value = cJSON_GetObjectItem(jsonObj, jsonKey);
        if (cJSON_IsString(value) == false) {
            vxlog_error("credentials: token should be a string");
            cJSON_Delete(jsonObj);
            return uc;
        }
        uc.token = std::string(cJSON_GetStringValue(value));
    }

    cJSON_Delete(jsonObj);

    return uc;
}

void HubClient::_saveCredentialsOnDisk(const UserCredentials& creds) {
    vx_assert(vx::ThreadManager::shared().isMainThread());

    FILE *credsFile = nullptr;
    cJSON *jsonObj = nullptr;

    const bool credsFileExists = fs::storageFileExists("/credentials.json");

    if (credsFileExists == false) {
        // file doesn't exist yet
        jsonObj = cJSON_CreateObject();
        if (jsonObj == nullptr) {
            vxlog_error("can't create credentials json obj");
            return;
        }

        // UserID
        {
            cJSON *userID = cJSON_CreateString(creds.ID.c_str());
            if (userID == nullptr) {
                vxlog_error("can't create userID json string");
                cJSON_Delete(jsonObj);
                return;
            }
            cJSON_AddItemToObject(jsonObj, "id", userID);
        }

        // Token
        {
            cJSON *const value = cJSON_CreateString(creds.token.c_str());
            if (value == nullptr) {
                vxlog_error("can't create token json string");
                cJSON_Delete(jsonObj);
                return;
            }
            cJSON_AddItemToObject(jsonObj, "token", value);
        }

    } else {

        // credentials file already exists

        credsFile = fs::openStorageFile("/credentials.json", "rb", 0);

        // getFileTextContentAndClose does close the file
        std::string content;
        fs::getFileTextContentAsStringAndClose(credsFile, content);
        credsFile = nullptr;
        if (content.empty()) {
            return;
        }

        // parse JSON
        jsonObj = cJSON_Parse(content.c_str());
        if (jsonObj == nullptr) {
            return;
        }
        if (cJSON_IsObject(jsonObj) == false) {
            cJSON_Delete(jsonObj);
            return;
        }

        // add credentials to the JSON
        cJSON *userID = cJSON_CreateString(creds.ID.c_str());
        if (userID == nullptr) {
            cJSON_Delete(jsonObj);
            return;
        }
        if (cJSON_HasObjectItem(jsonObj, "id")) {
            cJSON_ReplaceItemInObject(jsonObj, "id", userID);
        } else {
            cJSON_AddItemToObject(jsonObj, "id", userID);
        }

        {
            cJSON *const value = cJSON_CreateString(creds.token.c_str());
            if (value == nullptr) {
                cJSON_Delete(jsonObj);
                return;
            }
            const char* jsonKey = "token";
            if (cJSON_HasObjectItem(jsonObj, jsonKey)) {
                cJSON_ReplaceItemInObject(jsonObj, jsonKey, value);
            } else {
                cJSON_AddItemToObject(jsonObj, jsonKey, value);
            }
        }

        // 0.1.10 (188)
        // Remove old privileged token if found
        {
            const char* jsonKey = "tokenPrivileged";
            if (cJSON_HasObjectItem(jsonObj, jsonKey)) {
                cJSON_DeleteItemFromObject(jsonObj, jsonKey);
            }
        }
    }

    // write credentials json file

    char *jsonStr = cJSON_PrintUnformatted(jsonObj);
    credsFile = fs::openStorageFile("/credentials.json", "wb", strlen(jsonStr));
    if (credsFile == nullptr) {
        free(jsonStr);
        cJSON_Delete(jsonObj);
        return;
    }
    fputs(jsonStr, credsFile);
    fclose(credsFile);

    free(jsonStr);
    cJSON_Delete(jsonObj);

    // TODO: gdevillele: hide this somewhere else
    //                   replace this by a call in willResignActive maybe?
    vx::fs::syncFSToDisk();

    this->_loadCredentialsFromDisk();
}

/// can be called from any thread
HubClient::UserCredentials HubClient::getCredentials() {
    const std::lock_guard<std::mutex> lock(_credentialsMutex);
    return _credentials; // returns a copy
}

/// can be called from any thread
void HubClient::setCredentials(std::string *const id,
                               std::string *const token) {
    const std::lock_guard<std::mutex> lock(_credentialsMutex);
    if (id != nullptr) {
        _credentials.ID = *id;
    }
    if (token != nullptr) {
        _credentials.token = *token;
    }
    _credentialsWaitingToBeSavedOnDisk = true;
}

void HubClient::tick(const double dt) {
    {
        const std::lock_guard<std::mutex> lock(_credentialsMutex);
        if (_credentialsWaitingToBeSavedOnDisk) {
            this->_saveCredentialsOnDisk(_credentials);
            _credentialsWaitingToBeSavedOnDisk = false;
        }
    }

    {
        const std::lock_guard<std::mutex> locker(_lock);
        std::list<HubClientRequest_SharedPtr>::iterator it = _pendingRequests.begin();
        for (; it != _pendingRequests.end(); ) {
            const HubClientRequest_SharedPtr& req = (*it);

            if (req->sender().expired()) {
                req->cancel();
                it = _pendingRequests.erase(it);
            } else {
                it++;
            }
        }
    }
}

HubClient::HubClient() : TickListener(),
_credentials(),
_credentialsWaitingToBeSavedOnDisk(false),
_credentialsMutex(),
_serverAddress(),
_serverPort(0),
_secureConnection(false),
_universalSender(HubRequestSender_SharedPtr(new HubRequestSender)),
_pendingRequests(),
_lock() {
    // make sure we are in the main thread
    vx_assert(vx::ThreadManager::shared().isMainThreadOrMainThreadNotSet());

    // read credentials from disk and cache them in memory
    {
        const std::lock_guard<std::mutex> lock(_credentialsMutex);
        _credentials = this->_loadCredentialsFromDisk();
        _credentialsWaitingToBeSavedOnDisk = false;
    }

    // parse the API URL to define values of :
    // -> _secureConnection
    // -> _serverAddress
    // -> _serverPort

    std::string apiURL = std::string(Config::apiHost());

    vxlog_info("HubClient API URL: %s", apiURL.c_str());

    const vx::URL url = vx::URL::make(apiURL);
    vx_assert(url.isValid());
    _serverAddress = url.host();
    _serverPort = url.port();

    // define _secureConnection value and remove scheme from url
    if (url.scheme() == VX_HTTPS_SCHEME) {
        _secureConnection = true;
    }
}

HubClientError HubClient::parseError(const bool& success, const uint16_t& statusCode) {

    if (success == false) {
        return HubClientError::NETWORK;
    }

    if (statusCode >= 500) {
        return HubClientError::INTERNAL;
    } else if (statusCode >= 400) {
        if (statusCode == 401) {
            return HubClientError::UNAUTHORIZED;
        } else if (statusCode == 403) {
            return HubClientError::FORBIDDEN;
        } else if (statusCode == 404){
            return HubClientError::NOT_FOUND;
        } else if (statusCode == 409) {
            return HubClientError::CONFLICT;
        } else {
            return HubClientError::BAD_REQUEST;
        }
    } else if (statusCode == HTTP_STATUS_CODE_OK) {
        return HubClientError::NONE;
    } else if (statusCode == HTTP_STATUS_CODE_NOT_MODIFIED) {
        return HubClientError::NONE;
    }

    return HubClientError::UNKNOWN;
}

/// Remove all the requests sent by the provided sender, from the
/// "pending requests" and "downloaded requests" lists
/// @param sender ...
void HubClient::cancelAllRequestsForSender(HubRequestSender *sender) {
    const std::lock_guard<std::mutex> locker(_lock);

    HubRequestSender_SharedPtr sharedPtr;

    std::list<HubClientRequest_SharedPtr>::iterator it = _pendingRequests.begin();
    for (; it != _pendingRequests.end(); ) {
        const HubClientRequest_SharedPtr& req = (*it);

        sharedPtr = req->sender().lock();
        if (sharedPtr != nullptr && sharedPtr.get() == sender) {
            req->cancel();
            it = _pendingRequests.erase(it);
        } else {
            it++;
        }
    }

    // too late to cancel if not within _pendingRequests
}

HubClientRequest_SharedPtr HubClient::newRequest(HubRequestSender_WeakPtr sender) {
    HubClientRequest_SharedPtr request = HubClientRequest::make(sender);
    {
        const std::lock_guard<std::mutex> locker(_lock);
        _pendingRequests.push_back(request);
    }
    return request;
}

void HubClient::cancel(HubClientRequest_SharedPtr request) {
    if (request->cancel() == false) {
        return;
    }
    {
        const std::lock_guard<std::mutex> locker(_lock);
        _pendingRequests.remove(request);
    }
}

bool HubClient::done(HubClientRequest_SharedPtr request) {
    // returns true only if the request be set as "done".
    // can't work if it's already been cancelled for example.
    if (request->done() == false) { return false; }
    {
        const std::lock_guard<std::mutex> locker(_lock);
        _pendingRequests.remove(request);
    }
    return true;
}

/// decodes World JSON object data into a World struct
bool HubClient::_decodeWorld(const cJSON * const json, hub::World& out) {
    if (json == nullptr) { return false; }
    vx::json::readStringField(json, "id", out.ID, true);
    vx::json::readStringField(json, "title", out.title, true);
    vx::json::readStringField(json, "description", out.description, true);

    const std::string authorIdField = vx::json::hasField(json, "authorId") ? "authorId" : "author-id";
    vx::json::readStringField(json, authorIdField, out.author, true);

    const std::string authorNameField = vx::json::hasField(json, "authorName") ? "authorName" : "author-name";
    vx::json::readStringField(json, authorNameField, out.authorName, true);

    vx::json::readIntField(json, "likes", out.likes, true);
    vx::json::readIntField(json, "views", out.views, true);
    vx::json::readIntField(json, "maxPlayers", out.maxPlayers, true);
    vx::json::readBoolField(json, "liked", out.liked, true);

    const std::string isAuthorField = vx::json::hasField(json, "isAuthor") ? "isAuthor" : "is-author";
    vx::json::readBoolField(json, isAuthorField, out.isAuthor, true);

    vx::json::readBoolField(json, "featured", out.featured, true);
    vx::json::readStringField(json, "script", out.script, true);
    vx::json::readStringField(json, "mapBase64", out.mapBase64, true);
    return true;
}

cJSON *HubClient::_encodeFriendRequest(const hub::FriendRequest &in) const {
    cJSON *json = cJSON_CreateObject();
    vx::json::writeStringField(json, "senderID", in.senderUserID);
    vx::json::writeStringField(json, "recipientID", in.recipientUserID);
    return json;
}

cJSON *HubClient::_encodeFriendRequestReply(const hub::FriendRequestReply &in) const {
    cJSON *json = cJSON_CreateObject();
    vx::json::writeStringField(json, "senderID", in.senderID);
    vx::json::writeBoolField(json, "accept", in.accept);
    return json;
}

bool HubClient::_parseUser(const std::string& jsonStr, User& userRef) {
    if (jsonStr.empty()) {
        return false;
    }

    cJSON *json = cJSON_Parse(jsonStr.c_str());
    if (json == nullptr) {
        return false;
    }

    const bool ok = (vx::json::readStringField(json, "id", userRef.userID, false) &&
                     vx::json::readStringField(json, "username", userRef.username, false));

    cJSON_Delete(json);
    return ok;
}

bool HubClient::_parseUserArray(const std::string& jsonStr, std::vector<User>& userArrRef) {
    if (jsonStr.empty()) {
        return false;
    }

    cJSON *json = cJSON_Parse(jsonStr.c_str());
    if (json == nullptr) {
        return false;
    }

    if (cJSON_IsArray(json) == false) {
        return false;
    }

    const int arraySize = cJSON_GetArraySize(json);

    bool ok = true;
    for (int i = 0; i < arraySize; ++i) {
        cJSON *item = cJSON_GetArrayItem(json, i);
        if (item == nullptr) {
            ok = false;
            break;
        }
        User usr;
        ok = (vx::json::readStringField(item, "id", usr.userID, false) &&
              vx::json::readStringField(item, "username", usr.username, false));
        if (ok == false) {
            break;
        }
        userArrRef.push_back(std::move(usr));
    }

    cJSON_Delete(json);
    return ok;
}

HubClientRequest_SharedPtr HubClient::pingRegion(HubRequestSender_WeakPtr sender,
                                                 const Region::Type region,
                                                 PingAreaCallback callback) {

    HubClientRequest_SharedPtr request = this->newRequest(sender);
    if (request->send() == false) {
        return nullptr;
    }

    std::unordered_map<std::string, std::string> headers = {
        {HTTP_HEADER_CUBZH_CLIENT_VERSION_V2, vx::device::appVersionCached()},
        {HTTP_HEADER_CUBZH_CLIENT_BUILDNUMBER_V2, vx::device::appBuildNumberCached()},
    };

    HttpClient::shared().GET(Region::URL(region),
                             443, // port
                             "/ping",
                             QueryParams(),
                             true, // secure connection
                             headers,
                             nullptr,
                             [this, request, callback](HttpRequest_SharedPtr req) {

        if (request->isCanceled()) {
            return; // do nothing, the request has been canceled
        }

        const HttpResponse& resp = req->getResponse();

        // parse response data
        HubClientError errType = HubClient::parseError(resp.getSuccess(), resp.getStatusCode());
        const bool success = errType == HubClientError::NONE;

        // if the request has a null callback, the last argument must be false
        if (this->done(request)) {
            if (callback == nullptr) {
                return;
            }
            const std::chrono::milliseconds nowMs = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch());
            const std::chrono::milliseconds sentTime = req->getCreationTime();
            const uint32_t latencyMs = static_cast<uint32_t>((nowMs - sentTime).count());
            callback(success, latencyMs, errType);
        }
    });

    return request;
}

void HubClient::logout() {
    // remove credentials
    const std::lock_guard<std::mutex> lock(_credentialsMutex);
    _credentials.ID.clear();
    _credentials.token.clear();
    _saveCredentialsOnDisk(_credentials);

    vx::http::CookieStore::shared().removeAll();

    tracking::TrackingClient::shared().removeDebugID();
}

HubClientRequest_SharedPtr HubClient::getRunningServer(HubRequestSender_WeakPtr sender,
                                                       const std::string &worldID,
                                                       const std::string &gameServerTag,
                                                       const vx::GameServer::Mode mode,
                                                       const Region::Type region,
                                                       GetServerCallback callback) {

    HubClientRequest_SharedPtr request = this->newRequest(sender);
    if (request->send() == false) { return nullptr; }

    std::unordered_map<std::string, std::string> headers = {
        {HTTP_HEADER_CUBZH_CLIENT_VERSION_V2, vx::device::appVersionCached()},
        {HTTP_HEADER_CUBZH_CLIENT_BUILDNUMBER_V2, vx::device::appBuildNumberCached()},
    };

    const UserCredentials creds = this->getCredentials();
    if (creds.ID.empty() == false && creds.token.empty() == false) {
        headers[HTTP_HEADER_CUBZH_USER_ID] = creds.ID;
        headers[HTTP_HEADER_CUBZH_USER_TOKEN] = creds.token;
    }

    std::string modeStr = "play";
    if (mode == GameServer::Mode::Dev) {
        modeStr = "dev";
    }

    const std::string path = "/worlds/" + worldID + "/server/" + gameServerTag + "/" + vx::str::toLower(Region::Name(region)) + "/" + modeStr;

    HttpClient::shared().GET(this->_serverAddress,
                             this->_serverPort,
                             path,
                             QueryParams(),
                             this->_secureConnection,
                             headers,
                             nullptr,
                             [this, request, callback, path](HttpRequest_SharedPtr req) {

        // do nothing, the request has been canceled
        if (request->isCanceled()) {
            return;
        }

        HttpResponse& resp = req->getResponse();
        std::string allBytes;
        const HubClientError err = HubClient::parseError(resp.getSuccess(), resp.getStatusCode());
        bool success = err == HubClientError::NONE && resp.readAllBytes(allBytes);

        hub::GameServer gs;
        gs.ID = "";
        gs.address = "";
        gs.players = 0;
        gs.maxPlayers = 0;
        gs.devMode = false;
        gs.openSource = true;

        if (success) {
            cJSON *jsonResp = cJSON_Parse(allBytes.c_str());

            cJSON *gameServerJson = cJSON_GetObjectItem(jsonResp, "server");

            vx::json::readStringField(gameServerJson, "id", gs.ID);
            vx::json::readStringField(gameServerJson, "address", gs.address);
            vx::json::readIntField(gameServerJson, "players", gs.players);
            vx::json::readIntField(gameServerJson, "max-players", gs.maxPlayers);
            //vx::json::readBoolField(gameServerJson, "dev-mode", devMode);
            //vx::json::readBoolField(gameServerJson, "open-source", openSource);

            cJSON_Delete(jsonResp);
        }

        // if the request has a null callback, the last argument function must be false
        if (this->done(request)) {
            if (callback == nullptr) { return; }
            callback(success, err, request, gs);
        }
    });

    return request;
}

HubClientRequest_SharedPtr HubClient::setWorldThumbnail(HubRequestSender_WeakPtr sender,
                                                        const std::string& worldID,
                                                        void* thumbnailData,
                                                        size_t thumbnailDataSize,
                                                        SetWorldThumbnailCallback callback) {

    HubClientRequest_SharedPtr request = this->newRequest(sender);
    if (request->send() == false) { return nullptr; }

    const std::string path = "/worlds/" + worldID + "/thumbnail.png";
    const std::string thumbnailBytes(static_cast<char*>(thumbnailData), thumbnailDataSize);
    const UserCredentials creds = this->getCredentials();
    const std::unordered_map<std::string, std::string> headers = {
        {HTTP_HEADER_CUBZH_USER_ID, creds.ID},
        {HTTP_HEADER_CUBZH_USER_TOKEN, creds.token}
    };

    HttpClient::shared().POST(this->_serverAddress,
                              this->_serverPort,
                              path,
                              QueryParams(),
                              this->_secureConnection,
                              headers,
                              nullptr,
                              thumbnailBytes,
                              [this, path, request, callback](HttpRequest_SharedPtr req) {

        if (request->isCanceled()) {
            return; // do nothing, the request has been canceled
        }

        const HttpResponse& resp = req->getResponse();

        bool success = (resp.getStatusCode() == HTTP_STATUS_CODE_OK);

        if (this->done(request)) {
            if (callback == nullptr) { return; }
            callback(success, request);
        }
    });

    return request;
}

HttpRequest_SharedPtr HubClient::getWorldData(HubRequestSender_WeakPtr sender,
                                              const std::string& worldID,
                                              GetWorldDataCallback callback,
                                              ProgressionCallback progressionCallback) {

    const UserCredentials creds = this->getCredentials();
    const std::unordered_map<std::string, std::string> headers = {
        {HTTP_HEADER_CUBZH_USER_ID, creds.ID},
        {HTTP_HEADER_CUBZH_USER_TOKEN, creds.token},
        {HTTP_HEADER_CUBZH_CLIENT_VERSION_V2, vx::device::appVersionCached()},
        {HTTP_HEADER_CUBZH_CLIENT_BUILDNUMBER_V2, vx::device::appBuildNumberCached()},
    };

    const std::string path = "/worlds/" + worldID + "?field=script&field=mapBase64&field=maxPlayers";

    HttpRequest_SharedPtr request = HttpRequest::make("GET",
                                                      this->_serverAddress,
                                                      this->_serverPort,
                                                      path,
                                                      QueryParams(),
                                                      this->_secureConnection);

    vx::HttpRequestOpts opts;
    opts.setStreamResponse(true);
    request->setOpts(opts);

    request->setHeaders(headers);

    request->setCallback([callback, progressionCallback](HttpRequest_SharedPtr req){
        // process response
        HttpResponse& resp = req->getResponse();

        // retrieve HTTP response status
        const HTTPStatus respStatus = resp.getStatus();

        if ((respStatus != HTTPStatus::OK && respStatus != HTTPStatus::NOT_MODIFIED)) {
            callback(false, respStatus, "", "", "", 0, std::unordered_map<std::string, std::string>());
            return;
        }

        if (progressionCallback != nullptr) {
            progressionCallback(resp.getContentLength(), resp.getContentLoaded());
        }

        if (resp.getDownloadComplete() == false) {
            // download not fully done, early return
            // vxlog_debug("PARTIAL %s - %d/%d", req->getPath().c_str(), resp.getContentLoaded(), resp.getContentLength());
            return;
        }

        // retrieve HTTP response body
        std::string respBody;
        const bool didReadBody = resp.readAllBytes(respBody);

        if (didReadBody == false) {
            callback(false, respStatus, "", "", "", 0, std::unordered_map<std::string, std::string>());
            return;
        }

        // parse response body
        vx::hub::World world;
        cJSON *jsonResp = cJSON_Parse(respBody.c_str());
        _decodeWorld(jsonResp, world);
        cJSON_Delete(jsonResp);
        jsonResp = nullptr;

        callback(true, respStatus, world.script, world.mapBase64, "", world.maxPlayers, std::unordered_map<std::string, std::string>());
    });

    request->sendAsync();

    return request;
}

static void addBundleFile(const std::string& path, const std::string& bytes) {
    FILE *f = fs::openStorageFile(std::string("bundle") + "/" + path, "wb", bytes.size());
    if (f == nullptr) {
        // TODO: handle error
        vxlog_error("couldn't store bundle file (1)");
    } else {
        size_t written = fwrite(bytes.data(), sizeof(char), bytes.size(), f);
        if (written != bytes.size()) {
            vxlog_error("couldn't store bundle file (2)");
        }
        fclose(f);
    }
}

static void loadBundleFiles(const std::shared_ptr<std::unordered_map<std::string, std::string>>& bundleFiles,
                            std::shared_ptr<int> toLoad,
                            const std::function<void()>& callback,
                            const std::function<std::string(const std::string&)>& computeUrl) {
    for (std::pair<std::string, std::string> e : (*bundleFiles)) {
        std::string urlStr = computeUrl(e.first);
        std::string finalPath = e.second;
        const vx::URL url = vx::URL::make(urlStr);
        HttpRequest_SharedPtr request = HttpRequest::make("GET",
                                                          url.host(),
                                                          url.port(),
                                                          url.path(),
                                                          QueryParams(),
                                                          true);
        request->setCallback([toLoad, callback, finalPath](HttpRequest_SharedPtr req){

            // process response
            HttpResponse& resp = req->getResponse();

            // retrieve HTTP response status
            const HTTPStatus respStatus = resp.getStatus();
            // retrieve HTTP response body
            std::string respBody;
            const bool didReadBody = resp.readAllBytes(respBody);

            bool success = (respStatus == HTTPStatus::OK || respStatus == HTTPStatus::NOT_MODIFIED) && didReadBody;
            (*toLoad)--;

            if (success) {
                addBundleFile(finalPath, respBody);
            }
            if ((*toLoad) == 0) {
                callback();
            }
        });
        request->sendAsync();
    }
}

static void loadMapBase64(std::shared_ptr<std::string> map_base64_url,
                          std::shared_ptr<std::string> map_base64,
                          std::shared_ptr<int> toLoad,
                          const std::function<void()>& callback) {
    (*toLoad)++;
    HttpRequestCallback mapFileLoaded = [toLoad, callback, map_base64](HttpRequest_SharedPtr req){

        // process response
        HttpResponse& resp = req->getResponse();

        // retrieve HTTP response status
        const HTTPStatus respStatus = resp.getStatus();
        // retrieve HTTP response body
        std::string respBody;
        const bool didReadBody = resp.readAllBytes(respBody);

        const bool success = (respStatus == HTTPStatus::OK || respStatus == HTTPStatus::NOT_MODIFIED) && didReadBody;
        (*toLoad)--;

        if (success) {
            *map_base64 = respBody;
        }

        if ((*toLoad) == 0) {
            callback();
        }
    };

    const vx::URL url = vx::URL::make(*map_base64_url);
    HttpRequest_SharedPtr request = HttpRequest::make("GET",
                                                      url.host(),
                                                      url.port(),
                                                      url.path(),
                                                      QueryParams(),
                                                      true);
    request->setCallback(mapFileLoaded);
    request->sendAsync();
}

static void cubzhJsonParseEnvAndBundle(cJSON *config,
                                       std::shared_ptr<std::unordered_map<std::string, std::string>> env,
                                       std::shared_ptr<std::unordered_map<std::string, std::string>> bundleFiles) {
    if (cJSON_HasObjectItem(config, "env")) {
        const cJSON *envObj = cJSON_GetObjectItem(config, "env");
        if (cJSON_IsObject(envObj)) {
            cJSON *item = nullptr;
            cJSON_ArrayForEach(item, envObj) {
                if (cJSON_IsString(item)) {
                    printf("ENV %s: %s\n", item->string, item->valuestring);
                    (*env)[item->string] = item->valuestring;
                }
            }
        }
    }
    if (cJSON_HasObjectItem(config, "bundle")) {
        const cJSON *bundle = cJSON_GetObjectItem(config, "bundle");
        if (cJSON_IsArray(bundle)) {
            cJSON *item = nullptr;
            cJSON_ArrayForEach(item, bundle) {
                if (cJSON_IsString(item)) {
                    std::string str = std::string(cJSON_GetStringValue(item));
                    printf("BUNDLE FILE: %s\n", str.c_str());
                    (*bundleFiles)[str] = str;
                } else if (cJSON_IsObject(item)) {
                    std::string from = std::string(cJSON_GetStringValue(cJSON_GetObjectItem(item, "from")));
                    std::string to = std::string(cJSON_GetStringValue(cJSON_GetObjectItem(item, "to")));
                    printf("BUNDLE FILE: %s (original path: %s)\n", to.c_str(), from.c_str());
                    (*bundleFiles)[from] = to;
                }
            }
        }
    }
}

static void loadConfig(cJSON *config,
                       std::shared_ptr<std::string> script_url,
                       std::shared_ptr<std::string> map_base64,
                       std::shared_ptr<std::unordered_map<std::string, std::string>> env,
                       std::shared_ptr<std::unordered_map<std::string, std::string>> bundleFiles,
                       std::function<std::string(const std::string&)> rawFileCallback,
                       std::function<void()> callback) {
    std::shared_ptr<std::string> map_base64_url = std::make_shared<std::string>("");

    std::string scriptRelPath = "cubzh.lua";
    if (vx::json::readStringField(config, "script", scriptRelPath, false)) {
        *script_url = rawFileCallback(scriptRelPath);
    }
    std::string mapBase64RelPath = "";
    if (vx::json::readStringField(config, "map", mapBase64RelPath, false)) {
        *map_base64_url = rawFileCallback(mapBase64RelPath);
    }
    cubzhJsonParseEnvAndBundle(config, env, bundleFiles);
    cJSON_Delete(config);

    std::shared_ptr<int> toLoad = std::make_shared<int>((*bundleFiles).size());

    if (*map_base64_url != "") {
        loadMapBase64(map_base64_url, map_base64, toLoad, callback);
    }

    if ((*bundleFiles).empty() == false) {
        loadBundleFiles(bundleFiles, toLoad, callback, rawFileCallback);
        return;
    }

    if (*map_base64_url == "" && (*bundleFiles).empty()) {
        callback();
    }
}

bool repoHasConfig(cJSON *jsonResp) {
    cJSON *array;
    std::string keyName;
    if (cJSON_HasObjectItem(jsonResp, "siblings")) { // huggingface
        keyName = "rfilename";
        array = cJSON_GetObjectItem(jsonResp, "siblings");
    } else { // github
        keyName = "path";
        array = jsonResp;
    }

    std::string name;
    bool ok;
    if (cJSON_IsArray(array)) {
        const int size = cJSON_GetArraySize(array);
        for (int i = 0; i < size; ++i) {
            const cJSON* obj = cJSON_GetArrayItem(array, i);
            if (cJSON_IsObject(obj)) {
                ok = vx::json::readStringField(obj, keyName, name, false);
                if (ok) {
                    if (name == "cubzh.json") {
                        return true;
                    }
                }
            }
        }
    }
    return false;
}

void loadGithubOrHFRepository(HttpRequest_SharedPtr req,
                              vx::HubClient::GetWorldDataCallback callback,
                              std::function<std::string(const std::string &)> rawFileCallback,
                              std::function<std::string()> getDefaultScriptUrl) {
    // process response
    HttpResponse& resp = req->getResponse();

    // retrieve HTTP response status
    const HTTPStatus respStatus = resp.getStatus();
    // retrieve HTTP response body
    std::string respBody;
    const bool didReadBody = resp.readAllBytes(respBody);

    std::shared_ptr<std::unordered_map<std::string, std::string>> env = std::make_shared<std::unordered_map<std::string, std::string>>();

    if ((respStatus != HTTPStatus::OK && respStatus != HTTPStatus::NOT_MODIFIED) || didReadBody == false) {
        // error
        callback(false, respStatus, "", "", "", 0, *env);
        return;
    }

    std::string config_url = "";
    std::shared_ptr<std::string> map_base64 = std::make_shared<std::string>("");
    std::shared_ptr<std::string> script_url = std::make_shared<std::string>("");
    *script_url = getDefaultScriptUrl();
    std::shared_ptr<std::unordered_map<std::string, std::string>> bundleFiles = std::make_shared<std::unordered_map<std::string, std::string>>();

    cJSON *jsonResp = cJSON_Parse(respBody.c_str());
    if (repoHasConfig(jsonResp)) {
        config_url = rawFileCallback("cubzh.json");
    }
    cJSON_Delete(jsonResp);
    jsonResp = nullptr;

    std::function<void()> getScript = [callback, env, map_base64, script_url](){
        const vx::URL url = vx::URL::make(*script_url);
        HttpRequest_SharedPtr request = HttpRequest::make("GET",
                                                          url.host(),
                                                          url.port(),
                                                          url.path(),
                                                          QueryParams(),
                                                          true);
        request->setCallback([callback, env, map_base64](HttpRequest_SharedPtr req){

            // process response
            HttpResponse& resp = req->getResponse();

            // retrieve HTTP response status
            const HTTPStatus respStatus = resp.getStatus();
            // retrieve HTTP response body
            std::string respBody;
            const bool didReadBody = resp.readAllBytes(respBody);

            const bool success = (respStatus == HTTPStatus::OK || respStatus == HTTPStatus::NOT_MODIFIED) && didReadBody;
            if (success == false) {
                // error
                callback(false, respStatus, "", "", "", 1, *env);
                return;
            }

            callback(true, HTTPStatus::OK, respBody, *map_base64, "", 1, *env);
        });
        request->sendAsync();
    };

    if (config_url.empty()) { // no config, load script at default url
        getScript();
        return;
    }

    // load config
    const vx::URL url = vx::URL::make(config_url);
    HttpRequest_SharedPtr request = HttpRequest::make("GET",
                                                      url.host(),
                                                      url.port(),
                                                      url.path(),
                                                      QueryParams(),
                                                      true);
    request->setCallback([callback,
                          rawFileCallback,
                          script_url,
                          env,
                          bundleFiles,
                          getScript,
                          map_base64](HttpRequest_SharedPtr req){

        // process response
        HttpResponse& resp = req->getResponse();

        // retrieve HTTP response status
        const HTTPStatus respStatus = resp.getStatus();
        // retrieve HTTP response body
        std::string respBody;
        const bool didReadBody = resp.readAllBytes(respBody);

        const bool success = (respStatus == HTTPStatus::OK || respStatus == HTTPStatus::NOT_MODIFIED) || didReadBody;
        if (success == false) {
            callback(false, respStatus, "", "", "", 1, *env);
            return;
        }

        cJSON *config = cJSON_Parse(respBody.c_str());
        if (cJSON_IsObject(config) == false) {
            // expecting a JSON with top level object
            cJSON_Delete(config);
            callback(false, HTTPStatus::UNKNOWN, "", "", "", 0, *env);
            return;
        }

        loadConfig(config, script_url, map_base64, env, bundleFiles, rawFileCallback, getScript);
    });
    request->sendAsync();
}

HttpRequest_SharedPtr HubClient::getWorldDataFromGithub(HubRequestSender_WeakPtr sender,
                                                        const std::string& repo,
                                                        const std::string& srcPath,
                                                        const std::string& ref,
                                                        GetWorldDataCallback callback) {

    std::string path = "/repos/" + repo + "/contents";
    if (srcPath.empty() == false) {
        path += "/" + srcPath;
    }
    if (ref.empty() == false) {
        path += "?ref=" + ref;
    }
    std::string repoRef = repo + "/" + (ref.empty() ? "main" : ref);

    HttpRequest_SharedPtr request = HttpRequest::make("GET",
                                                      "api.github.com",
                                                      443,
                                                      path,
                                                      QueryParams(),
                                                      true);

    std::function<std::string(const std::string &)> rawFileCallback = [repoRef](const std::string& name) {
        return "https://raw.githubusercontent.com/" + repoRef + "/" + name;
    };
    request->setHeaders({});
    request->setCallback([path, srcPath, repoRef, callback, rawFileCallback](HttpRequest_SharedPtr req){
        loadGithubOrHFRepository(req, callback, rawFileCallback, [path, srcPath, rawFileCallback](){
            // handle simple lua file
            if (path.size() > 4 && path.substr(path.size() - 4) == ".lua") {
                return rawFileCallback(srcPath);
            }
            return rawFileCallback("cubzh.lua");
        });
    });
    request->sendAsync();

    return request;
}

HttpRequest_SharedPtr HubClient::getWorldDataFromHuggingFaceSpace(HubRequestSender_WeakPtr sender,
                                                                  const std::string& space,
                                                                  const std::string& ref,
                                                                  GetWorldDataCallback callback) {

    // https://huggingface.co/api/spaces/ThomasSimonini/Huggy
    // FILES within "siblings"
    //    "siblings": [
    //        {
    //          "rfilename": ".gitattributes"
    //        },
    //        {
    //          "rfilename": "Build/Huggy.data.unityweb"
    //        },
    // https://huggingface.co/spaces/ThomasSimonini/Huggy/raw/main/index.html

    // space: "cubzh/ai-npcs"
    std::string path = "/api/spaces/" + space;

    // ref not supported yet
    //    if (ref.empty() == false) {
    //        path += "?ref=" + ref;
    //    }

    std::function<std::string(const std::string &)> rawFileCallback = [space](const std::string& name) {
        return "https://huggingface.co/spaces/" + space + "/raw/main/" + name;
    };

    HttpRequest_SharedPtr request = HttpRequest::make("GET",
                                                      "huggingface.co",
                                                      443,
                                                      path,
                                                      QueryParams(),
                                                      true);
    request->setHeaders({});
    request->setCallback([path, rawFileCallback, callback](HttpRequest_SharedPtr req){
        loadGithubOrHFRepository(req, callback, rawFileCallback, [rawFileCallback](){
            return rawFileCallback("cubzh.lua");
        });
    });

    request->sendAsync();

    return request;
}

HttpRequest_SharedPtr HubClient::setWorldScript(HubRequestSender_WeakPtr sender,
                                                const std::string& worldID,
                                                const std::string& script,
                                                SetWorldScriptCallback callback) {

    const UserCredentials creds = this->getCredentials();
    const std::unordered_map<std::string, std::string> headers = {
        {HTTP_HEADER_CUBZH_USER_ID, creds.ID},
        {HTTP_HEADER_CUBZH_USER_TOKEN, creds.token}
    };

    const std::string path = "/worlds/" + worldID + "/script";

    HttpRequest_SharedPtr request = HttpRequest::make("POST",
                                                      this->_serverAddress,
                                                      this->_serverPort,
                                                      path,
                                                      QueryParams(),
                                                      this->_secureConnection);
    request->setHeaders(headers);
    request->setBodyBytes(script);
    request->setCallback([callback](HttpRequest_SharedPtr req){
        const HttpResponse& resp = req->getResponse();
        const bool success = resp.getStatus() == HTTPStatus::OK;
        if (callback != nullptr) {
            callback(success, req);
        }
    });

    request->sendAsync();

    return request;
}

HttpRequest_SharedPtr HubClient::rollbackWorld(HubRequestSender_WeakPtr sender,
                                                const std::string& worldID,
                                                const std::string& hash,
                                                SetWorldScriptCallback callback) {

    const UserCredentials creds = this->getCredentials();
    const std::unordered_map<std::string, std::string> headers = {
        {HTTP_HEADER_CUBZH_USER_ID, creds.ID},
        {HTTP_HEADER_CUBZH_USER_TOKEN, creds.token}
    };

    const std::string path = "/worlds/" + worldID + "/history";

    HttpRequest_SharedPtr request = HttpRequest::make("PATCH",
                                                      this->_serverAddress,
                                                      this->_serverPort,
                                                      path,
                                                      QueryParams(),
                                                      this->_secureConnection);

    cJSON *bodyJson = cJSON_CreateObject();
    json::writeStringField(bodyJson, "hash", hash);
    char *s = cJSON_PrintUnformatted(bodyJson);
    const std::string bodyStr = std::string(s);
    free(s);
    s = nullptr;
    cJSON_Delete(bodyJson);

    request->setHeaders(headers);
    request->setBodyBytes(bodyStr);
    request->setCallback([callback](HttpRequest_SharedPtr req){
        const HttpResponse& resp = req->getResponse();
        const bool success = resp.getStatus() == HTTPStatus::OK;
        if (callback != nullptr) {
            callback(success, req);
        }
    });

    request->sendAsync();

    return request;
}

HttpRequest_SharedPtr HubClient::updateWorldViews(const std::string &worldID,
                                                  const int& addViews,
                                                  UpdateWorldViewsCallback callback) {

    const UserCredentials creds = this->getCredentials();
    const std::unordered_map<std::string, std::string> headers = {
        {HTTP_HEADER_CUBZH_CLIENT_VERSION_V2, vx::device::appVersionCached()},
        {HTTP_HEADER_CUBZH_CLIENT_BUILDNUMBER_V2, vx::device::appBuildNumberCached()},
        {HTTP_HEADER_CUBZH_USER_ID, creds.ID},
        {HTTP_HEADER_CUBZH_USER_TOKEN, creds.token}
    };

    const std::string path = "/privileged/worlds/" + worldID + "/views";

    HttpRequest_SharedPtr request = HttpRequest::make(VX_HTTPMETHOD_PATCH,
                                                      this->_serverAddress,
                                                      this->_serverPort,
                                                      path,
                                                      QueryParams(),
                                                      this->_secureConnection);
    cJSON *bodyJson = cJSON_CreateObject();
    json::writeIntField(bodyJson, "value", addViews);
    char *s = cJSON_PrintUnformatted(bodyJson);
    const std::string bodyStr = std::string(s);
    free(s);
    s = nullptr;
    cJSON_Delete(bodyJson);

    request->setHeaders(headers);
    request->setBodyBytes(bodyStr);
    request->setCallback([callback](HttpRequest_SharedPtr req){
        const HttpResponse& resp = req->getResponse();
        const bool success = resp.getStatus() == HTTPStatus::OK;
        if (callback != nullptr) {
            callback(success, req);
        }
    });

    request->sendAsync();

    return request;
}

HttpRequest_SharedPtr HubClient::getPasskeyChallenge(ChallengeCallback callback) {
    // ensures callback is defined
    if (callback == nullptr) {
        vxlog_error("[HubClient::getChallenge] callback is null");
        return nullptr;
    }

    const UserCredentials creds = this->getCredentials();
    const std::unordered_map<std::string, std::string> headers = {
        {HTTP_HEADER_CUBZH_CLIENT_VERSION_V2, vx::device::appVersionCached()},
        {HTTP_HEADER_CUBZH_CLIENT_BUILDNUMBER_V2, vx::device::appBuildNumberCached()},
        {HTTP_HEADER_CUBZH_USER_ID, creds.ID},
        {HTTP_HEADER_CUBZH_USER_TOKEN, creds.token}
    };

    const std::string path = "/users/self/passkey-challenge";

    HttpRequest_SharedPtr request = HttpRequest::make(VX_HTTPMETHOD_GET,
                                                      this->_serverAddress,
                                                      this->_serverPort,
                                                      path,
                                                      QueryParams(),
                                                      this->_secureConnection);
    request->setHeaders(headers);
    request->setCallback([callback](HttpRequest_SharedPtr req){
        HttpResponse& resp = req->getResponse();
        bool success = resp.getStatus() == HTTPStatus::OK;
        std::string userID;
        std::string username;
        std::string challenge;
        if (success) {
            // read response body
            std::string respBody;
            success = resp.readAllBytes(respBody);
            if (success) {
                // parse response body
                cJSON *json = cJSON_Parse(respBody.c_str());
                if (json != nullptr) {
                    success = json::readStringField(json, "challenge", challenge);
                    cJSON_Delete(json);
                }
            }
        }
        callback(success, challenge, req);
    });

    request->sendAsync();

    return request;
}

HttpRequest_SharedPtr HubClient::authorizePasskey(vx::auth::PassKey::CreateResponse response,
                                                  SimpleCallback callback) {
    // ensures callback is defined
    if (callback == nullptr) {
        vxlog_error("[HubClient::authorizePasskey] callback is null");
        return nullptr;
    }

    const UserCredentials creds = this->getCredentials();
    const std::unordered_map<std::string, std::string> headers = {
        {HTTP_HEADER_CUBZH_CLIENT_VERSION_V2, vx::device::appVersionCached()},
        {HTTP_HEADER_CUBZH_CLIENT_BUILDNUMBER_V2, vx::device::appBuildNumberCached()},
        {HTTP_HEADER_CUBZH_USER_ID, creds.ID},
        {HTTP_HEADER_CUBZH_USER_TOKEN, creds.token}
    };

    const std::string path = "/users/self/authorize-passkey";

    HttpRequest_SharedPtr request = HttpRequest::make(VX_HTTPMETHOD_POST,
                                                      this->_serverAddress,
                                                      this->_serverPort,
                                                      path,
                                                      QueryParams(),
                                                      this->_secureConnection);

    const std::string &bodyStr = response.getResponseJSON();
    if (bodyStr.empty()) {
        // Error: failed to construct the JSON body to send in the request
        return nullptr;
    }

    request->setHeaders(headers);
    request->setBodyBytes(bodyStr);
    request->setCallback([callback](HttpRequest_SharedPtr req){
        const HttpResponse& resp = req->getResponse();
        const bool success = resp.getStatus() == HTTPStatus::OK;
        callback(success, req);
    });

    request->sendAsync();

    return request;
}

HttpRequest_SharedPtr HubClient::getItemModel(HubRequestSender_WeakPtr sender,
                                              const std::string &itemRepo,
                                              const std::string &itemName,
                                              const bool forceRevalidateCache,
                                              getItemModelCallback callback) {

    HubClientRequest_SharedPtr request = this->newRequest(sender);
    if (request->send() == false) { return nullptr; }

    // construct URL path
    std::string path = "";
    if (itemRepo != "" && itemName != "") {
        path = "/items/" + itemRepo + "/" + itemName + "/model";
    } else {
        // invalid request
        vxlog_error("getItemModel: item repo and name are empty");
        if (callback == nullptr) { return nullptr; }
        callback(false, nullptr, itemRepo, itemName, "", false);
        return nullptr;
    }

    // HTTP Headers
    std::unordered_map<std::string, std::string> headers = {
        {HTTP_HEADER_CUBZH_CLIENT_VERSION_V2, vx::device::appVersionCached()},
        {HTTP_HEADER_CUBZH_CLIENT_BUILDNUMBER_V2, vx::device::appBuildNumberCached()},
    };

    // HTTP request options
    HttpRequestOpts opts;
    opts.setForceCacheRevalidate(forceRevalidateCache);

    HttpRequest_SharedPtr req = HttpClient::shared().GET(this->_serverAddress,
                                                         this->_serverPort,
                                                         path,
                                                         QueryParams(),
                                                         this->_secureConnection,
                                                         headers,
                                                         &opts,
                                                         [request, callback, itemRepo, itemName]
                                                         (HttpRequest_SharedPtr req) {
        // process response
        HttpResponse& resp = req->getResponse();

        // retrieve HTTP response status
        const HTTPStatus respStatus = resp.getStatus();
        // retrieve HTTP response body

        std::string respBody;
        const bool didReadBody = resp.readAllBytes(respBody);

        const bool success = (respStatus == HTTPStatus::OK || respStatus == HTTPStatus::NOT_MODIFIED) && didReadBody;

        // if the request has a null callback, the last argument of the done() function must be false
        if (HubClient::getInstance().done(request)) {
            if (callback == nullptr) { return; }
            if (success == false) {
                // do not update the pcube
                callback(false, req, itemRepo, itemName, "", false);
            } else {
                if (resp.getStatusCode() == HTTP_STATUS_CODE_OK) {
                    // the pcubes needs to be modified
                    callback(true, req, itemRepo, itemName, respBody, false /* etag invalid */);

                } else if (resp.getStatusCode() == HTTP_STATUS_CODE_NOT_MODIFIED) {
                    // the pcubes did not change since the last request
                    callback(true, req, itemRepo, itemName, "", true /* etag valid */);

                } else {
                    vx_assert(false); // not supposed to happen
                }
            }
        }
    });

    return req;
}

HubClientRequest_SharedPtr HubClient::setItem3ZH(HubRequestSender_WeakPtr sender,
                                                 const std::string &itemRepo,
                                                 const std::string &itemName,
                                                 const std::string &category,
                                                 const std::string &bytes,
                                                 SetItem3ZHCallback callback) {

    HubClientRequest_SharedPtr request = this->newRequest(sender);
    if (request->send() == false) { return nullptr; }

    std::string path = "";
    if (itemRepo != "" && itemName != "") {
        path = "/items/" + itemRepo + "/" + itemName + "/model";
    } else {
        // invalid request
        vxlog_error("setItemPcubes: use non empty itemID");
        if (callback == nullptr) { return request; }
        callback(false);
        return request;
    }

    const UserCredentials creds = this->getCredentials();
    // HTTP Headers
    std::unordered_map<std::string, std::string> headers = {
        {HTTP_HEADER_CUBZH_CLIENT_VERSION_V2, vx::device::appVersionCached()},
        {HTTP_HEADER_CUBZH_CLIENT_BUILDNUMBER_V2, vx::device::appBuildNumberCached()},
        {HTTP_HEADER_CUBZH_USER_ID, creds.ID},
        {HTTP_HEADER_CUBZH_USER_TOKEN, creds.token},
        {HTTP_HEADER_CUBZH_ITEM_CATEGORY, category}
    };

    HttpClient::shared().POST(this->_serverAddress,
                              this->_serverPort,
                              path,
                              QueryParams(),
                              this->_secureConnection,
                              headers,
                              nullptr,
                              bytes,
                              [this, request, callback, itemRepo, itemName]
                              (HttpRequest_SharedPtr req) {

        const HttpResponse& resp = req->getResponse();

        HubClientError err = HubClient::parseError(resp.getSuccess(), resp.getStatusCode());
        bool success = err == HubClientError::NONE;

        // TODO: make .etag file

        // if the request has a null callback, the last argument of the done()
        // function must be false
        if (this->done(request)) {

#if __VX_USE_HTTP_CACHING == 1
            const bool ok = HttpClient::shared().removeCachedResponseForRequest(req);
            if (ok == false) {
                vxlog_debug("failed to remove HTTP cache");
            }
#endif

            if (callback == nullptr) {
                return;
            }
            callback(success);

        } else {
            vxlog_debug("done failed");
        }
    });

    return request;
}

#if defined(P3S_CLIENT_HEADLESS)
// Empty implementation for headless builds
HubClientRequest_SharedPtr HubClient::postPushToken(HubRequestSender_WeakPtr sender,
                                                    const hub::PushToken::Type& type,
                                                    const std::string& token,
                                                    PostPushTokenCallback callback) {
    return nullptr;
}
#else
HubClientRequest_SharedPtr HubClient::postPushToken(HubRequestSender_WeakPtr sender,
                                                    const hub::PushToken::Type& type,
                                                    const std::string& token,
                                                    PostPushTokenCallback callback) {

    HubClientRequest_SharedPtr request = this->newRequest(sender);
    if (request->send() == false) {
        return nullptr;
    }

    const UserCredentials creds = this->getCredentials();

    // define HTTP headers
    std::unordered_map<std::string, std::string> headers = {
        {HTTP_HEADER_CUBZH_CLIENT_VERSION_V2, vx::device::appVersionCached()},
        {HTTP_HEADER_CUBZH_CLIENT_BUILDNUMBER_V2, vx::device::appBuildNumberCached()},
        {HTTP_HEADER_CUBZH_USER_ID, creds.ID},
        {HTTP_HEADER_CUBZH_USER_TOKEN, creds.token},
    };

    // construct JSON body of HTTP request
#if defined(DEBUG)
    const std::string variant = "dev";
#else
    const std::string variant = "prod";
#endif
    const hub::PushToken pushToken = hub::PushToken(hub::PushToken::Value(type), token, variant);
    const std::string pushTokenJSONStr = pushToken.encodeToJSONString();
    const std::string path = "/users/pushtoken";
    HttpClient::shared().POST(this->_serverAddress,
                              this->_serverPort,
                              path,
                              QueryParams(),
                              this->_secureConnection,
                              headers,
                              nullptr,
                              pushTokenJSONStr,
                              [this, path, request, callback](HttpRequest_SharedPtr req) {

        if (request->isCanceled()) {
            return; // do nothing, the request has been canceled
        }

        const HttpResponse& resp = req->getResponse();

        // check HTTP status code
        const bool ok = resp.getStatusCode() == HTTP_STATUS_CODE_OK;

        // if the request has a null callback, the last argument of the done()
        // function must be false
        if (this->done(request) && callback != nullptr) {
            callback(ok, request);
        }
    });

    return request;
}
#endif

HttpRequest_SharedPtr HubClient::verifyPurchase(const vx::IAP::Purchase_SharedPtr &purchase,
                                                VerifyPurchaseCallback callback) {
    // ensures callback is defined
    if (callback == nullptr) {
        vxlog_error("[HubClient::verifyPurchase] callback is null");
        return nullptr;
    }

    const UserCredentials creds = this->getCredentials();
    const std::unordered_map<std::string, std::string> headers = {
        {HTTP_HEADER_CUBZH_CLIENT_VERSION_V2, vx::device::appVersionCached()},
        {HTTP_HEADER_CUBZH_CLIENT_BUILDNUMBER_V2, vx::device::appBuildNumberCached()},
        {HTTP_HEADER_CUBZH_USER_ID, creds.ID},
        {HTTP_HEADER_CUBZH_USER_TOKEN, creds.token},
#if defined(__VX_DISTRIBUTION_APPSTORE)
        // allows to purchases coins for free in sandbox mode
        // (used by Apple for reviews)
        {HTTP_HEADER_IAP_SANDBOX_KEY, "a262649552c3d4d717af2d522fd4e2c2"}
#endif
    };

    std::string path = "/purchases/verify";

    HttpRequest_SharedPtr request = HttpRequest::make(VX_HTTPMETHOD_POST,
                                                      this->_serverAddress,
                                                      this->_serverPort,
                                                      path,
                                                      QueryParams(),
                                                      this->_secureConnection);

    // Create the main JSON object
    cJSON *bodyJson = cJSON_CreateObject();
    // TODO: add platform
    if (purchase->receiptData != "") { vx::json::writeStringField(bodyJson, "receipt-data", purchase->receiptData); }
    if (purchase->transactionID != "") { vx::json::writeStringField(bodyJson, "transaction-id", purchase->transactionID); }
    if (purchase->productID != "") { vx::json::writeStringField(bodyJson, "product-id", purchase->productID); }

#if defined(__VX_PLATFORM_IOS) || defined(__VX_PLATFORM_MACOS)
    vx::json::writeStringField(bodyJson, "purchase-type", "apple-iap");
#endif

    char *s = cJSON_PrintUnformatted(bodyJson);
    const std::string bodyStr = std::string(s);
    free(s);
    s = nullptr;
    cJSON_Delete(bodyJson);

    request->setHeaders(headers);
    request->setBodyBytes(bodyStr);

    request->setCallback([callback](HttpRequest_SharedPtr req){
        HttpResponse& resp = req->getResponse();
        bool success = false;
        bool sandbox = false;
        std::string error = "";
        if (resp.getStatus() != HTTPStatus::NETWORK) {
            std::string respBody;
            success = resp.readAllBytes(respBody);
            if (success) {
                vxlog_debug("BODY: %s", respBody.c_str());
                cJSON *json = cJSON_Parse(respBody.c_str());
                if (json != nullptr) {
                    json::readBoolField(json, "success", success, true);
                    json::readBoolField(json, "sandbox", sandbox, true);
                    json::readStringField(json, "error", error, true);
                    cJSON_Delete(json);
                }
            }
        }
        callback(success, sandbox, error, req);
    });

    request->sendAsync();
    return request;
}

std::string HubClient::getUserID() {
    return this->getCredentials().ID;
}

HttpRequest_SharedPtr vx::HubClient::privateAPIGetWorldScriptAndMaxPlayers(HubRequestSender_WeakPtr sender,
                                                                           const std::string &worldID,
                                                                           GetWorldScriptAndMaxPlayersCallback callback) {

    // TODO: this should be moved out of the private API
    //       - remove the /private/ path prefix
    //       - update the hub server's implementation
    const std::string path = PRIVATE_API_PATH_PREFIX + "/worlds/" + worldID + "?f=script&f=maxPlayers";

    HttpRequest_SharedPtr request = HttpRequest::make("GET",
                                                      this->_serverAddress,
                                                      this->_serverPort,
                                                      path,
                                                      QueryParams(),
                                                      this->_secureConnection);

    request->setCallback([callback](HttpRequest_SharedPtr req){

        // process response
        HttpResponse& resp = req->getResponse();

        // retrieve HTTP response status
        const HTTPStatus respStatus = resp.getStatus();
        // retrieve HTTP response body
        std::string respBody;
        const bool didReadBody = resp.readAllBytes(respBody);

        const bool success = (respStatus == HTTPStatus::OK || respStatus == HTTPStatus::NOT_MODIFIED) && didReadBody;
        if (success == false) {
            // error
            callback(false, respStatus, "", 0);
            return;
        }

        cJSON *result = cJSON_Parse(respBody.c_str());
        vx::hub::World world;
        _decodeWorld(cJSON_GetObjectItem(result, "world"), world);
        cJSON_Delete(result);
        
        callback(true, respStatus, world.script, world.maxPlayers);
        return;
    });

    request->sendAsync();

    return request;
}
