//
//  VXFont.hpp
//  Cubzh
//
//  Created by Adrien Duermael on 2/18/20.
//

#pragma once

#ifdef P3S_CLIENT_HEADLESS

#include "HeadlessClient_ImGui.h"
#include <cstddef>

#else

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Weverything"
// bgfx+imgui
#include <imgui/bgfx-imgui.h>
#include <dear-imgui/imgui.h>
#include "BgfxText/font_manager.h"
#pragma GCC diagnostic pop

#endif

#include "VXNotifications.hpp"
#include "VXConfig.hpp"

namespace vx {

    typedef struct FontCuratedSizes {
        float body, title, tiny;
    } FontCuratedSizes;

    class Font final {
public:

    enum class Type {
        body,
        title,
        tiny,
    };

    struct TexChar {
        ImWchar32 c;
        int x;
        int y;
        int w;
        int h;
        int fontTexIndex;
    };

    static Font *shared();

    virtual ~Font();

    void loadFonts(unsigned char ** const out_pixels,
                   int * const out_width,
                   int * const out_height);

    void pushImFont(const Type fontType);
    void popImFont() const;

    ImVec2 calcImFontTextSize(const Type fontType,
                              const char* text,
                              const float width = -1.0f) const;
    ImVec2 calcImFontTextSize(const Type fontType,
                              const char* textBegin,
                              const char* textEnd) const;

    float getImFontCharAdvanceX(const Type fontType,
                                const unsigned int c) const;

    size_t getImFontFirstWarpPart(const Type fontType,
                                  const char *start,
                                  const char *end,
                                  const char **cut,
                                  const float width) const;

#ifndef P3S_CLIENT_HEADLESS
    FontManager *getWorldFontManager() const;
    FontHandle getFont(FontName f) const;
#endif

    float getScaleForFontType(const FontName font, const Type& fontType) const;
    float getSizeForFontType(const FontName font, const Type& fontType) const;

    void computeFontScales();
    std::string addLineBreaks(const char* txt, const float width);

private:

    static Font *_sharedInstance;

    Font();

    ImFont *_imguiFont;

    FontCuratedSizes _fontCuratedSizes[FontName_COUNT];

#ifndef P3S_CLIENT_HEADLESS
    FontManager *_worldFontManager;
    FontHandle _worldFonts[FontName_COUNT];
#endif
};

}
