// -------------------------------------------------------------
//  Cubzh
//  VXRDGif.hpp
//  Created by Arthur Cormerais on October 31, 2024.
// -------------------------------------------------------------

#pragma once

#ifndef P3S_CLIENT_HEADLESS

#include <unordered_map>

#include "VXRDConfig.hpp"
#include "weakptr.h"
#include "transform.h"
#include "doubly_linked_list.h"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Weverything"
#include "bgfx/bgfx.h"
#include "gif_load.h"

#pragma GCC diagnostic pop

namespace vx {
    namespace rendering {
        class GifRenderer {
        public:
            static bool isGif(const void *data, size_t size);

            void tick(const double dt);
            bool parse(uint32_t id, void *data, size_t size);
            bool bake(uint32_t id);
            void remove(uint32_t id);
            void removeAll();
            bool getTexture(uint32_t id, UVTexture *out);
        private:
            struct GifAtlas {
                uint32_t *data;
                uint32_t size;
                bgfx::TextureHandle th;
                uint16_t w, h;
                uint16_t nx, ny, cursorX, cursorY; // used while parsing
            };

            struct GifFrame {
                float u1, u2, v1, v2;
                float duration;
                uint8_t atlasIdx;
            };

            struct GifEntry {
                GifFrame *frames;
                GifAtlas *atlases;
                double timer;
                uint16_t cursor, nbFrames;
                uint8_t writeIdx, nbAtlases; // used while parsing
            };
            std::unordered_map<uint32_t, GifEntry> _gifEntries;

            static uint32_t gifLoadGetBGRA(const GIF_WHDR *whdr, uint8_t paletteIdx);
            static void gifLoadFrameFunc(void *userdata, GIF_WHDR *whdr);
            bool tryGet(uint32_t id, GifEntry **out);
            void release(GifEntry *entry);
        };
    }
}

#else

namespace vx {
    namespace rendering {
        class GifRenderer {
        public:
            GifRenderer();
            ~GifRenderer();
        };
    }
}

#endif
