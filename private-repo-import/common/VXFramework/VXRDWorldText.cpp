//
//  VXRDWorldText.cpp
//  Cubzh
//
//  Created by Arthur Cormerais on --/--/2020.
//

#include "VXRDWorldText.hpp"

using namespace vx;
using namespace rendering;

#ifndef P3S_CLIENT_HEADLESS

#include "filesystem.hpp"
#include "VXGameRenderer.hpp"
#include "VXFont.hpp"

#define ID_FIRST 1
static uint32_t _nextID = ID_FIRST;

WorldTextRenderer::WorldTextRenderer(RendererFeatures *features, QualityParameters *quality,
                                     bgfx::ProgramHandle *font_unlit, bgfx::ProgramHandle *font, bgfx::ProgramHandle *font_overlay_unlit, bgfx::ProgramHandle *font_overlay,
                                     bgfx::ProgramHandle *font_weight,
                                     bgfx::ProgramHandle *quad, bgfx::ProgramHandle *quad_overlay, bgfx::ProgramHandle *quad_weight,
                                     bgfx::UniformHandle params, uint32_t sortOrder) {

    _features = features;
    _qualityParams = quality;
    _fontProgram_unlit = font_unlit;
    _fontProgram = font;
    _fontProgram_overlay_unlit = font_overlay_unlit;
    _fontProgram_overlay = font_overlay;
    _fontProgram_weight = font_weight;
    _quadProgram = quad;
    _quadProgram_overlay = quad_overlay;
    _quadProgram_weight = quad_weight;
    _projWidth = 0.0f;
    _projHeight = 0.0f;
    _screenPPP = 1.0f;
    _fov = 0.0f;
    _view = nullptr;
    _proj = nullptr;
    _projDirty = true;
    _viewDirty = true;
    _sortOrder = sortOrder;
    _uNormal = bgfx::createUniform("u_normal", bgfx::UniformType::Vec4);
    _uParams = params;

    _fontManager = Font::shared()->getWorldFontManager();
    _textManager = new TextBufferManager(_fontManager);

#if DEBUG_RENDERER_WORLDTEXT_STATS
    _debug_screens = 0;
    _debug_worlds = 0;
    _debug_refreshes = 0;
    _debug_culled = 0;
#endif
}

WorldTextRenderer::~WorldTextRenderer() {
    bgfx::destroy(_uNormal);

    removeAll();
    
    delete _textManager;
}

void WorldTextRenderer::setViewProperties(float *view, float *proj, float projWidth, float projHeight,
                                          float screenPPP, float fov, bool projDirty, bool viewDirty) {
    
    _view = view;
    _proj = proj;
    _projWidth = projWidth;
    _projHeight = projHeight;
    _fov = fov;
    _screenPPP = screenPPP; // for texts position & scissors in Screen mode
    _projDirty = projDirty;
    _viewDirty = viewDirty;
}

