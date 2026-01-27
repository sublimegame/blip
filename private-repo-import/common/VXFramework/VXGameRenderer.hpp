// -------------------------------------------------------------
//  Cubzh
//  VXGameRenderer.hpp
//  Created by Arthur Cormerais on March 20, 2020.
// -------------------------------------------------------------

#pragma once

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wpadded"

#include <unordered_map>
#include <string>
#include <mutex>

// vx
#include "VXRDConfig.hpp"
#include "VXNode.hpp"
#include "VXRDDecals.hpp"
#include "VXRDWorldText.hpp"
#include "VXRDSky.hpp"
#include "VXRDUtils.hpp"
#include "VXRDGif.hpp"
#include "VXGame.hpp"
#include "camera.h"

// bgfx
#ifndef P3S_CLIENT_HEADLESS

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Weverything"
#include "bgfx/bgfx.h"
#include "bx/readerwriter.h"
#include "bx/rng.h"
#include "bimg/decode.h"
#pragma GCC diagnostic pop

#endif

// engine
#include "float3.h"
#include "game.h"
#include "scene.h"
#include "shape.h"
#include "light.h"
#include "doubly_linked_list.h"
#include "quad.h"
#include "mesh.h"

using namespace vx::rendering;

// RENDERER CONFIG
// Vertex Buffer contains vertices index, 4 per face
#define VB_VERTICES 4
// Index Buffer contains triangulation indices, 2 triangles ie. 6 indices per face
#define IB_INDICES 6
#define VB_BYTES VB_VERTICES * sizeof(float)
#define IB_BYTES IB_INDICES * sizeof(uint32_t)
#define TEX3D_MAXSIZE 256
#define TEX3D_UPDATE_MAX_BYTES 32 * 32 * 32 * 4
#define STAGE_SHAPE_PALETTE    0
#define STAGE_QUAD_TEX    0
#define STAGE_MESH_ALBEDO       0
#define STAGE_MESH_NORMAL       1
#define STAGE_MESH_METALLIC     2
#define STAGE_MESH_EMISSIVE     3
#define STAGE_BLIT_OPAQUE         0
#define STAGE_BLIT_ILLUMINATION   1
#define STAGE_BLIT_EMISSION       2
#define STAGE_BLIT_DEPTH          3
#define STAGE_BLIT_TRANSPARENCY_1 4
#define STAGE_BLIT_TRANSPARENCY_2 5
#define STAGE_LIGHT_NORMAL        0
#define STAGE_LIGHT_DEPTH         1
#define STAGE_LIGHT_SHADOWMAP1    2 // expands based on shadowmap cascades
#define STAGE_LIGHT_PBR_OPAQUE    2
#define STAGE_LIGHT_PBR_PARAMS    3
#define STAGE_LIGHT_PBR_SHADOWMAP1 4 // expands based on shadowmap cascades
#define STAGE_TARGET    0
#define MRT_MAX         5
#define RESERVED_TEXTURES 512 // keep a flat amount of resources for internal use
#define RESERVED_VERTEX_BUFFERS 128
#define RESERVED_INDEX_BUFFERS 128
#define QUAD_MASK_ID_MAX 31 // encoded on stencil higher 5 bits
#define QUAD_MASK_DEPTH_MAX 7 // encoded on stencil lower 3 bits
#define QUAD_BATCH_TEXTURED_INIT 8
#define QUAD_BATCH_TEXTURED_FACTOR 4
// Mapping bgfx capabilities to useful features
#define RENDERER_FEATURE_NONE uint16_t(0)
#define RENDERER_FEATURE_TEX3D uint16_t(1)
#define RENDERER_FEATURE_MRT uint16_t(2)
#define RENDERER_FEATURE_INSTANCING uint16_t(4)
#define RENDERER_FEATURE_COMPUTE uint16_t(8)
#define RENDERER_FEATURE_INDIRECT uint16_t(16)
#define RENDERER_FEATURE_READBACK uint16_t(32)
#define RENDERER_FEATURE_TEXBLIT uint16_t(64)
#define RENDERER_FEATURE_SHADOWSAMPLER uint16_t(128)
// Each drawPass_Shape call will update only the draw entries where a structural change happened, either
// (false) fully or (true) only for the data that has changed within the draw entry
#define RENDERER_USE_SLICES true
// Hold rendering draw entries for a grace period after texture is created
#define RENDERER_DRAW_ENTRY_GRACE_FRAMES 3
// Release inactive draw entries after a number of frames
#define RENDERER_DRAW_ENTRY_INACTIVE_FRAMES 180
// Internal switch for skybox shader with / without cubemaps
#define RENDERER_SKYBOX_USES_CUBEMAPS false
// Prevent using features even if available
#define RENDERER_ALLOW_TEX3D false
#define RENDERER_ALLOW_MRT true
#define RENDERER_ALLOW_INSTANCING true
#define RENDERER_ALLOW_COMPUTE true
#define RENDERER_ALLOW_INDIRECT true
#define RENDERER_ALLOW_READBACK true
#define RENDERER_ALLOW_TEXBLIT true
#define RENDERER_ALLOW_SHADOWSAMPLERS false // stuck for light pass, bgfx doesn't allow multiple depth attachments
// View ordering
#define VIEW_CLEAR_1                0 // clear views only used when first target per segment is not fullscreen
#define VIEW_CLEAR_2                1
#define VIEW_CLEAR_3                2
#define VIEW_SKYBOX                 3
#define VIEW_STENCIL(start)                 (start + 1) // first target view
#define VIEW_OPAQUE(start)                  (start + 2)
#define VIEW_SHADOWS_1(start)               (start + 3)
#define VIEW_SHADOWS_2(start)               (start + 4)
#define VIEW_SHADOWS_3(start)               (start + 5)
#define VIEW_SHADOWS_4(start)               (start + 6)
#define VIEW_LIGHTS(start)                  (start + 7)
#define VIEW_TRANSPARENT_WEIGHT(start)      (start + 8)
#define VIEW_BLIT(start)                    (start + 9)
#define VIEW_TRANSPARENT(start)             (start + 10) // last target view
#define TARGET_VIEWS_COUNT VIEW_TRANSPARENT(0)
// ⚠ post-process targets views, expands up to VIEW_SKYBOX + nbPostTargets * TARGET_VIEWS_COUNT
// VIEW_POSTPROCESS: dynamically set to last target view + 1
// ⚠ after-post-process targets views, expands up to VIEW_POSTPROCESS + nbAfterPostTargets * TARGET_VIEWS_COUNT
// VIEW_AFTER_POST: dynamically set to last after-post-process target view + 1
#define VIEW_UI_OVERLAY(afterpostView) (afterpostView + 1)
#define VIEW_UI_BLOCKER(afterpostView) (afterpostView + 2)
#define VIEW_UI_IMGUI(afterpostView) (afterpostView + 3)
// ⚠ lua System UI targets views, expands up to VIEW_UI_IMGUI + nbSystemTargets * TARGET_VIEWS_COUNT
// VIEW_UI_SYSTEM: dynamically set to last system target view + 1
#define SORT_ORDER_STENCIL      20000 // ⚠ expands sort order
#define SORT_ORDER_DEFAULT      15000
#define SORT_ORDER_DECALS       10000 // ⚠ expands sort order
//#define SORT_ORDER_WORLDTEXT    5000  // ⚠ expands sort order
/// Shadowmaps
#define SHADOWMAP_SPHERIZE true
#define SHADOWMAP_STABILIZE_TEXEL true
#define SHADOWMAP_BLUR_CLEAR 0xffffffff
#define SHADOWMAP_CASCADES_MAX 4
static float SHADOWMAP_CASCADES_SIZE_FACTORS[4] = {
    1.0f, // shadowmap 1 (closest)
    0.5f, // shadowmap 2
    0.5f, // shadowmap 3
    0.5f // shadowmap 4 (farthest)
};
#define SHADOWMAP_NORMAL_OFFSET 0.4f
/// Performance & quality tiers
#define RENDER_UPSCALE_ENABLED true
#define RENDER_SCALE_MAX_TEX 5376 // 3840 * 1.4, avoids ludicrous tex size on 4k screens
#define SHAPE_PER_CHUNK_DRAW false
#define SHAPE_PER_CHUNK_VB_MODE 0 // 0: static, 1: dynamic
/// per quality tier: off (0), low (1), medium (2), high (3), ultra (4)
static uint16_t SHADOWCASTERS_MAX[5] = { 0, 1, 1, 1, 1 };
#if defined(__VX_PLATFORM_ANDROID) || defined(__VX_PLATFORM_WASM)
static bool SHADOWMAP_FILTERING[5] = { false, true, true, true, true };
static uint8_t SHADOWMAP_CASCADES_COUNT[5] = { 0, 1, 1, 1, 2 };
static uint16_t SHADOWMAP_MAX_SIZE[5] = { 0, 512, 1024, 2048, 2048 };
static uint16_t SHADOWMAP_MAX_VIEW_DIST[5] = { 0, 200, 250, 250, 500 };
static uint16_t SHADOWMAP_DIRECTIONAL_DISTANCE[5] = { 0, 0, 0, 0, 250 };
static float SHADOWMAP_BIAS[5] = { 0.0f, 0.0015f, 0.003f, 0.003f, 0.003f };
#elif defined(__VX_PLATFORM_IOS)
static bool SHADOWMAP_FILTERING[5] = { false, true, true, true, true };
static uint8_t SHADOWMAP_CASCADES_COUNT[5] = { 0, 1, 1, 2, 2 };
static uint16_t SHADOWMAP_MAX_SIZE[5] = { 0, 512, 1024, 2048, 4096 };
static uint16_t SHADOWMAP_MAX_VIEW_DIST[5] = { 0, 100, 200, 300, 400 };
static uint16_t SHADOWMAP_DIRECTIONAL_DISTANCE[5] = { 0, 0, 200, 250, 300 };
static float SHADOWMAP_BIAS[5] = { 0.0f, 0.0015f, 0.0015f, 0.0015f, 0.0015f };
#else
static bool SHADOWMAP_FILTERING[5] = { false, true, true, true, true };
static uint8_t SHADOWMAP_CASCADES_COUNT[5] = { 0, 1, 2, 2, 4 };
static uint16_t SHADOWMAP_MAX_SIZE[5] = { 0, 1024, 2048, 4096, 4096 };
static uint16_t SHADOWMAP_MAX_VIEW_DIST[5] = { 0, 200, 400, 600, 800 };
static uint16_t SHADOWMAP_DIRECTIONAL_DISTANCE[5] = { 0, 200, 300, 400, 500 };
static float SHADOWMAP_BIAS[5] = { 0.0f, 0.0015f, 0.0015f, 0.0015f, 0.0015f };
#endif
/// per platform
#if defined(__VX_PLATFORM_WASM)
#define LIGHTS_MAX 16
#define CAMERAS_MAX 4
#elif defined(__VX_PLATFORM_ANDROID)
#define LIGHTS_MAX 16
#define CAMERAS_MAX 4
#elif defined(__VX_PLATFORM_IOS)
#define LIGHTS_MAX 32
#define CAMERAS_MAX 4
#else
#define LIGHTS_MAX 65535
#define CAMERAS_MAX 8
#endif

