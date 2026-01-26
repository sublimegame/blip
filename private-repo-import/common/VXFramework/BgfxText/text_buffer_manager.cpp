#ifndef P3S_CLIENT_HEADLESS

#include <bgfx/bgfx.h>
#include <hb.h>

#include <stddef.h> // offsetof
#include <wchar.h>  // wcslen
#include <math.h>
#include <float.h>
#include <string.h>
#include <deque>

#include "text_buffer_manager.h"
#include "utf8.h"
#include "cube_atlas.h"
#include "config.h"
#include "vxlog.h"
#include "fifo_list.h"

#define MAX_BUFFERED_CHARACTERS (8192 - 5)
#define UNICODE_CONTINUATION_MASK 0b11000000
#define UNICODE_CONTINUATION_VALUE 0b10000000

// returns false if invalid UTF-8 sequence
// function given by chatGPT and modified to fit our needs
bool _utf8_decode(const char *str, uint32_t *c, int *length) {
    vx_assert(str != nullptr);
    vx_assert(c != nullptr);
    vx_assert(length != nullptr);
    
    *c = 0;
    *length = 1;

    // Determine the number of bytes in the UTF-8 sequence by looking at the first byte
    if ((*str & 0x80) == 0x00) {
        *c = static_cast<uint32_t>(*str & 0x7f);
        *length = 1;
    } else if ((*str & 0xe0) == 0xc0) {
        *c = static_cast<uint32_t>(*str & 0x1f);
        *length = 2;
    } else if ((*str & 0xf0) == 0xe0) {
        *c = static_cast<uint32_t>(*str & 0x0f);
        *length = 3;
    } else if ((*str & 0xf8) == 0xf0) {
        *c = static_cast<uint32_t>(*str & 0x07);
        *length = 4;
    } else {
        // Invalid UTF-8 sequence
        return false;
    }

    // Read the remaining bytes in the sequence and compute the Unicode code point
    for (int i = 1; i < *length; ++i) {
        if ((str[i] & 0xc0) != 0x80) {
            // Invalid UTF-8 sequence
            *c = 0;
            *length = 1;
            return false;
        }
        *c = (*c << 6) | (str[i] & 0x3f);
    }
    return true;
}

typedef struct {
    hb_codepoint_t glyphIdx;
    uint32_t clusterIdx;
    hb_position_t  advanceX;
    hb_position_t  advanceY;
    hb_position_t  offsetX;
    hb_position_t  offsetY;
    float scale;
    bool isNewLineReplacement;
    uint8_t fontIdx;

    char pad[2];
} GlyphEntry;

class TextBuffer {
public:
	TextBuffer(FontManager* fm);
	~TextBuffer();

	void setTextColor(uint32_t rgba=0x000000FF);
    uint32_t getTextColor();
	void applyColor();
	void setPenPosition(float x, float y);
	void addOffset(float dx, float dy);
    void setAlignment(TextAlignment value);
    float setSlant(float value);
    static uint32_t decodeMultilineText(const FontHandle *fonts, uint8_t fontCount, FontManager *fm, const char* text, const char* end,
                                        float scale, float spacing, float maxWidth, GlyphEntry **glyphsOut, FifoList *newLinesOut);
	void appendText(const FontHandle *fonts, uint8_t fontCount, const char* text, const char* end=nullptr,
					float scale=1.0f, float maxWidth=0.0f, float spacing=0.0f);
	void appendAtlasFace(uint16_t faceIndex, bool filtering);
	void clearTextBuffer();
	const uint8_t* getVertexBuffer();
	uint32_t getVertexCount() const;
	uint32_t getVertexSize() const;
	const uint16_t* getIndexBuffer() const;
	uint32_t getIndexCount() const;
	uint32_t getIndexSize() const;
	const TextRectangle* getRectangle() const;

private:
    /// To be called after text shaping, and _rectangle must be initialized before calling first appendGlyph
    /// @param font font to use, the caller must ensure the font has requested glyph or it will be skipped
    /// @param glyphIdx is a glyph index (NOT a codepoint) for given font
    /// @param glyphPos metrics from text shaping
    /// @param scale applied on geometry
    /// @param spacing extra offset between glyphs, note that TrueType fonts have kerning
	void appendGlyph(FontHandle font, hb_codepoint_t glyphIdx, hb_glyph_position_t glyphPos, float scale=1.0f, float spacing=0.0f);
	void setVertex(uint32_t i, float x, float y, uint32_t rgba);

	uint32_t _textColor;
	float _penX;
	float _penY;
	float _originX;
	float _originY;
	TextRectangle _rectangle;
	FontManager* _fontManager;

	struct TextVertex {
		float x, y;
		int16_t u, v, w, t;
		uint32_t rgba;
	};

	TextVertex* _vertexBuffer;
	uint16_t* _indexBuffer;

	uint32_t _indexCount;
	uint16_t _vertexCount;

    struct LineAlignment {
        uint16_t endVertex;
        float offset;
    };
    std::deque<LineAlignment> _alignmentMap;
    TextAlignment _alignment;
    float _slant, _slantMaxOffset;
};

TextBuffer::TextBuffer(FontManager* fm) {
    _fontManager = fm;
    _vertexBuffer = new TextVertex[MAX_BUFFERED_CHARACTERS * 4];
    _indexBuffer = new uint16_t[MAX_BUFFERED_CHARACTERS * 6];

    _textColor = 0xffffffff;
    _penX = 0.0f;
    _penY = 0.0f;
    _originX = 0.0f;
    _originY = 0.0f;
    _indexCount = 0;
    _vertexCount = 0;
	_rectangle = { -1, -1, -1, -1 };
    _alignment = TextAlignment_Left;
    _slant = -1.0f;
}

TextBuffer::~TextBuffer() {
	delete [] _vertexBuffer;
	delete [] _indexBuffer;
}

void TextBuffer::setTextColor(uint32_t rgba) {
	_textColor = rgba;
}

uint32_t TextBuffer::getTextColor() {
    return _textColor;
}

void TextBuffer::applyColor() {
	for (uint32_t i = 0; i < _vertexCount; ++i) {
		_vertexBuffer[i].rgba = _textColor;
	}
}

void TextBuffer::setPenPosition(float x, float y) {
	_penX = x; _penY = y;
	_originX = x; _originY = y;
}

