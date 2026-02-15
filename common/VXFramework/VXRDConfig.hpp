// -------------------------------------------------------------
//  Cubzh
//  VXRDConfig.hpp
//  Created by Arthur Cormerais on september 5, 2023.
// -------------------------------------------------------------

#pragma once

#ifndef P3S_CLIENT_HEADLESS
#include "bgfx/bgfx.h"
#endif

/// MARK: - Global lights -

#define LIGHT_DEFAULT_INTENSITY 1.0f
#define BAKED_DEFAULT_INTENSITY 1.0f
#define LIGHT_DEFAULT_AMBIENT 0.2f
#define SKYBOX_DEFAULT_AMBIENT 0.1f
#define FACE_SHADING_MIN_FACTOR 0.1f

/// MARK: - Fog -

#define FOG_ENABLED true
#define FOG_EMISSION 0.4f
#define FOG_COLOR { 0.0f, 95.0f/255.0f, 139.0f/255.0f }

/// MARK: - Skybox -

#define SKYBOX_SKYLIGHT_COLOR { 1.0f, 1.0f, 234.0f/255.0f }
#define SKYBOX_SKY_COLOR { 0.0f, 174.0f/255.0f, 1.0f }
#define SKYBOX_HORIZON_COLOR { 153.0f/255.0f, 242.0f/255.0f, 1.0f }
#define SKYBOX_ABYSS_COLOR { 0.0f, 93.0f/255.0f, 127.0f/255.0f }
#define SKYBOX_ROTATION 0.008f

/// MARK: - Sky -
// See VXRDSky.hpp for details

#define SKY_CLOUDS_ENABLED true
#define SKY_CLOUDS_UNLIT false
#define SKY_CLOUDS_WIDTH 640 // world units
#define SKY_CLOUDS_DEPTH 640
#define SKY_CLOUDS_ALTITUDE 500
#define SKY_CLOUDS_SPREAD 5
#define SKY_CLOUDS_INNER_EDGE 15 // world units
#define SKY_CLOUDS_OUTER_EDGE 20 // world units
// Base and max additive scale allow to control the overall clouds compactness
// eg. : finer-grained clouds / beefier clouds / blocky clouds
#define SKY_CLOUDS_BASE_SCALE 0.05f         // 0.0 / 0.1 / 1.0
#define SKY_CLOUDS_MAX_ADD_SCALE 1.4f       // 1.0 / 2.0 / 0.0
// Cutout, frequencies, magnitude allow to control the overall clouds silhouette and distribution
// eg. : sparse big clouds / numerous small clouds
#define SKY_CLOUDS_CUTOUT { 0.6f, 0.56f }   // 0.0 / .5-.54
#define SKY_CLOUDS_FREQUENCY_X 0.08f        // .05 / .15
#define SKY_CLOUDS_FREQUENCY_Z 0.06f        // .04 / .14
#define SKY_CLOUDS_MAGNITUDE { 1.0f, 1.04f }// 1.0 / 1.0-1.04
#define SKY_CLOUDS_PERIOD 30.0f
#define SKY_CLOUDS_SPEED .4f
#define SKY_CLOUDS_BASE_COLOR { 1.0f, 1.0f, 1.0f }

/// MARK: - Shadow cookie -

#define SHADOW_COOKIE_SIZE_OFFSET 2.8f
#define SHADOW_COOKIE_ALPHA 0.22f
#define SHADOW_COOKIE_RANGE 10
#define SHADOW_COOKIE_FADE true
#define SHADOW_COOKIE_DIST_SIZE_RATIO 2.0f

/// MARK: - Draw modes overrides -

#define DRAWMODE_ALPHA_OVERRIDE .6f
#define DRAWMODE_HIGHLIGHT_SPEED 3.0f
#define DRAWMODE_HIGHLIGHT_R .4f
#define DRAWMODE_HIGHLIGHT_G .4f
#define DRAWMODE_HIGHLIGHT_B .8f
#define DRAWMODE_GREY .3f
#define DRAWMODE_GRID_R 0.0f;
#define DRAWMODE_GRID_G 0.0f;
#define DRAWMODE_GRID_B 0.0f;

/// MARK: - Textures -

#define QUAD_TEX_MAX_SIZE 2048
#define MESH_TEX_MAX_SIZE 2048
#define SKYBOX_TEX_MAX_SIZE 4096
#define SKYBOX_MAX_CUBEMAPS 2

/// MARK: - Misc. -

#define CAMERA_DEFAULT_FOV_MOBILE_RATE .83f
#define TRANSPARENT_SORT_ORDER_DELTA 0.01f

