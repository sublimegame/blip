// -------------------------------------------------------------
//  Cubzh
//  VXRDSky.cpp
//  Created by Arthur Cormerais on October 18, 2020.
// -------------------------------------------------------------

#include "VXRDSky.hpp"

using namespace vx;
using namespace rendering;

#ifndef P3S_CLIENT_HEADLESS

// vx
#include "vxlog.h"
#include "VXGameRenderer.hpp"

#include <cfloat>

// bgfx
// to access some helpers eg. release bgfx Memory
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Weverything"
#include "../bgfx/bgfx/src/bgfx_p.h"
#pragma GCC diagnostic pop

/// Vertex layout
bgfx::VertexLayout Sky::SkyVertex::_skyVertexLayout;

Sky::Sky(RendererFeatures *features, bgfx::ProgramHandle *cloudsProgram, bgfx::ProgramHandle *cloudsProgram_unlit,
         bgfx::ProgramHandle *cs_updateCloudsProgram, bgfx::ProgramHandle *cs_indirectCloudsProgram) {

    _instanceData = nullptr;
    _state = nullptr;
    _features = features;
    _cloudsProgram = cloudsProgram;
    _cloudsProgram_unlit = cloudsProgram_unlit;
    _cs_updateCloudsProgram = cs_updateCloudsProgram;
    _cs_indirectCloudsProgram = cs_indirectCloudsProgram;

    SkyVertex::init();
    setCubeData();
    setCompute();
    setDrawIndirect();
}

Sky::~Sky() {
    releaseCubeData();
    releaseClouds();
    releaseCompute();
    releaseDrawIndirect();
}

bool Sky::draw(bgfx::ViewId id, uint32_t depth, float offset) {
    if (_state == nullptr || _state->clouds == false) {
        return false;
    }

    // Initialize once, or if compute shaders available, dispatch clouds update every frame
    generateClouds(id, offset);

    // Instanced clouds
    if (_features->isInstancingEnabled()) {
        bgfx::ProgramHandle program = _state->unlit ? *_cloudsProgram_unlit : *_cloudsProgram;

        // Realtime instanced clouds, optionally use draw indirect if available
        if (_features->isComputeEnabled()) {
            if (_features->isDrawIndirectEnabled()) {
                bgfx::setBuffer(0, _cs_instanceCount, bgfx::Access::Read);
                bgfx::setBuffer(1, _indirectBuffer, bgfx::Access::Write);
                bgfx::dispatch(id, *_cs_indirectCloudsProgram);
            }

            bgfx::setVertexBuffer(0, _vbh);
            bgfx::setIndexBuffer(_ibh);
            bgfx::setInstanceDataBuffer(_cs_instanceBuffer, 0, RENDERER_SKY_COMPUTE_INSTANCES_MAX);

            bgfx::setState(BGFX_STATE_WRITE_RGB
                           | BGFX_STATE_WRITE_A
                           | BGFX_STATE_WRITE_Z
                           | BGFX_STATE_DEPTH_TEST_LESS
                           | BGFX_STATE_CULL_CW);

            if (_features->isDrawIndirectEnabled()) {
                bgfx::submit(id, program, _indirectBuffer, 0, 1, depth);
            } else {
                bgfx::submit(id, program, depth);
            }
        }
        // Static instanced clouds
        else {
            // Skip if instance data not created, or if no instance were generated
            if (_instanceData == nullptr || _instancesCount == 0) {
                return false;
            }

            // Skip if not enough buffer space
            if (bgfx::getAvailInstanceDataBuffer(_instancesCount, _instanceStride) < _instancesCount) {
                return false;
            }

            // Parameters used for scrolling static clouds
#if DEBUG_RENDERER_SKY_REALTIME_NO_CS
            _uStaticCloudsParamsData[0] = 0.0f;
#else
            _uStaticCloudsParamsData[0] = _state->speed * 2.0f;                  // speed
#endif
            _uStaticCloudsParamsData[1] = _state->depth + 2 * _state->outerEdge; // total depth (world units)
            _uStaticCloudsParamsData[2] = _state->outerEdge;                     // outer edge (world units)
            _uStaticCloudsParamsData[3] = _state->origin.x;                      // origin X (world units)

            _uStaticCloudsParamsData[4] = _state->origin.y;                      // altitude (world units)
            _uStaticCloudsParamsData[5] = _state->origin.z;                      // origin Z (world units)
            _uStaticCloudsParamsData[6] = _state->color.x;                       // base color (R)
            _uStaticCloudsParamsData[7] = _state->color.y;                       // base color (G)

            _uStaticCloudsParamsData[8] = _state->color.z;                       // base color (B)
            _uStaticCloudsParamsData[9] = 0.0f;
            _uStaticCloudsParamsData[10] = 0.0f;
            _uStaticCloudsParamsData[11] = 0.0f;
            bgfx::setUniform(_uStaticCloudsParams, _uStaticCloudsParamsData, 3);

            bgfx::InstanceDataBuffer idb;
            bgfx::allocInstanceDataBuffer(&idb, _instancesCount, _instanceStride);
            memcpy(idb.data, _instanceData, _instancesCount * _instanceStride);

            bgfx::setVertexBuffer(0, _vbh);
            bgfx::setIndexBuffer(_ibh);
            bgfx::setInstanceDataBuffer(&idb);

            bgfx::setState(BGFX_STATE_WRITE_RGB
                           | BGFX_STATE_WRITE_A
                           | BGFX_STATE_WRITE_Z
                           | BGFX_STATE_DEPTH_TEST_LESS
                           | BGFX_STATE_CULL_CW);
            bgfx::submit(id, program, depth);
        }
    } else {
        // TODO: non-instanced draw clouds fallback
        return false;
    }

    return true;
}