void TextBuffer::addOffset(float dx, float dy) {
    // apply offsets to all current vertices
	for (uint32_t i = 0; i < _vertexCount; ++i) {
		_vertexBuffer[i].x += dx;
		_vertexBuffer[i].y += dy;
	}

	// apply offsets to current rect
    _rectangle.x0 += dx;
	_rectangle.x1 += dx;
	_rectangle.y0 += dy;
	_rectangle.y1 += dy;
}

void TextBuffer::setAlignment(TextAlignment value) {
    // no need to apply alignment on single-line texts
    if (_alignmentMap.size() <= 1) {
        return;
    }

    uint16_t startVertex = 0;
    std::deque<LineAlignment>::iterator itr;
    for (itr = _alignmentMap.begin(); itr != _alignmentMap.end(); ++itr) {
        if (itr->offset > 0.0f) {
            // skip if existing alignment is the same
            if (_alignment == value) {
                return;
            }

            // reset previous alignment
            for (uint32_t i = startVertex; i <= itr->endVertex; ++i) {
                _vertexBuffer[i].x -= itr->offset;
            }
        }

        // apply new alignment
        itr->offset = (_rectangle.x1 - _vertexBuffer[itr->endVertex].x) * (value == TextAlignment_Left ? 0.0f :
                                                                           value == TextAlignment_Center ? 0.5f : 1.0f);
        for (uint32_t i = startVertex; i <= itr->endVertex; ++i) {
            _vertexBuffer[i].x += itr->offset;
        }

        startVertex = itr->endVertex + 1;
    }

    _alignment = value;
}

float TextBuffer::setSlant(float value) {
    if (float_isEqual(value, _slant, EPSILON_ZERO)) {
        return 0.0f;
    }

    const float offset = _slantMaxOffset * ((2.0f * value - 1.0f) - (_slant != -1.0f ? 2.0f * _slant - 1.0f : 0.0f));
    bool side = true;
    for (uint32_t i = 0; i < _vertexCount; ++i) {
        if (side) {
            _vertexBuffer[i].x += offset;
        } else {
            _vertexBuffer[i].x -= offset;
        }
        if (i % 2 == 0) {
            side = !side;
        }
    }

    _rectangle.x0 -= offset;
    _rectangle.x1 += offset;
    _slant = value;

    return 2.0f * offset;
}

