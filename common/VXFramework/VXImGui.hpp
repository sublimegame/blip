
#pragma once

// C/C++
#include "stdint.h"

#include "VXMenu.hpp"
#include "VXValueAnimator.hpp"
#include "inputs.h"

#ifndef P3S_CLIENT_HEADLESS
#include <bgfx/bgfx.h>
#endif

namespace vx {

class ImguiContext final {
    
public:
    
    /// Constructor
    ImguiContext();
    
    /// Destructor
    ~ImguiContext();
    
    void destroy();
    
    void create();
    
    ///
#ifndef P3S_CLIENT_HEADLESS
    void beginFrame(Menu* currentMenu, uint32_t width, uint32_t height, float scrollY, bgfx::ViewId view);
#endif
    
    ///
    void endFrame();
    
private:
    
    /// Indicates if keys should be reset when the frame ends
    bool _resetKeys;

#ifndef P3S_CLIENT_HEADLESS
    /// This maps our Input enum to ImGuiKey dear-imgui enum
    /// /!\ BGFX/IMGUI update july 2022: we no longer give the mapping to dear-imgui via the IO struct,
    /// we now need to map ourselves
    ImGuiKey _keyMap[InputCount];
#endif
    
};
}
