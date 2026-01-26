//
//  VXConfig.hpp
//  Cubzh
//
//  Created by Adrien Duermael on 3/9/20.
//

#pragma once

#include <string>
#if __VX_PLATFORM_WINDOWS
#include "envvars.h"
#endif
#include "xptools.h"
#include "vxconfig.h" // temporary

#define GAME_ID_FOR_CUBZH_APP "cubzh"
#define GAME_ID_FOR_LOCAL_ITEMEDITOR "item_editor"

#ifdef __VX_SINGLE_THREAD
#define OPERATIONQUEUE_MAIN_PER_FRAME 20
#else
#define OPERATIONQUEUE_MAIN_PER_FRAME 10
#endif

// MARK: - CONTROLS -

#define SENSITIVITY_MIN 0.01f
#define SENSITIVITY_MAX 3.0f
#define SENSITIVITY_DEFAULT 1.0f
#define ZOOM_SENSITIVITY_DEFAULT 1.0f

#define RENDER_QUALITY_TIER_INVALID -1
#define RENDER_QUALITY_TIER_MIN 1
#define RENDER_QUALITY_TIER_MAX 5

// MARK: - FOG -

#define FOG_DEFAULT_START_DISTANCE 200
#define FOG_DEFAULT_END_DISTANCE 400

// MARK: - FONTS -

enum FontName {
    FontName_Pixel,
    FontName_Noto,
    FontName_NotoMono,
    FontName_Emoji, // shared
    FontName_NotoJP // shared
#define FontName_COUNT 5
};

#define DEARIMGUI_FONT FontName_NotoMono

#if defined(__VX_PLATFORM_ANDROID) || defined(__VX_PLATFORM_IOS)
#define MONOGRAM_FONT_SIZE_TINY 18.0f
#define MONOGRAM_FONT_SIZE_BODY 22.0f
#define MONOGRAM_FONT_SIZE_TITLE 24.0f

#define NOTO_FONT_SIZE_TINY 13.0f
#define NOTO_FONT_SIZE_BODY 15.0f
#define NOTO_FONT_SIZE_TITLE 18.0f
#else // PC, Mac, Wasm
#define MONOGRAM_FONT_SIZE_TINY 28.0f
#define MONOGRAM_FONT_SIZE_BODY 32.0f
#define MONOGRAM_FONT_SIZE_TITLE 34.0f

#define NOTO_FONT_SIZE_TINY 18.0f
#define NOTO_FONT_SIZE_BODY 22.0f
#define NOTO_FONT_SIZE_TITLE 24.0f
#endif

// TTF sample size
#if defined(__VX_PLATFORM_ANDROID) || defined(__VX_PLATFORM_IOS)
#define FONT_ATLAS_FACE 512
#define MONOGRAM_FONT_SIZE 22.0f
#define MONOGRAM_SPACE_SIZE 11.0f
#define NOTO_FONT_SIZE 26.0f
#define NOTO_SPACE_SIZE 9.0f
#define EMOJI_FONT_SIZE 48.0f
#else
#define FONT_ATLAS_FACE 1024
#define MONOGRAM_FONT_SIZE 32.0f
#define MONOGRAM_SPACE_SIZE 16.0f
#define NOTO_FONT_SIZE 48.0f // 32.0f 48.0f 64.0f
#define NOTO_SPACE_SIZE 18.0f // 12.0f 18.0f 24.0f
#define EMOJI_FONT_SIZE 64.0f
#endif

// SDF config
#define FONT_SDF_WEIGHT_SPACING 1.0f // ratio
#define FONT_SDF_OUTLINE_WEIGHT_SPACING 1.0f // ratio
#define MONOGRAM_SDF_PADDING 3 // texels
#define MONOGRAM_SDF_RADIUS 3.0f // texels
#define MONOGRAM_SDF_SOFTNESS_MAX 0.0f // ratio
#define MONOGRAM_SDF_SOFTNESS_MIN 0.0f // ratio
#define NOTO_SDF_PADDING 12 // texels
#define NOTO_SDF_RADIUS 12.0f // texels
#define NOTO_SDF_SOFTNESS_MAX 0.11f // ratio
#define NOTO_SDF_SOFTNESS_MIN 0.005f // ratio

// Pixel font metrics
#define PIXEL_FONT_SIZE 11.0f

#define LUA_TEXT_FONT_SIZE_DEFAULT "default"
#define LUA_TEXT_FONT_SIZE_BIG "big"
#define LUA_TEXT_FONT_SIZE_SMALL "small"

