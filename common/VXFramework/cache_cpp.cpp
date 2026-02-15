//
//  cache.cpp
//  Cubzh
//
//  Created by Gaetan de Villele on 06/03/2023.
//

#include "cache.h"

// C++
#include <cstring>
#include <string>

// xptools
#include "filesystem.hpp"
#include "vxlog.h"
#include "VXJSON.hpp"

#include "cJSON.h"

#include "cache.h"

// ------------------------------
// CacheInfo type
// ------------------------------
class CacheInfo {
public:

    static const std::string FIELD_TIME;
    static const std::string FIELD_ETAG;

    inline CacheInfo() :
    _time(nullptr),
    _etag() {}

    inline CacheInfo(const int32_t * const time, const std::string& etag) :
    _time(nullptr),
    _etag(etag) {
        if (time != nullptr) {
            _time = static_cast<int32_t *>(malloc(sizeof(int32_t)));
            *_time = *time;
        }
    }

    inline ~CacheInfo() {
        if (_time != nullptr) {
            free(_time);
        }
    }

    /// Returns an error message. Empty error message means "no error".
    std::string marshal(std::string& outJsonStr) {
        // encode to JSON
        cJSON *json = cJSON_CreateObject();

        if (_time != nullptr) {
            vx::json::writeIntField(json, CacheInfo::FIELD_TIME, *_time);
        }
        if (_etag.empty() == false) {
            vx::json::writeStringField(json, CacheInfo::FIELD_ETAG, _etag);
        }

        char *s = cJSON_PrintUnformatted(json);
        outJsonStr.assign(s);
        free(s);

        cJSON_Delete(json);
        json = nullptr;

        return ""; // no error
    }

    /// Returns an error message. Empty error message means "no error".
    std::string unmarshal(const std::string &jsonStr) {
        // decode JSON string
        cJSON *json = cJSON_Parse(jsonStr.c_str());
        if (json == nullptr) {
            return "failed to parse JSON string";
        }

        int32_t time;
        std::string etag;
        vx::json::readIntField(json, CacheInfo::FIELD_TIME, time, true);
        vx::json::readStringField(json, CacheInfo::FIELD_ETAG, etag, true);

        cJSON_Delete(json);
        json = nullptr;

        if (_time == nullptr) {
            _time = static_cast<int32_t *>(malloc(sizeof(int32_t)));
            if (_time == nullptr) {
                return "failed to alloc memory for CacheInfo";
            }
        }
        
        *_time = time;
        _etag = etag;

        return ""; // no error
    }

    inline int32_t *getTime() const { return _time; }
    inline std::string getEtag() const { return _etag; }

private:

    /// seconds since 2001/01/01 00:00:00
    int32_t *_time;

    ///
    std::string _etag;
};
const std::string CacheInfo::FIELD_TIME = "time";
const std::string CacheInfo::FIELD_ETAG = "etag";
// ------------------------------

// ------------------------------
// UNEXPOSED API
// ------------------------------

bool cache_isItemRelFilepathValid(const std::string& itemRelFilepath) {
    // `itemRelFilepath` must be of the form: <repo>/<name>.3zh
    // TODO: gaetan: implement me!
    return true;
}

// return true on success, false otherwise
bool cache_getCacheFilepathForItem(const std::string& itemRelFilepath, std::string& output) {
    if (cache_isItemRelFilepathValid(itemRelFilepath) == false) {
        output = "itemRelFilepath value is not valid";
        return false;
    }
    // cache/<repo>/<name>.3zh.cache
    output = CACHE_DIR + itemRelFilepath + ".cache";
    return true;
}

// itemRelFilepath must be of the form: <repo>/<name>.3zh
bool cache_cacheFileExistsForItem(const std::string& itemRelFilepath) {
    vxlog_debug("[cache_3zhFileCacheExists] %s", itemRelFilepath.c_str());

    // get path of .cache file
    std::string cacheFilepath;
    {
        const bool ok = cache_getCacheFilepathForItem(itemRelFilepath, cacheFilepath);
        if (ok == false) {
            vxlog_debug("[cache_cacheFileExistsForItem] ERROR: %s", cacheFilepath.c_str());
            return false;
        }
    }

    // check if a file exists at this path
    {
        bool isDir = false;
        const bool fileExists = vx::fs::storageFileExists(cacheFilepath, isDir);
        if (fileExists == false || isDir) {
            return false;
        }
    }

    return true; // .cache file exists
}

// ------------------------------
// API for cache system v2
// ------------------------------