//// RENDERER DEBUG CONFIG
// General toggle for renderer debug features, and to enable some shader debug features
// for shader config, see: common/shaders/config.sh
#if DEBUG
    #define DEBUG_RENDERER false
#endif
#if DEBUG && DEBUG_RENDERER
    // Display rendering & shape stats
    #define DEBUG_RENDERER_DISPLAY_FPS true
    #define DEBUG_RENDERER_STATS true
    #define DEBUG_RENDERER_STATS_GAME false
    #define DEBUG_RENDERER_STATS_VB_OCCUPANCY false
    // Each drawPass_Shape call will fully update ALL of its draw entries, each frame, whether or not they have changed
    #define DEBUG_RENDERER_UPDATE_ALL false
    // Release every draw entries in between frames
    #define DEBUG_RENDERER_RELEASE_ALL false
    // Force a voxel features flag value
    #define DEBUG_RENDERER_FORCE_FEATURES false
    #define DEBUG_RENDERER_VOXEL_FEATURES 0
    // Display an overlay w/ scene hierarchy
    #define DEBUG_RENDERER_DISPLAY_HIERARCHY false
    // Display scene transforms bounding boxes
    #define DEBUG_RENDERER_DISPLAY_BOXES false
    // Display scene transforms colliders
    #define DEBUG_RENDERER_DISPLAY_COLLIDERS false
    // Display scene r-tree stats & calls
    #define DEBUG_RENDERER_DISPLAY_RTREE_STATS false
    // Display scene r-tree boxes
    #define DEBUG_RENDERER_DISPLAY_RTREE_BOXES false
    // Display an overlay w/ r-tree hierarchy, replaces scene hierarchy if both are toggled
    #define DEBUG_RENDERER_DISPLAY_RTREE false
    // Display rigidbody stats & calls
    #define DEBUG_RENDERER_DISPLAY_RIGIDBODY_STATS false
    // Display sub-renderers stats & calls (whichever enabled)
    #define DEBUG_RENDERER_SUBRDR_STATS false
    // Update full color atlas every frame
    #define DEBUG_RENDERER_COLOR_ATLAS_FULL false
    // Shadows always ON, shadow decals always OFF
    #define DEBUG_RENDERER_SHADOWS_FORCED_ON false
    // Display geometry culling spheres
    #define DEBUG_RENDERER_DISPLAY_CULLSPHERES false
    #define DEBUG_RENDERER_DISPLAY_CHUNKS_CULLSPHERES false
    // Display shape chunks' bounding boxes
    #define DEBUG_RENDERER_DISPLAY_CHUNKS false
