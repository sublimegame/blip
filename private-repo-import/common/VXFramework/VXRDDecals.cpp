// -------------------------------------------------------------
//  Cubzh
//  VXRDDecals.cpp
//  Created by Arthur Cormerais on May 20, 2020.
// -------------------------------------------------------------

#include "VXRDDecals.hpp"

using namespace vx;
using namespace rendering;

#ifndef P3S_CLIENT_HEADLESS

// vx
#include "vxlog.h"
#include "VXGameRenderer.hpp"

// bgfx
// to access some helpers eg. release bgfx Memory
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Weverything"
#include "../bgfx/bgfx/src/bgfx_p.h"
#pragma GCC diagnostic pop

Decals::Decals(RendererFeatures *features, QualityParameters *quality, bgfx::IndexBufferHandle ibh,
               bgfx::ProgramHandle *ph, bgfx::UniformHandle *fb1, bgfx::UniformHandle *params,
               bgfx::UniformHandle *color1) {

    _features = features;
    _qualityParams = quality;
    _voxelIbh = ibh;
    _program = ph;
    _scene = nullptr;
    _sFb1 = fb1;
    _uParams = params;
    _uColor1 = color1;

#if DEBUG_RENDERER_DECALS_STATS
    _debug_requests = 0;
    _debug_projections = 0;
#endif
}

Decals::~Decals() {
    removeAll();
}

bool Decals::request(uint32_t id, float3 eye, float width, float height, uint32_t stencil,
                     Transform *filterOut, Projection proj, UpdateMode mode, bool recompute) {

    DecalEntry *entry = nullptr;
    const bool newEntry = tryGet(id, &entry) == false;
    if (newEntry) {
        _decalEntries[id] = createDecalEntry(eye, width, height, stencil, filterOut, proj, mode);
        entry = &_decalEntries[id];
    }

    const bool newProj = newEntry || (projectionEquals(entry, eye, width, height, proj) == false);
    if (mode == UpdateMode::Replace) {
        if (newProj) {
            setProjection(entry, eye, width, height, proj);
            entry->count = 0;
        } else if (recompute) {
            entry->count = 0;
        }
    } else {
        vxlog_warning("⚠ Decals: update mode not implemented, skipping request");
        return false;
    }

#if DEBUG_RENDERER_DECALS_STATS
    ++_debug_requests;
#endif

    return newEntry;
}

void Decals::setColor(uint32_t id, float r, float g, float b, float a) {
    DecalEntry *entry = nullptr;
    if (tryGet(id, &entry)) {
        entry->rgba[0] = r;
        entry->rgba[1] = g;
        entry->rgba[2] = b;
        entry->rgba[3] = a;
    }
}

void Decals::setTexture(uint32_t id, bgfx::TextureHandle newTex, bool release) {
    DecalEntry *entry = nullptr;
    if (tryGet(id, &entry)) {
        entry->tex = newTex;
        entry->releaseTex = release;
    }
}

void Decals::setRange(uint32_t id, uint16_t range, bool fade, float sizeRatio) {
    DecalEntry *entry = nullptr;
    if (tryGet(id, &entry)) {
        entry->range = range;
        entry->fade = fade;
        entry->sizeRatio = sizeRatio;
    }
}

void Decals::reset(Scene *scene) {
    weakptr_release(_scene);
    _scene = scene_get_and_retain_weakptr(scene);

    removeAll();
}

void Decals::removeAll() {
    std::unordered_map<uint32_t, DecalEntry>::iterator itr;
    for (itr = _decalEntries.begin(); itr != _decalEntries.end(); ++itr) {
        release(&itr->second);
    }
    _decalEntries.clear();
}

void Decals::remove(uint32_t id) {
    DecalEntry *entry = nullptr;
    if (tryGet(id, &entry)) {
        release(entry);
        _decalEntries.erase(id);
    }
}

bool Decals::draw(bgfx::ViewId id, uint32_t depth) {
    bool drawn = false;
    std::unordered_map<uint32_t, DecalEntry>::iterator itr;
    for (itr = _decalEntries.begin(); itr != _decalEntries.end(); ++itr) {
        // the DecalEntry IDs are a good way to ensure (any) unique sort order
        drawn = draw(id, depth + itr->first, &itr->second) || drawn;
    }
    return drawn;
}

