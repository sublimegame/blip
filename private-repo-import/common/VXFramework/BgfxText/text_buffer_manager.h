#ifndef P3S_CLIENT_HEADLESS

#pragma once

#include "font_manager.h"
#include "world_text.h"

BGFX_HANDLE(TextBufferHandle)

#define MAX_TEXT_BUFFER_COUNT (4<<10)
#define ATLAS_BACKGROUND_COLOR 0x000000FF
#define FONT_SLANT_MAX_PERCENT 0.5f

/// type of vertex and index buffer to use with a TextBuffer
struct BufferType {
	enum Enum {
		Static,
		Dynamic,
		Transient,
	};
};

struct TextRectangle {
    float x0, y0, x1, y1;
};

class TextBuffer;
class TextBufferManager {
public:
	TextBufferManager(FontManager* fm);
	~TextBufferManager();

	TextBufferHandle createTextBuffer(BufferType::Enum bufferType);
	void destroyTextBuffer(TextBufferHandle tbh);
	void submitTextBuffer(TextBufferHandle tbh, bgfx::ViewId id, bgfx::ProgramHandle program,
						  uint32_t sortOrder=0, uint64_t state=BGFX_STATE_NONE,
						  bgfx::UniformHandle uh=BGFX_INVALID_HANDLE, float *uData=nullptr);
    void clearTextBuffer(TextBufferHandle tbh);

	void setTextColor(TextBufferHandle tbh, uint32_t rbga=0x000000FF);
	uint32_t getTextColor(TextBufferHandle tbh);
	void applyColor(TextBufferHandle tbh);
	void addTextOffset(TextBufferHandle tbh, float dx, float dy);
    void setAlignment(TextBufferHandle tbh, TextAlignment value);
    float setSlant(TextBufferHandle tbh, float value);

	void setPenPosition(TextBufferHandle tbh, float x, float y);

	/// Append an ASCII/utf-8 string to the buffer using current pen position and color
	void appendText(TextBufferHandle tbh, const FontHandle *fonts, uint8_t fontCount, const char *text,
					const char *end=nullptr, float scale=1.0f, float maxWidth=0.0f, float spacing=0.0f);

	/// Append a whole face of the atlas cube, mostly used for debugging and visualizing atlas
	void appendAtlasFace(TextBufferHandle tbh, uint16_t faceIndex, bool filtering=true);

	/// Return the rectangular size of the current text buffer
	const TextRectangle* getRectangle(TextBufferHandle tbh) const;

    uint32_t getVertexCount(TextBufferHandle tbh) const;

    float getCharacterSize(FontHandle fh, const char *text) const;

    /// Stops at given position and snap to current glyph start point
    /// @param inCharIndex considered instead of inOutX & inoutY if not NULL.
    /// @returns number of utf8 characters decoded
    size_t snapTextCursor(const FontHandle *fonts, uint8_t fontCount, const char *text, const char *end, float scale,
                          float spacing, float maxWidth, float *inoutX, float *inoutY, size_t *inCharIndex) const;

	static uint32_t RGBAtoABGR(uint32_t rgba);
	static uint32_t BGRAtoRGBA(uint32_t rgba);
    static uint32_t AtoR(uint32_t rgba);
private:
	struct BufferCache {
		uint16_t indexBufferHandleIdx;
		uint16_t vertexBufferHandleIdx;
		TextBuffer* textBuffer;
		BufferType::Enum bufferType;
	};

	BufferCache* _textBuffers;
	bx::HandleAllocT<MAX_TEXT_BUFFER_COUNT> _textBufferHandles;
	FontManager* _fontManager;
	bgfx::VertexLayout _vertexLayout;
	bgfx::UniformHandle _sAtlas, _sPointAtlas;
};

#endif // P3S_CLIENT_HEADLESS
