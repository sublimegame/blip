// -------------------------------------------------------------
//  Cubzh
//  VXRDDecals.hpp
//  Created by Arthur Cormerais on May 20, 2020.
// -------------------------------------------------------------

#pragma once

#include <unordered_map>

#ifndef P3S_CLIENT_HEADLESS

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Weverything"
// bgfx
#include <bgfx/bgfx.h>
#include "bx/math.h"
#pragma GCC diagnostic pop

#endif

#include "scene.h"

//// DECALS CONFIG
#define RENDERER_DECALS_OFFSET .005f

//// DECALS DEBUG CONFIG
#define DEBUG_RENDERER_DECALS true
#if DEBUG_RENDERER_DECALS
    #define DEBUG_RENDERER_DECALS_STATS true
#else
    #define DEBUG_RENDERER_DECALS_STATS false
#endif

namespace vx {
    struct RendererFeatures;
    struct QualityParameters;

    namespace rendering {
#ifndef P3S_CLIENT_HEADLESS

        class Decals {
        public:
            //// Projection enum is mapped to engine face constants so it can be conveniently cast
            /// eg. when placing a decal based on a raycast
            typedef enum {
                Downward = FACE_TOP,
                Upward = FACE_DOWN,
                Right = FACE_LEFT,
                Left = FACE_RIGHT,
                Forward = FACE_FRONT,
                Backward = FACE_BACK
            } Projection;

            //// Update mode dictates how the decals from a same request are applied
            /// Replace: removes the previous decal and keeps the latest requested
            /// Stack: accumulate decals up to a limit (not implemented)
            /// Merge: merge decals mesh (not implemented)
            typedef enum {
                Replace,
                Stack, // TODO!
                Merge // TODO!
            } UpdateMode;

            Decals(RendererFeatures *features, QualityParameters *quality, bgfx::IndexBufferHandle ibh,
                   bgfx::ProgramHandle *ph, bgfx::UniformHandle *fb1, bgfx::UniformHandle *params,
                   bgfx::UniformHandle *color1);
            ~Decals();
            bool request(uint32_t id, float3 eye, float width, float height, uint32_t stencil,
                         Transform *filterOut=nullptr, Projection proj=Projection::Downward,
                         UpdateMode mode=UpdateMode::Replace, bool recompute=false);
            void setColor(uint32_t id, float r, float g, float b, float a);
            void setTexture(uint32_t id, bgfx::TextureHandle tex, bool release);
            void setRange(uint32_t id, uint16_t range=0, bool fade=false, float sizeRatio=1.0f);
            void reset(Scene *scene);
            void removeAll();
            void remove(uint32_t id);
            bool draw(bgfx::ViewId id, uint32_t depth);
#if DEBUG_RENDERER_DECALS_STATS
            uint32_t _debug_getRequests();
            uint32_t _debug_getProjections();
            void _debug_reset();
#endif
        private:
            struct DecalEntry {
                float3 eye;             // world point at the center of projection
                float width, height;    // world units
                size_t count;           // quads count
                Projection proj;        // projection along an oriented axis
                UpdateMode mode;        // decal type
                uint16_t range;         // maximum projection range, 0 for the whole map
                bool fade;              // should decal fade out with distance
                float sizeRatio;        // how distance affects decal size, 1 for no effect
                Weakptr *filterOut;
                float rgba[4];
                bgfx::TextureHandle tex;
                bool releaseTex;
                bgfx::DynamicVertexBufferHandle dvb;
                float mtx[16];
                uint32_t stencil;
            };

            /// Each decal entry is mapped to a unique ID, eg. a Shape's ID
            std::unordered_map<uint32_t, DecalEntry> _decalEntries;

            //// Reference to current scene into which projections are made
            Weakptr *_scene;

            //// Resources managed by GameRenderer, no need to release
            bgfx::IndexBufferHandle _voxelIbh;
            bgfx::ProgramHandle *_program;
            RendererFeatures *_features;
            QualityParameters *_qualityParams;
            bgfx::UniformHandle *_sFb1;
            bgfx::UniformHandle *_uParams, *_uColor1;
            
#if DEBUG_RENDERER_DECALS_STATS
            uint32_t _debug_requests, _debug_projections;
#endif

            void release(DecalEntry*);
            bool draw(bgfx::ViewId id, uint32_t depth, DecalEntry*);
            bool tryGet(uint32_t id, DecalEntry **out);
            DecalEntry createDecalEntry(float3 eye, float width, float height, uint32_t stencil, Transform *filterOut, Projection proj, UpdateMode mode);
            void setProjection(DecalEntry *entry, float3 eye, float width, float height, Projection proj);
            bool projectionEquals(DecalEntry *entry, float3 eye, float width, float height, Projection proj);
            void computeProjection(DecalEntry *entry, size_t *count);
        };

#else /* P3S_CLIENT_HEADLESS */

        class Decals {
        public:
            Decals();
            ~Decals();
        };

#endif /* P3S_CLIENT_HEADLESS */
    }
}
