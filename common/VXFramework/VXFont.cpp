//
//  VXFont.cpp
//  Cubzh
//
//  Created by Adrien Duermael on 2/18/20.
//

#include "VXFont.hpp"

// C++
#include <cmath>
#include <cstring>

#include "VXConfig.hpp"
#include "VXNode.hpp"

// xptools
#include "xptools.h"
#include "vxlog.h"

#ifndef P3S_CLIENT_HEADLESS
#include "imgui_internal.h"
#define STB_IMAGE_IMPLEMENTATION // TODO: why is this necessary? one is already declared inside bgfx/bimg/src/image_decode.cpp
#include "../bgfx/bimg/3rdparty/stb/stb_image.h"
#endif

using namespace vx;

// https://github.com/ocornut/imgui/blob/master/docs/FONTS.txt

//
// MARK: - Static -
//

Font *Font::_sharedInstance = nullptr;

Font *Font::shared() {
    if (Font::_sharedInstance == nullptr) {
        Font::_sharedInstance = new Font();
    }
    return Font::_sharedInstance;
}



// --------------------------------------------------
//
// MARK: - Constructor / Destructor -
//
// --------------------------------------------------

Font::Font() :
    _imguiFont(nullptr) {
    
#ifndef P3S_CLIENT_HEADLESS
    _worldFontManager = nullptr;
#endif
}

Font::~Font() {
#ifndef P3S_CLIENT_HEADLESS
    for (int font = 0; font < FontName_COUNT; ++font) {
        _worldFontManager->destroyFont(_worldFonts[font]);
    }
    delete _worldFontManager;
#endif
}



// --------------------------------------------------
//
// MARK: - Public -
//
// --------------------------------------------------

