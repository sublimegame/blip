//
//  main.cpp
//  Cubzh
//
//  Created by Gaetan de Villele on 08/09/2020.
//

// C++
#include <filesystem>

// xptools
#include "process.hpp"

// Voxowl
#include "URL.hpp"
#include "VXConfig.hpp"
#include "VXGameServer.hpp"

// More consistent error message prefix
#define GAMESERVER_LOG_PREFIX "[GameServer] "

// local macros
#define GAMESERVER_MODE_DEV "dev"
#define GAMESERVER_MODE_PLAY "play"

// hub API address
#define HUB_API_URL "https://api.cu.bzh:443"

// default count of player slots
#define DEFAULT_MAX_PLAYERS 8

/// Server configuration
typedef struct {
    std::string executableParentDir;
    vx::GameServer::Mode mode;
    std::string worldID;
    std::string serverID;
    std::string hubAuthToken;
    std::string hubAddr;
    uint16_t hubPort;
    bool hubSecure;
    int maxPlayers;
} ServerConfig;

/// Parses command line arguments and fill `config`
/// @param argc number of arguments
/// @param argv arguments
/// @param outConfig configuration to fill
/// @return error string (empty string if successful)
std::string parseArgumentsAndLoadConfiguration(const int argc, const char * const argv[], ServerConfig &outConfig);

/// Arguments by index
/// 0 : executable path
/// 1 : mode ("play" or "dev")
/// 2 : world ID
/// 3 : server ID prefix (example: eu-3-)
/// 4 : max players (optional)
int main(int argc, char *argv[]) {
    vxlog_info(GAMESERVER_LOG_PREFIX "starting...");

    // Parse arguments
    ServerConfig config;
    const std::string error = parseArgumentsAndLoadConfiguration(argc, argv, config);
    if (error.empty() == false) {
        vxlog_error(GAMESERVER_LOG_PREFIX "failed to parse arguments and load configuration: %s", error.c_str());
        return EXIT_FAILURE;
    }

    // Set memory usage limit
    vx::Process::setMemoryUsageLimitMB(LUA_DEFAULT_MEMORY_USAGE_LIMIT_FOR_GAMESERVER);

    // not using in-mem storage anymore
    // vx::fs::Helper::shared()->setInMemoryStorage(true);
    vx::fs::Helper::shared()->setStorageRelPathPrefix("gameserver");

    vxlog_info(GAMESERVER_LOG_PREFIX "executable dir : %s", config.executableParentDir.c_str());
    vxlog_info(GAMESERVER_LOG_PREFIX "mode           : %s", config.mode == vx::GameServer::Mode::Dev ? "dev" : "play");
    vxlog_info(GAMESERVER_LOG_PREFIX "World ID       : %s", config.worldID.c_str());
    vxlog_info(GAMESERVER_LOG_PREFIX "server ID      : %s", config.serverID.c_str());
    vxlog_info(GAMESERVER_LOG_PREFIX "Hub address    : %s", config.hubAddr.c_str());
    vxlog_info(GAMESERVER_LOG_PREFIX "Hub port       : %d", config.hubPort);
    vxlog_info(GAMESERVER_LOG_PREFIX "Hub secure     : %s", config.hubSecure ? "true" : "false");
    vxlog_info(GAMESERVER_LOG_PREFIX "Max players    : %d", config.maxPlayers);

    // Use unique_ptr for RAII
    std::unique_ptr<vx::GameServer> gameServer = std::unique_ptr<vx::GameServer>(vx::GameServer::newServer(80,        // listen port
                                                                                                           false,     // secure
                                                                                                           config.mode,
                                                                                                           config.worldID,
                                                                                                           config.serverID,
                                                                                                           config.hubAuthToken,
                                                                                                           config.hubAddr,
                                                                                                           config.hubPort,
                                                                                                           config.hubSecure,
                                                                                                           config.maxPlayers));
    if (gameServer == nullptr) {
        vxlog_error(GAMESERVER_LOG_PREFIX "failed to create game server");
        return EXIT_FAILURE;
    }

    vxlog_info(GAMESERVER_LOG_PREFIX "starting server...");
    gameServer->start();

    vxlog_info(GAMESERVER_LOG_PREFIX "stopping...");
    return EXIT_SUCCESS;
}