void Sky::reset(SkyState *state) {
    _state = state;

    if (_features->isComputeEnabled() == false) {
        releaseClouds();
    }
}

void Sky::setCubeData() {
    // The basic cloud element is a cube
    static Sky::SkyVertex cubeVertices[] = {
            {-1.0f,  1.0f,  1.0f },
            { 1.0f,  1.0f,  1.0f },
            {-1.0f, -1.0f,  1.0f },
            { 1.0f, -1.0f,  1.0f },
            {-1.0f,  1.0f, -1.0f },
            { 1.0f,  1.0f, -1.0f },
            {-1.0f, -1.0f, -1.0f },
            { 1.0f, -1.0f, -1.0f },
    };

    static const uint16_t cubeTriangles[] = {
            0, 1, 2, // 0
            1, 3, 2,
            4, 6, 5, // 2
            5, 6, 7,
            0, 2, 4, // 4
            4, 2, 6,
            1, 5, 3, // 6
            5, 7, 3,
            0, 4, 1, // 8
            4, 5, 1,
            2, 3, 6, // 10
            6, 3, 7,
    };

    _vbh = bgfx::createVertexBuffer(bgfx::makeRef(cubeVertices, sizeof(cubeVertices)), SkyVertex::_skyVertexLayout);
    _ibh = bgfx::createIndexBuffer(bgfx::makeRef(cubeTriangles, sizeof(cubeTriangles)));
}

void Sky::releaseCubeData() {
    bgfx::destroy(_vbh);
    bgfx::destroy(_ibh);
}