void Font::loadFonts(unsigned char ** const out_pixels,
                     int * const out_width,
                     int * const out_height) {
#ifndef P3S_CLIENT_HEADLESS

    ImGuiIO &io = ImGui::GetIO();
    
    // -----------------------------------
    // BODY
    // -----------------------------------
    
    FILE *coordsFile = fs::openBundleFile("fonts/pixel.txt");

    int bodyNbChars = 1;
    
    while(!feof(coordsFile)) {
        if(fgetc(coordsFile) == '\n') {
            bodyNbChars++;
        }
    }

    const size_t bufferSize = sizeof(TexChar) * bodyNbChars;
    TexChar *bodyChars = static_cast<TexChar *>(malloc(bufferSize));
    memset(bodyChars, 0, bufferSize); // fill buffer with zeros

    fseek(coordsFile, 0, SEEK_SET);
    
    char* coords = fs::getFileTextContentAndClose(coordsFile);
    char* token = strtok(coords, " \n");
    
    int i = 0;
    int charIndex = 0;
    
    // DEBUG
    //vxlog_debug("\n----- BODY CHARS -----");
    //vxlog_debug("NB: %d", bodyNbChars);
    //vxlog_debug("CHARS:");
    
    while (token != nullptr) {
        
        switch (i) {
            case 0:
                ImTextCharFromUtf8(&(bodyChars[charIndex].c), token, nullptr);
                //vxlog_debug("%u (%s) ", bodyChars[charIndex].c, token);
                break;
            case 1:
                bodyChars[charIndex].x = static_cast<int>(strtol(token, nullptr, 10));
                // vxlog_debug("(%d ,", bodyChars[charIndex].x);
                break;
            case 2:
                bodyChars[charIndex].y = static_cast<int>(strtol(token, nullptr, 10));
                // vxlog_debug("%d) ", bodyChars[charIndex].y);
                break;
            case 3:
                bodyChars[charIndex].w = static_cast<int>(strtol(token, nullptr, 10));
                // vxlog_debug("w:%d, ", bodyChars[charIndex].w);
                break;
            case 4:
                bodyChars[charIndex].h = static_cast<int>(strtol(token, nullptr, 10));
                // vxlog_debug("h:%d", bodyChars[charIndex].w);
                break;
            default:
                break;
        }
        
        token = strtok(nullptr, " \n");
        i++;
        if (i % 5 == 0) {
            i = 0;
            charIndex++;
        }
    }
    
    // DEBUG
    // vxlog_debug("----------------------");
    
    free(coords);
    
    FILE *file = fs::openBundleFile("fonts/pixel.png");
    int image_width = 0;
    int image_height = 0;
    stbi_uc *image_data = stbi_load_from_file(file, &image_width, &image_height, nullptr, 4);
    
    fclose(file);

    size_t ttfDataSize[FontName_COUNT] = { 0, 0, 0 };
    void *ttfData[FontName_COUNT] = { nullptr, nullptr, nullptr };
    for (int font = 0; font < FontName_COUNT; ++font) {
        FILE *ttfPtr = fs::openBundleFile(s_fontTTF[font]);
        if (ttfPtr == nullptr) {
            vxlog_error("[Font::loadFonts] 🔥 failed to open font TTF");
            free(bodyChars);
            stbi_image_free(image_data);
            return;
        }

        ttfData[font] = fs::getFileContent(ttfPtr, &ttfDataSize[font]); // file closed within getFileContent
    }

    if (DEARIMGUI_FONT == FontName_Pixel) {
        // Note: completely overwritten by our custom font, but we can't create a Dear-Imgui font w/o TTF
        FILE *ttfPtr_Monogram = fs::openBundleFile("fonts/monogram_extended.ttf");
        if (ttfPtr_Monogram == nullptr) {
            vxlog_error("[Font::loadFonts] 🔥 failed to open font monogram_extended.ttf");
            free(bodyChars);
            stbi_image_free(image_data);
            return;
        }

        size_t ttfDataSize_Monogram = 0;
        void *ttfData_Monogram = fs::getFileContent(ttfPtr_Monogram, &ttfDataSize_Monogram);

        _imguiFont = io.Fonts->AddFontFromMemoryTTF(ttfData_Monogram,
                                                    static_cast<int>(ttfDataSize_Monogram),
                                                    PIXEL_FONT_SIZE);
    } else {
        ImFontConfig config = ImFontConfig();
        config.FontDataOwnedByAtlas = false; // owned by _worldFont_Noto
        _imguiFont = io.Fonts->AddFontFromMemoryTTF(ttfData[DEARIMGUI_FONT],
                                                    static_cast<int>(ttfDataSize[DEARIMGUI_FONT]),
                                                    s_fontNativeSize[DEARIMGUI_FONT],
                                                    &config);
    }

    _worldFontManager = new FontManager(FONT_ATLAS_FACE);

    for (int font = 0; font < FontName_COUNT; ++font) {
        const bool colored = font == FontName_Emoji;
        
        TrueTypeHandle tth = _worldFontManager->createTtf(static_cast<uint8_t*>(ttfData[font]), ttfDataSize[font]);
        _worldFonts[font] = _worldFontManager->createFontByPixelSize(tth, 0, s_fontNativeSize[font], s_fontSpaceSize[font], colored, s_fontSDFRadius[font], s_fontSDFPadding[font], s_fontFiltering[font]);
        if (font != FontName_Emoji) {
            _worldFontManager->preloadGlyph(_worldFonts[font], "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ. +-*/,;:?!=~&|{}[]_@#<>123456789\n");
        }
        free(ttfData[font]);
        ttfData[font] = nullptr;
    }

    const float bitmapScale = s_fontNativeSize[DEARIMGUI_FONT] / PIXEL_FONT_SIZE;
    const uint16_t texPitch = 4 * image_width;
    for (i = 0; i < bodyNbChars; i++) {
        // emojis are past Y coordinate 66
        // note: emoji codepoints are well beyond 1000+, so we could test eg. bodyChars[i].c > 1000
        const bool coloredGlyph = bodyChars[i].y >= 66;
        const bool isEmoji = bodyChars[i].c > 1000;

        if (isEmoji) {
            bodyChars[i].fontTexIndex = io.Fonts->AddCustomRectFontGlyph(_imguiFont,
                                                                        bodyChars[i].c,
                                                                        bodyChars[i].w,
                                                                        bodyChars[i].h,
                                                                        (bodyChars[i].w + 1),
                                                                        ImVec2(0, 0),
                                                                        bitmapScale,
                                                                        coloredGlyph);
        }
    }
    
    // emoji variation selector
    io.Fonts->AddCustomRectFontGlyph(_imguiFont, 0xfe0f, 1, 1, 0);
    
    // -----------------------------------
    // BUILD ATLAS AND REPLACE CHARS
    // -----------------------------------
    
    // Build atlas
    io.Fonts->Build();
    
    io.Fonts->GetTexDataAsRGBA32(out_pixels, out_width, out_height);
    
    // REPLACE WITH CHARACTERS DEFINED IN PNG FONT
    
    for (int i = 0; i < bodyNbChars; i++) {
        
        int rect_id = bodyChars[i].fontTexIndex;
        
        if (ImFontAtlasCustomRect* rect = io.Fonts->GetCustomRectByIndex(rect_id)) {
            // Fill the custom rectangle with bitmap data
            for (int y = 0; y < rect->Height; y++) {
                ImU32* p = reinterpret_cast<ImU32*>(*out_pixels) + (rect->Y + y) * (*out_width) + (rect->X);
                for (int x = rect->Width; x > 0; x--) {
                    
                    int _x = rect->Width - x;
                    int _y = y;
                    
                    _x = (bodyChars[i].x + _x) * 4;
                    _y = (bodyChars[i].y + _y) * 4 * image_width;
                    
                    *p++ = IM_COL32(image_data[_y + _x + 2], image_data[_y + _x + 1], image_data[_y + _x], image_data[_y + _x + 3]);
                }
            }
        }
    }
    
    free(bodyChars);
    stbi_image_free(image_data);

    computeFontScales();
#endif
}