#else
    #define DEBUG_RENDERER_DISPLAY_FPS false
    #define DEBUG_RENDERER_STATS false
    #define DEBUG_RENDERER_STATS_GAME false
    #define DEBUG_RENDERER_STATS_VB_OCCUPANCY false
    #define DEBUG_RENDERER_UPDATE_ALL false
    #define DEBUG_RENDERER_RELEASE_ALL false
    #define DEBUG_RENDERER_FORCE_FEATURES false
    #define DEBUG_RENDERER_DISPLAY_HIERARCHY false
    #define DEBUG_RENDERER_DISPLAY_BOXES false
    #define DEBUG_RENDERER_DISPLAY_COLLIDERS false
    #define DEBUG_RENDERER_DISPLAY_RTREE_STATS false
    #define DEBUG_RENDERER_DISPLAY_RTREE_BOXES false
    #define DEBUG_RENDERER_DISPLAY_RTREE false
    #define DEBUG_RENDERER_DISPLAY_RIGIDBODY_STATS false
    #define DEBUG_RENDERER_SUBRDR_STATS false
    #define DEBUG_RENDERER_COLOR_ATLAS_FULL false
    #define DEBUG_RENDERER_SHADOWS_FORCED_ON false
    #define DEBUG_RENDERER_DISPLAY_CULLSPHERES false
    #define DEBUG_RENDERER_DISPLAY_CHUNKS false
#endif
#define DEBUG_RENDERER_FPS_MOVING_AVERAGE 240

namespace vx {

    typedef std::function<void(void)> BlockerCallback;

    class GameRendererState;
    typedef std::shared_ptr<GameRendererState> GameRendererState_SharedPtr;
    typedef std::weak_ptr<GameRendererState> GameRendererState_WeakPtr;

    class GameRendererState {
    public:
        GameRendererState();
        virtual ~GameRendererState();
        void setDefaultsFromMap(const Shape *map);

        void resetDirty(uint8_t flag);
        bool isDirty(uint8_t flag) const;
        bool isDirty() const;
        void setAllDirty();

        void setSkyboxTextures(Texture *tex[SKYBOX_MAX_CUBEMAPS]);
        void setSkyboxLerp(float t);
        void setSkyColor(float3 rgb);
        void setHorizonColor(float3 rgb);
        void setAbyssColor(float3 rgb);

        // Fog
        /// Distance fog based of ambient color
        bool fogToggled();
        void toggleFog(bool value);
        /// Distance fog colouring increases between start and end distances, beyond that it is fully colored
        float getFogStart();
        void setFogStart(float value);
        float getFogEnd();
        void setFogEnd(float value);
        /// [0:1] ratio of emission absorbed through fog relative to fog current properties
        /// - 0 means emission is never affected by fog
        /// - eg. 0.4 means emission is absorbed along a 60% longer distance than lit colors
        /// - 1 means emission is absorbed at the same pace as lit colors
        /// - note: values higher than 1 will make emission be mixed in fog at a faster pace than lit colors
        float getFogEmission();
        void setFogEmission(float value);
        uint16_t getFogLayers();
        void setFogLayers(uint16_t value);

        // Sky system
        bool cloudsToggled();
        void toggleClouds(bool value);
        bool getCloudsUnlit();
        void setCloudsUnlit(bool value);
        float3 getCloudsColor();
        void setCloudsColor(float3 rgb);
        float3 getCloudsOrigin();
        void setCloudsOrigin(float3 value);
        float getCloudsWidth();
        void setCloudsWidth(float value);
        float getCloudsDepth();
        void setCloudsDepth(float value);
        float getCloudsBaseScale();
        void setCloudsBaseScale(float value);
        float getCloudsAddScale();
        void setCloudsAddScale(float value);
        float getCloudsSpread();
        void setCloudsSpread(float value);
        float getCloudsSpeed();
        void setCloudsSpeed(float value);

        /// Fog
        bool fog;
        float fogStart, fogEnd;
        float fogEmission;
        uint16_t fogLayers;
        float3 fogColor;

#ifndef P3S_CLIENT_HEADLESS
        /// Sky system
        SkyState sky;
#endif

        /// Skybox
        uint32_t clearColor;
        float3 skyColor; // TODO to uint32_t, RGB packed in 1 float
        float3 abyssColor;
        float3 horizonColor;
        float3 skyLightColor;
        Texture *skyboxTex[SKYBOX_MAX_CUBEMAPS];
        float skyboxLerp;
        SkyboxMode skyboxMode;

        /// Lighting
        float lightsIntensity, bakedIntensity, lightsAmbientFactor, skyboxAmbientFactor;
        float3 ambientColorOverride;

        /// Public debug
        bool debug_displayBoxes;
        bool debug_displayColliders;
        bool debug_displayFPS;

    private:
        void setDirty(uint8_t flag);

        /// Dirty flag
        /// fog:clear:fov:-:-:-:-:-
        uint8_t _dirty;
    };

#ifndef P3S_CLIENT_HEADLESS
    struct RendererFeatures {
        /// Pipeline parameters
        bgfx::TextureFormat::Enum depthFormat, depthStencilFormat;
        bgfx::TextureFormat::Enum mrt_rgba16fFormat, mrt_r16fFormat, mrt_rg8Format;
        bool screenquadFlipV, linearDepthGBuffer, NDCDepth;

        /// Features mask
        uint16_t mask;

        bool isTexture3DEnabled() const;
        bool isMRTEnabled() const;
        bool isInstancingEnabled() const;
        bool isComputeEnabled() const;
        bool isDrawIndirectEnabled() const;
        bool isTextureReadBackEnabled() const;
        bool isTextureBlitEnabled() const;
        bool isShadowSamplerEnabled() const;

        bool useDeferred() const;
    };
#endif

    struct QualityParameters {
        uint8_t shadowsTier;
        float renderScale;
        AntiAliasing aa;
        uint8_t postTier;

        void fromPreset(QualityTier tier);
    };

    class GameRenderer {
    public:

        virtual ~GameRenderer();

        static GameRenderer *newGameRenderer(QualityTier tier, uint16_t width, uint16_t height, float devicePPP);
        static GameRenderer *getGameRenderer();
        static void memoryRefRelease(void* ptr, void* data);
        /// Standalone helper to populate given game's camera proj w/ screen & game state at the moment of calling
        static void setTemporaryCameraProj(Game *g);
        static uint64_t getRenderState(int32_t stateIdx, RenderPassState pass, TriangleCull cull=TriangleCull_Back, PrimitiveType type=PrimitiveType_Triangles, bool disableDepthTest=false);
        static bool hasTextureCapability(uint32_t count);
        static bool hasVBCapability(uint32_t count, bool dynamic);
        static bool hasIBCapability(uint32_t count, bool dynamic);
        /// @param faceData expected order: right, left, top, bottom, front, back
        static Texture *cubemapFacesToTexture(const void *faceData[6], size_t faceDataSize[6], bool filtering);

