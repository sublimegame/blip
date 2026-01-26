//
//  VXImGui.cpp
//
//
//  Created by ****** on **/**/**.
//

#include "VXImGui.hpp"

// C++
#include <cmath>

#include "VXNode.hpp"
#include "VXKeyboard.hpp"

#ifndef P3S_CLIENT_HEADLESS

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Weverything"
// imgui+bgfx
#include <imgui/bgfx-imgui.h>
#include <dear-imgui/imgui.h>
#pragma GCC diagnostic pop

#endif

using namespace vx;

ImguiContext::ImguiContext() {
    this->_resetKeys = false;
}

ImguiContext::~ImguiContext() {}

void ImguiContext::destroy() {
#ifndef P3S_CLIENT_HEADLESS
    imguiDestroy();
#endif
}

void ImguiContext::create() {
#ifndef P3S_CLIENT_HEADLESS
    imguiCreate();
    
    ImGuiIO& io = ImGui::GetIO();

    for (int i = 0; i < InputCount; ++i) {
        _keyMap[i] = ImGuiKey_None;
    }

    _keyMap[InputTab] = ImGuiKey_Tab;
    _keyMap[InputLeft] = ImGuiKey_LeftArrow;
    _keyMap[InputRight] = ImGuiKey_RightArrow;
    _keyMap[InputUp] = ImGuiKey_UpArrow;
    _keyMap[InputDown] = ImGuiKey_DownArrow;
    _keyMap[InputPageUp] = ImGuiKey_PageUp;
    _keyMap[InputPageDown] = ImGuiKey_PageDown;
    _keyMap[InputHome] = ImGuiKey_Home;
    _keyMap[InputEnd] = ImGuiKey_End;
    _keyMap[InputInsert] = ImGuiKey_Insert;
    _keyMap[InputDelete] = ImGuiKey_Delete;
    _keyMap[InputBackspace] = ImGuiKey_Backspace;
    _keyMap[InputSpace] = ImGuiKey_Space;
    _keyMap[InputReturn] = ImGuiKey_Enter;
    _keyMap[InputReturnKP] = ImGuiKey_KeypadEnter;
    _keyMap[InputEsc] = ImGuiKey_Escape;
    _keyMap[InputKeyA] = ImGuiKey_A;
    _keyMap[InputKeyC] = ImGuiKey_C;
    _keyMap[InputKeyV] = ImGuiKey_V;
    _keyMap[InputKeyX] = ImGuiKey_X;
    _keyMap[InputKeyY] = ImGuiKey_Y;
    _keyMap[InputKeyZ] = ImGuiKey_Z;
    // ImGuiKey_KeyPadEnter <= NOT HANDLED
    
    io.ConfigFlags |= 0
    | ImGuiConfigFlags_NavEnableKeyboard;
    
    io.ConfigInputTextCursorBlink = true;
#endif
}

#ifndef P3S_CLIENT_HEADLESS
void ImguiContext::beginFrame(Menu* currentMenu, uint32_t width, uint32_t height, float scrollY, bgfx::ViewId view) {
    
    // cursor
    float cx = 0.0f;
    float cy = 0.0f;
    bool cBtn1 = false; // mouse: left btn
    bool cBtn2 = false; // mouse: right btn
    bool cBtn3 = false; // mouse: middle btn
    uint8_t cBtn = 0;
    
    input_get_cursor(&cx, &cy, &cBtn1, &cBtn2, &cBtn3);
    
    if (cBtn1) cBtn |= IMGUI_MBUT_LEFT;
    if (cBtn2) cBtn |= IMGUI_MBUT_RIGHT;
    if (cBtn3) cBtn |= IMGUI_MBUT_MIDDLE;
    
    // We handle everything in points, imgui wants pixels
    // let's do the conversion
    cy = cy * Screen::nbPixelsInOnePoint;
    cx = cx * Screen::nbPixelsInOnePoint;
    
    cy = height - cy;
    
    imguiBeginFrame(static_cast<int32_t>(cx), static_cast<int32_t>(cy),
                    cBtn, scrollY, width, height, -1 /*_inputChar*/, view);
}
#endif

void ImguiContext::endFrame() {
#ifndef P3S_CLIENT_HEADLESS
    if (_resetKeys) {
        // reset keys
        ImGuiIO& io = ImGui::GetIO();
        for (int i = 0; i < InputCount; ++i) {
            io.AddKeyEvent(_keyMap[i], false);
        }
    }
    imguiEndFrame();
#endif
}

