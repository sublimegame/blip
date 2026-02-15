//
//  VXHubClient.hpp
//  Cubzh
//
//  Created by Adrien Duermael on 3/16/20.
//

#pragma once

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wpadded"

// C++
#include <functional>
#include <memory>
#include <mutex>

// xptools
#include "xptools.h"
#include "passkey.hpp"

#include "VXAccount.hpp"
#include "VXTickDispatcher.hpp"
#include "VXNotifications.hpp"
#include "VXHubRequestSender.hpp"
#include "iap.hpp"

// VX Networking
#include "VXHub.hpp"

// Gameserver
#include "VXGameServer.hpp"

#include "HttpRequest.hpp"

#define HTTP_HEADER_CUBZH_CLIENT_VERSION_V2 "Czh-Ver"
#define HTTP_HEADER_CUBZH_CLIENT_BUILDNUMBER_V2 "Czh-Rev"
#define HTTP_HEADER_CUBZH_USER_ID "Czh-Usr-Id"
#define HTTP_HEADER_CUBZH_USER_TOKEN "Czh-Tk"
#define HTTP_HEADER_CUBZH_ITEM_CATEGORY "Czh-Item-Category" // introduced in 0.0.66
#define HTTP_HEADER_IAP_SANDBOX_KEY "Blip-Iap-Sandbox-Key"

#ifdef ONLINE_GAMESERVER
#define HTTP_HEADER_CUBZH_GAME_SERVER_TOKEN "Czh-Server-Token"
#define HTTP_HEADER_CUBZH_GAME_SERVER_TOKEN_VALUE "pgCLIaAyfhnq4QIwkoVePiB6VTtYRx4BNBgMY5eMxFc6qFXcqS"
#endif

namespace vx {

class HubClientRequest;
typedef std::shared_ptr<HubClientRequest> HubClientRequest_SharedPtr;
typedef std::weak_ptr<HubClientRequest> HubClientRequest_WeakPtr;

// TODO: gdevillele: I think we could remove this.
// It does the same thing as HttpResponse::Status
enum class HubClientError {
    NONE,
    INTERNAL,
    NETWORK,
    BAD_REQUEST,
    NOT_FOUND,
    UNAUTHORIZED,
    FORBIDDEN,
    UNKNOWN,
    CONFLICT,
};

/// Server infrastructure regions
namespace Region {
enum Type {
    Europe = 0,
    USA,
    Singapore
};
const Type All[] = {
    Type::Europe,
    Type::USA,
    Type::Singapore
};
std::string URL(Type region);
std::string Name(Type region);
}

///
class HubClientRequest {

public:

    ///
    static HubClientRequest_SharedPtr make(HubRequestSender_WeakPtr sender);

    ///
    enum class State {
        idle,
        sent,
        done,
        canceled
    };

    ///
    State getState();

    ///
    HubRequestSender_WeakPtr sender();

    ///
    bool success();

    ///
    std::string err();

    /// Best is done to cancel request depending on its state.
    /// callback will never be called after cancel()
    /// Returns true if the call effectively sets state to State::cancel
    bool cancel();

    /// Tells whether the request is in the State::canceled state
    bool isCanceled();

    // Triggers callback
    // Returns true if the call effectively sets state to State::done
    bool done();

    ///
    bool isDone();

    // Returns false if request can't be sent (if canceled for example)
    bool send();

    //
    void setHttpRequest(const HttpRequest_SharedPtr& req);

private:

    ///
    HubClientRequest(HubRequestSender_WeakPtr sender);

    ///
    State _state;

    /// Underlying http request
    HttpRequest_WeakPtr _underlyingHttpRequestWeak;

    /// Sets new state, returns false if transition can't be considered
    bool setState(State s);

    ///
    HubRequestSender_WeakPtr _sender;

    ///
    bool _success;

    /// Error message
    std::string _err;

    /// mutex for thread to make operations thread safe
    std::mutex _lock;

    /// A weak pointer
    std::weak_ptr<HubClientRequest> _weakThis;
};

///
/// HubClient is available as a singleton to request to the API.
///
class HubClient final : public TickListener {

public:

    // UserCredentials
    typedef struct {
        std::string ID;
        std::string token;
    } UserCredentials;

    // User
    typedef struct {
        std::string userID;
        std::string username;
        // ...
    } User;

    inline const std::string& getHubServerAddr() const { return _serverAddress; }
    inline const uint16_t& getHubServerPort() const { return _serverPort; }
    inline const std::string getHubServerScheme() const { return _secureConnection ? "https" : "http"; }

    ///
    static HubClientError parseError(const bool& success, const uint16_t& statusCode);

    /// SimpleCallback provides only a "success"' boolean value.
    /// It can be used for simple success/failure usecases.
    typedef std::function<void(const bool &success,
                               HttpRequest_SharedPtr request)> SimpleCallback;