void Font::pushImFont(const Type fontType) {
#ifndef P3S_CLIENT_HEADLESS
    switch (fontType) {
        case Type::body:
        case Type::title:
        case Type::tiny:
            _imguiFont->Scale = getScaleForFontType(DEARIMGUI_FONT, fontType);
            ImGui::PushFont(_imguiFont);
            break;
    }
#endif
}

void Font::popImFont() const {
#ifndef P3S_CLIENT_HEADLESS
    ImGui::PopFont();
#endif
}

ImVec2 Font::calcImFontTextSize(const Type fontType,
                                const char* text,
                                const float width) const {
#ifndef P3S_CLIENT_HEADLESS
    const float fontSize = getSizeForFontType(DEARIMGUI_FONT, fontType) / Screen::nbPixelsInOnePoint;
    return _imguiFont->CalcTextSizeA(fontSize, FLT_MAX, width, text);
#else
    return ImVec2{10.0f, 10.0f};
#endif
}

ImVec2 Font::calcImFontTextSize(const Type fontType,
                                const char* textBegin,
                                const char* textEnd) const {
#ifndef P3S_CLIENT_HEADLESS
    switch (fontType) {
        case Type::body:
        case Type::title:
        case Type::tiny:
            const float fontSize = getSizeForFontType(DEARIMGUI_FONT, fontType) / Screen::nbPixelsInOnePoint;
            return _imguiFont->CalcTextSizeA(fontSize, FLT_MAX, -1.0f, textBegin, textEnd);
    }
#else
    return ImVec2{10.0f, 10.0f};
#endif
}