bool WorldTextRenderer::draw(bgfx::ViewId id, bgfx::ViewId overlayId, WorldText *wt, bool *overlayDrawn,
                             GeometryRenderPass pass, VERTEX_LIGHT_STRUCT_T vlighting,
                             bgfx::UniformHandle uh, uint32_t stencil) {
#if DEBUG_RENDERER_WORLDTEXT_STATS
    #define INC_SCREEN _debug_screens++;
    #define INC_WORLD _debug_worlds++;
    #define INC_REFRESH _debug_refreshes++;
    #define INC_CULLED _debug_culled++;
#else
    #define INC_SCREEN
    #define INC_WORLD
    #define INC_REFRESH
    #define INC_CULLED
#endif

    *overlayDrawn = false;

    if (world_text_is_empty(wt)) {
        INC_CULLED
        return false;
    }

    WorldTextEntry *entry = ensureWorldTextEntry(wt, true);

    if (isValid(entry->tbh) == false) {
        INC_CULLED
        return false;
    }

    Transform *t = world_text_get_transform(wt);
    const TextType type = world_text_get_type(wt);
    const bool pointsScaling = type == TextType_Screen || _fov <= 0.0f;
    const uint8_t sortOrder = world_text_get_sort_order(wt);

    // first time this frame
    if (entry->frame == false) {
        // world space to view space
        const float3 *pos = transform_get_position(t, false);
        float world[4] = {pos->x, pos->y, pos->z, 1.0f};
        float view[4];
        bx::vec4MulMtx(view, world, _view);

        // cull beyond max distance
        float depth = view[2];
        if (depth > world_text_get_max_distance(wt)) {
            entry->culled = true;
            INC_CULLED
            return false;
        }

        // distance factor
        const float max = world_text_get_max_distance(wt);
        const float prevd = CLAMP01F(entry->depth / max);
        const float d = CLAMP01F(depth / max);

        // update fade & sort order if distance changed
        if (float_isEqual(prevd, d, EPSILON_ZERO) == false) {
            entry->depth = depth;
            entry->fade = 1.0f - bx::clamp((d - WORLDTEXT_FADE_OUT_THRESHOLD) / (1.0f - WORLDTEXT_FADE_OUT_THRESHOLD), 0.0f, 1.0f);

            // fade may change text color
            world_text_set_color_dirty(wt, true);

            if (sortOrder > 0) {
                entry->sortOrder = _sortOrder - sortOrder;
            } else {
                // sort order based on view space depth delta
                entry->sortOrder = _sortOrder + static_cast<uint32_t>(depth / TRANSPARENT_SORT_ORDER_DELTA);
            }
        }

        // transparency depends on alpha & fade
        uint32_t color = world_text_get_color(wt);
        const uint8_t alpha = uint8_t(color >> 24);

        if (alpha == 0 || float_isZero(entry->fade, EPSILON_ZERO)) {
            entry->culled = true;
            INC_CULLED
            return false;
        }

        if (type == TextType_Screen) {
            // view space to clip space
            float clip[4];
            bx::vec4MulMtx(clip, view, _proj);

            // perspective division
            const float normX = clip[0] / clip[3];
            const float normY = clip[1] / clip[3];
            const float normZ = clip[2] / clip[3];

            // cull out-of-frustum
            if (abs(normX) > 1 || abs(normY) > 1 || abs(normZ) > 1) {
                entry->culled = true;
                INC_CULLED
                return false;
            }

            // clip space to screen space
            entry->textX = _projWidth * (normX + 1) / 2;
            entry->textY = _projHeight * (1 - (normY + 1) / 2);
        } else {
            entry->textX = 0.0f;
            entry->textY = 0.0f;
        }

        // skip drawing clipped entries
        if (isClipped(entry)) {
            entry->culled = true;
            INC_CULLED
            return false;
        }

        // refresh text color, before potential geometry refresh
        if (world_text_is_color_dirty(wt)) {
            if (entry->fade < 1.0f) {
                color = color - uint32_t(alpha << 24) + uint32_t(uint8_t(entry->fade * alpha) << 24);
            }
            _textManager->setTextColor(entry->tbh, color);
        }

        // refresh text geometry
        if (world_text_is_geometry_dirty(wt)) {
            refreshTextGeometry(entry, wt, pointsScaling);

            world_text_set_geometry_dirty(wt, false);
            world_text_set_alignment_dirty(wt, true);
            world_text_set_slant_dirty(wt, true);
            INC_REFRESH
        } else if (world_text_is_color_dirty(wt)) {
            // only color changed, update vertices
            _textManager->applyColor(entry->tbh);
        }
        world_text_set_color_dirty(wt, false);

        // refresh text formatting, after potential geometry refresh
        if (world_text_is_alignment_dirty(wt)) {
            _textManager->setAlignment(entry->tbh, world_text_get_alignment(wt));
            world_text_set_alignment_dirty(wt, false);
        }
        if (world_text_is_slant_dirty(wt)) {
            const float slantOffset = _textManager->setSlant(entry->tbh, world_text_get_slant(wt));
            world_text_set_slant_dirty(wt, false);

            world_text_set_cached_size(wt, world_text_get_width(wt) + slantOffset, world_text_get_height(wt), pointsScaling);
        }

        // skip drawing empty entries
        if (_textManager->getVertexCount(entry->tbh) == 0) {
            entry->culled = true;
            INC_CULLED
            return false;
        }

        entry->frame = true;
    }
    // already culled this frame
    else if (entry->culled) {
        INC_CULLED
        return false;
    }

    const FontName fontName = static_cast<FontName>(world_text_get_font_index(wt));
    const bool fontAlphaBlended = s_fontAlphaBlended[fontName];

    // final text color
    const uint32_t textColor = _textManager->getTextColor(entry->tbh);
    const uint8_t textAlpha = uint8_t(textColor >> 24);

    // final background color
    uint32_t bgColor = world_text_get_background_color(wt);
    uint8_t bgColorA = uint8_t(bgColor >> 24);
    if (entry->fade < 1.0f) {
        const uint8_t faded = uint8_t(entry->fade * bgColorA);
        bgColor = bgColor - uint32_t(bgColorA << 24) + uint32_t(faded << 24);
        bgColorA = faded;
    }

    const bool skipText = textAlpha == 0;
    const bool skipBg = bgColorA == 0;
    if (skipText && skipBg) {
        return false;
    }
    const bool isTextTransparent = textAlpha < 255;
    const bool isBgTransparent = bgColorA < 255;

    // select programs based on pass
    bgfx::ProgramHandle fontProgram = BGFX_INVALID_HANDLE, quadProgram = BGFX_INVALID_HANDLE;
    uint64_t state;
    bgfx::ViewId viewId;
    if (type == TextType_Screen) {
        if (pass == GeometryRenderPass_Opaque) {
            if (skipText == false) {
                fontProgram = *_fontProgram_overlay_unlit;
            }
            if (skipBg == false) {
                quadProgram = *_quadProgram_overlay;
            }
            state = GameRenderer::getRenderState(0, RenderPassState_AlphaBlending_Premultiplied,
                                                 world_text_is_doublesided(wt) ? TriangleCull_None : TriangleCull_Back,
                                                 PrimitiveType_Triangles, true);
            viewId = overlayId;
        } else {
            return false;
        }
    } else {
        if (pass == GeometryRenderPass_TransparentWeight) {
            if (skipText == false && isTextTransparent && fontAlphaBlended == false) {
                fontProgram = *_fontProgram_weight;
            }
            if (skipBg == false && isBgTransparent) {
                quadProgram = *_quadProgram_weight;
            }
            state = GameRenderer::getRenderState(0, RenderPassState_AlphaWeight, world_text_is_doublesided(wt) ? TriangleCull_None : TriangleCull_Back,
                                                 PrimitiveType_Triangles, sortOrder > 0);
            viewId = id;
        } else if (pass == GeometryRenderPass_TransparentFallback) {
            if (skipText == false && isTextTransparent && fontAlphaBlended == false) {
                fontProgram = world_text_is_unlit(wt) ? *_fontProgram_unlit : *_fontProgram;
            }
            if (skipBg == false && isBgTransparent) {
                quadProgram = *_quadProgram;
            }
            state = GameRenderer::getRenderState(0, RenderPassState_AlphaBlending, world_text_is_doublesided(wt) ? TriangleCull_None : TriangleCull_Back,
                                                 PrimitiveType_Triangles, sortOrder > 0);
            viewId = id;
        } else if (pass == GeometryRenderPass_Opaque) {
            if (skipText == false && isTextTransparent == false && fontAlphaBlended == false) {
                fontProgram = world_text_is_unlit(wt) ? *_fontProgram_unlit : *_fontProgram;
            }
            if (skipBg == false && isBgTransparent == false) {
                quadProgram = *_quadProgram;
            }
            state = GameRenderer::getRenderState(0, RenderPassState_Opacity, world_text_is_doublesided(wt) ? TriangleCull_None : TriangleCull_Back,
                                                 PrimitiveType_Triangles, sortOrder > 0);
            viewId = id;
        } else if (pass == GeometryRenderPass_TransparentPost) {
            if (skipText == false && fontAlphaBlended) {
                fontProgram = world_text_is_unlit(wt) ? *_fontProgram_overlay_unlit : *_fontProgram_overlay;
                state = GameRenderer::getRenderState(0, RenderPassState_AlphaBlending, world_text_is_doublesided(wt) ? TriangleCull_None : TriangleCull_Back,
                                                     PrimitiveType_Triangles, sortOrder > 0);
                viewId = id;
            }
        } else {
            return false;
        }
    }

    const bool drawText = bgfx::isValid(fontProgram);
    const bool drawBg = bgfx::isValid(quadProgram);
    if (drawText == false && drawBg == false) {
        return false;
    }

    // normal for lighting pass
    const Matrix4x4 *ltw = transform_get_ltw(t);
    float3 normal = { -ltw->x3y1, -ltw->x3y2, -ltw->x3y3 };
    float3_normalize(&normal);

    // apply screen pos via model mtx
    float model[16];
    bx::mtxTranslate(model, entry->textX, entry->textY, 0.0f);

    // flip Y and/or apply points scaling
    const bool flip = type == TextType_World; // Note: text is currently setup in screen first, which uses top-left origin
    const bool swapScaling = world_text_is_cached_size_points(wt) != pointsScaling;
    if (flip || swapScaling) {
        float3 scale;
        if (swapScaling) {
            const float pointsScale = pointsScaling ? 1.0f / _screenPPP : _screenPPP;
            scale = { pointsScale, pointsScale, pointsScale };
        } else {
            scale = { 1.0f, 1.0f, 1.0f };
        }
        if (flip) {
            scale.y *= -1;
        }

        float tmp[16], scaled[16];
        bx::mtxScale(tmp, scale.x, scale.y, scale.z);
        bx::mtxMul(scaled, tmp, reinterpret_cast<const float *>(ltw));

        memcpy(tmp, model, sizeof(float) * 16);
        bx::mtxMul(model, tmp, scaled);
    }

    uint32_t modelCache;
    if (drawText) {
        if (_projDirty || transform_is_any_dirty(t) || _viewDirty && pointsScaling == false) {
            entry->softness = -1.0f;
        }
        if (transform_is_any_dirty(t)) {
            float3 lossyScale; transform_get_lossy_scale(t, &lossyScale, false);
            entry->lossyScale = lossyScale.y;
            transform_reset_any_dirty(t);
        }
        if (entry->softness < 0.0f) {
            float ssScale = world_text_get_font_size(wt) * entry->lossyScale;
            if (pointsScaling) {
                ssScale /= _projHeight;
            } else {
                ssScale *= _screenPPP / (2.0f * entry->depth * tanf(_fov * 0.5f));
            }
            entry->softness = maximum(s_fontSDFSoftnessMax[fontName] * powf(1.0f - CLAMP01F(ssScale), 13.0f), s_fontSDFSoftnessMin[fontName]);
        }

        // Metadata packing,
        // - weight (8 bits, normalized float)
        // - softness (8 bits, normalized float)
        // - outline weight (8 bits, normalized float)
        const uint32_t metadata1 = uint8_t(entry->weight * 255) +
                                   uint8_t(entry->softness * 255) * 256 +
                                   uint8_t(entry->outlineWeight * 255) * 65536;

        // - vertex lighting SRGB (4 bits each)
        const uint32_t metadata2 = vlighting.ambient + vlighting.red * 16 +
                                   vlighting.green * 256 + vlighting.blue * 4096;

        const float paramsData[4] = {
            static_cast<float>(metadata1),
            static_cast<float>(metadata2),
            static_cast<float>(world_text_drawmode_get_outline_color(wt)),
            0.0f
        };
        bgfx::setUniform(_uParams, paramsData);

        if (_features->useDeferred()) {
            const float normalData[4] = { normal.x, normal.y, normal.z, 0.0f };
            bgfx::setUniform(_uNormal, normalData);
        }

        // draw text quads
        if (type == TextType_Screen) {
            const TextRectangle *rect = _textManager->getRectangle(entry->tbh);
            bgfx::setScissor(CLAMP((entry->textX + rect->x0), 0, _projWidth) * _screenPPP,
                             CLAMP((entry->textY + rect->y0), 0, _projHeight) * _screenPPP,
                             (rect->x1 - rect->x0) * _screenPPP,
                             (rect->y1 - rect->y0) * _screenPPP);
            INC_SCREEN
        } else {
            INC_WORLD
        }
        modelCache = bgfx::setTransform(model);
        bgfx::setStencil(stencil);
        _textManager->submitTextBuffer(entry->tbh, viewId, fontProgram, entry->sortOrder, state);
    }

    if (drawBg) {
        // draw background quad
        if (type == TextType_Screen) {
            bgfx::setScissor(CLAMP((entry->textX + entry->bgX0), 0, _projWidth) * _screenPPP,
                             CLAMP((entry->textY + entry->bgY0), 0, _projHeight) * _screenPPP,
                             (entry->bgX1 - entry->bgX0) * _screenPPP,
                             (entry->bgY1 - entry->bgY0) * _screenPPP);
        }
        if (drawText) {
            bgfx::setTransform(modelCache);
        } else {
            modelCache = bgfx::setTransform(model);
        }
        bgfx::setStencil(stencil);
        drawQuad(viewId, quadProgram, entry->sortOrder + 1, state, entry->bgX1, entry->bgX0,
                 entry->bgY0, entry->bgY1, WORLDTEXT_BG_QUAD_DELTA, &normal, bgColor,
                 world_text_is_unlit(wt), vlighting);

        // optionally draw a tail under the background quad
        if (world_text_get_tail(wt)) {
            if (type == TextType_Screen) {
                bgfx::setScissor(CLAMP((entry->textX + entry->tailX0), 0, _projWidth) * _screenPPP,
                                 CLAMP((entry->textY + entry->tailY0), 0, _projWidth) * _screenPPP,
                                 (entry->tailX1 - entry->tailX0) * _screenPPP,
                                 (entry->tailY1 - entry->tailY0) * _screenPPP);
            }

            bgfx::setTransform(modelCache);
            bgfx::setStencil(stencil);
            drawQuad(viewId, quadProgram, entry->sortOrder + 1, state, entry->tailX1, entry->tailX0,
                     entry->tailY0, entry->tailY1, WORLDTEXT_BG_QUAD_DELTA, &normal, bgColor,
                     world_text_is_unlit(wt), vlighting);
        }
    }

    *overlayDrawn = viewId == overlayId;
    return viewId == id; // only returns whether drawn to current view, not overlay view
}

