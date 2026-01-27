//
//  VXUpdateManager.cpp
//  Cubzh
//
//  Created by Xavier Legland on 2/14/22.
//

#include "VXUpdateManager.hpp"

// xptools
#include "xptools.h"
#include "device.hpp"

#include "config.h"
#include "VXMenuConfig.hpp"

// C++
#include <cstring>

using namespace vx;

UpdateManager::UpdateManager() :
_version(""),
_buildNumber(0) {
    
    const std::string versionFilePath = "/app-version.json";
    
    bool isDir;
    if (fs::storageFileExists(versionFilePath, isDir) == false) {
        return;
    }
    
    if (isDir) {
        fs::removeStorageFileOrDirectory(versionFilePath);
    }
    
    FILE *versionFile = fs::openStorageFile("/app-version.json");
        
    if (versionFile == nullptr) {
        vxlog_error("can't open %s", versionFilePath.c_str());
        return;
    }
                
    char * const versionChars = fs::getFileTextContentAndClose(versionFile);
    if (versionChars == nullptr) {
        // not supposed to happen
        vxlog_error("can't read %s", versionFilePath.c_str());
        return;
    }
    
    // get the previous version
    cJSON * const versionJson = cJSON_Parse(versionChars);
    free(versionChars);
    
    if (versionJson == nullptr) {
        vxlog_error("can't get json from %s", versionFilePath.c_str());
        return;
    }
    
    if (cJSON_IsObject(versionJson) == false) {
        vxlog_error("%s: top level object expected", versionFilePath.c_str());
        cJSON_Delete(versionJson);
        return;
    }
    
    if (cJSON_HasObjectItem(versionJson, "version")) {
        cJSON *versionItem = cJSON_GetObjectItem(versionJson, "version");
        if (cJSON_IsString(versionItem)) {
            _version = std::string(versionItem->valuestring);
        }
    }
    
    if (cJSON_HasObjectItem(versionJson, "build")) {
        cJSON *buildItem = cJSON_GetObjectItem(versionJson, "build");
        if (cJSON_IsNumber(buildItem)) {
            _buildNumber = buildItem->valueint;
        }
    }
    
    cJSON_Delete(versionJson);
}

UpdateManager::~UpdateManager() {}

bool UpdateManager::firstVersionRun() const {
    return _version != vx::device::appVersion() || _buildNumber != vx::device::appBuildNumber();
}

void UpdateManager::stampCurrentVersion() {
    
    const std::string versionFilePath = "/app-version.json";
    
    FILE *f = fs::openStorageFile(versionFilePath, "wb");
    if (f == nullptr) {
        vxlog_error("can't open %s", versionFilePath.c_str());
        return;
    }
    
    cJSON *versionJson = cJSON_CreateObject();
    
    cJSON_AddStringToObject(versionJson, "version", vx::device::appVersion().c_str());
    cJSON_AddNumberToObject(versionJson, "build", vx::device::appBuildNumber());
    
    char *s = cJSON_PrintUnformatted(versionJson);
    const std::string jsonStr = std::string(s);
    free(s);
    
    cJSON_Delete(versionJson);
    
    fwrite(jsonStr.c_str(), sizeof(char), jsonStr.size(), f);
    fclose(f);

    // TODO: gdevillele: hide this somewhere else
    //                   replace this by a call in willResignActive maybe?
    vx::fs::syncFSToDisk();
}