extern "C" {

// `itemCacheFilepath` is of the form `<repo>/<name>.3zh`
// return true on success, false otherwise
bool cache_getCacheInfoForItem(const char *itemFilePath, md5CStr * const outEtag, int32_t * const outTimestamp) {
    std::string cacheInfoPath;
    {
        const bool ok = cache_getCacheFilepathForItem(std::string(itemFilePath), cacheInfoPath);
        if (ok == false) {
            vxlog_debug("[cache_getCacheDateFormItem] ERROR: %s", cacheInfoPath.c_str());
            return false;
        }
    }

    FILE *fd = vx::fs::openStorageFile(cacheInfoPath);
    if (fd == nullptr) {
        return false;
    }

    std::string fileContent;
    const bool ok = vx::fs::getFileTextContentAsStringAndClose(fd, fileContent);
    if (ok == false) {
        vxlog_debug("[cache_getCacheDateFormItem] ERROR: failed to read file %s", cacheInfoPath.c_str());
        return false;
    }

    // parse JSON file content
    CacheInfo info;
    const std::string err = info.unmarshal(fileContent);
    if (err.empty() == false) {
        vxlog_debug("[cache_getCacheDateFormItem] ERROR: %s", err.c_str());
        return false;
    }

    const std::string etag = info.getEtag();
    if (outEtag != nullptr && etag.empty() == false) {
        strcpy(*outEtag, etag.c_str()); // strcpy does add the trailing NULL char
    }

    if (outTimestamp != nullptr && info.getTime() != nullptr) {
        *outTimestamp = *info.getTime();
    }

    // outEtag = info.getEtag();
    return true;
}

/// Returns true on success, false otherwise.
bool cache_setCacheInfoForItem(const char *itemFilePath, const char *etagCStr, const int32_t * const timestamp) {
    if (itemFilePath == nullptr || timestamp == nullptr || etagCStr == nullptr) {
        return false;
    }

    std::string cacheInfoPath;
    {
        const bool ok = cache_getCacheFilepathForItem(std::string(itemFilePath), cacheInfoPath);
        if (ok == false) {
            vxlog_debug("cache_setCacheInfoForItem - err (1): %s", cacheInfoPath.c_str());
            return false;
        }
    }

    const std::string etagStr(etagCStr);
    CacheInfo info(timestamp, etagStr);

    std::string jsonStr;
    const std::string err = info.marshal(jsonStr);
    if (err.empty() == false) {
        vxlog_error("cache_setCacheInfoForItem - err (2): %s", err.c_str());
        return false; // failure
    }

    FILE *fd = vx::fs::openStorageFile(cacheInfoPath.c_str(), "wb");
    if (fd == nullptr) {
        vxlog_error("cache_setCacheInfoForItem - err (3): %s", cacheInfoPath.c_str());
        return false;
    }

    size_t written = fwrite(jsonStr.c_str(), 1, jsonStr.length(), fd);
    fclose(fd);
    fd = nullptr;

    const bool ok = written == 1 * jsonStr.length();
    return ok;
}

} // extern "C"

bool cacheCPP_getItemCacheInfo(const std::string& repo, const std::string& name, std::string& outEtag, int32_t& outCreationTime) {

    // get cache info for item
    md5CStr cacheEtag = md5CStrZero;
    int32_t cacheTimestamp = 0;

    std::string repoName = repo + "/" + name;

    const std::string itemFilePathVoxels = repoName + ".3zh";
    const bool ok = cache_getCacheInfoForItem(itemFilePathVoxels.c_str(), &cacheEtag, &cacheTimestamp);
    if (ok) {
        outEtag.assign(cacheEtag);
        outCreationTime = cacheTimestamp;
    } else { // try to load a .glb
        const std::string itemFilePathMesg = repoName + ".glb";
        outEtag.assign(cacheEtag);
        outCreationTime = cacheTimestamp;
    }

    return ok;
}

#include "VXResource.hpp"

// returns an error message
CacheErr cacheCPP_expireCacheForItem(const std::string& repo, const std::string& name) {
#if defined(CLIENT_ENV)
    if (repo.empty() || name.empty()) {
        return CacheErr(new std::string("item repo or name is empty"));
    }
    // set time of cache to timestamp 0 (2001-01-01)
    const int32_t timestamp = 0;
    const bool ok = updateItemCacheInfo(repo.c_str(), name.c_str(), nullptr, timestamp);
    if (ok == false) {
        const std::string errMsg = "failed to make item cache expire for " + repo + "." + name;
        vxlog_warning("%s", errMsg.c_str());
        return CacheErr(new std::string(errMsg));
    }
#endif
    return nullptr; // no error
}