void WorldTextRenderer::endOfFrame() {
    std::unordered_map<uint32_t, WorldTextEntry>::iterator itr;
    WorldText *wt;
    Transform *t;
    for (itr = _textEntries.begin(); itr != _textEntries.end(); ) {
        wt = static_cast<WorldText*>(weakptr_get(itr->second.worldText));

        // remove obsolete entries
        if (wt == nullptr || isValid(itr->second.tbh) == false) {
            release(&itr->second);
            itr = _textEntries.erase(itr);
            continue;
        }

        t = world_text_get_transform(wt);

        // remove outside of scene, or empty entries
        if (transform_is_removed_from_scene(t) || world_text_is_empty(wt)) {
            release(&itr->second);
            itr = _textEntries.erase(itr);
            continue;
        }

        // reset frame/culling flag
        itr->second.frame = false;
        itr->second.culled = false;

        itr++;
    }
}

void WorldTextRenderer::removeAll() {
    std::unordered_map<uint32_t, WorldTextEntry>::iterator itr;
    for (itr = _textEntries.begin(); itr != _textEntries.end(); ++itr) {
        release(&itr->second);
    }
    _textEntries.clear();
    _nextID = ID_FIRST;
}

void WorldTextRenderer::getOrComputeWorldTextSize(WorldText *wt, float *outWidth, float *outHeight, bool points) {
    WorldTextEntry *entry = ensureWorldTextEntry(wt, true);
    if (isValid(entry->tbh) == false) {
        return;
    }

    if (world_text_is_geometry_dirty(wt)) {
        refreshTextGeometry(entry, wt, points);

        world_text_set_geometry_dirty(wt, false);
        INC_REFRESH
    }

    *outWidth = world_text_get_width(wt);
    *outHeight = world_text_get_height(wt);

    if (world_text_is_cached_size_points(wt) != points) {
        const float scale = points ? 1.0f / vx::Screen::nbPixelsInOnePoint : vx::Screen::nbPixelsInOnePoint;
        *outWidth *= scale;
        *outHeight *= scale;
    }
}