        typedef enum {
            none,       // ratio not enforced
            crop,       // crops to enforce ratio
            padding, // adds transparent pixels around
        } EnforceRatioStrategy;
        
        /// @param out pointer to RGBA texels or PNG content if "outPng" is true
        /// @param outWidth pointer to final width
        /// @param outHeight pointer to final height
        /// @param outSize pointer to final size, equals to width * height * 4 for texels data, or png content size
        /// @param filename optionally save a PNG file in app storage (do not include extension)
        /// @param outPng optionally dump PNG content in "out" pointer
        /// @param noBg optionally set true to have a transparent background
        /// @param maxWidth optionally specify a max width to downscale
        /// @param maxHeight optionally specify a max height to downscale
        /// @param unflip optionally ask to not unflip texels through copy, since this isn't really
        /// @param trim optionally ask to remove transparent pixels all around
        /// @param ratioStrategy optionally set a strategy to enforce the ratio (none by default)
        /// @param outputRatio output ratio, only enforced if a strategy other than none is set
        /// necessary before writing (see VXRDUtils::savePng) and may be done efficiently elsewhere
        /// by the caller eg. by simply stepping backwards (-pitch)
        void screenshot(void** out, uint16_t *outWidth, uint16_t *outHeight, size_t *outSize,
                        const std::string& filename="",
                        bool outPng=false,
                        bool noBg=false,
                        uint16_t maxWidth=0,
                        uint16_t maxHeight=0,
                        bool unflip=true,
                        bool trim=true,
                        EnforceRatioStrategy ratioStrategy = none,
                        float outputRatio = 0.0f);
        void getOrComputeWorldTextSize(WorldText *wt, float *outWidth, float *outHeight, bool points);
        float getCharacterSize(FontName font, const char *text, bool points);
        size_t snapWorldTextCursor(WorldText *wt, float *inoutX, float *inoutY, size_t *inCharIndex, bool points);

        bool isFallbackEnabled();
        QualityTier getQualityTier();
        float getRenderWidth();
        float getRenderHeight();
        bool reload(QualityTier tier);
        bool reload(QualityParameters params);
        void togglePBR(bool toggle);
        bool isPBR();
        void debug_checkAndDrawShapeRtree(const Transform *t, const Shape *s, const int onlyLevel=-1);

#ifndef P3S_CLIENT_HEADLESS
        void tick(const double dt);
        void render(bgfx::FrameBufferHandle outFbh=BGFX_INVALID_HANDLE);
        void refreshViews(uint16_t width, uint16_t height, float devicePPP);
        bgfx::ViewId getImguiView();
#endif

#if DEBUG_RENDERER
        float _debug_shadowOffset, _debug_shadowBias;
#endif
    protected:
        GameRenderer(QualityTier tier, uint16_t width, uint16_t height, float devicePPP);
    private:
        //// Private singleton instance:
        /// - renderer lifecycle is controlled by app lifecycle
        /// - calling static functions w/o renderer will do nothing
        static GameRenderer *_instance;

        /// Game currently rendered, and next game to render after fade-out/fade-in animation
        Game_SharedPtr _game;
        Game_SharedPtr _gameFadingIn;

        GameRendererState_SharedPtr _gameState;
        Shape *_currentMapShape; // TODO: remove this
        float _time;

#ifndef P3S_CLIENT_HEADLESS

        //// List of geometry to draw, built each frame
        typedef enum {
            GeometryType_Shape,
            GeometryType_WorldText,
            GeometryType_Collider,
            GeometryType_BoundingBox,
            GeometryType_ModelBox,
            GeometryType_Quad,
            GeometryType_Decals,
            GeometryType_Mesh
        } GeometryType;
        typedef enum {
            GeometryCulling_None,
            GeometryCulling_Intersect,
            GeometryCulling_Culled
        } GeometryCulling;
        struct GeometryEntry {
            void *ptr;
            float3 center;
            float radius;
            float eyeSqDist;
            GeometryType type;
            VERTEX_LIGHT_STRUCT_T sampleColor;
            uint16_t layers;
            uint8_t maskID, maskDepth;
            bool stencilWrite;
            bool geometryCulled;
            bool worldDirty;
            bool cameraDirty;
        };
        std::unordered_map<GeometryType, DoublyLinkedList*> _geometryEntries;

        //// Match shape engine buffers unique ID to corresponding VB/IB
#if SHAPE_PER_CHUNK_DRAW
        struct ChunkCullingEntry {
            float3 center;
            float radius;
            bool geometryCulled;
        };
#endif
        struct ShapeBufferEntry {
#if SHAPE_PER_CHUNK_DRAW
            std::unordered_map<Chunk *, ChunkCullingEntry> chunks;
#if SHAPE_PER_CHUNK_VB_MODE == 0
            std::unordered_map<GeometryRenderPass, bgfx::VertexBufferHandle> vbh;
#elif SHAPE_PER_CHUNK_VB_MODE == 1
            std::unordered_map<GeometryRenderPass, bgfx::DynamicVertexBufferHandle> vbh;
#endif
#endif // SHAPE_PER_CHUNK_DRAW
            uint32_t count;                                 /* 4 bytes */
            bgfx::VertexBufferHandle static_vbh;            /* 2 bytes */
            bgfx::DynamicVertexBufferHandle dynamic_vbh;    /* 2 bytes */
            uint16_t inactivity;                            /* 2 bytes */
        };
        std::unordered_map<uint32_t, ShapeBufferEntry> _shapeEntries;

        //// Shared vertex/index buffer data used by all shape entries
        uint32_t *_triangles;
        bgfx::IndexBufferHandle _voxelIbh;

        //// Quads batching
        typedef struct {
            QuadVertex *vertices;
            uint64_t state;              /* 8 bytes */
            uint32_t capacity, count;    /* 2x4 bytes */
            bgfx::ProgramHandle program; /* 2 bytes */
            bgfx::VertexBufferHandle vb; /* 2 bytes */
        } QuadsBatch;
        QuadsBatch _quadsBatch;
        uint32_t _quadsBatchAllMaxCount, _quadsBatchTexturedMaxCount;

        //// Match quad data hash with uvtexture & batch
        struct QuadEntry {
            QuadsBatch batch;                   /* 8 bytes */
            UVTexture tex;                      /* 4 bytes */
            MediaType type;                     /* 4 bytes */
            uint16_t width, height;             /* 2x2 bytes */
            int16_t inactivity;                 /* 2 bytes */
        };
        std::unordered_map<uint32_t, QuadEntry> _quadEntries;

