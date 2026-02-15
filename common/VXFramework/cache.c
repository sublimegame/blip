// -------------------------------------------------------------
//  Cubzh Core
//  cache.c
//  Created by Gaetan de Villele on August 30, 2019.
// -------------------------------------------------------------

#include "cache.h"

#include <assert.h>
#include <dirent.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "utils.h"
#include "serialization.h"

// xptools
#include "vxlog.h"
#include "xptools.h"
#include "filesystem.h"

bool prefix(const char *pre, const char *str) {
    return strncmp(pre, str, strlen(pre)) == 0;
}

// Converts <repo>.<name>.pcubes form to <repo>/<name>.pcubes.
// Adding official/ prefix if necessary.
// Adding .pcubes suffix if necessary
// Returns same string if already in path format.
// Returned string should be released by caller.
// Returns NULL if something goes wrong.
char *cache_ensure_path_format(const char *path, const bool storage) {
    if (path == NULL) {
        return NULL;
    }
    if (strlen(path) == 0) {
        return NULL;
    }

    // vxlog_debug("[cache_ensure_path_format] %s (storage: %s)", path, storage ? "YES" : "NO");

    char *tmpStr = NULL;
    stringArray_t *tmpArr = NULL;

    // check extension, add it if missing
    if (strlen(path) >= 4 && strcmp(path + (strlen(path) - 4), ".3zh") == 0) {
        tmpStr = string_new_copy(path); // don't change anything
    } else {
        // add missing extension
        tmpStr = string_new_join(path, ".3zh", NULL);
    }

    // tmpStr: path with extension
    tmpArr = string_split(tmpStr, ".");
    const int len = stringArray_length(tmpArr);

    if (len > 3 || len < 2) {
        // invalid name, contains more than 2 '.' or no '.'
        free(tmpStr);
        stringArray_free(tmpArr);
        return NULL;

    } else if (len == 3) { // path is of the form <repo>.<name>.pcubes, replace first . by /
        free(tmpStr);
        tmpStr = string_new_join(stringArray_get(tmpArr, 0),
                                 c_getPathSeparatorCStr(),
                                 stringArray_get(tmpArr, 1),
                                 ".3zh",
                                 NULL);
        stringArray_free(tmpArr);
        tmpArr = NULL;
    } else {
        stringArray_free(tmpArr);
        tmpArr = NULL;
    }
    // NOTE: <repo>.<name>.3zh.cache is not handled in this if/else

    // tmpStr: <name>.pcubes or <repo>/<name>.pcubes

    // add official/ prefix if necessary
    {
        tmpArr = string_split(tmpStr, c_getPathSeparatorCStr()); // split on '/'

        const int elemCount = stringArray_length(tmpArr);
        if (elemCount == 1) { // trying to load official item: look in official
            char *tmpStr2 = tmpStr;
            tmpStr = string_new_join("official", c_getPathSeparatorCStr(), tmpStr2, NULL);
            free(tmpStr2);
        }
        stringArray_free(tmpArr);
        tmpArr = NULL;
    }

    return tmpStr;
}

///
bool cache_isFileInCache(const char *relFilepath, char **alternative) {

    // vxlog_debug("REL PATH: %s", relFilepath);

    char *sanitized_path = cache_ensure_path_format(relFilepath, true);

    // vxlog_debug("SANITIZED PATH: %s", sanitized_path);

    if (sanitized_path == NULL)
        return false;

    char *fullpath = string_new_join(CACHE_DIR, sanitized_path, NULL);

    // vxlog_debug("FULL PATH: %s", fullpath);

    bool isDir = false;
    const bool exists = c_storageFileExists(fullpath, &isDir);
    free(fullpath);

    const bool found = exists && isDir == false;

    if (found == false && alternative != NULL) {

        // vxlog_debug("LOOKING FOR ALTERNATIVE... (%s)", sanitized_path);

        // vxlog_debug("LOOKING FOR ALTERNATIVE... (%s) (%s)", sanitized_path, separator);

        stringArray_t *tmpArr = string_split(sanitized_path, c_getPathSeparatorCStr());
        free(sanitized_path);

        // vxlog_debug("LOOKING FOR ALTERNATIVE size: %d", stringArray_length(tmpArr));
        // vxlog_debug("LOOKING FOR ALTERNATIVE (2) (%s)", stringArray_get(tmpArr, 1));

        *alternative = string_new_join("official",
                                       c_getPathSeparatorCStr(),
                                       stringArray_get(tmpArr, 1),
                                       NULL);

        stringArray_free(tmpArr);
        // vxlog_debug("ALTERNATIVE: %s", *alternative);

        return cache_isFileInCache(*alternative, NULL); // search with "official"
    }

    // vxlog_debug("FOUND IN CACHE: %s", found ? "YES" : "NO");

    free(sanitized_path);
    return found;
}

