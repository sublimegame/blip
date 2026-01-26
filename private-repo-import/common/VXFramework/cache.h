// -------------------------------------------------------------
//  Cubzh Core
//  cache.h
//  Created by Gaetan de Villele on August 30, 2019.
// -------------------------------------------------------------

#pragma once

#if defined(__cplusplus)
extern "C" {
#endif

// C
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "shape.h"

// path of Cache directory relative to storage root directory
#define CACHE_DIR "cache/"

/// md5CStr is a buffer designed to contain an md5 string, with a NULL terminator.
/// Having a NULL terminator allows to use it as a regular C string (`char *`).
typedef char md5CStr[32+1];
#define md5CStrZero {0}

/// Returns whether a given file is present in the cache directory.
/// Looking for alternative in official repo when alternative isn't NULL.
bool cache_isFileInCache(const char *relFilepath, char **alternative);

///
bool cache_isFileInCacheRaw(const char *relFilepath);

/// Write file in cache.
/// Return value indicates whether the operation succeeded.
bool cache_writeFile(const char *relFilepath, const void *data, const uint32_t dataLength);

/// Write file in cache. (function is specific to pcubes files)
/// Return value indicates whether the operation succeeded.
bool cache_writePcubesFile(const char *relFilepath, const void *data, const uint32_t dataLength);

/// Removes file from cache
/// @param relFilepath file path, relative to the cache's root
/// @return Returns whether the file has been removed (or was already absent).
///         Returns true if the provided path is NULL.
bool cache_removeFile(const char *relFilepath);

///
bool cache_removeFileRaw(const char *relFilepath);

// fopen for files in cache
FILE *cache_fopen(const char *relFilepath, const char *mode);

//
FILE *cache_fopenRaw(const char *relFilepath, const char *mode);

//
bool cache_getFileTextContent(const char *relFilepath, char **text);

/// converts <repo>.<name>.pcubes string to <repo>/<name>.pcubes
/// converts <name>.pcubes string to official/<name>.pcubes
/// Adds "official/" prefix if necessary.
/// Adds ".3zh" suffix if necessary.
/// Returns same string if already in path format.
/// Returned string should be released by caller.
/// Returns NULL if something goes wrong.
char *cache_ensure_path_format(const char *path, const bool storage);

// MARK: - Baked files -

bool cache_save_baked_file(const Shape *s);
bool cache_load_or_clear_baked_file(Shape *s);
bool cache_remove_baked_files(const char *repo, const char *name);

/// Returns true on success, false otherwise.
bool cache_getCacheInfoForItem(const char *itemFilePath, md5CStr * const outEtag, int32_t * const outTimestamp);

/// Returns true on success, false otherwise.
bool cache_setCacheInfoForItem(const char *itemFilePath, const char *etagCStr, const int32_t * const timestamp);

#if defined(__cplusplus)
} // extern "C"
#endif

// ------------------------------
// Cache C++ API
// ------------------------------

#if defined(__cplusplus)

// C++
#include <memory>
#include <string>

///
typedef std::shared_ptr<std::string> CacheErr;

///
bool cacheCPP_getItemCacheInfo(const std::string& repo, 
                               const std::string& name,
                               std::string& outEtag,
                               int32_t& outCreationTime);

/// Make cache expire. (max-age)
CacheErr cacheCPP_expireCacheForItem(const std::string& repo, 
                                     const std::string& name);

#endif
