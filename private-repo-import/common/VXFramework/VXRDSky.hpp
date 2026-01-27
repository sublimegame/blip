// -------------------------------------------------------------
//  Cubzh
//  VXRDSky.hpp
//  Created by Arthur Cormerais on October 18, 2020.
// -------------------------------------------------------------

#pragma once

#ifndef P3S_CLIENT_HEADLESS
// bgfx
#include <bgfx/bgfx.h>
#include "bx/math.h"
#endif

#include "float3.h"

//// SKY CONFIG
#define RENDERER_SKY_INSTANCES_MAX 16384
#define RENDERER_SKY_COMPUTE_INSTANCES_MAX 65536
#define RENDERER_SKY_EPSILON .05f
/// Make sure that CS can run through all instances (dispatch size * group size >= instances max)
#define RENDERER_DISPATCH_SIZE 512
/// Just for reference here, see COMPUTE_GROUP_SIZE in voxels_config.sh
#define RENDERER_COMPUTE_GROUP_SIZE 128

//// SKY DEBUG CONFIG
#if DEBUG
    #define DEBUG_RENDERER_SKY true
#else
    #define DEBUG_RENDERER_SKY false
#endif
#if DEBUG_RENDERER_SKY
    #define DEBUG_RENDERER_SKY_LOG false
    #define DEBUG_RENDERER_SKY_REALTIME_NO_CS false
#endif

namespace vx {
    struct RendererFeatures;

    namespace rendering {
        struct RangeValue {
            float min, max;
        };

#ifndef P3S_CLIENT_HEADLESS
        //// Values that should be restored on a partial reload
        struct SkyState {
            bool clouds;
            /// Clouds are lit (translucent) by default
            bool unlit;
            /// Dimensions of the clouds generation (origin.y: clouds altitude)
            float3 origin;
            float width, depth, spread;
            /// Distance from the edge of the map to where clouds disappear, from inner to outer edge
            uint16_t innerEdge, outerEdge;
            /// Clouds blocks base scale
            float baseScale;
            /// Clouds blocks max scale based on noise value
            float maxAddScale;
            /// Noise value threshold for cutout, can be animated
            RangeValue cutout;
            /// Noise frequency, higher value means more dense and smaller patterns
            float frequencyX, frequencyZ;
            /// Noise magnitude affects how big the areas with maximum value are, can be animated
            RangeValue magnitude;
            /// Animation period for range values
            float period;
            /// Scroll speed factor
            float speed;
            /// Base clouds color
            float3 color;
        };

        class Sky {
        public:
            Sky(RendererFeatures *features,
                bgfx::ProgramHandle *cloudsProgram,
                bgfx::ProgramHandle *cloudsProgram_unlit,
                bgfx::ProgramHandle *cs_updateCloudsProgram,
                bgfx::ProgramHandle *cs_indirectCloudsProgram);
            ~Sky();

            bool draw(bgfx::ViewId id, uint32_t depth, float offset);
            void reset(SkyState *state);
        private:
            /// Layout for sky vertex buffer
            struct SkyVertex {
                float _x, _y, _z;

                static void init() {
                    _skyVertexLayout
                            .begin()
                            .add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
                            .end();
                }

                static bgfx::VertexLayout _skyVertexLayout;
            };

            //// Pointer to sky state, used to restore rendering state on partial reload
            SkyState *_state;

            //// Instancing
            const uint16_t _instanceStride = 4 * 4 * sizeof(float);
            uint8_t *_instanceData;
            uint32_t _instancesCount;
            bgfx::UniformHandle _uStaticCloudsParams;
            float _uStaticCloudsParamsData[12];

            //// Compute shaders
            bgfx::DynamicVertexBufferHandle _cs_instanceBuffer;
            bgfx::DynamicIndexBufferHandle _cs_instanceCount;
            bgfx::DynamicVertexBufferHandle _cs_cloudsParams;
            float _cs_cloudsParamsData[24];

            //// Draw indirect
            bgfx::IndirectBufferHandle _indirectBuffer;

            //// Resources managed by GameRenderer, no need to release
            bgfx::ProgramHandle *_cloudsProgram, *_cloudsProgram_unlit, *_cs_updateCloudsProgram, *_cs_indirectCloudsProgram;

            bgfx::VertexBufferHandle _vbh;
            bgfx::IndexBufferHandle _ibh;
            RendererFeatures *_features;

            void setCubeData();
            void releaseCubeData();
            void setCompute();
            void releaseCompute();
            void setDrawIndirect();
            void releaseDrawIndirect();
            void generateClouds(bgfx::ViewId id, float offset);
            void releaseClouds();
            float snoise(float x, float y);
        };

#else /* P3S_CLIENT_HEADLESS */

        class Sky {
        public:
            Sky();
            ~Sky();
        };

#endif /* P3S_CLIENT_HEADLESS */
    }
}