/// MARK: - Types -

namespace vx {
    namespace rendering {
        typedef enum QualityTier {
            QualityTier_Compatibility,
            QualityTier_Low,
            QualityTier_Medium,
            QualityTier_High,
            QualityTier_Ultra,
            QualityTier_Custom // unused for now
        } QualityTier;

        typedef enum GeometryRenderPass {
            GeometryRenderPass_Opaque,
            GeometryRenderPass_TransparentWeight,
            GeometryRenderPass_TransparentFallback,
            GeometryRenderPass_TransparentPost,
            GeometryRenderPass_Shadowmap,
            GeometryRenderPass_StencilWrite
        } GeometryRenderPass;

        typedef enum RenderPassState {
            RenderPassState_Opacity,
            RenderPassState_AlphaBlending,
            RenderPassState_AlphaBlending_Premultiplied,
            RenderPassState_Additive,
            RenderPassState_Multiplicative,
            RenderPassState_Subtractive,
            RenderPassState_AlphaWeight,
            RenderPassState_Blit,
            RenderPassState_Layered,
            RenderPassState_DepthPacking,
            RenderPassState_DepthSampling,
            RenderPassState_DepthTesting
        } RenderPassState;

        typedef enum AntiAliasing {
            AntiAliasing_None,
            AntiAliasing_FXAA
        } AntiAliasing;

        typedef enum TriangleCull {
            TriangleCull_None,
            TriangleCull_Front,
            TriangleCull_Back
        } TriangleCull;

        typedef enum MediaType {
            MediaType_None,
            MediaType_Image,
            MediaType_Greyscale,
            MediaType_Gif
        } MediaType;

        typedef enum {
            SkyboxMode_Clear,
            SkyboxMode_Linear,
            SkyboxMode_Cubemap,
            SkyboxMode_CubemapLerp
        } SkyboxMode;

#ifndef P3S_CLIENT_HEADLESS

        typedef struct UVTexture {
            float u1, u2, v1, v2;
            bgfx::TextureHandle th;
        } UVTexture;
        const static UVTexture texture_none = { 0.0f, 1.0f, 0.0f, 1.0f, BGFX_INVALID_HANDLE };

#endif
    }
}

/// MARK: - Vertex Layouts -

#ifndef P3S_CLIENT_HEADLESS

namespace vx {
    namespace rendering {
        struct ShapeVertex {
            float x, y, z, colorIdx;
            float metadata;

            static void init() {
                _shapeVertexLayout
                    .begin()
                    .add(bgfx::Attrib::Position, 4, bgfx::AttribType::Float)
                    .add(bgfx::Attrib::TexCoord0, 1, bgfx::AttribType::Float)
                    .end();
            }

            static bgfx::VertexLayout _shapeVertexLayout;
        };

        struct ScreenQuadVertex {
            float _x, _y, _u, _v;
            float _farX, _farY, _farZ;

            static void init() {
                _screenquadVertexLayout
                    .begin()
                    .add(bgfx::Attrib::Position,  4, bgfx::AttribType::Float)
                    .add(bgfx::Attrib::TexCoord0, 3, bgfx::AttribType::Float)
                    .end();
            }
            static bgfx::VertexLayout _screenquadVertexLayout;
        };

        struct QuadVertex {
            float x, y, z, metadata;
            float nx, ny, nz;
            uint32_t rgba;
            float u, v;

            static void init() {
                _quadVertexLayout
                    .begin()
                    .add(bgfx::Attrib::Position, 4, bgfx::AttribType::Float)
                    .add(bgfx::Attrib::Normal, 3, bgfx::AttribType::Float) // TODO: Uint8, but is used to pack other parameters sometimes
                    .add(bgfx::Attrib::Color0, 4, bgfx::AttribType::Uint8, true)
                    .add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float) // UV in any range for tiling
                    .end();
            }

            static bgfx::VertexLayout _quadVertexLayout;
        };

        struct MeshVertex {
            float x, y, z;
            uint8_t nx, ny, nz;
            uint8_t tx, ty, tz;
            uint32_t rgba;
            int16_t u, v;

            static void init() {
                _meshVertexLayout
                    .begin()
                    .add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
                    .add(bgfx::Attrib::Normal, 3, bgfx::AttribType::Uint8, true)
                    .add(bgfx::Attrib::Tangent, 3, bgfx::AttribType::Uint8, true)
                    .add(bgfx::Attrib::Color0, 4, bgfx::AttribType::Uint8, true)
                    .add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Int16, true)
                    .end();
            }

            static bgfx::VertexLayout _meshVertexLayout;
        };
    }
}
#endif