#if DEBUG_RENDERER_DECALS_STATS
uint32_t Decals::_debug_getRequests() {
    return _debug_requests;
}

uint32_t Decals::_debug_getProjections() {
    return _debug_projections;
}

void Decals::_debug_reset() {
    _debug_requests = 0;
    _debug_projections = 0;
}
#endif

void Decals::release(DecalEntry *entry) {
    if (bgfx::isValid(entry->tex) && entry->releaseTex) {
        bgfx::destroy(entry->tex);
    }
    if (bgfx::isValid(entry->dvb)) {
        bgfx::destroy(entry->dvb);
    }
    weakptr_release(entry->filterOut);
}

bool Decals::draw(bgfx::ViewId id, uint32_t depth, DecalEntry *entry) {
    // Compute projection if necessary
    if (entry->count == 0) {
        computeProjection(entry, &entry->count);
    }

    // Skip empty projection
    if (entry->count == 0) {
        return false;
    }

    // Set decal texture
    if (bgfx::isValid(entry->tex)) {
        bgfx::setTexture(0, *_sFb1, entry->tex);
    } else {
        return false;
    }

    // Unused by decals (9-slice parameters)
    float paramsData[8];
    for(uint8_t i = 0; i < 6; ++i) {
        paramsData[i] = 0.5f;
    }
    paramsData[6] = 0.0f;
    paramsData[7] = 0.0f;
    bgfx::setUniform(*_uParams, paramsData);
    bgfx::setUniform(*_uColor1, paramsData + 4);

    // Set vertex buffer for decal vertices
    bgfx::setVertexBuffer(0, entry->dvb, 0, static_cast<uint32_t>(entry->count * VB_VERTICES));

    // Set index buffer from a sub-range of the full static buffer
    bgfx::setIndexBuffer(_voxelIbh, 0, static_cast<uint32_t>(entry->count * IB_INDICES));

    bgfx::setTransform(entry->mtx);
    bgfx::setStencil(entry->stencil);
    if (_features->useDeferred()) {
        bgfx::setState(GameRenderer::getRenderState(0, RenderPassState_AlphaWeight));
    } else {
        bgfx::setState(GameRenderer::getRenderState(0, RenderPassState_AlphaBlending));
    }

    bgfx::submit(id, *_program, depth);

    return true;
}

bool Decals::tryGet(uint32_t id, DecalEntry **out) {
    std::unordered_map<uint32_t, DecalEntry>::iterator itr = _decalEntries.find(id);
    bool exists = itr != _decalEntries.end();
    if (exists) {
        *out = &itr->second;
    }
    return exists && *out != nullptr;
}

Decals::DecalEntry Decals::createDecalEntry(float3 eye, float width, float height, uint32_t stencil,
                                            Transform *filterOut, Projection proj, UpdateMode mode) {

    DecalEntry entry;
    setProjection(&entry, eye, width, height, proj);
    entry.mode = mode;
    entry.filterOut = transform_get_and_retain_weakptr(filterOut);
    entry.rgba[0] = entry.rgba[1] = entry.rgba[2] = entry.rgba[3] = 1.0f;
    entry.tex = BGFX_INVALID_HANDLE;
    entry.releaseTex = false;
    entry.dvb = BGFX_INVALID_HANDLE;
    entry.count = 0;
    entry.range = 0;
    entry.fade = false;
    entry.sizeRatio = 1;
    entry.stencil = stencil;

    return entry;
}

void Decals::setProjection(DecalEntry *entry, float3 eye, float width, float height, Projection proj) {
    entry->eye = eye;
    entry->width = width;
    entry->height = height;
    entry->proj = proj;
}

bool Decals::projectionEquals(DecalEntry *entry, float3 eye, float width, float height, Projection proj) {
    return entry->proj == proj
           && float_isEqual(entry->eye.x, eye.x, EPSILON_ZERO)
           && float_isEqual(entry->eye.y, eye.y, EPSILON_ZERO)
           && float_isEqual(entry->eye.z, eye.z, EPSILON_ZERO)
           && float_isEqual(entry->width, width, EPSILON_ZERO)
           && float_isEqual(entry->height, height, EPSILON_ZERO);
}