/// Decodes the whole string and find multi-line breaks. This function isn't useful for single line texts
/// (no new lines inserted), where the text can be processed in one loop
/// @param codepointsOut array of decoded codepoints
/// @param newLinesOut ptrs to codepoints where a new line must be inserted
/// @returns number of characters decoded
uint32_t TextBuffer::decodeMultilineText(const FontHandle *fonts, uint8_t fontCount, FontManager *fm, const char* text, const char* end,
                                         float scale, float spacing, float maxWidth, GlyphEntry **glyphsOut, FifoList *newLinesOut) {
    
    if (text == nullptr || glyphsOut == nullptr || newLinesOut == nullptr || fonts == nullptr || fontCount == 0) {
        return 0;
    }

    const uint32_t length = static_cast<uint32_t>(bx::strLen(text));
    if (end == nullptr) {
        end = text + length;
    }
    vx_assert(end >= text);

    if (length == 0) {
        return 0;
    }

    hb_buffer_t* buffer = hb_buffer_create();
    if (buffer == nullptr) {
        vxlog_error("Failed to create HarfBuzz buffer");
        return 0;
    }

    hb_feature_t features[] = {
        {HB_TAG('k', 'e', 'r', 'n'), 1, 0, (unsigned int)-1}, // Kerning
        {HB_TAG('l', 'i', 'g', 'a'), 1, 0, (unsigned int)-1}, // Ligatures
        {HB_TAG('c', 'l', 'i', 'g'), 1, 0, (unsigned int)-1}, // Contextual Ligatures
        {HB_TAG('e', 'm', 'o', 'j'), 1, 0, (unsigned int)-1}, // Emoji
        {HB_TAG('c', 'o', 'm', 'p'), 1, 0, (unsigned int)-1}  // Composition
    };

    hb_buffer_add_utf8(buffer, text, length, 0, length);
    hb_buffer_guess_segment_properties(buffer);
    hb_shape(fm->getHbFont(fonts[0]), buffer, features, 5);
    
    unsigned int glyphCount;
    const hb_glyph_info_t* glyphInfo = hb_buffer_get_glyph_infos(buffer, &glyphCount);
    const hb_glyph_position_t* glyphPos = hb_buffer_get_glyph_positions(buffer, nullptr);

    const FontInfo& primaryInfo = fm->getFontInfo(fonts[0]);
    const float space = primaryInfo.spaceWidth * scale + spacing;
    const float tab = 4 * primaryInfo.spaceWidth * scale + spacing;

    *glyphsOut = new GlyphEntry[length]; // Note: number of glyphs will be < length
    uint32_t shapedGlyphs = 0;

    float *fallbacksScale = nullptr;
    GlyphEntry *wordStart = nullptr;
    float wordSize = 0.0f, lineSize = 0.0f;
    for (unsigned int i = 0; i < glyphCount; ++i) {
        GlyphEntry entry = {
            glyphInfo[i].codepoint,
            glyphInfo[i].cluster,
            glyphPos[i].x_advance,
            glyphPos[i].y_advance,
            glyphPos[i].x_offset,
            glyphPos[i].y_offset,
            1.0f, false, 0
        };
        (*glyphsOut)[shapedGlyphs] = entry;

        const char c = text[entry.clusterIdx];

        if (c == '\n') {
            lineSize = 0.0f;
            wordSize = 0.0f;
            wordStart = nullptr;
            ++shapedGlyphs;
        } else if (c == ' ') {
            lineSize += wordSize + space;
            wordSize = 0.0f;
            wordStart = (*glyphsOut) + shapedGlyphs;
            ++shapedGlyphs;
        } else if (c == '\t') {
            lineSize += wordSize + tab;
            wordSize = 0.0f;
            wordStart = (*glyphsOut) + shapedGlyphs;
            ++shapedGlyphs;
        } else if (entry.glyphIdx == 0 && fontCount > 1) {
            // if not found in primary font, try to re-shape the cluster using the fallback fonts
            // Note: backtracking isn't implemented (would need to remove previously added glyphs),
            // in case of ZWJ sequence, but this shouldn't be needed if only one font supports emojis
            uint32_t startIdx = i, endIdx = i;
            while (startIdx > 0 && glyphInfo[startIdx - 1].cluster == glyphInfo[i].cluster) {
                --startIdx;
            }
            while (endIdx + 1 < glyphCount && glyphInfo[endIdx + 1].cluster == glyphInfo[i].cluster) {
                ++endIdx;
            }

            uint32_t startByte = glyphInfo[startIdx].cluster;
            uint32_t endByte = endIdx + 1 < glyphCount ? glyphInfo[endIdx + 1].cluster : length;
            if (startByte > endByte) { // HarfBuzz cluster values may not be strictly increasing
                std::swap(startByte, endByte);
            }

            uint32_t byteLen = endByte - startByte;
            while (startByte + byteLen < length &&
                   (text[startByte + byteLen] & UNICODE_CONTINUATION_MASK) == UNICODE_CONTINUATION_VALUE) {
                ++byteLen;  // continue until end of current UTF-8 character
            }

            if (fallbacksScale == nullptr) {
                fallbacksScale = static_cast<float*>(malloc(sizeof(float) * (fontCount - 1)));
                for (uint8_t j = 1; j < fontCount; ++j) {
                    const FontInfo& fallbackInfo = fm->getFontInfo(fonts[j]);
                    fallbacksScale[j - 1] = (primaryInfo.ascender - primaryInfo.descender) / (fallbackInfo.ascender - fallbackInfo.descender);
                }
            }

            for (uint8_t j = 1; j < fontCount; ++j) {
                hb_buffer_t *fallbackBuffer = hb_buffer_create();
                hb_buffer_add_utf8(fallbackBuffer, text + startByte, byteLen, 0, byteLen);
                hb_buffer_guess_segment_properties(fallbackBuffer);
                hb_shape(fm->getHbFont(fonts[j]), fallbackBuffer, features, 5);

                unsigned int fallbackGlyphCount;
                const hb_glyph_info_t* fallbackGlyphInfo = hb_buffer_get_glyph_infos(fallbackBuffer, &fallbackGlyphCount);
                const hb_glyph_position_t* fallbackGlyphPos = hb_buffer_get_glyph_positions(fallbackBuffer, nullptr);

                // find first fallback font that supports at least first glyph in cluster
                if (fallbackGlyphCount == 0 || fallbackGlyphInfo[0].codepoint == 0) {
                    hb_buffer_destroy(fallbackBuffer);
                    continue;
                }

                for (unsigned int k = 0; k < fallbackGlyphCount; ++k) {
                    entry = {
                        fallbackGlyphInfo[k].codepoint,
                        glyphInfo[startIdx].cluster + fallbackGlyphInfo[k].cluster,
                        fallbackGlyphPos[k].x_advance,
                        fallbackGlyphPos[k].y_advance,
                        fallbackGlyphPos[k].x_offset,
                        fallbackGlyphPos[k].y_offset,
                        fallbacksScale[j - 1],
                        false, j
                    };
                    (*glyphsOut)[shapedGlyphs] = entry;

                    const bool firstWord = lineSize == 0.0f;
                    const float advanceX = spacing + entry.advanceX / PACK_26_6_PIXEL_FORMAT * scale * fallbacksScale[j - 1];

                    if (lineSize + wordSize + advanceX > maxWidth) {
                        if (firstWord) { // break in place
                            fifo_list_push(newLinesOut, (*glyphsOut) + shapedGlyphs);
                            wordSize = 0.0f;
                        } else { // replace previous space w/ a break line
                            vx_assert(wordStart != nullptr);
                            wordStart->isNewLineReplacement = true;
                            lineSize = 0.0f;
                        }
                        wordStart = nullptr;
                    }
                    wordSize += advanceX;
                    ++shapedGlyphs;
                }

                i = endIdx;
                hb_buffer_destroy(fallbackBuffer);
                break;
            }
        } else {
            const bool firstWord = lineSize == 0.0f;
            const float advanceX = spacing + entry.advanceX / PACK_26_6_PIXEL_FORMAT * scale;

            if (lineSize + wordSize + advanceX > maxWidth) {
                if (firstWord) { // break in place
                    fifo_list_push(newLinesOut, (*glyphsOut) + shapedGlyphs);
                    wordSize = 0.0f;
                } else { // replace previous space w/ a break line
                    vx_assert(wordStart != nullptr);
                    wordStart->isNewLineReplacement = true;
                    lineSize = 0.0f;
                }
                wordStart = nullptr;
            }
            wordSize += advanceX;
            ++shapedGlyphs;
        }
    }
    hb_buffer_destroy(buffer);

    if (fallbacksScale != nullptr) {
        free(fallbacksScale);
    }

    return shapedGlyphs;
}

