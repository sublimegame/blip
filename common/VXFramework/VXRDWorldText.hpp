//
//  VXRDWorldText.hpp
//  Cubzh
//
//  Created by Arthur Cormerais on --/--/2020.
//

#pragma once

// C++
#include <unordered_map>

#ifndef P3S_CLIENT_HEADLESS

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Weverything"
// bgfx
#include "BgfxText/font_manager.h"
#include "BgfxText/text_buffer_manager.h"
#include "bgfx/bgfx.h"
#include "bx/math.h"
#pragma GCC diagnostic pop

#endif

#include "matrix4x4.h"
#include "world_text.h"
#include "VXRDConfig.hpp"
#include "VXConfig.hpp"

//// WORLD TEXT CONFIG
#define WORLDTEXT_BG_QUAD_DELTA 0.01f
#define WORLDTEXT_FADE_OUT_THRESHOLD 0.95f

//// WORLD TEXT DEBUG CONFIG
#define DEBUG_RENDERER_WORLDTEXT true
#if DEBUG_RENDERER_WORLDTEXT
    #define DEBUG_RENDERER_WORLDTEXT_STATS true
#else
    #define DEBUG_RENDERER_WORLDTEXT_STATS false
#endif

namespace vx {
    struct RendererFeatures;
    struct QualityParameters;

    namespace rendering {
#ifndef P3S_CLIENT_HEADLESS

        class WorldTextRenderer {
        public:
            WorldTextRenderer(RendererFeatures *features, QualityParameters *quality,
                              bgfx::ProgramHandle *font_unlit, bgfx::ProgramHandle *font, bgfx::ProgramHandle *font_overlay_unlit, bgfx::ProgramHandle *font_overlay,
                              bgfx::ProgramHandle *font_weight,
                              bgfx::ProgramHandle *quad, bgfx::ProgramHandle *quad_overlay, bgfx::ProgramHandle *quad_weight,
                              bgfx::UniformHandle params, uint32_t sortOrder);
            ~WorldTextRenderer();
            void setViewProperties(float *view, float *proj, float projWidth, float projHeight, float screenPPP, float fov, bool projDirty, bool viewDirty);
            bool draw(bgfx::ViewId id, bgfx::ViewId overlayId, WorldText *wt, bool *overlayDrawn, GeometryRenderPass pass, VERTEX_LIGHT_STRUCT_T vlighting, bgfx::UniformHandle uh=BGFX_INVALID_HANDLE, uint32_t stencil=BGFX_STENCIL_DEFAULT);
            void endOfFrame();
            void removeAll();
            void getOrComputeWorldTextSize(WorldText *wt, float *outWidth, float *outHeight, bool points);
            float getCharacterSize(FontName font, const char *text, bool points);
            size_t snapWorldTextCursor(WorldText *wt, float *inoutX, float *inoutY, size_t *inCharIndex, bool points);
            bgfx::TextureHandle getAtlas();
#if DEBUG_RENDERER_WORLDTEXT_STATS
            uint32_t _debug_getScreenTexts();
            uint32_t _debug_getWorldTexts();
            uint32_t _debug_getTextRefreshes();
            uint32_t _debug_getCulled();
            void _debug_reset();
#endif
        private:
            /// Each world text entry is mapped to a unique ID
            struct WorldTextEntry {
                Weakptr *worldText; /* 8 bytes */
                float textX, textY; /* 2x4 bytes */
                float depth; /* 4 bytes */
                uint32_t sortOrder; /* 4 bytes */
                float bgX0, bgX1, bgY0, bgY1; /* 4x4 bytes */
                float tailX0, tailX1, tailY0, tailY1; /* 4x4 bytes */
                float fade; /* 4 bytes */
                float weight, outlineWeight; /* 2x4 bytes */
                float softness; /* 4 bytes */
                float lossyScale; /* 4 bytes */
                TextBufferHandle tbh; /* 2 bytes */
                bool frame, culled; /* 2x1 bytes */
            };
            std::unordered_map<uint32_t, WorldTextEntry> _textEntries;

            /// Resources managed by GameRenderer, no need to release
            bgfx::ProgramHandle *_fontProgram_unlit, *_fontProgram, *_fontProgram_overlay_unlit, *_fontProgram_overlay;
            bgfx::ProgramHandle *_fontProgram_weight;
            bgfx::ProgramHandle *_quadProgram, *_quadProgram_overlay, *_quadProgram_weight;
            RendererFeatures *_features;
            QualityParameters *_qualityParams;

            /// View properties
            float _projWidth, _projHeight;
            float _screenPPP;
            float _fov;
            bool _projDirty, _viewDirty;

            FontManager *_fontManager;
            TextBufferManager *_textManager;
            float *_view, *_proj;
            uint32_t _sortOrder;
            bgfx::UniformHandle _uNormal, _uParams;

#if DEBUG_RENDERER_WORLDTEXT_STATS
            uint32_t _debug_screens, _debug_worlds, _debug_refreshes, _debug_culled;
#endif

            WorldTextEntry* ensureWorldTextEntry(WorldText *wt, bool transient);
            WorldTextEntry createWorldTextEntry(bool transient);
            void reset(WorldTextEntry *);
            void clearText(WorldTextEntry *);
            bool isClipped(WorldTextEntry *);
            TrueTypeHandle loadTtf(const char* _filePath);
            bool tryGet(uint32_t id, WorldTextEntry **out);
            void release(WorldTextEntry*);
            void refreshTextGeometry(WorldTextEntry *entry, WorldText *wt, bool points);
            void drawQuad(bgfx::ViewId id, bgfx::ProgramHandle program, uint32_t sortOrder,
                          uint64_t state, float x0, float x1, float y0, float y1, float z,
                          const float3 *normal, uint32_t rgba, bool unlit, VERTEX_LIGHT_STRUCT_T vlighting);
        };

#else /* P3S_CLIENT_HEADLESS */

        class WorldTextRenderer {
        public:
            WorldTextRenderer();
            ~WorldTextRenderer();
        };

#endif /* P3S_CLIENT_HEADLESS */
    }
}