        //// Match mesh id to VB/IB
        struct MeshEntry {
            uint32_t count;                                 /* 4 bytes */
            bgfx::VertexBufferHandle static_vbh;            /* 2 bytes */
            bgfx::DynamicVertexBufferHandle dynamic_vbh;    /* 2 bytes */
            bgfx::IndexBufferHandle static_ibh;             /* 2 bytes */
            bgfx::DynamicIndexBufferHandle dynamic_ibh;     /* 2 bytes */
            uint16_t inactivity;                            /* 2 bytes */
        };
        std::unordered_map<uint32_t, MeshEntry> _meshEntries;

        //// Match texture hash to texture handle
        struct TextureEntry {
            bgfx::TextureHandle th;             /* 2 bytes */
            int16_t inactivity;                 /* 2 bytes */
        };
        std::unordered_map<uint32_t, TextureEntry> _textureEntries;

        //// Skybox & global lighting
        bgfx::UniformHandle _uSkyColor;
        bgfx::UniformHandle _uHorizonColor;
        bgfx::UniformHandle _uAbyssColor;
        float _uFogColorData[4];
        float _uSunColorData[4];
        float _uSkyColorData[4];
        float _uHorizonColorData[4];
        float _uAbyssColorData[4];
        
        //// Light sources from the scene
        std::map<uint8_t, DoublyLinkedList*> _lights;
        bgfx::UniformHandle _uLightParams;
        float _uLightParamsData[20];
        bgfx::UniformHandle _uLightVPMtx[SHADOWMAP_CASCADES_MAX];
        bgfx::UniformHandle _uShadowParams;
        float _uShadowParamsData[4];
        float _lightsAmbient[3];
        float _faceShadingFactor;

        //// Framebuffers textures samplers
        bgfx::UniformHandle _sFb[8];

        //// Used to upload per-draw parameters
        bgfx::UniformHandle _uParams, _uParams_fs;
        float _uParamsData[4], _uParamsData_fs[4];

        //// Used to upload draw entry override parameters used by draw modes
        /// - VS
        /// [0] alpha override
        /// [1] multiplicative R
        /// [2] multiplicative G
        /// [3] multiplicative B
        /// [4] additive R
        /// [5] additive G
        /// [6] additive B
        /// [7] <unused>
        /// - FS
        /// [0] grid scale magnitude
        /// [1] grid R
        /// [2] grid G
        /// [3] grid B
        bgfx::UniformHandle _uOverrideParams, _uOverrideParams_fs;
        float _uOverrideParamsData[8], _uOverrideParamsData_fs[4];

        //// Used to upload gamewise parameters
        /// [0] field of view (degree)
        /// [1] time
        /// [2] camera far
        /// [3] palette texture size
        /// [4] baked intensity
        /// [5] <unused>
        /// [6] <unused>
        /// [7] <unused>
        bgfx::UniformHandle _uGameParams;
        float _uGameParamsData[8];

        //// Used to upload the shared color atlas
        bgfx::TextureHandle _colorAtlasTexture;
        bgfx::UniformHandle _sColorAtlas;

        //// Used to upload ambient lighting data
        bgfx::UniformHandle _uFogColor;
        bgfx::UniformHandle _uSunColor;

        //// Programs (vert & frag shaders) driving each draw call
        bgfx::ProgramHandle _shapeProgram, _shapeProgram_dm;
        bgfx::ProgramHandle _shapeProgram_weight, _shapeProgram_dm_weight;
        bgfx::ProgramHandle _shapeProgram_sm;
        bgfx::ProgramHandle _blitProgram, _blitProgram_alpha;
        bgfx::ProgramHandle _lightProgram_point, _lightProgram_spot, _lightProgram_directional;
        bgfx::ProgramHandle _lightProgram_point_sm, _lightProgram_spot_sm, _lightProgram_directional_sm;
        bgfx::ProgramHandle _vertexColorProgram;
        bgfx::ProgramHandle _skyboxProgram_linear, _skyboxProgram_cubemap, _skyboxProgram_cubemapLerp;
        bgfx::ProgramHandle _colorProgram, _uiColorProgram;
        bgfx::ProgramHandle _uvProgram;
        bgfx::ProgramHandle _cloudsProgram, _cloudsProgram_unlit, _cs_cloudsUpdateProgram, _cs_cloudsIndirectProgram;
        bgfx::ProgramHandle _fontProgram_unlit, _fontProgram, _fontProgram_overlay_unlit, _fontProgram_overlay;
        bgfx::ProgramHandle _fontProgram_weight;
        bgfx::ProgramHandle _targetProgram;
        bgfx::ProgramHandle _quadProgram, _quadProgram_alpha;
        bgfx::ProgramHandle _quadProgram_weight;
        bgfx::ProgramHandle _quadProgram_tex, _quadProgram_tex_cutout, _quadProgram_tex_alpha, _quadProgram_tex_cutout_alpha;
        bgfx::ProgramHandle _quadProgram_tex_weight, _quadProgram_tex_cutout_weight;
        bgfx::ProgramHandle _quadProgram_sm, _quadProgram_sm_tex_cutout, _quadProgram_stencil, _quadProgram_stencil_tex_cutout;
        bgfx::ProgramHandle _meshProgram, _meshProgram_cutout, _meshProgram_alpha, _meshProgram_sm;
        bgfx::ProgramHandle _postProgram;

        //// Enabled renderer features & formats
        RendererFeatures _features;

        //// Opaque framebuffer
        bgfx::TextureHandle _opaqueFbTex[7];
        uint8_t _opaqueFb_depthIdx;
        bgfx::TextureHandle _depthStencilBuffer, _depthStencilBlit;
        bgfx::FrameBufferHandle _opaqueFbh;

        //// Lighting/shadow pass framebuffers
        bgfx::TextureHandle _lightFbTex[2], _shadowmapTh[SHADOWMAP_CASCADES_MAX];
        bgfx::FrameBufferHandle _lightFbh, _shadowFbh[SHADOWMAP_CASCADES_MAX];

        //// Transparency framebuffer
        bgfx::TextureHandle _transparencyFbTex[3];
        bgfx::FrameBufferHandle _transparencyFbh;

        //// Targets framebuffer, before and after post-process
        bgfx::TextureHandle _targetsFbTex, _afterPostTargetsFbTex, _systemTargetsFbTex;
        bgfx::FrameBufferHandle _targetsFbh, _afterPostTargetsFbh, _systemTargetsFbh;

        //// Postprocess framebuffer, used to scale to backbuffer size
        bgfx::TextureHandle _postprocessFbTex;
        bgfx::FrameBufferHandle _postprocessFbh;
        
        //// Used to maintain shadow decals requests
        std::unordered_map<uint16_t, bool> _shadowDecals;

