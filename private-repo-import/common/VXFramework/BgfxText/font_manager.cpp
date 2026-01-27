#ifndef P3S_CLIENT_HEADLESS

#include "font_manager.h"

#include <bx/bx.h>
#include <stb/stb_truetype.h>
#include <wchar.h> // wcslen
#include <tinystl/allocator.h>
#include <tinystl/unordered_map.h>
namespace stl = tinystl;

#define SDF_IMPLEMENTATION
#include <sdf/sdf.h>

#include <ft2build.h>
#include <freetype/freetype.h>
#include <freetype/ftcolor.h>

#include "common.h"
#include "cube_atlas.h"
#include "text_buffer_manager.h"
#include "utils.h"
#include "vxlog.h"
#include "config.h"
#include "VXConfig.hpp"

class TrueTypeFont {
public:
	TrueTypeFont();
	~TrueTypeFont();

	/// Initialize from an external buffer
	/// @remark The ownership of the buffer is external, and you must ensure it stays valid up to this object lifetime
	/// @return true if the initialization succeeds
	bool init(const uint8_t* buffer, uint32_t bufferSize, int32_t fontIndex, uint32_t pixelHeight, bool colored);

	FontInfo getFontInfo();
    hb_font_t* getHbFont();

	/// raster a glyph to outBuffer & update GlyphInfo
	/// @param sdfRadius raster as signed distance field (> 0) or as 8bit alpha (0)
    /// @param sdfPadding used if sdfRadius > 0
	/// @remark buffer min size: glyphInfo.m_width * glyphInfo * height * sizeof(char)
	bool bakeGlyph(hb_codepoint_t glyphIdx, float sdfRadius, uint8_t sdfPadding, bool filtering, GlyphInfo& outGlyphInfo, uint8_t* outBuffer);
private:
    friend class FontManager;

	float _scale;
    bool _colored;
    FT_Library _ftLibrary;
    FT_Face _ftFace;
    hb_font_t* _hbFont;
    FT_UInt _ftReplacementGlyph, _ftTofuGlyph;
};

TrueTypeFont::TrueTypeFont() : _ftLibrary(nullptr), _ftFace(nullptr), _hbFont(nullptr) { }

TrueTypeFont::~TrueTypeFont() {
    if (_hbFont) {
        hb_font_destroy(_hbFont);
    }
    if (_ftFace) {
        FT_Done_Face(_ftFace);
    }
    if (_ftLibrary) {
        FT_Done_FreeType(_ftLibrary);
    }
}

bool TrueTypeFont::init(const uint8_t* buffer, uint32_t bufferSize, int32_t fontIndex, uint32_t pixelHeight, bool colored) {
    if (bufferSize <= 256 || bufferSize >= 100000000 || pixelHeight <= 4 || pixelHeight >= 128) {
        vxlog_error("TrueType buffer size is suspicious");
        return false;
    }

    if (FT_Init_FreeType(&_ftLibrary)) {
        vxlog_error("Could not initialize FreeType library");
        return false;
    }
    //FT_Property_Set(_ftLibrary, "ot-svg", "svg-hook", NULL);

    if (FT_New_Memory_Face(_ftLibrary, buffer, bufferSize, fontIndex, &_ftFace)) {
        vxlog_error("Could not load FreeType font face");
        return false;
    }

    if (FT_Set_Pixel_Sizes(_ftFace, 0, pixelHeight)) {
        vxlog_warning("Could not set FreeType pixel size");
    }
    /* if (FT_Palette_Select(_ftFace, 0, nullptr)) {
        vxlog_warning("Could not select FreeType palette");
    } */

    _hbFont = hb_ft_font_create_referenced(_ftFace);
    if (_hbFont == nullptr) {
        vxlog_error("Could not create HarfBuzz font");
        return false;
    }
    hb_ft_font_set_funcs(_hbFont);
    if (colored) {
        hb_ft_font_set_load_flags(_hbFont, FT_LOAD_COLOR | FT_LOAD_DEFAULT);
    }
    //hb_font_set_scale(_hbFont, _ftFace->size->metrics.x_scale, _ftFace->size->metrics.y_scale);

    _ftReplacementGlyph = FT_Get_Char_Index(_ftFace, 65533); // 65533 = 0xFFFD = �
    _ftTofuGlyph = FT_Get_Char_Index(_ftFace, 9633); // 9633 = 0x25A1 = □

    if (_ftFace->units_per_EM == 0) {
        _scale = 1.0f;
    } else {
        _scale = static_cast<float>(pixelHeight) / _ftFace->units_per_EM;
    }
    _colored = colored;

    return true;
}