void TextBuffer::appendText(const FontHandle *fonts, uint8_t fontCount, const char* text, const char* end,
                            float scale, float maxWidth, float spacing) {
    if (text == nullptr || fonts == nullptr || fontCount == 0) {
        return;
    }

    const uint32_t length = static_cast<uint32_t>(bx::strLen(text));
    if (end == nullptr) {
        end = text + length;
    }
    vx_assert(end >= text);

    const FontInfo& primaryInfo = _fontManager->getFontInfo(fonts[0]);
    const float newLineHeight = (primaryInfo.lineGap + primaryInfo.ascender - primaryInfo.descender) * scale;

    // reset state if buffer is empty
    if (_vertexCount == 0) {
        _originX = _penX;
        _originY = _penY;
        _rectangle.x0 = _rectangle.x1 = _originX;
        _rectangle.y0 = _originY;
        _rectangle.y1 = _originY + (primaryInfo.ascender - primaryInfo.descender) * scale;
        _slantMaxOffset = FONT_SLANT_MAX_PERCENT * (primaryInfo.ascender - primaryInfo.descender) * scale;
    }

    if (length == 0) {
        return;
    }

    // multi-line break enabled
    if (maxWidth > 0.0f) {
        // first pass, decode the whole string & find word breaks
        FifoList *newLines = fifo_list_new();
        GlyphEntry *glyphs = nullptr;
        const uint32_t nbGlyphs = TextBuffer::decodeMultilineText(fonts, fontCount, _fontManager, text, end, scale, spacing, maxWidth, &glyphs, newLines);

        // second pass, append glyphs and insert new line breaks
        GlyphEntry *lineBreak = static_cast<GlyphEntry *>(fifo_list_pop(newLines));
        for (uint32_t i = 0; i < nbGlyphs ; ++i) {
            const GlyphEntry& entry = glyphs[i];
            const char c = text[entry.clusterIdx];

            // append or insert new line
            if (c == '\n' || entry.isNewLineReplacement || lineBreak == &entry) {
                _penX = _originX;
                _penY += newLineHeight;
                _rectangle.y1 += newLineHeight;
                _alignmentMap.push_back({static_cast<uint16_t>(_vertexCount > 0 ? _vertexCount - 1 : 0), -1.0f });
                if (lineBreak == &entry) {
                    lineBreak = static_cast<GlyphEntry *>(fifo_list_pop(newLines));
                } else {
                    continue;
                }
            }

            // spaces do not need to be a glyph, we just advance
            if (c == ' ') {
                _penX += primaryInfo.spaceWidth * scale + spacing;
                _rectangle.x1 = fmaxf(_rectangle.x1, _penX);
                continue;
            }

            // append tabs as 4 spaces
            if (c == '\t') {
                _penX += 4 * primaryInfo.spaceWidth * scale + spacing;
                _rectangle.x1 = fmaxf(_rectangle.x1, _penX);
                continue;
            }

            // anything that is not '\n', ' ', '\t' may be a glyph
            appendGlyph(fonts[entry.fontIdx], entry.glyphIdx,
                        { entry.advanceX, entry.advanceY, entry.offsetX, entry.offsetY, 0 },
                        scale * entry.scale, spacing);
        }

        fifo_list_free(newLines, nullptr);
        free(glyphs);
    }
    // single pass if multi-line break disabled
    else {
        hb_buffer_t* buffer = hb_buffer_create();
        if (buffer == nullptr) {
            vxlog_error("Failed to create HarfBuzz buffer");
            return;
        }

        hb_feature_t features[] = {
            {HB_TAG('k', 'e', 'r', 'n'), 1, 0, (unsigned int)-1}, // Kerning
            {HB_TAG('l', 'i', 'g', 'a'), 1, 0, (unsigned int)-1}, // Ligatures
            {HB_TAG('c', 'l', 'i', 'g'), 1, 0, (unsigned int)-1}, // Contextual Ligatures
            {HB_TAG('e', 'm', 'o', 'j'), 1, 0, (unsigned int)-1}, // Emoji
            {HB_TAG('c', 'o', 'm', 'p'), 1, 0, (unsigned int)-1}  // Composition
        };

        hb_buffer_add_utf8(buffer, text, length, 0, length);
        hb_buffer_guess_segment_properties(buffer);
        hb_shape(_fontManager->getHbFont(fonts[0]), buffer, features, 5);
        
        unsigned int glyphCount;
        const hb_glyph_info_t* glyphInfo = hb_buffer_get_glyph_infos(buffer, &glyphCount);
        const hb_glyph_position_t* glyphPos = hb_buffer_get_glyph_positions(buffer, nullptr);

        float *fallbacksScale = nullptr;
        for (unsigned int i = 0; i < glyphCount; ++i) {
            const char c = text[glyphInfo[i].cluster];

            // append new line
            if (c == '\n') {
                _penX = _originX;
                _penY += newLineHeight;
                _rectangle.y1 += newLineHeight;
                _alignmentMap.push_back({static_cast<uint16_t>(_vertexCount > 0 ? _vertexCount - 1 : 0), -1.0f });
                continue;
            }

            // spaces do not need to be a glyph, we just advance
            if (c == ' ') {
                _penX += primaryInfo.spaceWidth * scale + spacing;
                _rectangle.x1 = fmaxf(_rectangle.x1, _penX);
                continue;
            }

            // append tabs as 4 spaces
            if (c == '\t') {
                _penX += 4 * primaryInfo.spaceWidth * scale + spacing;
                _rectangle.x1 = fmaxf(_rectangle.x1, _penX);
                continue;
            }

            const hb_codepoint_t glyphid = glyphInfo[i].codepoint;

            // if not found in primary font, try to re-shape the cluster using the fallback fonts
            if (glyphid == 0) {
                uint32_t startIdx = i, endIdx = i;
                while (startIdx > 0 && glyphInfo[startIdx - 1].cluster == glyphInfo[i].cluster) {
                    --startIdx;
                }
                while (endIdx + 1 < glyphCount && glyphInfo[endIdx + 1].cluster == glyphInfo[i].cluster) {
                    ++endIdx;
                }

                uint32_t startByte = glyphInfo[startIdx].cluster;
                uint32_t endByte = endIdx + 1 < glyphCount ? glyphInfo[endIdx + 1].cluster : length;

                if (startByte > endByte) { // HarfBuzz cluster values may not be strictly increasing
                    std::swap(startByte, endByte);
                }

                uint32_t byteLen = endByte - startByte;
                while (startByte + byteLen < length &&
                    (text[startByte + byteLen] & UNICODE_CONTINUATION_MASK) == UNICODE_CONTINUATION_VALUE) {
                    ++byteLen;  // continue until end of current UTF-8 character
                }

                if (fallbacksScale == nullptr) {
                    fallbacksScale = static_cast<float*>(malloc(sizeof(float) * (fontCount - 1)));
                    for (uint8_t j = 1; j < fontCount; ++j) {
                        const FontInfo& fallbackInfo = _fontManager->getFontInfo(fonts[j]);
                        fallbacksScale[j - 1] = (primaryInfo.ascender - primaryInfo.descender) / (fallbackInfo.ascender - fallbackInfo.descender);
                    }
                }

                for (uint8_t j = 1; j < fontCount; ++j) {
                    hb_buffer_t *fallbackBuffer = hb_buffer_create();
                    hb_buffer_add_utf8(fallbackBuffer, text + startByte, byteLen, 0, byteLen);
                    hb_buffer_guess_segment_properties(fallbackBuffer);
                    hb_shape(_fontManager->getHbFont(fonts[j]), fallbackBuffer, features, 5);

                    unsigned int fallbackGlyphCount;
                    const hb_glyph_info_t* fallbackGlyphInfo = hb_buffer_get_glyph_infos(fallbackBuffer, &fallbackGlyphCount);
                    const hb_glyph_position_t* fallbackGlyphPos = hb_buffer_get_glyph_positions(fallbackBuffer, nullptr);

                    // find first fallback font that supports at least first glyph in cluster
                    if (fallbackGlyphCount == 0 || fallbackGlyphInfo[0].codepoint == 0) {
                        hb_buffer_destroy(fallbackBuffer);
                        continue;
                    }

                    for (unsigned int k = 0; k < fallbackGlyphCount; ++k) {
                        appendGlyph(fonts[j], fallbackGlyphInfo[k].codepoint, fallbackGlyphPos[k], scale * fallbacksScale[j - 1], spacing);
                    }

                    i = endIdx;
                    hb_buffer_destroy(fallbackBuffer);
                    break;
                }
            } else {
                // anything that is not '\n', ' ', '\t' may be a glyph
                appendGlyph(fonts[0], glyphid, glyphPos[i], scale, spacing);
            }
        }
        hb_buffer_destroy(buffer);
    }

    // final end-of-line character, may be used for text alignment
    _alignmentMap.push_back({ static_cast<uint16_t>(_vertexCount > 0 ? _vertexCount - 1 : 0), -1.0f });

    _slant = -1.0f;
}

