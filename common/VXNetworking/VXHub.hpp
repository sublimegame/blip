//
//  VXHub.hpp
//  Cubzh
//
//  Created by Gaetan de Villele on 14/09/2020.
//

#pragma once

// C++
#include <string>

#include "json.hpp"

#define PRIVATE_API_PATH_PREFIX std::string("/private")

namespace vx {
namespace hub {

typedef struct {
    std::string ID;
    std::string title;
    std::string description;
    std::string author;
    std::string authorName;
    std::string script;
    std::string mapBase64;
    int likes;
    int views;
    int maxPlayers;
    bool liked; // specific to Hub protocol (for game client, not gameserver client)
    bool isAuthor; // specific to Hub protocol (for game client, not gameserver client)
    bool featured;
    // std::vector<std::string> contributors;
    // std::vector<std::string> contributorNames;
    char pad[7];
} World;

typedef struct {
    std::string ID;
    std::string author; // author ID
    // std::vector<std::string> contributors; // contributor IDs;
    std::string repo;
    std::string name;
    std::string version;
    std::string description;
    std::string repoAndName;
    int likes;
    int views;
    bool isPublic;
    bool featured;
    std::string category; // "head", "body", ...
    // std::string authorName;
    // std::vector<std::string> contributorNames;
    // std::vector<std::string> tags;
    char pad[6];
} Item;

typedef struct {
    std::string ID;
    std::string address;
    int players;
    int maxPlayers;
    bool devMode; // true if in dev mode (meaning the code can be changed)
    bool openSource; // true if we can display the code and offer to fork it
} GameServer;

typedef struct {
    std::string senderUserID;
    std::string recipientUserID;
} FriendRequest;

typedef struct {
    /// user ID of the friend request sender
    std::string senderID;
    /// whether the friend request is accepted or declined
    bool accept;
} FriendRequestReply;

class PushToken {
public:
    enum class Type {
        Apple = 0,
        Firebase,
        // Windows,
    };
    static std::string Value(const Type tokenType);

    // Constructor
    PushToken(const std::string& type,
              const std::string& token,
              const std::string& variant);
    // Destructor
    ~PushToken();

    /// Generates a new JSON string representing the PushToken and returns it as a const reference
    std::string encodeToJSONString() const;

private:
    /// type of push token ("apple", "firebase", ...)
    std::string _type;
    /// token value
    std::string _token;
    /// variant (optional) (e.g. "dev", "prod")
    std::string _variant;

    ///
    cJSON *_encodeToJSON() const;
};

}
}