FontInfo TrueTypeFont::getFontInfo() {
    const int x0 = _ftFace->bbox.xMin;
    const int y0 = _ftFace->bbox.yMin;
    const int x1 = _ftFace->bbox.xMax;
    const int y1 = _ftFace->bbox.yMax;

    // /!\ font face metrics are in font units, which need to be scaled to our pixel size
	FontInfo outFontInfo;
	outFontInfo.ascender = static_cast<float>(_ftFace->ascender) * _scale;
    outFontInfo.descender = static_cast<float>(_ftFace->descender) * _scale;
    outFontInfo.lineGap = static_cast<float>(_ftFace->height - _ftFace->ascender + _ftFace->descender) * _scale;
    outFontInfo.maxAdvanceWidth = static_cast<float>(x1 - x0) * _scale;
	outFontInfo.underlinePosition = static_cast<float>(_ftFace->underline_position) * _scale;
    outFontInfo.underlineThickness = static_cast<float>(_ftFace->underline_thickness) * _scale;

	return outFontInfo;
}

hb_font_t* TrueTypeFont::getHbFont() {
    return _hbFont;
}

bool TrueTypeFont::bakeGlyph(hb_codepoint_t glyphIdx, float sdfRadius, uint8_t sdfPadding, bool filtering, GlyphInfo& outGlyphInfo, uint8_t* outBuffer) {
    FT_UInt glyphIndex = static_cast<FT_UInt>(glyphIdx);
    
    FT_Int32 loadFlags = FT_LOAD_DEFAULT;
    if (_colored/*  && _ftFace->face_flags & FT_FACE_FLAG_COLOR */) {
        loadFlags |= FT_LOAD_COLOR|FT_LOAD_NO_SVG;
    }

    if (FT_Load_Glyph(_ftFace, glyphIndex, loadFlags)) {
        vxlog_error("bakeGlyph - could not load glyph");
        return false;
    }
    FT_GlyphSlot glyph = _ftFace->glyph;

    if (/* glyph->format == FT_GLYPH_FORMAT_BITMAP &&  */glyph->bitmap.pixel_mode == FT_PIXEL_MODE_BGRA) {
        // Already rendered (color emoji)
    } else {
        if (FT_Render_Glyph(glyph, FT_RENDER_MODE_NORMAL) > 0) {
            vxlog_error("bakeGlyph - could not render glyph");
            return false;
        }
    }

    outGlyphInfo.width = static_cast<float>(glyph->bitmap.width);
    outGlyphInfo.height = static_cast<float>(glyph->bitmap.rows);
    outGlyphInfo.offset_x = static_cast<float>(glyph->bitmap_left);
    outGlyphInfo.offset_y = -static_cast<float>(glyph->bitmap_top);
    
    outGlyphInfo.colored = glyph->bitmap.pixel_mode == FT_PIXEL_MODE_BGRA;
    outGlyphInfo.filtering = filtering;

    const uint32_t w = glyph->bitmap.width;
    const uint32_t h = glyph->bitmap.rows;
    if (outGlyphInfo.colored) {
        bx::memCopy(outBuffer, glyph->bitmap.buffer, w * h * 4);
    } else {
        if (sdfRadius > 0.0f && w > 0 && h > 0) {
            const uint32_t dw = sdfPadding;
            const uint32_t dh = sdfPadding;

            const uint32_t nw = w + dw * 2;
            const uint32_t nh = h + dh * 2;

            const uint32_t buffSize = nw * nh * sizeof(uint8_t);
            uint8_t* alphaImg = (uint8_t*)malloc(buffSize);
            bx::memSet(alphaImg, 0, buffSize);

            // copy the original buffer to the temp one
            for (uint32_t y = dh; y < nh - dh; ++y) {
                bx::memCopy(alphaImg + y * nw + dw, glyph->bitmap.buffer + (y - dh) * w, w);
            }

            sdfBuildDistanceField(outBuffer, nw, sdfRadius, alphaImg, nw, nh, nw);
            free(alphaImg);

            outGlyphInfo.offset_x -= (float)dw;
            outGlyphInfo.offset_y -= (float)dh;
            outGlyphInfo.width = (float)nw;
            outGlyphInfo.height = (float)nh;
        } else {
            bx::memCopy(outBuffer, glyph->bitmap.buffer, w * h);
        }
    }

    return true;
}