void TextBuffer::appendAtlasFace(uint16_t faceIndex, bool filtering) {
	if(_vertexCount / 4 >= MAX_BUFFERED_CHARACTERS) {
		return;
	}

	float x0 = _penX;
	float y0 = _penY;
	float x1 = x0 + static_cast<float>(_fontManager->getAtlas(filtering)->getTextureSize());
	float y1 = y0 + static_cast<float>(_fontManager->getAtlas(filtering)->getTextureSize());

	_fontManager->getAtlas()->packFaceLayerUV(faceIndex,
		reinterpret_cast<uint8_t*>(_vertexBuffer),
		sizeof(TextVertex) * _vertexCount + offsetof(TextVertex, u),
		sizeof(TextVertex),
		true,
        filtering);

	setVertex(_vertexCount + 0, x0, y0, ATLAS_BACKGROUND_COLOR);
	setVertex(_vertexCount + 1, x0, y1, ATLAS_BACKGROUND_COLOR);
	setVertex(_vertexCount + 2, x1, y1, ATLAS_BACKGROUND_COLOR);
	setVertex(_vertexCount + 3, x1, y0, ATLAS_BACKGROUND_COLOR);

	_indexBuffer[_indexCount + 0] = _vertexCount + 0;
	_indexBuffer[_indexCount + 1] = _vertexCount + 1;
	_indexBuffer[_indexCount + 2] = _vertexCount + 2;
	_indexBuffer[_indexCount + 3] = _vertexCount + 0;
	_indexBuffer[_indexCount + 4] = _vertexCount + 2;
	_indexBuffer[_indexCount + 5] = _vertexCount + 3;
	_vertexCount += 4;
	_indexCount += 6;
}

void TextBuffer::clearTextBuffer() {
	_penX = 0;
	_penY = 0;
	_originX = 0;
	_originY = 0;
	_vertexCount = 0;
	_indexCount = 0;
	_rectangle = { -1, -1, -1, -1 };
    _alignmentMap.clear();
    _alignment = TextAlignment_Left;
}

const uint8_t* TextBuffer::getVertexBuffer() {
	return reinterpret_cast<uint8_t*>(_vertexBuffer);
}

uint32_t TextBuffer::getVertexCount() const {
	return _vertexCount;
}

uint32_t TextBuffer::getVertexSize() const {
	return sizeof(TextVertex);
}

const uint16_t* TextBuffer::getIndexBuffer() const {
	return _indexBuffer;
}

uint32_t TextBuffer::getIndexCount() const {
	return _indexCount;
}

uint32_t TextBuffer::getIndexSize() const {
	return sizeof(uint16_t);
}

const TextRectangle* TextBuffer::getRectangle() const {
	return &_rectangle;
}

void TextBuffer::appendGlyph(FontHandle font, hb_codepoint_t glyphIdx, hb_glyph_position_t glyphPos, float scale, float spacing) {
    // early out if we reached max buffer size
    if(_vertexCount / 4 >= MAX_BUFFERED_CHARACTERS) {
        return;
    }

    const FontInfo& info = _fontManager->getFontInfo(font);

    const GlyphInfo *glyph = _fontManager->getGlyphInfo(font, glyphIdx);
    if (glyph == nullptr || glyph->width == 0.0f) {
        return;
    }

    _penX += spacing;

	// position quad accounting for bitmap offsets (glyph), text shaping offsets (glyphPos), and scale
    const float x0 = _penX + (glyph->offset_x + glyphPos.x_offset / PACK_26_6_PIXEL_FORMAT) * scale;
    const float y0 = _penY + ((_penY == _originY ? 0.0f : info.lineGap) + info.ascender + glyph->offset_y + glyphPos.y_offset / PACK_26_6_PIXEL_FORMAT) * scale;
    const float x1 = (x0 + glyph->width * scale);
    const float y1 = (y0 + glyph->height * scale);

	const Atlas* atlas = _fontManager->getAtlas(glyph->filtering);
    atlas->packUV(glyph->regionIndex, reinterpret_cast<uint8_t*>(_vertexBuffer),
				  sizeof(TextVertex) * _vertexCount + offsetof(TextVertex, u),
                  sizeof(TextVertex), glyph->colored, glyph->filtering);

	setVertex(_vertexCount + 0, x0, y0, _textColor);
	setVertex(_vertexCount + 1, x0, y1, _textColor);
	setVertex(_vertexCount + 2, x1, y1, _textColor);
	setVertex(_vertexCount + 3, x1, y0, _textColor);

	_indexBuffer[_indexCount + 0] = _vertexCount + 0;
	_indexBuffer[_indexCount + 1] = _vertexCount + 1;
	_indexBuffer[_indexCount + 2] = _vertexCount + 2;
	_indexBuffer[_indexCount + 3] = _vertexCount + 0;
	_indexBuffer[_indexCount + 4] = _vertexCount + 2;
	_indexBuffer[_indexCount + 5] = _vertexCount + 3;
	_vertexCount += 4;
	_indexCount += 6;

    _penX += glyphPos.x_advance / PACK_26_6_PIXEL_FORMAT * scale;
    _rectangle.x1 = fmaxf(_rectangle.x1, _penX);
}

void TextBuffer::setVertex(uint32_t i, float x, float y, uint32_t rgba) {
	_vertexBuffer[i].x = x;
	_vertexBuffer[i].y = y;
	_vertexBuffer[i].rgba = rgba;
}