void Sky::setCompute() {
    if (_features->isInstancingEnabled() && _features->isComputeEnabled()) {
        bgfx::VertexLayout instanceVertexLayout, paramsVertexLayout;
        instanceVertexLayout.begin()
                .add(bgfx::Attrib::TexCoord0, 4, bgfx::AttribType::Float)
                .add(bgfx::Attrib::TexCoord1, 4, bgfx::AttribType::Float)
                .add(bgfx::Attrib::TexCoord2, 4, bgfx::AttribType::Float)
                .add(bgfx::Attrib::TexCoord3, 4, bgfx::AttribType::Float)
                .end();
        paramsVertexLayout.begin()
                .add(bgfx::Attrib::TexCoord0, 4, bgfx::AttribType::Float)
                .end();
    
        _cs_instanceBuffer = bgfx::createDynamicVertexBuffer(RENDERER_SKY_COMPUTE_INSTANCES_MAX, instanceVertexLayout, BGFX_BUFFER_COMPUTE_READ_WRITE);
        _cs_instanceCount = bgfx::createDynamicIndexBuffer(1, BGFX_BUFFER_INDEX32 | BGFX_BUFFER_COMPUTE_READ_WRITE);
        _cs_cloudsParams = bgfx::createDynamicVertexBuffer(5, paramsVertexLayout, BGFX_BUFFER_COMPUTE_READ);
        _uStaticCloudsParams = BGFX_INVALID_HANDLE;
    } else {
        _cs_instanceBuffer = BGFX_INVALID_HANDLE;
        _cs_instanceCount = BGFX_INVALID_HANDLE;
        _cs_cloudsParams = BGFX_INVALID_HANDLE;
        _uStaticCloudsParams = bgfx::createUniform("u_staticCloudsParams", bgfx::UniformType::Vec4, 3);
    }
}

void Sky::releaseCompute() {
    if (bgfx::isValid(_cs_instanceBuffer)) {
        bgfx::destroy(_cs_instanceBuffer);
    }
    if (bgfx::isValid(_cs_instanceCount)) {
        bgfx::destroy(_cs_instanceCount);
    }
    if (bgfx::isValid(_cs_cloudsParams)) {
        bgfx::destroy(_cs_cloudsParams);
    }
    if (bgfx::isValid(_uStaticCloudsParams)) {
        bgfx::destroy(_uStaticCloudsParams);
    }
}

void Sky::setDrawIndirect() {
    if (_features->isInstancingEnabled() && _features->isDrawIndirectEnabled()) {
        _indirectBuffer = bgfx::createIndirectBuffer(2);
    } else {
        _indirectBuffer = BGFX_INVALID_HANDLE;
    }
}

void Sky::releaseDrawIndirect() {
    if (bgfx::isValid(_indirectBuffer)) {
        bgfx::destroy(_indirectBuffer);
    }
}