typedef stl::unordered_map<hb_codepoint_t, GlyphInfo> GlyphHashMap;

// cache font data
struct FontManager::CachedFont {
	CachedFont() : trueTypeFont(nullptr) { }

	FontInfo fontInfo;
	GlyphHashMap cachedGlyphs;
    TrueTypeFont* trueTypeFont;
    TrueTypeHandle trueTypeHandle;
};

#define MAX_FONT_BUFFER_SIZE (512 * 512 * 4)

FontManager::FontManager(Atlas* atlas, Atlas* atlasPoint) : _ownAtlas(false),
                                                            _atlas(atlas),
                                                            _atlasPoint(atlasPoint) {
	init();
}

FontManager::FontManager(uint16_t size, uint16_t sizePoint) : _ownAtlas(true) ,
                                                              _atlas(new Atlas(size, 4096, true)),
                                                              _atlasPoint(new Atlas(sizePoint, 4096, false)) {
	init();
}

void FontManager::init() {
	_cachedFiles = new CachedFile[MAX_OPENED_FILES];
	_cachedFonts = new CachedFont[MAX_OPENED_FONT];
	_buffer = new uint8_t[MAX_FONT_BUFFER_SIZE];

	const uint32_t W = 3;
	// Create filler rectangle
	uint8_t buffer[W * W * 4];
	bx::memSet(buffer, 255, W * W * 4);

	_blackGlyph.width = W;
	_blackGlyph.height = W;

	///make sure the black glyph doesn't bleed by using a one pixel inner outline
	_blackGlyph.regionIndex = _atlas->addRegion(W, W, buffer, AtlasRegion::TYPE_GRAY, 1);
}

FontManager::~FontManager() {
    if (_fontHandles.getNumHandles() > 0) {
        vxlog_error("All the fonts must be destroyed before destroying the manager");
    }
	delete [] _cachedFonts;

    if (_filesHandles.getNumHandles() > 0) {
        vxlog_error("All the font files must be destroyed before destroying the manager");
    }
	delete [] _cachedFiles;

	delete [] _buffer;

	if (_ownAtlas) {
		delete _atlas;
	}
}

const Atlas* FontManager::getAtlas(bool filtering) const {
	return filtering ? _atlas : _atlasPoint;
}

TrueTypeHandle FontManager::createTtf(const uint8_t* buffer, const size_t size) {
	uint16_t id = _filesHandles.alloc();
    vx_assert(id != bx::kInvalidHandle);
	_cachedFiles[id].buffer = new uint8_t[size];
	_cachedFiles[id].bufferSize = size;
	bx::memCopy(_cachedFiles[id].buffer, buffer, size);

	TrueTypeHandle ret = { id };
	return ret;
}

void FontManager::destroyTtf(TrueTypeHandle tth) {
    vx_assert(isValid(tth));
	delete _cachedFiles[tth.idx].buffer;
	_cachedFiles[tth.idx].bufferSize = 0;
	_cachedFiles[tth.idx].buffer = nullptr;
	_filesHandles.free(tth.idx);
}

FontHandle FontManager::createFontByPixelSize(TrueTypeHandle tth, uint32_t ttfontIndex, uint32_t pixelSize, float spaceWidth, bool colored, float sdfRadius, uint8_t sdfPadding, bool filtering) {
    vx_assert(isValid(tth));

    TrueTypeFont* ttf = new TrueTypeFont();
    if (ttf->init(_cachedFiles[tth.idx].buffer, _cachedFiles[tth.idx].bufferSize, ttfontIndex, pixelSize, colored) == false) {
        delete ttf;
        FontHandle invalid = { bx::kInvalidHandle };
        return invalid;
    }

    uint16_t fontIdx = _fontHandles.alloc();
    vx_assert(fontIdx != bx::kInvalidHandle);

    CachedFont& font = _cachedFonts[fontIdx];
    font.trueTypeFont = ttf;
    font.trueTypeHandle = tth;
    font.fontInfo = ttf->getFontInfo();
    font.fontInfo.spaceWidth = spaceWidth;
    font.fontInfo.sdfRadius = sdfRadius;
    font.fontInfo.sdfPadding = sdfPadding;
    font.fontInfo.filtering = filtering;
    font.cachedGlyphs.clear();

    FontHandle handle = { fontIdx };
    return handle;
}