TextBufferManager::TextBufferManager(FontManager* fm)
	: _fontManager(fm) {

	_textBuffers = new BufferCache[MAX_TEXT_BUFFER_COUNT];
	_vertexLayout
		.begin()
		.add(bgfx::Attrib::Position,  2, bgfx::AttribType::Float)
		.add(bgfx::Attrib::TexCoord0, 4, bgfx::AttribType::Int16, true)
		.add(bgfx::Attrib::Color0,    4, bgfx::AttribType::Uint8, true)
		.end();
	_sAtlas = bgfx::createUniform("s_atlas", bgfx::UniformType::Sampler);
    _sPointAtlas = bgfx::createUniform("s_atlasPoint", bgfx::UniformType::Sampler);
}

TextBufferManager::~TextBufferManager() {
	if (_textBufferHandles.getNumHandles() > 0) {
		vxlog_error("All the text buffers must be destroyed before destroying the manager");
	}
	delete [] _textBuffers;

	bgfx::destroy(_sAtlas);
    bgfx::destroy(_sPointAtlas);
}

TextBufferHandle TextBufferManager::createTextBuffer(BufferType::Enum bufferType) {
	uint16_t textIdx = _textBufferHandles.alloc();
	BufferCache& bc = _textBuffers[textIdx];

	bc.textBuffer = new TextBuffer(_fontManager);
	bc.bufferType = bufferType;
	bc.indexBufferHandleIdx = bgfx::kInvalidHandle;
	bc.vertexBufferHandleIdx = bgfx::kInvalidHandle;

	TextBufferHandle ret = {textIdx};
	return ret;
}

void TextBufferManager::destroyTextBuffer(TextBufferHandle tbh) {
    vx_assert(isValid(tbh));

	BufferCache& bc = _textBuffers[tbh.idx];
	_textBufferHandles.free(tbh.idx);
	delete bc.textBuffer;
	bc.textBuffer = nullptr;

	if (bc.vertexBufferHandleIdx == bgfx::kInvalidHandle) {
		return;
	}

	switch (bc.bufferType) {
		case BufferType::Static: {
			bgfx::IndexBufferHandle ibh;
			bgfx::VertexBufferHandle vbh;
			ibh.idx = bc.indexBufferHandleIdx;
			vbh.idx = bc.vertexBufferHandleIdx;
			bgfx::destroy(ibh);
			bgfx::destroy(vbh);
		}
			break;
		case BufferType::Dynamic: {
			bgfx::DynamicIndexBufferHandle ibh;
			bgfx::DynamicVertexBufferHandle vbh;
			ibh.idx = bc.indexBufferHandleIdx;
			vbh.idx = bc.vertexBufferHandleIdx;
			bgfx::destroy(ibh);
			bgfx::destroy(vbh);
		}
			break;
		case BufferType::Transient: // destroyed every frame
			break;
	}
}

void TextBufferManager::submitTextBuffer(TextBufferHandle tbh, bgfx::ViewId id,
                                         bgfx::ProgramHandle program, uint32_t sortOrder,
                                         uint64_t state, bgfx::UniformHandle uh, float *uData) {
    vx_assert(isValid(tbh));

	BufferCache& bc = _textBuffers[tbh.idx];

	uint32_t indexSize  = bc.textBuffer->getIndexCount()  * bc.textBuffer->getIndexSize();
	uint32_t vertexSize = bc.textBuffer->getVertexCount() * bc.textBuffer->getVertexSize();

	if (0 == indexSize || 0 == vertexSize) {
		return;
	}

	bgfx::setTexture(0, _sAtlas, _fontManager->getAtlas(true)->getTextureHandle());
    bgfx::setTexture(1, _sPointAtlas, _fontManager->getAtlas(false)->getTextureHandle());
	if (bgfx::isValid(uh)) {
		bgfx::setUniform(uh, uData);
	}

	switch (bc.bufferType) {
	case BufferType::Static: {
			bgfx::IndexBufferHandle ibh;
			bgfx::VertexBufferHandle vbh;

			if (bgfx::kInvalidHandle == bc.vertexBufferHandleIdx) {
				ibh = bgfx::createIndexBuffer(
								bgfx::copy(bc.textBuffer->getIndexBuffer(), indexSize)
								);

				vbh = bgfx::createVertexBuffer(
						bgfx::copy(bc.textBuffer->getVertexBuffer(), vertexSize)
								, _vertexLayout
								);

				bc.vertexBufferHandleIdx = vbh.idx;
				bc.indexBufferHandleIdx = ibh.idx;
			} else {
				vbh.idx = bc.vertexBufferHandleIdx;
				ibh.idx = bc.indexBufferHandleIdx;
			}

			bgfx::setVertexBuffer(0, vbh, 0, bc.textBuffer->getVertexCount() );
			bgfx::setIndexBuffer(ibh, 0, bc.textBuffer->getIndexCount() );
		}
		break;
	case BufferType::Dynamic: {
			bgfx::DynamicIndexBufferHandle ibh;
			bgfx::DynamicVertexBufferHandle vbh;

			if (bgfx::kInvalidHandle == bc.vertexBufferHandleIdx) {
				ibh = bgfx::createDynamicIndexBuffer(
								bgfx::copy(bc.textBuffer->getIndexBuffer(), indexSize)
								);

				vbh = bgfx::createDynamicVertexBuffer(
						bgfx::copy(bc.textBuffer->getVertexBuffer(), vertexSize)
								, _vertexLayout
								);

				bc.indexBufferHandleIdx = ibh.idx;
				bc.vertexBufferHandleIdx = vbh.idx;
			} else {
				ibh.idx = bc.indexBufferHandleIdx;
				vbh.idx = bc.vertexBufferHandleIdx;

				bgfx::update(
					  ibh
					, 0
					, bgfx::copy(bc.textBuffer->getIndexBuffer(), indexSize)
					);

				bgfx::update(
					  vbh
					, 0
					, bgfx::copy(bc.textBuffer->getVertexBuffer(), vertexSize)
					);
			}

			bgfx::setVertexBuffer(0, vbh, 0, bc.textBuffer->getVertexCount() );
			bgfx::setIndexBuffer(ibh, 0, bc.textBuffer->getIndexCount() );
		}
		break;

	case BufferType::Transient: {
			bgfx::TransientIndexBuffer tib;
			bgfx::TransientVertexBuffer tvb;
			bgfx::allocTransientIndexBuffer(&tib, bc.textBuffer->getIndexCount() );
			bgfx::allocTransientVertexBuffer(&tvb, bc.textBuffer->getVertexCount(), _vertexLayout);
			bx::memCopy(tib.data, bc.textBuffer->getIndexBuffer(), indexSize);
			bx::memCopy(tvb.data, bc.textBuffer->getVertexBuffer(), vertexSize);
			bgfx::setVertexBuffer(0, &tvb, 0, bc.textBuffer->getVertexCount() );
			bgfx::setIndexBuffer(&tib, 0, bc.textBuffer->getIndexCount() );
		}
		break;
	}

	if (state == BGFX_STATE_NONE) {
        state = BGFX_STATE_WRITE_RGB
                | BGFX_STATE_BLEND_FUNC(BGFX_STATE_BLEND_SRC_ALPHA, BGFX_STATE_BLEND_INV_SRC_ALPHA);
	}

	bgfx::setState(state);
	bgfx::submit(id, program, sortOrder);
}