void Sky::generateClouds(bgfx::ViewId id, float offset) {
    // No realtime clouds if compute shaders are not available, or if no instancing (not implemented)
    if (_features->isInstancingEnabled() == false || (_features->isComputeEnabled() == false && _instanceData != nullptr)) {
#if DEBUG_RENDERER_SKY_REALTIME_NO_CS == false
        return;
#endif
    }

    float iptr;

    // Sample dimensions, based on spread
    const float width_sample = _state->width / _state->spread;
    const float depth_sample = _state->depth / _state->spread;
    const float innerEdge_sample = _state->innerEdge / _state->spread;
    const float outerEdge_sample = _state->outerEdge / _state->spread;

    // Period for range values
    const float p = 0.5f + 0.5f * sin(2.0f * PI_F * modf(offset / maximum(_state->period, 1.0f), &iptr));

    // Noise magnitude
    const float m = LERP(_state->magnitude.min, _state->magnitude.max, p);

    // Noise cutout
    const float c = LERP(_state->cutout.min, _state->cutout.max, p);

    if (_features->isInstancingEnabled()) {
        if (_features->isComputeEnabled()) {
            _cs_cloudsParamsData[0] = RENDERER_SKY_COMPUTE_INSTANCES_MAX;      // max invocations
            _cs_cloudsParamsData[1] = _state->origin.x;                        // origin X (world units)
            _cs_cloudsParamsData[2] = _state->origin.y;                        // altitude (world units)
            _cs_cloudsParamsData[3] = _state->origin.z;                        // origin Z (world units)

            _cs_cloudsParamsData[4] = width_sample;                            // width (sample units)
            _cs_cloudsParamsData[5] = depth_sample;                            // depth (sample units)
            _cs_cloudsParamsData[6] = innerEdge_sample;                        // inner edge (sample units)
            _cs_cloudsParamsData[7] = outerEdge_sample;                        // outer edge (sample units)

            _cs_cloudsParamsData[8] = p;                                       // period
            _cs_cloudsParamsData[9] = m;                                       // magnitude
            _cs_cloudsParamsData[10] = c;                                      // cutout
            _cs_cloudsParamsData[11] = _state->speed * offset;                 // offset

            _cs_cloudsParamsData[12] = _state->frequencyX;                     // X frequency
            _cs_cloudsParamsData[13] = _state->frequencyZ;                     // Z frequency
            _cs_cloudsParamsData[14] = _state->baseScale;                      // base clouds scale
            _cs_cloudsParamsData[15] = _state->maxAddScale;                    // max clouds add scale

            _cs_cloudsParamsData[16] = _state->color.x;                        // base color (R)
            _cs_cloudsParamsData[17] = _state->color.y;                        // base color (G)
            _cs_cloudsParamsData[18] = _state->color.z;                        // base color (B)
            _cs_cloudsParamsData[19] = _state->spread;                         // spread

            const bgfx::Memory* mem = bgfx::makeRef(&_cs_cloudsParamsData, 20 * sizeof(float));
            bgfx::update(_cs_cloudsParams, 0, mem);

            bgfx::setBuffer(0, _cs_instanceBuffer, bgfx::Access::Write);
            bgfx::setBuffer(1, _cs_instanceCount, bgfx::Access::ReadWrite);
            bgfx::setBuffer(2, _cs_cloudsParams, bgfx::Access::Read);

            bgfx::dispatch(id, *_cs_updateCloudsProgram, RENDERER_DISPATCH_SIZE, 1, 1);
        } else {
            if (_instanceData == nullptr) {
                _instanceData = (uint8_t *) malloc(RENDERER_SKY_INSTANCES_MAX * _instanceStride);
            }
            _instancesCount = 0;

            const int from = -static_cast<int>(outerEdge_sample);
            const int toWidth = static_cast<int>(width_sample + outerEdge_sample);
            const int toDepth = static_cast<int>(depth_sample + outerEdge_sample);
            const int innerWidth = static_cast<int>(width_sample - innerEdge_sample);
            const int innerDepth = static_cast<int>(depth_sample - innerEdge_sample);
            const float edgeLength = innerEdge_sample + outerEdge_sample;

            float n, scale, d;
            uint8_t *data = _instanceData;
            for (int x = from; x < toWidth; x++) {
                for (int z = from; z < toDepth; z++) {
                    if (_instancesCount == RENDERER_SKY_INSTANCES_MAX) {
                        break;
                    }

                    // Sample noise on the grid at provided frequencies
                    n = CLAMP(m * snoise(_state->frequencyX * float(x), _state->frequencyZ * (float(z) + _state->speed * offset)), -1.0f, 1.0f);

                    // Cutout everything outside of the threshold
                    if (n >= c) {
                        // Inverse distance ratio to the outer edges ie. 1.0 = in bounds, 0.0 = outer edge
                        d = edgeLength == 0 ? 1.0f : 1.0f - minimum(x < _state->innerEdge ? _state->innerEdge - x : maximum(x - innerWidth, 0),
                                                                    z < _state->innerEdge ? _state->innerEdge - z : maximum(z - innerDepth, 0)) / edgeLength;

                        // Instance scale variation based on:
                        // - noise value diff with cutout threshold
                        // - proximity to the outer edge
                        scale = (_state->baseScale + _state->maxAddScale * (n - c) / (1.0f - c)) * d;

                        // Add epsilon if result is near an integer value to avoid z-fighting
                        if (modf(scale, &iptr) < RENDERER_SKY_EPSILON) {
                            scale += RENDERER_SKY_EPSILON;
                        }

                        // Write instance transform
                        float* mtx = (float*)data;
                        bx::mtxScale(mtx, scale);
                        mtx[12] = x * _state->spread;
                        mtx[13] = 0.0f;
                        mtx[14] = z * _state->spread;

                        // Write instance color in the unused projection values
                        mtx[3] = _state->color.x;
                        mtx[7] = _state->color.y;
                        mtx[11] = _state->color.z;

                        data += _instanceStride;
                        _instancesCount++;
                    }
                }
            }
    #if DEBUG_RENDERER_SKY_LOG
          vxlog_debug("☁  Clouds generated w/ %d instances, noise range %f / %f", _instancesCount, min, max);
    #endif
        }
    } else {
        // TODO: non-instanced generate clouds fallback
        _instanceData = nullptr;
    }
}