    ///
    typedef std::function<void(const bool& success,
                               const std::string& challenge,
                               HttpRequest_SharedPtr request)> ChallengeCallback;

    ///
    typedef std::function<void(const bool& success,
                               const HubClientError& errType,
                               HubClientRequest_SharedPtr request,
                               hub::GameServer server)> GetServerCallback;

    ///
    typedef std::function<void(const bool& success,
                               const uint32_t& latency,
                               const HubClientError& errType)> PingAreaCallback;

    typedef std::function<void(const int contentLength, const int contentLoaded)> ProgressionCallback;

    ///
    typedef std::function<void(const bool &success,
                               const HTTPStatus &status,
                               const std::string &script,
                               const std::string &mapBase64,
                               const std::string &map3zh,
                               const int maxPlayers,
                               std::unordered_map<std::string, std::string> env)> GetWorldDataCallback;

    ///
    typedef std::function<void(const bool &success,
                               const HTTPStatus &status,
                               const std::string &script,
                               const int maxPlayers)> GetWorldScriptAndMaxPlayersCallback;

    ///
    typedef std::function<void(const bool &success,
                               HttpRequest_SharedPtr request)> SetWorldScriptCallback;

    ///
    typedef std::function<void(const bool &success,
                               HttpRequest_SharedPtr request)> UpdateWorldViewsCallback;

    ///
    typedef std::function<void(const bool &success,
                               HttpRequest_SharedPtr req,
                               const std::string& itemRepo,
                               const std::string& itemName,
                               const std::string& content,
                               const bool& etagValid)> getItemModelCallback;

    ///
    typedef std::function<void(const bool &success)> SetItem3ZHCallback;

    ///
    typedef std::function<void(const bool& success, HubClientRequest_SharedPtr request)> SetWorldThumbnailCallback;

    ///
    static HubClient &getInstance() {
        static HubClient instance;
        return instance;
    }

    ///
    inline HubRequestSender_WeakPtr getUniversalSender() {
        return HubRequestSender_WeakPtr(this->_universalSender);
    }

    /// Description
    /// @param sender sender description
    void cancelAllRequestsForSender(HubRequestSender *sender);

    /// Ping a single ping-server.
    /// The request is sent in the "request" OperationQueue.
    /// The callback function is called in the "main" OperationQueue.
    /// @param sender ...
    /// @param region ...
    /// @param callback ...
    HubClientRequest_SharedPtr pingRegion(HubRequestSender_WeakPtr sender,
                                          const Region::Type region,
                                          PingAreaCallback callback);

    /// Description
    /// Removes the credentials
    void logout();

    ///
    /// The request is sent in the "request" OperationQueue.
    /// The callback function is called in the "main" OperationQueue.
    HubClientRequest_SharedPtr getRunningServer(HubRequestSender_WeakPtr sender,
                                                const std::string& worldID,
                                                const std::string &gameServerTag,
                                                const vx::GameServer::Mode mode,
                                                const Region::Type region,
                                                GetServerCallback callback);

    /// Description
    /// TODO: force callback in main queue
    /// @param sender sender description
    /// @param worldID worldID description
    /// @param callback callback description
    HubClientRequest_SharedPtr setWorldThumbnail(HubRequestSender_WeakPtr sender,
                                                 const std::string& worldID,
                                                 void* thumbnailData,
                                                 size_t thumbnailDataSize,
                                                 SetWorldThumbnailCallback callback);

    ///
    HttpRequest_SharedPtr getWorldData(HubRequestSender_WeakPtr sender,
                                       const std::string& worldID,
                                       GetWorldDataCallback callback,
                                       ProgressionCallback progressionCallback);

    ///
    HttpRequest_SharedPtr getWorldDataFromGithub(HubRequestSender_WeakPtr sender,
                                                 const std::string& repo,
                                                 const std::string& path,
                                                 const std::string& ref,
                                                 GetWorldDataCallback callback);

    ///
    HttpRequest_SharedPtr getWorldDataFromHuggingFaceSpace(HubRequestSender_WeakPtr sender,
                                                           const std::string& space,
                                                           const std::string& ref,
                                                           GetWorldDataCallback callback);

    /// Description
    /// TODO: force callback in main queue
    /// @param sender sender description
    /// @param worldID worldID description
    /// @param script script description
    /// @param callback callback description
    HttpRequest_SharedPtr setWorldScript(HubRequestSender_WeakPtr sender,
                                         const std::string &worldID,
                                         const std::string &script,
                                         SetWorldScriptCallback callback);

    HttpRequest_SharedPtr rollbackWorld(HubRequestSender_WeakPtr sender,
                                        const std::string &worldID,
                                        const std::string &hash,
                                        SetWorldScriptCallback callback);