void TextBufferManager::clearTextBuffer(TextBufferHandle tbh) {
    vx_assert(isValid(tbh));
    BufferCache& bc = _textBuffers[tbh.idx];
    bc.textBuffer->clearTextBuffer();
}

void TextBufferManager::setTextColor(TextBufferHandle tbh, uint32_t rbga) {
    vx_assert(isValid(tbh));
	BufferCache& bc = _textBuffers[tbh.idx];
	bc.textBuffer->setTextColor(rbga);
}

uint32_t TextBufferManager::getTextColor(TextBufferHandle tbh) {
	vx_assert(isValid(tbh));
	BufferCache& bc = _textBuffers[tbh.idx];
	return bc.textBuffer->getTextColor();
}

void TextBufferManager::applyColor(TextBufferHandle tbh) {
	vx_assert(isValid(tbh));
	BufferCache& bc = _textBuffers[tbh.idx];
	bc.textBuffer->applyColor();
}

void TextBufferManager::addTextOffset(TextBufferHandle tbh, float dx, float dy) {
    vx_assert(isValid(tbh));
	BufferCache& bc = _textBuffers[tbh.idx];
	bc.textBuffer->addOffset(dx, dy);
}

void TextBufferManager::setAlignment(TextBufferHandle tbh, TextAlignment value) {
    vx_assert(isValid(tbh));
    _textBuffers[tbh.idx].textBuffer->setAlignment(value);
}

float TextBufferManager::setSlant(TextBufferHandle tbh, float value) {
    vx_assert(isValid(tbh));
    return _textBuffers[tbh.idx].textBuffer->setSlant(value);
}

void TextBufferManager::setPenPosition(TextBufferHandle tbh, float x, float y) {
    vx_assert(isValid(tbh));
	BufferCache& bc = _textBuffers[tbh.idx];
	bc.textBuffer->setPenPosition(x, y);
}

void TextBufferManager::appendText(TextBufferHandle tbh, const FontHandle *fonts, uint8_t fontCount, const char *text,
								   const char *end, float scale, float maxWidth, float spacing) {
    vx_assert(isValid(tbh));
    BufferCache& bc = _textBuffers[tbh.idx];
    bc.textBuffer->appendText(fonts, fontCount, text, end, scale, maxWidth, spacing);
}

void TextBufferManager::appendAtlasFace(TextBufferHandle tbh, uint16_t faceIndex, bool filtering) {
    vx_assert(isValid(tbh));
	BufferCache& bc = _textBuffers[tbh.idx];
	bc.textBuffer->appendAtlasFace(faceIndex, filtering);
}

const TextRectangle* TextBufferManager::getRectangle(TextBufferHandle tbh) const {
    vx_assert(isValid(tbh));
	BufferCache& bc = _textBuffers[tbh.idx];
	return bc.textBuffer->getRectangle();
}

uint32_t TextBufferManager::getVertexCount(TextBufferHandle tbh) const {
    vx_assert(isValid(tbh));
    BufferCache& bc = _textBuffers[tbh.idx];
    return bc.textBuffer->getVertexCount();
}

float TextBufferManager::getCharacterSize(FontHandle fh, const char *text) const {
    if (text == nullptr || bx::strLen(text) == 0) {
        return 0.0f;
    }

    hb_buffer_t* buffer = hb_buffer_create();
    if (buffer == nullptr) {
        vxlog_error("Failed to create HarfBuzz buffer");
        return 0.0f;
    }

    hb_feature_t features[] = {
        {HB_TAG('k', 'e', 'r', 'n'), 1, 0, (unsigned int)-1}, // Kerning
        {HB_TAG('l', 'i', 'g', 'a'), 1, 0, (unsigned int)-1}, // Ligatures
        {HB_TAG('c', 'l', 'i', 'g'), 1, 0, (unsigned int)-1}, // Contextual Ligatures
        {HB_TAG('e','m','o','j'), 1, 0, (unsigned int)-1},    // Emoji
        {HB_TAG('c','o','m','p'), 1, 0, (unsigned int)-1}     // Composition
    };

    hb_buffer_add_utf8(buffer, text, -1, 0, -1);
    hb_buffer_guess_segment_properties(buffer);
    hb_shape(_fontManager->getHbFont(fh), buffer, features, 5);
    
    unsigned int glyphCount;
    const hb_glyph_info_t* glyphInfo = hb_buffer_get_glyph_infos(buffer, &glyphCount);
    const hb_glyph_position_t* glyphPos = hb_buffer_get_glyph_positions(buffer, nullptr);

    for (unsigned int i = 0; i < glyphCount; ++i) {
        if (glyphInfo[i].codepoint != 0) {
            return glyphPos[i].x_advance / PACK_26_6_PIXEL_FORMAT;
        }
    }

    return 0.0f;
}

