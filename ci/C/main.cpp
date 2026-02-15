//
//  main.c
//
//  Created by Adrien Duermael on 01/02/20.
//  Copyright © 2020 voxowl. All rights reserved.
//

// C
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

// Lua
#include "lua.hpp"

// libsbs
#include "sbs.hpp"

#include "cache.h"

// Lua Sandbox
#include "lua_client.hpp"
#include "lua_sandbox.hpp"
#include "lua_utils.hpp"

#include "GameCoordinator.hpp"
#include "lua_node.hpp"
#include "VXConfig.hpp"
#include "VXGame.hpp"
#include "VXGameRenderer.hpp"
#include "VXNode.hpp"

#include "ThreadManager.hpp"

// functions' prototypes

int main(int argc, char *argv[]) {

    lua_State *lstate = nullptr;

    // loading config here manually because CI doesn't go
    // through GameCoordinator::_startupLoading
    bool success = vx::Config::load();
    if (success == false) {
        printf("%s\n", "🔥 can't load config.json");
        return 1;
    }

    vx::ThreadManager::shared().setMainThread();

    // NOTE:
    // Storage (inclusing cache) is already installed in the
    // CI container. No need to copy anything from the bundle.
    // cache_copy_bundle_files();

    // 1 - Load Lua script

#if defined(CI_TEST_LUA_FILE)
    const char *scriptFilename = CI_TEST_LUA_FILE;
#else
    const char *scriptFilename = "./test.lua";
#endif
    FILE *fp = fopen(scriptFilename, "rb");
    if (fp == nullptr) {
        printf("%s\n", "🔥 failed to open test.lua");
        return 1;
    }
    fseek(fp, 0, SEEK_END);
    long fileSize = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    char *scriptBytes = static_cast<char *>(malloc(fileSize + 1));
    fread(scriptBytes, 1, fileSize, fp);
    scriptBytes[fileSize] = 0; // NULL terminator
    fclose(fp);

    // GameRenderer needs to be intialized
    vx::GameRenderer::newGameRenderer(QualityTier_Low, 800, 600, 1.0f);

    // empty
    std::unordered_map<std::string, std::string> env;

    std::string script = std::string(scriptBytes);
    vx::GameData_SharedPtr data = vx::GameData::make(script, "", "", 1);

    //
    vx::GameCoordinator::getInstance()->loadWorld(
        GameMode_ClientOffline,
        data,
        vx::WorldFetchInfo::makeWithID("958fa098-7482-4a7a-b863-ec9330f0f4a1"),
        "",                         // no server address
        false,                      // not dev mode
        false,                      // not author
        std::vector<std::string>(), // no extra items to load
        env);                       // empty env

    vx::Game_SharedPtr g = nullptr;

    while (g == nullptr) {
        // waiting for world to be loaded
        vx::GameCoordinator::getInstance()->tickForCI(0.05);
        g = vx::GameCoordinator::getInstance()->getLoadedWorld();
    }

    lstate = g->getLuaState();

    LUAUTILS_STACK_SIZE(lstate);

    // run the lua Client.test_main function
    if (lua_client_pushField(lstate, "test_main") == false) {
        printf("%s\n", "lua test main function not found");
        return 1; // failure
    }
    if (lua_isfunction(lstate, -1) == 0) {
        printf("%s\n", "lua test main function not found");
        return 1; // failure
    }
    const int err = lua_pcall(lstate, 0, 1, 0); // 1 return value
    if (err != LUA_OK) {
        printf("%s\n", "failed to execute lua test function");
        // error string has been pushed onto the lua stack // +1
        if (lua_utils_isStringStrict(lstate, -1)) {
            printf("%s\n", lua_tostring(lstate, -1));
        } else {
            printf("%s\n", "lua error is not a string");
        }
        lua_pop(lstate, 1);
        return 1; // failure
    }
    // check for return value
    LUAUTILS_STACK_SIZE_ASSERT(lstate, 1);

    int typeOnTopOfStack = lua_type(lstate, -1);

    switch (typeOnTopOfStack) {
        case LUA_TSTRING: {
            const char *errorStr = lua_tostring(lstate, -1);
            printf("🔥 lua test function error: %s\n", errorStr);
            lua_pop(lstate, 1);
            return 1;
        }
        case LUA_TNIL: {
            // success
            break;
        }
        default: {
            printf("Unexpected return value from lua test main function\n");
            return 1;
        }
    }

    printf("Test suite ended successfully\n");
    return 0;
}
