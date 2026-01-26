
#include "fileutils.h"

#include "xptools.h"
#include "serialization.h"
#include "cache.h"

/// NOTE: "assets" means "application bundle"
Shape *serialization_load_shape_from_assets(const char *relPath,
                                            const char *shapeFullname,
                                            ColorAtlas* colorAtlas,
                                            const ShapeSettings *shapeSettings) {
    DoublyLinkedList *assets = serialization_load_assets_from_assets(relPath, shapeFullname, AssetType_Shape, colorAtlas, shapeSettings);
    if (assets == NULL) {
        return NULL;
    }
    Shape *shape = assets_get_root_shape(assets, true);

    // do not keep ownership on sub-objects + free unused palette
    doubly_linked_list_flush(assets, serialization_assets_free_func);
    doubly_linked_list_free(assets);

    return shape;
}

///
Shape *serialization_load_shape_from_cache(const char *relPath,
                                           const char *shapeFullname,
                                           ColorAtlas* colorAtlas,
                                           const ShapeSettings *shapeSettings) {
    DoublyLinkedList *assets = serialization_load_assets_from_cache(relPath, shapeFullname, AssetType_Shape, colorAtlas, shapeSettings);
    if (assets == NULL) {
        return NULL;
    }
    Shape *shape = assets_get_root_shape(assets, true);

    // do not keep ownership on sub-objects + free unused palette
    doubly_linked_list_flush(assets, serialization_assets_free_func);
    doubly_linked_list_free(assets);

    return shape;
}

DoublyLinkedList *serialization_load_assets_from_assets(const char *relPath, const char *assetFullname,
                                                        ASSET_MASK_T filter, ColorAtlas *colorAtlas,
                                                        const ShapeSettings *shapeSettings) {

    if (relPath == NULL) {
        return NULL;
    }

    FILE *fd = c_openBundleFile(relPath, "rb");
    if (fd == NULL) {
        return NULL;
    }

    // The file descriptor is owned by the stream, which will fclose it in the future.
    Stream *s = stream_new_file_read(fd);
    DoublyLinkedList *list = NULL;
    serialization_load_assets(s, assetFullname, filter, colorAtlas, shapeSettings, false, &list);
    
    return list;
}

///
DoublyLinkedList *serialization_load_assets_from_cache(const char *relPath,
                                                       const char *shapeFullname,
                                                       ASSET_MASK_T filter,
                                                       ColorAtlas *colorAtlas,
                                                       const ShapeSettings *settings) {

    // check whether wanted file is present in cache
    char *alternative = NULL;
    if (cache_isFileInCache(relPath, &alternative) == false) {
        vxlog_warning("file not found in cache: %s", relPath);
        if (alternative != NULL) {
            free(alternative);
            alternative = NULL;
        }
        return NULL;
    }

    FILE *fd = cache_fopen(alternative != NULL ? alternative : relPath, "rb");
    if (alternative != NULL) {
        free(alternative);
        alternative = NULL;
    }
    if (fd == NULL) { // failed to open file
        vxlog_error("[serialization_load_assets_from_cache] failed to open file");
        return NULL;
    }

    // The file descriptor is owned by the stream, which will fclose it in the future.
    Stream *s = stream_new_file_read(fd);
    DoublyLinkedList *list = NULL;
    serialization_load_assets(s, shapeFullname, filter, colorAtlas, settings, false, &list);

    return list;
}

bool serialization_save_shape_to_cache(Shape *shape,
                                       const void *imageData,
                                       const uint32_t imageDataSize,
                                       const char *filepath) {

    FILE *fd = cache_fopen(filepath, "wb");
    if (fd == NULL) {
        vxlog_error("can't open file (%s)", filepath);
        return false;
    }
    
    // fd closed within serialization_save_shape
    const bool success = serialization_save_shape(shape,
                                                  imageData,
                                                  imageDataSize,
                                                  fd);
    return success;
}