FontHandle FontManager::createCustomFont(float ascent, float descent, float lineGap, float maxAdvanceX, float sdfRadius, uint8_t sdfPadding, bool filtering) {
	uint16_t fontIdx = _fontHandles.alloc();
    vx_assert(fontIdx != bx::kInvalidHandle);

	CachedFont& font = _cachedFonts[fontIdx];
	font.trueTypeFont = nullptr;
    font.trueTypeHandle = BGFX_INVALID_HANDLE;
	font.fontInfo.ascender = ascent;
	font.fontInfo.descender = descent;
	font.fontInfo.lineGap = lineGap;
    font.fontInfo.spaceWidth = maxAdvanceX;
    font.fontInfo.maxAdvanceWidth = maxAdvanceX;
    font.fontInfo.sdfRadius = sdfRadius;
    font.fontInfo.sdfPadding = sdfPadding;
    font.fontInfo.filtering = filtering;
	font.cachedGlyphs.clear();

	FontHandle handle = { fontIdx };
	return handle;
}

void FontManager::destroyFont(FontHandle fh) {
    vx_assert(isValid(fh));

	CachedFont& font = _cachedFonts[fh.idx];

	if (font.trueTypeFont != nullptr) {
		delete font.trueTypeFont;
		font.trueTypeFont = nullptr;
	}
    if (isValid(font.trueTypeHandle)) {
        destroyTtf(font.trueTypeHandle);
        font.trueTypeHandle = BGFX_INVALID_HANDLE;
    }

	font.cachedGlyphs.clear();
	_fontHandles.free(fh.idx);
}

bool FontManager::preloadGlyph(FontHandle fh, const char* string) {
    vx_assert(isValid(fh));

    const uint32_t length = static_cast<uint32_t>(bx::strLen(string));
    if (length == 0) {
        return false;
    }

	CachedFont& font = _cachedFonts[fh.idx];
	if (font.trueTypeFont == nullptr) {
		return false;
	}

    hb_buffer_t* buffer = hb_buffer_create();
    if (buffer == nullptr) {
        vxlog_error("Failed to create HarfBuzz buffer");
        return false;
    }

    hb_feature_t features[] = {
        {HB_TAG('k', 'e', 'r', 'n'), 1, 0, (unsigned int)-1}, // Kerning
        {HB_TAG('l', 'i', 'g', 'a'), 1, 0, (unsigned int)-1}, // Ligatures
        {HB_TAG('c', 'l', 'i', 'g'), 1, 0, (unsigned int)-1}, // Contextual Ligatures
        {HB_TAG('e','m','o','j'), 1, 0, (unsigned int)-1},    // Emoji
        {HB_TAG('c','o','m','p'), 1, 0, (unsigned int)-1}     // Composition
    };

    hb_buffer_add_utf8(buffer, string, length, 0, length);
    hb_buffer_guess_segment_properties(buffer);
    hb_shape(font.trueTypeFont->getHbFont(), buffer, features, 5);
    
    unsigned int glyphCount;
    const hb_glyph_info_t* glyphInfo = hb_buffer_get_glyph_infos(buffer, &glyphCount);
    const hb_glyph_position_t* glyphPos = hb_buffer_get_glyph_positions(buffer, nullptr);

    bool success = true;
    for (unsigned int i = 0; i < glyphCount; ++i) {
        success = preloadGlyph(fh, glyphInfo[i].codepoint) && success;
    }

	return success;
}

bool FontManager::preloadGlyph(FontHandle fh, hb_codepoint_t glyphIdx) {
    vx_assert(isValid(fh));

	CachedFont& font = _cachedFonts[fh.idx];
	FontInfo& fontInfo = font.fontInfo;

	GlyphHashMap::iterator iter = font.cachedGlyphs.find(glyphIdx);
	if (iter != font.cachedGlyphs.end()) {
		return true;
	}

	if (font.trueTypeFont != nullptr) {
		GlyphInfo glyphInfo;

		if (font.trueTypeFont->bakeGlyph(glyphIdx, fontInfo.sdfRadius, fontInfo.sdfPadding, fontInfo.filtering, glyphInfo, _buffer) == false) {
			return false;
		}

		if (addBitmap(glyphInfo, _buffer, glyphInfo.colored) == false) {
			return false;
		}

		font.cachedGlyphs[glyphIdx] = glyphInfo;
		return true;
	}

	return false;
}