float WorldTextRenderer::getCharacterSize(FontName font, const char *text, bool points) {
    const float scale = points ? 1.0f / vx::Screen::nbPixelsInOnePoint : 1.0f;
    const float size = _textManager->getCharacterSize(Font::shared()->getFont(font), text);
    return size * scale;
}

/// This function assumes wt width & height are up-to-date in cache
size_t WorldTextRenderer::snapWorldTextCursor(WorldText *wt, float *inoutX, float *inoutY, size_t *inCharIndex, bool points) {
    const FontName fontName = static_cast<FontName>(world_text_get_font_index(wt));

    const float scale = points ? 1.0f / vx::Screen::nbPixelsInOnePoint : 1.0f;
    const float fontScale = world_text_get_font_size(wt) / s_fontNativeSize[fontName] * scale;
    const float padding2 = world_text_get_padding(wt) * 2;
    const float maxTextWidth = world_text_get_max_width(wt) - padding2;
    const float cachedWidth = world_text_get_width(wt);
    const float cachedHeight = world_text_get_height(wt);

    // glyph spacing from weight
    const float weight = points ? 0.5f + (world_text_get_weight(wt) - 0.5f) / Screen::nbPixelsInOnePoint : world_text_get_weight(wt);
    const float outlineWeight = world_text_drawmode_get_outline_weight(wt) * scale;
    const float spacing = ((weight - 0.5f) * FONT_SDF_WEIGHT_SPACING + outlineWeight * FONT_SDF_OUTLINE_WEIGHT_SPACING) * s_fontSDFRadius[fontName] * fontScale;

    // Offset from anchor
    const float hOffset = cachedWidth * world_text_get_anchor_x(wt);
    const float vOffset = cachedHeight * world_text_get_anchor_y(wt);
    *inoutX += hOffset;
    *inoutY += vOffset;

    // Flip Y: local origin is bottom-left, text origin is top-left
    const float textOrigin = cachedHeight - world_text_get_padding(wt);
    *inoutY = textOrigin - *inoutY;

    const FontHandle fonts[] = {
        Font::shared()->getFont(fontName),
        Font::shared()->getFont(FontName_Emoji),
        Font::shared()->getFont(FontName_NotoJP)
    };
    const size_t characters = _textManager->snapTextCursor(fonts, 3,
                                                           world_text_get_text(wt),
                                                           nullptr,
                                                           fontScale,
                                                           spacing,
                                                           maxTextWidth,
                                                           inoutX,
                                                           inoutY,
                                                           inCharIndex);

    // back in local space
    *inoutY = textOrigin - *inoutY;
    *inoutX -= hOffset;
    *inoutY -= vOffset;

    return characters;
}