        //// Used to maintain camera targets
        /*struct CameraEntry {
            bgfx::TextureHandle targetFbTex;
            bgfx::FrameBufferHandle targetFbh;
            bool active;
        };
        std::unordered_map<Camera*, CameraEntry> _targets;*/
        uint8_t _maxTarget, _maxView, _maxPerspectiveViewOrder;
        enum PipelineSegment {
            PipelineSegment_PostTarget,
            PipelineSegment_AfterPostTarget,
            PipelineSegment_SystemTarget
            #define PipelineSegment_COUNT 3
        };
        struct CameraEntry {
            Camera *camera;
            PipelineSegment segment;
        };
        std::map<uint8_t, DoublyLinkedList*> _cameras;
        bgfx::ViewId _postprocessView1, _postprocessView2, _afterpostView, _systemView;

        //// Pipeline dimensions,
        /// points width/height used projection dimensions on UI-related views
        uint16_t _pointsWidth, _pointsHeight;
        /// scaled width/height used for geometry-related perspective views
        uint16_t _renderWidth, _renderHeight;
        /// native device dimensions used for UI and ortho views
        uint16_t _screenWidth, _screenHeight;
        float _screenPPP;
        /// size used for framebuffers, this is the maximum size between render size & screen size
        uint16_t _framebufferWidth, _framebufferHeight;

        //// Quality
        QualityParameters _qualityParams;
        QualityTier _qualityPreset;
        bool _emulateShadows;

        bool _toggled, _errored;
        bool _viewsDirty, _shadowmapsDirty, _programsDirty;
        Decals *_decals;
        bgfx::TextureHandle _cookie;
        WorldTextRenderer *_worldText;
        Sky *_sky;
        GifRenderer *_gif;
        bgfx::UniformHandle _uColor1, _uColor2, _uMtx;
        float _blockerDuration, _blockerTimer;
        bool _blockerInOut;
        uint8_t _blockerGraceFrames;
        BlockerCallback _blockerCallback;
        bx::DefaultAllocator *_bxAllocator;
        bool _blitAlpha;
        bool _overlayDrawn;
        bool _usePBR;
        bgfx::TextureHandle _dummyTh;

        // MARK: - DRAW FUNCTIONS -
        void addGeometryEntry(GeometryType type, void *ptr, VERTEX_LIGHT_STRUCT_T sample, float3 center, float radius, uint16_t layers, uint8_t maskID, uint8_t maskDepth, bool stencilWrite, bool worldDirty=true);
        static void releaseGeometryEntry(void *ptr);
        static bool getGeometryForCamera_sortFunc(DoublyLinkedListNode *n1, DoublyLinkedListNode *n2);
        size_t getGeometryForCamera(const Camera *camera, std::unordered_map<GeometryType, DoublyLinkedList*> &outGeom);
        void preDrawCollectData_recurse(Transform *t, bool hierarchyDirty, bool hidden, bool sample, VERTEX_LIGHT_STRUCT_T *color, uint8_t *nextMaskID, uint8_t maskID=0, int maskDepth=-1);
        void preDrawCollectData();
        void preparePipelineForCameras();
        void drawSphere(bgfx::ViewId id, const float3 *center, float radius, uint32_t depth=SORT_ORDER_DEFAULT, uint32_t stencil=BGFX_STENCIL_DEFAULT);
        void drawBox(bgfx::ViewId id, const Box *box, const float3 *rot, uint32_t abgr=0, uint32_t depth=SORT_ORDER_DEFAULT, uint32_t stencil=BGFX_STENCIL_DEFAULT);
        void drawCube(bgfx::ViewId id, const float *mtx , uint32_t abgr=0, uint32_t depth=SORT_ORDER_DEFAULT, uint32_t stencil=BGFX_STENCIL_DEFAULT);
        bool drawPass_Lights_cull(const Light *light, const float *viewProj, float projWidth, float projHeight, float4 *scissor);
        void drawPass_Lights_shadowmap(bgfx::ViewId *shadowmaps, const Game_SharedPtr& game, const Light *light, float *invViewProj, float *out_lightViewProj, bool shadowCaster, std::unordered_map<GeometryType, DoublyLinkedList*> &geometry);
        void drawPass_Lights_add(bgfx::ViewId id, const Light *light, const float *invViewProj, const float *lightViewProj, const Camera *camera, const float4 *scissor, bool shadowCaster, float uMax, float vMax);
        bool drawPass_Lights(bgfx::ViewId add, bgfx::ViewId *shadowmaps, const Game_SharedPtr& game, float *invView, float *viewProj, const Camera *camera, float projWidth, float projHeight, float uMax, float vMax, std::unordered_map<GeometryType, DoublyLinkedList*> &geometry);
        bool drawPass_Shape(bgfx::ViewId id, Transform *t, Shape *shape, VERTEX_LIGHT_STRUCT_T sample, GeometryRenderPass pass, Plane **planes, bool perChunkDraw, bool worldDirty, bool cameraDirty, float3 *wireframe=nullptr, uint32_t depth=SORT_ORDER_DEFAULT, uint32_t stencil=BGFX_STENCIL_DEFAULT);
        bool drawPass_Quad(bgfx::ViewId id, Quad *quad, VERTEX_LIGHT_STRUCT_T sample, GeometryRenderPass pass, float *view, float *invView, bool pointsScaling, uint32_t depth=SORT_ORDER_DEFAULT, uint32_t stencil=BGFX_STENCIL_DEFAULT);
        bool drawPass_QuadBatch(bgfx::ViewId id, QuadsBatch *batch, bgfx::TextureHandle th=BGFX_INVALID_HANDLE, uint32_t depth=SORT_ORDER_DEFAULT);
        bool drawPass_Mesh(bgfx::ViewId id, Mesh *mesh, VERTEX_LIGHT_STRUCT_T sample, GeometryRenderPass pass, uint32_t depth=SORT_ORDER_DEFAULT, uint32_t stencil=BGFX_STENCIL_DEFAULT);
        bool drawPass_Geometry(bgfx::ViewId id, const Game_SharedPtr& game, GeometryRenderPass pass, float *view, float *invView, bool isBackdrop, bool pointsScaling, Plane **planes, std::unordered_map<GeometryType, DoublyLinkedList*> &geometry);
        void drawSkybox(bgfx::ViewId id, Camera *camera, float uMax, float vMax, SkyboxMode mode);
        void drawDeferred(bgfx::ViewId id, bool blitAlpha, bool blend, Camera *camera, float *proj, float projWidth, float projHeight, float targetWidth, float targetHeight, float uMax, float vMax, uint16_t layers);
        void drawTarget(bgfx::ViewId id, bgfx::ProgramHandle program, bgfx::TextureHandle target, uint32_t depth, float x, float y, float width, float height, float fullWidth, float fullHeight, float uMax, float vMax, bool blend);
        void drawPost(bgfx::ViewId id, bgfx::TextureHandle input, uint16_t width, uint16_t height, float uMax, float vMax) ;
        void drawBlocker(bgfx::ViewId, float alpha);