bool FontManager::addGlyphBitmap(FontHandle fh, CodePoint c, uint16_t w, uint16_t h,
                                 uint16_t pitch, const uint8_t* bitmapBuffer, float glyphOffsetX,
                                 float glyphOffsetY, float scale, bool AtoR, bool colored, bool filtering) {
    vx_assert(isValid(fh));

	CachedFont& font = _cachedFonts[fh.idx];

	GlyphHashMap::iterator iter = font.cachedGlyphs.find(c);
	if (iter != font.cachedGlyphs.end()) {
		return true;
	}

	GlyphInfo glyphInfo;

	glyphInfo.offset_x = glyphOffsetX * scale;
	glyphInfo.offset_y = glyphOffsetY * scale;
	glyphInfo.width = (float)w * scale;
	glyphInfo.height = (float)h * scale;
    glyphInfo.colored = colored;
    glyphInfo.filtering = filtering;

	uint8_t* dst = _buffer;
    uint32_t dstPitch = w * 4;
	const uint8_t* src = bitmapBuffer;
	uint32_t srcPitch = pitch;
	if (AtoR) {
        for (int32_t ii = 0; ii < h; ++ii) {
            for (int32_t jj = 0; jj < w; ++jj) {
                *((uint32_t*)dst) = TextBufferManager::AtoR(*((uint32_t *)src));

                dst += 4;
                src += 4;
            }
            src += (srcPitch - dstPitch);
        }
	} else {
        for (int32_t ii = 0; ii < h; ++ii) {
            bx::memCopy(dst, src, dstPitch);

            dst += dstPitch;
            src += srcPitch;
        }
	}
    glyphInfo.regionIndex = (filtering ? _atlas : _atlasPoint)->addRegion(w, h, _buffer, AtlasRegion::TYPE_BGRA8, 0);

	font.cachedGlyphs[c] = glyphInfo;
	return true;
}

const FontInfo& FontManager::getFontInfo(FontHandle fh) const {
    vx_assert(isValid(fh));
	return _cachedFonts[fh.idx].fontInfo;
}

hb_font_t* FontManager::getHbFont(FontHandle fh) const {
    vx_assert(isValid(fh));
    return _cachedFonts[fh.idx].trueTypeFont->getHbFont();
}

const GlyphInfo* FontManager::getGlyphInfo(FontHandle fh, hb_codepoint_t glyphIdx) {
	const GlyphHashMap& cachedGlyphs = _cachedFonts[fh.idx].cachedGlyphs;
	GlyphHashMap::const_iterator it = cachedGlyphs.find(glyphIdx);

	if (it == cachedGlyphs.end()) {
		if (preloadGlyph(fh, glyphIdx) == false) {
			return nullptr;
		}

		it = cachedGlyphs.find(glyphIdx);
	}

    if (it == cachedGlyphs.end()) {
        vxlog_error("Failed to preload glyph");
    }
	return &it->second;
}

bool FontManager::hasGlyph(FontHandle fh, CodePoint c) {
    vx_assert(isValid(fh));

    if (_cachedFonts[fh.idx].trueTypeFont != nullptr) {
        const TrueTypeFont* ttf = _cachedFonts[fh.idx].trueTypeFont;
        const FT_UInt glyphIdx = FT_Get_Char_Index(ttf->_ftFace, c);

        return glyphIdx != 0 && glyphIdx != ttf->_ftReplacementGlyph && glyphIdx != ttf->_ftTofuGlyph;
    } else {
        const GlyphHashMap& cachedGlyphs = _cachedFonts[fh.idx].cachedGlyphs;

        return cachedGlyphs.find(c) != cachedGlyphs.end();
    }
}

bgfx::TextureHandle FontManager::getAtlasTexture() {
    return _atlas->getTextureHandle();
}

bool FontManager::addBitmap(GlyphInfo& glyphInfo, const uint8_t* data, bool colored) {
    glyphInfo.regionIndex = (glyphInfo.filtering ? _atlas : _atlasPoint)
        ->addRegion((uint16_t)bx::ceil(glyphInfo.width),
                    (uint16_t)bx::ceil(glyphInfo.height),
                    data,
                    colored ? AtlasRegion::TYPE_BGRA8 : AtlasRegion::TYPE_GRAY
    );
	return true;
}

#endif // P3S_CLIENT_HEADLESS
