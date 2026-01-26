#ifndef P3S_CLIENT_HEADLESS

#ifndef FONT_MANAGER_H_HEADER_GUARD
#define FONT_MANAGER_H_HEADER_GUARD

#include <bx/handlealloc.h>
#include <bx/string.h>
#include <bgfx/bgfx.h>

#include <hb.h>
#include <hb-ft.h>
#include <hb-ot.h>

class Atlas;

#define MAX_OPENED_FILES 64
#define MAX_OPENED_FONT  64

struct FontInfo {
	/// The pixel extents above the baseline in pixels (typically positive)
	float ascender;
	/// The extents below the baseline in pixels (typically negative)
	float descender;
	/// The spacing in pixels between one row's descent and the next row's ascent
	float lineGap;
	/// This field gives the maximum horizontal cursor advance for all glyphs in the font
	float maxAdvanceWidth;
    /// Horizontal cursor advance for space characters
    float spaceWidth;
	/// The thickness of the under/hover/strike-trough line in pixels
	float underlineThickness;
	/// The position of the underline relatively to the baseline
	float underlinePosition;
    /// Signed distance field parameters, 0 if not using SDF
    float sdfRadius;
    uint8_t sdfPadding;
	/// Set this to false for pixel fonts
	bool filtering;
};

/// Unicode value of a character
typedef int32_t CodePoint;

// Glyph metrics:
// --------------
//
//                       xmin                     xmax
//                        |                         |
//                        |<-------- width -------->|
//                        |                         |
//              |         +-------------------------+----------------- ymax
//              |         |    ggggggggg   ggggg    |     ^        ^
//              |         |   g:::::::::ggg::::g    |     |        |
//              |         |  g:::::::::::::::::g    |     |        |
//              |         | g::::::ggggg::::::gg    |     |        |
//              |         | g:::::g     g:::::g     |     |        |
//    offset_x -|-------->| g:::::g     g:::::g     |  offset_y    |
//              |         | g:::::g     g:::::g     |     |        |
//              |         | g::::::g    g:::::g     |     |        |
//              |         | g:::::::ggggg:::::g     |     |        |
//              |         |  g::::::::::::::::g     |     |      height
//              |         |   gg::::::::::::::g     |     |        |
//  baseline ---*---------|---- gggggggg::::::g-----*--------      |
//            / |         |             g:::::g     |              |
//     origin   |         | gggggg      g:::::g     |              |
//              |         | g:::::gg   gg:::::g     |              |
//              |         |  g::::::ggg:::::::g     |              |
//              |         |   gg:::::::::::::g      |              |
//              |         |     ggg::::::ggg        |              |
//              |         |         gggggg          |              v
//              |         +-------------------------+----------------- ymin
//              |                                   |
//              |------------- advance_x ---------->|

/// Refer to diagram above for glyph metrics
struct GlyphInfo {
	// rasterized bitmap metrics
	float width;
	float height;
	float offset_x;
	float offset_y;

	/// Region index in the atlas storing textures
	uint16_t regionIndex;

    /// Colored glyphs use RGBA channels, non-colored use R only
    bool colored;

    /// Dictates which atlas to use for this glyph
    bool filtering;

	char pad[2];
};

BGFX_HANDLE(TrueTypeHandle)
BGFX_HANDLE(FontHandle)

class FontManager {
public:
	/// Create the font manager using an external cube atlas (doesn't take
	/// ownership of the atlas).
	FontManager(Atlas* atlas, Atlas* atlasPoint);

	/// Create the font manager and create the texture cube as BGRA8 with
	/// linear filtering.
	FontManager(uint16_t size=512, uint16_t sizePoint=512);

	~FontManager();

	/// Retrieve the atlas used by the font manager (e.g. to add stuff to it)
	const Atlas* getAtlas(bool filtering=true) const;

	/// Load a TrueType font from a given buffer. The buffer is copied and
	/// thus can be freed or reused after this call.
	///
	/// @return invalid handle if the loading fail
	TrueTypeHandle createTtf(const uint8_t* buffer, const size_t size);
	void destroyTtf(TrueTypeHandle tth);

    FontHandle createFontByPixelSize(TrueTypeHandle tth, uint32_t ttfontIndex, uint32_t pixelSize, float spaceWidth, bool colored, float sdfRadius, uint8_t sdfPadding, bool filtering);
	FontHandle createCustomFont(float ascent, float descent, float lineGap, float maxAdvanceX, float sdfRadius, uint8_t sdfPadding, bool filtering);
	void destroyFont(FontHandle fh);

	/// Preload a set of glyphs from a TrueType file.
	///
	/// @return True if every glyph could be preloaded, false otherwise if
	///   the Font is a baked font, this only do validation on the characters.
	bool preloadGlyph(FontHandle fh, const char* string);
	bool preloadGlyph(FontHandle fh, hb_codepoint_t glyphIdx);

	/// @param AtoR moves mono-channel glyph from A to R channel
	bool addGlyphBitmap(FontHandle fh, CodePoint c, uint16_t w, uint16_t h,
                        uint16_t pitch, const uint8_t* bitmapBuffer, float glyphOffsetX,
                        float glyphOffsetY, float scale, bool AtoR, bool colored, bool filtering);

	const FontInfo& getFontInfo(FontHandle fh) const;
    hb_font_t *getHbFont(FontHandle fh) const;

    /// Return the rendering informations about the glyph region. Load the
	/// glyph from a TrueType font if possible
	const GlyphInfo* getGlyphInfo(FontHandle fh, hb_codepoint_t glyphIdx);
    bool hasGlyph(FontHandle fh, CodePoint c);

	const GlyphInfo& getBlackGlyph() const {
		return _blackGlyph;
	}

	bgfx::TextureHandle getAtlasTexture();

private:
	struct CachedFont;
	struct CachedFile {
		uint8_t* buffer;
		size_t bufferSize;
	};

	void init();
	bool addBitmap(GlyphInfo& glyphInfo, const uint8_t* data, bool colored);

	bool _ownAtlas;
	Atlas *_atlas, *_atlasPoint;

	bx::HandleAllocT<MAX_OPENED_FONT> _fontHandles;
	CachedFont* _cachedFonts;

	bx::HandleAllocT<MAX_OPENED_FILES> _filesHandles;
	CachedFile* _cachedFiles;

	GlyphInfo _blackGlyph;

	// temporary buffer to raster glyph
	uint8_t* _buffer;
};

#endif // FONT_MANAGER_H_HEADER_GUARD

#endif // P3S_CLIENT_HEADLESS
