//
//  VXApplication.hpp
//  Cubzh
//
//  Created by Gaetan de Villele on 03/02/2020.
//  Copyright © 2020 voxowl. All rights reserved.
//

#pragma once

// C/C++
#include "stdint.h"

// vx
#include "VXNode.hpp"
#include "VXInsets.hpp"
#include "VXGameRenderer.hpp"
#include "VXRDUtils.hpp"
#include "VXKeyboard.hpp"
#include "GameCoordinator.hpp"

// xptools
#include "Macros.h"

// bgfx
#ifndef P3S_CLIENT_HEADLESS
#include <bgfx/bgfx.h>
#endif

#include "State.hpp"
#include "VXMenu.hpp"
#include "VXImGui.hpp"

namespace vx {

///
class VXApplication final {
    
public:
    
    ///
    static VXApplication *getInstance();
    
    static bool isMobile();
    
    ///
    ~VXApplication();
    
    // Events

    void init();

    ///
    /// gdevillele: environment argument could be removed
    void didFinishLaunching(void *nativeWindowHandle,
                            void *eglContext,
                            const uint32_t windowWidth,
                            const uint32_t windowHeight,
                            const float nbPixelsInOnePoint,
                            const Insets safeAreaInsets,
                            const char *environment,
                            int rendererType=-1);
    
    /// gained focus
    void didBecomeActive();
    
    /// lost focus
    void willResignActive();

    ///
    void willEnterForeground();

    ///
    void didEnterBackground();
    
    ///
    void willTerminate();
    
    /// run this from main thread
    void tick(const double dt);

    /// run this from render thread if applicable
    void render();
    
    /// this should be called before didFinishLaunching()
    void didResize(const uint32_t width,
                   const uint32_t height,
                   const float nbPixelsInOnePoint,
                   const Insets safeAreaInsets);
    
    ///
    void didReceiveMemoryWarning();

    ///
    void didReceiveNotification(const std::string& title,
                                const std::string& body,
                                const std::string& category,
                                uint32_t badge);

    /// This is called when user decides whether to grant Remote Notifications permissions.
    ///
    /// If `success` is `false`, then `info` has one of those two values:
    /// - `"user_denied"`
    /// - `"token_req_failed"`
    void didRegisterForRemoteNotifications(const std::string& tokenType,
                                           const std::string& tokenValue);

    /// returns true if the URL has been handled, false otherwise
    bool openURL(const std::string urlStr);
    
    ///
    void showKeyboard(Size keyboardSize, float duration);
    
    ///
    void hideKeyboard(float duration);

    bool didReceiveInstallReferrerInfo(const std::string basicString);

protected:
    
    VX_DISALLOW_COPY_AND_ASSIGN(VXApplication)

private:
    
    enum class MenuType {
        none,
        login,
        main,
        other
    };
    
    ///
    static VXApplication *_instance;
    
    /// Constructor
    VXApplication();
    
    ///
    uint32_t _windowWidth;
    
    ///
    uint32_t _windowHeight;

    ///
    Menu_SharedPtr _currentMenu;

    /// true if we are currently displaying a loading menu
    bool _displayingLoadingMenu;
    
    GameRenderer *_renderer;
    rendering::BgfxCallback _bgfxCallback;
    
    ///
    ImguiContext *_imguiContext;

    /// Flag used to forcibly skip any system lifecycle functions during a partial reload
    bool _systemTerminated;

    /// if the number of bound actions change in running game, the UI will refresh
    uint8_t _activeBoundActions;
    
    void setupRenderer(uint16_t width, uint16_t height, float devicePPP);
};

}