bgfx::TextureHandle WorldTextRenderer::getAtlas() {
    return _fontManager->getAtlasTexture();
}

#if DEBUG_RENDERER_WORLDTEXT_STATS
uint32_t WorldTextRenderer::_debug_getScreenTexts() {
    return _debug_screens;
}

uint32_t WorldTextRenderer::_debug_getWorldTexts() {
    return _debug_worlds;
}

uint32_t WorldTextRenderer::_debug_getTextRefreshes() {
    return _debug_refreshes;
}

uint32_t WorldTextRenderer::_debug_getCulled() {
    return _debug_culled;
}

void WorldTextRenderer::_debug_reset() {
    _debug_screens = 0;
    _debug_worlds = 0;
    _debug_refreshes = 0;
    _debug_culled = 0;
}
#endif

WorldTextRenderer::WorldTextEntry* WorldTextRenderer::ensureWorldTextEntry(WorldText *wt, bool transient) {
    WorldTextEntry *entry = nullptr;
    const uint32_t id = world_text_get_id(wt);

    if (id != WORLDTEXT_ID_NONE && tryGet(id, &entry)) {
        Weakptr *wptr = world_text_get_weakptr(wt);
        if (wptr != entry->worldText) {
            weakptr_release(entry->worldText);
            reset(entry);
        } else {
            return entry;
        }
    }

    if (entry == nullptr) {
        const uint32_t newId = _nextID++;
        world_text_set_id(wt, newId);
        _textEntries[newId] = createWorldTextEntry(transient);
        entry = &_textEntries[newId];
    }
    entry->worldText = world_text_get_and_retain_weakptr(wt);
    return entry;
}

