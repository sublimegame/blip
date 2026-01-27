//
//  VXMenuConfig.hpp
//  Cubzh
//
//  Created by Adrien Duermael on 2/24/20.
//

#pragma once

#include "VXColor.hpp"
#include "VXConfig.hpp"
#include "colors.h"

// xptools
#include "Macros.h"

/// General menu settings
#define VXMENU_GAME_FADE_IN 0.8f
#define VXMENU_GAME_FADE_OUT 0.25f
#define VXMENU_DRAG_SQR_THRESHOLD 36.0f // 6 points (squared)
#define VXMENU_LONGPRESS_THRESHOLD 0.3f // seconds to acheive a long press
#define VXMENU_SCROLL_MOUSE_FACTOR 10.0f // multiplies scroll delta (mouse)
/// Menu background game
#define HUB_SCRIPT_BUNDLE_PATH "modules/cubzh.lua"
#define HUB_MAPBASE64_BUNDLE_PATH "misc/hubmap.b64"
/// Popups
#define VXPOPUP_ERR_GENERIC_TITLE "❌"
#define VXPOPUP_ERR_GENERIC_BODY "An error occurred, you have been redirected to the main menu."
#define VXPOPUP_ERR_NETWORK_BODY "Failed to connect, please check your internet connection."
#define VXPOPUP_ERR_EXPORT_BODY "File to export can't be downloaded"
#define VXPOPUP_ERR_EXPORT_FILE_FORMAT "File format not supported"
#define VXPOPUP_ERR_EXPORT_BODY_TOO_BIG "Item to export is too big for VOX (more than 256 blocks wide)"
#define VXPOPUP_ERR_WIP_TITLE "Work in progress"
#define VXPOPUP_ERR_WIP_BODY "We are still building this part, come back later!"
#define VXPOPUP_ERR_OUTDATED_TITLE "Update required"
#define VXPOPUP_ERR_OUTDATED_BODY "Sorry, this version of the app is outdated. Please update! 🙂"


namespace vx {

class MenuConfig {
    
public:
    VX_DISALLOW_COPY_AND_ASSIGN(MenuConfig)
    
    static const MenuConfig& shared() {
        static MenuConfig m;
        return m;
    }
    
    //----------------
    // Margins
    //----------------

    float marginNano = 2.0f;
    float marginVerySmall = 5.0f;
    float marginSmall = 10.0f;
    float marginMedium = 20.0f;
    float marginLarge = 40.0f;
    
    //----------------
    // Color
    //----------------

    Color colorWhite = Color(uint8_t(255), uint8_t(255), uint8_t(255), uint8_t(255));
    Color colorBlack = Color(uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(255));
    Color colorPurple = Color(uint8_t(201), uint8_t(71), uint8_t(245), uint8_t(255));

    enum Theme {
        Neutral,
        Positive,
        Negative,
        Play,
        Build,
        Account,
        Disabled,
        System,
        Apple
    };
    
    // neutral
    Color colorNeutralBackground = Color(uint8_t(216), uint8_t(216), uint8_t(216), uint8_t(255));
    Color colorNeutralBackgroundPressed = Color(uint8_t(179), uint8_t(179), uint8_t(179), uint8_t(255));
    Color colorNeutralBackgroundHovered = Color(uint8_t(242), uint8_t(242), uint8_t(242), uint8_t(255));
    Color colorNeutralText = Color(uint8_t(96), uint8_t(96), uint8_t(96), uint8_t(255));
    Color colorNeutralTextHovered = colorNeutralText;
    Color colorNeutralTextPressed = colorNeutralText;
    
    // positive
    Color colorPositiveBackground = Color(uint8_t(161), uint8_t(217), uint8_t(0), uint8_t(255));
    
    //
    Color colorWarningBackground = Color(uint8_t(255), uint8_t(197), uint8_t(1), uint8_t(255));

    // negative
    Color colorNegativeBackground = Color(uint8_t(227), uint8_t(52), uint8_t(55), uint8_t(255));
    Color colorNegativeText = Color(uint8_t(255), uint8_t(50), uint8_t(50), uint8_t(255));

    // play
    Color colorPlayBackground;
    Color colorPlayBackgroundSelected;
    
    // build
    Color colorBuildBackground;
    Color colorBuildBackgroundSelected;
    
    // system
    Color colorSystemBackground = Color(uint8_t(42), uint8_t(42), uint8_t(42), uint8_t(107));
    
    // account
    Color colorAccountBackground = Color(uint8_t(0), uint8_t(161), uint8_t(169), uint8_t(255));
     
    // disabled
    Color colorDisabledBackground = Color(uint8_t(255), uint8_t(255), uint8_t(255), uint8_t(25));
    Color colorDisabledBackgroundPressed = colorDisabledBackground;
    Color colorDisabledBackgroundHovered = colorDisabledBackground;
    Color colorDisabledText = Color(uint8_t(255), uint8_t(255), uint8_t(255), uint8_t(204));
    Color colorDisabledTextHovered = colorDisabledText;
    Color colorDisabledTextPressed = colorDisabledText;
   
    // special
    Color colorTransparent = Color(uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0));
    
    // discord
    Color discordBackground = Color(uint8_t(87), uint8_t(101), uint8_t(242), uint8_t(255));
    
    Color colorTransparentBlack20 = Color(uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(51));
    Color colorTransparentBlack40 = Color(uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(102));
    Color colorTransparentBlack60 = Color(uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(153));
    Color colorTransparentBlack80 = Color(uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(204));
    Color colorTransparentBlack90 = Color(uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(229));