/***
 * TTF can support colors using different features of the file format,
 * - COLR/CPAL (supported)
 * - CBDT/CBLC (not supported)
 * - SVG (not supported)
 **/
const std::string s_fontTTF[FontName_COUNT] = {
    "fonts/monogram_extended.ttf",
    "fonts/NotoSans-Regular.ttf",
    "fonts/NotoSansMono-Regular.ttf",
    "fonts/Twemoji.Mozilla.ttf",
    "fonts/NotoSansJP-Regular.ttf"
};
const float s_fontNativeSize[FontName_COUNT] = {
    MONOGRAM_FONT_SIZE,
    NOTO_FONT_SIZE,
    NOTO_FONT_SIZE,
    EMOJI_FONT_SIZE,
    NOTO_FONT_SIZE
};
const float s_fontSize_Tiny[FontName_COUNT] = {
    MONOGRAM_FONT_SIZE_TINY,
    NOTO_FONT_SIZE_TINY,
    NOTO_FONT_SIZE_TINY,
    NOTO_FONT_SIZE_TINY,
    NOTO_FONT_SIZE_TINY
};
const float s_fontSize_Body[FontName_COUNT] = {
    MONOGRAM_FONT_SIZE_BODY,
    NOTO_FONT_SIZE_BODY,
    NOTO_FONT_SIZE_BODY,
    NOTO_FONT_SIZE_BODY,
    NOTO_FONT_SIZE_BODY
};
const float s_fontSize_Title[FontName_COUNT] = {
    MONOGRAM_FONT_SIZE_TITLE,
    NOTO_FONT_SIZE_TITLE,
    NOTO_FONT_SIZE_TITLE,
    NOTO_FONT_SIZE_TITLE,
    NOTO_FONT_SIZE_TITLE
};
const float s_fontSpaceSize[FontName_COUNT] = {
    MONOGRAM_SPACE_SIZE,
    NOTO_SPACE_SIZE,
    NOTO_SPACE_SIZE,
    NOTO_SPACE_SIZE,
    NOTO_SPACE_SIZE
};
const float s_fontSDFRadius[FontName_COUNT] = { // texels
    MONOGRAM_SDF_RADIUS,
    NOTO_SDF_RADIUS,
    NOTO_SDF_RADIUS,
    NOTO_SDF_RADIUS,
    NOTO_SDF_RADIUS
};
const float s_fontSDFPadding[FontName_COUNT] = { // texels
    MONOGRAM_SDF_PADDING,
    NOTO_SDF_PADDING,
    NOTO_SDF_PADDING,
    NOTO_SDF_PADDING,
    NOTO_SDF_PADDING
};
const float s_fontSDFSoftnessMax[FontName_COUNT] = {
    MONOGRAM_SDF_SOFTNESS_MAX,
    NOTO_SDF_SOFTNESS_MAX,
    NOTO_SDF_SOFTNESS_MAX,
    NOTO_SDF_SOFTNESS_MAX,
    NOTO_SDF_SOFTNESS_MAX
};
const float s_fontSDFSoftnessMin[FontName_COUNT] = {
    MONOGRAM_SDF_SOFTNESS_MIN,
    NOTO_SDF_SOFTNESS_MIN,
    NOTO_SDF_SOFTNESS_MIN,
    NOTO_SDF_SOFTNESS_MIN,
    NOTO_SDF_SOFTNESS_MIN
};
const bool s_fontFiltering[FontName_COUNT] = {
    false, // FontName_Pixel
    true, // FontName_Noto
    true, // FontName_NotoMono
    true, // FontName_Emoji
    true  // FontName_NotoJP
};

// alpha-blended fonts are rendered in TRANSPARENT pass, otherwise rendered in OPAQUE/OIT
// Note: if the font uses text softness, it must be alpha-blended
static const bool s_fontAlphaBlended[FontName_COUNT] = {
    false, // FontName_Pixel
    true, // FontName_Noto
    true, // FontName_NotoMono
    true, // FontName_Emoji
    true  // FontName_NotoJP
};

// MARK: - REGULAR EXPRESSIONS -