WorldTextRenderer::WorldTextEntry WorldTextRenderer::createWorldTextEntry(bool transient) {
    WorldTextEntry entry;
    entry.worldText = nullptr;
    entry.textX = -1.0f;
    entry.textY = -1.0f;
    entry.depth = 0.0f;
    entry.sortOrder = 0;
    entry.bgX0 = entry.bgX1 = entry.bgY0 = entry.bgY1 = 0.0f;
    entry.tailX0 = entry.tailX1 = entry.tailY0 = entry.tailY1 = 0.0f;
    entry.fade = 1.0f;
    entry.weight = 0.5f;
    entry.outlineWeight = 0.0f;
    entry.softness = 0.0f;
    entry.tbh = _textManager->createTextBuffer(transient ? BufferType::Transient : BufferType::Static);
    entry.frame = false;
    entry.culled = false;
    return entry;
}

void WorldTextRenderer::reset(WorldTextEntry *entry) {
    entry->textX = -1.0f;
    entry->textY = -1.0f;
    clearText(entry);
}

void WorldTextRenderer::clearText(WorldTextEntry *entry) {
    if (isValid(entry->tbh)) {
        _textManager->clearTextBuffer(entry->tbh);
    }
}

bool WorldTextRenderer::isClipped(WorldTextEntry *entry) {
    return entry->textX < 0 || entry->textY < 0;
}

