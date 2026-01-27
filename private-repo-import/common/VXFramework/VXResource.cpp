//
//  VXResource.cpp
//  Cubzh
//
//  Created by Adrien Duermael on 15/10/2020.
//

#include "VXResource.hpp"

#include "xptools.h"
#include "vxlog.h"
#include "BZMD5.hpp"
#include "config.h"

#if defined(CLIENT_ENV)

#include "VXHubClient.hpp"
#include "cache.h"

bool saveItemInCache(const std::string& repo,
                     const std::string& name,
                     const std::string& pcubes) {

    // attempt to flush the existing cache
    const std::string pcubesPath = repo + vx::fs::getPathSeparatorStr() + name + ".3zh";
    if (cache_removeFileRaw(pcubesPath.c_str())) {
        // vxlog_debug("cache : removed %s", pcubesPath.c_str());
    }

    // compute the md5 checksum of the pcubes data
    const std::string pcubesMD5Digest = md5(pcubes);

    // get current timestamp (Apple version from 2001-01-01)
    const int32_t timestampApple = vx::device::timestampApple();

    // write cache info file (.3zh.cache)
    bool ok = cache_setCacheInfoForItem(pcubesPath.c_str(), pcubesMD5Digest.c_str(), &timestampApple);
    if (ok == false) {
        // cache info file write failure
        vxlog_error("failed to write cache info file %s", pcubesPath.c_str());
        return false;
    }

    // now write the pcubes file
    ok = cache_writePcubesFile(pcubesPath.c_str(), pcubes.c_str(), static_cast<uint32_t>(pcubes.size()));
    if (ok == false) {
        vxlog_error("FAILED TO WRITE pcubes file !");
        return false;
    }

    // vxlog_debug("written %s", pcubesPath.c_str());
    return true;
}

bool updateItemCacheInfo(const std::string& repo,
                         const std::string& name,
                         std::string * const etag,
                         const int32_t time) {

    if (etag != nullptr) {
        for (int i = 0; i < 3; ++i) {
            vxlog_error("ERROR [updateItemCacheInfo] etag value is not supported yet");
        }
        return false;
    }

    const std::string itemFilePath = repo + vx::fs::getPathSeparatorStr() + name + ".3zh";

    md5CStr etagTmp = md5CStrZero;
    int32_t timeTmp = 0;
    bool ok = cache_getCacheInfoForItem(itemFilePath.c_str(), &etagTmp, &timeTmp);
    if (ok == false) {
        vxlog_error("failed to update cache info for item %s", itemFilePath.c_str());
        return false;
    }
    // vxlog_debug("UPDATING CACHE INFO FOR %s/%s", repo.c_str(), name.c_str());
    // vxlog_debug("  before : %d %s", timeTmp, etagTmp);
    // vxlog_debug("  after  : %d %s", time, "-");

    // write cache info file (.3zh.cache)
    ok = cache_setCacheInfoForItem(itemFilePath.c_str(), etagTmp, &time);
    if (ok == false) {
        // cache info file write failure
        vxlog_error("failed to update cache info for item %s", itemFilePath.c_str());
        return false;
    }

    // vxlog_debug("updated %s", itemFilePath.c_str());
    return true;
}

#endif