        // MARK: - RENDERER LIFECYCLE -
        void start(const Game_SharedPtr& game);
        void stop();
        void fade(float duration, bool inOut, uint8_t graceFrames= 0, BlockerCallback callback= nullptr);
        void checkAndSetRendererFeatures();
        void resetOverrideParams();
        void setGameParams(float fov, float farPlane, float paletteSize);
        void setLightingParams();
        uint32_t updateColorAtlas(CGame *cgame);
        void setStartupViews(uint16_t width, uint16_t height);
        void setCommonViews();
        void setDefaultViews();
        void setTargetViews(PipelineSegment segment, uint8_t target, bool isBackdrop, uint16_t width, uint16_t height,
                            bgfx::ViewId *stencil, bgfx::ViewId *opaque, bgfx::ViewId *lights,
                            bgfx::ViewId *shadows, bgfx::ViewId *weight, bgfx::ViewId *blitDeferred,
                            bgfx::ViewId *transparent);
        void clearViews();
        bool refreshViewsInternal();
        void refreshDefaultViews();
        void refreshStateInternal();
        void setSubRenderers();
        void releaseSubRenderers();
        void setSkybox();
        void releaseSkybox();
        void setStaticResources();
        void releaseStaticResources();
        void setPrimitivesStaticData();
        void releasePrimitivesStaticData();
        void setUniforms();
        void releaseUniforms();
        void setPrograms();
        void releasePrograms();
        bgfx::FrameBufferHandle getPipelineSegmentFramebuffer(const PipelineSegment s);
        bool createFramebuffers(uint16_t width, uint16_t height);
        void releaseFramebuffers();
        bool createShadowmaps();
        void releaseShadowmaps();
        void setGame(const Game_SharedPtr& game);
        void toggleInternal(bool);
        void clearObjectLists();

        // MARK: - LIGHTING -
        void requestShadowDecal(uint32_t id, const float3 *pos, float width, float height, Transform *filterOut, uint32_t stencil);
        void clearShadowDecals();
        VERTEX_LIGHT_STRUCT_T sampleBakedLightingFromPoint(float x, float y, float z);
        void setFogColor(const float3 *fog);
        void setSkyboxColors(const float3 *sky, const float3 *horizon, const float3 *abyss, const float3 *skyLight);
        void refreshGlobalColors();
        void setFogPlanes(float start, float end);
        void setFogInternal();
        void setFogEmissionValue(float v);

        // MARK: - CAMERA -
        /*bool createCameraTarget(CameraEntry *entry, const float width, const float height);
        void clearCameraTargets(const bool purge=false);*/

        // MARK: - SHAPE ENTRY -
        bool createShapeEntry(GameRenderer::ShapeBufferEntry *entry, uint32_t nbFaces);
        void setBuffer2DIndexing(uint32_t flat, uint16_t *u, uint16_t *v, bool isSize, uint16_t bufferSize);
        void updateTex2D(const void *data, uint32_t size, bgfx::TextureHandle handle, uint16_t tx, uint16_t ty, uint16_t tw, uint16_t th, const uint16_t pitch);
        void updateShapeBuffers(ShapeBufferEntry *entry, VertexAttributes *vertices, uint32_t start, uint32_t end);
        void updateShapeEntry(ShapeBufferEntry *entry, uint32_t nbVertices, VertexAttributes *vertices, DoublyLinkedList *slices);
        void releaseShapeEntry(ShapeBufferEntry *entry);
        void releaseShapeEntries(uint32_t count= UINT32_MAX);
        void popShapeEntries();
        void checkAndPopInactiveShapeEntries();
        bgfx::ProgramHandle getShapeEntryProgram(bool drawModes, bool weight);
        bool shapeEntriesToDrawcalls(bgfx::ViewId id, const Transform *t, const Shape* shape, bool transparent,
                                     bool drawModes, float metadata, GeometryRenderPass pass,
                                     Plane **planes, bool perChunkDraw, bool worldDirty, bool cameraDirty,
                                     uint32_t depth, uint32_t stencil);

        // MARK: - QUAD ENTRY -
        bool ensureQuadEntry(Quad *quad, QuadEntry **entry, bool batched, bool pointsScaling);
        void releaseQuadEntry(uint32_t hash, QuadEntry *entry);
        void releaseQuadEntries();
        void checkAndPopInactiveQuadEntries();

        // MARK: - MESH ENTRY -
        bool ensureMeshEntry(Mesh *mesh, MeshEntry **entry);
        void releaseMeshEntry(uint32_t hash, MeshEntry *entry);
        void releaseMeshEntries();
        void checkAndPopInactiveMeshEntries();

        // MARK: - TEXTURE ENTRY -
        bool ensureTextureEntry(Texture *tex, uint16_t maxDim, TextureEntry **entry);
        void releaseTextureEntry(TextureEntry *entry);
        void releaseTextureEntries();
        void checkAndPopInactiveTextureEntries();

        // MARK: - UTILS -
        float packNormalizedFloatsToFloat(float f1, float f2);
        void setVec4(float *data, uint32_t idx, float x, float y, float z, float w);
        uint32_t lerpABGR32(uint32_t abgr1, uint32_t abgr2, float v);
        void getFrustumWorldPoints(float3 *points, float nearPlane, float farPlane, float projWidth, float projHeight, const float *invView, float fov, bool perspective, float3 *outCenter=nullptr);
        void getFrustumWorldPoints2(float3 *points, float ndcNear, float ndcFar, const float *invViewProj, float3 *outCenter=nullptr);
        void splitFrustum(float nearPlane, float farPlane, uint8_t count, float *out, float linearWeight=0.25f);
        void splitFrustumNDC(uint8_t count, float *out, float interval=0.002f);
        void splitFrustumNDC_Hardcoded(uint8_t count, float *out);
        void computeWorldPlanes(Transform *t, float3 pos, float3 viewOffset, float nearPlane, float farPlane, float fov_rad, bool perspective, float projWidth, float projHeight, Plane **out);
        float rgb2luma(float r, float g, float b);
        static bool isTex2DFormatSupported(const bgfx::Caps* caps, bgfx::TextureFormat::Enum format);

        // MARK: - VIEW TRANSFORM -
        void setViewTransformFromCamera(bgfx::ViewId id, Camera *camera, const float width, const float height,
                                        float *viewOut=nullptr, float *projOut=nullptr);
        void setViewTransform(bgfx::ViewId id, bx::Vec3 pos, bx::Vec3 lookAt,
                              const float width, const float height, float *viewOut=nullptr,
                              float *projOut=nullptr);
        static void setViewTransform(bgfx::ViewId id, const void *view, bool orthographic,
                                     const float width, const float height, bool NDCDepth,
                                     float *projOut=nullptr, bool cameraView=false,
                                     float nearPlane=0.0f, float farPlane=1.0f,
                                     float fov=CAMERA_DEFAULT_FOV);
        static void computeProjMtx(bool orthographic, float width, float height, bool NDCDepth, bool cameraView,
                                   float nearPlane, float farPlane, float fov, float *projOut);