TrueTypeHandle WorldTextRenderer::loadTtf(const char* _filePath) {
    TrueTypeHandle handle;
    size_t size;
    void* data = fs::getFileContent(fs::openBundleFile(_filePath), &size);

    if (data != nullptr) {
        handle = _fontManager->createTtf(const_cast<const uint8_t*>(reinterpret_cast<uint8_t*>(data)),
                                         size);
        free(data);
    } else {
        handle = BGFX_INVALID_HANDLE;
    }

    return handle;
}

bool WorldTextRenderer::tryGet(uint32_t id, WorldTextEntry **out) {
    std::unordered_map<uint32_t, WorldTextEntry>::iterator itr = _textEntries.find(id);
    const bool exists = itr != _textEntries.end();
    if (exists) {
        *out = &itr->second;
    }
    return exists && *out != nullptr;
}

void WorldTextRenderer::release(WorldTextEntry *entry) {
    if (isValid(entry->tbh)) {
        _textManager->destroyTextBuffer(entry->tbh);
    }
    WorldText *wt = static_cast<WorldText*>(weakptr_get(entry->worldText));
    if (wt != nullptr) {
        world_text_set_geometry_dirty(wt, true);
        world_text_set_color_dirty(wt, true);
        world_text_set_id(wt, WORLDTEXT_ID_NONE);
    }
    weakptr_release(entry->worldText);
}