/// NO PATH SANITIZE
bool cache_isFileInCacheRaw(const char *relFilepath) {
    char *fullpath = string_new_join(CACHE_DIR, relFilepath, NULL);

    bool isDir = false;
    bool exists = false;
    exists = c_storageFileExists(fullpath, &isDir);
    free(fullpath);

    return exists && isDir == false; // found
}

/// relFilepath : <repo>/<name>.<ext>
bool cache_writeFile(const char *relFilepath, const void *data, const uint32_t dataLength) {
    char *fullpath = string_new_join(CACHE_DIR, relFilepath, NULL);

    // open file for writing (binary mode)
    // (parent dirs created if missing)
    FILE *fp = c_openStorageFileWithSize(fullpath, "wb", dataLength);
    free(fullpath);
    fullpath = NULL;
    if (fp == NULL) {
        return false; // failure
    }

    // write data in file
    if (fwrite(data, sizeof(char), dataLength, fp) != dataLength) {
        fclose(fp);
        return false;
    }

    // close file
    if (fclose(fp) != 0) {
        return false; // failure
    }

    c_syncFSToDisk();
    return true;
}

///
bool cache_writePcubesFile(const char *relFilepath, const void *data, const uint32_t dataLength) {

    char *sanitized_path = cache_ensure_path_format(relFilepath, true);
    if (sanitized_path == NULL) {
        return false;
    }

    char *fullpath = string_new_join(CACHE_DIR, sanitized_path, NULL);
    free(sanitized_path);

    // open file for writing (binary mode)
    // (parent dirs created if missing)
    FILE *fp = c_openStorageFileWithSize(fullpath, "wb", dataLength);
    free(fullpath);
    if (fp == NULL) {
        return false; // failure
    }

    // write data in file
    if (fwrite(data, sizeof(char), dataLength, fp) != dataLength) {
        fclose(fp);
        return false;
    }

    // close file
    if (fclose(fp) != 0) {
        return false; // failure
    }

    c_syncFSToDisk();
    return true;
}

bool cache_removeFile(const char *relFilepath) {
    if (relFilepath == NULL) {
        return true;
    }

    char *sanitized_path = cache_ensure_path_format(relFilepath, true);
    if (sanitized_path == NULL) {
        return false;
    }

    char *fullpath = string_new_join(CACHE_DIR, sanitized_path, NULL);
    free(sanitized_path);

    const bool result = c_removeStorageFile(fullpath);
    free(fullpath);

    return result;
}

bool cache_removeFileRaw(const char *relFilepath) {
    if (relFilepath == NULL) {
        return true;
    }
    char *fullpath = string_new_join(CACHE_DIR, relFilepath, NULL);
    const bool result = c_removeStorageFile(fullpath);
    free(fullpath);
    return result;
}

FILE *cache_fopen(const char *relFilepath, const char *mode) {
    char *sanitized_path = cache_ensure_path_format(relFilepath, true);
    if (sanitized_path == NULL) {
        vxlog_error("🔥 %s", "sanitized_path is NULL");
        return NULL;
    }

    char *fullpath = string_new_join(CACHE_DIR, sanitized_path, NULL);
    free(sanitized_path);

    // (parent dirs created if missing)
    FILE *fp = c_openStorageFile(fullpath, mode);
    free(fullpath);

    return fp;
}