#define REGEX_USERNAME "^[a-z][a-z0-9]{0,15}$"
#define REGEX_PERMISSIVE "^.{0,512}$"
#define REGEX_PERMISSIVE_NO_SPACE "^[^ ]{0,512}$"
#define REGEX_MAGIC_KEY "^[0-9]{0,6}$"
#define REGEX_EMAIL "^[a-zA-Z0-9.!#$%&'*+\\/=?^_`{|}~-]+@[a-zA-Z0-9](?:[a-zA-Z0-9-]{0,61}[a-zA-Z0-9])?(?:\\.[a-zA-Z0-9](?:[a-zA-Z0-9-]{0,61}[a-zA-Z0-9])?)*$"
#define REGEX_USERNAME_OR_EMAIL "^[a-z][a-z0-9]{0,15}$|^[a-zA-Z0-9.!#$%&'*+\\/=?^_`{|}~-]+@[a-zA-Z0-9](?:[a-zA-Z0-9-]{0,61}[a-zA-Z0-9])?(?:\\.[a-zA-Z0-9](?:[a-zA-Z0-9-]{0,61}[a-zA-Z0-9])?)*$"

// MARK: - Others -

// Minimum window size for Desktop versions (in points)
#define P3S_WINDOW_MINIMUM_WIDTH 320
#define P3S_WINDOW_MINIMUM_HEIGHT 320

#define DRAG_SCROLL_THRESHOLD 10.0f

#define SCROLL_READABLE_SPEED 2.0f
#define SCROLL_UNREADABLE_SPEED 50.0f

// used for zoom in Item Editor
#define SCROLL_WHEEL_ZOOM_SENSITIVITY 20.0f // (when using mouse wheel)
#define TOUCH_ZOOM_SENSITIVITY 0.03f // (when using pinch touch gesture)

// minimum amount of points in
#define SCREEN_MIN_POINTS 320.0f

// Min width for descrition area in play & build nodes
#define DESCRIPTION_AREA_MIN_WIDTH 200.0f
#define THUMBNAIL_MAX_WIDTH 250.0f
#define THUMBNAIL_MAX_HEIGHT 250.0f
#define MIN_WIDTH_TO_FORCE_LANDSCAPE_DISPLAY 600.0f

// bevel definition for thumbnails
#define BEVEL 3.0f

#define CHAT_TEXT_LINEWRAP 230.0f // pixels

/// Limit before forcing garbage collector.
/// Sending warning + stoping all Lua callbacks if memory usage remains above it.
#define LUA_DEFAULT_MEMORY_USAGE_LIMIT 1500 // 1500MB
#define LUA_DEFAULT_MEMORY_USAGE_LIMIT_FOR_GAMESERVER 100 // 100MB
#define LUA_MAX_RECURSION_DEPTH 5000

// --------------------------------------------------
//
// vx::Button
//
// --------------------------------------------------

// factor of brightness diff between button color and button hover color
#define BUTTON_COLOR_HOVER_DIFF (0.1)
// factor of brightness diff between button color and button pressed color
#define BUTTON_COLOR_PRESSED_DIFF (-0.2)

// factor of brightness diff between button background color and text color
#define BUTTON_TEXT_VS_BACKGROUND_COLOR_DIFF (-0.45)
// factor of brightness diff between button textcolor and button hover textcolor
#define BUTTON_TEXTHOVER_VS_TEXT_COLOR_DIFF (-0.05)
// factor of brightness diff between button textcolor and button press textcolor
#define BUTTON_TEXTPRESSED_VS_TEXT_COLOR_DIFF (0.1)

// factor of brightness diff between button background color and bevel colors
#define BUTTON_BEVELCOLOR_LIGHTER_DIFF (0.15)
#define BUTTON_BEVELCOLOR_DARKER_DIFF (-0.3)

#define GAME_TITLE_REGEX "^[a-zA-Z][a-zA-Z0-9_ ']*$"
#define GAME_TITLE_MAX_CHARS 50

#define ITEM_NAME_REGEX "^[a-z][a-z0-9_]*$"
#define ITEM_NAME_MAX_CHARS 20

// same limit as the one in the Hub server
#define DESCRIPTION_MAX_CHARS 10000

namespace vx {

class Config {
    
public:
    
    static const Config& shared() {
        static Config m;
        return m;
    }
    
    double searchTimeRefresh = 0.5;
    float defaultFadeInTime = 0.3f;
    
    static bool load();
    
    static void setApiHost(const char *host);
    
    static const char* apiHost();
    
    static std::string debugGameServer();
    static std::string debugHubServer();
    static std::string gameServerTag();

    static std::string version();
    static std::string commit();
    
private:
    static bool _loaded;
    static char* _apiHost;
    static std::string _debugGameServer;
    static std::string _debugHubServer;
    static std::string _gameServerTag;
    static std::string _version;
    static std::string _commit;

    static std::string readOptionalField(const cJSON* config, const char* name);
};

}
