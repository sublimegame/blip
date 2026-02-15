//
//  VXConfig.cpp
//  Cubzh
//
//  Created by Adrien Duermael on 3/9/20.
//

#include "VXConfig.hpp"

#include <stdio.h>
#include <string.h>

#include "xptools.h"
#include "utils.h"

using namespace vx;

char* Config::_apiHost = nullptr;
std::string Config::_debugGameServer = "";
std::string Config::_debugHubServer = "";
std::string Config::_gameServerTag = "";
std::string Config::_version = "";
std::string Config::_commit = "";
bool Config::_loaded = false;

void _mergeJSON(cJSON *dst, cJSON *src) {
    cJSON *child = NULL;
    cJSON_ArrayForEach(child, src) {
        cJSON_DetachItemFromObject(src, child->string);
        if (cJSON_GetObjectItemCaseSensitive(dst, child->string)) {
            cJSON_ReplaceItemInObjectCaseSensitive(dst, child->string, child);
        } else {
            cJSON_AddItemToObject(dst, child->string, child);
        }
    }
}

cJSON *_loadJSONFile(std::string filename) {
    FILE *file = fs::openBundleFile(filename.c_str());
    if (file == nullptr) {
        vxlog_error("can't open %s", filename.c_str());
        return nullptr;
    }

    char *chars = fs::getFileTextContentAndClose(file);
    if (chars == nullptr) {
        vxlog_error("can't read %s (1)", filename.c_str());
        return nullptr;
    }

    cJSON *json = cJSON_Parse(chars);
    free(chars);

    if (json == nullptr) {
        vxlog_error("can't read %s (2)", filename.c_str());
        return nullptr;
    }

    if (cJSON_IsObject(json) == false) {
        vxlog_error("%s: top level object expected", filename.c_str());
        cJSON_Delete(json);
        return nullptr;
    }

    return json;
}

bool Config::load() {

    if (Config::_loaded) {
        // already loaded, just return true
        return true;
    }

    cJSON *config = _loadJSONFile("config.json");
    if (config == nullptr) {
        return false;
    }

    //vxlog_debug("%s\n", cJSON_Print(config));

    // MANDATORY FIELDS

    if (cJSON_HasObjectItem(config, "APIHost") == false) {
        vxlog_error("config: missing APIHost");
        cJSON_Delete(config);
        return false;
    }

    const cJSON *apiHostEntry = cJSON_GetObjectItem(config, "APIHost");

    if (cJSON_IsString(apiHostEntry) == false) {
        vxlog_error("config: APIHost should be a string");
        cJSON_Delete(config);
        return false;
    }

    Config::setApiHost(cJSON_GetStringValue(apiHostEntry));

    // OPTIONAL FIELDS

    Config::_debugGameServer = readOptionalField(config, "DebugGameServer");
    Config::_debugHubServer = readOptionalField(config, "DebugHubServer");
    Config::_gameServerTag = readOptionalField(config, "GameServerTag");
    Config::_version = readOptionalField(config, "Version");
    Config::_commit = readOptionalField(config, "Commit");

    Config::_loaded = true;

    cJSON_Delete(config);
    return true;
}

void Config::setApiHost(const char *host) {
    if (Config::_apiHost != nullptr) {
        free(Config::_apiHost);
        Config::_apiHost = nullptr;
    }
    Config::_apiHost = string_new_copy(host);
}

const char* Config::apiHost() {
    return Config::_apiHost == nullptr ? "" : Config::_apiHost;
}

std::string Config::debugGameServer() {
    return Config::_debugGameServer;
}

std::string Config::debugHubServer() {
    return Config::_debugHubServer;
}

std::string Config::gameServerTag() {
    return Config::_gameServerTag;
}

std::string Config::version() {
    return Config::_version;
}

std::string Config::commit() {
    return Config::_commit;
}

std::string Config::readOptionalField(const cJSON* config, const char* name) {
    if (cJSON_HasObjectItem(config, name)) {
        const cJSON *tagEntry = cJSON_GetObjectItem(config, name);
        if (cJSON_IsString(tagEntry)) {
            char *str = cJSON_GetStringValue(tagEntry);
            if (str != nullptr) {
                return std::string(str);
            }
        }
    }
    return std::string();
}