FILE *cache_fopenRaw(const char *relFilepath, const char *mode) {
    char *fullpath = string_new_join(CACHE_DIR, relFilepath, NULL);
    // (parent dirs created if missing)
    FILE *fp = c_openStorageFile(fullpath, mode);
    free(fullpath);
    return fp;
}

bool cache_getFileTextContent(const char *relFilepath, char **text) {
    if (relFilepath == NULL || strlen(relFilepath) == 0 || text == NULL) {
        return false;
    }

    FILE *fd = cache_fopenRaw(relFilepath, "r");
    if (fd == NULL) {
        return false;
    }

    fseek(fd, 0, SEEK_END);
    long fsize = ftell(fd);
    fseek(fd, 0, SEEK_SET);

    char *string = (char *)malloc(fsize + 1);
    if (string == NULL) {
        fclose(fd);
        return false;
    }
    fread(string, 1, fsize, fd);
    fclose(fd);

    string[fsize] = 0;
    *text = string;

    return true;
}

// MARK: - Baked files -

bool cache_save_baked_file(const Shape *s) {
    if (shape_uses_baked_lighting(s) == false) {
        return false;
    }

    const uint64_t hash = shape_get_baked_lighting_hash(s);
    char bakedFileID[20]; snprintf(bakedFileID, 20, "%" PRIu64, hash);
    char *bakedFilepath = utils_get_baked_fullname(bakedFileID, shape_get_fullname(s));
    if (bakedFilepath == NULL) {
        return false;
    }

    FILE *fd = cache_fopen(bakedFilepath, "wb");
    if (fd == NULL) {
        free(bakedFilepath);
        return false;
    }

    const bool success = serialization_save_baked_file(s, hash, fd);
    fclose(fd);

    if (success) {
        vxlog_info("💾 cache: successfully saved baked file %s", bakedFilepath);
    } else {
        vxlog_error("failed to save baked file %s", bakedFilepath);
    }
    free(bakedFilepath);

    return success;
}

bool cache_load_or_clear_baked_file(Shape *s) {
    const uint64_t hash = shape_get_baked_lighting_hash(s);
    char bakedFileID[20]; snprintf(bakedFileID, 20, "%" PRIu64, hash);
    char *bakedFilepath = utils_get_baked_fullname(bakedFileID, shape_get_fullname(s));
    if (bakedFilepath == NULL) {
        shape_toggle_baked_lighting(s, false);
        return false;
    }

    FILE *fd = cache_fopen(bakedFilepath, "rb");
    if (fd == NULL) {
        free(bakedFilepath);
        shape_toggle_baked_lighting(s, false);
        return false;
    }

    const bool success = serialization_load_baked_file(s, hash, fd);
    fclose(fd);

    if (success) {
        vxlog_info("💾 cache: successfully loaded baked file %s", bakedFilepath);
        free(bakedFilepath);
        shape_toggle_baked_lighting(s, true);
        return true;
    } else {
        if (cache_removeFile(bakedFilepath)) {
            vxlog_info("💾 cache: obsolete baked file %s removed", bakedFilepath);
        } else {
            vxlog_error("💾 ️⚠️ cache: failed to remove obsolete baked file %s", bakedFilepath);
        }

        free(bakedFilepath);
        shape_toggle_baked_lighting(s, false);
        return false;
    }
}

bool cache_remove_baked_files(const char *repo, const char *name) {
    if (repo == NULL || name == NULL) {
        return false;
    }
    char *fullpath = string_new_join(CACHE_DIR, repo, NULL);
    char *prefix = string_new_join("baked_", name, "_", NULL);
    const bool ok = c_removeStorageFilesWithPrefix(fullpath, prefix);
    free(fullpath);
    free(prefix);
    return ok;
}
