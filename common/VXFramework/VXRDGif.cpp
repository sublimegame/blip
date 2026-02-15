// -------------------------------------------------------------
//  Cubzh
//  VXRDGif.cpp
//  Created by Arthur Cormerais on October 31, 2024.
// -------------------------------------------------------------

#include "VXRDGif.hpp"

#ifndef P3S_CLIENT_HEADLESS

#include "gif_load.h"
#include "vxlog.h"
#include "fifo_list.h"
#include "VXGameRenderer.hpp"

#define ATLAS_MAX_SIZE 2048

using namespace vx;
using namespace rendering;

bool GifRenderer::isGif(const void *data, size_t size) {
    if (size < 3) {
        return false;
    }
    return memcmp(static_cast<const uint8_t*>(data), "GIF", 3) == 0;
}

void GifRenderer::tick(const double dt) {
    std::unordered_map<uint32_t, GifEntry>::iterator itr;
    GifEntry *entry;
    for (itr = _gifEntries.begin(); itr != _gifEntries.end(); ++itr) {
        entry = &itr->second;

        // advance timer and cycle cursor
        entry->timer += dt;
        if (entry->timer >= entry->frames[entry->cursor].duration) {
            ++entry->cursor;
            if (entry->cursor >= entry->nbFrames) {
                entry->cursor = 0;
            }
            entry->timer = 0.0f;
        }
    }
}

bool GifRenderer::parse(uint32_t id, void *data, size_t size) {
    if (tryGet(id, nullptr)) {
        return false;
    }

    GifEntry entry = { nullptr, nullptr, 0.0, 0, 0, 0, 0 };

    const long read = GIF_Load(data, static_cast<long>(size), gifLoadFrameFunc, nullptr, &entry, 0L);
    if (read == 0) {
        vxlog_warning("Could not decode any frame from GIF file");
        return false;
    } else if (read < 0) {
        vxlog_warning("Incomplete GIF file, proceeding with %d frames", -read);
    } else if (entry.nbFrames == 0) {
        vxlog_warning("GIF file size not supported, discard");
        return false;
    }

    entry.cursor = 0; // was temporarily used while parsing

    _gifEntries[id] = entry;

    return true;
}

bool GifRenderer::bake(uint32_t id) {
    GifEntry *entry;
    if (tryGet(id, &entry)) {
        for (uint8_t i = 0; i < entry->nbAtlases; ++i) {
            if (bgfx::isValid(entry->atlases[i].th) == false && GameRenderer::hasTextureCapability(1)) {
                const bgfx::Memory *mem = bgfx::makeRef(entry->atlases[i].data, entry->atlases[i].size);
                entry->atlases[i].data = nullptr; // owned by mem

                entry->atlases[i].th = bgfx::createTexture2D(entry->atlases[i].w, entry->atlases[i].h,
                                                             false, 1, bgfx::TextureFormat::BGRA8,
                                                             BGFX_SAMPLER_UVW_CLAMP|BGFX_SAMPLER_MIN_POINT|BGFX_SAMPLER_MAG_POINT|BGFX_SAMPLER_MIP_POINT,
                                                             mem);
            }
            if (bgfx::isValid(entry->atlases[i].th) == false) {
                vxlog_warning("Could not create GIF entry atlas %d", i);
            }
        }
        return true;
    }
    return false;
}

void GifRenderer::remove(uint32_t id) {
    GifEntry *entry;
    if (tryGet(id, &entry)) {
        release(entry);
        _gifEntries.erase(id);
    }
}

void GifRenderer::removeAll() {
    std::unordered_map<uint32_t, GifEntry>::iterator itr;
    for (itr = _gifEntries.begin(); itr != _gifEntries.end(); ++itr) {
        release(&itr->second);
    }
    _gifEntries.clear();
}

bool GifRenderer::getTexture(uint32_t id, UVTexture *out) {
    GifEntry *entry;
    if (tryGet(id, &entry)) {
        // return current frame
        const GifFrame frame = entry->frames[entry->cursor];
        out->th = entry->atlases[frame.atlasIdx].th;
        out->u1 = frame.u1;
        out->u2 = frame.u2;
        out->v1 = frame.v1;
        out->v2 = frame.v2;

        return true;
    } else {
        out->th = BGFX_INVALID_HANDLE;
    }

    return false;
}

uint32_t GifRenderer::gifLoadGetBGRA(const GIF_WHDR *whdr, uint8_t paletteIdx) {
    if (paletteIdx == whdr->tran) {
        return 0;
    } else {
        return ((uint32_t)(whdr->cpal[paletteIdx].R << ((GIF_BIGE)? 8 : 16))
                | (uint32_t)(whdr->cpal[paletteIdx].G << ((GIF_BIGE)? 16 : 8))
                | (uint32_t)(whdr->cpal[paletteIdx].B << ((GIF_BIGE)? 24 : 0))
                | ((GIF_BIGE)? 0xFF : 0xFF000000));
    }
}