size_t TextBufferManager::snapTextCursor(const FontHandle *fonts, uint8_t fontCount, const char *text, const char *end, float scale,
                                         float spacing, float maxWidth, float *inoutX, float *inoutY, size_t *inCharIndex) const {
    vx_assert(inoutX != nullptr && inoutY != nullptr);

    if (text == nullptr || fonts == nullptr || fontCount == 0) {
        return 0;
    }

    if (end == nullptr) {
        end = text + bx::strLen(text);
    }
    vx_assert(end >= text);

    const FontInfo& primaryInfo = _fontManager->getFontInfo(fonts[0]);

    // new line = line gap + font height
    // space = advance
    // tabs = 4 spaces
    const float newline = (primaryInfo.lineGap + primaryInfo.ascender - primaryInfo.descender) * scale;
    const float space = primaryInfo.spaceWidth * scale;
    const float tab = 4 * primaryInfo.spaceWidth * scale;

    if (*inoutY < 0.0f || (inCharIndex != nullptr && *inCharIndex == 0)) {
        *inoutX = 0.0f; // cursor at beginning of text
        *inoutY = newline;
        return 0;
    }

    // disable multi-line break,
    // 1) if max width isn't valid
    if (maxWidth <= 0.0f) {
        maxWidth = FLT_MAX;
    }
    // 2) if max width leaves no space for a single character
    if (primaryInfo.maxAdvanceWidth * scale > maxWidth) {
        maxWidth = FLT_MAX;
    }

    const float cursorX = *inoutX;
    const float cursorY = *inoutY;
    *inoutY = newline;

    // TODO: process is simpler if multi-line break is disabled
    // TODO: cache glyph entries?

    // first pass, decode the whole string & find word breaks
    FifoList *newLines = fifo_list_new();
    GlyphEntry *glyphs = nullptr;
    const uint32_t nbGlyphs = TextBuffer::decodeMultilineText(fonts, fontCount, _fontManager, text, end, scale, spacing, maxWidth, &glyphs, newLines);

    // second pass, find cursor accounting for inserted line breaks
    const uint32_t stopAtLine = static_cast<uint32_t>(floorf(cursorY / newline));
    uint32_t line = 0;
    float wordSize = 0.0f, lineSize = 0.0f;
    GlyphEntry *lineBreak = static_cast<GlyphEntry *>(fifo_list_pop(newLines));

    for (uint32_t i = 0; i < nbGlyphs ; ++i) {
        const GlyphEntry& entry = glyphs[i];
        const char c = text[entry.clusterIdx];

        if (&entry == lineBreak) {
            if (line == stopAtLine) {
                *inoutX = lineSize + wordSize; // cursor at end of line
                return entry.clusterIdx;
            }
            ++line;

            *inoutY += newline;
            lineSize = 0.0f;
            wordSize = 0.0f;

            lineBreak = static_cast<GlyphEntry *>(fifo_list_pop(newLines));
        }

        if (entry.glyphIdx != 0) {
            if (c == '\n') {
                if (inCharIndex != nullptr) {
                    if (*inCharIndex <= entry.clusterIdx) {
                        *inoutX = lineSize + wordSize; // cursor at end of line
                        return entry.clusterIdx;
                    }
                } else if (line == stopAtLine) {
                    *inoutX = lineSize + wordSize; // cursor at end of line
                    return entry.clusterIdx;
                }
                ++line;

                *inoutY += newline;
                lineSize = 0.0f;
                wordSize = 0.0f;
            } else if (c == ' ') {
                if (inCharIndex != nullptr) {
                    if (*inCharIndex <= entry.clusterIdx) {
                        *inoutX = lineSize + wordSize; // cursor before space
                        return entry.clusterIdx - 1;
                    }
                }
                else if (line == stopAtLine && lineSize + wordSize + space > cursorX) {
                    if (cursorX < lineSize + wordSize + space * 0.5f) {
                        *inoutX = lineSize + wordSize; // cursor before space
                        return entry.clusterIdx - 1;
                    } else {
                        *inoutX = lineSize + wordSize + space; // cursor after space
                        return entry.clusterIdx;
                    }
                }

                lineSize += wordSize + space;
                wordSize = 0.0f;
            } else if (c == '\t') {
                if (inCharIndex != nullptr) {
                    if (*inCharIndex <= entry.clusterIdx) {
                        *inoutX = lineSize + wordSize; // cursor before tab
                        return entry.clusterIdx - 1;
                    }
                } else if (line == stopAtLine && lineSize + wordSize + tab > cursorX) {
                    if (cursorX < lineSize + wordSize + tab * 0.5f) {
                        *inoutX = lineSize + wordSize; // cursor before tab
                        return entry.clusterIdx - 1;
                    } else {
                        *inoutX = lineSize + wordSize + tab; // cursor after tab
                        return entry.clusterIdx;
                    }
                }

                lineSize += wordSize + tab;
                wordSize = 0.0f;
            } else {
                const float advanceX = spacing + glyphs[i].advanceX / PACK_26_6_PIXEL_FORMAT * scale * entry.scale;
                const float totalLine = lineSize + wordSize + advanceX;
                if (inCharIndex != nullptr) {
                    if (*inCharIndex <= entry.clusterIdx) {
                        if (i > 0) {
                            *inoutX = lineSize + wordSize; // cursor before current glyph
                            return glyphs[i - 1].clusterIdx;
                        } else {
                            *inoutX = 0.0f;
                            return 0;
                        }
                    }
                } else if (line == stopAtLine && totalLine > cursorX) {
                    if (cursorX < totalLine - advanceX * 0.5f) {
                        if (i > 0) {
                            *inoutX = lineSize + wordSize; // cursor before current glyph
                            return glyphs[i - 1].clusterIdx;
                        } else {
                            *inoutX = 0.0f;
                            return 0;
                        }
                    } else {
                        *inoutX = totalLine; // cursor after current glyph
                        return entry.clusterIdx;
                    }
                }
                wordSize += advanceX;
            }
        }
    }

    *inoutX = lineSize + wordSize; // cursor at end of last line

    if (nbGlyphs == 0) {
        return 0;
    }
    return glyphs[nbGlyphs - 1].clusterIdx;
}

uint32_t TextBufferManager::RGBAtoABGR(uint32_t rgba) {
    return ( ((rgba >> 0) & 0xff) << 24)
           | (((rgba >> 8) & 0xff) << 16)
           | (((rgba >> 16) & 0xff) << 8)
           | (((rgba >> 24) & 0xff) << 0);
}

uint32_t TextBufferManager::BGRAtoRGBA(uint32_t rgba) {
    return ( ((rgba >> 0) & 0xff) << 16)
           | (((rgba >> 8) & 0xff) << 8)
           | (((rgba >> 16) & 0xff) << 0)
           | (((rgba >> 24) & 0xff) << 24);
}

uint32_t TextBufferManager::AtoR(uint32_t rgba) {
    return (((rgba >> 24) & 0xff) << 0);
}

#endif // P3S_CLIENT_HEADLESS