float Font::getImFontCharAdvanceX(const Type fontType,
                                  const unsigned int c) const {
#ifndef P3S_CLIENT_HEADLESS
    return (static_cast<int>(c) < _imguiFont->IndexAdvanceX.Size ? _imguiFont->IndexAdvanceX.Data[c] : _imguiFont->FallbackAdvanceX) * (getScaleForFontType(DEARIMGUI_FONT, fontType));

#else
    return 1.0f;
#endif
}

size_t Font::getImFontFirstWarpPart(const Type fontType,
                                    const char *start,
                                    const char *end,
                                    const char **cut,
                                    const float width) const {
    const char* _cut = nullptr;
    
#ifndef P3S_CLIENT_HEADLESS
    
    _cut = _imguiFont->CalcWordWrapPositionA(getScaleForFontType(DEARIMGUI_FONT, fontType), start, end, width);
    
    if (_cut != nullptr) {
        *cut = _cut;
    }
    
    if (_cut == nullptr) {
        return 0;
    }
    
    return _cut - start;
#else
    return 0;
#endif
}

#ifndef P3S_CLIENT_HEADLESS
FontManager *Font::getWorldFontManager() const {
    return _worldFontManager;
}
#endif

#ifndef P3S_CLIENT_HEADLESS
FontHandle Font::getFont(FontName f) const {
    return _worldFonts[f];
}
#endif

float Font::getScaleForFontType(const FontName font, const Type& fontType) const {
    switch (fontType) {
        case Type::body:
            return _fontCuratedSizes[font].body;
        case Type::title:
            return _fontCuratedSizes[font].title;
        case Type::tiny:
            return _fontCuratedSizes[font].tiny;
    }
    return 0.0f;
}

float Font::getSizeForFontType(const FontName font, const Type& fontType) const {
    return s_fontNativeSize[font] * getScaleForFontType(font, fontType);
}

void Font::computeFontScales() {
    for (int i = 0; i < FontName_COUNT; ++i) {
        const float newBodyFontScale = s_fontSize_Body[i] / s_fontNativeSize[i] * Screen::nbPixelsInOnePoint;
        const float newTitleFontScale = s_fontSize_Title[i] / s_fontNativeSize[i] * Screen::nbPixelsInOnePoint;
        const float newTinyFontScale = s_fontSize_Tiny[i] / s_fontNativeSize[i] * Screen::nbPixelsInOnePoint;

        if (float_isEqual(_fontCuratedSizes[i].body, newBodyFontScale, EPSILON_ZERO) == false ||
            float_isEqual(_fontCuratedSizes[i].title, newTitleFontScale, EPSILON_ZERO) == false ||
            float_isEqual(_fontCuratedSizes[i].tiny, newTinyFontScale, EPSILON_ZERO) == false) {
            _fontCuratedSizes[i].body = newBodyFontScale;
            _fontCuratedSizes[i].title = newTitleFontScale;
            _fontCuratedSizes[i].tiny = newTinyFontScale;
            NotificationCenter::shared().notify(NotificationName::fontScaleDidChange);
        }
    }
}

std::string Font::addLineBreaks(const char* txt, const float width) {
    char *cursor = const_cast<char *>(txt);
    char *end = cursor + strlen(txt);
    const char* cut = nullptr;
    bool firstLine = true;
    std::string formattedText;

    while(cursor < end) {
        size_t len = Font::getImFontFirstWarpPart(Font::Type::body, cursor, end, &cut,
                                                  width * Screen::nbPixelsInOnePoint);
        if (len <= 0) {
            cursor++;
        } else {
            if (firstLine == false) {
                formattedText += "\n";
            } else {
                firstLine = false;
            }

            std::string line = std::string(cursor, len);
            formattedText += vx::str::trim(line);
            cursor += len;

            // move cursor not to consider leading spaces in next getImFontFirstWarpPart call
            while (cursor < end) {
                if (*cursor == ' ') {
                    cursor++;
                } else {
                    break;
                }
            }
        }
    }
    
    return formattedText;
}