    Color colorTransparentWhite10 = Color(uint8_t(255), uint8_t(255), uint8_t(255), uint8_t(25));
    Color colorTransparentWhite20 = Color(uint8_t(255), uint8_t(255), uint8_t(255), uint8_t(51));
    Color colorTransparentWhite40 = Color(uint8_t(255), uint8_t(255), uint8_t(255), uint8_t(102));
    Color colorTransparentWhite60 = Color(uint8_t(255), uint8_t(255), uint8_t(255), uint8_t(153));
    Color colorTransparentWhite70 = Color(uint8_t(255), uint8_t(255), uint8_t(255), uint8_t(178));
    Color colorTransparentWhite80 = Color(uint8_t(255), uint8_t(255), uint8_t(255), uint8_t(204));
    Color colorTransparentWhite90 = Color(uint8_t(255), uint8_t(255), uint8_t(255), uint8_t(229));
    
    // Color lightBackground;
    // Color mediumBackground;
    Color colorBackgroundGrey = Color(uint8_t(128), uint8_t(128), uint8_t(128), uint8_t(255));
    Color colorBackgroundDark = Color(uint8_t(64), uint8_t(64), uint8_t(64), uint8_t(255));
    Color colorBackgroundSuperDark = Color(uint8_t(38), uint8_t(38), uint8_t(38), uint8_t(255));
    Color colorWindowBackgroundDefault = colorBackgroundDark;
    Color colorWindowFrameDefault = colorBackgroundGrey;
    
    // CODE
    Color colorCodeLineNumber = Color(uint8_t(64), uint8_t(64), uint8_t(46), uint8_t(255));
    Color colorCodeComment = Color(uint8_t(128), uint8_t(128), uint8_t(128), uint8_t(255));
    Color colorCodeSelect = Color(uint8_t(50), uint8_t(50), uint8_t(128), uint8_t(230));
    Color colorCodeDefault = Color(uint8_t(255), uint8_t(255), uint8_t(255), uint8_t(255));
    Color colorCodeLocal = Color(uint8_t(207), uint8_t(79), uint8_t(215), uint8_t(255));
    Color colorCodeControls = Color(uint8_t(57), uint8_t(215), uint8_t(153), uint8_t(255));
    Color colorCodeJumps = Color(uint8_t(54), uint8_t(204), uint8_t(144), uint8_t(255));
    Color colorCodeNil = Color(uint8_t(179), uint8_t(47), uint8_t(124), uint8_t(255));
    Color colorCodeBool = Color(uint8_t(215), uint8_t(57), uint8_t(148), uint8_t(255));
    Color colorCodeNumber = Color(uint8_t(215), uint8_t(126), uint8_t(57), uint8_t(255));
    Color colorCodeString = Color(uint8_t(129), uint8_t(215), uint8_t(57), uint8_t(255));
    Color colorCodeBinaryOp = Color(uint8_t(117), uint8_t(204), uint8_t(171), uint8_t(255));
    Color colorCodeMathOp = Color(uint8_t(109), uint8_t(191), uint8_t(160), uint8_t(255));
    Color colorCodeBitwiseOp = Color(uint8_t(102), uint8_t(179), uint8_t(149), uint8_t(255));
    Color colorCodeComparisonOp = Color(uint8_t(123), uint8_t(215), uint8_t(179), uint8_t(255));
    Color colorCodeAssignationOp = Color(uint8_t(83), uint8_t(166), uint8_t(134), uint8_t(255));
    Color colorCodeLengthOp = Color(uint8_t(131), uint8_t(230), uint8_t(192), uint8_t(255));
    Color colorCodeName = Color(uint8_t(57), uint8_t(146), uint8_t(215), uint8_t(255)); // unused, for default strings starting with capital letter
    Color colorCodeBrackets = colorCodeControls;
    
    //----------------
    // Special
    //----------------
    
    float squareButtonSize = 34.0f;
    const float mainMenuMaxWidth = 400.0f;

    //----------------
    // Buttons
    //----------------
    
    float buttonHeight = 40.0f;
    float buttonHeightSmall = 30.0f;
    float buttonWidth = 200.0f;
    
    //----------------
    // Text inputs
    //----------------
    
    float textInputHeight = 40.0f;
    float textInputWidth = 200.0f;
    float textInputHeightSmall = 30.0f;
    float textInputWidthSmall = 150.0f;
    float textInputBorderSize = 2.0f;
    
    double cursorBlinkCycle = 0.8;
    
    Color textInputBackgroundColor = Color(uint8_t(153), uint8_t(153), uint8_t(153), uint8_t(255));
    Color textInputBackgroundColorFocused = Color(uint8_t(38), uint8_t(130), uint8_t(204), uint8_t(255));
    Color textInputBorderColor = Color(uint8_t(127), uint8_t(127), uint8_t(127), uint8_t(255));
    Color textInputTextColor = colorNeutralText;
    Color textInputTextColorFocused = colorWhite;
    Color textInputHintColor = Color(uint8_t(76), uint8_t(76), uint8_t(76), uint8_t(255));

    //----------------
    // Chat
    //----------------

    unsigned short maxStoredCommands = 100;
    
private:
    MenuConfig() {
        // EXPLORE
        colorPlayBackground = Color(uint8_t(0), uint8_t(120), uint8_t(255), uint8_t(255));
        colorPlayBackgroundSelected = Color(uint8_t(0), uint8_t(198), uint8_t(255), uint8_t(255));

        // BUILD
        colorBuildBackground = Color(uint8_t(253), uint8_t(110), uint8_t(14), uint8_t(255));
        colorBuildBackgroundSelected = Color(uint8_t(255), uint8_t(191), uint8_t(0), uint8_t(255));
    }

};

}