    /// Add views to the world on the API
    HttpRequest_SharedPtr updateWorldViews(const std::string &worldID,
                                           const int& addViews,
                                           UpdateWorldViewsCallback callback);

    // --------------------------------------------------
    // SIGNUP / LOGIN
    // --------------------------------------------------

    /// Get new passkey challenge for user
    HttpRequest_SharedPtr getPasskeyChallenge(ChallengeCallback callback);

    /// Upload passkey public key to user account
    HttpRequest_SharedPtr authorizePasskey(vx::auth::PassKey::CreateResponse response,
                                           SimpleCallback callback);

    // --------------------------------------------------
    // ITEMS
    // --------------------------------------------------

    /// TODO: force callback in main queue
    HttpRequest_SharedPtr getItemModel(HubRequestSender_WeakPtr sender,
                                       const std::string& itemRepo,
                                       const std::string& itemName,
                                       const bool forceRevalidateCache,
                                       getItemModelCallback callback);
    
    /// TODO: force callback in main queue
    HubClientRequest_SharedPtr setItem3ZH(HubRequestSender_WeakPtr sender,
                                          const std::string &itemRepo,
                                          const std::string &itemName,
                                          const std::string &category,
                                          const std::string &bytes,
                                          SetItem3ZHCallback callback);

    // --------------------------------------------------
    // USERS
    // --------------------------------------------------

    ///
    typedef std::function<void(const bool& success,
                               HubClientRequest_SharedPtr request)> PostPushTokenCallback;

    /// Register a push token on the Hub
    HubClientRequest_SharedPtr postPushToken(HubRequestSender_WeakPtr sender,
                                             const hub::PushToken::Type& type,
                                             const std::string& token,
                                             PostPushTokenCallback callback);

    typedef std::function<void(const bool& success, const bool& sandbox, std::string err, HttpRequest_SharedPtr request)> VerifyPurchaseCallback;

    HttpRequest_SharedPtr verifyPurchase(const vx::IAP::Purchase_SharedPtr &purchase, VerifyPurchaseCallback callback);

    HubClient(HubClient const&) = delete;
    void operator=(HubClient const&)  = delete;

    /// returns a copy of the user ID, currently used by the Hub Client
    std::string getUserID();

    /// can be called from any thread
    UserCredentials getCredentials();

    /// can be called from any thread
    void setCredentials(std::string *const id,
                        std::string *const token);

    ///
    HttpRequest_SharedPtr privateAPIGetWorldScriptAndMaxPlayers(HubRequestSender_WeakPtr sender,
                                                                const std::string &worldID,
                                                                GetWorldScriptAndMaxPlayersCallback callback);

private:

    // User utility functions

    /// Parses a User JSON into the userRef reference provided.
    /// Returns true on success, false otherwise.
    static bool _parseUser(const std::string& jsonStr, User& userRef);

    /// Parses a User Array JSON into the userArrRef reference provided.
    /// Returns true on success, false otherwise.
    static bool _parseUserArray(const std::string& jsonStr, std::vector<User>& userArrRef);

    ///
    void tick(const double dt) override;

    /// Constructor
    HubClient();

    /// Saves user credentials in /credentials.json file
    void _saveCredentialsOnDisk(const UserCredentials& creds);

    /// Loads user credentials from /credentials.json file
    UserCredentials _loadCredentialsFromDisk();

    /// Returns a new ApiClientRequest, automatically inserted in _pendingRequests
    HubClientRequest_SharedPtr newRequest(HubRequestSender_WeakPtr sender);

    ///
    void cancel(HubClientRequest_SharedPtr request);

    /// Returns true if the call effectively sets request's state to State::done
    bool done(HubClientRequest_SharedPtr request);

    /// decodes World JSON object data into a World struct
    static bool _decodeWorld(const cJSON * const json, hub::World& out);

    ///
    cJSON *_encodeFriendRequest(const hub::FriendRequest &in) const;

    ///
    cJSON *_encodeFriendRequestReply(const hub::FriendRequestReply &in) const;

    ///
    cJSON *_encodePushToken(const hub::PushToken& in) const;

    //
    // Fields
    //

    ///
    UserCredentials _credentials;
    bool _credentialsWaitingToBeSavedOnDisk;
    std::mutex _credentialsMutex;

    /// Address of remote Hub server
    std::string _serverAddress;

    /// Connection port of remote Hub server
    uint16_t _serverPort;

    /// Indicates if HTTPS should be used instead of HTTP
    bool _secureConnection;

    /// A sender that can be used when there's nothing better
    HubRequestSender_SharedPtr _universalSender;

    /// Contains all pending requests
    std::list<HubClientRequest_SharedPtr> _pendingRequests;

    /// Synchronization lock used for several fields
    std::mutex _lock;
};

}

#pragma clang diagnostic pop