        // MARK: - PRIMITIVES -
        void setFullScreenSpaceQuadTransient(bool flipV, const float *invViewProj=nullptr, const float3 *viewPos=nullptr);
        void setScreenSpaceQuadTransient(float x, float y, float width, float height, float fullWidth, float fullHeight,
                                         float uMax, float vMax, bool flipV, const float *invViewProj=nullptr, const float3 *viewPos=nullptr);
        void drawVoxels(bgfx::ViewId view, ShapeBufferEntry *entry, bgfx::ProgramHandle ph, uint64_t state, uint32_t depth);
        void drawVoxels(bgfx::ViewId view, bgfx::VertexBufferHandle vbh, const uint32_t from,
                        const uint32_t to, bgfx::ProgramHandle ph, uint64_t state, uint32_t depth);
        void drawVoxels(bgfx::ViewId view, bgfx::DynamicVertexBufferHandle vbh, const uint32_t from,
                        const uint32_t to, bgfx::ProgramHandle ph, uint64_t state, uint32_t depth);
        void drawStatic(bgfx::ViewId view, bgfx::VertexBufferHandle vbh, bgfx::IndexBufferHandle ibh,
                        bgfx::ProgramHandle ph, uint64_t state, uint32_t depth=0, const void *mtx=nullptr);
        void drawDynamic(bgfx::ViewId view, bgfx::DynamicVertexBufferHandle vbh, bgfx::DynamicIndexBufferHandle ibh,
                         bgfx::ProgramHandle ph, uint64_t state, uint32_t depth=0, const void *mtx=nullptr);
        void drawTransient(bgfx::ViewId view, bgfx::TransientVertexBuffer vbh, bgfx::TransientIndexBuffer ibh,
                           bgfx::ProgramHandle ph, uint64_t state, uint32_t depth=0, const void *mtx=nullptr);

        // MARK: - LOADING / ALLOC -
        // loadProgram, loadShader, loadMem are based on bgfx_utils.cpp in the examples
        bgfx::ProgramHandle loadProgram(bx::FileReaderI *_reader, const char *_vsName, const char *_fsName);
        bgfx::ShaderHandle loadShader(bx::FileReaderI *_reader, const char *_name);
        const bgfx::Memory* loadMem(bx::FileReaderI *_reader, const char *_filePath);
        static bimg::ImageContainer* parseImage(const void* data, uint32_t size, uint64_t samplerFlags, uint16_t maxWidth=0, uint16_t maxHeight=0, bgfx::TextureFormat::Enum *outFormat=nullptr);
        static bgfx::TextureHandle createTextureFromImage(bimg::ImageContainer* imageContainer, uint64_t flags,
                                                          bgfx::TextureInfo* info, bimg::Orientation::Enum* orientation,
                                                          uint16_t maxDim, uint16_t *outWidth, uint16_t *outHeight);
        bgfx::TextureHandle loadTexture(const void* data, size_t size,
                                        uint64_t flags= BGFX_TEXTURE_NONE | BGFX_SAMPLER_NONE,
                                        bgfx::TextureInfo* info= nullptr,
                                        bimg::Orientation::Enum* orientation=nullptr,
                                        uint16_t maxDim=UINT16_MAX,
                                        uint16_t *outWidth=nullptr, uint16_t *outHeight=nullptr);
        bgfx::TextureHandle loadTextureFromFile(const char* filePath,
                                                uint64_t flags= BGFX_TEXTURE_NONE | BGFX_SAMPLER_NONE,
                                                bgfx::TextureInfo* info=nullptr,
                                                bimg::Orientation::Enum* orientation=nullptr);
        // end of bgfx_utils

        /// Loads a RGBA8 cubemap from 6 files in app bundle
        /// @param size individual texture size, each texture is a square [size x size]
        /// @param flags bgfx texture flags
        /// @param f0 +X
        /// @param f1 -X
        /// @param f2 +Y
        /// @param f3 -Y
        /// @param f4 +Z
        /// @param f5 -Z
        bgfx::TextureHandle loadTextureCubeFromFiles(uint16_t size, uint64_t flags,
                const char* f0, const char* f1, const char* f2,
                const char* f3, const char* f4, const char* f5);
        void releaseTex2Ds(int num, ...);

        // MARK: - DEBUG -
        uint16_t _debug_textLine_left, _debug_textLine_right;
        int _debug_fps_ma[DEBUG_RENDERER_FPS_MOVING_AVERAGE], _debug_fps_ma_cursor;
#if DEBUG_RENDERER
        bool _debug_flip;
        bx::RngMwc _debug_rng;
        uint16_t _debug_lights, _debug_lights_drawn, _debug_cameras, _debug_shadows_pass,
            _debug_shapes_opaque_drawn, _debug_shapes_transparent_drawn, _debug_shapes_shadowmap_drawn, _debug_shapes_culled,
            _debug_quads_opaque_drawn, _debug_quads_transparent_drawn, _debug_quads_stencil_drawn, _debug_quads_shadowmap_drawn, _debug_quads_culled,
            _debug_meshes_opaque_drawn, _debug_meshes_transparent_drawn, _debug_meshes_shadowmap_drawn, _debug_meshes_culled;

        void debug_displayStats();
        uint32_t debug_countShapeVertices(Shape *value, bool transparent);
        void debug_displayGameInfo_recurse(Transform *t, uint32_t *shapesCount, uint32_t *chunks, uint32_t *blocks, uint32_t *shapesVertices, uint32_t *quadsCount, uint32_t *meshesCount, uint32_t *meshesVertices);
        void debug_displayGameInfo(const Game_SharedPtr& g);
        void debug_getShapeVBOccupancy(Shape *s, uint32_t *count, uint32_t *maxCount,
                                       uint32_t *mem, uint32_t *maxMem, uint32_t *vbCount,
                                       bool transparent);
        void debug_displayVBOccupancy(const Game_SharedPtr& g);
        void debug_displaySceneHierarchy(const Game_SharedPtr& g);
        void debug_displayRtree_recurse(Rtree *r, RtreeNode *rn, uint8_t level, const float tw=0, const float th=0, const bool boxes=false, const Matrix4x4 *mtx=nullptr, const int onlyLevel=-1);
        void debug_displayRtree(const Game_SharedPtr& g);
        void debug_displayRigidbodyStats(const Game_SharedPtr& g);
        void debug_displaySubRenderersStats();
#endif /* DEBUG_RENDERER */
        void debug_clearText();
        void debug_lua_displayFPS();
        void debug_lua_displayCollidersStats();
#endif /* P3S_CLIENT_HEADLESS */
    };
}

#pragma clang diagnostic pop