void WorldTextRenderer::refreshTextGeometry(WorldTextEntry *entry, WorldText *wt, bool points) {
    clearText(entry);

    const bool tail = world_text_get_tail(wt);

    // get font
    const FontName fontName = static_cast<FontName>(world_text_get_font_index(wt));
    const FontHandle fonts[] = {
        Font::shared()->getFont(fontName),
        Font::shared()->getFont(FontName_Emoji),
        Font::shared()->getFont(FontName_NotoJP)
    };

    // scaling factors
    const float pointsScale = points ? 1.0f / Screen::nbPixelsInOnePoint : 1.0f;
    const float fontScale = world_text_get_font_size(wt) / s_fontNativeSize[fontName];
    const float maxTextWidth = world_text_get_max_width(wt) - world_text_get_padding(wt) * 2;
    const float tailSize = world_text_get_font_size(wt) / 2.0f * pointsScale;

    // glyph spacing from weight
    const float weight = points ? 0.5f + (world_text_get_weight(wt) - 0.5f) / Screen::nbPixelsInOnePoint : world_text_get_weight(wt);
    const float outlineWeight = world_text_drawmode_get_outline_weight(wt) * pointsScale;
    const float spacing = ((weight - 0.5f) * FONT_SDF_WEIGHT_SPACING + outlineWeight * FONT_SDF_OUTLINE_WEIGHT_SPACING) * s_fontSDFRadius[fontName] * fontScale;

    // create text quads
    _textManager->setPenPosition(entry->tbh, 0.0f, 0.0f);
    _textManager->appendText(entry->tbh, fonts, 3, world_text_get_text(wt), nullptr, fontScale * pointsScale, maxTextWidth, spacing);

    const float anchorX = world_text_get_anchor_x(wt);
    const float anchorY = world_text_get_anchor_y(wt);

    // get final rect containing all the scaled glyphs
    const TextRectangle *rect = _textManager->getRectangle(entry->tbh);
    const float textWidth = rect->x1 - rect->x0;
    const float textHeight = rect->y1 - rect->y0;

    // keep track of text horizontal & vertical offsets, based on text anchor
    // note: we express world text anchor Y axis bottom to top, while screen Y is top to bottom here
    float hOffset = textWidth * anchorX;
    float vOffset = textHeight * (1.0f - anchorY);

    // background/tail quads
    {
        // background offsets needs to stay relative to text anchor
        const float padding2 = world_text_get_padding(wt) * 2.0f;
        const float tailWidth = tail ? tailSize : 0.0f;
        const float tailHeight = tail ? tailSize : 0.0f;
        const float tailEpsilon = 0.05f;
        const float tailVOffset = tailHeight * (1.0f - anchorY);

        entry->bgX0 = rect->x0 - hOffset - padding2 * anchorX;
        entry->bgX1 = rect->x1 - hOffset + padding2 * (1.0f - anchorX);
        entry->bgY0 = rect->y0 - vOffset - padding2 * (1.0f - anchorY) - tailVOffset;
        entry->bgY1 = rect->y1 - vOffset + padding2 * anchorY - tailVOffset;

        entry->tailX0 = rect->x0 - tailWidth * anchorX;
        entry->tailX1 = entry->tailX0 + tailWidth;
        entry->tailY0 = entry->bgY1 - tailEpsilon;
        entry->tailY1 = entry->bgY1 + tailHeight;

        // additional text offsets from background & tail, relative to text anchor
        hOffset += padding2 * (anchorX - .5f);
        vOffset += padding2 * (.5f - anchorY) + tailVOffset;

        world_text_set_cached_size(wt, textWidth + padding2, textHeight + padding2 + tailHeight, points);
    }

    // apply final text offsets
    _textManager->addTextOffset(entry->tbh, -hOffset, -vOffset);

    // cache SDF parameters
    entry->weight = weight;
    entry->softness = -1.0f; // text softness dirty
    entry->outlineWeight = outlineWeight;
}

void WorldTextRenderer::drawQuad(bgfx::ViewId id, bgfx::ProgramHandle program, uint32_t sortOrder,
                                 uint64_t state, float x0, float x1, float y0, float y1, float z,
                                 const float3 *normal, uint32_t rgba, bool unlit, VERTEX_LIGHT_STRUCT_T vlighting) {

    if (bgfx::getAvailTransientVertexBuffer(4, QuadVertex::_quadVertexLayout) == 4
        && bgfx::getAvailTransientIndexBuffer(6) == 6) {

        bgfx::TransientVertexBuffer tvb;
        bgfx::allocTransientVertexBuffer(&tvb, 4, QuadVertex::_quadVertexLayout);
        QuadVertex *vertices = (QuadVertex*)tvb.data;

        bgfx::TransientIndexBuffer tib;
        bgfx::allocTransientIndexBuffer(&tib, 6);
        uint16_t *triangles = (uint16_t*)tib.data;

        const float metadata = static_cast<float>((unlit ? 1 : 0) +
                                                  vlighting.ambient * 8 + vlighting.red * 128 +
                                                  vlighting.green * 2048 + vlighting.blue * 32768);

        vertices[0] = { x0, y0, z, metadata, normal->x, normal->y, normal->z, rgba, 0, 0 };
        vertices[1] = { x1, y0, z, metadata, normal->x, normal->y, normal->z, rgba, 0, 0 };
        vertices[2] = { x1, y1, z, metadata, normal->x, normal->y, normal->z, rgba, 0, 0 };
        vertices[3] = { x0, y1, z, metadata, normal->x, normal->y, normal->z, rgba, 0, 0 };

        triangles[0] = 0;
        triangles[1] = 1;
        triangles[2] = 2;
        triangles[3] = 2;
        triangles[4] = 3;
        triangles[5] = 0;

        bgfx::setVertexBuffer(0, &tvb);
        bgfx::setIndexBuffer(&tib);

        bgfx::setState(state);
        bgfx::submit(id, program, sortOrder);
    }
}

#else /* P3S_CLIENT_HEADLESS */

WorldTextRenderer::WorldTextRenderer() {}
WorldTextRenderer::~WorldTextRenderer() {}

#endif /* P3S_CLIENT_HEADLESS */
