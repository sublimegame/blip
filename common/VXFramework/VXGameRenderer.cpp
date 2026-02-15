// -------------------------------------------------------------
//  Cubzh
//  VXGameRenderer.cpp
//  Created by Arthur Cormerais on March 20, 2020.
// -------------------------------------------------------------

#include "VXGameRenderer.hpp"

// C++
#include <cmath>
#include <utility>

#include "GameCoordinator.hpp"
#include "OperationQueue.hpp"
#include "filesystem.hpp"
#include "vxlog.h"
#include "VXApplication.hpp"
#include "serialization.h"
#include "colors.h"
#include "color_atlas.h"
#include "doubly_linked_list.h"
#include "scene.h"
#include "utils.h"
#include "weakptr.h"
#include "texture.h"
#ifndef P3S_CLIENT_HEADLESS
#include "audioSource.hpp"
#include "audioListener.hpp"
#endif

#ifndef P3S_CLIENT_HEADLESS

// to access some helpers eg. texture formats name
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Weverything"
// bgfx
#include "bx/allocator.h"
#include "../bgfx/bimg/3rdparty/stb/stb_image.h"
#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include "../bgfx/bimg/3rdparty/stb/stb_image_resize.h"
#include "../bgfx/bgfx/src/bgfx_p.h"
#pragma GCC diagnostic pop
#endif

using namespace vx;
using namespace rendering;

#define RENDERERSTATE_NONE      0
#define RENDERERSTATE_FOG       1   // fog state is dirty
#define RENDERERSTATE_CLOUDS    2   // clouds particle system is dirty
#define RENDERERSTATE_SKYBOX    4   // skybox cubemap(s) is dirty
#define RENDERERSTATE_ALL       7

#ifndef P3S_CLIENT_HEADLESS

#define ENSURE_VALID(h) if (bgfx::isValid(h) == false) { return false; }
#define ENSURE_VALID_N(h, n, ...) \
    if (bgfx::isValid(h) == false) { \
        if (n > 0) { releaseTex2Ds(n, __VA_ARGS__); }\
        return false;\
    }
#define ENSURE_VALID_F(h, f) if (bgfx::isValid(h) == false) { f(); return false; }

/// Layout for vertex color vertex buffer, used for debugging eg. bounding box in wireframe
struct PosColorVertex {
    float m_x;
    float m_y;
    float m_z;
    uint32_t m_abgr;

    static void init() {
        _vertexColorLayout
        .begin()
        .add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
        .add(bgfx::Attrib::Color0,   4, bgfx::AttribType::Uint8, true)
        .end();
    }

    static bgfx::VertexLayout _vertexColorLayout;
};

/// Static cube data
static PosColorVertex s_cubeVertices[] = {
    {0.0f,  1.0f,  1.0f, 0xff000000 },
    { 1.0f,  1.0f,  1.0f, 0xff0000ff },
    {0.0f, 0.0f,  1.0f, 0xff00ff00 },
    { 1.0f, 0.0f,  1.0f, 0xff00ffff },
    {0.0f,  1.0f, 0.0f, 0xffff0000 },
    { 1.0f,  1.0f, 0.0f, 0xffff00ff },
    {0.0f, 0.0f, 0.0f, 0xffffff00 },
    { 1.0f, 0.0f, 0.0f, 0xffffffff },
};

//static const uint16_t s_cubeTriList[] = {
//    0, 1, 2, // 0
//    1, 3, 2,
//    4, 6, 5, // 2
//    5, 6, 7,
//    0, 2, 4, // 4
//    4, 2, 6,
//    1, 5, 3, // 6
//    5, 7, 3,
//    0, 4, 1, // 8
//    4, 5, 1,
//    2, 3, 6, // 10
//    6, 3, 7,
//};

static const uint16_t s_cubeLineList[] = {
    0, 1,
    0, 2,
    0, 4,
    1, 3,
    1, 5,
    2, 3,
    2, 6,
    3, 7,
    4, 5,
    4, 6,
    5, 7,
    6, 7,
};

static const uint64_t s_ptState[] {
    UINT64_C(0),
    BGFX_STATE_PT_TRISTRIP,
    BGFX_STATE_PT_LINES,
    BGFX_STATE_PT_LINESTRIP,
    BGFX_STATE_PT_POINTS,
};

static bgfx::VertexBufferHandle _cubeVbh;
static bgfx::IndexBufferHandle _cubeIbh;

static PosColorVertex *s_sphereVertices = nullptr;
static uint16_t *s_sphereTriList = nullptr;
static bgfx::VertexBufferHandle _sphereVbh;
static bgfx::IndexBufferHandle _sphereIbh;

/// Vertex layouts
bgfx::VertexLayout PosColorVertex::_vertexColorLayout;
bgfx::VertexLayout ShapeVertex::_shapeVertexLayout;
bgfx::VertexLayout ScreenQuadVertex::_screenquadVertexLayout;
bgfx::VertexLayout QuadVertex::_quadVertexLayout;
bgfx::VertexLayout MeshVertex::_meshVertexLayout;

GameRenderer *GameRenderer::_instance = nullptr;

GameRenderer *GameRenderer::newGameRenderer(QualityTier tier, uint16_t width, uint16_t height, float devicePPP) {
    return new GameRenderer(tier, width, height, devicePPP);
}

GameRenderer *GameRenderer::getGameRenderer() {
    return _instance;
}

GameRenderer::GameRenderer(QualityTier tier, uint16_t width, uint16_t height, float devicePPP) {
    _instance = this;
    _gameFadingIn = nullptr;
    _game = nullptr;
    _gameState = nullptr;
    _time = 0.0f;
    _currentMapShape = nullptr;
    _bxAllocator = nullptr;
    _depthStencilBuffer = BGFX_INVALID_HANDLE;
    _depthStencilBlit = BGFX_INVALID_HANDLE;
    for (int i = 0; i < 7; ++i) {
        _opaqueFbTex[i] = BGFX_INVALID_HANDLE;
    }
    _opaqueFbh = BGFX_INVALID_HANDLE;
    _lightFbTex[0] = BGFX_INVALID_HANDLE;
    _lightFbTex[1] = BGFX_INVALID_HANDLE;
    _lightFbh = BGFX_INVALID_HANDLE;
    for (int i = 0; i < SHADOWMAP_CASCADES_MAX; ++i) {
        _shadowmapTh[i] = BGFX_INVALID_HANDLE;
        _shadowFbh[i] = BGFX_INVALID_HANDLE;
    }
    _transparencyFbTex[0] = BGFX_INVALID_HANDLE;
    _transparencyFbTex[1] = BGFX_INVALID_HANDLE;
    _transparencyFbTex[2] = BGFX_INVALID_HANDLE;
    _transparencyFbh = BGFX_INVALID_HANDLE;
    _targetsFbTex = BGFX_INVALID_HANDLE;
    _targetsFbh = BGFX_INVALID_HANDLE;
    _afterPostTargetsFbTex = BGFX_INVALID_HANDLE;
    _afterPostTargetsFbh = BGFX_INVALID_HANDLE;
    _systemTargetsFbTex = BGFX_INVALID_HANDLE;
    _systemTargetsFbh = BGFX_INVALID_HANDLE;
    _postprocessFbTex = BGFX_INVALID_HANDLE;
    _postprocessFbh = BGFX_INVALID_HANDLE;
    _toggled = false;
    _errored = false;
    _viewsDirty = true;
    _shadowmapsDirty = false;
    _programsDirty = false;
    _blockerInOut = false;
    _blockerTimer = 0.0f;
    _blockerDuration = -1.0f;
    _blockerGraceFrames = 0;
    _blitAlpha = false;
    _overlayDrawn = false;
    _usePBR = false;
    _dummyTh = BGFX_INVALID_HANDLE;
    _maxTarget = 0;
    _maxView = VIEW_SKYBOX;
    _faceShadingFactor = 1.0f;
    _quadsBatch = { nullptr, 0, 0, 0, BGFX_INVALID_HANDLE, BGFX_INVALID_HANDLE };
    _quadsBatchAllMaxCount = _quadsBatchTexturedMaxCount = 0;
#if DEBUG_RENDERER
    _debug_shadowBias = -1.0f;
    _debug_shadowOffset = -1.0f;
#endif

    _qualityParams.fromPreset(tier);
    _qualityPreset = tier;

    memset(_debug_fps_ma, 0, DEBUG_RENDERER_FPS_MOVING_AVERAGE * sizeof(int));
    _debug_fps_ma_cursor = 0;

    ShapeVertex::init();
    ScreenQuadVertex::init();
    QuadVertex::init();
    MeshVertex::init();

    // checking capabilities to select supported program variants
    checkAndSetRendererFeatures();
    if (isFallbackEnabled()) {
        _qualityParams.fromPreset(QualityTier_Compatibility);
        _qualityPreset = QualityTier_Compatibility;
    }

    setUniforms();
    setPrograms();

    setPrimitivesStaticData();
    setStaticResources();
    setSubRenderers();

    setStartupViews(width, height);
    refreshViews(width, height, devicePPP);

    vxlog_trace("🖌️ game renderer created");
}

GameRenderer::~GameRenderer() {
    stop();

    // We must wait previous frame to finish before we can free main thread allocs used as MemoryRef
    bgfx::frame();

    releasePrimitivesStaticData();
    releaseStaticResources();
    releaseSubRenderers();

    releaseUniforms();
    releasePrograms();
    releaseFramebuffers();

    delete _bxAllocator;

    for (auto itr = _geometryEntries.begin(); itr != _geometryEntries.end(); itr++) {
        doubly_linked_list_flush(itr->second, releaseGeometryEntry);
        doubly_linked_list_free(itr->second);
    }

    if (_quadsBatch.vertices != nullptr) {
        free(_quadsBatch.vertices);
    }
    if (bgfx::isValid(_quadsBatch.vb)) {
        bgfx::destroy(_quadsBatch.vb);
    }

    _instance = nullptr;

    vxlog_trace("🖌️ game renderer destroyed");
}

void GameRenderer::memoryRefRelease(void* ptr, void* data) {
    if (ptr != nullptr) {
        free(ptr);
    }
}

void GameRenderer::setTemporaryCameraProj(Game *g) {
    if (_instance == nullptr) return;
    
    const CGame *cgame = g->getCGame();
    const Camera *c = game_get_camera(cgame);
    const bgfx::Caps *caps = bgfx::getCaps();

    const float w = camera_get_width(c) > 0 ? camera_get_width(c) : _instance->_screenWidth;
    const float h = camera_get_height(c) > 0 ? camera_get_height(c) : _instance->_screenHeight;
    const float aspect = w / h;
    
    float proj[16], viewProj[16];
    
    bx::mtxProj(proj,
                utils_rad2Deg(camera_utils_get_vertical_fov(c, aspect)),
                aspect,
                camera_get_near(c),
                camera_get_far(c),
                caps->homogeneousDepth);
    bx::mtxMul(viewProj, reinterpret_cast<const float *>(camera_get_view_matrix(c)), proj);
    
    camera_set_proj_matrix(c, reinterpret_cast<Matrix4x4 *>(proj), reinterpret_cast<Matrix4x4 *>(viewProj));
}

uint64_t GameRenderer::getRenderState(int32_t stateIdx, RenderPassState pass, TriangleCull cull, PrimitiveType type, bool disableDepthTest) {
    uint64_t stateFlag = s_ptState[stateIdx];

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wold-style-cast"
    switch(pass) {
        case RenderPassState_Opacity:
            stateFlag = stateFlag | BGFX_STATE_WRITE_RGB
                        | BGFX_STATE_WRITE_A
                        | BGFX_STATE_WRITE_Z
                        | (disableDepthTest ? BGFX_STATE_NONE : BGFX_STATE_DEPTH_TEST_LESS);
            break;
        case RenderPassState_AlphaBlending:
            stateFlag = stateFlag | BGFX_STATE_WRITE_RGB
                        | BGFX_STATE_WRITE_A
                        | BGFX_STATE_BLEND_FUNC_SEPARATE(BGFX_STATE_BLEND_SRC_ALPHA, BGFX_STATE_BLEND_INV_SRC_ALPHA, BGFX_STATE_BLEND_ONE, BGFX_STATE_BLEND_INV_SRC_ALPHA)
                        | (disableDepthTest ? BGFX_STATE_NONE : BGFX_STATE_DEPTH_TEST_LESS);
            break;
        case RenderPassState_AlphaBlending_Premultiplied:
            stateFlag = stateFlag | BGFX_STATE_WRITE_RGB
                        | BGFX_STATE_WRITE_A
                        | BGFX_STATE_BLEND_FUNC(BGFX_STATE_BLEND_SRC_ALPHA, BGFX_STATE_BLEND_INV_SRC_ALPHA)
                        | (disableDepthTest ? BGFX_STATE_NONE : BGFX_STATE_DEPTH_TEST_LESS);
            break;
        case RenderPassState_Additive:
            stateFlag = stateFlag | BGFX_STATE_WRITE_RGB
                        | BGFX_STATE_WRITE_A
                        | BGFX_STATE_BLEND_FUNC(BGFX_STATE_BLEND_ONE, BGFX_STATE_BLEND_ONE)
                        | (disableDepthTest ? BGFX_STATE_NONE : BGFX_STATE_DEPTH_TEST_LESS);
            break;
        case RenderPassState_Multiplicative:
            stateFlag = stateFlag | BGFX_STATE_WRITE_RGB
                        | BGFX_STATE_WRITE_A
                        | BGFX_STATE_BLEND_FUNC(BGFX_STATE_BLEND_DST_COLOR, BGFX_STATE_BLEND_ZERO)
                        | (disableDepthTest ? BGFX_STATE_NONE : BGFX_STATE_DEPTH_TEST_LESS);
            break;
        case RenderPassState_Subtractive:
            stateFlag = stateFlag | BGFX_STATE_WRITE_RGB
                        | BGFX_STATE_WRITE_A
                        | BGFX_STATE_BLEND_EQUATION_SUB
                        | BGFX_STATE_BLEND_FUNC(BGFX_STATE_BLEND_ONE, BGFX_STATE_BLEND_ONE)
                        | (disableDepthTest ? BGFX_STATE_NONE : BGFX_STATE_DEPTH_TEST_LESS);
            break;
        case RenderPassState_AlphaWeight:
            stateFlag = stateFlag | BGFX_STATE_WRITE_RGB
                        | BGFX_STATE_WRITE_A
                        | BGFX_STATE_BLEND_FUNC_SEPARATE(BGFX_STATE_BLEND_ONE, BGFX_STATE_BLEND_ONE, BGFX_STATE_BLEND_ZERO, BGFX_STATE_BLEND_INV_SRC_ALPHA)
                        | (disableDepthTest ? BGFX_STATE_NONE : BGFX_STATE_DEPTH_TEST_LESS);
            break;
        case RenderPassState_Blit:
            stateFlag = stateFlag | BGFX_STATE_WRITE_RGB
                        | BGFX_STATE_WRITE_A;
            break;
        case RenderPassState_Layered:
            stateFlag = stateFlag | BGFX_STATE_WRITE_RGB
                        | BGFX_STATE_WRITE_A
                        | BGFX_STATE_BLEND_FUNC(BGFX_STATE_BLEND_ONE, BGFX_STATE_BLEND_INV_SRC_ALPHA);
            break;
        case RenderPassState_DepthPacking:
            stateFlag = stateFlag | BGFX_STATE_WRITE_RGB
                        | BGFX_STATE_WRITE_A
                        | BGFX_STATE_WRITE_Z
                        | BGFX_STATE_DEPTH_TEST_LESS;
            break;
        case RenderPassState_DepthSampling:
        case RenderPassState_DepthTesting:
            stateFlag = stateFlag | BGFX_STATE_WRITE_Z
                        | BGFX_STATE_DEPTH_TEST_LESS;
            break;
    }
    if (cull != TriangleCull_None) {
        stateFlag = stateFlag | (cull == TriangleCull_Back ? BGFX_STATE_CULL_CW : BGFX_STATE_CULL_CCW);
    }
    switch (type) {
        case PrimitiveType_Points:
            stateFlag = stateFlag | BGFX_STATE_PT_POINTS;
            break;
        case PrimitiveType_Lines:
            stateFlag = stateFlag | BGFX_STATE_PT_LINES;
            break;
        case PrimitiveType_LineStrip:
            stateFlag = stateFlag | BGFX_STATE_PT_LINESTRIP;
            break;
        case PrimitiveType_TriangleStrip:
            stateFlag = stateFlag | BGFX_STATE_PT_TRISTRIP;
            break;
        case PrimitiveType_Triangles:
        default:
            break;
    }
#pragma GCC diagnostic pop

    return stateFlag;
}

bool GameRenderer::hasTextureCapability(uint32_t count) {
    const bgfx::Stats* stats = bgfx::getStats();
    return stats->numTextures + count < BGFX_CONFIG_MAX_TEXTURES - RESERVED_TEXTURES;
}

bool GameRenderer::hasVBCapability(uint32_t count, bool dynamic) {
    const bgfx::Stats* stats = bgfx::getStats();
    if (dynamic) {
        return stats->numDynamicVertexBuffers + count < BGFX_CONFIG_MAX_DYNAMIC_VERTEX_BUFFERS - RESERVED_VERTEX_BUFFERS;
    } else {
        return stats->numVertexBuffers + count < BGFX_CONFIG_MAX_VERTEX_BUFFERS - RESERVED_VERTEX_BUFFERS;
    }
}

bool GameRenderer::hasIBCapability(uint32_t count, bool dynamic) {
    const bgfx::Stats* stats = bgfx::getStats();
    if (dynamic) {
        return stats->numDynamicIndexBuffers + count < BGFX_CONFIG_MAX_DYNAMIC_INDEX_BUFFERS - RESERVED_INDEX_BUFFERS;
    } else {
        return stats->numIndexBuffers + count < BGFX_CONFIG_MAX_INDEX_BUFFERS - RESERVED_INDEX_BUFFERS;
    }
}

Texture *GameRenderer::cubemapFacesToTexture(const void *faceData[6], size_t faceDataSize[6], bool filtering) {
    bx::DefaultAllocator *allocator = new bx::DefaultAllocator();
    const uint64_t samplerFlags = filtering ? BGFX_SAMPLER_NONE : BGFX_SAMPLER_MIN_POINT | BGFX_SAMPLER_MAG_POINT | BGFX_SAMPLER_MIP_POINT;

    // parse and harmonize formats (always BGRA8), find smallest dimensions among faces
    const bimg::TextureFormat::Enum format = bimg::TextureFormat::BGRA8;
    const uint8_t channels = 4;
    uint32_t minWidth = SKYBOX_TEX_MAX_SIZE;
    uint32_t minHeight = SKYBOX_TEX_MAX_SIZE;
    bimg::ImageContainer *faceImages[6] = { nullptr };
    bool hasMissingFace = false;
    for (int i = 0; i < 6; ++i) {
        if (faceData[i] != nullptr) {
            faceImages[i] = bimg::imageParse(allocator, faceData[i], faceDataSize[i]);
            if (faceImages[i] != nullptr) {
                minWidth = std::min(minWidth, faceImages[i]->m_width);
                minHeight = std::min(minHeight, faceImages[i]->m_height);

                if (faceImages[i]->m_format != format) {
                    bimg::ImageContainer* output = imageConvert(allocator, format, *faceImages[i]);
                    bimg::imageFree(faceImages[i]);
                    faceImages[i] = output;
                }
            }
            if (faceImages[i] == nullptr) {
                vxlog_warning("cubemapFacesToTexture: failed to parse cubemap face %d, skipping", i);
                hasMissingFace = true;
            }
        } else {
            hasMissingFace = true;
        }
    }

    const int expectedSize = minWidth * minHeight * channels;

    // downscale oversized faces
    for (int i = 0; i < 6; ++i) {
        if (faceImages[i] != nullptr && (faceImages[i]->m_width > minWidth || faceImages[i]->m_height > minHeight)) {
            unsigned char* resized = static_cast<unsigned char*>(malloc(expectedSize));
            const int result = stbir_resize_uint8(static_cast<const unsigned char*>(faceImages[i]->m_data),
                                                  faceImages[i]->m_width, faceImages[i]->m_height, 0,
                                                  resized, minWidth, minHeight, 0, channels);

            if (result == 0) {
                vxlog_warning("cubemapFacesToTexture: failed to resize cubemap face %d, skipping", i);
                bimg::imageFree(faceImages[i]);
                free(resized);
                faceImages[i] = nullptr;
                hasMissingFace = true;
            } else {
                bimg::ImageContainer* output = imageAlloc(allocator, format, minWidth, minHeight, 0, 1, false, false, resized);
                bimg::imageFree(faceImages[i]);
                faceImages[i] = output;
            }
        }
    }

    // fill missing faces with white
    bimg::ImageContainer* whiteImage = nullptr;
    if (hasMissingFace) {
        unsigned char* whiteData = static_cast<unsigned char*>(malloc(expectedSize));
        memset(whiteData, 255, expectedSize);

        whiteImage = imageAlloc(allocator, format, minWidth, minHeight, 0, 1, false, false, whiteData);

        for (int i = 0; i < 6; ++i) {
            if (faceImages[i] == nullptr) {
                faceImages[i] = whiteImage;
            }
        }
    }

    // create cubemap texture (parsed)
    Texture *tex = texture_new_cubemap_faces(
        faceImages[2], faceImages[3],  // top, bottom
        faceImages[4], faceImages[5],  // front, back
        faceImages[0], faceImages[1],  // right, left
        expectedSize, minWidth, minHeight,
        static_cast<uint8_t>(format)
    );
    texture_set_filtering(tex, filtering);

    for (int i = 0; i < 6; ++i) {
        if (faceImages[i] != nullptr && faceImages[i] != whiteImage) {
            bimg::imageFree(faceImages[i]);
        }
    }
    if (whiteImage != nullptr) {
        bimg::imageFree(whiteImage);
    }
    delete allocator;

    return tex;
}

void GameRenderer::tick(const double dt) {
    if (_errored) {
        return;
    }

    // Skip warmup frames
    if (dt == 0.0) { return; }

    // Automatically transition to latest active game
    Game_SharedPtr g = GameCoordinator::getInstance()->getActiveGame();
    if (g != nullptr) {
        if (_game != g && _gameFadingIn != g) {
            start(g);
        }
    } else if (_game != nullptr || _gameFadingIn != nullptr) {
        stop();
        _gameFadingIn = nullptr;
    }

    if (_toggled == false || _game == nullptr) {
        return;
    }

    float dtF = static_cast<float>(dt);
    _time += dtF;

    // Blocker fade in/out
    if (_blockerGraceFrames > 0) {
        _blockerGraceFrames--;
    } else if (_blockerTimer <= _blockerDuration) {
        _blockerTimer += dtF;
        if (_blockerTimer > _blockerDuration && _blockerCallback != nullptr) {
            _blockerCallback();
        }
    }

    _gif->tick(dt);
}

void GameRenderer::render(bgfx::FrameBufferHandle outFbh) {
    if (_toggled == false || _errored
        || _game == nullptr || _gameState == nullptr
        || _renderWidth == 0 || _renderHeight == 0) {

        clearViews();
        return;
    }

#if DEBUG_RENDERER_RELEASE_ALL
    releaseDrawEntries();
#else
    // Release draw entries created for engine's vertex buffers that were destroyed
    popShapeEntries();
#endif

    // Release inactive draw entries after grace frames are spent w/o being rendered
    checkAndPopInactiveShapeEntries();
    checkAndPopInactiveQuadEntries();
    checkAndPopInactiveMeshEntries();
    checkAndPopInactiveTextureEntries();

    // Release coders-requested resources that weren't used last frame
    clearShadowDecals();
    //clearCameraTargets();

    if (_programsDirty) {
        releasePrograms();
        setPrograms();
    }
    if ((_viewsDirty && refreshViewsInternal() == false) || (_shadowmapsDirty && createShadowmaps() == false)) {
        vxlog_error("⚠️⚠️⚠️renderer views allocation failed, a restart is advised");
        _errored = true;

        clearViews();
        return;
    }
    if (_gameState->isDirty()) {
        refreshStateInternal();
    }

    // Perform a scene recursion to collect render data
    preDrawCollectData();

    // After releasing resources, skip if no camera
    if (_cameras.empty()) {
        clearViews();
        return;
    }
    preparePipelineForCameras();

    // check for changes to the palette
    const uint32_t paletteSize = updateColorAtlas(_game->getCGame());

    // Day/night cycle
    refreshGlobalColors();

    // Set pipeline views, defined based on collected cameras
    bgfx::setViewRect(_postprocessView1, 0, 0, bgfx::BackbufferRatio::Equal);
    bgfx::setViewRect(_postprocessView2, 0, 0, bgfx::BackbufferRatio::Equal);
    bgfx::setViewRect(_afterpostView, 0, 0, bgfx::BackbufferRatio::Equal);
    bgfx::setViewRect(VIEW_UI_OVERLAY(_afterpostView), 0, 0, bgfx::BackbufferRatio::Equal);
    bgfx::setViewRect(VIEW_UI_BLOCKER(_afterpostView), 0, 0, bgfx::BackbufferRatio::Equal);
    bgfx::setViewRect(VIEW_UI_IMGUI(_afterpostView), 0, 0, bgfx::BackbufferRatio::Equal);
    bgfx::setViewRect(_systemView, 0, 0, bgfx::BackbufferRatio::Equal);
    bgfx::setViewClear(_postprocessView1, BGFX_CLEAR_NONE);
    bgfx::setViewClear(_postprocessView2, BGFX_CLEAR_NONE);
    bgfx::setViewClear(_afterpostView, BGFX_CLEAR_NONE);
    bgfx::setViewClear(VIEW_UI_OVERLAY(_afterpostView), BGFX_CLEAR_NONE);
    bgfx::setViewClear(VIEW_UI_BLOCKER(_afterpostView), BGFX_CLEAR_NONE);
    bgfx::setViewClear(VIEW_UI_IMGUI(_afterpostView), BGFX_CLEAR_NONE);
    bgfx::setViewClear(_systemView, BGFX_CLEAR_NONE);
#if DEBUG_RENDERER
    bgfx::setViewName(_postprocessView1, "Post-process 1");
    bgfx::setViewName(_postprocessView2, "Post-process 2");
    bgfx::setViewName(_afterpostView, "After Post");
    bgfx::setViewName(VIEW_UI_OVERLAY(_afterpostView), "UI Overlay");
    bgfx::setViewName(VIEW_UI_BLOCKER(_afterpostView), "UI Blocker");
    bgfx::setViewName(VIEW_UI_IMGUI(_afterpostView), "UI ImGui");
    bgfx::setViewName(_systemView, "System");
#endif

    // Set common views screen projection
    float screenProj[16];
    setViewTransform(VIEW_SKYBOX, nullptr, true, 1.0f, 1.0f, _features.NDCDepth, screenProj);
    bgfx::setViewTransform(_postprocessView1, nullptr, screenProj);
    bgfx::setViewTransform(_postprocessView2, nullptr, screenProj);
    bgfx::setViewTransform(_afterpostView, nullptr, screenProj);
    bgfx::setViewTransform(VIEW_UI_BLOCKER(_afterpostView), nullptr, screenProj);
    bgfx::setViewTransform(_systemView, nullptr, screenProj);

    // Overlay view uses points projection size to facilitate lua UI use-cases
    setViewTransform(VIEW_UI_OVERLAY(_afterpostView), nullptr, true, _pointsWidth, _pointsHeight, _features.NDCDepth);
    setViewTransform(VIEW_UI_IMGUI(_afterpostView), nullptr, true, _screenWidth, _screenHeight, _features.NDCDepth);

    bgfx::setViewMode(_postprocessView1, bgfx::ViewMode::Default);
    bgfx::setViewMode(_postprocessView2, bgfx::ViewMode::Default);
    bgfx::setViewMode(_afterpostView, bgfx::ViewMode::Default);
    bgfx::setViewMode(VIEW_UI_OVERLAY(_afterpostView), bgfx::ViewMode::DepthDescending);
    bgfx::setViewMode(VIEW_UI_BLOCKER(_afterpostView), bgfx::ViewMode::Default);
    bgfx::setViewMode(VIEW_UI_IMGUI(_afterpostView), bgfx::ViewMode::DepthDescending);
    bgfx::setViewMode(_systemView, bgfx::ViewMode::Default);

    // Final output to backbuffer or given framebuffer
    bgfx::setViewFrameBuffer(_postprocessView1, _postprocessFbh);
    bgfx::setViewFrameBuffer(_postprocessView2, outFbh);
    bgfx::setViewFrameBuffer(_afterpostView, outFbh);
    bgfx::setViewFrameBuffer(VIEW_UI_OVERLAY(_afterpostView), outFbh);
    bgfx::setViewFrameBuffer(VIEW_UI_BLOCKER(_afterpostView), BGFX_INVALID_HANDLE);
    bgfx::setViewFrameBuffer(VIEW_UI_IMGUI(_afterpostView), BGFX_INVALID_HANDLE);
    bgfx::setViewFrameBuffer(_systemView, BGFX_INVALID_HANDLE);

    // Cameras count is capped, not system cameras
    uint8_t totalCameraCount = 0, systemCameraCount = 0;
    #define INC_CAMERA ++totalCameraCount; if (isSystem) { ++systemCameraCount; }
    uint8_t cameraCount[PipelineSegment_COUNT] = { 0, 0, 0 };

    const bool deferred = _features.useDeferred();
    DoublyLinkedListNode *n = nullptr;
    CameraEntry *ce = nullptr;
    Plane *planes[6];
    std::unordered_map<GeometryType, DoublyLinkedList*> cameraGeom;
    for (auto it = _cameras.begin(); it != _cameras.end(); ++it) {
        n = doubly_linked_list_first(it->second);
        while (n != nullptr) {
            ce = static_cast<CameraEntry*>(doubly_linked_list_node_pointer(n));

            float targetWidth, targetHeight, targetX = 0.0f, targetY = 0.0f, fullWidth, fullHeight, uMax, vMax;
            _overlayDrawn = false;

            const bool isSystem = ce->segment == PipelineSegment_SystemTarget;
            const bool isAfterPost = ce->segment == PipelineSegment_AfterPostTarget;

            // List of geometry for current camera, early skip if no geometry to draw
            if (getGeometryForCamera(ce->camera, cameraGeom) == 0) {
                INC_CAMERA
                n = doubly_linked_list_node_next(n);
                continue;
            }

            // Game/lighting parameters are values common to all drawcalls
            setGameParams(camera_get_fov(ce->camera), camera_get_far(ce->camera), paletteSize);
            setLightingParams();

            // Projection dimensions may be overriden and depends on projection mode & pipeline segment,
            // TODO: right now, each segment has one shared buffer size ; if needed, how to improve this,
            //  more declarative lua options (camera.IsUI, camera.UsesPostProcess)?
            //  more flexibility in the multi-camera system (1 target/camera, post-process applied multiple times)?
            const ProjectionMode mode = camera_get_mode(ce->camera);
            const bool pointsScaling = mode == Orthographic && (isAfterPost || isSystem);
            float width, height;
            if (pointsScaling) {
                // ortho projection use points dimensions to facilitate Lua UI use-cases
                if (camera_uses_auto_projection_width(ce->camera)) {
                    camera_set_width(ce->camera, _pointsWidth, false);
                    width = _pointsWidth;
                } else {
                    width = camera_get_width(ce->camera);
                }
                if (camera_uses_auto_projection_height(ce->camera)) {
                    camera_set_height(ce->camera, _pointsHeight, false);
                    height = _pointsHeight;
                } else {
                    height = camera_get_height(ce->camera);
                }
            } else {
                // perspective projection use scaled resolution
                if (camera_uses_auto_projection_width(ce->camera)) {
                    camera_set_width(ce->camera, _renderWidth, false);
                    width = _renderWidth;
                } else {
                    width = camera_get_width(ce->camera) * _qualityParams.renderScale;
                }
                if (camera_uses_auto_projection_height(ce->camera)) {
                    camera_set_height(ce->camera, _renderHeight, false);
                    height = _renderHeight;
                } else {
                    height = camera_get_height(ce->camera) * _qualityParams.renderScale;
                }
            }
            const float aspect = width / height;
            const float fov = camera_utils_get_vertical_fov(ce->camera, aspect);

            // First camera in view order is the backdrop (always targets fullscreen)
            const bool isBackdrop = totalCameraCount == 0;
            if (isBackdrop) {
                targetWidth = mode == Orthographic && (isAfterPost || isSystem) ? _screenWidth : _renderWidth;
                targetHeight = mode == Orthographic && (isAfterPost || isSystem) ? _screenHeight : _renderHeight;
                uMax = targetWidth / _framebufferWidth;
                vMax = targetHeight / _framebufferHeight;

                bgfx::setViewRect(VIEW_SKYBOX, 0, 0, targetWidth, targetHeight);

                drawSkybox(VIEW_SKYBOX, ce->camera, targetWidth, targetHeight, _gameState->skyboxMode);
            }
            // Skip excess cameras
            else if (isSystem == false && totalCameraCount - systemCameraCount >= CAMERAS_MAX) {
                break;
            }
            // Non-backdrop cameras target
            else {
                // Target resolution may be overriden and depends on projection mode & pipeline segment,
                if (mode == Orthographic && (isAfterPost || isSystem)) {
                    // ortho targets use screen resolution for optimal display (pixelperfect)
                    targetWidth = camera_get_target_width(ce->camera) > 0 ?
                                  camera_get_target_width(ce->camera) * _screenPPP : _screenWidth;
                    targetHeight = camera_get_target_height(ce->camera) > 0 ?
                                   camera_get_target_height(ce->camera) * _screenPPP : _screenHeight;
                    targetX = camera_get_target_x(ce->camera) * _screenPPP;
                    targetY = camera_get_target_y(ce->camera) * _screenPPP;
                    fullWidth = _screenWidth;
                    fullHeight = _screenHeight;
                } else {
                    // perspective targets use scaled resolution
                    targetWidth = camera_get_target_width(ce->camera) > 0 ?
                                  camera_get_target_width(ce->camera) * _qualityParams.renderScale : _renderWidth;
                    targetHeight = camera_get_target_height(ce->camera) > 0 ?
                                   camera_get_target_height(ce->camera) * _qualityParams.renderScale : _renderHeight;
                    targetX = camera_get_target_x(ce->camera) * _qualityParams.renderScale;
                    targetY = camera_get_target_y(ce->camera) * _qualityParams.renderScale;
                    fullWidth = _renderWidth;
                    fullHeight = _renderHeight;
                }
                targetWidth = minimum(targetWidth, fullWidth);
                targetHeight = minimum(targetHeight, fullHeight);
                uMax = targetWidth / _framebufferWidth;
                vMax = targetHeight / _framebufferHeight;

                // Get, create, or refresh target
                /*bool newTarget = _targets.find(camera) == _targets.end();
                if (newTarget == false && camera_is_target_dirty(camera)) {
                    bgfx::destroy(_targets[camera].targetFbh);
                    _targets.erase(camera);
                    newTarget = true;
                }
                if (newTarget) {
                    CameraEntry entry;
                    if (createCameraTarget(&entry, targetWidth, targetHeight) == false) {
                        vxlog_error("⚠️⚠️⚠️renderer target allocation failed, skipping");
                        break;
                    }
                    _targets[camera] = entry;
                }*/
                //target = _targets[camera].targetFbh;
            }

            // Camera planes used for culling
            Transform *cameraView = camera_get_view_transform(ce->camera);
            computeWorldPlanes(cameraView, *transform_get_position(cameraView, false), float3_zero,
                               camera_get_near(ce->camera), camera_get_far(ce->camera),
                               fov, mode == Perspective, width, height, planes);

            // Set views state for current target
            bgfx::ViewId stencil, opaque, lights, shadows[SHADOWMAP_CASCADES_MAX], weight, blitDeferred, transparent;
            setTargetViews(ce->segment, cameraCount[ce->segment], isBackdrop, targetWidth, targetHeight, &stencil, &opaque, &lights, shadows, &weight, &blitDeferred, &transparent);

            // Use target fbh because 'transparent' view wants depth from g-buffer, which is at framebuffer size,
            // so each segment is blit back into backbuffer size by _postprocessView1, _afterpostView, and _systemView
            bgfx::setViewFrameBuffer(blitDeferred, getPipelineSegmentFramebuffer(ce->segment));
            bgfx::setViewFrameBuffer(transparent, getPipelineSegmentFramebuffer(ce->segment));

            // Set target views dimensions
            bgfx::setViewRect(blitDeferred, targetX, targetY, targetWidth, targetHeight);
            bgfx::setViewRect(transparent, targetX, targetY, targetWidth, targetHeight);

            // First blit in each segment should clear if target isn't fullscreen (backdrop has skybox)
            if (isBackdrop == false && cameraCount[ce->segment] == 0 && (targetX != 0.0f || targetY != 0.0f ||
                targetWidth != fullWidth || targetHeight != fullHeight)) {

                switch(ce->segment) {
                    case PipelineSegment_PostTarget:
                        bgfx::touch(VIEW_CLEAR_1);
                        break;
                    case PipelineSegment_AfterPostTarget:
                        bgfx::touch(VIEW_CLEAR_2);
                        break;
                    case PipelineSegment_SystemTarget:
                        bgfx::touch(VIEW_CLEAR_3);
                        break;
                }
            }

            // Set game view/proj TODO: use cached camera proj
            static float view[16], proj[16];
            setViewTransformFromCamera(stencil, ce->camera, width, height, view, proj);
            bgfx::setViewTransform(opaque, view, proj);
            if (deferred) {
                bgfx::setViewTransform(weight, view, proj);
            }
            bgfx::setViewTransform(transparent, view, proj);
            _worldText->setViewProperties(view, proj, _pointsWidth, _pointsHeight, _screenPPP, mode == Perspective ? fov : 0.0f,
                                          camera_is_projection_dirty(ce->camera), camera_is_view_dirty(ce->camera));

            // Set target views screen projection
            if (deferred) {
                bgfx::setViewTransform(lights, nullptr, screenProj);
            }
            bgfx::setViewTransform(blitDeferred, nullptr, screenProj);

            // Helper matrices
            static float invView[16], viewProj[16];
            bx::mtxInverse(invView, view);
            bx::mtxMul(viewProj, view, proj);

            // Camera projection matrix used for helper functions
            camera_set_proj_matrix(ce->camera, reinterpret_cast<Matrix4x4 *>(proj), reinterpret_cast<Matrix4x4 *>(viewProj));
            camera_clear_dirty(ce->camera);

            // Non-backdrop targets always write alpha for blending (can be overriden eg. screenshots),
            // first target in each segment writes w/o blend, successive targets use alpha blending
            const bool blitAlpha = _blitAlpha || isBackdrop == false;
            const bool blitBlend = cameraCount[ce->segment] > 0;

            // Draw all passes for current camera
            const uint16_t layers = camera_get_layers(ce->camera);
            if (drawPass_Geometry(stencil, _game, GeometryRenderPass_StencilWrite, view, invView, isBackdrop, pointsScaling, planes, cameraGeom) == false) {
                bgfx::touch(stencil);
            }
            if (drawPass_Geometry(opaque, _game, GeometryRenderPass_Opaque, view, invView, isBackdrop, pointsScaling, planes, cameraGeom) == false) {
                bgfx::touch(opaque);
            }
            if (_overlayDrawn == false) {
                bgfx::touch(VIEW_UI_OVERLAY(_afterpostView));
            }
            if (deferred) {
                if (drawPass_Lights(lights, shadows, _game, invView, viewProj, ce->camera, width, height, uMax, vMax, cameraGeom) == false) {
                    bgfx::touch(lights);
                }
                if (drawPass_Geometry(weight, _game, GeometryRenderPass_TransparentWeight, view, invView, isBackdrop, pointsScaling, planes, cameraGeom) == false) {
                    bgfx::touch(weight);
                }
            } else {
                drawPass_Geometry(transparent, _game, GeometryRenderPass_TransparentFallback, view, invView, isBackdrop, pointsScaling, planes, cameraGeom);
            }
            drawDeferred(blitDeferred, blitAlpha, blitBlend, ce->camera, proj, width, height, targetWidth, targetHeight, uMax, vMax, layers);

            if (drawPass_Geometry(transparent, _game, GeometryRenderPass_TransparentPost, view, invView, isBackdrop, pointsScaling, planes, cameraGeom) == false) {
                bgfx::touch(transparent);
            }

            // Flush camera entries and planes
            for (auto itr = cameraGeom.begin(); itr != cameraGeom.end(); itr++) {
                doubly_linked_list_flush(itr->second, nullptr); // entries not freed here
            }
            for (int i = 0; i < 6; ++i) {
                plane_free(planes[i]);
                planes[i] = nullptr;
            }

            ++cameraCount[ce->segment];
            INC_CAMERA
            n = doubly_linked_list_node_next(n);
        }
        doubly_linked_list_flush(it->second, free);
    }
    for (auto itr = cameraGeom.begin(); itr != cameraGeom.end(); itr++) {
        doubly_linked_list_free(itr->second);
    }
    for (auto itr = _geometryEntries.begin(); itr != _geometryEntries.end(); itr++) {
        doubly_linked_list_flush(itr->second, releaseGeometryEntry);
    }
    clearObjectLists();

    // Reset unused views
    for (int i = _systemView + 1; i < _maxView; ++i) {
        bgfx::resetView(i);
        bgfx::setViewFrameBuffer(i, BGFX_INVALID_HANDLE);
    }
    _maxView = _systemView;

    const float uMax_render = static_cast<float>(_renderWidth) / _framebufferWidth;
    const float vMax_render = static_cast<float>(_renderHeight) / _framebufferHeight;
    const float uMax_screen = static_cast<float>(_screenWidth) / _framebufferWidth;
    const float vMax_screen = static_cast<float>(_screenHeight) / _framebufferHeight;

    // Optional post-process, or blit post-process views into backbuffer
    if (bgfx::isValid(_postProgram)) {
        // if upscaled, apply postprocess after upscaling
        if (_renderWidth < _screenWidth) {
            drawTarget(_postprocessView1, _targetProgram, _targetsFbTex, 0,
                       0, 0, _screenWidth, _screenHeight, _screenWidth, _screenHeight,
                       uMax_render, vMax_render, false);

            drawPost(_postprocessView2, _postprocessFbTex, _screenWidth, _screenHeight, 1.0f, 1.0f);
        }
        // if downscaled, apply postprocess before downscaling
        else {
            drawPost(_postprocessView2, _targetsFbTex, _framebufferWidth, _framebufferHeight, uMax_render, vMax_render);
        }
    } else {
        drawTarget(_postprocessView2, _targetProgram, _targetsFbTex, 0,
                   0, 0, _screenWidth, _screenHeight, _screenWidth, _screenHeight,
                   uMax_render, vMax_render, false);
    }

    // Blit after-post-process views into backbuffer
    if (cameraCount[PipelineSegment_AfterPostTarget] > 0) {
        drawTarget(_afterpostView, _targetProgram, _afterPostTargetsFbTex,
                   0, 0, 0, _screenWidth, _screenHeight,
                   _screenWidth, _screenHeight, uMax_screen, vMax_screen, true);
    }

    // Draw blocker or clear, keep it if last action was to fade out
    if (_blockerTimer <= _blockerDuration || _blockerInOut == false) {
        drawBlocker(VIEW_UI_BLOCKER(_afterpostView), _blockerInOut ? 1.0f - _blockerTimer / _blockerDuration : _blockerTimer / _blockerDuration);
    }

    // Blit system views into backbuffer
    if (cameraCount[PipelineSegment_SystemTarget] > 0) {
        drawTarget(_systemView, _targetProgram, _systemTargetsFbTex, 0,
                   0, 0, _screenWidth, _screenHeight, _screenWidth, _screenHeight,
                   uMax_screen, vMax_screen, true);
    }

    // End-of-frame cleanup
    _worldText->endOfFrame();

    debug_clearText();
    if (_gameState->debug_displayFPS) {
        debug_lua_displayFPS();
    }
#if DEBUG_RENDERER
#if DEBUG_RENDERER_STATS
    debug_displayStats();
#endif
#if DEBUG_RENDERER_SUBRDR_STATS
    debug_displaySubRenderersStats();
#endif
    if (_game != nullptr) {
#if DEBUG_RENDERER_STATS_GAME
        debug_displayGameInfo(_game);
#endif
#if DEBUG_RENDERER_STATS_VB_OCCUPANCY
        debug_displayVBOccupancy(_game);
#endif
#if !DEBUG_RENDERER_DISPLAY_RTREE && DEBUG_RENDERER_DISPLAY_HIERARCHY
        debug_displaySceneHierarchy(_game);
#endif
#if DEBUG_RENDERER_DISPLAY_RTREE || DEBUG_RENDERER_DISPLAY_RTREE_STATS || DEBUG_RENDERER_DISPLAY_RTREE_BOXES
        debug_displayRtree(_game);
#endif
#if DEBUG_RENDERER_DISPLAY_RIGIDBODY_STATS
        debug_displayRigidbodyStats(_game);
#endif
    }
#endif
    if (_gameState->debug_displayColliders) {
        debug_lua_displayCollidersStats();
    }
}

void GameRenderer::refreshViews(uint16_t width, uint16_t height, float devicePPP) {
    // VX may send a refresh views w/ dimensions 0 when minimizing the app
    if (width == 0 || height == 0 || devicePPP < 0) {
        return;
    }

    _screenWidth = width;
    _screenHeight = height;
    _screenPPP = devicePPP;

    _pointsWidth = _screenWidth / _screenPPP;
    _pointsHeight = _screenHeight / _screenPPP;

#if RENDER_UPSCALE_ENABLED
    _qualityParams.renderScale = minimum(_qualityParams.renderScale, static_cast<float>(RENDER_SCALE_MAX_TEX) / maximum(_pointsWidth, _pointsHeight));
#else
    _qualityParams.renderScale = minimum(_qualityParams.renderScale, _screenPPP);
#endif

    _renderWidth = _pointsWidth * _qualityParams.renderScale;
    _renderHeight = _pointsHeight * _qualityParams.renderScale;

    _framebufferWidth = maximum(_renderWidth, _screenWidth);
    _framebufferHeight = maximum(_renderHeight, _screenHeight);

    if (_toggled) {
        _viewsDirty = true;
    } else {
        refreshDefaultViews();
    }
}

bgfx::ViewId GameRenderer::getImguiView() {
    return VIEW_UI_IMGUI(_afterpostView);
}

void GameRenderer::screenshot(void** out,
                              uint16_t *outWidth,
                              uint16_t *outHeight,
                              size_t *outSize,
                              const std::string& filename,
                              bool outPng,
                              bool noBg,
                              uint16_t maxWidth,
                              uint16_t maxHeight,
                              bool unflip,
                              bool trim,
                              EnforceRatioStrategy ratioStrategy,
                              float outputRatio) {
    if (_game == nullptr || _gameState == nullptr || _renderWidth == 0 || _renderHeight == 0 || _errored) {
        return;
    }

    if (_features.isTextureBlitEnabled() == false) {
        vxlog_error("⚠️⚠️⚠️ screenshot: texture blit is not supported and emulation isn't available");
        return;
    }

    if (_features.isTextureReadBackEnabled() == false) {
        vxlog_error("⚠️⚠️⚠️ screenshot: texture read back is not supported and emulation isn't available");
        return;
    }

    if (_game == nullptr || _gameState == nullptr) {
        return;
    }

    const uint64_t samplerFlags = 0
                                  | BGFX_SAMPLER_MIN_POINT
                                  | BGFX_SAMPLER_MAG_POINT
                                  | BGFX_SAMPLER_MIP_POINT
                                  | BGFX_SAMPLER_U_CLAMP
                                  | BGFX_SAMPLER_V_CLAMP;

    const bgfx::ViewId blitView = VIEW_UI_BLOCKER(_afterpostView);
    uint32_t width = _screenWidth;
    uint32_t height = _screenHeight;

    bgfx::FrameBufferHandle outFbh = bgfx::createFrameBuffer(_screenWidth, _screenHeight, bgfx::TextureFormat::RGBA8, BGFX_TEXTURE_RT | samplerFlags);
    if (bgfx::isValid(outFbh) == false) {
        vxlog_error("⚠️⚠️⚠️ screenshot: resource allocation failed, skipping");
        return;
    }

    bgfx::TextureHandle blitTh = bgfx::createTexture2D(width, height, false, 1, bgfx::TextureFormat::RGBA8, BGFX_TEXTURE_BLIT_DST | BGFX_TEXTURE_READ_BACK | samplerFlags);
    if (bgfx::isValid(blitTh) == false) {
        bgfx::destroy(outFbh);
        vxlog_error("⚠️⚠️⚠️ screenshot: resource allocation failed, skipping");
        return;
    }

    void *data = malloc(width * height * 4);

    // optional transparent background
    const SkyboxMode skybox = _gameState->skyboxMode;
    if (noBg) {
        bgfx::setViewClear(VIEW_SKYBOX, BGFX_CLEAR_COLOR, 0x00000000);
        _gameState->skyboxMode = SkyboxMode_Clear;
        _blitAlpha = true;
    }

    // draw into fb and blit to read back tex
    render(outFbh);
    bgfx::blit(blitView,
               blitTh, 0, 0,
               bgfx::getTexture(outFbh),
               (_screenWidth - width) / 2, (_screenHeight - height) / 2,
               width, height);

    // read output, this uses read pixels if read back isn't enabled
    uint32_t frame = bgfx::readTexture(blitTh, data);
    while (frame >= bgfx::frame());

    // reset views
    if (noBg) {
        bgfx::setViewClear(VIEW_SKYBOX, BGFX_CLEAR_COLOR, _gameState->clearColor);
        _gameState->skyboxMode = skybox;
        _blitAlpha = false;
    }
    _viewsDirty = true;

    // TODO: apply flip & return data, anything else below could be moved & called outside of renderer
    // TODO: downscale can be done much more efficiently with a shader

    // Utils::savePng("before-trim", blitW, blitH, data, _screenquadFlipV, nullptr, nullptr);

    // trim if requested (removes transparent pixels all around)
    if (trim) {
        uint16_t trimmedW = 0;
        uint16_t trimmedH = 0;
        void *trimmed = nullptr;
        
        Utils::trim(data, width, height, &trimmed, trimmedW, trimmedH, 4);
        if (trimmed != nullptr) {
            void *oldData = data;
            data = trimmed;
            width = trimmedW;
            height = trimmedH;
            free(oldData);
        }
    }

    // Utils::savePng("trimmed", trimmedW, trimmedH, data, _screenquadFlipV, nullptr, nullptr);
    
    // -----------------------------
    // enforce ratio if there's
    // a strategy for it.
    // -----------------------------
    
    if (ratioStrategy != EnforceRatioStrategy::none && outputRatio > 0.0f) {

        if (ratioStrategy == EnforceRatioStrategy::padding) {
            
            uint16_t expandedW = width;
            uint16_t expandedH = height;
            void *expanded = nullptr;
            
            const float ratio = static_cast<float>(width) / static_cast<float>(height);
            if (ratio >= outputRatio) { // width too big, increase height
                expandedH = static_cast<uint32_t>(static_cast<float>(width) * (1.0f / outputRatio));
            } else { // height too big, increase width
                expandedW = static_cast<uint32_t>(static_cast<float>(height) * outputRatio);
            }
            
            Utils::expand(data, width, height, &expanded, expandedW, expandedH, 4);
            
            void *oldData = data;
            data = expanded;
            width = expandedW;
            height = expandedH;
            free(oldData);
            
            // Utils::savePng("expanded", finalW, finalH, data, _screenquadFlipV, nullptr, nullptr);

        } else if (ratioStrategy == EnforceRatioStrategy::crop) {
            
            uint16_t croppedW = width;
            uint16_t croppedH = height;
            void *cropped = nullptr;
            
            const float ratio = static_cast<float>(width) / static_cast<float>(height);
            if (ratio >= outputRatio) { // width too big, decrease it
                float croppedWF = static_cast<float>(height) * outputRatio;
                croppedW = static_cast<uint32_t>(croppedWF);
                if (floorf(croppedWF) != croppedWF) {
                    ++croppedW; // always round up
                }
            } else { // height too big, decrease it
                float croppedHF = static_cast<float>(width) * (1.0f / outputRatio);
                croppedH = static_cast<uint32_t>(croppedHF);
                if (floorf(croppedHF) != croppedHF) {
                    ++croppedH; // always round up
                }
            }
            
            Utils::crop(data, width, height, &cropped, croppedW, croppedH, 4);
            
            void *oldData = data;
            data = cropped;
            width = croppedW;
            height = croppedH;
            free(oldData);
            
            // Utils::savePng("cropped", width, height, data, _screenquadFlipV, nullptr, nullptr);
        }
    }

    // -----------------------------
    // downscale maintaining ratio
    // if bigger than max size
    // -----------------------------
    
    uint32_t limitedWidth = maxWidth > 0 && width > maxWidth ? maxWidth : width;
    uint32_t limitedHeight = maxHeight > 0 && height > maxHeight ? maxHeight : height;

    if (limitedWidth != width || limitedHeight != height) {
        
        const float ratioW = static_cast<float>(limitedWidth) / static_cast<float>(width);
        const float ratioH = static_cast<float>(limitedHeight) / static_cast<float>(height);
        if (ratioW < ratioH) {
            float limitedHeightF = ratioW * static_cast<float>(height);
            limitedHeight = static_cast<uint32_t>(limitedHeightF);
            if (floorf(limitedHeightF) != limitedHeightF) {
                ++limitedHeight; // always round up
            }
        } else {
            float limitedWidthF = ratioH * static_cast<float>(width);
            limitedWidth = static_cast<uint32_t>(limitedWidthF);
            if (floorf(limitedWidthF) != limitedWidthF) {
                ++limitedWidth; // always round up
            }
        }

        void *downscaled = nullptr;
        Utils::downscalePoint(data, width, height, &downscaled, limitedWidth, limitedHeight, 4);

        void *oldData = data;
        data = downscaled;
        width = limitedWidth;
        height = limitedHeight;
        free(oldData);
        
        // Utils::savePng("downscaled", width, height, data, _screenquadFlipV, nullptr, nullptr);
    }
    
    // -----------------------------
    // save a PNG to vx::fs storage
    // and/or dump PNG data in "out" pointer
    // -----------------------------

    if (filename.empty() == false) {
        Utils::savePng(filename, width, height, data, _features.screenquadFlipV, outPng ? out : nullptr, outPng ? outSize : nullptr);
    } else if (outPng) {
        Utils::savePng("", width, height, data, _features.screenquadFlipV, out, outSize, 8, false);
    }

    // texels data in "out" pointer
    if (out != nullptr && outPng == false) {
        // unflip texels if requested
        if (unflip && _features.screenquadFlipV) {
            const uint32_t pitch = width * 4;
            uint8_t *unflipped = static_cast<uint8_t*>(malloc(width * height * 4));
            uint8_t *src = static_cast<uint8_t*>(data) + (height - 1) * pitch;
            for (uint32_t i = 0; i < height; i++) {
                memcpy(unflipped + i * pitch, src, pitch);
                src -= pitch;
            }
            free(data);
            data = unflipped;
        }

        *out = data;
    } else {
        free(data);
    }
    if (outWidth != nullptr) {
        *outWidth = width;
    }
    if (outHeight != nullptr) {
        *outHeight = height;
    }
    if (outSize != nullptr && outPng == false) {
        *outSize = width * height * 4;
    }

    bgfx::destroy(blitTh);
    bgfx::destroy(outFbh);
}

void GameRenderer::getOrComputeWorldTextSize(WorldText *wt, float *outWidth, float *outHeight, bool points) {
    _worldText->getOrComputeWorldTextSize(wt, outWidth, outHeight, points);
}

float GameRenderer::getCharacterSize(FontName font, const char *text, bool points) {
    return _worldText->getCharacterSize(font, text, points);
}

size_t GameRenderer::snapWorldTextCursor(WorldText *wt, float *inoutX, float *inoutY, size_t *inCharIndex, bool points) {
    return _worldText->snapWorldTextCursor(wt, inoutX, inoutY, inCharIndex, points);
}

bool GameRenderer::isFallbackEnabled() {
    return _features.useDeferred() == false;
}

QualityTier GameRenderer::getQualityTier() {
    return _qualityPreset;
}

float GameRenderer::getRenderWidth() {
    return _renderWidth;
}

float GameRenderer::getRenderHeight() {
    return _renderHeight;
}

bool GameRenderer::reload(QualityTier tier) {
    _qualityPreset = tier;

    QualityParameters params; params.fromPreset(tier);
    return reload(params);
}

bool GameRenderer::reload(QualityParameters params) {
    // When on fallback, lock the device to tier 0
    if (isFallbackEnabled()) {
        params.fromPreset(QualityTier_Compatibility);
    }

    bool willReload = false;
    if (params.shadowsTier != _qualityParams.shadowsTier) {
        _shadowmapsDirty = true;
        _programsDirty = true;
    }
    if (params.renderScale != _qualityParams.renderScale) {
        willReload = true;
    }
    if (params.aa != _qualityParams.aa || params.postTier != _qualityParams.postTier) {
        _programsDirty = true;
        _viewsDirty = true;
    }
    _qualityParams = params;

    if (willReload) {
        refreshViews(_screenWidth, _screenHeight, _screenPPP);
    }

    return willReload;
}

void GameRenderer::togglePBR(bool toggle) {
    if (_features.useDeferred() && _usePBR != toggle) {
        _usePBR = toggle;
        _programsDirty = true;
        _viewsDirty = true;
    }
}

bool GameRenderer::isPBR() {
    return _usePBR;
}

void GameRenderer::debug_checkAndDrawShapeRtree(const Transform *t, const Shape *s, const int onlyLevel) {
#if DEBUG_RENDERER
    Rtree *r = shape_get_rtree(s);
    vx_assert(debug_rtree_integrity_check(r));
    Matrix4x4 model; transform_utils_get_model_ltw(t, &model);
    debug_displayRtree_recurse(r, rtree_get_root(r), 0, 0, 0, true, &model, onlyLevel);
#endif
}

// MARK: - DRAW FUNCTIONS -

void GameRenderer::addGeometryEntry(GeometryType type, void *ptr, VERTEX_LIGHT_STRUCT_T sample,
                                    float3 center, float radius, uint16_t layers, uint8_t maskID,
                                    uint8_t maskDepth, bool stencilWrite, bool worldDirty) {

    GeometryEntry *entry = static_cast<GeometryEntry*>(malloc(sizeof(GeometryEntry)));
    entry->type = type;
    entry->ptr = ptr;
    entry->sampleColor = sample;
    entry->center = center;
    entry->radius = radius;
    entry->eyeSqDist = FLT_MAX;
    entry->layers = layers;
    entry->maskID = maskID;
    entry->maskDepth = maskDepth;
    entry->stencilWrite = stencilWrite;
    entry->geometryCulled = false;
    entry->worldDirty = worldDirty;
    if (_geometryEntries.find(type) == _geometryEntries.end()) {
        _geometryEntries[type] = doubly_linked_list_new();
    }
    doubly_linked_list_push_last(_geometryEntries[type], entry);

    if (radius > 0) {
#if DEBUG_RENDERER_DISPLAY_CULLSPHERES
        drawSphere(VIEW_OPAQUE(VIEW_SKYBOX), &entry->center, entry->radius);
#endif
    }
}

void GameRenderer::releaseGeometryEntry(void *ptr) {
    GeometryEntry *entry = static_cast<GeometryEntry*>(ptr);
    if (entry->type == GeometryType_BoundingBox) {
        free(entry->ptr);
    } else if (entry->type == GeometryType_ModelBox) {
        matrix4x4_free(static_cast<Matrix4x4*>(entry->ptr));
    } else {
        weakptr_release(static_cast<Weakptr*>(entry->ptr));
    }
    free(entry);
}

bool GameRenderer::getGeometryForCamera_sortFunc(DoublyLinkedListNode *n1, DoublyLinkedListNode *n2) {
    const GeometryEntry *entry1 = static_cast<GeometryEntry*>(doubly_linked_list_node_pointer(n1));
    const GeometryEntry *entry2 = static_cast<GeometryEntry*>(doubly_linked_list_node_pointer(n2));

    return entry1->eyeSqDist > entry2->eyeSqDist;
}

// TODO: sort is way too slow here, could try different algorithm, not urgent
size_t GameRenderer::getGeometryForCamera(const Camera *camera, std::unordered_map<GeometryType, DoublyLinkedList*> &outGeom) {
    DoublyLinkedListNode *n;
    GeometryEntry *entry;
    const uint16_t layers = camera_get_layers(camera);
    // const float3 *pos = camera_get_position(camera);

    size_t count = 0;
    for (auto itr = _geometryEntries.begin(); itr != _geometryEntries.end(); itr++) {
        const GeometryType type = itr->first;

        if (outGeom.find(type) == outGeom.end()) {
            outGeom[type] = doubly_linked_list_new();
        } else if (doubly_linked_list_is_empty(outGeom[type]) == false) {
            doubly_linked_list_flush(outGeom[type], nullptr); // entries not freed here
        }

        n = doubly_linked_list_first(itr->second);
        while (n != nullptr) {
            entry = static_cast<GeometryEntry*>(doubly_linked_list_node_pointer(n));

            if (camera_layers_match(layers, entry->layers)) {
                /*const float dx = entry->center.x - entry->radius - pos->x;
                const float dy = entry->center.y - entry->radius - pos->y;
                const float dz = entry->center.z - entry->radius - pos->z;
                entry->eyeSqDist = dx * dx + dy * dy + dz * dz;*/
                entry->geometryCulled = false;
                entry->cameraDirty = true;

                doubly_linked_list_push_last(outGeom[type], entry);
                ++count;
            }

            n = doubly_linked_list_node_next(n);
        }
        //doubly_linked_list_sort_ascending(outGeom[itr->first], getGeometryForCamera_sortFunc);
    }

    return count;
}

void GameRenderer::preDrawCollectData_recurse(Transform *t, bool hierarchyDirty, bool hidden, bool sample,
                                              VERTEX_LIGHT_STRUCT_T *color, uint8_t *nextMaskID,
                                              uint8_t maskID, int maskDepth) {

    DoublyLinkedListNode *n = transform_get_children_iterator(t);
    Transform *child = nullptr;
    uint8_t childMaskID;
    int childMaskDepth;
    uint16_t layers;

    transform_refresh(t, hierarchyDirty, false);

    while (n != nullptr) {
        child = static_cast<Transform *>(doubly_linked_list_node_pointer(n));
        childMaskID = maskID;
        childMaskDepth = maskDepth;
        layers = CAMERA_LAYERS_DEFAULT;

        const bool drawSelf = hidden == false && transform_is_hidden(child) == false;
        const bool drawCollider = _gameState->debug_displayColliders || debug_transform_display_collider(child);
        const bool drawBox = _gameState->debug_displayBoxes || debug_transform_display_box(child);

        switch(transform_get_type(child)) {
            case HierarchyTransform:
                break;
            case PointTransform:
                if (drawCollider) {
                    Box collider;
                    if (transform_get_or_compute_world_aligned_collider(child, &collider, false) != nullptr) {
                        float3 center; box_get_center(&collider, &center);
                        const float radius = box_get_diagonal(&collider) * 0.5f;
                        const uint16_t defaultCameraLayers = camera_get_layers(game_get_camera(_game->getCGame()));

                        addGeometryEntry(GeometryType_Collider,
                                         transform_get_and_retain_weakptr(child),
                                         *color, center, radius, defaultCameraLayers,
                                         maskID, maskDepth, false);
                    }
                }
                break;
            case ShapeTransform: {
                Shape *shape = static_cast<Shape*>(transform_get_ptr(child));
                layers = shape_get_layers(shape);

                // World bounding sphere
                float3 center = float3_zero;
                float radius = 0.0f;
                bool worldDirty = true;

                if (drawSelf || drawBox || drawCollider) {
                    Box waabb;
                    worldDirty = shape_get_world_aabb(shape, &waabb, false);

                    box_get_center(&waabb, &center);
                    radius = box_get_diagonal(&waabb) * 0.5f;
                }

                const bool wireframe = drawCollider && rigidbody_uses_per_block_collisions(shape_get_rigidbody(shape));
                if (drawSelf || wireframe) {
                    if (sample) {
                        *color = sampleBakedLightingFromPoint(center.x, center.y, center.z);
                    }

                    shape_check_and_clear_drawmodes(shape);

                    addGeometryEntry(GeometryType_Shape, transform_get_and_retain_weakptr(child),
                                     *color, center, radius, layers, maskID,
                                     maskDepth, false, worldDirty);
                }
                if (drawBox) {
                    Box *aabb = box_new();
                    shape_get_world_aabb(shape, aabb, false);

                    addGeometryEntry(GeometryType_BoundingBox, aabb, *color, center, radius,
                                     layers, maskID, maskDepth, false, worldDirty);
                }
                if (drawCollider) {
                    addGeometryEntry(GeometryType_Collider, transform_get_and_retain_weakptr(child),
                                     *color, center, radius, layers, maskID,
                                     maskDepth, false, worldDirty);
                }
#if DEBUG_RENDERER_DISPLAY_CHUNKS
                Index3D *chunks = shape_get_chunks(shape);
                Index3DIterator *it = index3d_iterator_new(chunks);
                Chunk *chunk;
                while (index3d_iterator_pointer(it) != NULL) {
                    chunk = static_cast<Chunk *>(index3d_iterator_pointer(it));

                    const SHAPE_COORDS_INT3_T chunkOrigin = chunk_get_origin(chunk);
                    Box chunkAABB; chunk_get_bounding_box(chunk, &chunkAABB.min, &chunkAABB.max);

                    Matrix4x4 model = *shape_get_model_matrix(shape);
                    Matrix4x4 *mtx = matrix4x4_new_identity();

                    float3 translation = {
                        (float)(chunkAABB.min.x + chunkOrigin.x),
                        (float)(chunkAABB.min.y + chunkOrigin.y),
                        (float)(chunkAABB.min.z + chunkOrigin.z)
                    };
                    if (float3_isZero(&translation, EPSILON_ZERO) == false) {
                        matrix4x4_set_translation(mtx, translation.x, translation.y, translation.z);
                        matrix4x4_op_multiply(&model, mtx);
                    }

                    matrix4x4_set_scaleXYZ(mtx,
                                           chunkAABB.max.x - chunkAABB.min.x,
                                           chunkAABB.max.y - chunkAABB.min.y,
                                           chunkAABB.max.z - chunkAABB.min.z);
                    matrix4x4_op_multiply_2(&model, mtx);

                    addGeometryEntry(GeometryType_ModelBox, mtx, *color, center, radius,
                                     layers, maskID, maskDepth, false);

                    index3d_iterator_next(it);
                }
                index3d_iterator_free(it);
#endif
                break;
            }
            case QuadTransform: {
                Quad *quad = static_cast<Quad *>(transform_get_ptr(child));
                layers = quad_get_layers(quad);

                // World bounding sphere
                float3 center;
                float radius = 0.0f;

                if (drawSelf || drawCollider) {
                    const Matrix4x4 *ltw = transform_get_ltw(child);

                    const float3 localCenter = {
                        quad_get_width(quad) * (0.5f - quad_get_anchor_x(quad)),
                        quad_get_height(quad) * (0.5f - quad_get_anchor_y(quad)),
                        0.0f
                    };
                    matrix4x4_op_multiply_vec_point(&center, &localCenter, ltw);

                    float3 lossyScale; matrix4x4_get_scaleXYZ(ltw, &lossyScale);
                    radius = quad_utils_get_diagonal(quad) * 0.5f * float3_mmax(&lossyScale);
                };

                if (drawSelf) {
                    if (sample) {
                        *color = sampleBakedLightingFromPoint(center.x, center.y, center.z);
                    }

                    quad_check_and_clear_drawmodes(quad);

                    bool stencilWrite = quad_is_mask(quad);
                    if (stencilWrite) {
                        if (maskDepth < 0) { // new mask
                            if (*nextMaskID <= QUAD_MASK_ID_MAX) {
                                childMaskID = *nextMaskID;
                                childMaskDepth = 0;
                                ++(*nextMaskID);
                            } else {
                                stencilWrite = false;
                            }
                        } else { // nested mask
                            if (maskDepth + 1 <= QUAD_MASK_DEPTH_MAX) {
                                childMaskDepth = maskDepth + 1;
                            } else {
                                stencilWrite = false;
                            }
                        }
                    }
                    addGeometryEntry(GeometryType_Quad, transform_get_and_retain_weakptr(child),
                                     *color, center, radius, layers,
                                     childMaskID, childMaskDepth, stencilWrite);

                    if (quad_get_data_size(quad) > 0) {
                        ++_quadsBatchTexturedMaxCount;
                    }
                    ++_quadsBatchAllMaxCount;
                }
                if (drawCollider) {
                    addGeometryEntry(GeometryType_Collider, transform_get_and_retain_weakptr(child),
                                     *color, center, radius, layers, maskID,
                                     maskDepth, false);
                }
                break;
            }
            case WorldTextTransform: {
                WorldText *wt = static_cast<WorldText*>(transform_get_ptr(child));
                if (drawSelf) {
                    layers = world_text_get_layers(wt);

                    const float3 *pos = transform_get_position(child, false);
                    if (sample) {
                        *color = sampleBakedLightingFromPoint(pos->x, pos->y, pos->z);
                        sample = false; // all children w/ same lighting
                    }

                    world_text_check_and_clear_drawmodes(wt);

                    addGeometryEntry(GeometryType_WorldText, transform_get_and_retain_weakptr(child), *color,
                                     *pos, 0.0f, layers, maskID, maskDepth, false);
                }
                break;
            }
            case LightTransform: {
                Light *light = static_cast<Light *>(transform_get_ptr(child));
                if (light_is_enabled(light) && drawSelf) {
                    layers = light_get_layers(light);

                    const uint8_t priority = light_get_priority(light);
                    if (_lights.find(priority) == _lights.end()) {
                        _lights[priority] = doubly_linked_list_new();
                    }
                    doubly_linked_list_push_last(_lights[priority], light);

                    if (_qualityParams.shadowsTier > 0) {
                        _emulateShadows &= light_is_shadow_caster(light) == false;
                    }
#if DEBUG_RENDERER_STATS
                    ++_debug_lights;
#endif
                }
                break;
            }
            case CameraTransform: {
                Camera *camera = static_cast<Camera*>(transform_get_ptr(child));
                if (camera_is_enabled(camera)) {
                    layers = camera_get_layers(camera);

                    const uint8_t order = camera_get_order(camera);
                    if (_cameras.find(order) == _cameras.end()) {
                        _cameras[order] = doubly_linked_list_new();
                    }
                    CameraEntry *entry = static_cast<CameraEntry*>(malloc(sizeof(CameraEntry)));
                    entry->camera = camera;

                    if (camera_get_mode(camera) == Perspective) {
                        _maxPerspectiveViewOrder = maximum(_maxPerspectiveViewOrder, order);
                    }

                    doubly_linked_list_push_last(_cameras[order], entry);
#if DEBUG_RENDERER_STATS
                    ++_debug_cameras;
#endif
                }
                break;
            }
            case AudioSourceTransform: {
                AudioSource * const as = static_cast<AudioSource*>(transform_get_ptr(child));
                audioSource_refresh(as);
                break;
            }
            case AudioListenerTransform: {
                AudioListener * const al = static_cast<AudioListener*>(transform_get_ptr(child));
                audioListener_refresh(al);
                break;
            }
            case MeshTransform: {
                Mesh *mesh = static_cast<Mesh*>(transform_get_ptr(child));
                if (mesh_get_vertex_count(mesh) > 0) {
                    layers = mesh_get_layers(mesh);

                    // World bounding sphere
                    float3 center = float3_zero;
                    float radius = 0.0f;

                    if (drawSelf || drawBox || drawCollider) {
                        Box waabb; mesh_get_world_aabb(mesh, &waabb, false);

                        box_get_center(&waabb, &center);
                        radius = box_get_diagonal(&waabb) * 0.5f;
                    }

                    if (drawSelf) {
                        const float3 *pos = transform_get_position(child, false);
                        if (sample) {
                            *color = sampleBakedLightingFromPoint(pos->x, pos->y, pos->z);
                        }

                        addGeometryEntry(GeometryType_Mesh, transform_get_and_retain_weakptr(child),
                                         *color, center, radius, layers, maskID,
                                         maskDepth, false, false);
                    }
                    if (drawBox) {
                        Box *aabb = box_new();
                        mesh_get_world_aabb(mesh, aabb, false);

                        addGeometryEntry(GeometryType_BoundingBox, aabb, *color, center, radius,
                                         layers, maskID, maskDepth, false, false);
                    }
                    if (drawCollider) {
                        addGeometryEntry(GeometryType_Collider, transform_get_and_retain_weakptr(child),
                                         *color, center, radius, layers, maskID,
                                         maskDepth, false, false);
                    }
                }
                break;
            }
        }
        if (drawSelf && transform_get_shadow_decal(child) > 0) {
            addGeometryEntry(GeometryType_Decals, transform_get_and_retain_weakptr(child), *color,
                            float3_zero, 0.0f, layers, maskID, maskDepth, false);
        }

        preDrawCollectData_recurse(child, hierarchyDirty || transform_is_hierarchy_dirty(child),
                                   hidden || transform_is_hidden_branch(child), sample,
                                   color, nextMaskID, childMaskID, childMaskDepth);

        transform_reset_hierarchy_dirty(child);

        n = doubly_linked_list_node_next(n);
    }
}

void GameRenderer::preDrawCollectData() {
    Scene *scene = game_get_scene(_game->getCGame());
    VERTEX_LIGHT_STRUCT_T sample; DEFAULT_LIGHT(sample)
    uint8_t nextMaskID = 1;
    _emulateShadows = true;
    _quadsBatchAllMaxCount = _quadsBatchTexturedMaxCount = 0;
    _maxPerspectiveViewOrder = 0;

    preDrawCollectData_recurse(scene_get_root(scene), transform_is_hierarchy_dirty(scene_get_root(scene)), false, true, &sample, &nextMaskID);

    if (_quadsBatchAllMaxCount > _quadsBatch.capacity) {
        if (_quadsBatch.vertices != nullptr) {
            free(_quadsBatch.vertices);
        }
        _quadsBatch.vertices = static_cast<QuadVertex*>(malloc(_quadsBatchAllMaxCount * VB_VERTICES * sizeof(QuadVertex)));
        _quadsBatch.capacity = _quadsBatch.vertices != nullptr ? _quadsBatchAllMaxCount : 0;
    }
}

void GameRenderer::preparePipelineForCameras() {
    _postprocessView1 = VIEW_SKYBOX + 1;
    _postprocessView2 = _postprocessView1 + 1;
    _afterpostView = _postprocessView2 + 1;
    _systemView = VIEW_UI_IMGUI(_afterpostView) + 1;

    DoublyLinkedListNode *n = nullptr;
    CameraEntry *ce = nullptr;
    for (auto it = _cameras.begin(); it != _cameras.end(); ++it) {
        n = doubly_linked_list_first(it->second);
        while (n != nullptr) {
            ce = static_cast<CameraEntry*>(doubly_linked_list_node_pointer(n));

            const bool isSystem = camera_get_layers(ce->camera) >= (1 << CAMERA_LAYERS_MASK_API_BITS);
            if (isSystem) {
                ce->segment = PipelineSegment_SystemTarget;
            } else {
                // Currently, all ortho targets assigned to after-post-process segment, unless its view order
                // was set by coder to be below another perspective camera
                const bool isOrtho = camera_get_mode(ce->camera) == Orthographic;
                ce->segment = isOrtho && camera_get_order(ce->camera) >= _maxPerspectiveViewOrder ?
                    PipelineSegment_AfterPostTarget : PipelineSegment_PostTarget;
            }

            if (ce->segment == PipelineSegment_PostTarget) {
                _postprocessView1 += TARGET_VIEWS_COUNT;
                _postprocessView2 = _postprocessView1 + 1;
                _afterpostView += TARGET_VIEWS_COUNT;
            } else if (ce->segment == PipelineSegment_AfterPostTarget) {
                _afterpostView += TARGET_VIEWS_COUNT;
            }
            _systemView += TARGET_VIEWS_COUNT;

            n = doubly_linked_list_node_next(n);
        }
    }
}

void GameRenderer::drawSphere(bgfx::ViewId id, const float3 *center, float radius, uint32_t depth, uint32_t stencil) {
    float mtx[16];
    bx::mtxSRT(mtx,
               radius, radius, radius,
               0.0f, 0.0f, 0.0f,
               center->x, center->y, center->z);

    bgfx::setStencil(stencil);

    drawStatic(id, _sphereVbh, _sphereIbh, _vertexColorProgram, getRenderState(0, RenderPassState_AlphaBlending), depth, mtx);
}

void GameRenderer::drawBox(bgfx::ViewId id, const Box *box, const float3 *rot, uint32_t abgr, uint32_t depth, uint32_t stencil) {
    float mtx[16];
    bx::mtxSRT(mtx,
               box->max.x - box->min.x,
               box->max.y - box->min.y,
               box->max.z - box->min.z,
               rot->x, rot->y, rot->z,
               box->min.x, box->min.y, box->min.z);

    drawCube(id, mtx, abgr, depth, stencil);
}

void GameRenderer::drawCube(bgfx::ViewId id, const float *mtx, uint32_t abgr, uint32_t depth, uint32_t stencil) {
    bgfx::setStencil(stencil);

    if (abgr != 0) {
        float colorData[4] = {
            uint8_t(abgr >> 24) / 255.0f,
            uint8_t(abgr >> 16) / 255.0f,
            uint8_t(abgr >> 8) / 255.0f,
            uint8_t(abgr >> 0) / 255.0f
        };
        bgfx::setUniform(_uColor1, colorData);

        drawStatic(id, _cubeVbh, _cubeIbh, _colorProgram, getRenderState(2, RenderPassState_Opacity), depth, mtx);
    } else {
        drawStatic(id, _cubeVbh, _cubeIbh, _vertexColorProgram, getRenderState(2, RenderPassState_Opacity), depth, mtx);
    }
}

bool GameRenderer::drawPass_Lights_cull(const Light *light, const float *viewProj, float projWidth,
                                        float projHeight, float4 *scissor) {

    Transform *t = light_get_transform(light);
    const float3 *pos = transform_get_position(t, false);
    const bx::Vec3 origin = {pos->x, pos->y, pos->z};
    const float range = light_get_range(light);
    const LightType type = light_get_type(light);

    scissor->x = scissor->y = scissor->z = scissor->w = -1.0f;

    // Cull and set scissors, except directional lights which are always a fullscreen pass
    if (type != LightType_Directional) {
        // Light sphere to world AABB
        bx::Vec3 min = bx::sub(origin, range);
        bx::Vec3 max = bx::add(origin, range);
        // TODO: shorten AABB for spot light, but not sure it's worth the calculation here

        // World AABB to clip space
        const bx::Vec3 box[8] = {
            {min.x, min.y, min.z},
            {min.x, min.y, max.z},
            {min.x, max.y, min.z},
            {min.x, max.y, max.z},
            {max.x, min.y, min.z},
            {max.x, min.y, max.z},
            {max.x, max.y, min.z},
            {max.x, max.y, max.z}
        };

        bx::Vec3 clip = bx::mulH(box[0], viewProj);
        min = max = clip;
        for (int i = 1; i < 8; ++i) {
            clip = bx::mulH(box[i], viewProj);
            min = bx::min(min, clip);
            max = bx::max(max, clip);
        }

        // Cull light if the whole clip AABB is outside frustum,
        if (max.z < 0.0f // near plane
            || min.z > 1.0f // far plane
            || max.x < -1.0f || min.x > 1.0f || max.y < -1.0f || min.y > 1.0f) { // frustum planes
            return false;
        }

        // No need to compute scissors if view is inside light volume
        if (min.z >= 0.0f && max.z <= 1.0f) {
            // NDC to unorm (z unused after this point)
            min = {min.x * 0.5f + 0.5f, min.y * 0.5f + 0.5f, 0.0f};
            max = {max.x * 0.5f + 0.5f, max.y * 0.5f + 0.5f, 0.0f};

            // Get corresponding screen rect for scissors
            const float x0 = CLAMP(min.x * projWidth, 0.0f, static_cast<float>(projWidth));
            const float y0 = CLAMP(min.y * projHeight, 0.0f, static_cast<float>(projHeight));
            const float x1 = CLAMP(max.x * projWidth, 0.0f, static_cast<float>(projWidth));
            const float y1 = CLAMP(max.y * projHeight, 0.0f, static_cast<float>(projHeight));
            const float height = y1 - y0;

            scissor->x = x0;
            scissor->y = _renderHeight - height - y0;
            scissor->z = x1 - x0;
            scissor->w = height;
        }
    }

    return true;
}

void GameRenderer::drawPass_Lights_shadowmap(bgfx::ViewId *shadowmaps, const Game_SharedPtr& game,
                                             const Light *light, float *invViewProj, float *out_lightViewProj,
                                             bool shadowCaster, std::unordered_map<GeometryType, DoublyLinkedList*> &geometry) {

    if (shadowCaster == false) {
        return;
    }

    Transform *t = light_get_transform(light);
    const LightType type = light_get_type(light);
    float lightView[16], lightProj[16];

    // Compute shadowmap view/proj based on light type
    switch(type) {
        case LightType_Point: {
            /*for (uint8_t csm = 0; csm < SHADOWMAP_CASCADES_COUNT; ++csm) {
                bgfx::setViewFrameBuffer(shadowmaps[csm], _shadowFbh[csm]);
                bgfx::touch(shadowmaps[csm]);
            }*/
            // TODO cubemap
            return;
            /*const float3 *pos = transform_get_position(t);
            bx::mtxTranslate(lightView, pos->x, pos->y, pos->z);

            const float radius = light_get_range(light);
            bx::mtxProj(lightProj, 90.0f, 1.0f, shadowmapNear, minimum(radius, shadowmapFar), _features.NDCDepth);
            break;*/
        }
        case LightType_Spot: {
            /*for (uint8_t csm = 0; csm < SHADOWMAP_CASCADES_COUNT; ++csm) {
                bgfx::setViewFrameBuffer(shadowmaps[csm], _shadowFbh[csm]);
                bgfx::touch(shadowmaps[csm]);
            }*/
            // TODO: split space if spot angle > 90°
            return; // temporarily disabled
            /*const float3 *pos = transform_get_position(t);
            float3 forward; transform_get_forward(t, &forward);
            const bx::Vec3 eye = { pos->x, pos->y, pos->z };
            bx::mtxLookAt(lightView, eye, { eye.x + forward.x, eye.y + forward.y, eye.z + forward.z });

            const float angle = light_get_angle(light);
            const float range = light_get_range(light);
            bx::mtxProj(lightProj, utils_rad2Deg(angle), 1.0f, shadowmapNear, minimum(range, shadowmapFar), _features.NDCDepth);
            break;*/
        }
        case LightType_Directional: {
            Plane *planes[6];
            const uint8_t cascades = SHADOWMAP_CASCADES_COUNT[_qualityParams.shadowsTier];
            const uint16_t distance = SHADOWMAP_DIRECTIONAL_DISTANCE[_qualityParams.shadowsTier];

            float3 forward; transform_get_forward(t, &forward, false);
            float3 up; transform_get_up(t, &up, false);
            bx::mtxLookAt(lightView, { 0.0f, 0.0f, 0.0f }, { forward.x, forward.y, forward.z }, { up.x, up.y, up.z });

            // Cascade sub-frustums
            float ndcSplits[SHADOWMAP_CASCADES_MAX * 2];
            splitFrustumNDC_Hardcoded(cascades, ndcSplits);

            for (uint8_t csm = 0; csm < cascades; ++csm) {
                // Sub-frustum world points
                float3 wcenter;
                float3 world[8]; getFrustumWorldPoints2(world,
                                                        ndcSplits[csm * 2],
                                                        ndcSplits[csm * 2 + 1],
                                                        invViewProj,
                                                        &wcenter);

#if SHADOWMAP_SPHERIZE
                // Spherize around world sub-frustum to stabilize shadowmaps
                float wradius = float3_sqr_distance(&wcenter, &world[0]);
                for (uint8_t i = 1; i < 8; ++i) {
                    wradius = maximum(wradius, float3_sqr_distance(&wcenter, &world[i]));
                }
                wradius = ceilf(sqrtf(wradius));

                // World sphere to light local space AABB
                bx::Vec3 lcenter = bx::mul({ wcenter.x, wcenter.y, wcenter.z }, lightView);
                bx::Vec3 min = { lcenter.x - wradius, lcenter.y - wradius, lcenter.z - wradius };
                bx::Vec3 max = { lcenter.x + wradius, lcenter.y + wradius, lcenter.z + wradius };
#else
                // World points to light local space AABB
                bx::Vec3 local = bx::mul({ world[0].x, world[0].y, world[0].z }, lightView);
                bx::Vec3 min = local;
                bx::Vec3 max = local;
                for (uint8_t i = 1; i < 8; ++i) {
                    local = bx::mul({ world[i].x, world[i].y, world[i].z }, lightView);
                    min = bx::min(min, local);
                    max = bx::max(max, local);
                }
#endif

#if SHADOWMAP_STABILIZE_TEXEL
                const float smSize = SHADOWMAP_CASCADES_SIZE_FACTORS[csm] * SHADOWMAP_MAX_SIZE[_qualityParams.shadowsTier];
                const float unitsPerTexelX = (max.x - min.x) / smSize;
                const float unitsPerTexelY = (max.y - min.y) / smSize;
                min.x = unitsPerTexelX * floorf(min.x / unitsPerTexelX);
                min.y = unitsPerTexelY * floorf(min.y / unitsPerTexelY);
                max.x = unitsPerTexelX * floorf(max.x / unitsPerTexelX);
                max.y = unitsPerTexelY * floorf(max.y / unitsPerTexelY);
#endif

                // Projection
                const float nearPlane = min.z - distance;
                const float farPlane = max.z;
                const float projWidth = max.x - min.x;
                const float projHeight = max.y - min.y;
                const float3 viewOffset = { min.x + projWidth * 0.5f, min.y + projHeight * 0.5f, 0.0f };

                bx::mtxOrtho(lightProj, min.x, max.x, min.y, max.y, nearPlane, farPlane, 0.0f, _features.NDCDepth);

                bx::mtxMul(out_lightViewProj + csm * 16, lightView, lightProj);

                // Frustum planes used for culling
                computeWorldPlanes(t, float3_zero, viewOffset, nearPlane, farPlane, 0.0f, false, projWidth, projHeight, planes);

                // Set shadowmap view/proj
                bgfx::setViewTransform(shadowmaps[csm], lightView, lightProj);

                // Bind current shadowmap
                bgfx::setViewFrameBuffer(shadowmaps[csm], _shadowFbh[csm]);

                // Reset shadowmap
                bgfx::touch(shadowmaps[csm]);

                drawPass_Geometry(shadowmaps[csm], _game, GeometryRenderPass_Shadowmap, lightView, nullptr, false, false, planes, geometry);

                for (int i = 0; i < 6; ++i) {
                    plane_free(planes[i]);
                    planes[i] = nullptr;
                }

#if DEBUG_RENDERER_STATS
                ++_debug_shadows_pass;
#endif
            }
            return;
        }
    }
}

void GameRenderer::drawPass_Lights_add(bgfx::ViewId id, const Light *light, const float *invViewProj,
                                       const float *lightViewProj, const Camera *camera,
                                       const float4 *scissor, bool shadowCaster, float uMax, float vMax) {

    Transform *t = light_get_transform(light);
    const float3 *pos = transform_get_position(t, false);
    const bx::Vec3 origin = { pos->x, pos->y, pos->z };
    const float range = light_get_range(light);
    const LightType type = light_get_type(light);
    const float intensity = light_get_intensity(light) >= 0 ? light_get_intensity(light) : _gameState->lightsIntensity;
    const float3 *cameraPos = camera_get_position(camera, false);

    // Draw w/ scissors if applicable
    if (scissor->x >= 0) {
        bgfx::setScissor(uint16_t(scissor->x), uint16_t(scissor->y), uint16_t(scissor->z), uint16_t(scissor->w));
    }

    const float3 *color = light_get_color(light);

    // Add contribution to ambient color
    if (type == LightType_Directional) {
        _lightsAmbient[0] += color->x * _gameState->lightsAmbientFactor;
        _lightsAmbient[1] += color->y * _gameState->lightsAmbientFactor;
        _lightsAmbient[2] += color->z * _gameState->lightsAmbientFactor;
        _faceShadingFactor -= intensity * rgb2luma(color->x, color->y, color->z);
    }

    // Packing light data
    _uLightParamsData[0] = origin.x;
    _uLightParamsData[1] = origin.y;
    _uLightParamsData[2] = origin.z;
    _uLightParamsData[3] = range;
    _uLightParamsData[4] = color->x;
    _uLightParamsData[5] = color->y;
    _uLightParamsData[6] = color->z;
    _uLightParamsData[7] = light_get_hardness(light);
    _uLightParamsData[8] = cameraPos->x;
    _uLightParamsData[9] = cameraPos->y;
    _uLightParamsData[10] = cameraPos->z;
    _uLightParamsData[11] = light_get_angle(light);
    if (type == LightType_Spot || type == LightType_Directional) {
        float3 forward; transform_get_forward(t, &forward, false);
        _uLightParamsData[12] = forward.x;
        _uLightParamsData[13] = forward.y;
        _uLightParamsData[14] = forward.z;
    } else {
        _uLightParamsData[12] = 0.0f;
        _uLightParamsData[13] = 0.0f;
        _uLightParamsData[14] = 0.0f;
    }
    _uLightParamsData[15] = intensity;
    _uLightParamsData[16] = _features.linearDepthGBuffer ? 0.0f : uMax;
    _uLightParamsData[17] = _features.linearDepthGBuffer ? 0.0f : vMax;
    _uLightParamsData[18] = 0.0f;
    _uLightParamsData[19] = 0.0f;
    bgfx::setUniform(_uLightParams, _uLightParamsData, 5);

    if (_features.linearDepthGBuffer == false) {
        // Inverse view-proj is used to reconstruct fragment world pos from depth
        bgfx::setUniform(_uMtx, invViewProj);
    }
    if (shadowCaster) {
        // Light view-proj is used to transform fragment world pos into light clip space
        if (type == LightType_Directional) {
            for (uint8_t i = 0; i < SHADOWMAP_CASCADES_COUNT[_qualityParams.shadowsTier]; ++i) {
                if (bgfx::isValid(_uLightVPMtx[i])) {
                    bgfx::setUniform(_uLightVPMtx[i], lightViewProj + i * 16);
                } else {
                    _uLightVPMtx[i] = bgfx::createUniform(("u_lightVP" + std::to_string(i + 1)).c_str(), bgfx::UniformType::Mat4);
                }
            }
        } else {
            bgfx::setUniform(_uLightVPMtx[0], lightViewProj);
        }

        // Packing shadow data
#if DEBUG_RENDERER
        _uShadowParamsData[0] = _debug_shadowBias > -1.0f ? _debug_shadowBias : SHADOWMAP_BIAS[_qualityParams.shadowsTier];
        _uShadowParamsData[1] = SHADOWMAP_MAX_SIZE[_qualityParams.shadowsTier];
        _uShadowParamsData[2] = _debug_shadowOffset > -1.0f ? _debug_shadowOffset : SHADOWMAP_NORMAL_OFFSET;
#else
        _uShadowParamsData[0] = SHADOWMAP_BIAS[_qualityParams.shadowsTier];
        _uShadowParamsData[1] = SHADOWMAP_MAX_SIZE[_qualityParams.shadowsTier];
        _uShadowParamsData[2] = SHADOWMAP_NORMAL_OFFSET;
#endif
        _uShadowParamsData[3] = minimum(camera_get_far(camera), SHADOWMAP_MAX_VIEW_DIST[_qualityParams.shadowsTier]);
        bgfx::setUniform(_uShadowParams, _uShadowParamsData, 1);
    }

    // Binding normal from g-buffer, depth buffer, and optionally shadowmap to samplers
    bgfx::setTexture(STAGE_LIGHT_NORMAL, _sFb[0], _opaqueFbTex[1]); // opaque pass normals
    bgfx::setTexture(STAGE_LIGHT_DEPTH, _sFb[1], _opaqueFbTex[_opaqueFb_depthIdx]); // opaque depth/linear depth
    if (_usePBR) {
        bgfx::setTexture(STAGE_LIGHT_PBR_OPAQUE, _sFb[2], _opaqueFbTex[0]); // opaque pass albedo
        bgfx::setTexture(STAGE_LIGHT_PBR_PARAMS, _sFb[3], _opaqueFbTex[4]); // opaque pass PBR parameters
    }
    if (shadowCaster) {
        const uint8_t stage = _usePBR ? STAGE_LIGHT_PBR_SHADOWMAP1 : STAGE_LIGHT_SHADOWMAP1;
        if (type == LightType_Directional) {
            for (uint8_t i = 0; i < SHADOWMAP_CASCADES_COUNT[_qualityParams.shadowsTier]; ++i) {
                bgfx::setTexture(stage + i, _sFb[stage + i], _shadowmapTh[i]);
            }
        } else {
            bgfx::setTexture(stage, _sFb[stage], _shadowmapTh[0]);
        }
    }

    // Set state for additive blending
    bgfx::setState(getRenderState(0, RenderPassState_Additive, TriangleCull_Back, PrimitiveType_Triangles, true));

    // Set screen quad blit output
    setScreenSpaceQuadTransient(0.0f, 0.0f, _framebufferWidth, _framebufferHeight, _framebufferWidth, _framebufferHeight,
                                uMax, vMax, _features.screenquadFlipV, invViewProj, cameraPos);

    // Drawcall
    switch(type) {
        case LightType_Point:
            bgfx::submit(id, shadowCaster ? _lightProgram_point_sm : _lightProgram_point, SORT_ORDER_DEFAULT);
            break;
        case LightType_Spot:
            bgfx::submit(id, shadowCaster ? _lightProgram_spot_sm : _lightProgram_spot, SORT_ORDER_DEFAULT);
            break;
        case LightType_Directional:
            bgfx::submit(id, shadowCaster ? _lightProgram_directional_sm : _lightProgram_directional, SORT_ORDER_DEFAULT);
            break;
    }

#if DEBUG_RENDERER_STATS
    ++_debug_lights_drawn;
#endif
}

bool GameRenderer::drawPass_Lights(bgfx::ViewId add, bgfx::ViewId *shadowmaps, const Game_SharedPtr& game,
                                   float *invView, float *viewProj, const Camera *camera,
                                   float projWidth, float projHeight, float uMax, float vMax,
                                   std::unordered_map<GeometryType, DoublyLinkedList*> &geometry) {

    const uint16_t viewDist = SHADOWMAP_MAX_VIEW_DIST[_qualityParams.shadowsTier];
    bool drawn = false, shadowCaster = false;
    uint64_t count = 0, shadowsCount = 0;
    DoublyLinkedListNode *n;
    const Light *light = nullptr;
    float invViewProj[16], lightViewProj[SHADOWMAP_CASCADES_MAX * 16], smInvViewProj[16];
    float4 scissor;
    const uint8_t layers = camera_get_layers(camera);

    bx::mtxInverse(invViewProj, viewProj);

    if (camera_get_far(camera) > viewDist) {
        float smProj[16], smViewProj[16];
        const Matrix4x4 *view = camera_get_view_matrix(camera);

        computeProjMtx(camera_get_mode(camera) == Orthographic, projWidth, projHeight,
                       _features.NDCDepth, view,
                       camera_get_near(camera), viewDist,
                       utils_rad2Deg(camera_utils_get_vertical_fov(camera, projWidth / projHeight)),
                       smProj);
        bx::mtxMul(smViewProj, reinterpret_cast<const float *>(view), smProj);

        bx::mtxInverse(smInvViewProj, smViewProj);
    } else {
        memcpy(smInvViewProj, invViewProj, 16 * sizeof(float));
    }

    // Draw lights in order of priority groups, default group being 255
    const uint8_t shadowcasters = SHADOWCASTERS_MAX[_qualityParams.shadowsTier];
    for(auto it = _lights.begin(); it != _lights.end(); ++it) {
        n = doubly_linked_list_first(it->second);
        while (n != nullptr) {
            light = static_cast<const Light*>(doubly_linked_list_node_pointer(n));
            if (count < LIGHTS_MAX &&
                camera_layers_match(light_get_layers(light), layers) &&
                drawPass_Lights_cull(light, viewProj, projWidth, projHeight, &scissor)) {

#if DEBUG_RENDERER_SHADOWS_FORCED_ON
                if (shadowsCount < shadowcasters && light_get_type(light) == LightType_Directional) {
#else
                if (shadowsCount < shadowcasters && light_is_shadow_caster(light)) {
#endif
                    // Allocated on first use, released when renderer stops
                    if (bgfx::isValid(_shadowFbh[0]) == false) {
                        _shadowmapsDirty = true;
                        shadowCaster = false;
                    } else {
                        shadowCaster = true;
                        ++shadowsCount;
                    }
                } else {
                    shadowCaster = false;
                }

                drawPass_Lights_shadowmap(shadowmaps, game, light, smInvViewProj, lightViewProj, shadowCaster, geometry);
                drawPass_Lights_add(add, light, invViewProj, lightViewProj, camera, &scissor, shadowCaster, uMax, vMax);

                drawn = true;
                ++count;
            }
            n = doubly_linked_list_node_next(n);
        }
    }

    return drawn;
}

bool GameRenderer::drawPass_Shape(bgfx::ViewId id, Transform *t, Shape *shape, VERTEX_LIGHT_STRUCT_T sample,
                                  GeometryRenderPass pass, Plane **planes, bool perChunkDraw,
                                  bool worldDirty, bool cameraDirty, float3 *wireframe, uint32_t depth, uint32_t stencil) {

    if (shape_is_hidden(shape) && wireframe == nullptr) {
        return false;
    }

    // Select vb chain based on render pass
    bool transparentPass;
    switch(pass) {
        case GeometryRenderPass_Opaque:
        case GeometryRenderPass_Shadowmap:
            transparentPass = false;
            break;
        case GeometryRenderPass_TransparentWeight:
        case GeometryRenderPass_TransparentFallback:
            transparentPass = true;
            break;
        case GeometryRenderPass_TransparentPost:
        case GeometryRenderPass_StencilWrite:
            return false;
    }

    bool drawn = false;

    // Baked vertex lighting,
    // - any shape may enable it, data is built into vertex attributes
    // - any shape inside another shape w/ baked lighting samples it at its position, mounted as uniform (only checks Map TODO: generic shape support)
    const bool usesBakedLighting = shape_uses_baked_lighting(shape);
    const bool isUnlit = shape_is_unlit(shape);

    // Check draw modes, they can bypass the pipeline with arbitrary effects
    const uint8_t legacyMode = shape_get_draw_mode_deprecated(shape);
    bool usesDrawModes = false;
    bool usesHiddenWireframe = false;
    resetOverrideParams();

    float metadata;
    if (pass != GeometryRenderPass_Shadowmap) {
        // Metadata packing,
        // - unlit (1 bit)
        // - uses baked lighting (1 bit)
        // - vertex lighting SRGB (4 bits each)
        metadata = static_cast<float>((isUnlit ? 1 : 0) +
                                      (usesBakedLighting ? 2 : 0) +
                                      sample.ambient * 4 + sample.red * 64 + sample.green * 1024 + sample.blue * 16384);

        if (shape_uses_drawmodes(shape)) {
            const RGBAColor multColor = uint32_to_color(shape_drawmode_get_multiplicative_color(shape));
            const RGBAColor addColor = uint32_to_color(shape_drawmode_get_additive_color(shape));

            if (multColor.a == 0) {
                return false;
            }

            _uOverrideParamsData[0] = multColor.a / 255.0f;
            _uOverrideParamsData[1] = multColor.r / 255.0f;
            _uOverrideParamsData[2] = multColor.g / 255.0f;
            _uOverrideParamsData[3] = multColor.b / 255.0f;
            _uOverrideParamsData[4] = addColor.r / 255.0f;
            _uOverrideParamsData[5] = addColor.g / 255.0f;
            _uOverrideParamsData[6] = addColor.b / 255.0f;

            usesDrawModes = true;
        }
        // use additive color override for a simple highlight effect
        if ((legacyMode & SHAPE_DRAWMODE_HIGHLIGHT) == SHAPE_DRAWMODE_HIGHLIGHT) {
            const float v = sin(_time * DRAWMODE_HIGHLIGHT_SPEED)* .75f + .25f;
            _uOverrideParamsData[4] = DRAWMODE_HIGHLIGHT_R * v;
            _uOverrideParamsData[5] = DRAWMODE_HIGHLIGHT_G * v;
            _uOverrideParamsData[6] = DRAWMODE_HIGHLIGHT_B * v;
            usesDrawModes = true;
        }
        // use mult. color override for a greyed out effect
        if ((legacyMode & SHAPE_DRAWMODE_GREY) == SHAPE_DRAWMODE_GREY) {
            _uOverrideParamsData[1] = DRAWMODE_GREY;
            _uOverrideParamsData[2] = DRAWMODE_GREY;
            _uOverrideParamsData[3] = DRAWMODE_GREY;
            usesDrawModes = true;
        }
        // activate shape-scaled grid display
        if ((legacyMode & SHAPE_DRAWMODE_GRID) == SHAPE_DRAWMODE_GRID || wireframe != nullptr) {
            float3 scale; matrix4x4_get_scaleXYZ(transform_get_ltw(t), &scale);
            _uOverrideParamsData_fs[0] = float3_length(&scale);
            _uOverrideParamsData_fs[1] = wireframe != nullptr ? wireframe->x : DRAWMODE_GRID_R;
            _uOverrideParamsData_fs[2] = wireframe != nullptr ? wireframe->y : DRAWMODE_GRID_G;
            _uOverrideParamsData_fs[3] = wireframe != nullptr ? wireframe->z : DRAWMODE_GRID_B;
            usesDrawModes = true;

            if (shape_is_hidden(shape)) {
                usesHiddenWireframe = true;
            }
        }
    } else {
        metadata = 0.0f;
    }
    // reroute the opaque buffers to be drawn in transparent pass if draw modes touches alpha
    const bool reroute = utils_rgba_get_alpha(shape_drawmode_get_multiplicative_color(shape)) < 1.0f;
    if (reroute || (legacyMode & SHAPE_DRAWMODE_ALL_TRANSPARENT) == SHAPE_DRAWMODE_ALL_TRANSPARENT || usesHiddenWireframe) {
        if (transparentPass) {
            if (reroute == false) {
                _uOverrideParamsData [0] = DRAWMODE_ALPHA_OVERRIDE;
            }
            usesDrawModes = true;

            if (_uOverrideParamsData[0] <= 0.0f) {
                return false;
            }

            drawn = shapeEntriesToDrawcalls(id, t, shape, false, usesDrawModes, metadata, pass, planes, perChunkDraw, worldDirty, cameraDirty, depth, stencil) || drawn;
        } else {
            return false;
        }
    }

    drawn = shapeEntriesToDrawcalls(id, t, shape, transparentPass, usesDrawModes, metadata, pass, planes, perChunkDraw, worldDirty, cameraDirty, depth, stencil) || drawn;

#if DEBUG_RENDERER_STATS_GAME
    if (drawn) {
        switch(pass) {
            case GeometryRenderPass_Opaque: ++_debug_shapes_opaque_drawn; break;
            case GeometryRenderPass_Shadowmap: ++_debug_shapes_shadowmap_drawn; break;
            case GeometryRenderPass_TransparentWeight:
            case GeometryRenderPass_TransparentFallback: ++_debug_shapes_transparent_drawn; break;
            default: break;
        }
    }
#endif

    return drawn;
}

bool GameRenderer::drawPass_Quad(bgfx::ViewId id, Quad *quad, VERTEX_LIGHT_STRUCT_T sample, GeometryRenderPass pass, float *view, float *invView, bool pointsScaling, uint32_t depth, uint32_t stencil) {
    bool isOpaque;
    const bool isVisible = quad_utils_get_visibility(quad, &isOpaque);
    const uint8_t quadOrder = quad_get_sort_order(quad);
    const bool alphaBlend = quad_uses_alpha_blending(quad) || quadOrder > 0;
    const float cutout = quad_get_cutout(quad);
    const BlendingMode blending = quad_drawmode_get_blending(quad);
    const BillboardMode billboard = quad_drawmode_get_billboard(quad);

    if (quad_get_data_size(quad) > 0 && cutout > 1.0f) {
        return false;
    }
    const bool hasCutout = cutout > 0.0f;

    bgfx::ProgramHandle program, program_tex;
    uint64_t state;
    switch(pass) {
        case GeometryRenderPass_Opaque:
            if (isOpaque) {
                program = _quadProgram;
                program_tex = hasCutout ? _quadProgram_tex_cutout : _quadProgram_tex;
                state = getRenderState(0, RenderPassState_Opacity, quad_is_doublesided(quad) ? TriangleCull_None : TriangleCull_Back,
                                       PrimitiveType_Triangles, quadOrder > 0);
            } else {
                return false;
            }
            break;
        case GeometryRenderPass_TransparentWeight:
            if (isOpaque || isVisible == false || alphaBlend || blending != BlendingMode_Default) {
                return false;
            } else {
                program = _quadProgram_weight;
                program_tex = hasCutout ? _quadProgram_tex_cutout_weight : _quadProgram_tex_weight;
                state = getRenderState(0, RenderPassState_AlphaWeight, quad_is_doublesided(quad) ? TriangleCull_None : TriangleCull_Back);
            }
            break;
        case GeometryRenderPass_Shadowmap:
            if (isOpaque == false || billboard != BillboardMode_None || blending != BlendingMode_Default) {
                return false;
            } else {
                program = _quadProgram_sm;
                program_tex = hasCutout ? _quadProgram_sm_tex_cutout : _quadProgram_sm;
                state = getRenderState(0, _features.isShadowSamplerEnabled() ? RenderPassState_DepthSampling : RenderPassState_DepthPacking,
                                          quad_is_doublesided(quad) ? TriangleCull_None : TriangleCull_Back);
            }
            break;
        case GeometryRenderPass_StencilWrite: {
            program = _quadProgram_stencil;
            program_tex = hasCutout ? _quadProgram_stencil_tex_cutout : _quadProgram_stencil;
            if (depth < SORT_ORDER_STENCIL) { // nested masks: don't use depth testing
                state = 0;
            } else { // new masks: use depth testing to support correct overlapping
                state = getRenderState(0, RenderPassState_DepthTesting, TriangleCull_None);
            }
            break;
        }
        case GeometryRenderPass_TransparentPost:
            if (alphaBlend == false && blending == BlendingMode_Default) {
                return false;
            }
        case GeometryRenderPass_TransparentFallback:
            if (isOpaque || isVisible == false) {
                return false;
            } else {
                if (blending == BlendingMode_Default) {
                    program = _quadProgram_alpha;
                    program_tex = hasCutout ? _quadProgram_tex_cutout_alpha : _quadProgram_tex_alpha;
                } else {
                    program = _quadProgram;
                    program_tex = hasCutout ? _quadProgram_tex_cutout : _quadProgram_tex;
                }
                RenderPassState rps;
                switch(blending) {
                    case BlendingMode_Additive:
                        rps = RenderPassState_Additive;
                        break;
                    case BlendingMode_Multiplicative:
                        rps = RenderPassState_Multiplicative;
                        break;
                    case BlendingMode_Subtractive:
                        rps = RenderPassState_Subtractive;
                        break;
                    default:
                        rps = RenderPassState_AlphaBlending;
                        break;
                }
                state = getRenderState(0, rps, quad_is_doublesided(quad) ? TriangleCull_None : TriangleCull_Back,
                                       PrimitiveType_Triangles, quadOrder > 0);
            }
            break;
    }

    const bool unlit = quad_is_unlit(quad) || blending != BlendingMode_Default;

    const bool batched =
        // no batching for quad masks
        pass != GeometryRenderPass_StencilWrite &&
        // no batching for masked quads TODO
        stencil == BGFX_STENCIL_DEFAULT &&
        // no batching for custom cutout thresholds (> 0)
        (hasCutout == false || float_isEqual(cutout, QUAD_CUTOUT_DEFAULT, EPSILON_ZERO)) &&
        // no batching in alpha-blending pass, except for special blending quads TODO: could batch when sortOrder is used
        (pass != GeometryRenderPass_TransparentPost || blending != BlendingMode_Default) &&
            // everything in one batch for shadows (textures, lit/unlit, etc. do not matter)
            (pass == GeometryRenderPass_Shadowmap ||
                // 9-slice batched if quad unlit or OIT ('normal' vertex attribute is used to pack 9-slice params)
             ((quad_uses_9slice(quad) && (unlit || pass == GeometryRenderPass_TransparentWeight)) || quad_uses_9slice(quad) == false));
    // TODO: non-batched additive quads support (mask, masked, custom cutout, 9-slice transparent)
    // TODO: non-batched billboard support (alpha + above)

    // select batch
    QuadEntry *entry = nullptr;
    QuadsBatch *batch = nullptr;
    if (quad_get_data_size(quad) > 0 && (hasCutout || (pass != GeometryRenderPass_Shadowmap && pass != GeometryRenderPass_StencilWrite))) {
        ensureQuadEntry(quad, &entry, batched, pointsScaling);
    }
    if (entry != nullptr) {
        if (bgfx::isValid(entry->tex.th) == false) {
            return false; // texture still loading
        } else if (entry->inactivity < 0) {
            return false; // texture grace frames
        }
    }
    const bool textured = entry != nullptr && bgfx::isValid(entry->tex.th);
    const bool use9Slice = textured && quad_uses_9slice(quad);
    const bool pack9SliceNormal = batched || pass == GeometryRenderPass_TransparentWeight;
    if (batched) {
        batch = textured ? &entry->batch : &_quadsBatch;
    }

    // metadata packing,
    // - unlit (1 bit)
    // - 9-slice parameters packed in normal? (1 bit)
    // - cutout? (1 bit)
    // - greyscale? (1 bit)
    // - vertex lighting SRGB (4 bits each) OR additive factor (8 bits)
    const float metadata = static_cast<float>((unlit ? 1 : 0) +
                                              (pack9SliceNormal ? 2 : 0) +
                                              (hasCutout ? 4 : 0) +
                                              (textured && entry->type == MediaType_Greyscale ? 8 : 0) +
                                              (unlit ? quad_drawmode_get_blending_factor(quad) * 16 :
                                                sample.ambient * 16 + sample.red * 256 +
                                                sample.green * 4096 + sample.blue * 65536));

    // UV coordinates (tiling/offset disabled w/ 9-slice)
    float u_zero, u_one, v_zero, v_one;
    if (textured) {
        const float u_size = entry->tex.u2 - entry->tex.u1;
        const float v_size = entry->tex.v2 - entry->tex.v1;

        u_zero = (use9Slice ? 0.0f : quad_get_offset_u(quad)) * u_size + entry->tex.u1;
        u_one = (use9Slice ? 1.0f : quad_get_tiling_u(quad) + quad_get_offset_u(quad)) * u_size + entry->tex.u1;
        v_zero = (use9Slice ? 0.0f : quad_get_offset_v(quad)) * v_size + entry->tex.v1;
        v_one = (use9Slice ? 1.0f : quad_get_tiling_v(quad) + quad_get_offset_v(quad)) * v_size + entry->tex.v1;
    } else {
        u_zero = v_zero = 0.0f;
        u_one = v_one = 1.0f;
    }

    // 9-slice parameters
    float uSlice, vSlice, uBorderMin, uBorderMax, vBorderMin, vBorderMax;
    if (use9Slice) {
        uSlice = quad_get_9slice_u(quad);
        vSlice = quad_get_9slice_v(quad);

        // automatic 9-slice corner size based on texture size
        if (quad_get_9slice_corner_width(quad) < 0.0f) {
            uBorderMin = CLAMP(entry->width * uSlice * quad_get_9slice_scale(quad),
                               0.0f, quad_get_width(quad) * uSlice) / quad_get_width(quad);
            vBorderMin = CLAMP(entry->height * vSlice * quad_get_9slice_scale(quad),
                               0.0f, quad_get_height(quad) * vSlice) / quad_get_height(quad);
            uBorderMax = 1.0f - CLAMP(entry->width * (1.0f - uSlice) * quad_get_9slice_scale(quad),
                                      0.0f, quad_get_width(quad) * (1.0f - uSlice)) / quad_get_width(quad);
            vBorderMax = 1.0f - CLAMP(entry->height * (1.0f - vSlice) * quad_get_9slice_scale(quad),
                                      0.0f, quad_get_height(quad) * (1.0f - vSlice)) / quad_get_height(quad);
        }
        // requested 9-slice corner size
        else {
            const float cornerWidth = quad_get_9slice_corner_width(quad);
            const float cornerHeight = cornerWidth * vSlice / uSlice;
            uBorderMin = CLAMP(cornerWidth * quad_get_9slice_scale(quad),
                               0.0f, quad_get_width(quad) * uSlice) / quad_get_width(quad);
            vBorderMin = CLAMP(cornerHeight * quad_get_9slice_scale(quad),
                               0.0f, quad_get_height(quad) * vSlice) / quad_get_height(quad);
            uBorderMax = 1.0f - uBorderMin;
            vBorderMax = 1.0f - vBorderMin;
        }
    } else {
        uSlice = vSlice = uBorderMin = uBorderMax = vBorderMin = vBorderMax = 0.5f;
    }

    // normal for lighting pass, or used to pack 9-slice parameters if batched (9-slice unlit) or transparent (OIT pass)
    const Matrix4x4 *ltw = transform_get_ltw(quad_get_transform(quad));
    float3 normal;
    if (pack9SliceNormal) {
        normal = {
            packNormalizedFloatsToFloat(uSlice, vSlice),
            packNormalizedFloatsToFloat(uBorderMin, uBorderMax),
            packNormalizedFloatsToFloat(vBorderMin, vBorderMax)
        };
    } else {
        normal = { -ltw->x3y1, -ltw->x3y2, -ltw->x3y3 };
        float3_normalize(&normal);
    }

    bool drawn = false;
    if (batch != nullptr && batch->vertices != nullptr) {
        const float3 pos = { ltw->x4y1, ltw->x4y2, ltw->x4y3 };

        float3 scaledRight, scaledUp;
        if (billboard == BillboardMode_None) {
            scaledRight = { ltw->x1y1, ltw->x1y2, ltw->x1y3 };
            scaledUp = { ltw->x2y1, ltw->x2y2, ltw->x2y3 };
        } else {
            const float scaleX = sqrtf(ltw->x1y1 * ltw->x1y1 + ltw->x1y2 * ltw->x1y2 + ltw->x1y3 * ltw->x1y3);
            const float scaleY = sqrtf(ltw->x2y1 * ltw->x2y1 + ltw->x2y2 * ltw->x2y2 + ltw->x2y3 * ltw->x2y3);

            // quad models face towards +Z, flip it to face camera
            const float3 viewRight = { -invView[0], -invView[1], -invView[2] };
            const float3 viewUp = { invView[4], invView[5], invView[6] };

            if (billboard == BillboardMode_Screen) {
                scaledRight = {
                    viewRight.x * scaleX,
                    viewRight.y * scaleX,
                    viewRight.z * scaleX
                };
                scaledUp = {
                    viewUp.x * scaleY,
                    viewUp.y * scaleY,
                    viewUp.z * scaleY
                };
            } else if (billboard == BillboardMode_Horizontal) {
                scaledRight = { ltw->x1y1, ltw->x1y2, ltw->x1y3 };

                const float3 v = { invView[12] - pos.x, invView[13] - pos.y, invView[14] - pos.z };
                float3 up = float3_cross_product3(&v, &scaledRight);
                float3_normalize(&up);
                scaledUp = { up.x * scaleY, up.y * scaleY, up.z * scaleY };
            } else if (billboard == BillboardMode_Vertical) {
                scaledUp = { ltw->x2y1, ltw->x2y2, ltw->x2y3 };

                const float3 v = { invView[12] - pos.x, invView[13] - pos.y, invView[14] - pos.z };
                float3 right = float3_cross_product3(&scaledUp, &v);
                float3_normalize(&right);
                scaledRight = { right.x * scaleX, right.y * scaleX, right.z * scaleX };
            } else {
                // extract quad rotation as 2D angle in XY plane (ScreenZ), XZ plane (ScreenY), or YZ plane (ScreenX)
                const float angle = billboard == BillboardMode_ScreenZ ? atan2f(ltw->x1y2, ltw->x1y1) :
                                    billboard == BillboardMode_ScreenY ? atan2f(ltw->x1y3, ltw->x1y1) :
                                    atan2f(ltw->x3y2, ltw->x2y2);
                const float cosTheta = cosf(angle);
                const float sinTheta = sinf(angle);

                // apply rotation to view vectors
                scaledRight = {
                    (viewRight.x * cosTheta + viewUp.x * -sinTheta) * scaleX,
                    (viewRight.y * cosTheta + viewUp.y * -sinTheta) * scaleX,
                    (viewRight.z * cosTheta + viewUp.z * -sinTheta) * scaleX
                };
                scaledUp = {
                    (viewRight.x * sinTheta + viewUp.x * cosTheta) * scaleY,
                    (viewRight.y * sinTheta + viewUp.y * cosTheta) * scaleY,
                    (viewRight.z * sinTheta + viewUp.z * cosTheta) * scaleY
                };
            }
        }

        const float hOffset = quad_get_anchor_x(quad) * quad_get_width(quad);
        const float vOffset = quad_get_anchor_y(quad) * quad_get_height(quad);

        const float3 wWidth = { scaledRight.x * quad_get_width(quad),
                                scaledRight.y * quad_get_width(quad),
                                scaledRight.z * quad_get_width(quad) };
        const float3 wHeight = { scaledUp.x * quad_get_height(quad),
                                 scaledUp.y * quad_get_height(quad),
                                 scaledUp.z * quad_get_height(quad) };
        const float3 wOrigin = { pos.x - (scaledRight.x * hOffset + scaledUp.x * vOffset),
                                 pos.y - (scaledRight.y * hOffset + scaledUp.y * vOffset),
                                 pos.z - (scaledRight.z * hOffset + scaledUp.z * vOffset) };

        const uint32_t idx = batch->count * 4;
        batch->vertices[idx] =     { wOrigin.x, wOrigin.y, wOrigin.z, metadata,
                                     normal.x, normal.y, normal.z,
                                     quad_get_vertex_color(quad, 0), u_zero, v_one };
        batch->vertices[idx + 1] = { wOrigin.x + wWidth.x, wOrigin.y + wWidth.y, wOrigin.z + wWidth.z, metadata,
                                     normal.x, normal.y, normal.z,
                                     quad_get_vertex_color(quad, 1), u_one, v_one };
        batch->vertices[idx + 2] = { wOrigin.x + wWidth.x + wHeight.x, wOrigin.y + wWidth.y + wHeight.y, wOrigin.z + wWidth.z + wHeight.z, metadata,
                                     normal.x, normal.y, normal.z,
                                     quad_get_vertex_color(quad, 2), u_one, v_zero };
        batch->vertices[idx + 3] = { wOrigin.x + wHeight.x, wOrigin.y + wHeight.y, wOrigin.z + wHeight.z, metadata,
                                     normal.x, normal.y, normal.z,
                                     quad_get_vertex_color(quad, 3), u_zero, v_zero };

        if (batch->count == 0 || hasCutout) {
            batch->program = textured ? program_tex : program;
            batch->state = state;
        }
        ++batch->count;
    } else if (bgfx::getAvailTransientVertexBuffer(4, QuadVertex::_quadVertexLayout) == 4) {
        bgfx::TransientVertexBuffer tvb;
        bgfx::allocTransientVertexBuffer(&tvb, 4, QuadVertex::_quadVertexLayout);
        QuadVertex *vertices = reinterpret_cast<QuadVertex*>(tvb.data);

        const float hOffset = quad_get_anchor_x(quad) * quad_get_width(quad);
        const float vOffset = quad_get_anchor_y(quad) * quad_get_height(quad);

        vertices[0] = { -hOffset, -vOffset, 0.0f, metadata,
                        normal.x, normal.y, normal.z,
                        quad_get_vertex_color(quad, 0), u_zero, v_one };
        vertices[1] = { quad_get_width(quad) - hOffset, -vOffset, 0.0f, metadata,
                        normal.x, normal.y, normal.z,
                        quad_get_vertex_color(quad, 1), u_one, v_one };
        vertices[2] = { quad_get_width(quad) - hOffset, quad_get_height(quad) - vOffset, 0.0f, metadata,
                        normal.x, normal.y, normal.z,
                        quad_get_vertex_color(quad, 2), u_one, v_zero };
        vertices[3] = { -hOffset, quad_get_height(quad) - vOffset, 0.0f, metadata,
                        normal.x, normal.y, normal.z,
                        quad_get_vertex_color(quad, 3), u_zero, v_zero };

        // select drawcall order for transparent passes
        uint32_t sortOrder = depth;
        if (pass == GeometryRenderPass_TransparentPost || pass == GeometryRenderPass_TransparentFallback) {
            if (quadOrder > 0) {
                sortOrder -= quadOrder;
            } else {
                // world space to view space
                const float3 *pos = transform_get_position(quad_get_transform(quad), false);
                float world[4] = { pos->x, pos->y, pos->z, 1.0f };
                float viewPos[4];
                bx::vec4MulMtx(viewPos, world, view);

                // sort order based on view space depth delta
                sortOrder += static_cast<uint32_t>(viewPos[2] / TRANSPARENT_SORT_ORDER_DELTA);
            }
        }

        if (textured) {
            bgfx::setTexture(STAGE_QUAD_TEX, _sFb[0], entry->tex.th);
        }

        if (pack9SliceNormal == false) {
            float paramsData[4];
            paramsData[0] = uSlice;
            paramsData[1] = vSlice;
            paramsData[2] = uBorderMin;
            paramsData[3] = uBorderMax;
            bgfx::setUniform(_uParams, paramsData);
        }
        if (pack9SliceNormal == false || (textured && hasCutout)) {
            float paramsData[4];
            paramsData[0] = vBorderMin;
            paramsData[1] = vBorderMax;
            paramsData[2] = minimum(cutout, 1.0f - EPSILON_ZERO);
            paramsData[3] = 0.0f;
            bgfx::setUniform(_uColor1, paramsData);
        }

        bgfx::setTransform(ltw);

        bgfx::setStencil(stencil);

        bgfx::setVertexBuffer(0, &tvb);
        bgfx::setIndexBuffer(_voxelIbh, 0, IB_INDICES);

        bgfx::setState(state);
        bgfx::submit(id, textured ? program_tex : program, sortOrder);

        drawn = true;
    }

#if DEBUG_RENDERER_STATS_GAME
    if (drawn || batched) {
        switch(pass) {
            case GeometryRenderPass_Opaque: ++_debug_quads_opaque_drawn; break;
            case GeometryRenderPass_Shadowmap: ++_debug_quads_shadowmap_drawn; break;
            case GeometryRenderPass_TransparentWeight:
            case GeometryRenderPass_TransparentFallback: ++_debug_quads_transparent_drawn; break;
            case GeometryRenderPass_StencilWrite: ++_debug_quads_stencil_drawn; break;
            default: break;
        }
    }
#endif

    return drawn;
}

bool GameRenderer::drawPass_QuadBatch(bgfx::ViewId id, QuadsBatch *batch, bgfx::TextureHandle th, uint32_t depth) {
    if (batch->count == 0) {
        return false;
    }

    if (bgfx::isValid(batch->vb)) {
        bgfx::destroy(batch->vb);
        batch->vb = BGFX_INVALID_HANDLE;
    }

    const uint32_t batchVertexCount = batch->count * VB_VERTICES;
    if (bgfx::getAvailTransientVertexBuffer(batchVertexCount, QuadVertex::_quadVertexLayout) == batchVertexCount) {
        bgfx::TransientVertexBuffer tvb;
        bgfx::allocTransientVertexBuffer(&tvb, batchVertexCount, QuadVertex::_quadVertexLayout);
        memcpy(tvb.data, batch->vertices, batchVertexCount * sizeof(QuadVertex));

        bgfx::setVertexBuffer(0, &tvb);
    } else {
        batch->vb = bgfx::createVertexBuffer(bgfx::copy(batch->vertices, batchVertexCount * sizeof(QuadVertex)), QuadVertex::_quadVertexLayout);

        bgfx::setVertexBuffer(0, batch->vb);
    }
    bgfx::setIndexBuffer(_voxelIbh, 0, batch->count * IB_INDICES);

    if (bgfx::isValid(th)) {
        bgfx::setTexture(STAGE_QUAD_TEX, _sFb[0], th);
    }

    // 9-slice parameters are packed into vertex attributes when used w/ batching (unlit quads),
    // here, set default parameters for other configurations (batched lit quads)
    float paramsData[8];
    for (uint8_t i = 0; i < 6; ++i) {
        paramsData[i] = 0.5f;
    }
    paramsData[6] = minimum(QUAD_CUTOUT_DEFAULT, 1.0f - EPSILON_ZERO);
    paramsData[7] = 0.0f;
    bgfx::setUniform(_uParams, paramsData);
    bgfx::setUniform(_uColor1, paramsData + 4);

    bgfx::setState(batch->state);
    bgfx::submit(id, batch->program, depth);

    return true;
}

bool GameRenderer::drawPass_Mesh(bgfx::ViewId id, Mesh *mesh, VERTEX_LIGHT_STRUCT_T sample, GeometryRenderPass pass, uint32_t depth, uint32_t stencil) {
    // TODO: dynamic mesh

    const Material *mat = mesh_get_material(mesh);

    if ((material_is_opaque(mat) && pass != GeometryRenderPass_Opaque && pass != GeometryRenderPass_Shadowmap) ||
        (material_is_opaque(mat) == false && pass != GeometryRenderPass_TransparentFallback && pass != GeometryRenderPass_TransparentPost)) {
        return false;
    }

    MeshEntry *entry;
    if (ensureMeshEntry(mesh, &entry) == false) {
        return false;
    }
    vx_assert_d(entry != nullptr);

    const float cutout = material_get_alpha_cutout(mat);
    const TriangleCull cull = material_is_double_sided(mat) ? TriangleCull_None :
                              mesh_is_front_ccw(mesh) ? TriangleCull_Back : TriangleCull_Front;

    uint64_t state;
    bgfx::ProgramHandle program;
    switch(pass) {
        case GeometryRenderPass_Opaque:
            if (cutout >= 1.0f) {
                return false;
            }
            state = getRenderState(0, RenderPassState_Opacity, cull, mesh_get_primitive_type(mesh));
            program = cutout > 0 ? _meshProgram_cutout : _meshProgram;
            break;
        case GeometryRenderPass_TransparentWeight:
            return false;
        case GeometryRenderPass_TransparentFallback:
        case GeometryRenderPass_TransparentPost:
            state = getRenderState(0, RenderPassState_AlphaBlending, cull, mesh_get_primitive_type(mesh));
            program = _meshProgram_alpha;
            break;
        case GeometryRenderPass_Shadowmap:
            state = getRenderState(0, _features.isShadowSamplerEnabled() ?
                                      RenderPassState_DepthSampling : RenderPassState_DepthPacking,
                                   cull, mesh_get_primitive_type(mesh));
            program = _meshProgram_sm;
            break;
        case GeometryRenderPass_StencilWrite:
            return false;
    }

    TextureEntry *textures[MaterialTexture_Count];
    for (uint8_t i = 0; i < MaterialTexture_Count; ++i) {
        if ((i == MaterialTexture_Metallic && _usePBR || i != MaterialTexture_Metallic) &&
            ensureTextureEntry(material_get_texture(mat, static_cast<MaterialTexture>(i)), MESH_TEX_MAX_SIZE, &textures[i])) {

            if (bgfx::isValid(textures[i]->th) == false) {
                return false; // texture still loading
            } else if (textures[i]->inactivity < 0) {
                return false; // texture grace frames
            }
        } else {
            textures[i] = nullptr;
        }
    }

    if (bgfx::isValid(entry->static_vbh)) {
        bgfx::setVertexBuffer(0, entry->static_vbh);
    } else if (bgfx::isValid(entry->dynamic_vbh)) {
        bgfx::setVertexBuffer(0, entry->dynamic_vbh);
    } else {
        return false;
    }

    if (bgfx::isValid(entry->static_ibh)) {
        bgfx::setIndexBuffer(entry->static_ibh);
    } else if (bgfx::isValid(entry->dynamic_ibh)) {
        bgfx::setIndexBuffer(entry->dynamic_ibh);
    }

    // Metadata packing,
    // - has albedo tex (1 bit)
    // - has normal map (1 bit)
    // - has emissive tex (1 bit)
    // - has metallic-roughness map (1 bit)
    // - metallic factor (8 bits, normalized float)
    // - roughness factor (8 bits, normalized float)
    const bool useAlbedoTex = textures[MaterialTexture_Albedo] != nullptr;
    const bool useNormalMap = textures[MaterialTexture_Normal] != nullptr;
    const bool useMetallicMap = textures[MaterialTexture_Metallic] != nullptr;
    const bool useEmissiveTex = textures[MaterialTexture_Emissive] != nullptr;
    const uint32_t metadata = useAlbedoTex + useNormalMap * 2 + useEmissiveTex * 4 +
                              (_usePBR ? useMetallicMap * 8 +
                                         floorf(material_get_metallic(mat) * 255) * 16 +
                                         floorf(material_get_roughness(mat) * 255) * 4096 : 0);
    // - unlit (1 bit)
    // - vertex lighting SRGB (4 bits each)
    const uint32_t metadata2 = (material_is_unlit(mat) ? 1 : 0) +
                               sample.ambient * 2 + sample.red * 32 + sample.green * 512 + sample.blue * 8192;

    if (useAlbedoTex) {
        bgfx::setTexture(STAGE_MESH_ALBEDO, _sFb[0], textures[MaterialTexture_Albedo]->th);
    } else {
        bgfx::setTexture(STAGE_MESH_ALBEDO, _sFb[0], _dummyTh);
    }
    if (_features.useDeferred()) {
        if (useNormalMap) {
            bgfx::setTexture(STAGE_MESH_NORMAL, _sFb[1], textures[MaterialTexture_Normal]->th);
        } else {
            bgfx::setTexture(STAGE_MESH_NORMAL, _sFb[1], _dummyTh);
        }
        if (useMetallicMap && _usePBR) {
            bgfx::setTexture(STAGE_MESH_METALLIC, _sFb[2], textures[MaterialTexture_Metallic]->th);
        } else {
            bgfx::setTexture(STAGE_MESH_METALLIC, _sFb[2], _dummyTh);
        }
        if (useEmissiveTex) {
            bgfx::setTexture(STAGE_MESH_EMISSIVE, _sFb[3], textures[MaterialTexture_Emissive]->th);
        } else {
            bgfx::setTexture(STAGE_MESH_EMISSIVE, _sFb[3], _dummyTh);
        }
    }

    float colorData[4]; utils_rgba_to_float(material_get_albedo(mat), colorData);
    bgfx::setUniform(_uColor1, colorData);

    _uParamsData[0] = static_cast<float>(metadata);
    _uParamsData[1] = static_cast<float>(material_get_emissive(mat));
    _uParamsData[2] = cutout;
    _uParamsData[3] = static_cast<float>(metadata2);
    bgfx::setUniform(_uParams, _uParamsData);

    Matrix4x4 model; transform_utils_get_model_ltw(mesh_get_transform(mesh), &model);
    bgfx::setTransform(&model);

    bgfx::setStencil(stencil);
    bgfx::setState(state);
    bgfx::submit(id, program, depth);

#if DEBUG_RENDERER_STATS_GAME
    switch(pass) {
        case GeometryRenderPass_Opaque: ++_debug_meshes_opaque_drawn; break;
        case GeometryRenderPass_Shadowmap: ++_debug_meshes_shadowmap_drawn; break;
        case GeometryRenderPass_TransparentWeight:
        case GeometryRenderPass_TransparentFallback: ++_debug_meshes_transparent_drawn; break;
        default: break;
    }
#endif

    return true;
}

bool GameRenderer::drawPass_Geometry(bgfx::ViewId id, const Game_SharedPtr& game, GeometryRenderPass pass, float *view,
                                     float *invView, bool isBackdrop, bool pointsScaling, Plane **planes,
                                     std::unordered_map<GeometryType, DoublyLinkedList*> &geometry) {

#if DEBUG_RENDERER_STATS_GAME
    #define INC_CULLED if (entry->type == GeometryType_Shape) ++_debug_shapes_culled; \
        else if (entry->type == GeometryType_Quad) ++_debug_quads_culled;             \
        else if (entry->type == GeometryType_Mesh) ++_debug_meshes_culled;
#else
    #define INC_CULLED
#endif

    bool drawn = false;
    _quadsBatch.count = 0;
    for (auto itr = _quadEntries.begin(); itr != _quadEntries.end(); itr++) {
        itr->second.batch.count = 0;
    }

    // Draw geometry by type to reduce render state swaps
    for (auto itr = geometry.begin(); itr != geometry.end(); itr++) {
        const GeometryType type = itr->first;

        // Select geometry types based on pass, set any shared render state elements
        switch(type) {
            case GeometryType_Shape: {
                if (pass == GeometryRenderPass_StencilWrite || pass == GeometryRenderPass_TransparentPost) {
                    continue;
                }
                break;
            }
            case GeometryType_WorldText:
                if (pass == GeometryRenderPass_Shadowmap || pass == GeometryRenderPass_StencilWrite) {
                    continue;
                }
                break;
            case GeometryType_Collider:
                if (pass == GeometryRenderPass_Shadowmap || pass == GeometryRenderPass_StencilWrite || pass == GeometryRenderPass_TransparentPost) {
                    continue;
                }
                break;
            case GeometryType_BoundingBox:
                if (pass == GeometryRenderPass_Shadowmap || pass == GeometryRenderPass_StencilWrite || pass == GeometryRenderPass_TransparentPost) {
                    continue;
                }
                break;
            case GeometryType_ModelBox:
                if (pass == GeometryRenderPass_Shadowmap || pass == GeometryRenderPass_StencilWrite || pass == GeometryRenderPass_TransparentPost) {
                    continue;
                }
                break;
            case GeometryType_Quad:
                break;
            case GeometryType_Decals: {
                if (pass == GeometryRenderPass_Shadowmap || pass == GeometryRenderPass_StencilWrite || pass == GeometryRenderPass_TransparentPost) {
                    continue;
                }
                break;
            }
            case GeometryType_Mesh: {
                if (pass == GeometryRenderPass_StencilWrite || pass == GeometryRenderPass_TransparentWeight) {
                    continue;
                }
                break;
            }
        }

        DoublyLinkedListNode *n = doubly_linked_list_first(itr->second);
        GeometryEntry *entry;
        Transform *t;
        while (n != nullptr) {
            entry = static_cast<GeometryEntry*>(doubly_linked_list_node_pointer(n));

            // Check weakptr, remove entry if invalid
            if (type != GeometryType_BoundingBox && type != GeometryType_ModelBox) {
                t = static_cast<Transform *>(weakptr_get(static_cast<Weakptr *>(entry->ptr)));
                if (t == nullptr) {
                    INC_CULLED
                    n = doubly_linked_list_node_next(n);
                    continue;
                }
            } else {
                t = nullptr;
            }

            // Already culled by previous pass
            if (pass != GeometryRenderPass_Shadowmap && entry->geometryCulled) {
                INC_CULLED
                n = doubly_linked_list_node_next(n);
                continue;
            }

            const bool drawCollider = _gameState->debug_displayColliders || (t != nullptr && debug_transform_display_collider(t));

            // Early skips,
            // (1) hidden object, except if displaying colliders for per-block shapes
            // (2) shadowmap pass, if shadowcasting is disabled for the object
            // (3) stencil pass, if not a mask
            if ((t != nullptr && transform_is_hidden(t) && (drawCollider == false || type != GeometryType_Shape)) || // (1)
                (pass == GeometryRenderPass_Shadowmap && transform_utils_has_shadow(t) == false) || // (2)
                (pass == GeometryRenderPass_StencilWrite && entry->stencilWrite == false)) { // (3)

                INC_CULLED
                n = doubly_linked_list_node_next(n);
                continue;
            }

            // Bounding sphere culling if applicable
            GeometryCulling culling = GeometryCulling_None;
            if (entry->radius > 0) {
                for (uint8_t i = 0; i < 6; ++i) {
                    const int intersect = plane_intersect_sphere(planes[i], &entry->center, entry->radius);
                    if (intersect < 0) {
                        culling = GeometryCulling_Culled;
                        break;
                    } else if (intersect == 0) {
                        culling = GeometryCulling_Intersect;
                    }
                }
                if (culling == GeometryCulling_Culled) {
                    if (pass != GeometryRenderPass_Shadowmap) {
                        entry->geometryCulled = true;
                    }
                    INC_CULLED
                    n = doubly_linked_list_node_next(n);
                    continue;
                }
            }

            // Set stencil used for masking
            uint32_t stencil, sortOrder;
    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wold-style-cast"
            if (pass == GeometryRenderPass_StencilWrite) {
                if (entry->maskDepth == 0) {
                    // new mask
                    const uint8_t ref = uint8_t(CLAMP(entry->maskID, 0, 31) << 3);
                    stencil = BGFX_STENCIL_TEST_ALWAYS |
                              BGFX_STENCIL_FUNC_REF(ref) |
                              BGFX_STENCIL_FUNC_RMASK(0xff) |
                              BGFX_STENCIL_OP_FAIL_S_KEEP |
                              BGFX_STENCIL_OP_FAIL_Z_KEEP |
                              BGFX_STENCIL_OP_PASS_Z_REPLACE;
                } else {
                    // nested mask: test parent mask value, incr mask depth (lower 3 bits)
                    const uint8_t ref = uint8_t(CLAMP(entry->maskID, 0, 31) << 3) + CLAMP(entry->maskDepth - 1, 0, 7);
                    stencil = BGFX_STENCIL_TEST_EQUAL |
                              BGFX_STENCIL_FUNC_REF(ref) |
                              BGFX_STENCIL_FUNC_RMASK(0xff) |
                              BGFX_STENCIL_OP_FAIL_S_KEEP |
                              BGFX_STENCIL_OP_FAIL_Z_KEEP |
                              BGFX_STENCIL_OP_PASS_Z_INCRSAT;
                }
                sortOrder = SORT_ORDER_STENCIL - entry->maskDepth;
            } else if (entry->maskID > 0) {
                // masked object
                const uint8_t ref = uint8_t(CLAMP(entry->maskID, 0, 31) << 3) + CLAMP(entry->maskDepth, 0, 7);
                stencil = BGFX_STENCIL_TEST_LEQUAL |
                          BGFX_STENCIL_FUNC_REF(ref) |
                          BGFX_STENCIL_FUNC_RMASK(0xff) |
                          BGFX_STENCIL_OP_FAIL_S_KEEP |
                          BGFX_STENCIL_OP_FAIL_Z_KEEP |
                          BGFX_STENCIL_OP_PASS_Z_KEEP;
                sortOrder = SORT_ORDER_DEFAULT;
            } else {
                stencil = BGFX_STENCIL_DEFAULT;
                sortOrder = SORT_ORDER_DEFAULT;
            }
    #pragma GCC diagnostic pop

            // Drawcall
            switch(type) {
                case GeometryType_Shape: {
                    RigidBody *rb = transform_get_rigidbody(t);
                    Shape *s = static_cast<Shape *>(transform_get_ptr(t));

                    shape_refresh_vertices(s);

                    const bool perChunkDraw = culling == GeometryCulling_Intersect;
                    if (pass == GeometryRenderPass_Shadowmap) {
                        drawn = drawPass_Shape(id, t, s, entry->sampleColor, pass, planes, perChunkDraw,
                                               entry->worldDirty, entry->cameraDirty, nullptr, sortOrder, stencil) || drawn;
                    } else {
                        // static/trigger per-block rigidbodies as blue/green wireframe
                        const bool wireframe = drawCollider && rigidbody_uses_per_block_collisions(rb);
                        float3 wireframeColor;
                        if (wireframe) {
                            if (rigidbody_get_simulation_mode(rb) == RigidbodyMode_TriggerPerBlock) {
                                float3_set(&wireframeColor, 0.0f, 1.0f, 0.0f);
                            } else {
                                float3_set(&wireframeColor, 0.0f, 0.0f, 1.0f);
                            }
                        }

                        drawn = drawPass_Shape(id, t, s, entry->sampleColor, pass, planes, perChunkDraw,
                                               entry->worldDirty, entry->cameraDirty,
                                               wireframe ? &wireframeColor : nullptr, sortOrder, stencil) || drawn;
                    }
                    entry->worldDirty = false;
                    entry->cameraDirty = false;
                    break;
                }
                case GeometryType_WorldText: {
                    WorldText *wt = static_cast<WorldText *>(transform_get_ptr(t));

                    drawn = _worldText->draw(id, VIEW_UI_OVERLAY(_afterpostView), wt, &_overlayDrawn, pass, entry->sampleColor, _uParams, stencil) || drawn;
                    break;
                }
                case GeometryType_BoundingBox: {
                    if (pass == GeometryRenderPass_Opaque) {
                        Box *aabb = static_cast<Box *>(entry->ptr);

                        drawBox(id, aabb, &float3_zero, 0, sortOrder + 1, stencil);
                        drawn = true;
                    }
                    break;
                }
                case GeometryType_ModelBox: {
                    if (pass == GeometryRenderPass_Opaque) {
                        Matrix4x4 *mtx = static_cast<Matrix4x4 *>(entry->ptr);

                        drawCube(id, reinterpret_cast<const float *>(mtx), 0x000000ff, sortOrder + 1, stencil);
                        drawn = true;
                    }
                    break;
                }
                case GeometryType_Collider: {
                    if (pass == GeometryRenderPass_Opaque) {
                        const RigidBody *rb = transform_get_rigidbody(t);
                        Box collider;

                        if (rigidbody_is_enabled(rb) && rigidbody_is_collider_valid(rb)) {
                            // dynamic rigidbodies as red box
                            if (rigidbody_is_dynamic(rb)) {
                                transform_get_or_compute_world_aligned_collider(t, &collider, false);

                                drawBox(id, &collider, &float3_zero, 0xff0000ff, sortOrder + 1, stencil);
                            }
                            // box static/trigger rigidbodies as blue and green boxes,
                            // per-block displayed as wireframe via drawmode
                            else if (transform_get_type(t) != ShapeTransform || rigidbody_uses_per_block_collisions(rb) == false) {
                                Matrix4x4 model; transform_utils_get_model_ltw(t, &model);
                                collider = *rigidbody_get_collider(rb);
                                Matrix4x4 mtx = matrix4x4_identity;

                                float3 offset = collider.min;
                                if (float3_isZero(&offset, EPSILON_ZERO) == false) {
                                    matrix4x4_set_translation(&mtx, offset.x, offset.y, offset.z);
                                    matrix4x4_op_multiply(&model, &mtx);
                                }

                                matrix4x4_set_scaleXYZ(&mtx,
                                                       collider.max.x - collider.min.x,
                                                       collider.max.y - collider.min.y,
                                                       collider.max.z - collider.min.z);
                                matrix4x4_op_multiply_2(&model, &mtx);

                                drawCube(id, reinterpret_cast<const float *>(&mtx),
                                         rigidbody_get_simulation_mode(rb) == RigidbodyMode_Trigger ? 0x00ff00ff : 0x0000ffff,
                                         sortOrder + 1, stencil);
                            }
                        }
                    }
                    break;
                }
                case GeometryType_Quad: {
                    Quad *q = static_cast<Quad *>(transform_get_ptr(t));

                    drawn = drawPass_Quad(id, q, entry->sampleColor, pass, view, invView, pointsScaling, sortOrder, stencil) || drawn;
                    break;
                }
                case GeometryType_Decals: {
                    if (_emulateShadows && (pass == GeometryRenderPass_TransparentWeight || pass == GeometryRenderPass_TransparentFallback)) {
                        const uint16_t decalID = transform_get_id(t);
                        const float shadowCookieSize = transform_get_shadow_decal(t);
                        requestShadowDecal(decalID, transform_get_position(t, false),
                                           SHADOW_COOKIE_SIZE_OFFSET + shadowCookieSize,
                                           SHADOW_COOKIE_SIZE_OFFSET + shadowCookieSize,
                                           t, stencil);
                        _shadowDecals[decalID] = true;
                    }
                    break;
                }
                case GeometryType_Mesh: {
                    Mesh *m = static_cast<Mesh *>(transform_get_ptr(t));

                    drawn = drawPass_Mesh(id, m, entry->sampleColor, pass, sortOrder, stencil) || drawn;
                    break;
                }
            }

            n = doubly_linked_list_node_next(n);
        }

        if (drawn == false && type == GeometryType_Shape) {
            // flush shared shape parameters if no shapes were drawn
            bgfx::touch(id);
        }
    }

    // Quads batches
    drawn = drawPass_QuadBatch(id, &_quadsBatch) || drawn;
    for (auto itr = _quadEntries.begin(); itr != _quadEntries.end(); itr++) {
        drawn = drawPass_QuadBatch(id, &itr->second.batch, itr->second.tex.th) || drawn;
    }

    // Per-pass draw only TODO: move these to separate functions
    if (pass == GeometryRenderPass_Opaque) {
        if (isBackdrop) { // TODO add a Clouds.Layers
            drawn = _sky->draw(id, SORT_ORDER_DEFAULT, _time) || drawn;
        }
    }
    if (pass == GeometryRenderPass_TransparentWeight || pass == GeometryRenderPass_TransparentFallback) {
        drawn = _decals->draw(id, SORT_ORDER_DECALS) || drawn;
    }

    return drawn;
}

void GameRenderer::drawSkybox(bgfx::ViewId id, Camera *camera, float width, float height, SkyboxMode mode) {
    if (_gameState->skyboxMode == SkyboxMode_Clear) {
        bgfx::touch(id);
        return;
    }

    static float skyboxView[16];
    quaternion_to_rotation_matrix(camera_get_rotation(camera), reinterpret_cast<Matrix4x4 *>(&skyboxView));
    bgfx::setUniform(_uMtx, skyboxView);

    bgfx::ProgramHandle program = _skyboxProgram_linear;
    if (_gameState->skyboxMode >= SkyboxMode_Cubemap) {
        vx_assert_d(_gameState->skyboxTex[0] != nullptr);

        TextureEntry *entry;
        if (ensureTextureEntry(_gameState->skyboxTex[0], SKYBOX_TEX_MAX_SIZE, &entry)) {
            if (bgfx::isValid(entry->th) && entry->inactivity >= 0) {
                bgfx::setTexture(0, _sFb[0], entry->th);
                program = _skyboxProgram_cubemap;
            }
        }
    }
    if (_gameState->skyboxMode == SkyboxMode_CubemapLerp) {
        vx_assert_d(_gameState->skyboxTex[1] != nullptr);

        TextureEntry *entry;
        if (ensureTextureEntry(_gameState->skyboxTex[1], SKYBOX_TEX_MAX_SIZE, &entry)) {
            if (bgfx::isValid(entry->th) && entry->inactivity >= 0) {
                bgfx::setTexture(1, _sFb[1], entry->th);
                program = _skyboxProgram_cubemapLerp;
                
                _uParamsData_fs[0] = _gameState->skyboxLerp;
                _uParamsData_fs[1] = 0.0f;
                _uParamsData_fs[2] = 0.0f;
                _uParamsData_fs[3] = 0.0f;
                bgfx::setUniform(_uParams_fs, _uParamsData_fs);
            }
        }
    }

    // note that skybox always needs UV between 0(-1) and 1, so "flipV" stays true
    // it is used to compute gradient values on the screenquad w/ the skybox mtx
    setScreenSpaceQuadTransient(0.0f, 0.0f, width, height, width, height, 1.0f, 1.0f, true);

    bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A);
    bgfx::submit(id, program);
}

void GameRenderer::drawDeferred(bgfx::ViewId id, bool blitAlpha, bool blend, Camera *camera, float *proj,
                                float projWidth, float projHeight, float targetWidth, float targetHeight,
                                float uMax, float vMax, uint16_t layers) {

    // Fullscreen multiplicative color
    float colorData[4]; utils_rgba_to_float(camera_get_color(camera), colorData);
    bgfx::setUniform(_uColor1, colorData);

    if (_features.useDeferred()) {
        // Combined ambient color, can be overriden
        if (_gameState->ambientColorOverride.x >= 0) {
            colorData[0] = _gameState->ambientColorOverride.x;
            colorData[1] = _gameState->ambientColorOverride.y;
            colorData[2] = _gameState->ambientColorOverride.z;
        } else {
            colorData[0] = _uSunColorData[0] * _gameState->skyboxAmbientFactor + _lightsAmbient[0];
            colorData[1] = _uSunColorData[1] * _gameState->skyboxAmbientFactor + _lightsAmbient[1];
            colorData[2] = _uSunColorData[2] * _gameState->skyboxAmbientFactor + _lightsAmbient[2];
        }
        colorData[3] = 0.0f;
        bgfx::setUniform(_uColor2, colorData);

        // Draw params
        float paramsData[4];
        paramsData[0] = _gameState->fogToggled() && camera_layers_match(layers, _gameState->fogLayers) ? 1.0f : 0.0;
        if (_features.linearDepthGBuffer == false && paramsData[0] == 1.0f) {
            float fogNear = camera_get_near(camera) + _gameState->fogStart;
            float fogFar = camera_get_near(camera) + _gameState->fogEnd;

            // Clip space fog near and far points
            fogNear = float_isZero(fogNear, EPSILON_ZERO) ? 0.01f : fogNear; // avoid anomaly
            fogFar = float_isEqual(fogNear, fogFar, EPSILON_ZERO) ? fogNear + 0.01f : fogFar; // avoid divide-by-zero
            bx::Vec3 viewNear = {projWidth * 0.5f, projHeight * 0.5f, fogNear};
            bx::Vec3 viewFar = {projWidth * 0.5f, projHeight * 0.5f, fogFar};
            bx::Vec3 clipNear = bx::mulH(viewNear, proj);
            bx::Vec3 clipFar = bx::mulH(viewFar, proj);

            paramsData[1] = clipNear.z;
            paramsData[2] = fabsf(clipFar.z - clipNear.z);
        } else {
            paramsData[1] = 0.0f;
            paramsData[2] = 0.0f;
        }
        paramsData[3] = 0.0f;
        bgfx::setUniform(_uParams, paramsData);

        // Blit depth buffer, so that it may be used as input while using target FB w/ depth buffer used by transparent pass
        bgfx::blit(id, _depthStencilBlit, 0, 0, _opaqueFbTex[_opaqueFb_depthIdx], 0, 0, _framebufferWidth, _framebufferHeight);

        // Binding opaque, light accumulation, vertex/ambient lighting, and transparency framebuffers output to samplers
        bgfx::setTexture(STAGE_BLIT_OPAQUE, _sFb[0], _opaqueFbTex[0]); // opaque pass albedo
        bgfx::setTexture(STAGE_BLIT_ILLUMINATION, _sFb[1], _lightFbTex[0]); // deferred illumination
        bgfx::setTexture(STAGE_BLIT_EMISSION, _sFb[2], _lightFbTex[1]); // deferred emission
        bgfx::setTexture(STAGE_BLIT_DEPTH, _sFb[3], _depthStencilBlit); // opaque depth/linear depth
        bgfx::setTexture(STAGE_BLIT_TRANSPARENCY_1, _sFb[4], _transparencyFbTex[0]); // RGB: pre-multiplied transparency color, A: revealage
        bgfx::setTexture(STAGE_BLIT_TRANSPARENCY_2, _sFb[5], _transparencyFbTex[1]); // pre-multiplied weight
    } else {
        // Binding color output to sampler
        bgfx::setTexture(STAGE_BLIT_OPAQUE, _sFb[0], _opaqueFbTex[0]);
    }

    if (blend) {
        bgfx::setState(getRenderState(0, RenderPassState_Layered));
    } else {
        bgfx::setState(getRenderState(0, RenderPassState_Blit));
    }

    setScreenSpaceQuadTransient(0.0f, 0.0f, targetWidth, targetHeight,
                                targetWidth, targetHeight, uMax, vMax,
                                _features.screenquadFlipV);

    bgfx::submit(id, blitAlpha ? _blitProgram_alpha : _blitProgram, SORT_ORDER_DEFAULT);
}

void GameRenderer::drawTarget(bgfx::ViewId id, bgfx::ProgramHandle program, bgfx::TextureHandle target,
                              uint32_t depth, float x, float y, float width, float height,
                              float fullWidth, float fullHeight, float uMax, float vMax, bool blend) {

    bgfx::setTexture(STAGE_TARGET, _sFb[0], target);

    if (blend) {
        bgfx::setState(getRenderState(0, RenderPassState_Layered));
    } else {
        bgfx::setState(getRenderState(0, RenderPassState_Blit));
    }

    setScreenSpaceQuadTransient(x, y, width, height, fullWidth, fullHeight, uMax, vMax, _features.screenquadFlipV);

    bgfx::submit(id, program, SORT_ORDER_DEFAULT + depth);
}

void GameRenderer::drawPost(bgfx::ViewId id, bgfx::TextureHandle input, uint16_t width, uint16_t height, float uMax, float vMax) {
    // Post-process params
    float paramsData[4];
    paramsData[0] = 1.0f / width; // texel size U
    paramsData[1] = 1.0f / height; // texel size V
    paramsData[2] = 0.0f;
    paramsData[3] = 0.0f;
    bgfx::setUniform(_uParams, paramsData);

    bgfx::setTexture(STAGE_TARGET, _sFb[0], input);

    bgfx::setState(getRenderState(0, RenderPassState_Blit));

    setScreenSpaceQuadTransient(0.0f, 0.0f, _screenWidth, _screenHeight, _screenWidth, _screenHeight,
                                uMax, vMax, _features.screenquadFlipV);

    bgfx::submit(id, _postProgram, SORT_ORDER_DEFAULT);
}

void GameRenderer::drawBlocker(bgfx::ViewId id, float alpha) {
    // Binding blocker color
    float colorData[4] = { .0f, .0f, .0f, alpha };
    bgfx::setUniform(_uColor1, colorData);

    // Blocker is alpha blended with other views
    bgfx::setState(getRenderState(0, RenderPassState_Layered));

    // Set blocker screen quad
    setFullScreenSpaceQuadTransient(_features.screenquadFlipV);

    bgfx::submit(id, _uiColorProgram, SORT_ORDER_DEFAULT);
}

// MARK: - RENDERER LIFECYCLE -

void GameRenderer::start(const Game_SharedPtr& g) {
    if (this->_game == nullptr) {
        setGame(g);
        toggleInternal(true);
        this->fade(VXMENU_GAME_FADE_IN, true, 3);
    } else if (this->_gameFadingIn == nullptr) {
        this->_gameFadingIn = g;
        this->fade(VXMENU_GAME_FADE_OUT, false, 0, [this]() {
            setGame(this->_gameFadingIn);
            toggleInternal(true);
            this->_gameFadingIn = nullptr;
            this->fade(VXMENU_GAME_FADE_IN, true, 3);
        });
    } else {
        // replace game fading in (previous game currently fading out)
        this->_gameFadingIn = g;
    }
}

void GameRenderer::stop() {
    if (_debug_textLine_left != 0 || _debug_textLine_right != 0) {
        debug_clearText();
    }

    if (_currentMapShape != nullptr) {
        shape_release(_currentMapShape);
        _currentMapShape = nullptr;
    }

    releaseSkybox();
    releaseShadowmaps();
    releaseShapeEntries();
    releaseQuadEntries();
    releaseMeshEntries();
    releaseTextureEntries();
    clearObjectLists();
    _decals->removeAll();
    _shadowDecals.clear();
    _worldText->removeAll();
    toggleInternal(false);

    _game = nullptr;
    _gameState = nullptr;
    _blockerInOut = false;

    // vxlog_trace("🖌️ game renderer stopped");
}

void GameRenderer::fade(float duration, bool inOut, uint8_t graceFrames, BlockerCallback callback) {
    if (_blockerInOut == inOut) {
        if (callback != nullptr) {
            callback();
        }
        return;
    }

    _blockerTimer = 0.0f;
    _blockerDuration = duration;
    _blockerGraceFrames = graceFrames;
    _blockerInOut = inOut;
    _blockerCallback = std::move(callback);
}

void GameRenderer::checkAndSetRendererFeatures() {
    const bgfx::Caps* caps = bgfx::getCaps();
    const uint64_t samplerFlags = 0
                                  | BGFX_SAMPLER_MIN_POINT
                                  | BGFX_SAMPLER_MAG_POINT
                                  | BGFX_SAMPLER_MIP_POINT
                                  | BGFX_SAMPLER_U_CLAMP
                                  | BGFX_SAMPLER_V_CLAMP;

#if DEBUG_RENDERER_FORCE_FEATURES
    _features.mask = DEBUG_RENDERER_VOXEL_FEATURES;

    const bool feature_tex3D = _features.isTexture3DEnabled();
    const bool tex3D_RG8 = true;
    const bool feature_mrt = _features.isMRTEnabled();
    const bool feature_inst = _features.isInstancingEnabled();
    const bool feature_cs = _features.isComputeEnabled();
    const bool feature_ind = _features.isDrawIndirectEnabled();
    const bool feature_readBack = _features.isTextureReadBackEnabled();
    const bool feature_blit = _features.isTextureBlitEnabled();
#else
    /// texture 3D capability
    const bool tex3D = (caps->supported & BGFX_CAPS_TEXTURE_3D) != 0;
    // log additional supported formats of interest
    const bool tex3D_RG8 = (BGFX_CAPS_FORMAT_TEXTURE_3D & caps->formats[bgfx::TextureFormat::RG8]) != 0;
    const bool tex3D_RGBA8 = (BGFX_CAPS_FORMAT_TEXTURE_3D & caps->formats[bgfx::TextureFormat::RGBA8]) != 0;

    const bool feature_tex3D = RENDERER_ALLOW_TEX3D && tex3D && (tex3D_RG8 || tex3D_RGBA8);

    /// multiple render target capability
    const bool mrt = caps->limits.maxFBAttachments >= MRT_MAX;
    const bool mrt_RG8 = bgfx::isTextureValid(0, false, 1, bgfx::TextureFormat::RG8, BGFX_TEXTURE_RT | samplerFlags); // optional (PBR buffer)
    const bool mrt_RGBA8 = bgfx::isTextureValid(0, false, 1, bgfx::TextureFormat::RGBA8, BGFX_TEXTURE_RT | samplerFlags); // required
    const bool mrt_RGBA16F = bgfx::isTextureValid(0, false, 1, bgfx::TextureFormat::RGBA16F, BGFX_TEXTURE_RT | samplerFlags); // required (OIT)
    const bool mrt_RGBA32F = bgfx::isTextureValid(0, false, 1, bgfx::TextureFormat::RGBA32F, BGFX_TEXTURE_RT | samplerFlags); // alternative to RGBA16F
    const bool mrt_R16F = bgfx::isTextureValid(0, false, 1, bgfx::TextureFormat::R16F, BGFX_TEXTURE_RT | samplerFlags); // required (linear depth & OIT)
    const bool mrt_R32F = bgfx::isTextureValid(0, false, 1, bgfx::TextureFormat::R32F, BGFX_TEXTURE_RT | samplerFlags); // alternative to R16F

    const bool feature_mrt = RENDERER_ALLOW_MRT && mrt && mrt_RGBA8 && (mrt_RGBA16F || mrt_RGBA32F) && (mrt_R16F || mrt_R32F);

    /// instancing capability
    const bool inst = (caps->supported & BGFX_CAPS_INSTANCING) != 0;

    const bool feature_inst = RENDERER_ALLOW_INSTANCING && inst;

    /// compute shaders capability
    const bool cs = (caps->supported & BGFX_CAPS_COMPUTE) != 0;

    const bool feature_cs = RENDERER_ALLOW_COMPUTE && cs;

    /// draw indirect capability
    const bool ind = (caps->supported & BGFX_CAPS_DRAW_INDIRECT) != 0;

    const bool feature_ind = RENDERER_ALLOW_INDIRECT && ind;

    /// texture read back capability
    const bool readBack = (caps->supported & BGFX_CAPS_TEXTURE_READ_BACK) != 0;

    const bool feature_readBack = RENDERER_ALLOW_READBACK && readBack;

    /// texture blit capability
    const bool blit = (caps->supported & BGFX_CAPS_TEXTURE_BLIT) != 0;

    const bool feature_blit = RENDERER_ALLOW_TEXBLIT && blit;

    /// shadow samplers capability
    const bool ss = (caps->supported & BGFX_CAPS_TEXTURE_COMPARE_LEQUAL) != 0;

    const bool feature_ss = RENDERER_ALLOW_SHADOWSAMPLERS && ss;

    _features.mask = RENDERER_FEATURE_NONE
                     | (feature_tex3D ? RENDERER_FEATURE_TEX3D : RENDERER_FEATURE_NONE)
                     | (feature_mrt ? RENDERER_FEATURE_MRT : RENDERER_FEATURE_NONE)
                     | (feature_inst ? RENDERER_FEATURE_INSTANCING : RENDERER_FEATURE_NONE)
                     | (feature_cs ? RENDERER_FEATURE_COMPUTE : RENDERER_FEATURE_NONE)
                     | (feature_ind ? RENDERER_FEATURE_INDIRECT : RENDERER_FEATURE_NONE)
                     | (feature_readBack ? RENDERER_FEATURE_READBACK : RENDERER_FEATURE_NONE)
                     | (feature_blit ? RENDERER_FEATURE_TEXBLIT : RENDERER_FEATURE_NONE)
                     | (feature_ss ? RENDERER_FEATURE_SHADOWSAMPLER : RENDERER_FEATURE_NONE);
    _features.mrt_r16fFormat = mrt_R16F ? bgfx::TextureFormat::R16F : bgfx::TextureFormat::R32F;
    _features.mrt_rgba16fFormat = mrt_RGBA16F ? bgfx::TextureFormat::RGBA16F : bgfx::TextureFormat::RGBA32F;
    _features.mrt_rg8Format = mrt_RG8 ? bgfx::TextureFormat::RG8 : bgfx::TextureFormat::RGBA8;
#endif

    /// check compatible depth format
    _features.depthFormat = bgfx::isTextureValid(0, false, 1, bgfx::TextureFormat::D32F, BGFX_TEXTURE_RT | samplerFlags)
                              ? bgfx::TextureFormat::D32F
                              : bgfx::isTextureValid(0, false, 1, bgfx::TextureFormat::D32, BGFX_TEXTURE_RT | samplerFlags)
                                ? bgfx::TextureFormat::D32
                                : bgfx::isTextureValid(0, false, 1, bgfx::TextureFormat::D24, BGFX_TEXTURE_RT | samplerFlags)
                                  ? bgfx::TextureFormat::D24: bgfx::TextureFormat::D16;
    _features.depthStencilFormat = bgfx::isTextureValid(0, false, 1, bgfx::TextureFormat::D24S8, BGFX_TEXTURE_RT | samplerFlags)
                            ? bgfx::TextureFormat::D24S8 : _features.depthFormat;

    /// check backend specific features,
    const bool gl_gles = caps->rendererType == bgfx::RendererType::OpenGL || caps->rendererType == bgfx::RendererType::OpenGLES;
    _features.screenquadFlipV = gl_gles; // screen origin difference
    // deferred lights: fragment pos reconstructed from depth buffer or linear depth in g-buffer,
    // if depth precision is lacking (GLES)
    _features.linearDepthGBuffer = gl_gles;
    _features.NDCDepth = caps->homogeneousDepth;

    vxlog_info("Renderer using %s", bgfx::getRendererName(caps->rendererType));
    vxlog_info("Renderer features flag set to %d", _features.mask);
    if (feature_tex3D) {
        vxlog_info("--- RENDERER_FEATURE_TEX3D");
    }
    if (feature_mrt) {
        vxlog_info("--- RENDERER_FEATURE_MRT");
    }
    if (feature_inst) {
        vxlog_info("--- RENDERER_FEATURE_INSTANCING");
    }
    if (feature_cs) {
        vxlog_info("--- RENDERER_FEATURE_COMPUTE");
    }
    if (feature_ind) {
        vxlog_info("--- RENDERER_FEATURE_INDIRECT");
    }
    if (feature_readBack) {
        vxlog_info("--- RENDERER_FEATURE_READBACK");
    }
    if (feature_blit) {
        vxlog_info("--- RENDERER_FEATURE_TEXBLIT");
    }
    if (feature_ss) {
        vxlog_info("--- RENDERER_FEATURE_SHADOWSAMPLER");
    }
    vxlog_info("Using depth format: %s/%s", bgfx::getName(_features.depthFormat), bgfx::getName(_features.depthStencilFormat));
    vxlog_info("Screenquad flip V: %s", _features.screenquadFlipV ? "true" : "false");
    vxlog_info("Linear depth in g-buffer: %s", _features.linearDepthGBuffer ? "true" : "false");
    vxlog_info("NDC depth: %s", _features.NDCDepth ? "true" : "false");

    vertex_buffer_set_lighting_enabled(true);
}

void GameRenderer::resetOverrideParams() {
    _uOverrideParamsData[0] = 1.0f; // alpha override, 1 is disabled
    _uOverrideParamsData[1] = 1.0f; // default mult. color R
    _uOverrideParamsData[2] = 1.0f; // default mult. color G
    _uOverrideParamsData[3] = 1.0f; // default mult. color B
    _uOverrideParamsData[4] = 0.0f; // default add. color R
    _uOverrideParamsData[5] = 0.0f; // default add. color G
    _uOverrideParamsData[6] = 0.0f; // default add. color B
    _uOverrideParamsData[7] = 0.0f;
    _uOverrideParamsData_fs[0] = 0.0f; // grid scale magnitude, 0 is disabled
    _uOverrideParamsData_fs[1] = 0.0f; // grid R
    _uOverrideParamsData_fs[2] = 0.0f; // grid G
    _uOverrideParamsData_fs[3] = 0.0f; // grid B
}

void GameRenderer::setGameParams(float fov, float farPlane, float paletteSize) {
    _uGameParamsData[0] = fov;
    _uGameParamsData[1] = _time;
    _uGameParamsData[2] = farPlane;
    _uGameParamsData[3] = paletteSize;
    _uGameParamsData[4] = _gameState->bakedIntensity;
    _uGameParamsData[5] = 0.0f;
    _uGameParamsData[6] = 0.0f;
    _uGameParamsData[7] = 0.0f;
    bgfx::setUniform(_uGameParams, _uGameParamsData, 2);
}

void GameRenderer::setLightingParams() {
    _uFogColorData[3] = CLAMP(_faceShadingFactor, FACE_SHADING_MIN_FACTOR, 1.0f);
    _uHorizonColorData[3] = _gameState->skyboxAmbientFactor;

    bgfx::setUniform(_uFogColor, _uFogColorData);
    bgfx::setUniform(_uSunColor, _uSunColorData);
    bgfx::setUniform(_uSkyColor, _uSkyColorData);
    bgfx::setUniform(_uHorizonColor, _uHorizonColorData);
    bgfx::setUniform(_uAbyssColor, _uAbyssColorData);

    // Reset lights ambient accumulation & face shading factor
    _lightsAmbient[0] = _lightsAmbient[1] = _lightsAmbient[2] = 0.0f;
    _faceShadingFactor = 1.0f;
}

uint32_t GameRenderer::updateColorAtlas(CGame *cgame) {
    ColorAtlas *colorAtlas = game_get_color_atlas(cgame);
    vx_assert(cgame != nullptr && colorAtlas != nullptr);

#if DEBUG_RENDERER_COLOR_ATLAS_FULL
    const uint32_t size = colorAtlas->size * colorAtlas->size * 4;
    const bgfx::Memory *mem = bgfx::alloc(size);
    uint8_t *data = mem->data;
    RGBAColor color;
    ATLAS_COLOR_INDEX_INT_T idx, dataIdx;
    for (unsigned int i = 0; i < colorAtlas->size; ++i) {
        for (unsigned int j = 0; j < colorAtlas->size; ++j) {
            idx = (j / 2) * colorAtlas->size + i;
            dataIdx = j * colorAtlas->size + i;

            if (j % 2 == 0) {
                color = colorAtlas->colors[idx];
            } else {
                color = colorAtlas->complementaryColors[idx];
            }
            data[dataIdx * 4] = color.r;
            data[dataIdx * 4 + 1] = color.g;
            data[dataIdx * 4 + 2] = color.b;
            data[dataIdx * 4 + 3] = color.a;
        }
    }
    bgfx::updateTexture2D(_colorAtlasTexture, 0, 0, 0, 0, colorAtlas->size, colorAtlas->size, mem, colorAtlas->size * 4);
#else
    // TODO: partial update above a certain size
    if (colorAtlas->dirty_slice_min != ATLAS_COLOR_INDEX_ERROR && colorAtlas->dirty_slice_max != ATLAS_COLOR_INDEX_ERROR) {
        const bool multiline = colorAtlas->dirty_slice_max >= colorAtlas->size;
        const uint32_t fromX = multiline ? 0 : colorAtlas->dirty_slice_min;
        const uint32_t width = multiline ? colorAtlas->size : colorAtlas->dirty_slice_max - colorAtlas->dirty_slice_min + 1;
        const uint32_t fromY = colorAtlas->dirty_slice_min / colorAtlas->size;
        const uint32_t toY = colorAtlas->dirty_slice_max / colorAtlas->size;
        const uint32_t height = toY - fromY + 1;

        const uint32_t pitch = width * 4;
        const uint32_t size = pitch * height * 2; // RGBA color + complementary
        const bgfx::Memory *mem = bgfx::alloc(size);
        uint8_t *data = mem->data;

        RGBAColor color;
        ATLAS_COLOR_INDEX_INT_T idx, sliceIdx;
        for (unsigned int i = 0; i < width; ++i) {
            for (unsigned int j = 0; j < height; ++j) {
                idx = (fromY + j) * colorAtlas->size + fromX + i;

                color = colorAtlas->colors[idx];
                sliceIdx = (j * 2) * pitch + i * 4;
                data[sliceIdx] = color.r;
                data[sliceIdx + 1] = color.g;
                data[sliceIdx + 2] = color.b;
                data[sliceIdx + 3] = color.a;

                color = colorAtlas->complementaryColors[idx];
                sliceIdx = (j * 2 + 1) * pitch + i * 4;
                data[sliceIdx] = color.r;
                data[sliceIdx + 1] = color.g;
                data[sliceIdx + 2] = color.b;
                data[sliceIdx + 3] = color.a;
            }
        }

        bgfx::updateTexture2D(_colorAtlasTexture,
                              0, 0,
                              fromX, fromY * 2,
                              width, height * 2,
                              mem, pitch);

        color_atlas_flush_slice(colorAtlas);
    }
#endif

    return colorAtlas->size;
}

void GameRenderer::setStartupViews(uint16_t width, uint16_t height) {
    const bgfx::Caps *caps = bgfx::getCaps();

    bgfx::setViewName(VIEW_SKYBOX, "Default clear");
    bgfx::setViewRect(VIEW_SKYBOX, 0, 0, bgfx::BackbufferRatio::Equal);
    bgfx::setViewClear(VIEW_SKYBOX, BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH, 0x000000ff, 1.0f, 0);

    bgfx::setViewName(VIEW_UI_IMGUI(VIEW_SKYBOX), "UI");
    bgfx::setViewMode(VIEW_UI_IMGUI(VIEW_SKYBOX), bgfx::ViewMode::Sequential);
    bgfx::setViewRect(VIEW_UI_IMGUI(VIEW_SKYBOX), 0, 0, bgfx::BackbufferRatio::Equal);
    setViewTransform(VIEW_UI_IMGUI(VIEW_SKYBOX), nullptr, true, width, height, caps->homogeneousDepth);

    bgfx::reset(width, height, BGFX_RESET_NONE);

    _afterpostView = VIEW_SKYBOX;
}

void GameRenderer::setCommonViews() {
    // Check shader fs_transparency_weight.sc for details on the transparency blending
    // /!\ clear alpha to 1.0 here as it actually contains the 'revealage' factor
    bgfx::setPaletteColor(0, 0.0f, 0.0f, 0.0f, 1.0f);
    // /!\ clears to 0.0 as this actually contains the pre-multiplied weight
    bgfx::setPaletteColor(1, 0.0f, 0.0f, 0.0f, 0.0f);

    bgfx::setPaletteColor(2, 1.0f, 1.0f, 1.0f, 1.0f);
    bgfx::setPaletteColor(3, 1.0f, 0.0f, 0.0f, 0.0f); // default voxel light

    // note that views are persistent ie. need to call resetViews() on renderer destruction

    bgfx::setViewRect(VIEW_CLEAR_1, 0, 0, _framebufferWidth, _framebufferHeight);
    bgfx::setViewClear(VIEW_CLEAR_1, BGFX_CLEAR_COLOR, 0x00000000);

    bgfx::setViewRect(VIEW_CLEAR_2, 0, 0, _framebufferWidth, _framebufferHeight);
    bgfx::setViewClear(VIEW_CLEAR_2, BGFX_CLEAR_COLOR, 0x00000000);

    bgfx::setViewRect(VIEW_CLEAR_3, 0, 0, _framebufferWidth, _framebufferHeight);
    bgfx::setViewClear(VIEW_CLEAR_3, BGFX_CLEAR_COLOR, 0x00000000);

    bgfx::setViewRect(VIEW_SKYBOX, 0, 0, _framebufferWidth, _framebufferHeight);
    bgfx::setViewClear(VIEW_SKYBOX, BGFX_CLEAR_COLOR, _gameState->clearColor);

#if DEBUG_RENDERER
    bgfx::setViewName(VIEW_CLEAR_1, "Clear 1");
    bgfx::setViewName(VIEW_CLEAR_2, "Clear 2");
    bgfx::setViewName(VIEW_CLEAR_3, "Clear 3");
    bgfx::setViewName(VIEW_SKYBOX, "Skybox");
#endif

    _viewsDirty = true;
}

void GameRenderer::setDefaultViews() {
    for (int i = 0; i <= _systemView; ++i) {
        bgfx::resetView(i);
        bgfx::setViewFrameBuffer(i, BGFX_INVALID_HANDLE);
    }

    bgfx::reset(_screenWidth, _screenHeight, BGFX_RESET_NONE);

    bgfx::setViewRect(VIEW_SKYBOX, 0, 0, bgfx::BackbufferRatio::Equal);
    bgfx::setViewClear(VIEW_SKYBOX, BGFX_CLEAR_COLOR, 0x000000ff);

    bgfx::setViewMode(VIEW_UI_IMGUI(VIEW_SKYBOX), bgfx::ViewMode::DepthDescending);
    bgfx::setViewRect(VIEW_UI_IMGUI(VIEW_SKYBOX), 0, 0, bgfx::BackbufferRatio::Equal);
    setViewTransform(VIEW_UI_IMGUI(VIEW_SKYBOX), nullptr, true, _screenWidth, _screenHeight, _features.NDCDepth);

    _afterpostView = VIEW_SKYBOX;
}

void GameRenderer::setTargetViews(PipelineSegment segment, uint8_t target, bool isBackdrop, uint16_t width, uint16_t height,
                                  bgfx::ViewId *stencil, bgfx::ViewId *opaque, bgfx::ViewId *lights,
                                  bgfx::ViewId *shadows, bgfx::ViewId *weight, bgfx::ViewId *blitDeferred,
                                  bgfx::ViewId *transparent) {
    vx_assert(width <= _framebufferWidth && height <= _framebufferHeight);

    const uint8_t cascades = SHADOWMAP_CASCADES_COUNT[_qualityParams.shadowsTier];

    const uint8_t offset = (segment == PipelineSegment_AfterPostTarget ? _postprocessView2 :
                            (segment == PipelineSegment_SystemTarget ? VIEW_UI_IMGUI(_afterpostView) :
                           VIEW_SKYBOX));
    const uint8_t start = offset + target * TARGET_VIEWS_COUNT;
    *stencil = VIEW_STENCIL(start);
    *opaque = VIEW_OPAQUE(start);
    *lights = VIEW_LIGHTS(start);
    for (uint8_t i = 0; i < cascades; ++i) {
        *(shadows + i) = VIEW_SHADOWS_1(start) + i;
    }
    *weight = VIEW_TRANSPARENT_WEIGHT(start);
    *blitDeferred = VIEW_BLIT(start);
    *transparent = VIEW_TRANSPARENT(start);

    bgfx::setViewClear(*stencil, BGFX_CLEAR_DEPTH | BGFX_CLEAR_STENCIL);
    bgfx::setViewRect(*stencil, 0, 0, width, height);
    bgfx::setViewMode(*stencil, bgfx::ViewMode::DepthDescending);
    bgfx::setViewFrameBuffer(*stencil, _opaqueFbh);

    // backdrop target cleared by skybox view, non-backdrop targets cleared by opaque view
    if (isBackdrop) {
        bgfx::setViewClear(*opaque, BGFX_CLEAR_DEPTH);
    } else {
        bgfx::setViewClear(*opaque, BGFX_CLEAR_DEPTH | BGFX_CLEAR_COLOR, 1.0f, 0,
                           1, // albedo: clears to (0,0,0,0)
                           0, // normal + unlit: clears to (0,0,0,1)
                           1, // illumination: clears to (0,0,0,0)
                           0, // emission + unlit: clears to (0,0,0,1)
                           1  // optional linear depth: clears to 0
       );
    }
    bgfx::setViewRect(*opaque, 0, 0, width, height);
    bgfx::setViewMode(*opaque, bgfx::ViewMode::DepthDescending);
    bgfx::setViewFrameBuffer(*opaque, _opaqueFbh);

    if (_features.useDeferred()) {
        // note: cleared as part of opaque framebuffer
        bgfx::setViewClear(*lights, BGFX_CLEAR_NONE);
        bgfx::setViewRect(*lights, 0, 0, width, height);
        bgfx::setViewMode(*lights, bgfx::ViewMode::Default);
        bgfx::setViewFrameBuffer(*lights, _lightFbh);

        const uint16_t shadowmapSize = SHADOWMAP_MAX_SIZE[_qualityParams.shadowsTier];
        for (uint8_t i = 0; i < cascades; ++i) {
            if (_features.isShadowSamplerEnabled()) {
                bgfx::setViewClear(*(shadows + i), BGFX_CLEAR_DEPTH);
            } else {
                bgfx::setViewClear(*(shadows + i), BGFX_CLEAR_DEPTH | BGFX_CLEAR_COLOR, SHADOWMAP_BLUR_CLEAR, 1.0f, 0);
            }
            bgfx::setViewRect(*(shadows + i), 0, 0, SHADOWMAP_CASCADES_SIZE_FACTORS[i] * shadowmapSize, SHADOWMAP_CASCADES_SIZE_FACTORS[i] * shadowmapSize);
            bgfx::setViewMode(*(shadows + i), bgfx::ViewMode::Default);
            // note: framebuffer set in drawPass_Lights_shadowmap
        }

        bgfx::setViewClear(*weight, BGFX_CLEAR_COLOR, 1.0f, 0, 0, 1);
        bgfx::setViewRect(*weight, 0, 0, width, height);
        bgfx::setViewMode(*weight, bgfx::ViewMode::Default);
        bgfx::setViewFrameBuffer(*weight, _transparencyFbh);
    }

    bgfx::setViewClear(*blitDeferred, BGFX_CLEAR_NONE);
    bgfx::setViewMode(*blitDeferred, bgfx::ViewMode::Default);

    bgfx::setViewClear(*transparent, BGFX_CLEAR_NONE);
    bgfx::setViewMode(*transparent, bgfx::ViewMode::DepthDescending);

    // blitDeferred & transparent views fb/rect set outside this function

#if DEBUG_RENDERER
    const std::string n = std::to_string(target + 1);
    const std::string info = segment == PipelineSegment_PostTarget ? "[Post-process Target " + n + "]" :
                             segment == PipelineSegment_AfterPostTarget ? "[After Post-process Target " + n + "]" :
                             "[System Target " + n + "]";

    bgfx::setViewName(*stencil, ("Stencil " + info).c_str());
    bgfx::setViewName(*opaque, ("Opaque " + info).c_str());
    if (_features.useDeferred()) {
        bgfx::setViewName(*lights, ("Lights " + info).c_str());
        for (uint8_t i = 0; i < cascades; ++i) {
            bgfx::setViewName(*(shadows + i), ("Shadows" + std::to_string(i + 1) + " " + info).c_str());
        }
        bgfx::setViewName(*weight, ("Transparent Weight " + info).c_str());
    }
    bgfx::setViewName(*blitDeferred, ("Blit Deferred" + info).c_str());
    bgfx::setViewName(*transparent, ("Transparent " + info).c_str());
#endif

    bgfx::setViewOrder();

    _maxTarget = maximum(_maxTarget, target);
}

void GameRenderer::clearViews() {
    bgfx::touch(VIEW_SKYBOX);

    if (_toggled) {
        for (int i = 1; i <= _maxView; ++i) {
            bgfx::touch(i);
        }
    }
}

bool GameRenderer::refreshViewsInternal() {
    bgfx::reset(_screenWidth, _screenHeight, BGFX_RESET_NONE);

    bgfx::setViewRect(VIEW_CLEAR_1, 0, 0, _framebufferWidth, _framebufferHeight);
    bgfx::setViewRect(VIEW_CLEAR_2, 0, 0, _framebufferWidth, _framebufferHeight);
    bgfx::setViewRect(VIEW_CLEAR_3, 0, 0, _framebufferWidth, _framebufferHeight);
    bgfx::setViewRect(VIEW_SKYBOX, 0, 0, _framebufferWidth, _framebufferHeight);

    // geometry views render at scaled resolution
    if (createFramebuffers(_framebufferWidth, _framebufferHeight) == false) {
        return false;
    }

    bgfx::setViewFrameBuffer(VIEW_CLEAR_1, _targetsFbh);
    bgfx::setViewFrameBuffer(VIEW_CLEAR_2, _afterPostTargetsFbh);
    bgfx::setViewFrameBuffer(VIEW_CLEAR_3, _systemTargetsFbh);
    bgfx::setViewFrameBuffer(VIEW_SKYBOX, _opaqueFbh);

    _viewsDirty = false;

    vxlog_trace("🖌️ game renderer views changed (%d, %d, %f) using (%d, %d) at render scale %f",
                _screenWidth, _screenHeight, static_cast<double>(_screenPPP),
                _renderWidth, _renderHeight, static_cast<double>(_qualityParams.renderScale));

    return true;
}

void GameRenderer::refreshDefaultViews() {
    bgfx::reset(_screenWidth, _screenHeight, BGFX_RESET_NONE);

    bgfx::setViewRect(VIEW_SKYBOX, 0, 0, bgfx::BackbufferRatio::Equal);
    bgfx::setViewRect(VIEW_UI_IMGUI(VIEW_SKYBOX), 0, 0, bgfx::BackbufferRatio::Equal);
    setViewTransform(VIEW_UI_IMGUI(VIEW_SKYBOX), nullptr, true, _screenWidth, _screenHeight, _features.NDCDepth);

    _afterpostView = VIEW_SKYBOX;
}

void GameRenderer::refreshStateInternal() {
    if (_gameState->isDirty(RENDERERSTATE_FOG)) {
        setFogInternal();
        _gameState->resetDirty(RENDERERSTATE_FOG);
    }
    if (_gameState->isDirty(RENDERERSTATE_CLOUDS)) {
        _sky->reset(&_gameState->sky);
        _gameState->resetDirty(RENDERERSTATE_CLOUDS);
    }
    if (_gameState->isDirty(RENDERERSTATE_SKYBOX)) {
        setSkybox();
        _gameState->resetDirty(RENDERERSTATE_SKYBOX);
    }
}

void GameRenderer::setSubRenderers() {
    _worldText = new WorldTextRenderer(&_features, &_qualityParams,
                                       &_fontProgram_unlit, &_fontProgram, &_fontProgram_overlay_unlit, &_fontProgram_overlay,
                                       &_fontProgram_weight,
                                       &_quadProgram, &_quadProgram_alpha, &_quadProgram_weight,
                                       _uParams, SORT_ORDER_DEFAULT);

    _decals = new Decals(&_features, &_qualityParams, _voxelIbh,
                         _features.useDeferred() ? &_quadProgram_tex_weight : &_quadProgram_tex_alpha,
                         _sFb, &_uParams, &_uColor1);
    _cookie = loadTextureFromFile("images/shadow_cookie.png", BGFX_SAMPLER_UVW_CLAMP);

    _sky = new Sky(&_features, &_cloudsProgram, &_cloudsProgram_unlit,
                   &_cs_cloudsUpdateProgram, &_cs_cloudsIndirectProgram);

    _gif = new GifRenderer();
}

void GameRenderer::releaseSubRenderers() {
    delete _worldText;
    delete _decals;
    if (bgfx::isValid(_cookie)) {
        bgfx::destroy(_cookie);
    }
    delete _sky;
    delete _gif;
}

void GameRenderer::setSkybox() {
    if (_gameState->skyboxTex[0] != nullptr) {
        if (_gameState->skyboxTex[1] != nullptr) {
            _gameState->skyboxMode = SkyboxMode_CubemapLerp;
        } else {
            _gameState->skyboxMode = SkyboxMode_Cubemap;
        }
    } else if (float3_isEqual(&_gameState->skyColor, &_gameState->horizonColor, EPSILON_ZERO) &&
               float3_isEqual(&_gameState->skyColor, &_gameState->abyssColor, EPSILON_ZERO)) {
        _gameState->skyboxMode = SkyboxMode_Clear;
    } else {
        _gameState->skyboxMode = SkyboxMode_Linear;
    }
    _gameState->clearColor = utils_float_to_rgba(1.0f, _gameState->skyColor.z, _gameState->skyColor.y, _gameState->skyColor.x);

    if (_gameState->skyboxMode == SkyboxMode_Clear) {
        bgfx::setViewClear(VIEW_SKYBOX, BGFX_CLEAR_COLOR, _gameState->clearColor);
    } else {
        bgfx::setViewClear(VIEW_SKYBOX, BGFX_CLEAR_NONE);
    }
}

void GameRenderer::releaseSkybox() {}

void GameRenderer::setStaticResources() {
    //// Static VB and IB
    _triangles = static_cast<uint32_t *>(malloc(SHAPE_BUFFER_MAX_COUNT * IB_BYTES));

    uint32_t v1, v2, v3, v4, triIdx;
    for (uint32_t i = 0; i < SHAPE_BUFFER_MAX_COUNT; i++) {
        v1 = VB_VERTICES * i;
        v2 = VB_VERTICES * i + 1;
        v3 = VB_VERTICES * i + 2;
        v4 = VB_VERTICES * i + 3;

        triIdx = 6 * i;
        _triangles[triIdx] = v2;
        _triangles[triIdx + 1] = v3;
        _triangles[triIdx + 2] = v4;
        _triangles[triIdx + 3] = v4;
        _triangles[triIdx + 4] = v1;
        _triangles[triIdx + 5] = v2;
    }

    _voxelIbh = bgfx::createIndexBuffer(bgfx::makeRef(_triangles, SHAPE_BUFFER_MAX_COUNT * IB_BYTES), BGFX_BUFFER_INDEX32);

    if (bgfx::isValid(_voxelIbh) == false) {
        _errored = true;
        vxlog_error("⚠️⚠️⚠️ renderer resource allocation failed, a restart is advised");
        return;
    }

    // initialize empty atlas texture (w/ error color)
    const uint32_t size = COLOR_ATLAS_SIZE * COLOR_ATLAS_SIZE;
    const bgfx::Memory *mem = bgfx::alloc(size * 4);
    for (uint32_t i = 0; i < size; ++i) {
        *((RGBAColor*)mem->data + i) = RGBAColor_magenta;
    }
    _colorAtlasTexture = bgfx::createTexture2D(COLOR_ATLAS_SIZE, COLOR_ATLAS_SIZE, false, 1, bgfx::TextureFormat::RGBA8,
                                               BGFX_SAMPLER_UVW_CLAMP|BGFX_SAMPLER_MIN_POINT|BGFX_SAMPLER_MAG_POINT|BGFX_SAMPLER_MIP_POINT);

    if (bgfx::isValid(_colorAtlasTexture) == false) {
        _errored = true;
        vxlog_error("⚠️⚠️⚠️ renderer resource allocation failed, a restart is advised");
        return;
    }

    bgfx::updateTexture2D(_colorAtlasTexture, 0, 0, 0, 0, COLOR_ATLAS_SIZE, COLOR_ATLAS_SIZE, mem);

    const bgfx::Memory *mem2 = bgfx::alloc(4);
    *((RGBAColor*)mem2->data) = RGBAColor_black;
    _dummyTh = bgfx::createTexture2D(1, 1, false, 1, bgfx::TextureFormat::RGBA8, BGFX_SAMPLER_UVW_CLAMP, mem2);
}

void GameRenderer::releaseStaticResources() {
    free(_triangles);
    bgfx::destroy(_voxelIbh);
    bgfx::destroy(_colorAtlasTexture);
    bgfx::destroy(_dummyTh);
}

void GameRenderer::setPrimitivesStaticData() {
    PosColorVertex::init();

    float *vertices = nullptr;
    size_t nbVertices, nbTriangles;
    Utils::generateUVSphere(12, 12, &vertices, &s_sphereTriList, &nbVertices, &nbTriangles);
    s_sphereVertices = static_cast<PosColorVertex *>(malloc(nbVertices * sizeof(PosColorVertex)));
    for (size_t i=0; i < nbVertices; ++i) {
        s_sphereVertices[i] = { vertices[i * 3], vertices[i * 3 + 1], vertices[i * 3 + 2], 0x3b000000 };
    }
    free(vertices);

    _cubeVbh = bgfx::createVertexBuffer(
            bgfx::makeRef(s_cubeVertices, sizeof(s_cubeVertices)),
            PosColorVertex::_vertexColorLayout
    );

    _cubeIbh = bgfx::createIndexBuffer(bgfx::makeRef(s_cubeLineList, sizeof(s_cubeLineList)));

    _sphereVbh = bgfx::createVertexBuffer(
        bgfx::makeRef(s_sphereVertices, static_cast<uint32_t>(nbVertices * sizeof(PosColorVertex))),
        PosColorVertex::_vertexColorLayout
    );

    _sphereIbh = bgfx::createIndexBuffer(bgfx::makeRef(s_sphereTriList, static_cast<uint32_t>(nbTriangles * sizeof(uint16_t) * 3)));
}

void GameRenderer::releasePrimitivesStaticData() {
    bgfx::destroy(_cubeIbh);
    bgfx::destroy(_cubeVbh);
    bgfx::destroy(_sphereVbh);
    bgfx::destroy(_sphereIbh);
    free(s_sphereVertices);
    free(s_sphereTriList);
}

void GameRenderer::setUniforms() {
    //// Reserved for parameters
    _uParams = bgfx::createUniform("u_params", bgfx::UniformType::Vec4, 1);
    _uParams_fs = bgfx::createUniform("u_params_fs", bgfx::UniformType::Vec4, 1);
    _uOverrideParams = bgfx::createUniform("u_overrideParams", bgfx::UniformType::Vec4, 2);
    _uOverrideParams_fs = bgfx::createUniform("u_overrideParams_fs", bgfx::UniformType::Vec4, 1);
    _uGameParams = bgfx::createUniform("u_gameParams", bgfx::UniformType::Vec4, 2);
    if (_features.useDeferred()) {
        _uLightParams = bgfx::createUniform("u_lightParams", bgfx::UniformType::Vec4, 5);
        _uShadowParams = bgfx::createUniform("u_shadowParams", bgfx::UniformType::Vec4, 1);
    } else {
        _uLightParams = BGFX_INVALID_HANDLE;
        _uShadowParams = BGFX_INVALID_HANDLE;
    }

    //// Global lighting colors
    _uFogColor = bgfx::createUniform("u_fogColor", bgfx::UniformType::Vec4);
    _uSunColor = bgfx::createUniform("u_sunColor", bgfx::UniformType::Vec4);

    //// Skybox colors
    _uSkyColor = bgfx::createUniform("u_skyColor", bgfx::UniformType::Vec4);
    _uHorizonColor = bgfx::createUniform("u_horizonColor", bgfx::UniformType::Vec4);
    _uAbyssColor = bgfx::createUniform("u_abyssColor", bgfx::UniformType::Vec4);

    //// Textures samplers
    _sColorAtlas = bgfx::createUniform("s_colorAtlas", bgfx::UniformType::Sampler);
    for (uint8_t i = 0; i < 8; ++i) {
        _sFb[i] = bgfx::createUniform(("s_fb" + std::to_string(i + 1)).c_str(), bgfx::UniformType::Sampler);
    }

    //// Cascaded shadowmap light view-proj matrices
    for (uint8_t i = 0; i < SHADOWMAP_CASCADES_COUNT[_qualityParams.shadowsTier]; ++i) {
        if (_features.useDeferred()) {
            _uLightVPMtx[i] = bgfx::createUniform(("u_lightVP" + std::to_string(i + 1)).c_str(), bgfx::UniformType::Mat4);
        } else {
            _uLightVPMtx[i] = BGFX_INVALID_HANDLE;
        }
    }
    for (uint8_t i = SHADOWMAP_CASCADES_COUNT[_qualityParams.shadowsTier]; i < SHADOWMAP_CASCADES_MAX; ++i) {
        _uLightVPMtx[i] = BGFX_INVALID_HANDLE;
    }

    //// Misc.
    _uColor1 = bgfx::createUniform("u_color1", bgfx::UniformType::Vec4);
    _uColor2 = bgfx::createUniform("u_color2", bgfx::UniformType::Vec4);
    _uMtx = bgfx::createUniform("u_mtx", bgfx::UniformType::Mat4);
}

void GameRenderer::releaseUniforms() {
    bgfx::destroy(_sColorAtlas);
    bgfx::destroy(_uParams);
    bgfx::destroy(_uParams_fs);
    bgfx::destroy(_uOverrideParams);
    bgfx::destroy(_uOverrideParams_fs);
    bgfx::destroy(_uGameParams);
    bgfx::destroy(_uFogColor);
    bgfx::destroy(_uSunColor);
    bgfx::destroy(_uSkyColor);
    bgfx::destroy(_uHorizonColor);
    bgfx::destroy(_uAbyssColor);
    if (bgfx::isValid(_uLightParams)) {
        bgfx::destroy(_uLightParams);
    }
    for (uint8_t i = 0; i < SHADOWMAP_CASCADES_COUNT[_qualityParams.shadowsTier] ; ++i) {
        if (bgfx::isValid(_uLightVPMtx[i])) {
            bgfx::destroy(_uLightVPMtx[i]);
        }
    }
    if (bgfx::isValid(_uShadowParams)) {
        bgfx::destroy(_uShadowParams);
    }
    for (uint8_t i = 0; i < 8; ++i) {
        if (bgfx::isValid(_sFb[i])) {
            bgfx::destroy(_sFb[i]);
        }
    }
    bgfx::destroy(_uColor1);
    bgfx::destroy(_uColor2);
    bgfx::destroy(_uMtx);
}

void GameRenderer::setPrograms() {
    BXFileReader reader;

    const bool deferred = _features.useDeferred();
    const bool instancing = _features.isInstancingEnabled();
    const bool compute = _features.isComputeEnabled();
    const bool indirect = _features.isDrawIndirectEnabled();
    const bool ss = _features.isShadowSamplerEnabled();

    //// LIGHTING PASS,
    /// - deferred lighting may get fragment pos directly from g-buffer or reconstruct it from depth
    /// - shadowmaps may use packed depth or depth sampler
    /// - no fallback currently
    typedef enum {
        lighting_deferred_linear_depth,
        lighting_deferred,
        lighting_fallback
    } LightingPass;
    const LightingPass lpass = deferred ?
                               (_features.linearDepthGBuffer ? lighting_deferred_linear_depth : lighting_deferred)
                                        : lighting_fallback;
    switch(lpass) {
        case lighting_deferred_linear_depth: {
            _lightProgram_point = loadProgram(&reader, "vs_screen_far", _usePBR ? "fs_deferred_light_variant_point_pbr_ldepth" : "fs_deferred_light_variant_point_ldepth");
            _lightProgram_spot = loadProgram(&reader, "vs_screen_far", _usePBR ? "fs_deferred_light_variant_spot_pbr_ldepth" : "fs_deferred_light_variant_spot_ldepth");
            _lightProgram_directional = loadProgram(&reader, "vs_screen_far", _usePBR ? "fs_deferred_light_variant_directional_pbr_ldepth" : "fs_deferred_light_variant_directional_ldepth");

            if (ss) {
                _lightProgram_point_sm = loadProgram(&reader, "vs_screen_far", _usePBR ? "fs_deferred_light_variant_point_pbr_ldepth_sm_ss" : "fs_deferred_light_variant_point_ldepth_sm_ss");
                _lightProgram_spot_sm = loadProgram(&reader, "vs_screen_far", _usePBR ? "fs_deferred_light_variant_spot_pbr_ldepth_sm_ss" : "fs_deferred_light_variant_spot_ldepth_sm_ss");

                switch(SHADOWMAP_CASCADES_COUNT[_qualityParams.shadowsTier]) {
                    case 4: {
                        _lightProgram_directional_sm = SHADOWMAP_FILTERING[_qualityParams.shadowsTier] ?
                                                       loadProgram(&reader, "vs_screen_far", _usePBR ? "fs_deferred_light_variant_directional_pbr_ldepth_sm4_soft_ss" : "fs_deferred_light_variant_directional_ldepth_sm4_soft_ss") :
                                                       loadProgram(&reader, "vs_screen_far", _usePBR ? "fs_deferred_light_variant_directional_pbr_ldepth_sm4_ss" : "fs_deferred_light_variant_directional_ldepth_sm4_ss");
                        break;
                    }
                    case 2: {
                        _lightProgram_directional_sm = SHADOWMAP_FILTERING[_qualityParams.shadowsTier] ?
                                                       loadProgram(&reader, "vs_screen_far", _usePBR ? "fs_deferred_light_variant_directional_pbr_ldepth_sm2_soft_ss" : "fs_deferred_light_variant_directional_ldepth_sm2_soft_ss") :
                                                       loadProgram(&reader, "vs_screen_far", _usePBR ? "fs_deferred_light_variant_directional_pbr_ldepth_sm2_ss" : "fs_deferred_light_variant_directional_ldepth_sm2_ss");
                        break;
                    }
                    default:
                    case 1: {
                        _lightProgram_directional_sm = SHADOWMAP_FILTERING[_qualityParams.shadowsTier] ?
                                                       loadProgram(&reader, "vs_screen_far", _usePBR ? "fs_deferred_light_variant_directional_pbr_ldepth_sm1_soft_ss" : "fs_deferred_light_variant_directional_ldepth_sm1_soft_ss") :
                                                       loadProgram(&reader, "vs_screen_far", _usePBR ? "fs_deferred_light_variant_directional_pbr_ldepth_sm1_ss" : "fs_deferred_light_variant_directional_ldepth_sm1_ss");
                        break;
                    }
                    case 0: {
                        _lightProgram_directional_sm = BGFX_INVALID_HANDLE;
                        break;
                    }
                }
            } else {
                _lightProgram_point_sm = loadProgram(&reader, "vs_screen_far", _usePBR ? "fs_deferred_light_variant_point_pbr_ldepth_sm" : "fs_deferred_light_variant_point_ldepth_sm");
                _lightProgram_spot_sm = loadProgram(&reader, "vs_screen_far", _usePBR ? "fs_deferred_light_variant_spot_pbr_ldepth_sm" : "fs_deferred_light_variant_spot_ldepth_sm");

                switch(SHADOWMAP_CASCADES_COUNT[_qualityParams.shadowsTier]) {
                    case 4: {
                        _lightProgram_directional_sm = SHADOWMAP_FILTERING[_qualityParams.shadowsTier] ?
                                                       loadProgram(&reader, "vs_screen_far", _usePBR ? "fs_deferred_light_variant_directional_pbr_ldepth_sm4_soft" : "fs_deferred_light_variant_directional_ldepth_sm4_soft") :
                                                       loadProgram(&reader, "vs_screen_far", _usePBR ? "fs_deferred_light_variant_directional_pbr_ldepth_sm4" : "fs_deferred_light_variant_directional_ldepth_sm4");
                        break;
                    }
                    case 2: {
                        _lightProgram_directional_sm = SHADOWMAP_FILTERING[_qualityParams.shadowsTier] ?
                                                       loadProgram(&reader, "vs_screen_far", _usePBR ? "fs_deferred_light_variant_directional_pbr_ldepth_sm2_soft" : "fs_deferred_light_variant_directional_ldepth_sm2_soft") :
                                                       loadProgram(&reader, "vs_screen_far", _usePBR ? "fs_deferred_light_variant_directional_pbr_ldepth_sm2" : "fs_deferred_light_variant_directional_ldepth_sm2");
                        break;
                    }
                    default:
                    case 1: {
                        _lightProgram_directional_sm = SHADOWMAP_FILTERING[_qualityParams.shadowsTier] ?
                                                       loadProgram(&reader, "vs_screen_far", _usePBR ? "fs_deferred_light_variant_directional_pbr_ldepth_sm1_soft" : "fs_deferred_light_variant_directional_ldepth_sm1_soft") :
                                                       loadProgram(&reader, "vs_screen_far", _usePBR ? "fs_deferred_light_variant_directional_pbr_ldepth_sm1" : "fs_deferred_light_variant_directional_ldepth_sm1");
                        break;
                    }
                    case 0: {
                        _lightProgram_directional_sm = BGFX_INVALID_HANDLE;
                        break;
                    }
                }
            }
            break;
        }
        case lighting_deferred: {
            _lightProgram_point = loadProgram(&reader, "vs_screen", _usePBR ? "fs_deferred_light_variant_point_pbr" : "fs_deferred_light_variant_point");
            _lightProgram_spot = loadProgram(&reader, "vs_screen", _usePBR ? "fs_deferred_light_variant_spot_pbr" : "fs_deferred_light_variant_spot");
            _lightProgram_directional = loadProgram(&reader, "vs_screen", _usePBR ? "fs_deferred_light_variant_directional_pbr" : "fs_deferred_light_variant_directional");

            if (ss) {
                _lightProgram_point_sm = loadProgram(&reader, "vs_screen", _usePBR ? "fs_deferred_light_variant_point_pbr_sm_ss" : "fs_deferred_light_variant_point_sm_ss");
                _lightProgram_spot_sm = loadProgram(&reader, "vs_screen", _usePBR ? "fs_deferred_light_variant_spot_pbr_sm_ss" : "fs_deferred_light_variant_spot_sm_ss");

                switch(SHADOWMAP_CASCADES_COUNT[_qualityParams.shadowsTier]) {
                    case 4: {
                        _lightProgram_directional_sm = SHADOWMAP_FILTERING[_qualityParams.shadowsTier] ?
                                                       loadProgram(&reader, "vs_screen", _usePBR ? "fs_deferred_light_variant_directional_pbr_sm4_soft_ss" : "fs_deferred_light_variant_directional_sm4_soft_ss") :
                                                       loadProgram(&reader, "vs_screen", _usePBR ? "fs_deferred_light_variant_directional_pbr_sm4_ss" : "fs_deferred_light_variant_directional_sm4_ss");
                        break;
                    }
                    case 2: {
                        _lightProgram_directional_sm = SHADOWMAP_FILTERING[_qualityParams.shadowsTier] ?
                                                       loadProgram(&reader, "vs_screen", _usePBR ? "fs_deferred_light_variant_directional_pbr_sm2_soft_ss" : "fs_deferred_light_variant_directional_sm2_soft_ss") :
                                                       loadProgram(&reader, "vs_screen", _usePBR ? "fs_deferred_light_variant_directional_pbr_sm2_ss" : "fs_deferred_light_variant_directional_sm2_ss");
                        break;
                    }
                    default:
                    case 1: {
                        _lightProgram_directional_sm = SHADOWMAP_FILTERING[_qualityParams.shadowsTier] ?
                                                       loadProgram(&reader, "vs_screen", _usePBR ? "fs_deferred_light_variant_directional_pbr_sm1_soft_ss" : "fs_deferred_light_variant_directional_sm1_soft_ss") :
                                                       loadProgram(&reader, "vs_screen", _usePBR ? "fs_deferred_light_variant_directional_pbr_sm1_ss" : "fs_deferred_light_variant_directional_sm1_ss");
                        break;
                    }
                    case 0: {
                        _lightProgram_directional_sm = BGFX_INVALID_HANDLE;
                        break;
                    }
                }
            } else {
                _lightProgram_point_sm = loadProgram(&reader, "vs_screen", _usePBR ? "fs_deferred_light_variant_point_pbr_sm" : "fs_deferred_light_variant_point_sm");
                _lightProgram_spot_sm = loadProgram(&reader, "vs_screen", _usePBR ? "fs_deferred_light_variant_spot_pbr_sm" : "fs_deferred_light_variant_spot_sm");

                switch(SHADOWMAP_CASCADES_COUNT[_qualityParams.shadowsTier]) {
                    case 4: {
                        _lightProgram_directional_sm = SHADOWMAP_FILTERING[_qualityParams.shadowsTier] ?
                                                       loadProgram(&reader, "vs_screen", _usePBR ? "fs_deferred_light_variant_directional_pbr_sm4_soft" : "fs_deferred_light_variant_directional_sm4_soft") :
                                                       loadProgram(&reader, "vs_screen", _usePBR ? "fs_deferred_light_variant_directional_pbr_sm4" : "fs_deferred_light_variant_directional_sm4");
                        break;
                    }
                    case 2: {
                        _lightProgram_directional_sm = SHADOWMAP_FILTERING[_qualityParams.shadowsTier] ?
                                                       loadProgram(&reader, "vs_screen", _usePBR ? "fs_deferred_light_variant_directional_pbr_sm2_soft" : "fs_deferred_light_variant_directional_sm2_soft") :
                                                       loadProgram(&reader, "vs_screen", _usePBR ? "fs_deferred_light_variant_directional_pbr_sm2" : "fs_deferred_light_variant_directional_sm2");
                        break;
                    }
                    default:
                    case 1: {
                        _lightProgram_directional_sm = SHADOWMAP_FILTERING[_qualityParams.shadowsTier] ?
                                                       loadProgram(&reader, "vs_screen", _usePBR ? "fs_deferred_light_variant_directional_pbr_sm1_soft" : "fs_deferred_light_variant_directional_sm1_soft") :
                                                       loadProgram(&reader, "vs_screen", _usePBR ? "fs_deferred_light_variant_directional_pbr_sm1" : "fs_deferred_light_variant_directional_sm1");
                        break;
                    }
                    case 0: {
                        _lightProgram_directional_sm = BGFX_INVALID_HANDLE;
                        break;
                    }
                }
            }
            break;
        }
        case lighting_fallback: {
            _lightProgram_point = BGFX_INVALID_HANDLE;
            _lightProgram_spot = BGFX_INVALID_HANDLE;
            _lightProgram_directional = BGFX_INVALID_HANDLE;

            _lightProgram_point_sm = BGFX_INVALID_HANDLE;
            _lightProgram_spot_sm = BGFX_INVALID_HANDLE;
            _lightProgram_directional_sm = BGFX_INVALID_HANDLE;
            break;
        }
    }
    if (deferred) {
        if (ss) {
            _shapeProgram_sm = loadProgram(&reader, "vs_voxels_variant_spass_ss", "fs_clear");
            _quadProgram_sm = loadProgram(&reader, "vs_quad_variant_spass_ss", "fs_clear");
            _quadProgram_sm_tex_cutout = loadProgram(&reader, "vs_quad_variant_spass_ss_tex_cutout", "fs_clear_tex_cutout");
            _meshProgram_sm = loadProgram(&reader, "vs_mesh_variant_spass_ss", "fs_clear");
        } else {
            _shapeProgram_sm = loadProgram(&reader, "vs_voxels_variant_spass", "fs_pack_depth");
            _quadProgram_sm = loadProgram(&reader, "vs_quad_variant_spass", "fs_pack_depth");
            _quadProgram_sm_tex_cutout = loadProgram(&reader, "vs_quad_variant_spass_tex_cutout", "fs_pack_depth_tex_cutout");
            _meshProgram_sm = loadProgram(&reader, "vs_mesh_variant_spass", "fs_pack_depth");
        }
    } else {
        _shapeProgram_sm = BGFX_INVALID_HANDLE;
        _quadProgram_sm = BGFX_INVALID_HANDLE;
        _meshProgram_sm = BGFX_INVALID_HANDLE;
    }

    //// OPAQUE PASS
    /// - requires geometry to support deferred lighting
    /// - optionally supports PBR parameters
    /// - writes Z while drawing
    ///
    /// In this pass: SKYBOX, CLOUDS, SHAPE, FONT, QUAD, MESH
    ///
    /// Skybox,
    /// - clears the view & buffers if any
    /// - may use cubemaps, although it is currently not a runtime option
    ///
    /// Clouds,
    /// - no alpha
    /// - use instancing
    /// - dynamic if compute/indirect available, static otherwise
    ///
    /// Shape,
    /// - no alpha in this pass
    /// - transparent geometry drawn in TransparentWeight pass
    /// - may be lit from uniform, attributes, lights, or unlit
    /// - may use draw mode overrides
    ///
    /// Font (and their background quad),
    /// - TransparentPost pass or Opaque pass depending on font
    /// - may be lit from uniform, lights, or unlit
    ///
    /// Quad,
    /// - no alpha in this pass
    /// - transparent geometry drawn in TransparentWeight or TransparentPost pass
    /// - may be lit from uniform, lights, or unlit
    ///
    /// Mesh,
    /// - no alpha in this pass
    /// - transparent geometry drawn in TransparentPost pass
    /// - may be lit from uniform, lights, or unlit
    typedef enum {
        opaque_lpass_linear_depth,
        opaque_lpass,
        opaque_fallback
    } OpaquePass;
    const OpaquePass opass = deferred ?
                             (_features.linearDepthGBuffer ? opaque_lpass_linear_depth : opaque_lpass)
                                      : opaque_fallback;
    switch(opass) {
        case opaque_lpass_linear_depth: {
            _skyboxProgram_linear = loadProgram(&reader, "vs_skybox", "fs_skybox_variant_clear_ldepth");
            _skyboxProgram_cubemap = loadProgram(&reader, "vs_skybox", "fs_skybox_variant_cube_clear_ldepth");
            _skyboxProgram_cubemapLerp = loadProgram(&reader, "vs_skybox", "fs_skybox_variant_cubelerp_clear_ldepth");

            if (instancing) {
                _cloudsProgram = loadProgram(&reader, compute ? "vs_clouds_instancing_variant_compute_lpass_ldepth" : "vs_clouds_instancing_variant_lpass_ldepth",
                                                      _usePBR ? "fs_vcolor_variant_lpass_pbr_ldepth" : "fs_vcolor_variant_lpass_ldepth");
                _cloudsProgram_unlit = loadProgram(&reader, compute ? "vs_clouds_instancing_variant_compute" : "vs_clouds_instancing_fallback",
                                                            _usePBR ? "fs_vcolor_variant_lpass_pbr_ldepth_unlit" : "fs_vcolor_variant_lpass_ldepth_unlit");
            } else {
                _cloudsProgram = BGFX_INVALID_HANDLE;
                _cloudsProgram_unlit = BGFX_INVALID_HANDLE;
            }

            _shapeProgram = loadProgram(&reader, "vs_voxels_variant_lpass_ldepth", _usePBR ? "fs_voxels_variant_lpass_pbr_ldepth" : "fs_voxels_variant_lpass_ldepth");
            _shapeProgram_dm = loadProgram(&reader, "vs_voxels_variant_lpass_ldepth_dm", _usePBR ? "fs_voxels_variant_lpass_pbr_ldepth_dm" : "fs_voxels_variant_lpass_ldepth_dm");

            _fontProgram = loadProgram(&reader, "vs_font_variant_ldepth", _usePBR ? "fs_font_variant_lpass_pbr_ldepth" : "fs_font_variant_lpass_ldepth");
            _fontProgram_unlit = loadProgram(&reader, "vs_font_variant_ldepth", _usePBR ? "fs_font_variant_lpass_pbr_ldepth_unlit" : "fs_font_variant_lpass_ldepth_unlit");

            _quadProgram = loadProgram(&reader, "vs_quad_variant_lpass_ldepth", _usePBR ? "fs_quad_variant_lpass_pbr_ldepth" : "fs_quad_variant_lpass_ldepth");
            _quadProgram_tex = loadProgram(&reader, "vs_quad_variant_lpass_tex_ldepth", _usePBR ? "fs_quad_variant_lpass_pbr_tex_ldepth" : "fs_quad_variant_lpass_tex_ldepth");
            _quadProgram_tex_cutout = loadProgram(&reader, "vs_quad_variant_lpass_tex_ldepth", _usePBR ? "fs_quad_variant_lpass_pbr_tex_ldepth_cutout" : "fs_quad_variant_lpass_tex_ldepth_cutout");

            _meshProgram = loadProgram(&reader, "vs_mesh_variant_lpass_ldepth", _usePBR ? "fs_mesh_variant_lpass_pbr_ldepth" : "fs_mesh_variant_lpass_ldepth");
            _meshProgram_cutout = loadProgram(&reader, "vs_mesh_variant_lpass_ldepth", _usePBR ? "fs_mesh_variant_lpass_pbr_ldepth_cutout" : "fs_mesh_variant_lpass_ldepth_cutout");
            break;
        }
        case opaque_lpass: {
            _skyboxProgram_linear = loadProgram(&reader, "vs_skybox", "fs_skybox_variant_clear");
            _skyboxProgram_cubemap = loadProgram(&reader, "vs_skybox", "fs_skybox_variant_cube_clear");
            _skyboxProgram_cubemapLerp = loadProgram(&reader, "vs_skybox", "fs_skybox_variant_cubelerp_clear");

            if (instancing) {
                _cloudsProgram = loadProgram(&reader, compute ? "vs_clouds_instancing_variant_compute_lpass" : "vs_clouds_instancing_variant_lpass",
                                                      _usePBR ? "fs_vcolor_variant_lpass_pbr" : "fs_vcolor_variant_lpass");
                _cloudsProgram_unlit = loadProgram(&reader, compute ? "vs_clouds_instancing_variant_compute" : "vs_clouds_instancing_fallback",
                                                            _usePBR ? "fs_vcolor_variant_lpass_pbr_unlit" : "fs_vcolor_variant_lpass_unlit");
            } else {
                _cloudsProgram = BGFX_INVALID_HANDLE;
                _cloudsProgram_unlit = BGFX_INVALID_HANDLE;
            }

            _shapeProgram = loadProgram(&reader, "vs_voxels_variant_lpass", _usePBR ? "fs_voxels_variant_lpass_pbr" : "fs_voxels_variant_lpass");
            _shapeProgram_dm = loadProgram(&reader, "vs_voxels_variant_lpass_dm", _usePBR ? "fs_voxels_variant_lpass_pbr_dm" : "fs_voxels_variant_lpass_dm");

            _fontProgram = loadProgram(&reader, "vs_font_fallback", _usePBR ? "fs_font_variant_lpass_pbr" : "fs_font_variant_lpass");
            _fontProgram_unlit = loadProgram(&reader, "vs_font_fallback", _usePBR ? "fs_font_variant_lpass_pbr_unlit" : "fs_font_variant_lpass_unlit");

            _quadProgram = loadProgram(&reader, "vs_quad_variant_lpass", _usePBR ? "fs_quad_variant_lpass_pbr" : "fs_quad_variant_lpass");
            _quadProgram_tex = loadProgram(&reader, "vs_quad_variant_lpass_tex", _usePBR ? "fs_quad_variant_lpass_pbr_tex" : "fs_quad_variant_lpass_tex");
            _quadProgram_tex_cutout = loadProgram(&reader, "vs_quad_variant_lpass_tex", _usePBR ? "fs_quad_variant_lpass_pbr_tex_cutout" : "fs_quad_variant_lpass_tex_cutout");

            _meshProgram = loadProgram(&reader, "vs_mesh_variant_lpass", _usePBR ? "fs_mesh_variant_lpass_pbr" : "fs_mesh_variant_lpass");
            _meshProgram_cutout = loadProgram(&reader, "vs_mesh_variant_lpass", _usePBR ? "fs_mesh_variant_lpass_pbr_cutout" : "fs_mesh_variant_lpass_cutout");
            break;
        }
        case opaque_fallback: {
            _skyboxProgram_linear = loadProgram(&reader, "vs_skybox", "fs_skybox_fallback");
            _skyboxProgram_cubemap = loadProgram(&reader, "vs_skybox", "fs_skybox_variant_cube");
            _skyboxProgram_cubemapLerp = loadProgram(&reader, "vs_skybox", "fs_skybox_variant_cubelerp");

            if (instancing) {
                const char *vs = compute ? "vs_clouds_instancing_variant_compute" : "vs_clouds_instancing_fallback";
                _cloudsProgram = loadProgram(&reader, vs, "fs_vcolor_fallback");
                _cloudsProgram_unlit = loadProgram(&reader, vs, "fs_vcolor_fallback");
            } else {
                _cloudsProgram = BGFX_INVALID_HANDLE;
                _cloudsProgram_unlit = BGFX_INVALID_HANDLE;
            }

            _shapeProgram = loadProgram(&reader, "vs_voxels_fallback", "fs_voxels_fallback");
            _shapeProgram_dm = loadProgram(&reader, "vs_voxels_variant_dm", "fs_voxels_variant_dm");

            _fontProgram = loadProgram(&reader, "vs_font_fallback", "fs_font_fallback");
            _fontProgram_unlit = loadProgram(&reader, "vs_font_fallback", "fs_font_variant_unlit");

            _quadProgram = loadProgram(&reader, "vs_quad_fallback", "fs_quad_fallback");
            _quadProgram_tex = loadProgram(&reader, "vs_quad_variant_tex", "fs_quad_variant_tex");
            _quadProgram_tex_cutout = loadProgram(&reader, "vs_quad_variant_tex", "fs_quad_variant_tex_cutout");

            _meshProgram = loadProgram(&reader, "vs_mesh_fallback", "fs_mesh_fallback");
            _meshProgram_cutout = loadProgram(&reader, "vs_mesh_fallback", "fs_mesh_variant_cutout");
            break;
        }
    }
    _fontProgram_overlay_unlit = loadProgram(&reader, "vs_font_fallback", "fs_font_variant_unlit");
    _fontProgram_overlay = loadProgram(&reader, "vs_font_fallback", "fs_font_fallback");
    _quadProgram_alpha = loadProgram(&reader, "vs_quad_fallback", "fs_quad_variant_alpha");
    _quadProgram_tex_alpha = loadProgram(&reader, "vs_quad_variant_tex", "fs_quad_variant_tex_alpha");
    _quadProgram_tex_cutout_alpha = loadProgram(&reader, "vs_quad_variant_tex", "fs_quad_variant_tex_cutout_alpha");
    _quadProgram_stencil = loadProgram(&reader, "vs_clear", "fs_clear");
    _quadProgram_stencil_tex_cutout = loadProgram(&reader, "vs_quad_variant_spass_ss_tex_cutout", "fs_clear_tex_cutout");
    _meshProgram_alpha = loadProgram(&reader, "vs_mesh_fallback", "fs_mesh_variant_alpha");

    //// TRANSPARENT PASS
    /// - does not contribute to deferred lighting at the moment
    /// - requires geometry to support order-independant-transparency
    /// - fallback uses unsorted alpha blending
    ///
    /// In this pass: SHAPE, QUAD
    typedef enum {
        transparent_oit,
        transparent_fallback
    } TransparentPass;
    const TransparentPass tpass = deferred ? transparent_oit : transparent_fallback;
    switch(tpass) {
        case transparent_oit: {
            _shapeProgram_weight = loadProgram(&reader, "vs_voxels_variant_tpass", "fs_transparency_weight_variant_nodm");
            _shapeProgram_dm_weight = loadProgram(&reader, "vs_voxels_variant_tpass_dm", "fs_transparency_weight_variant_dm");

            _fontProgram_weight = loadProgram(&reader, "vs_font_variant_tpass", "fs_transparency_weight_variant_font_nodm");

            _quadProgram_weight = loadProgram(&reader, "vs_quad_variant_tpass", "fs_transparency_weight_variant_nodm");
            _quadProgram_tex_weight = loadProgram(&reader, "vs_quad_variant_tpass_tex", "fs_transparency_weight_variant_tex_nodm");
            _quadProgram_tex_cutout_weight = loadProgram(&reader, "vs_quad_variant_tpass_tex_cutout", "fs_transparency_weight_variant_tex_cutout_nodm");
            break;
        }
        case transparent_fallback: {
            _shapeProgram_weight = BGFX_INVALID_HANDLE;
            _shapeProgram_dm_weight = BGFX_INVALID_HANDLE;

            _fontProgram_weight = BGFX_INVALID_HANDLE;

            _quadProgram_weight = BGFX_INVALID_HANDLE;
            _quadProgram_tex_weight = BGFX_INVALID_HANDLE;
            _quadProgram_tex_cutout_weight = BGFX_INVALID_HANDLE;
            break;
        }
    }

    //// COMPUTE
    if (compute) {
        _cs_cloudsUpdateProgram = bgfx::createProgram(loadShader(&reader, "cs_clouds_update"), true);
        if (indirect) {
            _cs_cloudsIndirectProgram = bgfx::createProgram(loadShader(&reader, "cs_clouds_indirect"), true);
        } else {
            _cs_cloudsIndirectProgram = BGFX_INVALID_HANDLE;
        }
    } else {
        _cs_cloudsUpdateProgram = BGFX_INVALID_HANDLE;
        _cs_cloudsIndirectProgram = BGFX_INVALID_HANDLE;
    }

    //// Final geometry blit
    if (deferred) {
        if (_features.linearDepthGBuffer) {
            _blitProgram = loadProgram(&reader, "vs_screen", "fs_blit_variant_mrt_ldepth");
            _blitProgram_alpha = loadProgram(&reader, "vs_screen", "fs_blit_variant_mrt_alpha_ldepth");
        } else {
            _blitProgram = loadProgram(&reader, "vs_screen", "fs_blit_variant_mrt");
            _blitProgram_alpha = loadProgram(&reader, "vs_screen", "fs_blit_variant_mrt_alpha");
        }
    } else {
        _blitProgram = loadProgram(&reader, "vs_screen", "fs_blit_fallback");
        _blitProgram_alpha = loadProgram(&reader, "vs_screen", "fs_blit_variant_alpha");
    }

    //// Post-process
    switch(_qualityParams.postTier) {
        case 1:
            if (_qualityParams.aa == AntiAliasing_FXAA) {
                _postProgram = loadProgram(&reader, "vs_screen", "fs_post_variant_low_fxaa");
            } else {
                _postProgram = BGFX_INVALID_HANDLE;
            }
            break;
        case 2:
            if (_qualityParams.aa == AntiAliasing_FXAA) {
                _postProgram = loadProgram(&reader, "vs_screen", "fs_post_variant_med_fxaa");
            } else {
                _postProgram = BGFX_INVALID_HANDLE;
            }
            break;
        case 3:
            if (_qualityParams.aa == AntiAliasing_FXAA) {
                _postProgram = loadProgram(&reader, "vs_screen", "fs_post_variant_high_fxaa");
            } else {
                _postProgram = BGFX_INVALID_HANDLE;
            }
            break;
        default:
            _postProgram = BGFX_INVALID_HANDLE;
            break;
    }

    //// MISC.
    /// Shader combinations used for other features & debug
    if (deferred) {
        if (_features.linearDepthGBuffer) {
            _vertexColorProgram = loadProgram(&reader, "vs_vcolor", _usePBR ? "fs_vcolor_variant_lpass_pbr_ldepth_unlit" : "fs_vcolor_variant_lpass_ldepth_unlit");
            _colorProgram = loadProgram(&reader, "vs_diffuse_fallback", _usePBR ? "fs_color_variant_lpass_pbr_ldepth" : "fs_color_variant_lpass_ldepth");
        } else {
            _vertexColorProgram = loadProgram(&reader, "vs_vcolor", _usePBR ? "fs_vcolor_variant_lpass_pbr_unlit" : "fs_vcolor_variant_lpass_unlit");
            _colorProgram = loadProgram(&reader, "vs_diffuse_fallback", _usePBR ? "fs_color_variant_lpass_pbr" : "fs_color_variant_lpass");
        }
    } else {
        _vertexColorProgram = loadProgram(&reader, "vs_vcolor", "fs_vcolor_fallback");
        _colorProgram = loadProgram(&reader, "vs_diffuse_fallback", "fs_color_fallback");
    }
    _uiColorProgram = loadProgram(&reader, "vs_diffuse_fallback", "fs_color_fallback");
    _uvProgram = loadProgram(&reader, "vs_diffuse_fallback", "fs_uv");
    _targetProgram = loadProgram(&reader, "vs_screen", "fs_target");

    _programsDirty = false;
}

void GameRenderer::releasePrograms() {
    bgfx::destroy(_shapeProgram);
    bgfx::destroy(_shapeProgram_dm);
    if (bgfx::isValid(_shapeProgram_weight)) {
        bgfx::destroy(_shapeProgram_weight);
    }
    if (bgfx::isValid(_shapeProgram_dm_weight)) {
        bgfx::destroy(_shapeProgram_dm_weight);
    }
    if (bgfx::isValid(_shapeProgram_sm)) {
        bgfx::destroy(_shapeProgram_sm);
    }
    if (bgfx::isValid(_blitProgram)) {
        bgfx::destroy(_blitProgram);
    }
    if (bgfx::isValid(_blitProgram_alpha)) {
        bgfx::destroy(_blitProgram_alpha);
    }
    if (bgfx::isValid(_lightProgram_point)) {
        bgfx::destroy(_lightProgram_point);
    }
    if (bgfx::isValid(_lightProgram_spot)) {
        bgfx::destroy(_lightProgram_spot);
    }
    if (bgfx::isValid(_lightProgram_directional)) {
        bgfx::destroy(_lightProgram_directional);
    }
    if (bgfx::isValid(_lightProgram_point_sm)) {
        bgfx::destroy(_lightProgram_point_sm);
    }
    if (bgfx::isValid(_lightProgram_spot_sm)) {
        bgfx::destroy(_lightProgram_spot_sm);
    }
    if (bgfx::isValid(_lightProgram_directional_sm)) {
        bgfx::destroy(_lightProgram_directional_sm);
    }
    if (bgfx::isValid(_cloudsProgram)) {
        bgfx::destroy(_cloudsProgram);
    }
    if (bgfx::isValid(_cloudsProgram_unlit)) {
        bgfx::destroy(_cloudsProgram_unlit);
    }
    if (bgfx::isValid(_cs_cloudsUpdateProgram)) {
        bgfx::destroy(_cs_cloudsUpdateProgram);
    }
    if (bgfx::isValid(_cs_cloudsIndirectProgram)) {
        bgfx::destroy(_cs_cloudsIndirectProgram);
    }
    bgfx::destroy(_fontProgram_unlit);
    bgfx::destroy(_fontProgram);
    bgfx::destroy(_fontProgram_overlay_unlit);
    bgfx::destroy(_fontProgram_overlay);
    if (bgfx::isValid(_fontProgram_weight)) {
        bgfx::destroy(_fontProgram_weight);
    }
    bgfx::destroy(_quadProgram);
    bgfx::destroy(_quadProgram_alpha);
    if (bgfx::isValid(_quadProgram_weight)) {
        bgfx::destroy(_quadProgram_weight);
    }
    bgfx::destroy(_quadProgram_tex);
    bgfx::destroy(_quadProgram_tex_cutout);
    bgfx::destroy(_quadProgram_tex_alpha);
    bgfx::destroy(_quadProgram_tex_cutout_alpha);
    if (bgfx::isValid(_quadProgram_tex_weight)) {
        bgfx::destroy(_quadProgram_tex_weight);
    }
    if (bgfx::isValid(_quadProgram_tex_cutout_weight)) {
        bgfx::destroy(_quadProgram_tex_cutout_weight);
    }
    if (bgfx::isValid(_quadProgram_sm)) {
        bgfx::destroy(_quadProgram_sm);
    }
    if (bgfx::isValid(_quadProgram_sm_tex_cutout)) {
        bgfx::destroy(_quadProgram_sm_tex_cutout);
    }
    bgfx::destroy(_quadProgram_stencil);
    bgfx::destroy(_quadProgram_stencil_tex_cutout);
    bgfx::destroy(_meshProgram);
    bgfx::destroy(_meshProgram_cutout);
    bgfx::destroy(_meshProgram_alpha);
    if (bgfx::isValid(_meshProgram_sm)) {
        bgfx::destroy(_meshProgram_sm);
    }
    if (bgfx::isValid(_postProgram)) {
        bgfx::destroy(_postProgram);
    }
    bgfx::destroy(_vertexColorProgram);
    bgfx::destroy(_skyboxProgram_linear);
    bgfx::destroy(_skyboxProgram_cubemap);
    bgfx::destroy(_skyboxProgram_cubemapLerp);
    bgfx::destroy(_colorProgram);
    bgfx::destroy(_uiColorProgram);
    bgfx::destroy(_uvProgram);
    bgfx::destroy(_targetProgram);
}

bgfx::FrameBufferHandle GameRenderer::getPipelineSegmentFramebuffer(const PipelineSegment s) {
    switch(s) {
        case PipelineSegment_PostTarget: return _targetsFbh;
        case PipelineSegment_AfterPostTarget: return _afterPostTargetsFbh;
        case PipelineSegment_SystemTarget: return _systemTargetsFbh;
    }
}

bool GameRenderer::createFramebuffers(uint16_t width, uint16_t height) {
    releaseFramebuffers();

    const uint64_t sampler_flags = 0
                                   | BGFX_SAMPLER_MIN_POINT
                                   | BGFX_SAMPLER_MAG_POINT
                                   | BGFX_SAMPLER_MIP_POINT
                                   | BGFX_SAMPLER_U_CLAMP
                                   | BGFX_SAMPLER_V_CLAMP;
    const uint64_t rt_flags = sampler_flags
                              | BGFX_TEXTURE_RT;
    const uint64_t blit_flags = sampler_flags
                                | BGFX_TEXTURE_BLIT_DST;

    //// Shared opaque albedo/color buffer
    _opaqueFbTex[0] = bgfx::createTexture2D(width, height, false, 1, bgfx::TextureFormat::RGBA8, rt_flags);
    ENSURE_VALID_F(_opaqueFbTex[0], releaseFramebuffers)

    //// Shared depth/stencil buffer
    _depthStencilBuffer = bgfx::createTexture2D(width, height, false, 1, _features.depthStencilFormat, rt_flags);
    ENSURE_VALID_F(_depthStencilBuffer, releaseFramebuffers)
    _depthStencilBlit = bgfx::createTexture2D(width, height, false, 1,
          _features.linearDepthGBuffer ? bgfx::TextureFormat::R16F : _features.depthStencilFormat, blit_flags);
    ENSURE_VALID_F(_depthStencilBlit, releaseFramebuffers)

    if (_features.useDeferred()) {
        //// Lighting framebuffer:
        /// - illumination accumulation (RGB8) + A8 unused here,
        /// - emission accumulation (RGB8) + unlit flag (A8)
        _lightFbTex[0] = bgfx::createTexture2D(width, height, false, 1, bgfx::TextureFormat::RGBA8, rt_flags);
        ENSURE_VALID_F(_lightFbTex[0], releaseFramebuffers)
        _lightFbTex[1] = bgfx::createTexture2D(width, height, false, 1, bgfx::TextureFormat::RGBA8, rt_flags);
        ENSURE_VALID_F(_lightFbTex[1], releaseFramebuffers)
        _lightFbh = bgfx::createFrameBuffer(2, _lightFbTex, false);
        ENSURE_VALID_F(_lightFbh, releaseFramebuffers)

        //// Opaque framebuffer:
        /// - albedo (RGBA8),
        /// - world normal (RGB8) + unlit flag for lights skip (A8),
        /// - illumination accumulation (RGB8 + A8 baked light value),
        /// - emission accumulation (RGB8) + unlit flag for final blit (A8),
        /// - optionally: metallic-roughness (RG8)
        /// - optionally: linear depth (R32F),
        /// - depth buffer
        _opaqueFbTex[1] = bgfx::createTexture2D(width, height, false, 1, bgfx::TextureFormat::RGBA8, rt_flags);
        ENSURE_VALID_F(_opaqueFbTex[1], releaseFramebuffers)
        _opaqueFbTex[2] = _lightFbTex[0];
        _opaqueFbTex[3] = _lightFbTex[1]; uint8_t i = 3;
        if (_usePBR) {
            _opaqueFbTex[++i] = bgfx::createTexture2D(width, height, false, 1, _features.mrt_rg8Format, rt_flags);
            ENSURE_VALID_F(_opaqueFbTex[i], releaseFramebuffers)
        }
        if (_features.linearDepthGBuffer) {
            _opaqueFbTex[++i] = bgfx::createTexture2D(width, height, false, 1, _features.mrt_r16fFormat, rt_flags);
            _opaqueFb_depthIdx = i;
            ENSURE_VALID_F(_opaqueFbTex[i], releaseFramebuffers)
        } else {
            _opaqueFb_depthIdx = i + 1;
        }
        _opaqueFbTex[++i] = _depthStencilBuffer;
        _opaqueFbh = bgfx::createFrameBuffer(i + 1, _opaqueFbTex, false);
        ENSURE_VALID_F(_opaqueFbh, releaseFramebuffers)
        for (++i; i < 7; ++i) {
            _opaqueFbTex[i] = BGFX_INVALID_HANDLE;
        }

        //// Transparency framebuffer: accumulation, weight, using opaque depth buffer
        _transparencyFbTex[0] = bgfx::createTexture2D(width, height, false, 1, _features.mrt_rgba16fFormat, rt_flags);
        ENSURE_VALID_F(_transparencyFbTex[0], releaseFramebuffers)
        _transparencyFbTex[1] = bgfx::createTexture2D(width, height, false, 1, _features.mrt_r16fFormat, rt_flags);
        ENSURE_VALID_F(_transparencyFbTex[1], releaseFramebuffers)
        _transparencyFbTex[2] = _depthStencilBuffer;
        _transparencyFbh = bgfx::createFrameBuffer(3, _transparencyFbTex, false);
        ENSURE_VALID_F(_transparencyFbh, releaseFramebuffers)
    } else {
        //// Opaque framebuffer:
        /// - color (RGBA8)
        /// - depth buffer
        _opaqueFbTex[1] = _depthStencilBuffer;
        _opaqueFbh = bgfx::createFrameBuffer(2, _opaqueFbTex, false);
        ENSURE_VALID_F(_opaqueFbh, releaseFramebuffers)
    }

    //// Targets framebuffer per pipeline segment, for blit & transparent (alpha-blending) passes,
    /// - post-process segment
    _targetsFbTex = bgfx::createTexture2D(width, height, false, 1, bgfx::TextureFormat::RGBA8, rt_flags);
    ENSURE_VALID_F(_targetsFbTex, releaseFramebuffers)
    bgfx::TextureHandle targetsFbTex[2] = { _targetsFbTex, _depthStencilBuffer };
    _targetsFbh = bgfx::createFrameBuffer(2, targetsFbTex, false);
    ENSURE_VALID_F(_targetsFbh, releaseFramebuffers)
    /// - after post-process segment
    _afterPostTargetsFbTex = bgfx::createTexture2D(width, height, false, 1, bgfx::TextureFormat::RGBA8, rt_flags);
    ENSURE_VALID_F(_afterPostTargetsFbTex, releaseFramebuffers)
    bgfx::TextureHandle afterPostTargetsFbTex[2] = { _afterPostTargetsFbTex, _depthStencilBuffer };
    _afterPostTargetsFbh = bgfx::createFrameBuffer(2, afterPostTargetsFbTex, false);
    ENSURE_VALID_F(_afterPostTargetsFbh, releaseFramebuffers)
    /// - system segment
    _systemTargetsFbTex = bgfx::createTexture2D(width, height, false, 1, bgfx::TextureFormat::RGBA8, rt_flags);
    ENSURE_VALID_F(_systemTargetsFbTex, releaseFramebuffers)
    bgfx::TextureHandle systemTargetsFbTex[2] = { _systemTargetsFbTex, _depthStencilBuffer };
    _systemTargetsFbh = bgfx::createFrameBuffer(2, systemTargetsFbTex, false);
    ENSURE_VALID_F(_systemTargetsFbh, releaseFramebuffers)

    //// Postprocess framebuffer, used to upscale target to backbuffer size (if needed) before applying postprocess
    if (bgfx::isValid(_postProgram) && _renderWidth < _screenWidth) {
        _postprocessFbTex = bgfx::createTexture2D(_screenWidth, _screenHeight, false, 1, bgfx::TextureFormat::RGBA8, rt_flags);
        ENSURE_VALID_F(_postprocessFbTex, releaseFramebuffers)
        _postprocessFbh = bgfx::createFrameBuffer(1, &_postprocessFbTex, false);
        ENSURE_VALID_F(_postprocessFbh, releaseFramebuffers)
    }

#if DEBUG_RENDERER
    if (bgfx::isValid(_lightFbh)) {
        bgfx::setName(_lightFbh, "Lighting");
    }
    bgfx::setName(_opaqueFbh, "G-buffer");
    if (bgfx::isValid(_transparencyFbh)) {
        bgfx::setName(_transparencyFbh, "OIT");
    }

    bgfx::setName(_targetsFbh, "Post-process Targets");
    bgfx::setName(_afterPostTargetsFbh, "After-Post Targets");
    bgfx::setName(_systemTargetsFbh, "System Targets");
#endif

    return true;
}

void GameRenderer::releaseFramebuffers() {
    if (_features.useDeferred()) {
        for (int i = 0; i < 2; ++i) {
            if (bgfx::isValid(_lightFbTex[i])) {
                bgfx::destroy(_lightFbTex[i]);
                _lightFbTex[i] = BGFX_INVALID_HANDLE;
            }
        }
        if (bgfx::isValid(_lightFbh)) {
            bgfx::destroy(_lightFbh);
            _lightFbh = BGFX_INVALID_HANDLE;
        }
        for (int i = 0; i < 7; ++i) {
            if (i != 2 && i != 3 && bgfx::isValid(_opaqueFbTex[i])) {
                bgfx::destroy(_opaqueFbTex[i]);
                _opaqueFbTex[i] = BGFX_INVALID_HANDLE;
            }
        }
        if (bgfx::isValid(_opaqueFbh)) {
            bgfx::destroy(_opaqueFbh);
            _opaqueFbh = BGFX_INVALID_HANDLE;
        }
        if (bgfx::isValid(_transparencyFbTex[0])) {
            bgfx::destroy(_transparencyFbTex[0]);
            _transparencyFbTex[0] = BGFX_INVALID_HANDLE;
        }
        if (bgfx::isValid(_transparencyFbTex[1])) {
            bgfx::destroy(_transparencyFbTex[1]);
            _transparencyFbTex[1] = BGFX_INVALID_HANDLE;
        }
        if (bgfx::isValid(_transparencyFbh)) {
            bgfx::destroy(_transparencyFbh);
            _transparencyFbh = BGFX_INVALID_HANDLE;
        }
    } else {
        for (int i = 0; i < 2; ++i) {
            if (bgfx::isValid(_opaqueFbTex[i])) {
                bgfx::destroy(_opaqueFbTex[i]);
                _opaqueFbTex[i] = BGFX_INVALID_HANDLE;
            }
        }
        if (bgfx::isValid(_opaqueFbh)) {
            bgfx::destroy(_opaqueFbh);
            _opaqueFbh = BGFX_INVALID_HANDLE;
        }
    }
    if (bgfx::isValid(_depthStencilBlit)) {
        bgfx::destroy(_depthStencilBlit);
        _depthStencilBlit = BGFX_INVALID_HANDLE;
    }
    if (bgfx::isValid(_targetsFbTex)) {
        bgfx::destroy(_targetsFbTex);
        _targetsFbTex = BGFX_INVALID_HANDLE;
    }
    if (bgfx::isValid(_targetsFbh)) {
        bgfx::destroy(_targetsFbh);
        _targetsFbh = BGFX_INVALID_HANDLE;
    }
    if (bgfx::isValid(_afterPostTargetsFbTex)) {
        bgfx::destroy(_afterPostTargetsFbTex);
        _afterPostTargetsFbTex = BGFX_INVALID_HANDLE;
    }
    if (bgfx::isValid(_afterPostTargetsFbh)) {
        bgfx::destroy(_afterPostTargetsFbh);
        _afterPostTargetsFbh = BGFX_INVALID_HANDLE;
    }
    if (bgfx::isValid(_systemTargetsFbTex)) {
        bgfx::destroy(_systemTargetsFbTex);
        _systemTargetsFbTex = BGFX_INVALID_HANDLE;
    }
    if (bgfx::isValid(_systemTargetsFbh)) {
        bgfx::destroy(_systemTargetsFbh);
        _systemTargetsFbh = BGFX_INVALID_HANDLE;
    }
    if (bgfx::isValid(_postprocessFbTex)) {
        bgfx::destroy(_postprocessFbTex);
        _postprocessFbTex = BGFX_INVALID_HANDLE;
    }
    if (bgfx::isValid(_postprocessFbh)) {
        bgfx::destroy(_postprocessFbh);
        _postprocessFbh = BGFX_INVALID_HANDLE;
    }
}

bool GameRenderer::createShadowmaps() {
    releaseShadowmaps();

    const uint16_t shadowmapSize = SHADOWMAP_MAX_SIZE[_qualityParams.shadowsTier];
    const uint64_t samplerFlags = 0
                                  | BGFX_SAMPLER_MIN_POINT
                                  | BGFX_SAMPLER_MAG_POINT
                                  | BGFX_SAMPLER_MIP_POINT
                                  | BGFX_SAMPLER_U_CLAMP
                                  | BGFX_SAMPLER_V_CLAMP;

    for (uint8_t i = 0; i < SHADOWMAP_CASCADES_COUNT[_qualityParams.shadowsTier]; ++i) {
        const uint16_t texSize = SHADOWMAP_CASCADES_SIZE_FACTORS[i] * shadowmapSize;
        if (_features.isShadowSamplerEnabled()) {
            _shadowmapTh[i] = bgfx::createTexture2D(texSize, texSize, false, 1, _features.depthFormat, BGFX_TEXTURE_RT | BGFX_SAMPLER_COMPARE_LEQUAL);
            ENSURE_VALID_F(_shadowmapTh[i], releaseShadowmaps)
            _shadowFbh[i] = bgfx::createFrameBuffer(1, &_shadowmapTh[i], true);
            ENSURE_VALID_F(_shadowFbh[i], releaseShadowmaps)
        } else {
            _shadowmapTh[i] = bgfx::createTexture2D(texSize, texSize, false, 1, bgfx::TextureFormat::RGBA8, BGFX_TEXTURE_RT | samplerFlags);
            ENSURE_VALID_F(_shadowmapTh[i], releaseShadowmaps)
            bgfx::TextureHandle handles[2];
            handles[0] = _shadowmapTh[i];
            handles[1] = bgfx::createTexture2D(texSize, texSize, false, 1, _features.depthFormat, BGFX_TEXTURE_RT_WRITE_ONLY);
            ENSURE_VALID_F(handles[1], releaseShadowmaps)
            _shadowFbh[i] = bgfx::createFrameBuffer(2, handles, true);
        }
    }

    _shadowmapsDirty = false;

    return true;
}

void GameRenderer::releaseShadowmaps() {
    for (uint8_t i = 0; i < SHADOWMAP_CASCADES_COUNT[_qualityParams.shadowsTier]; ++i) {
        if (bgfx::isValid(_shadowFbh[i])) {
            bgfx::destroy(_shadowFbh[i]);
            _shadowFbh[i] = BGFX_INVALID_HANDLE;
            _shadowmapTh[i] = BGFX_INVALID_HANDLE;
        }
    }
}

void GameRenderer::setGame(const Game_SharedPtr& g) {
    vx_assert(g != nullptr);

    if (_game == g) {
        return;
    }

    // in case previous game's resources were not released, but it is better if stop() is called
    // in advance to avoid stuttering
    if (_game != nullptr) {
        stop();
    }

    _game = g;
    CGame *cgame = _game->getCGame();
    
    _gameState = GameRendererState_SharedPtr(_game->getGameRendererState());
    _gameState->setAllDirty(); // Reset everything when swapping games

    // release existing current map shape if there's any
    if (_currentMapShape != nullptr) {
        shape_release(_currentMapShape);
    }
    // keep a strong reference on the new current map shape (can be NULL)
    _currentMapShape = game_get_map_shape(cgame);
    shape_retain(_currentMapShape);

    // different game i.e. different atlas data
    color_atlas_force_dirty_slice(game_get_color_atlas(cgame));

    _decals->reset(game_get_scene(cgame));
    _sky->reset(&_gameState->sky);

    refreshGlobalColors();
}

void GameRenderer::toggleInternal(bool value) {
    if (value) {
        setFogInternal();
        setCommonViews();
        refreshViewsInternal();
    } else {
        setDefaultViews();
    }
    _toggled = value;
}

void GameRenderer::clearObjectLists() {
    for(auto it = _cameras.begin(); it != _cameras.end(); ++it) {
        if (it->second != nullptr) {
            doubly_linked_list_flush(it->second, free);
            doubly_linked_list_free(it->second);
        }
    }
    _cameras.clear();

    for(auto it = _lights.begin(); it != _lights.end(); ++it) {
        if (it->second != nullptr) {
            doubly_linked_list_free(it->second);
        }
    }
    _lights.clear();
}

// MARK: - LIGHTING -

void GameRenderer::requestShadowDecal(uint32_t id, const float3 *pos, float width, float height, Transform *filterOut, uint32_t stencil) {
#if DEBUG_RENDERER_SHADOWS_FORCED_ON
      return;
#endif
    if (_decals->request(id, *pos, width, height, stencil, filterOut)) {
        _decals->setTexture(id, _cookie, false);
        _decals->setColor(id, 1.0f, 1.0f, 1.0f, SHADOW_COOKIE_ALPHA);
        _decals->setRange(id, SHADOW_COOKIE_RANGE, SHADOW_COOKIE_FADE, SHADOW_COOKIE_DIST_SIZE_RATIO);
    }
}

void GameRenderer::clearShadowDecals() {
    for(auto it = _shadowDecals.begin(); it != _shadowDecals.end(); ) {
        if (it->second) {
            it->second = false;
            ++it;
        } else {
            _decals->remove(it->first);
            it = _shadowDecals.erase(it);
        }
    }
}

VERTEX_LIGHT_STRUCT_T GameRenderer::sampleBakedLightingFromPoint(float x, float y, float z) {
    VERTEX_LIGHT_STRUCT_T sample;

    // TODO: sample baked lighting for generic shapes
    if (_currentMapShape != nullptr && utils_is_float3_to_coords_inbounds(x, y, z)) {
        const float3 *scale = shape_get_local_scale(_currentMapShape);
        const int xi = static_cast<int>(floorf(x / scale->x));
        const int yi = static_cast<int>(floorf(y / scale->y));
        const int zi = static_cast<int>(floorf(z / scale->z));

        sample = shape_get_light_or_default(_currentMapShape, xi, yi, zi);
    } else {
        DEFAULT_LIGHT(sample)
    }

    return sample;
}

void GameRenderer::setFogColor(const float3 *fog) {
    _uFogColorData[0] = fog->x;
    _uFogColorData[1] = fog->y;
    _uFogColorData[2] = fog->z;
    // w used for face shading factor
}

void GameRenderer::setSkyboxColors(const float3 *sky, const float3 *horizon, const float3 *abyss, const float3 *skyLight) {
    _uSkyColorData[0] = sky->x;
    _uSkyColorData[1] = sky->y;
    _uSkyColorData[2] = sky->z;
    // w used for fog length

    _uHorizonColorData[0] = horizon->x;
    _uHorizonColorData[1] = horizon->y;
    _uHorizonColorData[2] = horizon->z;
    // w used for skybox ambient factor

    _uAbyssColorData[0] = abyss->x;
    _uAbyssColorData[1] = abyss->y;
    _uAbyssColorData[2] = abyss->z;
    // w used for emissive fog

    _uSunColorData[0] = skyLight->x;
    _uSunColorData[1] = skyLight->y;
    _uSunColorData[2] = skyLight->z;
    // w used for fog start plane
}

void GameRenderer::refreshGlobalColors() {
    setFogColor(&_gameState->fogColor);
    setSkyboxColors(&_gameState->skyColor, &_gameState->horizonColor, &_gameState->abyssColor, &_gameState->skyLightColor);
}

void GameRenderer::setFogPlanes(float start, float end) {
    const float fogLength = fabsf(end - start);

    _uSunColorData[3] = start;
    _uSkyColorData[3] = float_isZero(fogLength, EPSILON_ZERO) ? 0.01f : fogLength; // avoid divide-by-zero
}

void GameRenderer::setFogEmissionValue(float v) {
    _uAbyssColorData[3] = v;
}

void GameRenderer::setFogInternal() {
    if (_gameState->fog) {
        setFogPlanes(_gameState->fogStart, _gameState->fogEnd);
        setFogEmissionValue(_gameState->fogEmission);
    } else {
        setFogPlanes(99999, 100000);
    }
}

// MARK: - CAMERA -

/*bool GameRenderer::createCameraTarget(GameRenderer::CameraEntry *entry, const float width, const float height) {
    vx_assert(entry != nullptr);

    const uint64_t samplerFlags = 0
                                  | BGFX_SAMPLER_MIN_POINT
                                  | BGFX_SAMPLER_MAG_POINT
                                  | BGFX_SAMPLER_MIP_POINT
                                  | BGFX_SAMPLER_U_BORDER
                                  | BGFX_SAMPLER_V_BORDER;

    entry->targetFbTex = bgfx::createTexture2D(width, height, false, 1, bgfx::TextureFormat::RGBA8, BGFX_TEXTURE_RT | samplerFlags);
    ENSURE_VALID(entry->targetFbTex)
    entry->targetFbh = bgfx::createFrameBuffer(1, &entry->targetFbTex, false);
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wclass-varargs"
    ENSURE_VALID_N(entry->targetFbh, 1, entry->targetFbTex)
#pragma GCC diagnostic pop
    entry->active = false;

    return true;
}

void GameRenderer::clearCameraTargets(const bool purge) {
    for(auto it = _targets.begin(); it != _targets.end(); ) {
        if (it->second.active && purge == false) {
            it->second.active = false;
            ++it;
        } else {
            // w/o MRT, target fb is opaque fb, which is not owned here
            if (_features.isMRTEnabled()) {
                bgfx::destroy(bgfx::getTexture(it->second.targetFbh, 0));
                bgfx::destroy(it->second.targetFbh);
            }
            it = _targets.erase(it);
        }
    }
}*/

// MARK: - SHAPE ENTRY -

bool GameRenderer::createShapeEntry(GameRenderer::ShapeBufferEntry *entry, uint32_t nbFaces) {
    vx_assert(entry != nullptr);

    entry->count = nbFaces;
    entry->inactivity = 0;
    entry->static_vbh = BGFX_INVALID_HANDLE;
    entry->dynamic_vbh = BGFX_INVALID_HANDLE;
#if SHAPE_PER_CHUNK_DRAW
    entry->vbh[GeometryRenderPass_Opaque] = BGFX_INVALID_HANDLE;
    entry->vbh[GeometryRenderPass_TransparentWeight] = BGFX_INVALID_HANDLE;
    entry->vbh[GeometryRenderPass_TransparentFallback] = BGFX_INVALID_HANDLE;
    entry->vbh[GeometryRenderPass_Shadowmap] = BGFX_INVALID_HANDLE;
    entry->vbh[GeometryRenderPass_StencilWrite] = BGFX_INVALID_HANDLE;
#endif

    return true;
}

void GameRenderer::setBuffer2DIndexing(uint32_t flat, uint16_t *u, uint16_t *v, bool isSize, uint16_t bufferSize) {
    if (isSize) {
        *u = static_cast<uint16_t>(minimum(flat, bufferSize));
        *v = static_cast<uint16_t>(minimum(flat / bufferSize + 1, bufferSize));
    } else {
        *u = static_cast<uint16_t>(minimum(flat % bufferSize, bufferSize - 1));
        *v = static_cast<uint16_t>(minimum(flat / bufferSize, bufferSize - 1));
    }
}

void GameRenderer::updateTex2D(const void *data, uint32_t size, bgfx::TextureHandle handle,
                               uint16_t tx, uint16_t ty, uint16_t tw, uint16_t th, const uint16_t pitch) {

    const bgfx::Memory* mem = bgfx::makeRef(data, size);
    bgfx::updateTexture2D(handle, 0, 0, tx, ty, tw, th, mem, pitch);
}

void GameRenderer::updateShapeBuffers(ShapeBufferEntry *entry, VertexAttributes *vertices,
                                      uint32_t start, uint32_t end) {
    if (bgfx::isValid(entry->dynamic_vbh)) {
        const bgfx::Memory *mem = bgfx::makeRef(vertices + start, (end - start + 1) * DRAWBUFFER_VERTICES_BYTES);
        bgfx::update(entry->dynamic_vbh, start, mem);
    } else {
        if (bgfx::isValid(entry->static_vbh)) {
            bgfx::destroy(entry->static_vbh);
        }
        const bgfx::Memory *mem = bgfx::makeRef(vertices, entry->count * DRAWBUFFER_VERTICES_BYTES);
        entry->static_vbh = bgfx::createVertexBuffer(mem, ShapeVertex::_shapeVertexLayout);
    }
}

void GameRenderer::updateShapeEntry(ShapeBufferEntry *entry, uint32_t nbVertices, VertexAttributes *vertices,
                                    DoublyLinkedList *slices) {
#if RENDERER_USE_SLICES
    // Update buffers entirely or using slices
    if (slices == nullptr) {
        updateShapeBuffers(entry, vertices, 0, nbVertices - 1);
    } else {
        DrawBufferWriteSlice *ws = static_cast<DrawBufferWriteSlice *>(doubly_linked_list_pop_first(
                slices));
        while (ws != nullptr) {
            updateShapeBuffers(entry, vertices, ws->from, ws->to);
            free(ws);
            ws = static_cast<DrawBufferWriteSlice *>(doubly_linked_list_pop_first(slices));
        }
    }
#else
    updateShapeBuffers(entry, vertices, 0, nbVertices - 1);
#endif
}

void GameRenderer::releaseShapeEntry(ShapeBufferEntry *entry) {
    if (bgfx::isValid(entry->static_vbh)) {
        bgfx::destroy(entry->static_vbh);
    }
    if (bgfx::isValid(entry->dynamic_vbh)) {
        bgfx::destroy(entry->dynamic_vbh);
    }
#if SHAPE_PER_CHUNK_DRAW
    entry->chunks.clear();
    if (bgfx::isValid(entry->vbh[GeometryRenderPass_Opaque])) {
        bgfx::destroy(entry->vbh[GeometryRenderPass_Opaque]);
    }
    if (bgfx::isValid(entry->vbh[GeometryRenderPass_TransparentWeight])) {
        bgfx::destroy(entry->vbh[GeometryRenderPass_TransparentWeight]);
    }
    if (bgfx::isValid(entry->vbh[GeometryRenderPass_TransparentFallback])) {
        bgfx::destroy(entry->vbh[GeometryRenderPass_TransparentFallback]);
    }
    if (bgfx::isValid(entry->vbh[GeometryRenderPass_Shadowmap])) {
        bgfx::destroy(entry->vbh[GeometryRenderPass_Shadowmap]);
    }
    if (bgfx::isValid(entry->vbh[GeometryRenderPass_StencilWrite])) {
        bgfx::destroy(entry->vbh[GeometryRenderPass_StencilWrite]);
    }
    entry->vbh.clear();
#endif
}

void GameRenderer::releaseShapeEntries(uint32_t count) {
    std::unordered_map<uint32_t, ShapeBufferEntry>::iterator itr;
    uint32_t i = 0;
    for (itr = _shapeEntries.begin(); itr != _shapeEntries.end() && i < count; ++itr, ++i) {
        releaseShapeEntry(&itr->second);
    }
    _shapeEntries.erase(_shapeEntries.begin(), itr);
}

void GameRenderer::popShapeEntries() {
    uint32_t vbId;
    std::unordered_map<uint32_t, ShapeBufferEntry>::iterator itr;

    while (vertex_buffer_pop_destroyed_id(&vbId)) {
        itr = _shapeEntries.find(vbId);
        if (itr != _shapeEntries.end()) {
            releaseShapeEntry(&itr->second);
            _shapeEntries.erase(vbId);
        }
    }
}

void GameRenderer::checkAndPopInactiveShapeEntries() {
    std::unordered_map<uint32_t, ShapeBufferEntry>::iterator itr;
    for (itr = _shapeEntries.begin(); itr != _shapeEntries.end(); ) {
        itr->second.inactivity++;
        if (itr->second.inactivity > RENDERER_DRAW_ENTRY_INACTIVE_FRAMES) {
            releaseShapeEntry(&itr->second);
            itr = _shapeEntries.erase(itr);
        } else {
            ++itr;
        }
    }
}

bgfx::ProgramHandle GameRenderer::getShapeEntryProgram(bool drawModes, bool weight) {
    if (drawModes) {
        return weight ? _shapeProgram_dm_weight : _shapeProgram_dm;
    } else {
        return weight ? _shapeProgram_weight : _shapeProgram;
    }
}

bool GameRenderer::shapeEntriesToDrawcalls(bgfx::ViewId id, const Transform *t, const Shape* shape, bool transparent,
                                           bool drawModes, float metadata, GeometryRenderPass pass,
                                           Plane **planes, bool perChunkDraw, bool worldDirty, bool cameraDirty,
                                           uint32_t depth, uint32_t stencil) {

    uint32_t vbId;
    uint32_t nbVertices, capacity;
    bool drawn = false;
    VertexAttributes *vertices;
    size_t nbSlices;
    const bool ss = _features.isShadowSamplerEnabled();
    uint32_t modelCache = BGFX_CONFIG_MAX_MATRIX_CACHE;
    Matrix4x4 model; transform_utils_get_model_ltw(t, &model);
#if SHAPE_PER_CHUNK_DRAW
    VertexBufferMemArea *vbma;
    Chunk *c;
    DoublyLinkedListNode *n;
    DrawBufferWriteSlice *drawcallSlice;
#endif

    VertexBuffer *vb = shape_get_first_vertex_buffer(shape, transparent);
    while(vb != nullptr) {
        vbId = vertex_buffer_get_id(vb);
        nbVertices = vertex_buffer_get_count(vb);
        capacity = vertex_buffer_get_max_count(vb);
        vertices = vertex_buffer_get_draw_buffer(vb);
        nbSlices = vertex_buffer_get_nb_draw_slices(vb);

        // Find draw entry for this vb ID
        std::unordered_map<uint32_t, ShapeBufferEntry>::iterator itr = _shapeEntries.find(vbId);
        bool isNew = itr == _shapeEntries.end();

        // Early skip if empty, release existing draw entry
        if (nbVertices == 0) {
            if (isNew == false) {
                releaseShapeEntry(&_shapeEntries[vbId]);
                _shapeEntries.erase(vbId);
            }
            vb = vertex_buffer_get_next(vb);
            continue;
        }

        // Create new draw entry if necessary, skip if max capabilities reached
        if (isNew) {
            ShapeBufferEntry entry;
            if (createShapeEntry(&entry, nbVertices) == false) {
                break;
            }
            _shapeEntries[vbId] = entry;
        }
        ShapeBufferEntry *drawEntry = &_shapeEntries[vbId];

        // Ensure vertex buffer, skip if max capabilities reached
#if SHAPE_PER_CHUNK_DRAW
        const bool _perChunkDraw = perChunkDraw && shape_get_nb_chunks(shape) > 1;
        if (_perChunkDraw == false) {
#else
        {
#endif
            const bool isDynamic = bgfx::isValid(drawEntry->dynamic_vbh) || nbVertices != drawEntry->count;
            if (isDynamic) {
                if (bgfx::isValid(drawEntry->static_vbh)) {
                    bgfx::destroy(drawEntry->static_vbh);
                    drawEntry->static_vbh = BGFX_INVALID_HANDLE;
                }
                if (bgfx::isValid(drawEntry->dynamic_vbh) == false) {
                    if (hasVBCapability(1, true) == false) {
                        vb = vertex_buffer_get_next(vb);
                        continue;
                    }
                    const bgfx::Memory *mem = bgfx::makeRef(vertices, nbVertices * DRAWBUFFER_VERTICES_BYTES);
                    drawEntry->dynamic_vbh = bgfx::createDynamicVertexBuffer(capacity, ShapeVertex::_shapeVertexLayout);
                    bgfx::update(drawEntry->dynamic_vbh, 0, mem);
                }
            } else if (bgfx::isValid(drawEntry->static_vbh) == false) {
                if (hasVBCapability(1, false) == false) {
                    vb = vertex_buffer_get_next(vb);
                    continue;
                }
                const bgfx::Memory *mem = bgfx::makeRef(vertices, nbVertices * DRAWBUFFER_VERTICES_BYTES);
                drawEntry->static_vbh = bgfx::createVertexBuffer(mem, ShapeVertex::_shapeVertexLayout);
            }
            vx_assert(bgfx::isValid(drawEntry->static_vbh) || bgfx::isValid(drawEntry->dynamic_vbh));
        }

        // Update draw entry count
        if (nbVertices != drawEntry->count) {
            if (nbVertices > capacity) {
                vxlog_warning("⚠️ shapeEntriesToDrawcalls: nbVertices > capacity, some data will be lost...");
                drawEntry->count = capacity;
            } else {
                drawEntry->count = nbVertices;
            }
        }

        // Upload buffer if structural changes happened
#if DEBUG_RENDERER_UPDATE_ALL
        updateDrawEntry(drawEntry, nbVertices, buffers, nullptr);
#else
        if (isNew == false && nbSlices > 0) {
            updateShapeEntry(drawEntry, nbVertices, vertices, vertex_buffer_get_draw_slices(vb));
        }
        //// else: nothing to update
        /// In this case, it is not a new entry and no data has been written. It means either
        /// nothing at all happened and we just want to draw as it was last frame, or there were
        /// only removals and this will be accounted for while passing the vertex buffer
#endif
        vertex_buffer_flush_draw_slices(vb);

#if SHAPE_PER_CHUNK_DRAW
        if (_perChunkDraw) {
            // Build drawcall slices based on chunks culling
            DoublyLinkedList *drawcallSlices = doubly_linked_list_new();
            vbma = vertex_buffer_get_first_mem_area(vb);
            uint32_t facesCount = 0;
            while (vbma != NULL) {
                c = vertex_buffer_mem_area_get_chunk(vbma);

                // Find chunk culling entry
                std::unordered_map<Chunk*, ChunkCullingEntry>::iterator chunks_itr = drawEntry->chunks.find(c);
                bool isNewChunk = chunks_itr == drawEntry->chunks.end();

                // Reset culling for each camera
                if (cameraDirty || isNewChunk) {
                    drawEntry->chunks[c].geometryCulled = false;
                }

                // Chunk already culled, skip
                if (drawEntry->chunks[c].geometryCulled) {
                    vbma = vertex_buffer_mem_area_get_global_next(vbma);
                    continue;
                }

                // World bounding sphere (cached)
                float3 center;
                float radius;
                if (worldDirty) {
                    Box aabb; chunk_get_bounding_box(c, &aabb.min, &aabb.max);
                    const SHAPE_COORDS_INT3_T chunkOrigin = chunk_get_origin(c);
                    aabb.min.x += chunkOrigin.x;
                    aabb.min.y += chunkOrigin.y;
                    aabb.min.z += chunkOrigin.z;
                    aabb.max.x += chunkOrigin.x;
                    aabb.max.y += chunkOrigin.y;
                    aabb.max.z += chunkOrigin.z;
                    Box waabb; shape_box_to_aabox(shape, &aabb, &waabb, false);
                    float3 lossyScale; shape_get_lossy_scale(shape, &lossyScale);
                    box_get_center(&waabb, &center);
                    radius = box_get_diagonal(&aabb) * 0.5f * float3_mmax(&lossyScale);

                    drawEntry->chunks[c].center = center;
                    drawEntry->chunks[c].radius = radius;
                } else {
                    center = drawEntry->chunks[c].center;
                    radius = drawEntry->chunks[c].radius;
                }

                // Bounding sphere culling (cached)
                bool culled = false;
                for (uint8_t i = 0; i < 6; ++i) {
                    if (plane_intersect_sphere(planes[i], &center, radius) < 0) {
                        culled = true;
                        break;
                    }
                }
                if (culled) {
                    drawEntry->chunks[c].geometryCulled = true;
                    vbma = vertex_buffer_mem_area_get_global_next(vbma);
                    continue;
                }

#if DEBUG_RENDERER_DISPLAY_CHUNKS_CULLSPHERES
                drawSphere(VIEW_OPAQUE, &center, radius, SORT_ORDER_DEFAULT, stencil);
#endif

                const uint32_t from = vertex_buffer_mem_area_get_start_idx(vbma);
                const uint32_t to = from + vertex_buffer_mem_area_get_count(vbma);

                n = doubly_linked_list_first(drawcallSlices);
                bool inserted = false;
                while (n != nullptr && inserted == false) {
                    drawcallSlice = static_cast<DrawBufferWriteSlice *>(doubly_linked_list_node_pointer(n));

                    if (drawcallSlice->from == to) {
                        drawcallSlice->from = from;
                        inserted = true;
                    } else if (drawcallSlice->to == from) {
                        drawcallSlice->to = to;
                        inserted = true;
                    }

                    n = doubly_linked_list_node_next(n);
                }
                if (inserted == false) {
                    drawcallSlice = static_cast<DrawBufferWriteSlice *>(malloc(sizeof(DrawBufferWriteSlice)));
                    drawcallSlice->from = from;
                    drawcallSlice->to = to;
                    doubly_linked_list_push_last(drawcallSlices, drawcallSlice);
                }
                facesCount += to - from;

                vbma = vertex_buffer_mem_area_get_global_next(vbma);
            }

            if (doubly_linked_list_is_empty(drawcallSlices)) {
                vb = vertex_buffer_get_next(vb);
                continue;
            }

            // Build a single vertex buffer based on drawcall slices
            n = doubly_linked_list_first(drawcallSlices);
            const bgfx::Memory* mem = bgfx::alloc(facesCount * DRAWBUFFER_VERTICES_PER_FACE_BYTES);
            VertexAttributes *cursor = (VertexAttributes*)mem->data;
            while (n != nullptr) {
                drawcallSlice = static_cast<DrawBufferWriteSlice *>(doubly_linked_list_node_pointer(n));

                const uint32_t sliceCount = drawcallSlice->to - drawcallSlice->from;
                memcpy(cursor, vertices + drawcallSlice->from * DRAWBUFFER_VERTICES_PER_FACE, sliceCount * DRAWBUFFER_VERTICES_PER_FACE_BYTES);
                cursor += sliceCount * DRAWBUFFER_VERTICES_PER_FACE;

                n = doubly_linked_list_node_next(n);
            }
#if SHAPE_PER_CHUNK_VB_MODE == 0
            if (bgfx::isValid(drawEntry->vbh[pass])) {
                bgfx::destroy(drawEntry->vbh[pass]);
            }
            drawEntry->vbh[pass] = bgfx::createVertexBuffer(mem, ShapeVertex::_shapeVertexLayout);
#elif SHAPE_PER_CHUNK_VB_MODE == 1
            if (bgfx::isValid(drawEntry->vbh[pass])) {
                bgfx::update(drawEntry->vbh[pass], 0, mem);
            } else {
                drawEntry->vbh[pass] = bgfx::createDynamicVertexBuffer(mem, ShapeVertex::_shapeVertexLayout, BGFX_BUFFER_ALLOW_RESIZE);
            }
#endif
            doubly_linked_list_flush(drawcallSlices, free);
            doubly_linked_list_free(drawcallSlices);

            // Binding textures to samplers
            if (pass != GeometryRenderPass_Shadowmap) {
                bgfx::setTexture(STAGE_SHAPE_PALETTE, _sColorAtlas, _colorAtlasTexture);

                // Vertex lighting sample: additionally bind uniform
                bgfx::setUniform(_uLighting, _uLightingData);
            }

            // Packing draw entry parameters
            _uParamsData[0] = static_cast<float>(drawEntry->texSize);
            _uParamsData[1] = static_cast<float>(drawEntry->vertTexSize);
            _uParamsData[2] = static_cast<float>(nbVertices);
            _uParamsData[3] = _gameState->bakedIntensity;
            bgfx::setUniform(_uParams, _uParamsData);

            // Packing draw entry override parameters
            if (pass != GeometryRenderPass_Shadowmap && drawModes) {
                bgfx::setUniform(_uOverrideParams, _uOverrideParamsData, 2);
                bgfx::setUniform(_uOverrideParams_fs, _uOverrideParamsData_fs, 1);
            }

            // Model matrix
            if (modelCache == BGFX_CONFIG_MAX_MATRIX_CACHE) {
                modelCache = bgfx::setTransform(model);
            } else {
                bgfx::setTransform(modelCache);
            }

            bgfx::setStencil(stencil);

            // Drawcall
            switch(pass) {
                case GeometryRenderPass_Opaque:
                    drawVoxels(id, drawEntry->vbh[pass], 0, facesCount,
                               getShapeEntryProgram(drawModes, false),
                               getRenderState(0, RenderPassState_Opacity, true),
                               SORT_ORDER_DEFAULT);
                    break;
                case GeometryRenderPass_TransparentFallback:
                    drawVoxels(id, drawEntry->vbh[pass], 0, facesCount,
                               getShapeEntryProgram(drawModes, false),
                               getRenderState(0, RenderPassState_AlphaBlending, true),
                               SORT_ORDER_DEFAULT);
                    break;
                case GeometryRenderPass_TransparentWeight:
                    drawVoxels(id, drawEntry->vbh[pass], 0, facesCount,
                               getShapeEntryProgram(drawModes, true),
                               getRenderState(0, RenderPassState_AlphaWeight, true),
                               SORT_ORDER_DEFAULT);
                    break;
                case GeometryRenderPass_Shadowmap:
                    // note: NO culling used for depth packing/sampling, to fit more closely when geometry in contact
                    drawVoxels(id, drawEntry->vbh[pass], 0, facesCount,
                               _shapeProgram_sm,
                               getRenderState(0, ss ? RenderPassState_DepthSampling : RenderPassState_DepthPacking, false),
                               SORT_ORDER_DEFAULT);
                    break;
                case GeometryRenderPass_StencilWrite:
                    vx_assert(false); // skipped early
                    break;
            }
        } else {
#else
        {
#endif

            if (pass != GeometryRenderPass_Shadowmap) {
                // Binding textures to samplers
                bgfx::setTexture(STAGE_SHAPE_PALETTE, _sColorAtlas, _colorAtlasTexture);

                // Bind uniforms once
                if (drawn == false) {
                    _uParamsData[0] = metadata;
                    _uParamsData[1] = 0.0f;
                    _uParamsData[2] = 0.0f;
                    _uParamsData[3] = 0.0f;
                    bgfx::setUniform(_uParams, _uParamsData);

                    _uParamsData_fs[0] = shape_is_unlit(shape) ? 1.0f : 0.0f;
                    _uParamsData_fs[1] = 0.0f;
                    _uParamsData_fs[2] = 0.0f;
                    _uParamsData_fs[3] = 0.0f;
                    bgfx::setUniform(_uParams_fs, _uParamsData_fs);

                    if (drawModes) {
                        bgfx::setUniform(_uOverrideParams, _uOverrideParamsData, 2);
                        bgfx::setUniform(_uOverrideParams_fs, _uOverrideParamsData_fs, 1);
                    }
                }
            }

            // Model matrix
            if (modelCache == BGFX_CONFIG_MAX_MATRIX_CACHE) {
                modelCache = bgfx::setTransform(&model);
            } else {
                bgfx::setTransform(modelCache);
            }

            bgfx::setStencil(stencil);

            // Drawcall
            switch(pass) {
                case GeometryRenderPass_Opaque:
                    drawVoxels(id, drawEntry,
                               getShapeEntryProgram(drawModes, false),
                               getRenderState(0, RenderPassState_Opacity),
                               depth);
                    break;
                case GeometryRenderPass_TransparentFallback:
                    drawVoxels(id, drawEntry,
                               getShapeEntryProgram(drawModes, false),
                               getRenderState(0, RenderPassState_AlphaBlending),
                               depth);
                    break;
                case GeometryRenderPass_TransparentWeight:
                    drawVoxels(id, drawEntry,
                               getShapeEntryProgram(drawModes, true),
                               getRenderState(0, RenderPassState_AlphaWeight),
                               depth);
                    break;
                case GeometryRenderPass_Shadowmap:
                    // note: NO culling used for depth packing/sampling, to fit more closely when geometry in contact
                    drawVoxels(id, drawEntry,
                               _shapeProgram_sm,
                               getRenderState(0, ss ? RenderPassState_DepthSampling : RenderPassState_DepthPacking, TriangleCull_None),
                               depth);
                    break;
                case GeometryRenderPass_TransparentPost:
                case GeometryRenderPass_StencilWrite:
                    break;
            }
        }

        drawn = true;
        drawEntry->inactivity = 0;

        vb = vertex_buffer_get_next(vb);
    }

    return drawn;
}

// MARK: - QUAD ENTRY -

bool GameRenderer::ensureQuadEntry(Quad *quad, QuadEntry **entry, bool batched, bool pointsScaling) {
    const uint32_t hash = quad_get_data_hash(quad);
    const uint32_t size = quad_get_data_size(quad);
    std::unordered_map<uint32_t, QuadEntry>::iterator itr = _quadEntries.find(hash);
    bool exists = itr != _quadEntries.end();

    if(size > 0) {
        if (exists) {
            *entry = &itr->second;
            if (batched) {
                QuadsBatch *batch = &itr->second.batch;
                if (batch->count + 1 > batch->capacity) {
                    const uint32_t capacity = minimum((batch->capacity > 0 ? batch->capacity * QUAD_BATCH_TEXTURED_FACTOR : QUAD_BATCH_TEXTURED_INIT), _quadsBatchTexturedMaxCount);
                    QuadVertex *v = (QuadVertex *)realloc(batch->vertices, capacity * VB_VERTICES * sizeof(QuadVertex));
                    if (v == nullptr) {
                        releaseQuadEntry(itr->first, &itr->second);
                        goto failure;
                    } else {
                        batch->vertices = v;
                        batch->capacity = capacity;
                    }
                }
            }
            if (itr->second.type == MediaType_Gif) {
                _gif->getTexture(hash, &(*entry)->tex);
            }
            itr->second.inactivity = minimum(0, itr->second.inactivity);
            return true;
        } else if (hasTextureCapability(1)) {
            QuadEntry newEntry;
            if (batched) {
                newEntry.batch = {
                    (QuadVertex *) malloc(QUAD_BATCH_TEXTURED_INIT * VB_VERTICES * sizeof(QuadVertex)),
                    0, QUAD_BATCH_TEXTURED_INIT, 0, BGFX_INVALID_HANDLE, BGFX_INVALID_HANDLE
                };
                if (newEntry.batch.vertices == nullptr) {
                    goto failure;
                }
            } else {
                newEntry.batch = { nullptr, 0, 0, 0, BGFX_INVALID_HANDLE, BGFX_INVALID_HANDLE };
            }
            newEntry.type = MediaType_None;
            newEntry.inactivity = 0;
            newEntry.tex = texture_none;
            newEntry.width = 0;
            newEntry.height = 0;
            _quadEntries[hash] = newEntry;
            *entry = &_quadEntries[hash];

            // parse media in background thread, then create texture(s) in main thread
            Weakptr *wptr = transform_get_and_retain_weakptr(quad_get_transform(quad));
            OperationQueue::getBackground()->dispatch([this, wptr, size, pointsScaling](){
                Transform *t = static_cast<Transform *>(weakptr_get(wptr));
                if (t != nullptr) {
                    Quad *q = static_cast<Quad *>(transform_get_ptr(t));
                    void *data = quad_get_data(q);

                    if (GifRenderer::isGif(data, size)) {
                        const uint32_t h = quad_get_data_hash(q);
                        if (_gif->parse(h, data, size)) {
                            OperationQueue::getMain()->dispatch([this, h]() {
                                if (_gif->bake(h)) {
                                    std::unordered_map<uint32_t, QuadEntry>::iterator itr = _quadEntries.find(h);
                                    if (itr != _quadEntries.end()) {
                                        _quadEntries[h].type = MediaType_Gif;
                                        _gif->getTexture(h, &_quadEntries[h].tex);
                                        _quadEntries[h].inactivity = -RENDERER_DRAW_ENTRY_GRACE_FRAMES;
                                    } else {
                                        _gif->remove(h);
                                    }
                                }
                            });
                        }
                    } else {
                        const uint64_t samplerFlags = quad_uses_filtering(q) ? BGFX_SAMPLER_NONE :
                                                      BGFX_SAMPLER_MIN_POINT | BGFX_SAMPLER_MAG_POINT | BGFX_SAMPLER_MIP_POINT;
                        const float pointsScale = pointsScaling ? _screenPPP : 1.0f;
                        const uint16_t maxWidth = static_cast<uint16_t>(minimum(quad_get_width(q) / quad_get_tiling_u(q) * pointsScale, QUAD_TEX_MAX_SIZE));
                        const uint16_t maxHeight = static_cast<uint16_t>(minimum(quad_get_height(q) / quad_get_tiling_v(q)  * pointsScale, QUAD_TEX_MAX_SIZE));
                        bgfx::TextureFormat::Enum format;
                        bimg::ImageContainer *imageContainer = parseImage(data, size, samplerFlags, maxWidth, maxHeight, &format);
                        if (imageContainer != nullptr) {
                            const uint32_t h = quad_get_data_hash(q);
                            const bool greyscale = format == bgfx::TextureFormat::A8 || format == bgfx::TextureFormat::R8;
                            OperationQueue::getMain()->dispatch([this, imageContainer, samplerFlags, h, greyscale]() {
                                uint16_t width, height;
                                bgfx::TextureHandle th = createTextureFromImage(imageContainer, samplerFlags, nullptr, nullptr,
                                                                                QUAD_TEX_MAX_SIZE, &width, &height);
                                if (bgfx::isValid(th)) {
                                    std::unordered_map<uint32_t, QuadEntry>::iterator itr = _quadEntries.find(h);
                                    if (itr != _quadEntries.end()) {
                                        _quadEntries[h].type = greyscale ? MediaType_Greyscale : MediaType_Image;
                                        _quadEntries[h].tex.th = th;
                                        _quadEntries[h].width = width;
                                        _quadEntries[h].height = height;
                                        _quadEntries[h].inactivity = -RENDERER_DRAW_ENTRY_GRACE_FRAMES;
                                    } else {
                                        bgfx::destroy(th);
                                    }
                                }
                            });
                        }
                    }
                }
                weakptr_release(wptr);
            });

            return true;
        }
    }

    goto failure;

    failure:
        *entry = nullptr;
        return false;
}

void GameRenderer::releaseQuadEntry(uint32_t hash, QuadEntry *entry) {
    free(entry->batch.vertices);
    if (entry->type == MediaType_Gif) {
        _gif->remove(hash);
    } else if (bgfx::isValid(entry->tex.th)) {
        bgfx::destroy(entry->tex.th);
    }
}

void GameRenderer::releaseQuadEntries() {
    for (auto itr = _quadEntries.begin(); itr != _quadEntries.end(); ++itr) {
        releaseQuadEntry(itr->first, &itr->second);
    }
    _quadEntries.clear();
}

void GameRenderer::checkAndPopInactiveQuadEntries() {
    for (auto itr = _quadEntries.begin(); itr != _quadEntries.end(); ) {
        itr->second.inactivity++;
        if (itr->second.inactivity > RENDERER_DRAW_ENTRY_INACTIVE_FRAMES) {
            releaseQuadEntry(itr->first, &itr->second);
            itr = _quadEntries.erase(itr);
        } else {
            ++itr;
        }
    }
}

// MARK: - MESH ENTRY -

bool GameRenderer::ensureMeshEntry(Mesh *mesh, MeshEntry **entry) {
    const Vertex* vb = mesh_get_vertex_buffer(mesh);
    const uint32_t vbCount = mesh_get_vertex_count(mesh);
    const void* ib = mesh_get_index_buffer(mesh);
    const uint32_t ibCount = mesh_get_index_count(mesh);

    *entry = nullptr;

    if (vb == nullptr || vbCount == 0) {
        return false;
    }

    const uint32_t hash = mesh_get_hash(mesh);
    std::unordered_map<uint32_t, MeshEntry>::iterator itr = _meshEntries.find(hash);
    bool exists = itr != _meshEntries.end();

    if (exists) {
        itr->second.inactivity = minimum(0, itr->second.inactivity);
        *entry = &itr->second;
        return true;
    } else if (hasVBCapability(1, false) && (ib == NULL || hasIBCapability(1, false))) {
        MeshEntry newEntry;
        newEntry.count = vbCount;
        newEntry.static_vbh = bgfx::createVertexBuffer(bgfx::makeRef(vb, vbCount * sizeof(Vertex)), MeshVertex::_meshVertexLayout);
        newEntry.dynamic_vbh = BGFX_INVALID_HANDLE;
        newEntry.inactivity = -RENDERER_DRAW_ENTRY_GRACE_FRAMES;

        if (ib != NULL) {
            const bool index32 = ibCount > UINT16_MAX;
            newEntry.static_ibh = bgfx::createIndexBuffer(bgfx::makeRef(ib, ibCount * (index32 ? sizeof(uint32_t) : sizeof(uint16_t))),
                                                          index32 ? BGFX_BUFFER_INDEX32 : BGFX_BUFFER_NONE);
        } else {
            newEntry.static_ibh = BGFX_INVALID_HANDLE;
        }
        newEntry.dynamic_ibh = BGFX_INVALID_HANDLE;

        _meshEntries[hash] = newEntry;
        *entry = &_meshEntries[hash];
        return true;
    }

    return false;
}

void GameRenderer::releaseMeshEntry(uint32_t hash, MeshEntry *entry) {
    if (bgfx::isValid(entry->static_vbh)) {
        bgfx::destroy(entry->static_vbh);
    }
    if (bgfx::isValid(entry->dynamic_vbh)) {
        bgfx::destroy(entry->dynamic_vbh);
    }
    if (bgfx::isValid(entry->static_ibh)) {
        bgfx::destroy(entry->static_ibh);
    }
    if (bgfx::isValid(entry->dynamic_ibh)) {
        bgfx::destroy(entry->dynamic_ibh);
    }
}

void GameRenderer::releaseMeshEntries() {
    for (auto itr = _meshEntries.begin(); itr != _meshEntries.end(); ++itr) {
        releaseMeshEntry(itr->first, &itr->second);
    }
    _meshEntries.clear();
}

void GameRenderer::checkAndPopInactiveMeshEntries() {
    std::unordered_map<uint32_t, MeshEntry>::iterator itr;
    for (itr = _meshEntries.begin(); itr != _meshEntries.end(); ) {
        itr->second.inactivity++;
        if (itr->second.inactivity > RENDERER_DRAW_ENTRY_INACTIVE_FRAMES) {
            releaseMeshEntry(itr->first, &itr->second);
            itr = _meshEntries.erase(itr);
        } else {
            ++itr;
        }
    }
}

// MARK: - TEXTURE ENTRY -

bool GameRenderer::ensureTextureEntry(Texture *tex, uint16_t maxDim, TextureEntry **entry) {
    if (tex == nullptr) {
        return false;
    }

    const uint32_t hash = texture_get_rendering_hash(tex);
    std::unordered_map<uint32_t, TextureEntry>::iterator itr = _textureEntries.find(hash);
    bool exists = itr != _textureEntries.end();

    if (exists) {
        itr->second.inactivity = minimum(0, itr->second.inactivity);
        *entry = &itr->second;
        return true;
    } else if (hasTextureCapability(1)) {
        TextureEntry newEntry;
        newEntry.th = BGFX_INVALID_HANDLE;
        newEntry.inactivity = 0;
        _textureEntries[hash] = newEntry;
        *entry = &_textureEntries[hash];

        if (texture_is_raw(tex)) {
            // parse in background thread, then create texture in main thread
            Weakptr *wptr = texture_get_and_retain_weakptr(tex);
            OperationQueue::getSlowBackground()->dispatch([this, wptr, maxDim]() {
                Texture *t = static_cast<Texture *>(weakptr_get(wptr));
                if (t != nullptr) {
                    texture_retain(t); // hold onto the texture until parsed

                    const uint64_t samplerFlags = texture_has_filtering(t) ? BGFX_SAMPLER_NONE :
                                                  BGFX_SAMPLER_MIN_POINT | BGFX_SAMPLER_MAG_POINT | BGFX_SAMPLER_MIP_POINT;
                    bgfx::TextureFormat::Enum format;
                    bimg::ImageContainer *imageContainer = parseImage(texture_get_data(t), texture_get_data_size(t), samplerFlags, 0, 0, &format);
                    if (imageContainer != nullptr) {
                        texture_set_parsed_data(t, imageContainer->m_data, imageContainer->m_size, imageContainer->m_width, imageContainer->m_height, format);
                        const uint32_t h = texture_get_rendering_hash(t);

                        texture_release(t); // done parsing

                        OperationQueue::getMain()->dispatch([this, imageContainer, h, samplerFlags, maxDim]() {
                            uint16_t width, height;
                            bgfx::TextureHandle th = createTextureFromImage(imageContainer, samplerFlags, nullptr, nullptr, maxDim, &width, &height);
                            if (bgfx::isValid(th)) {
                                std::unordered_map<uint32_t, TextureEntry>::iterator itr = _textureEntries.find(h);
                                if (itr != _textureEntries.end()) {
                                    itr->second.th = th;
                                    itr->second.inactivity = -RENDERER_DRAW_ENTRY_GRACE_FRAMES;
                                } else {
                                    bgfx::destroy(th);
                                }
                            }
                        });
                    }
                }
                weakptr_release(wptr);
            });
        } else {
            // directly create texture
            const uint64_t samplerFlags = texture_has_filtering(tex) ? BGFX_SAMPLER_NONE :
                                          BGFX_SAMPLER_MIN_POINT | BGFX_SAMPLER_MAG_POINT | BGFX_SAMPLER_MIP_POINT;
            const bgfx::TextureFormat::Enum format = static_cast<bgfx::TextureFormat::Enum>(texture_get_format(tex));
            const bgfx::Memory* mem = bgfx::makeRef(texture_get_data(tex), texture_get_data_size(tex));
            if (texture_get_type(tex) == TextureType_Cubemap) {
                (*entry)->th = bgfx::createTextureCube(texture_get_width(tex), false, 1, format, samplerFlags, mem);
            } else {
                (*entry)->th = bgfx::createTexture2D(texture_get_width(tex), texture_get_height(tex), false, 1, format, samplerFlags, mem);
            }
            (*entry)->inactivity = -RENDERER_DRAW_ENTRY_GRACE_FRAMES;
        }

        return true;
    }

    return false;
}

void GameRenderer::releaseTextureEntry(TextureEntry *entry) {
    if (bgfx::isValid(entry->th)) {
        bgfx::destroy(entry->th);
    }
}

void GameRenderer::releaseTextureEntries() {
    for (auto itr = _textureEntries.begin(); itr != _textureEntries.end(); ++itr) {
        releaseTextureEntry(&itr->second);
    }
    _textureEntries.clear();
}

void GameRenderer::checkAndPopInactiveTextureEntries() {
    std::unordered_map<uint32_t, TextureEntry>::iterator itr;
    for (itr = _textureEntries.begin(); itr != _textureEntries.end(); ) {
        itr->second.inactivity++;
        if (itr->second.inactivity > RENDERER_DRAW_ENTRY_INACTIVE_FRAMES) {
            releaseTextureEntry(&itr->second);
            itr = _textureEntries.erase(itr);
        } else {
            ++itr;
        }
    }
}

// MARK: - UTILS -

float GameRenderer::packNormalizedFloatsToFloat(float f1, float f2) {
    // use 12 bits to avoid going into too much loss when unpacking f2, precision is sufficient for 2 to 3 decimals
    const uint32_t shift = 4095;
    uint32_t uf1 = static_cast<uint32_t>(f1 * shift);
    uint32_t uf2 = static_cast<uint32_t>(f2 * shift);
    return static_cast<float>(uf2 * shift + uf1);
}

void GameRenderer::setVec4(float *data, uint32_t idx, float x, float y, float z, float w) {
    *(data + idx * 4) = x;
    *(data + idx * 4 + 1) = y;
    *(data + idx * 4 + 2) = z;
    *(data + idx * 4 + 3) = w;
}

uint32_t GameRenderer::lerpABGR32(uint32_t abgr1, uint32_t abgr2, float v) {
    float c1[4] = {
        uint8_t(abgr1 >> 24) / 255.0f,
        uint8_t(abgr1 >> 16) / 255.0f,
        uint8_t(abgr1 >> 8) / 255.0f,
        uint8_t(abgr1 >> 0) / 255.0f
    };
    float c2[4] = {
        uint8_t(abgr2 >> 24) / 255.0f,
        uint8_t(abgr2 >> 16) / 255.0f,
        uint8_t(abgr2 >> 8) / 255.0f,
        uint8_t(abgr2 >> 0) / 255.0f
    };
    return (uint8_t (LERP(c1[0], c2[0], v) * 255) << 24)
         + (uint8_t (LERP(c1[1], c2[1], v) * 255) << 16)
         + (uint8_t (LERP(c1[2], c2[2], v) * 255) << 8)
         + (uint8_t (LERP(c1[3], c2[3], v) * 255) << 0);
}

void GameRenderer::getFrustumWorldPoints(float3 *points, float nearPlane, float farPlane, float projWidth,
                                         float projHeight, const float *invView, float fov, bool perspective,
                                         float3 *outCenter) {

    float nw, nh, fw, fh;
    if (perspective) {
        // projection half-size at distance 1
        const float perspective_halfHeight = tanf(utils_deg2Rad(fov) * 0.5f);
        const float aspect = projWidth / projHeight;
        const float perspective_halfWidth = perspective_halfHeight * aspect;

        nw = nearPlane * perspective_halfWidth;
        nh = nearPlane * perspective_halfHeight;
        fw = farPlane * perspective_halfWidth;
        fh = farPlane * perspective_halfHeight;
    } else {
        nw = projWidth * 0.5f;
        nh = projHeight * 0.5f;
        fw = projWidth * 0.5f;
        fh = projHeight * 0.5f;
    }

    // frustum view space points
    const bx::Vec3 view[8] = {
        { -nw,  nh, nearPlane },
        {  nw,  nh, nearPlane },
        {  nw, -nh, nearPlane },
        { -nw, -nh, nearPlane },
        { -fw,  fh, farPlane  },
        {  fw,  fh, farPlane  },
        {  fw, -fh, farPlane  },
        { -fw, -fh, farPlane  }
    };

    // view to world space
    if (outCenter != nullptr) {
        *outCenter = { 0, 0, 0 };
    }
    for (uint8_t i = 0; i < 8; ++i) {
        bx::Vec3 world = bx::mul(view[i], invView);
        points[i] = { world.x, world.y, world.z };
        if (outCenter != nullptr) {
            outCenter->x += world.x;
            outCenter->y += world.y;
            outCenter->z += world.z;
        }
    }
    if (outCenter != nullptr) {
        outCenter->x /= 8;
        outCenter->y /= 8;
        outCenter->z /= 8;
    }
}

void GameRenderer::getFrustumWorldPoints2(float3 *points, float ndcNear, float ndcFar, const float *invViewProj, float3 *outCenter) {
    // clip space box
    const bx::Vec3 ndc[8] = {
        { -1.0f, -1.0f, ndcNear },
        {  1.0f, -1.0f, ndcNear },
        {  1.0f,  1.0f, ndcNear },
        { -1.0f,  1.0f, ndcNear },
        { -1.0f, -1.0f, ndcFar },
        {  1.0f, -1.0f, ndcFar },
        {  1.0f,  1.0f, ndcFar },
        { -1.0f,  1.0f, ndcFar }
    };

    // clip to world space
    if (outCenter != nullptr) {
        *outCenter = { 0, 0, 0 };
    }
    for (uint8_t i = 0; i < 8; ++i) {
        bx::Vec3 world = bx::mulH(ndc[i], invViewProj);
        points[i] = { world.x, world.y, world.z };
        if (outCenter != nullptr) {
            outCenter->x += world.x;
            outCenter->y += world.y;
            outCenter->z += world.z;
        }
    }
    if (outCenter != nullptr) {
        outCenter->x /= 8;
        outCenter->y /= 8;
        outCenter->z /= 8;
    }
}

void GameRenderer::splitFrustum(float nearPlane, float farPlane, uint8_t count, float *out, float linearWeight) {
    const float ratio = farPlane / nearPlane;
    const float length = farPlane - nearPlane;
    const uint8_t maxIdx = count * 2;

    out[0] = nearPlane;
    for (uint8_t i = 1; i < maxIdx - 1; i += 2) {
        const float norm = i / float(maxIdx);

        const float splitNear = (1.0f - linearWeight) * (nearPlane * bx::pow(ratio, norm)) + linearWeight * (nearPlane + length * norm);
        out[i + 1] = splitNear;
        out[i] = splitNear * 1.005f; // slight overlap over next near plane
    }
    out[maxIdx - 1] = farPlane;
}

void GameRenderer::splitFrustumNDC(uint8_t count, float *out, float interval) {
    const uint8_t maxIdx = count * 2;

    out[maxIdx - 1] = 1.0f;
    for (uint8_t i = maxIdx - 2; i > 0; i -= 2) {
        const float splitNear = out[i + 1] - interval;
        out[i] = splitNear;
        out[i - 1] = splitNear + interval * .005f; // slight overlap over next near plane
    }
    out[0] = -1.0f;
}

void GameRenderer::splitFrustumNDC_Hardcoded(uint8_t count, float *out) {
    vx_assert(count > 0 && count <= 4);

    switch(count) {
        case 4: {
            out[0] = 0.0f;     // shadowmap 1 (nearest)
            out[1] = 0.993f;

            out[2] = 0.993f;   // shadowmap 2
            out[3] = 0.997f;

            out[4] = 0.997f;   // shadowmap 3
            out[5] = 0.999f;

            out[6] = 0.999f;   // shadowmap 4 (farthest)
            out[7] = 1.0f;
            break;
        }
        case 3: {
            out[0] = 0.0f;     // shadowmap 1 (nearest)
            out[1] = 0.993f;

            out[2] = 0.993f;   // shadowmap 2
            out[3] = 0.997f;

            out[4] = 0.997f;   // shadowmap 3 (farthest)
            out[5] = 1.0f;
            break;
        }
        case 2: {
            out[0] = 0.0f;     // shadowmap 1 (nearest)
            out[1] = 0.997f;

            out[2] = 0.997f;   // shadowmap 2 (farthest)
            out[3] = 1.0f;
            break;
        }
        case 1: {
            out[0] = 0.0f;     // no cascade
            out[1] = 1.0f;
            break;
        }
    }

    if (_features.NDCDepth) {
        for (uint8_t i = 0; i < count; ++i) {
            out[i] = out[i] * 2.0f - 1.0f;
        }
    }
}

void GameRenderer::computeWorldPlanes(Transform *t, float3 pos, float3 viewOffset, float nearPlane,
                                      float farPlane, float fov_rad, bool perspective, float projWidth, float projHeight,
                                      Plane **out) {
    
    float3 right; transform_get_right(t, &right, false);
    float3 up; transform_get_up(t, &up, false);
    float3 forward; transform_get_forward(t, &forward, false);
    float3 p;

    if (perspective) {
        const float far_halfHeight = farPlane * tanf(fov_rad * 0.5f);
        const float aspect = projWidth / projHeight;
        const float far_halfWidth = far_halfHeight * aspect;
        const float3 farVec = {
            forward.x *  farPlane, forward.y *  farPlane, forward.z *  farPlane };

        // right & left planes
        p = { farVec.x + right.x * (far_halfWidth + viewOffset.x),
              farVec.y + right.y * (far_halfWidth + viewOffset.x),
              farVec.z + right.z * (far_halfWidth + viewOffset.x) };
        float3_cross_product(&p, &up);
        float3_normalize(&p);
        out[0] = plane_new_from_point2(&pos, &p);
        p = { farVec.x - right.x * (far_halfWidth - viewOffset.x),
              farVec.y - right.y * (far_halfWidth - viewOffset.x),
              farVec.z - right.z * (far_halfWidth - viewOffset.x) };
        float3_cross_product2(&up, &p);
        float3_normalize(&p);
        out[1] = plane_new_from_point2(&pos, &p);

        // top & bottom planes
        p = { farVec.x + up.x * (far_halfHeight + viewOffset.y),
              farVec.y + up.y * (far_halfHeight + viewOffset.y),
              farVec.z + up.z * (far_halfHeight + viewOffset.y) };
        float3_cross_product2(&right, &p);
        float3_normalize(&p);
        out[2] = plane_new_from_point2(&pos, &p);
        p = { farVec.x - up.x * (far_halfHeight - viewOffset.y),
              farVec.y - up.y * (far_halfHeight - viewOffset.y),
              farVec.z - up.z * (far_halfHeight - viewOffset.y) };
        float3_cross_product(&p, &right);
        float3_normalize(&p);
        out[3] = plane_new_from_point2(&pos, &p);
    } else {
        const float halfWidth = projWidth * 0.5f;
        const float halfHeight = projHeight * 0.5f;

        // right & left planes
        const float rd = float3_dot_product(&pos, &right);
        out[0] = plane_new(-right.x, -right.y, -right.z, -(rd + viewOffset.x + halfWidth));
        out[1] = plane_new2(&right, rd + viewOffset.x - halfWidth);

        // top & bottom planes
        const float ud = float3_dot_product(&pos, &up);
        out[2] = plane_new(-up.x, -up.y, -up.z, -(ud + viewOffset.y + halfHeight));
        out[3] = plane_new2(&up, ud + viewOffset.y - halfHeight);
    }

    // near & far planes
    const float fd = float3_dot_product(&pos, &forward);
    out[4] = plane_new2(&forward, fd + nearPlane + viewOffset.z);
    out[5] = plane_new(-forward.x, -forward.y, -forward.z, -(fd + farPlane - viewOffset.z));
}

float GameRenderer::rgb2luma(float r, float g, float b) {
    return r * 0.2126729f + g * 0.7151522f + b * 0.0721750f;
}

bool GameRenderer::isTex2DFormatSupported(const bgfx::Caps* caps, bgfx::TextureFormat::Enum format) {
    return (caps->formats[format] & BGFX_CAPS_FORMAT_TEXTURE_2D) == BGFX_CAPS_FORMAT_TEXTURE_2D;
}

// MARK: - VIEW TRANSFORM -

void GameRenderer::setViewTransformFromCamera(bgfx::ViewId id, Camera *camera, float width,
                                              float height, float *viewOut, float *projOut) {

    const Matrix4x4 *view = camera_get_view_matrix(camera);

    if (viewOut != nullptr) {
        memcpy(viewOut, view, 16 * sizeof(float));
    }

    setViewTransform(id, view, camera_get_mode(camera) == Orthographic,
                     width, height, _features.NDCDepth, projOut, true,
                     camera_get_near(camera), camera_get_far(camera),
                     utils_rad2Deg(camera_utils_get_vertical_fov(camera, width / height)));
}

void GameRenderer::setViewTransform(bgfx::ViewId id, bx::Vec3 pos, bx::Vec3 lookAt,
                                    float width, float height, float *viewOut, float *projOut) {

    float view[16];
    bx::mtxLookAt(view, pos, lookAt);

    if (viewOut != nullptr) {
        memcpy(viewOut, view, 16 * sizeof(float));
    }

    setViewTransform (id, view, false, width, height, _features.NDCDepth, projOut, true);
}

void GameRenderer::setViewTransform(bgfx::ViewId id, const void *view, bool orthographic, float width,
                                    float height, bool NDCDepth, float *projOut, bool cameraView,
                                    float nearPlane, float farPlane, float fov) {

    float proj[16];
    computeProjMtx(orthographic, width, height, NDCDepth, cameraView, nearPlane, farPlane, fov, proj);

    if (projOut != nullptr) {
        memcpy(projOut, proj, 16 * sizeof(float));
    }

    bgfx::setViewTransform(id, view, proj);
}

void GameRenderer::computeProjMtx(bool orthographic, float width, float height, bool NDCDepth, bool cameraView,
                                  float nearPlane, float farPlane, float fov, float *projOut) {
    vx_assert(projOut != nullptr);

    float proj[16];
    if(orthographic) {
        if (cameraView) {
            const float width2 = width * 0.5f;
            const float height2 = height * 0.5f;
            bx::mtxOrtho(proj, -width2, width2, -height2, height2, 0.0f, farPlane, 0.0f, NDCDepth);
        } else {
            bx::mtxOrtho(proj, 0.0f, width, height, 0.0f, 0.0f, farPlane, 0.0f, NDCDepth);
        }
    } else {
        bx::mtxProj(proj, fov, width/height, nearPlane, farPlane, NDCDepth);
    }

    memcpy(projOut, proj, 16 * sizeof(float));
}

// MARK: - PRIMITIVES -

void GameRenderer::setFullScreenSpaceQuadTransient(bool flipV, const float *invViewProj, const float3 *viewPos) {
    if (bgfx::getAvailTransientVertexBuffer(3, ScreenQuadVertex::_screenquadVertexLayout) == 3) {
        bgfx::TransientVertexBuffer vb;
        bgfx::allocTransientVertexBuffer(&vb, 3, ScreenQuadVertex::_screenquadVertexLayout);
        ScreenQuadVertex* vertex = reinterpret_cast<ScreenQuadVertex*>(vb.data);

        // this is a single triangle positionned so that quad rect is contained within
        const float minx = -1.0f;
        const float maxx = 1.0f;
        const float miny = 0.0f;
        const float maxy = 2.0f;

        const float minu = -1.0f;
        const float maxu =  1.0f;

        float minv, maxv;
        if (flipV) {
            minv = -1.0f;
            maxv = 1.0f;
        } else {
            minv = 2.0f;
            maxv = 0.0f;
        }

        vertex[0]._x = minx;
        vertex[0]._y = miny;
        vertex[0]._u = minu;
        vertex[0]._v = maxv;

        vertex[1]._x = maxx;
        vertex[1]._y = maxy;
        vertex[1]._u = maxu;
        vertex[1]._v = minv;

        vertex[2]._x = maxx;
        vertex[2]._y = miny;
        vertex[2]._u = maxu;
        vertex[2]._v = maxv;

        // optionally include eye world distance to far points
        if (invViewProj != nullptr && viewPos != nullptr) {
            // clip space
            const bx::Vec3 ndcFar[3] = {
                { -3.0f, 1.0f, 1.0f },
                {  1.0f, 1.0f, 1.0f },
                {  1.0f,  -3.0f, 1.0f }
            };

            // clip to world space
            for (uint8_t i = 0; i < 3; ++i) {
                bx::Vec3 world = bx::mulH(ndcFar[i], invViewProj);
                vertex[i]._farX = world.x - viewPos->x;
                vertex[i]._farY = world.y - viewPos->y;
                vertex[i]._farZ = world.z - viewPos->z;
            }
        }

        bgfx::setVertexBuffer(0, &vb);
    }
}

void GameRenderer::setScreenSpaceQuadTransient(float x, float y, float width, float height, float fullWidth,
                                               float fullHeight, float uMax, float vMax, bool flipV,
                                               const float *invViewProj, const float3 *viewPos) {

    if (bgfx::getAvailTransientVertexBuffer(4, ScreenQuadVertex::_screenquadVertexLayout) == 4
        && bgfx::getAvailTransientIndexBuffer(6, false) == 6) {

        bgfx::TransientVertexBuffer vb;
        bgfx::allocTransientVertexBuffer(&vb, 4, ScreenQuadVertex::_screenquadVertexLayout);
        ScreenQuadVertex* vertex = reinterpret_cast<ScreenQuadVertex*>(vb.data);

        const float quadX = x / fullWidth;
        const float quadY = y / fullHeight;
        const float quadWidth = width / fullWidth;
        const float quadHeight = height / fullHeight;

        const float maxx = quadX + quadWidth;
        const float maxy = quadY + quadHeight;

        const float minu = 0.0f;
        const float maxu = uMax;

        float minv, maxv;
        if (flipV) {
            minv = 1.0f - vMax;
            maxv = 1.0f;
        } else {
            minv = vMax;
            maxv = 0.0f;
        }

        vertex[0]._x = quadX;
        vertex[0]._y = quadY;
        vertex[0]._u = minu;
        vertex[0]._v = maxv;

        vertex[1]._x = maxx;
        vertex[1]._y = quadY;
        vertex[1]._u = maxu;
        vertex[1]._v = maxv;

        vertex[2]._x = maxx;
        vertex[2]._y = maxy;
        vertex[2]._u = maxu;
        vertex[2]._v = minv;

        vertex[3]._x = quadX;
        vertex[3]._y = maxy;
        vertex[3]._u = minu;
        vertex[3]._v = minv;

        // optionally include eye world distance to far points
        if (invViewProj != nullptr && viewPos != nullptr) {
            // clip space
            const bx::Vec3 ndcFar[4] = {
                { -1.0f, 1.0f, 1.0f },
                {  1.0f, 1.0f, 1.0f },
                {  1.0f,  -1.0f, 1.0f },
                { -1.0f,  -1.0f, 1.0f }
            };

            // clip to world space
            for (uint8_t i = 0; i < 4; ++i) {
                bx::Vec3 world = bx::mulH(ndcFar[i], invViewProj);
                vertex[i]._farX = world.x - viewPos->x;
                vertex[i]._farY = world.y - viewPos->y;
                vertex[i]._farZ = world.z - viewPos->z;
            }
        }

        bgfx::setVertexBuffer(0, &vb);

        bgfx::TransientIndexBuffer ib;
        bgfx::allocTransientIndexBuffer(&ib, 6, false);
        uint16_t* index = reinterpret_cast<uint16_t*>(ib.data);

        index[0] = 2;
        index[1] = 1;
        index[2] = 0;

        index[3] = 3;
        index[4] = 2;
        index[5] = 0;

        bgfx::setIndexBuffer(&ib);
    }
}

void GameRenderer::drawVoxels(bgfx::ViewId view, ShapeBufferEntry *entry, bgfx::ProgramHandle ph, uint64_t state, uint32_t depth) {
    // Set appropriate vertex buffer
    if (bgfx::isValid(entry->static_vbh)) {
        bgfx::setVertexBuffer(0, entry->static_vbh);
    } else if (bgfx::isValid(entry->dynamic_vbh)) {
        bgfx::setVertexBuffer(0, entry->dynamic_vbh);
    } else {
#if DEBUG
        vx_assert(false);
#endif
    }

    // Set index buffer from a sub-range of the full static buffer
    bgfx::setIndexBuffer(_voxelIbh, 0, entry->count * IB_INDICES / VB_VERTICES);

    // Relevant state should be provided directly based on the render pass we're in
    bgfx::setState(state);

    // Submit primitive for rendering to the provided view
    bgfx::submit(view, ph, depth);
}

void GameRenderer::drawVoxels(bgfx::ViewId view, bgfx::VertexBufferHandle vbh, const uint32_t from,
                              const uint32_t to, bgfx::ProgramHandle ph, uint64_t state, uint32_t depth) {

    // Set static vertex buffer
    bgfx::setVertexBuffer(0, vbh);

    // Set index buffer from a sub-range of the full static buffer
    bgfx::setIndexBuffer(_voxelIbh, from * IB_INDICES / VB_VERTICES, (to - from) * IB_INDICES / VB_VERTICES);

    // Relevant state should be provided directly based on the render pass we're in
    bgfx::setState(state);

    // Submit primitive for rendering to the provided view
    bgfx::submit(view, ph, depth);
}

void GameRenderer::drawVoxels(bgfx::ViewId view, bgfx::DynamicVertexBufferHandle vbh, const uint32_t from,
                              const uint32_t to, bgfx::ProgramHandle ph, uint64_t state, uint32_t depth) {

    // Set dynamic vertex buffer
    bgfx::setVertexBuffer(0, vbh);

    // Set index buffer from a sub-range of the full static buffer
    bgfx::setIndexBuffer(_voxelIbh, from * IB_INDICES / VB_VERTICES, (to - from) * IB_INDICES / VB_VERTICES);

    // Relevant state should be provided directly based on the render pass we're in
    bgfx::setState(state);

    // Submit primitive for rendering to the provided view
    bgfx::submit(view, ph, depth);
}

void GameRenderer::drawStatic(bgfx::ViewId view, bgfx::VertexBufferHandle vbh, bgfx::IndexBufferHandle ibh,
                              bgfx::ProgramHandle ph, uint64_t state, uint32_t depth, const void *mtx) {

    // Optionally set model matrix
    if (mtx != nullptr) {
        bgfx::setTransform(mtx);
    }

    // Set static vertex and index buffer
    bgfx::setVertexBuffer(0, vbh);
    bgfx::setIndexBuffer(ibh);

    // Relevant state should be provided directly based on the render pass we're in
    bgfx::setState(state);

    // Submit primitive for rendering to given view
    bgfx::submit(view, ph, depth);
}

void GameRenderer::drawDynamic(bgfx::ViewId view, bgfx::DynamicVertexBufferHandle vbh, bgfx::DynamicIndexBufferHandle ibh,
                               bgfx::ProgramHandle ph, uint64_t state, uint32_t depth, const void *mtx) {

    // Optionally set model matrix
    if (mtx != nullptr) {
        bgfx::setTransform(mtx);
    }

    // Set dynamic vertex and index buffer
    bgfx::setVertexBuffer(0, vbh);
    bgfx::setIndexBuffer(ibh);

    // Relevant state should be provided directly based on the render pass we're in
    bgfx::setState(state);

    // Submit primitive for rendering to given view
    bgfx::submit(view, ph, depth);
}

void GameRenderer::drawTransient(bgfx::ViewId view, bgfx::TransientVertexBuffer vbh, bgfx::TransientIndexBuffer ibh,
                                 bgfx::ProgramHandle ph, uint64_t state, uint32_t depth, const void *mtx) {

    // Optionally set model matrix
    if (mtx != nullptr) {
        bgfx::setTransform(mtx);
    }

    // Set transient vertex and index buffer
    bgfx::setVertexBuffer(0, &vbh);
    bgfx::setIndexBuffer(&ibh);

    // Relevant state should be provided directly based on the render pass we're in
    bgfx::setState(state);

    // Submit primitive for rendering to given view
    bgfx::submit(view, ph, depth);
}

// MARK: - LOADING / ALLOC -

bgfx::ProgramHandle GameRenderer::loadProgram(bx::FileReaderI* _reader, const char* _vsName, const char* _fsName) {
    bgfx::ShaderHandle vsh = loadShader(_reader, _vsName);
    bgfx::ShaderHandle fsh = BGFX_INVALID_HANDLE;
    if (_fsName != nullptr) {
        fsh = loadShader(_reader, _fsName);
    }

    return bgfx::createProgram(vsh, fsh, true /* destroy shaders when program is destroyed */);
}


bgfx::ShaderHandle GameRenderer::loadShader(bx::FileReaderI* _reader, const char* _name) {
    char filePath[512];

    const char* shaderPath = "???";

    switch (bgfx::getRendererType()) {
        case bgfx::RendererType::Noop:
        case bgfx::RendererType::Direct3D11:
        case bgfx::RendererType::Direct3D12: shaderPath = "shaders/dx11/";  break;
        case bgfx::RendererType::Gnm:        shaderPath = "shaders/pssl/";  break;
        case bgfx::RendererType::Metal:      shaderPath = "shaders/metal/"; break;
        case bgfx::RendererType::Nvn:        shaderPath = "shaders/nvn/";   break;
        case bgfx::RendererType::OpenGL:     shaderPath = "shaders/glsl/";  break;
        case bgfx::RendererType::OpenGLES:   shaderPath = "shaders/essl/";  break;
        case bgfx::RendererType::Vulkan:     shaderPath = "shaders/spirv/"; break;

        case bgfx::RendererType::Agc:
        case bgfx::RendererType::Count:
            vxlog_error("⚠️ loadShader unknown renderer type, this should not happen!");
            break;
    }

    bx::strCopy(filePath, BX_COUNTOF(filePath), shaderPath);
    bx::strCat(filePath, BX_COUNTOF(filePath), _name);
    bx::strCat(filePath, BX_COUNTOF(filePath), ".bin");

    bgfx::ShaderHandle handle = bgfx::createShader(loadMem(_reader, filePath));
    bgfx::setName(handle, _name);

    return handle;
}

const bgfx::Memory* GameRenderer::loadMem(bx::FileReaderI* _reader, const char* _filePath) {
    if (bx::open(_reader, _filePath)) {
        uint32_t size = static_cast<uint32_t>(bx::getSize(_reader));
        const bgfx::Memory* mem = bgfx::alloc(size+1);
        bx::read(_reader, mem->data, size, nullptr);
        bx::close(_reader);
        mem->data[mem->size-1] = '\0';
        return mem;
    }

    vxlog_error("⚠️ ⚠️ ⚠️ loadMem failed %s", _filePath);
    return nullptr;
}

static void releaseImageContainer(void* ptr, void* data) {
    bimg::ImageContainer* imageContainer = static_cast<bimg::ImageContainer*>(data);
    bimg::imageFree(imageContainer);
}

bimg::ImageContainer* GameRenderer::parseImage(const void* data, uint32_t size, uint64_t samplerFlags, uint16_t maxWidth, uint16_t maxHeight, bgfx::TextureFormat::Enum *outFormat) {
    static bx::DefaultAllocator *parseAllocator = new bx::DefaultAllocator();
    bimg::ImageContainer* imageContainer = bimg::imageParse(parseAllocator, data, size);
    
    if (imageContainer == nullptr) {
        return nullptr;
    }
    if (outFormat != nullptr) {
        *outFormat = static_cast<bgfx::TextureFormat::Enum>(imageContainer->m_format);
    }
    
    // currently only resize 8-bit formats
    bool resize = (maxWidth > 0 && imageContainer->m_width > maxWidth) || (maxHeight > 0 && imageContainer->m_height > maxHeight);
    uint8_t channels = 0;
    switch(imageContainer->m_format) {
        case bimg::TextureFormat::BGRA8:
        case bimg::TextureFormat::RGBA8:
            channels = 4;
            break;
        case bimg::TextureFormat::RGB8:
            channels = 3;
            break;
        case bimg::TextureFormat::RG8:
            channels = 2;
            break;
        case bimg::TextureFormat::R8:
            channels = 1;
            break;
        default:
            resize = false;
            break;
    }

    if (resize) {
        const uint16_t dstWidth = minimum(imageContainer->m_width, maxWidth);
        const uint16_t dstHeight = minimum(imageContainer->m_height, maxHeight);
        const uint32_t dstSize = dstWidth * dstHeight * channels;
        uint8_t *dstData = (uint8_t *) malloc(dstSize);
        const int result = stbir_resize_uint8((const uint8_t *) imageContainer->m_data,
                                              imageContainer->m_width, imageContainer->m_height, 0,
                                              dstData, dstWidth, dstHeight, 0,
                                              channels);

        if (result == 0) {
            free(dstData);
            return imageContainer;
        }

        bimg::ImageContainer* output = imageAlloc(parseAllocator,
                                                  imageContainer->m_format,
                                                  dstWidth, dstHeight, 0, 1,
                                                  false, false,
                                                  dstData);
        bimg::imageFree(imageContainer);
        imageContainer = output;
    }

    // check if texture format & parameters are supported
    // note: BGFX converts unsupported formats to BGRA8 on the render thread, to avoid that,
    // we use BGRA8 as default when parsing on the background thread
    bgfx::TextureFormat::Enum dstFormat = static_cast<bgfx::TextureFormat::Enum>(imageContainer->m_format);
    const bool supported = isTex2DFormatSupported(bgfx::getCaps(), dstFormat) && bgfx::isTextureValid(0, false, 1, dstFormat, samplerFlags);
    if (supported == false) {
        bimg::ImageContainer* output = imageConvert(parseAllocator, bimg::TextureFormat::BGRA8, *imageContainer);
        bimg::imageFree(imageContainer);
        imageContainer = output;
        if (outFormat != nullptr) {
            *outFormat = bgfx::TextureFormat::BGRA8;
        }
    }

    return imageContainer;
}

bgfx::TextureHandle GameRenderer::createTextureFromImage(bimg::ImageContainer* imageContainer, uint64_t flags,
                                                         bgfx::TextureInfo* info, bimg::Orientation::Enum* orientation,
                                                         uint16_t maxDim, uint16_t *outWidth, uint16_t *outHeight) {

    bgfx::TextureHandle th = BGFX_INVALID_HANDLE;
    if (imageContainer != nullptr) {
        if (orientation != nullptr) {
            *orientation = imageContainer->m_orientation;
        }

        const uint16_t width = uint16_t(imageContainer->m_width);
        const uint16_t height = uint16_t(imageContainer->m_height);
        const uint16_t depth = uint16_t(imageContainer->m_depth);
        if (outWidth != nullptr) {
            *outWidth = width;
        }
        if (outHeight != nullptr) {
            *outHeight = height;
        }

        if (width > maxDim || height > maxDim || depth > maxDim) {
            bimg::imageFree(imageContainer);
            return BGFX_INVALID_HANDLE;
        }

        const bgfx::Memory* mem = bgfx::makeRef(
            imageContainer->m_data,
            imageContainer->m_size,
            releaseImageContainer,
            imageContainer
        );

        if (imageContainer->m_cubeMap) {
            th = bgfx::createTextureCube(width,
                                         1 < imageContainer->m_numMips,
                                         imageContainer->m_numLayers,
                                         bgfx::TextureFormat::Enum(imageContainer->m_format),
                                         flags, mem);
#if RENDERER_ALLOW_TEX3D
        } else if (imageContainer->m_depth > 1) {
            th = bgfx::createTexture3D(width, height, depth,
                                       1 < imageContainer->m_numMips,
                                       bgfx::TextureFormat::Enum(imageContainer->m_format),
                                       flags, mem);
#endif
        } else if (bgfx::isTextureValid(0, false, imageContainer->m_numLayers,
                                        bgfx::TextureFormat::Enum(imageContainer->m_format), flags)) {

            th = bgfx::createTexture2D(width, height,
                                       1 < imageContainer->m_numMips,
                                       imageContainer->m_numLayers,
                                       bgfx::TextureFormat::Enum(imageContainer->m_format),
                                       flags, mem);
        } else {
            bimg::imageFree(imageContainer);
            return BGFX_INVALID_HANDLE;
        }

        if (info != nullptr) {
            bgfx::calcTextureSize(*info,
                                  width, height, depth,
                                  imageContainer->m_cubeMap,
                                  1 < imageContainer->m_numMips,
                                  imageContainer->m_numLayers,
                                  bgfx::TextureFormat::Enum(imageContainer->m_format));
        }
    }
    return th;
}

bgfx::TextureHandle GameRenderer::loadTexture(const void* data, size_t size, uint64_t flags,
                                              bgfx::TextureInfo* info, bimg::Orientation::Enum* orientation,
                                              uint16_t maxDim, uint16_t *outWidth, uint16_t *outHeight) {
    bgfx::TextureHandle th = BGFX_INVALID_HANDLE;
    if (data != nullptr && size > 0) {
        if (_bxAllocator == nullptr) {
            _bxAllocator = new bx::DefaultAllocator();
        }
        bimg::ImageContainer* imageContainer = bimg::imageParse(_bxAllocator, data, static_cast<uint32_t>(size));

        th = createTextureFromImage(imageContainer, flags, info, orientation, maxDim, outWidth, outHeight);
    }
    return th;
}

bgfx::TextureHandle GameRenderer::loadTextureFromFile(const char* filePath, uint64_t flags,
                                                      bgfx::TextureInfo* info, bimg::Orientation::Enum* orientation) {

    bgfx::TextureHandle handle = BGFX_INVALID_HANDLE;
    size_t size;
    void* data = fs::getFileContent(fs::openBundleFile(filePath), &size);
    if (data != nullptr) {
        handle = loadTexture(data, size, flags, info, orientation, false);
        bx::free(_bxAllocator, data, 0);

        if (bgfx::isValid(handle)) {
            bgfx::setName(handle, filePath);
        }
    }

    return handle;
}

bgfx::TextureHandle GameRenderer::loadTextureCubeFromFiles(
        uint16_t size, uint64_t flags,
        const char* f0, const char* f1, const char* f2,
        const char* f3, const char* f4, const char* f5) {

    bgfx::TextureHandle handle = BGFX_INVALID_HANDLE;
    size_t texSize = size * size * 4;
    size_t cubeSize = texSize * 6;
    const bgfx::Memory *mem = bgfx::alloc(static_cast<uint32_t>(cubeSize));
    uint8_t *cursor = mem->data;

    const char* files[6] = { f0, f1, f2, f3, f4, f5 };

    int width = 0;
    int height = 0;

    for (uint8_t i = 0; i < 6; i++) {
        FILE *f = fs::openBundleFile(files[i]);
        if (f != nullptr) {
            void* image = stbi_load_from_file (f, &width, &height, nullptr, 4);
            fclose(f);

            if (image == nullptr) {
                vxlog_error("⚠️⚠️⚠️loadTextureCubeFromFiles: failed loading %s", files[i]);
                return handle;
            }
            if (width != size || height != size) {
                vxlog_error("⚠️⚠️⚠️loadTextureCubeFromFiles: size mismatch %s", files[i]);
                return handle;
            }

            bx::memCopy(cursor, image, texSize);
            cursor += texSize;

            stbi_image_free(image);
        } else {
            vxlog_error("⚠️⚠️⚠️loadTextureCubeFromFiles: failed opening %s", files[i]);
            return handle;
        }
    }

    handle = bgfx::createTextureCube(width, false, 1, bgfx::TextureFormat::Enum::RGBA8, flags, mem);

    return handle;
}

void GameRenderer::releaseTex2Ds(int num, ...) {
    bgfx::TextureHandle arg;
    va_list valist; va_start(valist, num);
    for (int i = 0; i < num; ++i) {
        arg = va_arg(valist, bgfx::TextureHandle);
        if (bgfx::isValid(arg)) {
            bgfx::destroy(arg);
        }
    }
    va_end(valist);
}

// MARK: - DEBUG -

#if DEBUG_RENDERER

void GameRenderer::debug_displayStats() {
    const bgfx::Stats* stats = bgfx::getStats();
    if (_debug_textLine_left != 0) {
        _debug_textLine_left++;
    }
    bgfx::dbgTextPrintf(1, ++_debug_textLine_left, 0x0f, "\x1b[26;Backbuffer: \x1b[31;%dx%d", stats->width, stats->height);
    bgfx::dbgTextPrintf(1, ++_debug_textLine_left, 0x0f, "\x1b[26;CPU(frame): \x1b[31;%d", static_cast<int>(stats->cpuTimeFrame / static_cast<float>(stats->cpuTimerFreq) * 1000));
    bgfx::dbgTextPrintf(1, ++_debug_textLine_left, 0x0f, "\x1b[26;CPU(render): \x1b[31;%d", static_cast<int>((stats->cpuTimeEnd - stats->cpuTimeBegin) / static_cast<float>(stats->cpuTimerFreq) * 1000));
    bgfx::dbgTextPrintf(1, ++_debug_textLine_left, 0x0f, "\x1b[26;GPU: \x1b[31;%d", static_cast<int>((stats->gpuTimeEnd - stats->gpuTimeBegin) / static_cast<float>(stats->gpuTimerFreq) * 1000));
    bgfx::dbgTextPrintf(1, ++_debug_textLine_left, 0x0f, "\x1b[26;Draws: \x1b[31;%d", stats->numDraw);
    bgfx::dbgTextPrintf(1, ++_debug_textLine_left, 0x0f, "\x1b[26;VBs: \x1b[31;%d", stats->numVertexBuffers);
    bgfx::dbgTextPrintf(1, ++_debug_textLine_left, 0x0f, "\x1b[26;IBs: \x1b[31;%d", stats->numIndexBuffers);
    bgfx::dbgTextPrintf(1, ++_debug_textLine_left, 0x0f, "\x1b[26;Tex: \x1b[31;%d", stats->numTextures);
    bgfx::dbgTextPrintf(1, ++_debug_textLine_left, 0x0f, "\x1b[26;Unis: \x1b[31;%d", stats->numUniforms);
    bgfx::dbgTextPrintf(1, ++_debug_textLine_left, 0x0f, "\x1b[26;Entries: \x1b[31;%d", _shapeEntries.size());
    bgfx::dbgTextPrintf(1, ++_debug_textLine_left, 0x0f, "\x1b[26;Lights: \x1b[31;%d/%d", _debug_lights_drawn, _debug_lights);
    bgfx::dbgTextPrintf(1, ++_debug_textLine_left, 0x0f, "\x1b[26;Targets: \x1b[31;%d/%d", _maxTarget, _debug_cameras);
    bgfx::dbgTextPrintf(1, ++_debug_textLine_left, 0x0f, "\x1b[26;Shadows pass: \x1b[31;%d", _debug_shadows_pass);

    _debug_lights = 0;
    _debug_lights_drawn = 0;
    _debug_cameras = 0;
    _debug_shadows_pass = 0;
}

uint32_t GameRenderer::debug_countShapeVertices(Shape *value, bool transparent) {
    const VertexBuffer *vb = shape_get_first_vertex_buffer(value, transparent);
    uint32_t count = 0;
    while(vb != nullptr) {
        count += vertex_buffer_get_count(vb);
        vb = vertex_buffer_get_next(vb);
    }
    return count;
}

void GameRenderer::debug_displayGameInfo_recurse(Transform *t, uint32_t *shapesCount, uint32_t *chunks, uint32_t *blocks, uint32_t *shapesVertices, uint32_t *quadsCount, uint32_t *meshesCount, uint32_t *meshesVertices) {
    DoublyLinkedListNode *n = transform_get_children_iterator(t);
    Transform *child = nullptr;
    Shape *s = nullptr;
    while (n != nullptr) {
        child = static_cast<Transform *>(doubly_linked_list_node_pointer(n));

        switch(transform_get_type(child)) {
            case ShapeTransform:
                s = static_cast<Shape *>(transform_get_ptr(child));
                (*shapesCount)++;
                *chunks += shape_get_nb_chunks(s);
                *blocks += shape_get_nb_blocks(s);
                *shapesVertices += debug_countShapeVertices(s, false) + debug_countShapeVertices(s, true);
                break;
            case QuadTransform:
                (*quadsCount)++;
                break;
            case MeshTransform:
                (*meshesCount)++;
                *meshesVertices += mesh_get_vertex_count(static_cast<Mesh *>(transform_get_ptr(child)));
                break;
            default:
                break;
        }

        debug_displayGameInfo_recurse(child, shapesCount, chunks, blocks, shapesVertices, quadsCount, meshesCount, meshesVertices);

        n = doubly_linked_list_node_next(n);
    }
}

void GameRenderer::debug_displayGameInfo(const Game_SharedPtr& g) {
    if (_debug_textLine_left != 0) {
        _debug_textLine_left++;
    }

    uint32_t shapesCount = 0, chunks = 0, blocks = 0, shapesVertices = 0, quadsCount = 0, meshesCount = 0, meshesVertices = 0;
    const char *prefix = "\x1b[14;|";
    Shape *mapShape = game_get_map_shape(g->getCGame());
    Scene *scene = game_get_scene(g->getCGame());
    Shape *s;
    Transform *t, *c;
    DoublyLinkedListNode *n;

    debug_displayGameInfo_recurse(scene_get_root(game_get_scene(_game->getCGame())), &shapesCount, &chunks, &blocks, &shapesVertices, &quadsCount, &meshesCount, &meshesVertices);

    const int totalShapesDrawn = _debug_shapes_opaque_drawn + _debug_shapes_transparent_drawn + _debug_shapes_shadowmap_drawn;
    bgfx::dbgTextPrintf(1, ++_debug_textLine_left, 0x0f, "\x1b[14;SHAPES");
    bgfx::dbgTextPrintf(1, ++_debug_textLine_left, 0x0f, "%s\x1b[26;Count: \x1b[31;%d", prefix, shapesCount);
    bgfx::dbgTextPrintf(1, ++_debug_textLine_left, 0x0f, "%s\x1b[26;Drawn/Culled: \x1b[31;%d/%d", prefix, totalShapesDrawn, _debug_shapes_culled);
    bgfx::dbgTextPrintf(1, ++_debug_textLine_left, 0x0f, "%s-\x1b[26; opaque: \x1b[31;%d", prefix, _debug_shapes_opaque_drawn);
    bgfx::dbgTextPrintf(1, ++_debug_textLine_left, 0x0f, "%s-\x1b[26; transparent: \x1b[31;%d", prefix, _debug_shapes_transparent_drawn);
    bgfx::dbgTextPrintf(1, ++_debug_textLine_left, 0x0f, "%s-\x1b[26; shadowmap: \x1b[31;%d", prefix, _debug_shapes_shadowmap_drawn);
    bgfx::dbgTextPrintf(1, ++_debug_textLine_left, 0x0f, "%s\x1b[26;Chunks: \x1b[31;%d", prefix, chunks);
    bgfx::dbgTextPrintf(1, ++_debug_textLine_left, 0x0f, "%s\x1b[26;Blocks: \x1b[31;%d", prefix, blocks);
    bgfx::dbgTextPrintf(1, ++_debug_textLine_left, 0x0f, "%s\x1b[26;Vertices: \x1b[31;%d", prefix, shapesVertices);

    const int totalQuadsDrawn = _debug_quads_opaque_drawn + _debug_quads_transparent_drawn + _debug_quads_stencil_drawn + _debug_quads_shadowmap_drawn;
    bgfx::dbgTextPrintf(1, ++_debug_textLine_left, 0x0f, "\x1b[14;QUADS");
    bgfx::dbgTextPrintf(1, ++_debug_textLine_left, 0x0f, "%s\x1b[26;Count: \x1b[31;%d", prefix, quadsCount);
    bgfx::dbgTextPrintf(1, ++_debug_textLine_left, 0x0f, "%s\x1b[26;Drawn/Culled: \x1b[31;%d/%d", prefix, totalQuadsDrawn, _debug_quads_culled);
    bgfx::dbgTextPrintf(1, ++_debug_textLine_left, 0x0f, "%s-\x1b[26; opaque: \x1b[31;%d", prefix, _debug_quads_opaque_drawn);
    bgfx::dbgTextPrintf(1, ++_debug_textLine_left, 0x0f, "%s-\x1b[26; transparent: \x1b[31;%d", prefix, _debug_quads_transparent_drawn);
    bgfx::dbgTextPrintf(1, ++_debug_textLine_left, 0x0f, "%s-\x1b[26; stencil: \x1b[31;%d", prefix, _debug_quads_stencil_drawn);
    bgfx::dbgTextPrintf(1, ++_debug_textLine_left, 0x0f, "%s-\x1b[26; shadowmap: \x1b[31;%d", prefix, _debug_quads_shadowmap_drawn);

    const int totalMeshesDrawn = _debug_meshes_opaque_drawn + _debug_meshes_transparent_drawn + _debug_meshes_shadowmap_drawn;
    bgfx::dbgTextPrintf(1, ++_debug_textLine_left, 0x0f, "\x1b[14;MESHES");
    bgfx::dbgTextPrintf(1, ++_debug_textLine_left, 0x0f, "%s\x1b[26;Count: \x1b[31;%d", prefix, meshesCount);
    bgfx::dbgTextPrintf(1, ++_debug_textLine_left, 0x0f, "%s\x1b[26;Drawn/Culled: \x1b[31;%d/%d", prefix, totalMeshesDrawn, _debug_meshes_culled);
    bgfx::dbgTextPrintf(1, ++_debug_textLine_left, 0x0f, "%s-\x1b[26; opaque: \x1b[31;%d", prefix, _debug_meshes_opaque_drawn);
    bgfx::dbgTextPrintf(1, ++_debug_textLine_left, 0x0f, "%s-\x1b[26; transparent: \x1b[31;%d", prefix, _debug_meshes_transparent_drawn);
    bgfx::dbgTextPrintf(1, ++_debug_textLine_left, 0x0f, "%s-\x1b[26; shadowmap: \x1b[31;%d", prefix, _debug_meshes_shadowmap_drawn);
    bgfx::dbgTextPrintf(1, ++_debug_textLine_left, 0x0f, "%s\x1b[26;Vertices: \x1b[31;%d", prefix, meshesVertices);

    _debug_shapes_culled = 0;
    _debug_shapes_opaque_drawn = 0;
    _debug_shapes_transparent_drawn = 0;
    _debug_shapes_shadowmap_drawn = 0;
    _debug_quads_culled = 0;
    _debug_quads_opaque_drawn = 0;
    _debug_quads_transparent_drawn = 0;
    _debug_quads_stencil_drawn = 0;
    _debug_quads_shadowmap_drawn = 0;
    _debug_meshes_culled = 0;
    _debug_meshes_opaque_drawn = 0;
    _debug_meshes_transparent_drawn = 0;
    _debug_meshes_shadowmap_drawn = 0;
}

void GameRenderer::debug_getShapeVBOccupancy(Shape *s, uint32_t *count, uint32_t *maxCount,
                                             uint32_t *mem, uint32_t *maxMem, uint32_t *vbCount,
                                             bool transparent) {

    const VertexBuffer *vb = shape_get_first_vertex_buffer(s, transparent);
    while(vb != nullptr) {
        const size_t c = vertex_buffer_get_count(vb);
        const size_t m = vertex_buffer_get_max_count(vb);
        const uint32_t vbId = vertex_buffer_get_id(vb);

        *vbCount += 1;
        *count += c;
        *maxCount += m;
        *mem += c * DRAWBUFFER_VERTICES_BYTES;
        *maxMem += m * DRAWBUFFER_VERTICES_BYTES;

        vb = vertex_buffer_get_next(vb);
    }
}

void GameRenderer::debug_displayVBOccupancy(const Game_SharedPtr& g) {
    if (_debug_textLine_left != 0) {
        _debug_textLine_left++;
    }

    const char *prefix = "\x1b[14;|";
    Shape *mapShape = game_get_map_shape(g->getCGame());
    Scene *scene = game_get_scene(g->getCGame());
    uint16_t mapId = shape_get_id(mapShape);

    if (mapShape != nullptr) {
        uint32_t count = 0, maxCount = 0, mem = 0, maxMem = 0, vbCount = 0;
        debug_getShapeVBOccupancy(mapShape, &count, &maxCount, &mem, &maxMem, &vbCount, false);

        bgfx::dbgTextPrintf(1, ++_debug_textLine_left, 0x0f, "\x1b[14;MAP \x1b[11;opaque");
        bgfx::dbgTextPrintf(1, ++_debug_textLine_left, 0x0f, "%s\x1b[26;Buffers: \x1b[31;%d", prefix, vbCount);
        bgfx::dbgTextPrintf(1, ++_debug_textLine_left, 0x0f, "%s\x1b[26;Vertices: \x1b[31;%d/%d", prefix, count, maxCount);
        bgfx::dbgTextPrintf(1, ++_debug_textLine_left, 0x0f, "%s\x1b[26;Size: \x1b[31;%.1f/%.1fKo", prefix, static_cast<float>(mem) / 1024, static_cast<float>(maxMem) / 1024);
        bgfx::dbgTextPrintf(1, ++_debug_textLine_left, 0x0f, "%s\x1b[26;Occupancy: \x1b[31;%.2f%", prefix, 100.0f * static_cast<float>(count) / maxCount);

        count = maxCount = mem = maxMem = vbCount = 0;
        debug_getShapeVBOccupancy(mapShape, &count, &maxCount, &mem, &maxMem, &vbCount, true);

        if (count > 0) {
            bgfx::dbgTextPrintf(1, ++_debug_textLine_left, 0x0f, "\x1b[14;MAP \x1b[11;transparent");
            bgfx::dbgTextPrintf(1, ++_debug_textLine_left, 0x0f, "%s\x1b[26;Buffers: \x1b[31;%d", prefix, vbCount);
            bgfx::dbgTextPrintf(1, ++_debug_textLine_left, 0x0f, "%s\x1b[26;Vertices: \x1b[31;%d/%d", prefix, count, maxCount);
            bgfx::dbgTextPrintf(1, ++_debug_textLine_left, 0x0f, "%s\x1b[26;Size: \x1b[31;%.1f/%.1fKo", prefix, static_cast<float>(mem) / 1024, static_cast<float>(maxMem) / 1024);
            bgfx::dbgTextPrintf(1, ++_debug_textLine_left, 0x0f, "%s\x1b[26;Occupancy: \x1b[31;%.2f%", prefix, 100.0f * static_cast<float>(count) / maxCount);
        }
    }

    if (scene != nullptr) {
        Shape *s;
        DoublyLinkedList *itr, *subitr;
        DoublyLinkedListNode *n, *subn;

        uint32_t oCount = 0, oMaxCount = 0, oMem = 0, oMaxMem = 0, oVbCount = 0;
        uint32_t tCount = 0, tMaxCount = 0, tMem = 0, tMaxMem = 0, tVbCount = 0;

        itr = scene_new_shapes_iterator(scene);
        n = doubly_linked_list_first(itr);
        while (n != nullptr) {
            s = static_cast<Shape*>(doubly_linked_list_node_pointer(n));
            if (s != nullptr && mapId != shape_get_id(s)) {
                debug_getShapeVBOccupancy(s, &oCount, &oMaxCount, &oMem, &oMaxMem, &oVbCount, false);
                debug_getShapeVBOccupancy(s, &tCount, &tMaxCount, &tMem, &tMaxMem, &tVbCount, true);
            }
            n = doubly_linked_list_node_next(n);
        }
        doubly_linked_list_free(itr);

        bgfx::dbgTextPrintf(1, ++_debug_textLine_left, 0x0f, "\x1b[14;SCENE \x1b[11;opaque");
        bgfx::dbgTextPrintf(1, ++_debug_textLine_left, 0x0f, "%s\x1b[26;Buffers: \x1b[31;%d", prefix, oVbCount);
        bgfx::dbgTextPrintf(1, ++_debug_textLine_left, 0x0f, "%s\x1b[26;Vertices: \x1b[31;%d/%d", prefix, oCount, oMaxCount);
        bgfx::dbgTextPrintf(1, ++_debug_textLine_left, 0x0f, "%s\x1b[26;Size: \x1b[31;%.1f/%.1fKo", prefix, static_cast<float>(oMem) / 1024, static_cast<float>(oMaxMem) / 1024);
        bgfx::dbgTextPrintf(1, ++_debug_textLine_left, 0x0f, "%s\x1b[26;Occupancy: \x1b[31;%.2f%", prefix, 100.0f * static_cast<float>(oCount) / oMaxCount);

        if (tCount > 0) {
            bgfx::dbgTextPrintf(1, ++_debug_textLine_left, 0x0f, "\x1b[14;SCENE \x1b[11;transparent");
            bgfx::dbgTextPrintf(1, ++_debug_textLine_left, 0x0f, "%s\x1b[26;Buffers: \x1b[31;%d", prefix, tVbCount);
            bgfx::dbgTextPrintf(1, ++_debug_textLine_left, 0x0f, "%s\x1b[26;Vertices: \x1b[31;%d/%d", prefix, tCount, tMaxCount);
            bgfx::dbgTextPrintf(1, ++_debug_textLine_left, 0x0f, "%s\x1b[26;Size: \x1b[31;%.1f/%.1fKo", prefix, static_cast<float>(tMem) / 1024, static_cast<float>(tMaxMem) / 1024);
            bgfx::dbgTextPrintf(1, ++_debug_textLine_left, 0x0f, "%s\x1b[26;Occupancy: \x1b[31;%.2f%", prefix, 100.0f * static_cast<float>(tCount) / tMaxCount);
        }
    }
}

static int debug_siblingIdx;
void debug_displaySceneHierarchy_recurse(Transform *t, int depthIdx, const float tw, const float th) {
    DoublyLinkedListNode *n = transform_get_children_iterator(t);
    Transform *child = nullptr;
    while (n != nullptr) {
        child = static_cast<Transform *>(doubly_linked_list_node_pointer(n));

        switch(transform_get_type(child)) {
            case HierarchyTransform:
                bgfx::dbgTextPrintf(depthIdx, th - debug_siblingIdx - 2, 0x0f, "\x1b[55;|-H");
                break;
            case PointTransform:
                bgfx::dbgTextPrintf(depthIdx, th - debug_siblingIdx - 2, 0x0f, "\x1b[55;|-O");
                break;
            case ShapeTransform:
                bgfx::dbgTextPrintf(depthIdx, th - debug_siblingIdx - 2, 0x0f, "\x1b[55;|-S");
                break;
            case QuadTransform:
                bgfx::dbgTextPrintf(depthIdx, th - debug_siblingIdx - 2, 0x0f, "\x1b[55;|-Q");
                break;
            case CameraTransform:
                bgfx::dbgTextPrintf(depthIdx, th - debug_siblingIdx - 2, 0x0f, "\x1b[55;|-C");
                break;
            case LightTransform:
                bgfx::dbgTextPrintf(depthIdx, th - debug_siblingIdx - 2, 0x0f, "\x1b[55;|-L");
                break;
            case WorldTextTransform:
                bgfx::dbgTextPrintf(depthIdx, th - debug_siblingIdx - 2, 0x0f, "\x1b[55;|-T");
                break;
            case AudioListenerTransform:
                bgfx::dbgTextPrintf(depthIdx, th - debug_siblingIdx - 2, 0x0f, "\x1b[55;|-B");
                break;
            case AudioSourceTransform:
                bgfx::dbgTextPrintf(depthIdx, th - debug_siblingIdx - 2, 0x0f, "\x1b[55;|-A");
                break;
            default:
                break;
        }
        
        if (transform_get_children_count(child) == 0) {
            debug_siblingIdx++;
        }
        debug_displaySceneHierarchy_recurse(child, depthIdx + 3, tw, th);

        n = doubly_linked_list_node_next(n);
    }
}

void GameRenderer::debug_displaySceneHierarchy(const Game_SharedPtr& g) {
    const bgfx::Stats* stats = bgfx::getStats();
    const float tw = stats->textWidth;
    const float th = stats->textHeight;

    Transform *t = scene_get_root(game_get_scene(g->getCGame()));
    bgfx::dbgTextPrintf(1, th - 4, 0x0f, "\x1b[55;R");
    bgfx::dbgTextPrintf(1, th - 3, 0x0f, "\x1b[55;Hierarchy");
    debug_siblingIdx = 0;
    debug_displaySceneHierarchy_recurse(t, 2, tw, th - 2);

#if DEBUG_TRANSFORM_REFRESH_CALLS
    bgfx::dbgTextPrintf(tw - 4, th - 4, 0x0f, "\x1b[75;%3d", debug_transform_get_refresh_calls());
    bgfx::dbgTextPrintf(tw - 14, th - 3, 0x0f, "\x1b[75;Refresh calls");
    debug_transform_reset_refresh_calls();
#endif
}

static int debug_rtreeNodes;
static int debug_rtreeSiblingIdx;
void GameRenderer::debug_displayRtree_recurse(Rtree *r, RtreeNode *rn, uint8_t level, const float tw, const float th,
                                              const bool boxes, const Matrix4x4 *mtx, const int onlyLevel) {

    DoublyLinkedListNode *n = rtree_node_get_children_iterator(rn);
    RtreeNode *child = nullptr;
    while (n != nullptr) {
        child = static_cast<RtreeNode *>(doubly_linked_list_node_pointer(n));

        if (tw > 0 && th > 0) {
            if (rtree_node_get_leaf_ptr(child) == NULL) {
                bgfx::dbgTextPrintf(level * 3 + 2, th - debug_rtreeSiblingIdx - 2, 0x0f, "\x1b[55;|-B");
            } else {
                bgfx::dbgTextPrintf(level * 3 + 2, th - debug_rtreeSiblingIdx - 2, 0x0f, "\x1b[55;|-L");
            }
            if (rtree_node_get_children_count(child) == 0) {
                debug_rtreeSiblingIdx++;
            }
        }

        debug_displayRtree_recurse(r, child, level + 1, tw, th, boxes, mtx, onlyLevel);
        
        n = doubly_linked_list_node_next(n);
    }
    
    debug_rtreeNodes++;

    if (boxes && (onlyLevel == -1 || onlyLevel == level)) {
        const Box *aabb = rtree_node_get_aabb(rn);
        if (aabb != NULL) {
            const uint32_t color = lerpABGR32(0x0000ffff, 0xff0000ff, static_cast<float>(level) / rtree_get_height(r));
            if (mtx == nullptr) {
                drawBox(VIEW_OPAQUE(VIEW_SKYBOX), aabb, &float3_zero, color);
            } else {
                Matrix4x4 tmp, cube = *mtx;

                matrix4x4_set_translation(&tmp, aabb->min.x, aabb->min.y, aabb->min.z);
                matrix4x4_op_multiply(&cube, &tmp);

                matrix4x4_set_scaleXYZ(&tmp,
                                       aabb->max.x - aabb->min.x,
                                       aabb->max.y - aabb->min.y,
                                       aabb->max.z - aabb->min.z);
                matrix4x4_op_multiply(&cube, &tmp);

                drawCube(VIEW_OPAQUE(VIEW_SKYBOX), reinterpret_cast<const float *>(&cube), color);
            }
        }
    }
}

void GameRenderer::debug_displayRtree(const Game_SharedPtr& g) {
    if (_debug_textLine_right != 0) {
        _debug_textLine_right++;
    }
    
    const bgfx::Stats* stats = bgfx::getStats();
    const float tw = stats->textWidth;
    const float th = stats->textHeight;
    
    Rtree *r = scene_get_rtree(game_get_scene(g->getCGame()));
#if DEBUG_RENDERER_DISPLAY_RTREE
    bgfx::dbgTextPrintf(1, th - 4, 0x0f, "\x1b[55;R");
    bgfx::dbgTextPrintf(1, th - 3, 0x0f, "\x1b[55;R-tree");
    debug_rtreeSiblingIdx = 0;
#endif
    debug_rtreeNodes = 0;
#if DEBUG_RENDERER_DISPLAY_RTREE
    debug_displayRtree_recurse(r, rtree_get_root(r), 0, tw, th - 2, DEBUG_RENDERER_DISPLAY_RTREE_BOXES);
#else
    debug_displayRtree_recurse(r, rtree_get_root(r), 0, 0, 0, DEBUG_RENDERER_DISPLAY_RTREE_BOXES);
#endif
#if DEBUG_RENDERER_DISPLAY_RTREE_STATS && DEBUG_RTREE_CALLS
    bgfx::dbgTextPrintf(tw - 14, ++_debug_textLine_right, 0x0f, "\x1b[95;R-tree stats");
    bgfx::dbgTextPrintf(tw - 12, ++_debug_textLine_right, 0x0f, "\x1b[95;insert %3d", debug_rtree_get_insert_calls());
    bgfx::dbgTextPrintf(tw - 11, ++_debug_textLine_right, 0x0f, "\x1b[95;split %3d", debug_rtree_get_split_calls());
    bgfx::dbgTextPrintf(tw - 12, ++_debug_textLine_right, 0x0f, "\x1b[95;delete %3d", debug_rtree_get_remove_calls());
    bgfx::dbgTextPrintf(tw - 14, ++_debug_textLine_right, 0x0f, "\x1b[95;condense %3d", debug_rtree_get_condense_calls());
    bgfx::dbgTextPrintf(tw - 12, ++_debug_textLine_right, 0x0f, "\x1b[95;update %3d", debug_rtree_get_update_calls());
    bgfx::dbgTextPrintf(tw - 11, ++_debug_textLine_right, 0x0f, "\x1b[95;nodes %3d", debug_rtreeNodes);
    bgfx::dbgTextPrintf(tw - 12, ++_debug_textLine_right, 0x0f, "\x1b[95;height %3d", rtree_get_height(r));
    debug_rtree_reset_calls();
#endif
}

void GameRenderer::debug_displayRigidbodyStats(const Game_SharedPtr& g) {
#if DEBUG_RIGIDBODY
    if (_debug_textLine_right != 0) {
        _debug_textLine_right++;
    }
    
    const bgfx::Stats *stats = bgfx::getStats();
    const float tw = stats->textWidth;
    const float th = stats->textHeight;
    
    bgfx::dbgTextPrintf(tw - 17, ++_debug_textLine_right, 0x0f, "\x1b[95;Rigidbody stats");
    bgfx::dbgTextPrintf(tw - 12, ++_debug_textLine_right, 0x0f, "\x1b[95;solver %3d", debug_rigidbody_get_solver_iterations());
    bgfx::dbgTextPrintf(tw - 17, ++_debug_textLine_right, 0x0f, "\x1b[95;replacement %3d", debug_rigidbody_get_replacements());
    bgfx::dbgTextPrintf(tw - 15, ++_debug_textLine_right, 0x0f, "\x1b[95;collision %3d", debug_rigidbody_get_collisions());
    bgfx::dbgTextPrintf(tw - 11, ++_debug_textLine_right, 0x0f, "\x1b[95;sleep %3d", debug_rigidbody_get_sleeps());
    bgfx::dbgTextPrintf(tw - 11, ++_debug_textLine_right, 0x0f, "\x1b[95;awake %3d", debug_rigidbody_get_awakes());
    bgfx::dbgTextPrintf(tw - 16, ++_debug_textLine_right, 0x0f, "\x1b[95;aw.queries %3d", debug_scene_get_awake_queries());
    debug_rigidbody_reset_calls();
    debug_scene_reset_calls();
#endif
}

void GameRenderer::debug_displaySubRenderersStats() {
    if (_debug_textLine_right != 0) {
        _debug_textLine_right++;
    }
    
    const bgfx::Stats *stats = bgfx::getStats();
    const float tw = stats->textWidth;
    const float th = stats->textHeight;
    
#if DEBUG_RENDERER_DECALS_STATS
    bgfx::dbgTextPrintf(tw - 8, ++_debug_textLine_right, 0x0f, "\x1b[95;Decals");
    bgfx::dbgTextPrintf(tw - 14, ++_debug_textLine_right, 0x0f, "\x1b[95;requests %3d", _decals->_debug_getRequests());
    bgfx::dbgTextPrintf(tw - 17, ++_debug_textLine_right, 0x0f, "\x1b[95;projections %3d", _decals->_debug_getProjections());
    _decals->_debug_reset();
#endif
#if DEBUG_RENDERER_WORLDTEXT_STATS
    _debug_textLine_right++;
    bgfx::dbgTextPrintf(tw - 13, ++_debug_textLine_right, 0x0f, "\x1b[95;World Texts");
    bgfx::dbgTextPrintf(tw - 12, ++_debug_textLine_right, 0x0f, "\x1b[95;screen %3d", _worldText->_debug_getScreenTexts());
    bgfx::dbgTextPrintf(tw - 11, ++_debug_textLine_right, 0x0f, "\x1b[95;world %3d", _worldText->_debug_getWorldTexts());
    bgfx::dbgTextPrintf(tw - 12, ++_debug_textLine_right, 0x0f, "\x1b[95;culled %3d", _worldText->_debug_getCulled());
    bgfx::dbgTextPrintf(tw - 15, ++_debug_textLine_right, 0x0f, "\x1b[95;refreshes %3d", _worldText->_debug_getTextRefreshes());
    _worldText->_debug_reset();
#endif
}

#endif /* DEBUG_RENDERER */

void GameRenderer::debug_clearText() {
    bgfx::dbgTextClear();
    _debug_textLine_left = 0;
    _debug_textLine_right = 0;
}

typedef struct {
    uint32_t triggers, triggerPBs, statics, staticPBs, dynamics;
} CollidersStats;

bool debug_lua_displayCollidersStats_func(Transform *t, void *ptr) {
    CollidersStats *stats = static_cast<CollidersStats*>(ptr);

    const uint8_t mode = rigidbody_get_simulation_mode(transform_get_rigidbody(t));
    switch(mode) {
        case RigidbodyMode_Trigger:
            stats->triggers++;
            break;
        case RigidbodyMode_TriggerPerBlock:
            stats->triggerPBs++;
            break;
        case RigidbodyMode_Static:
            stats->statics++;
            break;
        case RigidbodyMode_StaticPerBlock:
            stats->staticPBs++;
            break;
        case RigidbodyMode_Dynamic:
            stats->dynamics++;
            break;
        default:
            break;
    }
    return false;
}

void GameRenderer::debug_lua_displayFPS() {
    const bgfx::Stats* stats = bgfx::getStats();
    if (_debug_textLine_left != 0) {
        _debug_textLine_left++;
    }

    const int current = static_cast<int>(static_cast<float>(stats->cpuTimerFreq) / stats->cpuTimeFrame);
#if DEBUG_RENDERER
    _debug_fps_ma[_debug_fps_ma_cursor] = current;
    _debug_fps_ma_cursor = (_debug_fps_ma_cursor + 1) % DEBUG_RENDERER_FPS_MOVING_AVERAGE;

    int average = _debug_fps_ma[0], min = _debug_fps_ma[0], max = _debug_fps_ma[0];
    for (int i=1; i < DEBUG_RENDERER_FPS_MOVING_AVERAGE; ++i) {
        min = minimum(min, _debug_fps_ma[i]);
        max = maximum(max, _debug_fps_ma[i]);
        average += _debug_fps_ma[i];
    }
    average /= DEBUG_RENDERER_FPS_MOVING_AVERAGE;

    bgfx::dbgTextPrintf(1, ++_debug_textLine_left, 0x0f, "\x1b[23;FPS: \x1b[26;%d ~%d [%d:%d]", current, average, min, max);
#else
    bgfx::dbgTextPrintf(1, ++_debug_textLine_left, 0x0f, "\x1b[23;FPS: \x1b[26;%d", current);
#endif
}

void GameRenderer::debug_lua_displayCollidersStats() {
    CollidersStats stats = { 0, 0, 0, 0, 0 };

    transform_recurse(scene_get_root(game_get_scene(_game->getCGame())),
                      debug_lua_displayCollidersStats_func,
                      &stats, false, true);

    if (_debug_textLine_left != 0) {
        _debug_textLine_left++;
    }
    bgfx::dbgTextPrintf(1, ++_debug_textLine_left, 0x0f, "\x1b[23;Colliders");
    bgfx::dbgTextPrintf(1, ++_debug_textLine_left, 0x0f, "\x1b[26;trigger: %d", stats.triggers);
    bgfx::dbgTextPrintf(1, ++_debug_textLine_left, 0x0f, "\x1b[26;trigger-per-block: %d", stats.triggerPBs);
    bgfx::dbgTextPrintf(1, ++_debug_textLine_left, 0x0f, "\x1b[25;static: %d", stats.statics);
    bgfx::dbgTextPrintf(1, ++_debug_textLine_left, 0x0f, "\x1b[25;static-per-block: %d", stats.staticPBs);
    bgfx::dbgTextPrintf(1, ++_debug_textLine_left, 0x0f, "\x1b[28;dynamic: %d", stats.dynamics);
}

#else /* P3S_CLIENT_HEADLESS */

GameRenderer *GameRenderer::newGameRenderer(QualityTier tier,
                                            uint16_t width,
                                            uint16_t height,
                                            float devicePPP) {
    return new GameRenderer(tier, width, height, devicePPP);
}
GameRenderer *GameRenderer::getGameRenderer() { return nullptr; }
GameRenderer::GameRenderer(QualityTier tier, uint16_t width, uint16_t height, float devicePPP) {}
GameRenderer::~GameRenderer() {}
void GameRenderer::memoryRefRelease(void* ptr, void* data) {}
void GameRenderer::setTemporaryCameraProj(Game *g) {}
void GameRenderer::screenshot(void** out,
                              uint16_t *outWidth,
                              uint16_t *outHeight,
                              size_t *outSize,
                              const std::string& filename,
                              bool outPng,
                              bool noBg,
                              uint16_t maxWidth,
                              uint16_t maxHeight,
                              bool unflip,
                              bool trim,
                              EnforceRatioStrategy ratioStrategy,
                              float outputRatio) {}
void GameRenderer::getOrComputeWorldTextSize(WorldText *wt, float *outWidth, float *outHeight, bool points) {}
float GameRenderer::getCharacterSize(FontName font, const char *text, bool points) { return 0.0f; }
size_t GameRenderer::snapWorldTextCursor(WorldText *wt, float *inoutX, float *inoutY, size_t *inCharIndex, bool points) { return 0; }
bool GameRenderer::isFallbackEnabled() { return false; }
QualityTier GameRenderer::getQualityTier() { return QualityTier_Compatibility; }
float GameRenderer::getRenderWidth() { return 800.0f; }
float GameRenderer::getRenderHeight() { return 600.0f; }
bool GameRenderer::reload(QualityTier tier) { return false; }
bool GameRenderer::reload(QualityParameters params) { return false; }
void GameRenderer::togglePBR(bool toggle) {}
bool GameRenderer::isPBR() { return false; }
void GameRenderer::debug_checkAndDrawShapeRtree(const Transform *t, const Shape *s, const int onlyLevel) {}

#endif /* P3S_CLIENT_HEADLESS */

#ifndef P3S_CLIENT_HEADLESS

// MARK: - RendererFeatures -

bool RendererFeatures::isTexture3DEnabled() const {
    return (mask & RENDERER_FEATURE_TEX3D) != 0;
}

bool RendererFeatures::isMRTEnabled() const {
    return (mask & RENDERER_FEATURE_MRT) != 0;
}

bool RendererFeatures::isInstancingEnabled() const {
    return (mask & RENDERER_FEATURE_INSTANCING) != 0;
}

bool RendererFeatures::isComputeEnabled() const {
    return (mask & RENDERER_FEATURE_COMPUTE) != 0;
}

bool RendererFeatures::isDrawIndirectEnabled() const {
    return (mask & RENDERER_FEATURE_INDIRECT) != 0;
}

bool RendererFeatures::isTextureReadBackEnabled() const {
    return (mask & RENDERER_FEATURE_READBACK) != 0;
}

bool RendererFeatures::isTextureBlitEnabled() const {
    return (mask & RENDERER_FEATURE_TEXBLIT) != 0;
}

bool RendererFeatures::isShadowSamplerEnabled() const {
    return (mask & RENDERER_FEATURE_SHADOWSAMPLER) != 0;
}

bool RendererFeatures::useDeferred() const {
    return isMRTEnabled() && isTextureBlitEnabled();
}

#endif /* P3S_CLIENT_HEADLESS */

// MARK: - QualityParameters -

void QualityParameters::fromPreset(QualityTier tier) {
#if defined(__VX_PLATFORM_WASM)
    switch(tier) {
        case QualityTier_Compatibility: {
            shadowsTier = 0;
            renderScale = 0.8f;
            aa = AntiAliasing_None;
            postTier = 0;
            break;
        }
        case QualityTier_Low: {
            shadowsTier = 1;
            renderScale = 1.0f;
            aa = AntiAliasing_None;
            postTier = 0;
            break;
        }
        case QualityTier_Medium: {
            shadowsTier = 2;
            renderScale = 1.0f;
            aa = AntiAliasing_FXAA;
            postTier = 1;
            break;
        }
        case QualityTier_High: {
            shadowsTier = 3;
            renderScale = 1.2f;
            aa = AntiAliasing_FXAA;
            postTier = 1;
            break;
        }
        case QualityTier_Ultra: {
            shadowsTier = 4;
            renderScale = 1.4f;
            aa = AntiAliasing_FXAA;
            postTier = 2;
            break;
        }
        default:
            break;
    }
#elif defined(__VX_PLATFORM_ANDROID) || defined(__VX_PLATFORM_IOS)
    switch(tier) {
        case QualityTier_Compatibility: {
            shadowsTier = 0;
            renderScale = 0.8f;
            aa = AntiAliasing_None;
            postTier = 0;
            break;
        }
        case QualityTier_Low: {
            shadowsTier = 1;
            renderScale = 1.0f;
            aa = AntiAliasing_None;
            postTier = 0;
            break;
        }
        case QualityTier_Medium: {
            shadowsTier = 2;
            renderScale = 1.2f;
            aa = AntiAliasing_FXAA;
            postTier = 1;
            break;
        }
        case QualityTier_High: {
            shadowsTier = 3;
            renderScale = 1.4f;
            aa = AntiAliasing_FXAA;
            postTier = 1;
            break;
        }
        case QualityTier_Ultra: {
            shadowsTier = 4;
            renderScale = 1.6f;
            aa = AntiAliasing_FXAA;
            postTier = 2;
            break;
        }
        default:
            break;
    }
#else
    switch(tier) {
        case QualityTier_Compatibility: {
            shadowsTier = 0;
            renderScale = 1.0f;
            aa = AntiAliasing_None;
            postTier = 0;
            break;
        }
        case QualityTier_Low: {
            shadowsTier = 1;
            renderScale = 1.0f;
            aa = AntiAliasing_None;
            postTier = 0;
            break;
        }
        case QualityTier_Medium: {
            shadowsTier = 2;
            renderScale = 1.2f;
            aa = AntiAliasing_FXAA;
            postTier = 1;
            break;
        }
        case QualityTier_High: {
            shadowsTier = 3;
            renderScale = 1.4f;
            aa = AntiAliasing_FXAA;
            postTier = 2;
            break;
        }
        case QualityTier_Ultra: {
            shadowsTier = 4;
            renderScale = 1.6f;
            aa = AntiAliasing_FXAA;
            postTier = 3;
            break;
        }
        default:
            break;
    }
#endif
}

// MARK: - GameRendererState -

GameRendererState::GameRendererState() {
    // Fog default settings
    this->fog = FOG_ENABLED;
    this->fogStart = FOG_DEFAULT_START_DISTANCE;
    this->fogEnd = FOG_DEFAULT_END_DISTANCE;
    this->fogEmission = FOG_EMISSION;
    this->fogLayers = CAMERA_LAYERS_DEFAULT;
    this->fogColor = FOG_COLOR;

#ifndef P3S_CLIENT_HEADLESS
    // Sky default settings
    this->sky.clouds = SKY_CLOUDS_ENABLED;
    this->sky.unlit = SKY_CLOUDS_UNLIT;
    this->sky.origin.x = 0.0f;
    this->sky.origin.z = 0.0f;
    this->sky.width = SKY_CLOUDS_WIDTH;
    this->sky.depth = SKY_CLOUDS_DEPTH;
    this->sky.origin.y = SKY_CLOUDS_ALTITUDE;
    this->sky.spread = SKY_CLOUDS_SPREAD;
    this->sky.baseScale = SKY_CLOUDS_BASE_SCALE;
    this->sky.maxAddScale = SKY_CLOUDS_MAX_ADD_SCALE;
    this->sky.cutout = SKY_CLOUDS_CUTOUT;
    this->sky.frequencyX = SKY_CLOUDS_FREQUENCY_X;
    this->sky.frequencyZ = SKY_CLOUDS_FREQUENCY_Z;
    this->sky.magnitude = SKY_CLOUDS_MAGNITUDE;
    this->sky.period = SKY_CLOUDS_PERIOD;
    this->sky.speed = SKY_CLOUDS_SPEED;
    this->sky.innerEdge = SKY_CLOUDS_INNER_EDGE;
    this->sky.outerEdge = SKY_CLOUDS_OUTER_EDGE;
    this->sky.color = SKY_CLOUDS_BASE_COLOR;
#endif

    // Lighting default settings
    this->lightsIntensity = LIGHT_DEFAULT_INTENSITY;
    this->bakedIntensity = BAKED_DEFAULT_INTENSITY;
    this->lightsAmbientFactor = LIGHT_DEFAULT_AMBIENT;
    this->skyboxAmbientFactor = SKYBOX_DEFAULT_AMBIENT;
    this->ambientColorOverride = { -1.0f, -1.0f, -1.0f };
    for (int i = 0; i < 2; ++i) {
        this->skyboxTex[i] = nullptr;
    }
    this->skyboxLerp = 0.0f;
    this->skyboxMode = SkyboxMode_Linear;

    // Skybox default settings
    this->skyColor = SKYBOX_SKY_COLOR;
    this->horizonColor = SKYBOX_HORIZON_COLOR;
    this->abyssColor = SKYBOX_ABYSS_COLOR;
    this->skyLightColor = SKYBOX_SKYLIGHT_COLOR;
    this->clearColor = utils_float_to_rgba(1.0f, this->skyColor.z, this->skyColor.y, this->skyColor.x);

    // Debug default settings
    this->debug_displayBoxes = DEBUG_RENDERER_DISPLAY_BOXES;
    this->debug_displayColliders = DEBUG_RENDERER_DISPLAY_COLLIDERS;
    this->debug_displayFPS = DEBUG_RENDERER_DISPLAY_FPS;
    
    // By default, reset everything for a new game
    this->_dirty = RENDERERSTATE_ALL;
}

GameRendererState::~GameRendererState() {
    for (int i = 0; i < 2; ++i) {
        if (this->skyboxTex[i] != nullptr) {
            texture_release(this->skyboxTex[i]);
        }
    }
}

#ifndef P3S_CLIENT_HEADLESS
void GameRendererState::setDefaultsFromMap(const Shape *map) {
    int3 mapSize; shape_get_bounding_box_size(map, &mapSize);
    const float3 *mapScale = shape_get_local_scale(map);
    const float scale = float3_mmin(mapScale);

    this->sky.spread = scale; // 1 clouds block per map block
    this->sky.origin.x = 0.0f;
    this->sky.origin.z = 0.0f;
    this->sky.width = mapSize.x * scale;
    this->sky.depth = mapSize.z * scale;
    this->sky.baseScale = SKY_CLOUDS_BASE_SCALE * scale;
    this->sky.maxAddScale = SKY_CLOUDS_MAX_ADD_SCALE * scale;
}

void GameRendererState::setSkyboxTextures(Texture *tex[2]) {
    for (int i = 0; i < SKYBOX_MAX_CUBEMAPS; ++i) {
        if (this->skyboxTex[i] != nullptr) {
            texture_release(this->skyboxTex[i]);
        }
        this->skyboxTex[i] = tex != nullptr ? tex[i] : nullptr;
    }
    setDirty(RENDERERSTATE_SKYBOX);
}

void GameRendererState::setSkyboxLerp(float t) {
    this->skyboxLerp = t;
}

void GameRendererState::setSkyColor(float3 rgb) {
    this->skyColor = rgb;
    setDirty(RENDERERSTATE_SKYBOX);
}

void GameRendererState::setHorizonColor(float3 rgb) {
    this->horizonColor = rgb;
    setDirty(RENDERERSTATE_SKYBOX);
}

void GameRendererState::setAbyssColor(float3 rgb) {
    this->abyssColor = rgb;
    setDirty(RENDERERSTATE_SKYBOX);
}

bool GameRendererState::fogToggled() {
    return this->fog;
}

void GameRendererState::toggleFog(bool value) {
    this->fog = value;
    setDirty(RENDERERSTATE_FOG);
}

float GameRendererState::getFogStart() {
    return this->fogStart;
}

void GameRendererState::setFogStart(float value) {
    this->fogStart = value;
    setDirty(RENDERERSTATE_FOG);
}

float GameRendererState::getFogEnd() {
    return this->fogEnd;
}

void GameRendererState::setFogEnd(float value) {
    this->fogEnd = value;
    setDirty(RENDERERSTATE_FOG);
}

float GameRendererState::getFogEmission() {
    return this->fogEmission;
}

void GameRendererState::setFogEmission(float v) {
    this->fogEmission = v;
    setDirty(RENDERERSTATE_FOG);
}

uint16_t GameRendererState::getFogLayers() {
    return this->fogLayers;
}

void GameRendererState::setFogLayers(uint16_t value) {
    this->fogLayers = value;
}

bool GameRendererState::cloudsToggled() {
    return this->sky.clouds;
}

void GameRendererState::toggleClouds(bool value) {
    this->sky.clouds = value;
}

bool GameRendererState::getCloudsUnlit() {
    return this->sky.unlit;
}

void GameRendererState::setCloudsUnlit(bool value) {
    this->sky.unlit = value;
}

float3 GameRendererState::getCloudsColor() {
    return this->sky.color;
}

void GameRendererState::setCloudsColor(float3 rgb) {
    this->sky.color = rgb;
}

float3 GameRendererState::getCloudsOrigin() {
    return this->sky.origin;
}

void GameRendererState::setCloudsOrigin(float3 value) {
    this->sky.origin = value;
}

float GameRendererState::getCloudsWidth() {
    return this->sky.width;
}

void GameRendererState::setCloudsWidth(float value) {
    this->sky.width = abs(value);
    setDirty(RENDERERSTATE_CLOUDS);
}

float GameRendererState::getCloudsDepth() {
    return this->sky.depth;
}

void GameRendererState::setCloudsDepth(float value) {
    this->sky.depth = abs(value);
    setDirty(RENDERERSTATE_CLOUDS);
}

float GameRendererState::getCloudsBaseScale() {
    return this->sky.baseScale;
}

void GameRendererState::setCloudsBaseScale(float value) {
    this->sky.baseScale = fabsf(value);
    setDirty(RENDERERSTATE_CLOUDS);
}

float GameRendererState::getCloudsAddScale() {
    return this->sky.maxAddScale;
}

void GameRendererState::setCloudsAddScale(float value) {
    this->sky.maxAddScale = fabsf(value);
    setDirty(RENDERERSTATE_CLOUDS);
}

float GameRendererState::getCloudsSpread() {
    return this->sky.spread;
}

void GameRendererState::setCloudsSpread(float value) {
    this->sky.spread = maximum(1.0f, value);
    setDirty(RENDERERSTATE_CLOUDS);
}

float GameRendererState::getCloudsSpeed() {
    return this->sky.speed;
}

void GameRendererState::setCloudsSpeed(float value) {
    this->sky.speed = value;
}

void GameRendererState::setDirty(uint8_t flag) {
    _dirty |= flag;
}

void GameRendererState::resetDirty(uint8_t flag) {
    _dirty &= ~flag;
}

bool GameRendererState::isDirty(uint8_t flag) const {
    return flag == (_dirty & flag);
}

bool GameRendererState::isDirty() const {
    return _dirty != RENDERERSTATE_NONE;
}

void GameRendererState::setAllDirty() {
    _dirty = RENDERERSTATE_ALL;
}

#else // headless client

void GameRendererState::setDefaultsFromMap(const Shape *map) {}
bool GameRendererState::fogToggled() { return false; }
void GameRendererState::toggleFog(bool value) {}
float GameRendererState::getFogStart() { return 0.0f; }
void GameRendererState::setFogStart(float value) {}
float GameRendererState::getFogEnd() { return 0.0f; }
void GameRendererState::setFogEnd(float value) {}
float GameRendererState::getFogEmission() { return 0.0f; }
void GameRendererState::setFogEmission(float v) {}
uint16_t GameRendererState::getFogLayers() { return 0; }
void GameRendererState::setFogLayers(uint16_t value) {}
bool GameRendererState::cloudsToggled() { return false; }
void GameRendererState::toggleClouds(bool value) {}
bool GameRendererState::getCloudsUnlit() { return false; }
void GameRendererState::setCloudsUnlit(bool value) {}
float3 GameRendererState::getCloudsColor() { return float3_zero; }
void GameRendererState::setCloudsColor(float3 rgb) {}
float3 GameRendererState::getCloudsOrigin() { return float3_zero; }
void GameRendererState::setCloudsOrigin(float3 value) {}
float GameRendererState::getCloudsWidth() { return 0.0f; }
void GameRendererState::setCloudsWidth(float value) {}
float GameRendererState::getCloudsDepth() { return 0.0f; }
void GameRendererState::setCloudsDepth(float value) {}
float GameRendererState::getCloudsBaseScale() { return 0.0f; }
void GameRendererState::setCloudsBaseScale(float value) {}
float GameRendererState::getCloudsAddScale() { return 0.0f; }
void GameRendererState::setCloudsAddScale(float value) {}
float GameRendererState::getCloudsSpread() { return 0.0f; }
void GameRendererState::setCloudsSpread(float value) {}
float GameRendererState::getCloudsSpeed() { return 0.0f; }
void GameRendererState::setCloudsSpeed(float value) {}
void GameRendererState::setDirty(uint8_t flag) {}
void GameRendererState::resetDirty(uint8_t flag) {}
bool GameRendererState::isDirty(uint8_t flag) const { return false; }
bool GameRendererState::isDirty() const { return false; }

#endif
