
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include "colors.h"
#include "doubly_linked_list.h"
#include "color_atlas.h"
#include "asset.h"
#include "shape.h"

typedef struct _Transform Transform;
typedef struct _Shape Shape;

Shape *serialization_load_shape_from_assets(const char *relPath,
                                            const char *shapeFullname,
                                            ColorAtlas* colorAtlas,
                                            const ShapeSettings *shapeSettings);

Shape *serialization_load_shape_from_cache(const char *relPath,
                                           const char *shapeFullname,
                                           ColorAtlas* colorAtlas,
                                           const ShapeSettings *shapeSettings);

DoublyLinkedList *serialization_load_assets_from_assets(const char *relPath,
                                                        const char *assetFullname,
                                                        ASSET_MASK_T filter,
                                                        ColorAtlas* colorAtlas,
                                                        const ShapeSettings *shapeSettings);

DoublyLinkedList *serialization_load_assets_from_cache(const char *relPath,
                                                       const char *shapeFullname,
                                                       ASSET_MASK_T filter,
                                                       ColorAtlas* colorAtlas,
                                                       const ShapeSettings *shapeSettings);

bool serialization_save_shape_to_cache(Shape *shape,
                                       const void *imageData,
                                       const uint32_t imageDataSize,
                                       const char *filepath);

#ifdef __cplusplus
} // extern "C"
#endif