std::string parseArgumentsAndLoadConfiguration(const int argc, const char * const argv[], ServerConfig &outConfig) {
    const int MIN_ARGUMENTS_COUNT = 4;

    // validate arguments count
    const int argsCount = argc - 1;
    if (argsCount < MIN_ARGUMENTS_COUNT) {
        return std::string("expected at least ") + std::to_string(MIN_ARGUMENTS_COUNT) + " arguments. Got " + std::to_string(argsCount);
    }

    // Arg 0: executable path
    const std::string executablePath(argv[0]);
    // get parent directory of executable using std::filesystem
    const std::string executableParentDir = std::filesystem::path(executablePath).parent_path().string();

    // Arg 1: mode ("dev" or "play")
    const std::string modeStr(argv[1]);
    vx::GameServer::Mode mode;
    if (modeStr == GAMESERVER_MODE_DEV) {
        mode = vx::GameServer::Mode::Dev;
    } else if (modeStr == GAMESERVER_MODE_PLAY) {
        mode = vx::GameServer::Mode::Play;
    } else {
        return std::string("mode is not supported. Expected ") + GAMESERVER_MODE_DEV + " or " + GAMESERVER_MODE_PLAY + ". Got " + modeStr;
    }

    // Arg 2: World ID
    const std::string worldID(argv[2]);
    if (worldID.empty() == true) {
        return std::string("world ID should not be an empty string");
    }

    // Arg 3: server ID prefix (example: eu-3-) (contains region and instance number)
    const std::string serverIDPrefix(argv[3]);
    if (serverIDPrefix.empty() == true) {
        return std::string("server ID prefix should not be an empty string");
    }

    // Arg 4: Hub API auth token
    const std::string hubAuthToken(argv[4]);
    if (hubAuthToken.empty()) {
        return "hub auth token argument is missing";
    }

    // Arg 5: number of player slots (optional)
    int maxPlayers = DEFAULT_MAX_PLAYERS;
    if (argsCount >= 5) {
        const std::string maxPlayersStr(argv[5]);
        // convert to int
        try {
            maxPlayers = std::stoi(maxPlayersStr);
        } catch (const std::invalid_argument) {
            return std::string("invalid max players value: ") + maxPlayersStr;
        }
    }

    // Get value of HOSTNAME environment variable
    const std::string hostname = std::string(std::getenv("HOSTNAME"));
    if (hostname.empty()) {
        return std::string("HOSTNAME environment variable is not set");
    }

    // Construct server ID with prefix and first 12 chars of hostname
    const std::string serverID = serverIDPrefix + hostname.substr(0, 12);
    
    // Load config.json file
    const bool foundConfig = vx::Config::load(executableParentDir + "/config.json");
    if (foundConfig == false) {
        vx::Config::setApiHost(HUB_API_URL);
    }

    // Parse hub API URL
    std::string hubAddr;
    uint16_t hubPort = 0;
    bool hubSecure = false;
    {
        // parse URL
        const vx::URL url = vx::URL::make(HUB_API_URL);
        if (url.isValid() == false) {
            return std::string("can't parse hub private URL: ") + HUB_API_URL;
        }
        
        // validate scheme
        const std::string scheme = url.scheme();
        if (scheme == "https") {
            hubSecure = true;
        } else if (scheme == "http") {
            hubSecure = false;
        } else {
            return std::string("unsupported URL scheme: ") + scheme;
        }

        hubAddr = url.host();
        hubPort = url.port();
    }

    outConfig.executableParentDir = executableParentDir;
    outConfig.mode = mode;
    outConfig.worldID = worldID;
    outConfig.serverID = serverID;
    outConfig.hubAuthToken = hubAuthToken;
    outConfig.hubAddr = hubAddr;
    outConfig.hubPort = hubPort;
    outConfig.hubSecure = hubSecure;
    outConfig.maxPlayers = maxPlayers;

    return ""; // no error
}