void Sky::releaseClouds() {
    if (_instanceData != nullptr) {
        free(_instanceData);
        _instanceData = nullptr;
    }
}

float _dot2(float x1, float y1, float x2, float y2) {
    return x1 * x2 + y1 * y2;
}

float _dot3(float x1, float y1, float z1, float x2, float y2, float z2) {
    return x1 * x2 + y1 * y2 + z1 * z2;
}

float _mod289(float f) {
    return f - floorf(f * (1.0f / 289.0f)) * 289.0f;
}

float _permute(float f) {
    return _mod289((f * 34.0f + 1.0f) * f);
}

//// 2D simplex noise used to randomly distribute & scale clouds in an interesting manner
/// Base GLES implementation from Ian McEwan, Ashima Arts, see: https://github.com/ashima/webgl-noise
float Sky::snoise(float x, float y) {
    // Precompute values for skewed triangular grid
    const float Cx = .211324865405187f;
    const float Cy = .366025403784439f;
    const float Cz = -.577350269189626f;
    const float Cw = .024390243902439f;

    // First corner (x0)
    float xyDotCyy = _dot2(x, y, Cy, Cy);
    float ix = floorf(x + xyDotCyy);
    float iy = floorf(y + xyDotCyy);
    float ixyDotCxx = _dot2(ix, iy, Cx, Cx);
    float x0x = x - ix + ixyDotCxx;
    float x0y = y - iy + ixyDotCxx;

    // Other two corners (x1, x2)
    float i1x = (x0x > x0y ? 1.0f : 0.0f);
    float i1y = (x0x > x0y ? 0.0f : 1.0f);
    float x1x = x0x + Cx - i1x;
    float x1y = x0y + Cx - i1y;
    float x2x = x0x + Cz;
    float x2y = x0y + Cz;

    // Permutations
    ix = _mod289(ix); // Avoid truncation effects in permutation
    iy = _mod289(iy);
    float px = _permute(_permute(iy) + ix);
    float py = _permute(_permute(iy + i1y) + ix + i1x);
    float pz = _permute(_permute(iy + 1.0f) + ix + 1.0f);
    float mx = maximum(.5f - _dot2(x0x, x0y, x0x, x0y), 0.0f);
    float my = maximum(.5f - _dot2(x1x, x1y, x1x, x1y), 0.0f);
    float mz = maximum(.5f - _dot2(x2x, x2y, x2x, x2y), 0.0f);

    mx = mx * mx;
    my = my * my;
    mz = mz * mz;
    mx = mx * mx;
    my = my * my;
    mz = mz * mz;

    // Gradients
    float iptr;
    float xx = 2.0f * modf(px * Cw, &iptr) - 1.0f;
    float xy = 2.0f * modf(py * Cw, &iptr) - 1.0f;
    float xz = 2.0f * modf(pz * Cw, &iptr) - 1.0f;
    float hx = abs(xx) - .5f;
    float hy = abs(xy) - .5f;
    float hz = abs(xz) - .5f;
    float a0x = xx - floorf(xx + .5f);
    float a0y = xy - floorf(xy + .5f);
    float a0z = xz - floorf(xz + .5f);

    // Normalise gradients
    mx *= 1.79284291400159f - 0.85373472095314f * (a0x * a0x + hx * hx);
    my *= 1.79284291400159f - 0.85373472095314f * (a0y * a0y + hy * hy);
    mz *= 1.79284291400159f - 0.85373472095314f * (a0z * a0z + hz * hz);

    // Compute final noise value at P
    float gx = a0x * x0x + hx * x0y;
    float gy = a0y * x1x + hy * x1y;
    float gz = a0z * x2x + hz * x2y;
    float final = 130.0f * _dot3(mx, my, mz, gx, gy, gz);

    return final;
}

#else /* P3S_CLIENT_HEADLESS */

Sky::Sky() {}
Sky::~Sky() {}

#endif /* P3S_CLIENT_HEADLESS */