void Decals::computeProjection(DecalEntry *entry, size_t *count) {
    // TODO: evaluate if this system will stay relevant, remove it or upgrade it
    // TODO: this could use renderer scene structure when available
    // TODO: this only implements Downward projection, implement all projections

    Scene *sc = static_cast<Scene*>(weakptr_get(_scene));
    if (sc == nullptr) {
        *count = 0;
        return;
    }

    Transform *filterOut = static_cast<Transform*>(weakptr_get(entry->filterOut));
    DoublyLinkedList *filterOutTransforms = nullptr;
    if (filterOut != nullptr) {
        filterOutTransforms = doubly_linked_list_new();
        doubly_linked_list_push_last(filterOutTransforms, filterOut);
    }

    Ray *ray = ray_new(&entry->eye, &float3_down);
    CastResult result; scene_cast_ray(sc, ray, PHYSICS_GROUP_ALL_SYSTEM, filterOutTransforms, &result);
    ray_free(ray);

    if (filterOutTransforms != nullptr) {
        doubly_linked_list_free(filterOutTransforms);
    }

    if (result.type == Hit_None) {
        *count = 0;
        return;
    }

    Shape *shape = transform_utils_get_shape(result.hitTr);
    if (shape == nullptr) {
        *count = 0;
        return;
    }

    Matrix4x4 model, invModel;
    transform_utils_get_model_ltw(result.hitTr, &model);
    transform_utils_get_model_wtl(result.hitTr, &invModel);

    SHAPE_SIZE_INT3_T dim = shape_get_allocated_size(shape);
    float3 lossyScale; matrix4x4_get_scaleXYZ(&model, &lossyScale);
    size_t flat, written = 0, processed = 0;

    // we project quads for maximum distance size
    const float maxWidth = entry->sizeRatio * entry->width / lossyScale.x;
    const float maxHeight = entry->sizeRatio * entry->height / lossyScale.z;

    // cancel projection if size parameters were <= 0
    if (maxWidth <= 0 || maxHeight <= 0) {
        *count = 0;
        return;
    }

    // decal origin world to model
    const float wOffset = maxWidth / 2;
    const float hOffset = maxHeight / 2;
    float3 origin; matrix4x4_op_multiply_vec_point(&origin, &entry->eye, &invModel);
    switch(entry->proj) {
        case Projection::Downward:
            origin = { origin.x - wOffset, origin.y + EPSILON_COLLISION, origin.z - hOffset };
            break;
        case Projection::Upward:
            origin = { origin.x - wOffset, origin.y - EPSILON_COLLISION, origin.z - hOffset };
            break;
        case Projection::Backward:
            origin = { origin.x - wOffset, origin.y - hOffset, origin.z + EPSILON_COLLISION };
            break;
        case Projection::Forward:
            origin = { origin.x - wOffset, origin.y - hOffset, origin.z - EPSILON_COLLISION };
            break;
        case Projection::Right:
            origin = { origin.x - EPSILON_COLLISION, origin.y - hOffset, origin.z - wOffset };
            break;
        case Projection::Left:
            origin = { origin.x + EPSILON_COLLISION, origin.y - hOffset, origin.z - wOffset };
            break;
    }

    // project downward starting from 1 level under the eye's level
    size_t ox = static_cast<size_t>(maximum(floor(origin.x), 0));
    size_t oy = static_cast<size_t>(maximum(floor(origin.y - 1), 0));
    size_t oz = static_cast<size_t>(maximum(floor(origin.z), 0));

    // cancel projection if origin is beyond shape size
    if (ox >= dim.x || oy >= dim.y || oz >= dim.z) {
        *count = 0;
        return;
    }

    // use real origin to predict number of quads
    size_t widthQuads = static_cast<size_t>(CLAMP(ceil(origin.x + maxWidth), 0, dim.x - 1) - ox);
    size_t heightQuads = static_cast<size_t>(CLAMP(ceil(origin.z + maxHeight), 0, dim.z - 1) - oz);

    // cancel projection if quads are outside shape
    if (widthQuads == 0 || heightQuads == 0) {
        *count = 0;
        return;
    }

    size_t maxDepth = entry->range == 0 ? oy : minimum(oy, entry->range - 1);
    float eyeDelta = origin.y - 1 - oy;

    // allocate vertex buffer data
    size_t quadsCount = widthQuads * heightQuads;
    uint32_t maxSize = quadsCount * VB_VERTICES * sizeof(QuadVertex);
    const bgfx::Memory *mem = bgfx::alloc(maxSize);
    QuadVertex *vertices = (QuadVertex *)mem->data;

    // allocate projection flags
    bool *flags = static_cast<bool *>(malloc(quadsCount));
    memset(flags, 0, quadsCount);

    Block *block;
    for (int y = oy, ly = 0; ly <= maxDepth && processed < quadsCount; y--, ly++) {
        for (int z = oz, lz = 0; lz < heightQuads; z++, lz++) {
            for (int x = ox, lx = 0; lx < widthQuads; x++, lx++) {
                flat = static_cast<size_t>(lz * widthQuads + lx);

                // early skip if quad was already projected or canceled
                if (flags[flat]) {
                    continue;
                }

                block = shape_get_block_immediate(shape, x, y, z);

                if (block_is_solid(block)) {
                    // account for both written and canceled quads
                    processed++;

                    // if stacked blocks, cancel projection for this quad altogether
                    block = shape_get_block_immediate(shape, x, y + 1, z);
                    if (block_is_solid(block)) {
                        flags[flat] = true;
                        continue;
                    }

                    // quad position
                    const float x0 = x;
                    const float x1 = x + 1;
                    const float z0 = z;
                    const float z1 = z + 1;
                    const float top = y + 1 + RENDERER_DECALS_OFFSET;

                    // fading and scaling (ie. s:tiling/t:offset) based on distance
                    const float d = (ly + eyeDelta) / (maxDepth + 1);
                    const float w = entry->fade ? 1.0f - d : 1.0f;
                    const float s = LERP(entry->sizeRatio, 1.0f, d);
                    const float t = (1 - s) / 2;

                    // quad UV
                    const float u0 = (x0 - origin.x) / maxWidth * s + t;
                    const float u1 = (x1 - origin.x) / maxWidth * s + t;
                    const float v0 = (z0 - origin.z) / maxHeight * s + t;
                    const float v1 = (z1 - origin.z) / maxHeight * s + t;

                    // faded color
                    const uint32_t rgba = utils_float_to_rgba(entry->rgba[0], entry->rgba[1], entry->rgba[2], entry->rgba[3] * w);

                    vertices[written * 4] =     { x0, top, z1, 0.0f, 0.0f, 0.0f, 0.0f, rgba, u0, v1 };
                    vertices[written * 4 + 1] = { x0, top, z0, 0.0f, 0.0f, 0.0f, 0.0f, rgba, u0, v0 };
                    vertices[written * 4 + 2] = { x1, top, z0, 0.0f, 0.0f, 0.0f, 0.0f, rgba, u1, v0 };
                    vertices[written * 4 + 3] = { x1, top, z1, 0.0f, 0.0f, 0.0f, 0.0f, rgba, u1, v1 };

                    flags[flat] = true;
                    written++;
                }
            }
        }
    }

    if (written != 0) {
        // get model matrix
        memcpy(entry->mtx, &model, 16 * sizeof(float));
        
        if (bgfx::isValid(entry->dvb)) {
            bgfx::update(entry->dvb, 0, mem);
        } else {
            entry->dvb = bgfx::createDynamicVertexBuffer(mem, QuadVertex::_quadVertexLayout,
                                                         BGFX_BUFFER_ALLOW_RESIZE);
        }
#if DEBUG_RENDERER_DECALS_STATS
        ++_debug_projections;
#endif
    } else {
        bgfx::release(mem);
    }
    *count = written;

    free(flags);
}

#else /* P3S_CLIENT_HEADLESS */

Decals::Decals() {}
Decals::~Decals() {}

#endif /* P3S_CLIENT_HEADLESS */