void GifRenderer::gifLoadFrameFunc(void *userdata, GIF_WHDR *whdr) {
    GifEntry *entry = static_cast<GifEntry*>(userdata);

    // early skip if size not supported
    if (whdr->xdim > ATLAS_MAX_SIZE || whdr->ydim > ATLAS_MAX_SIZE) {
        return;
    }

    // first frame, initialize atlas(es) and frames
    if (whdr->ifrm == 0) {
        // process the GIF as is, even if incomplete (negative whdr->nfrm)
        const long nfrm = abs(whdr->nfrm);
        entry->frames = static_cast<GifFrame*>(malloc(nfrm * sizeof(GifFrame)));
        entry->nbFrames = nfrm;

        // check if multiple atlases are needed
        const uint16_t nxPerAtlas = ATLAS_MAX_SIZE / whdr->xdim;
        const uint16_t nyPerAtlas = ATLAS_MAX_SIZE / whdr->ydim;
        entry->nbAtlases = ceilf(nfrm / static_cast<float>(nxPerAtlas * nyPerAtlas));
        entry->atlases = static_cast<GifAtlas*>(malloc(entry->nbAtlases * sizeof(GifAtlas)));

        // if multiple, first allocate the full atlas(es)
        for (uint8_t i = 0; i < entry->nbAtlases - 1; ++i) {
            const uint16_t w = nxPerAtlas * whdr->xdim;
            const uint16_t h = nyPerAtlas * whdr->ydim;
            const uint32_t size = w * h * sizeof(uint32_t);

            entry->atlases[i] = {
                static_cast<uint32_t*>(calloc(w * h, sizeof(uint32_t))), // initialized to 0
                size,
                BGFX_INVALID_HANDLE,
                w, h,
                nxPerAtlas, nyPerAtlas,
                0, 0
            };
        }

        // allocate smallest last atlas, or the single atlas
        const long remaining = nfrm - (entry->nbAtlases - 1) * nxPerAtlas * nyPerAtlas;
        const float aspect = whdr->xdim / static_cast<float>(whdr->ydim);
        const uint16_t ny = ceilf(sqrtf(remaining * aspect));
        const uint16_t nx = ceilf(remaining / static_cast<float>(ny));
        const uint16_t w = nx * whdr->xdim;
        const uint16_t h = ny * whdr->ydim;
        const uint32_t size = w * h * sizeof(uint32_t);
        entry->atlases[entry->nbAtlases - 1] = {
            static_cast<uint32_t*>(calloc(w * h, sizeof(uint32_t))), // initialized to 0
            size,
            BGFX_INVALID_HANDLE,
            w, h,
            nx, ny,
            0, 0
        };
    }

    vx_assert(entry->writeIdx < entry->nbAtlases);

    GifAtlas *atlas = &entry->atlases[entry->writeIdx];
    const uint32_t xdim = static_cast<uint32_t>(whdr->xdim);
    const uint32_t ydim = static_cast<uint32_t>(whdr->ydim);

    // frame origin within atlas
    const uint32_t atlasOrigin = atlas->w * ydim * atlas->cursorY + xdim * atlas->cursorX;

    // Gif frame offset support
    const uint32_t frameOffset = static_cast<uint32_t>(atlas->w * whdr->fryo + whdr->frxo);

    // Gif frame interlacing support
    if (whdr->intr) {
        // 4 interlace passes,
        // 1) every 8th row, starting at row 0
        // 2) every 8th row, starting at row 4
        // 3) every 4th row, starting at row 2
        // 4) every 2nd row, starting at row 1
        #define NUM_PASSES 4
        const uint32_t rowIncrements[NUM_PASSES] = { 8, 8, 4, 2 };
        const uint32_t startRows[NUM_PASSES] = { 0, 4, 2, 1 };

        uint32_t src = 0;
        for (uint8_t pass = 0; pass < NUM_PASSES; ++pass) {
            const uint32_t row_increment = rowIncrements[pass];
            const uint32_t start_row = startRows[pass];

            for (uint32_t y = start_row; y < whdr->fryd; y += row_increment) {
                for (uint32_t x = 0; x < whdr->frxd; ++x) {
                    const uint8_t paletteIdx = whdr->bptr[src];
                    if (paletteIdx != whdr->tran) {
                        atlas->data[atlasOrigin + atlas->w * y + x + frameOffset] = gifLoadGetBGRA(whdr, paletteIdx);
                    }
                    ++src;
                }
            }
        }
    } else {
        for (uint32_t x = 0; x < whdr->frxd; ++x) {
            for (uint32_t y = 0; y < whdr->fryd; ++y) {
                const uint8_t paletteIdx = whdr->bptr[whdr->frxd * y + x];
                if (paletteIdx != whdr->tran) {
                    atlas->data[atlasOrigin + atlas->w * y + x + frameOffset] = gifLoadGetBGRA(whdr, paletteIdx);
                }
            }
        }
    }

    // set parsed frame
    const float u1 = atlas->cursorX / static_cast<float>(atlas->nx);
    const float u2 = (atlas->cursorX + 1) / static_cast<float>(atlas->nx);
    const float v1 = atlas->cursorY / static_cast<float>(atlas->ny);
    const float v2 = (atlas->cursorY + 1) / static_cast<float>(atlas->ny);
    const float duration = maximum(0.02f, abs(whdr->time) * 0.01f);
    entry->frames[whdr->ifrm] = {
        u1, u2,
        v1, v2,
        duration,
        entry->writeIdx
    };

    // check and advance atlas cursors
    if (++atlas->cursorX == atlas->nx) {
        atlas->cursorX = 0;
        if (++atlas->cursorY == atlas->ny) {
            atlas->cursorY = 0;
            if(++entry->writeIdx == entry->nbAtlases) {
                vx_assert_d(whdr->ifrm == whdr->nfrm - 1);
                return; // last frame
            }
        }
    }

    // Gif blending mode for the NEXT frame
    switch(whdr->mode) {
        // restore last image which mode wasn't GIF_PREV
        case GIF_PREV: {
            if (whdr->ifrm == 0) {
                whdr->frxd = whdr->xdim;
                whdr->fryd = whdr->ydim;
                // first frame, go to GIF_BKGD from here
            } else {
                const GifFrame *prev = &entry->frames[entry->cursor];
                GifAtlas *prevAtlas = &entry->atlases[prev->atlasIdx];
                const uint32_t prevCursorX = static_cast<uint32_t>(prev->u1 * prevAtlas->nx); // could be stored in GifFrame if precision issues
                const uint32_t prevCursorY = static_cast<uint32_t>(prev->v1 * prevAtlas->ny);
                const uint32_t prevAtlasOrigin = prevAtlas->w * ydim * prevCursorY + xdim * prevCursorX;

                GifAtlas *nextAtlas = &entry->atlases[entry->writeIdx];
                const uint32_t nextOrigin = nextAtlas->w * ydim * nextAtlas->cursorY + xdim * nextAtlas->cursorX;
                const uint32_t pitch = xdim * sizeof(uint32_t);

                for (uint32_t y = 0; y < ydim; ++y) {
                    memcpy(nextAtlas->data + nextOrigin + nextAtlas->w * y, prevAtlas->data + prevAtlasOrigin + prevAtlas->w * y, pitch);
                }
                break;
            }
        }
        // restore the background color in the boundaries of the CURRENT frame
        case GIF_BKGD: {
            atlas = &entry->atlases[entry->writeIdx];
            const uint32_t nextOrigin = atlas->w * ydim * atlas->cursorY + xdim * atlas->cursorX;
            const uint32_t bgColor = gifLoadGetBGRA(whdr, whdr->tran >= 0 ? whdr->tran : whdr->bkgd);

            for (uint32_t x = 0; x < whdr->frxd; ++x) {
                for (uint32_t y = 0; y < whdr->fryd; ++y) {
                    atlas->data[nextOrigin + atlas->w * y + x + frameOffset] = bgColor;
                }
            }

            entry->cursor = whdr->ifrm + 1; // used by GIF_PREV
            break;
        }
        // keep the current frame
        case GIF_CURR: {
            GifAtlas *nextAtlas = &entry->atlases[entry->writeIdx];
            const uint32_t nextOrigin = nextAtlas->w * ydim * nextAtlas->cursorY + xdim * nextAtlas->cursorX;
            const uint32_t pitch = xdim * sizeof(uint32_t);

            for (uint32_t y = 0; y < ydim; ++y) {
                memcpy(nextAtlas->data + nextOrigin + nextAtlas->w * y, atlas->data + atlasOrigin + atlas->w * y, pitch);
            }

            entry->cursor = whdr->ifrm + 1; // used by GIF_PREV
            break;
        }
        case GIF_NONE:
        default:
            entry->cursor = whdr->ifrm + 1; // used by GIF_PREV
            break;
    }
}

bool GifRenderer::tryGet(uint32_t id, GifEntry **out) {
    std::unordered_map<uint32_t, GifEntry>::iterator itr = _gifEntries.find(id);
    const bool exists = itr != _gifEntries.end();
    if (exists && out != nullptr) {
        *out = &itr->second;
    }
    return exists;
}

void GifRenderer::release(GifEntry *entry) {
    free(entry->frames);
    for (uint8_t i = 0; i < entry->nbAtlases; ++i) {
        if (bgfx::isValid(entry->atlases[i].th)) {
            bgfx::destroy(entry->atlases[i].th);
        } else {
            free(entry->atlases[i].data);
        }
    }
    free(entry->atlases);
}

#endif
