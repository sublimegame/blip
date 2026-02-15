//
//  VXApplication.cpp
//  Cubzh
//
//  Created by Gaetan de Villele on 03/02/2020.
//  Copyright © 2020 voxowl. All rights reserved.
//

#include "VXApplication.hpp"

// C++
#include <cmath>
#include <sstream>

#include "HttpClient.hpp"

// xptools
#include "xptools.h"
#include "ThreadManager.hpp"
#include "process.hpp"
#include "filesystem.hpp"
#include "audio.hpp"
#include "OperationQueue.hpp"
#include "json.hpp"
#include "notifications.hpp"

// vx
#include "VXButton.hpp"
#include "VXLabel.hpp"
#include "VXFont.hpp"
#include "VXImGui.hpp"
#include "VXGameMessage.hpp"
#include "VXNetworkingUtils.hpp"
#include "VXResource.hpp"
#include "VXUpdateManager.hpp"
#include "VXAccount.hpp"
#include "VXPrefs.hpp"
#include "VXLatencyTester.hpp"

// Lua Sandbox
#include "lua_sandbox.hpp"

#include "VXGameRenderer.hpp"

#include "GameCoordinator.hpp"
#include "VXTickDispatcher.hpp"

#include "sbs.hpp"

// Lua
#include "lua.hpp"

// engine
#include "game.h"
#include "game_config.h"
#include "quaternion.h"
#include "color_palette.h"
#include "transform.h"
#include "inputs.h"

#include "cclog.h"

#ifndef P3S_CLIENT_HEADLESS

// bgfx
#include <bgfx/bgfx.h>
#include <bgfx/platform.h>
#include <bx/bx.h>

// bgfx+imgui
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Weverything"
#include <imgui/bgfx-imgui.h>
#include <dear-imgui/imgui.h>
#pragma GCC diagnostic pop

#endif

using namespace vx;

VXApplication::VXApplication() :
_currentMenu(Menu::make()) {
    _windowWidth = 0;
    _windowHeight = 0;
    _imguiContext = new ImguiContext();
    _renderer = nullptr;
    _displayingLoadingMenu = false;
    _activeBoundActions = 0;
}

VXApplication::~VXApplication() {
    delete _imguiContext;
}

VXApplication *VXApplication::getInstance() {
    if (_instance == nullptr) {
        _instance = new VXApplication();
    }
    return _instance;
}

bool VXApplication::isMobile() {
#if defined(__VX_PLATFORM_ANDROID) || defined(__VX_PLATFORM_IOS)
    return true;
#else
    return false;
#endif
}

static bool parseEnvironment(std::unordered_map<std::string,std::string>& environment, const std::string& environmentString) {
    cJSON* json = cJSON_Parse(environmentString.c_str());
    if (!cJSON_IsObject(json)) {
        return false;
    }
    cJSON *field = json->child;
    while (field) {
        const std::string key = field->string;
        const std::string value = cJSON_GetStringValue(field);
        environment[key] = value;
        field = field->next;
    }
    return true;
}

void VXApplication::init() {
    // Load config.json
    bool success = vx::Config::load();
    if (success == false) {
        vxlog_error("[VXApplication::init] failed to parse config.json");
    } else {
        vxlog_debug("[VXApplication::init] config.json loaded successfully");
    }
}

void VXApplication::didFinishLaunching(void *nativeWindowHandle,
                                       void *eglContext,
                                       const uint32_t windowWidth,
                                       const uint32_t windowHeight,
                                       const float nbPixelsInOnePoint,
                                       const Insets safeAreaInsets,
                                       const char *envJSONStr,
                                       int rendererType) {

    // Core : enable thread safety for transform IDs allocation
    transform_init_ID_thread_safety();
    chunk_alloc_default_light();

    // Variable number generator seed
    srand(time(NULL));

    vx::ThreadManager::shared().setMainThread();

    // Make Cubzh Core use cross platform implementation
    // This will go away once we open source Cubzh CPF (Cross Platform Framework)
    // since Cubzh Core will be able to use it as a dependency.
    cclog_function_ptr = vxlog_with_va_list;

    vx::Process::setMemoryUsageLimitMB(LUA_DEFAULT_MEMORY_USAGE_LIMIT);

    // HttpClient::shared().setCallbackMiddleware([](HttpRequest_SharedPtr req){});

#if DEBUG
    HttpClient::run_unit_tests();

#if DEBUG_QUATERNION_RUN_TESTS
    quaternion_run_unit_tests();
#endif

#if DEBUG_PALETTE_RUN_TESTS
    const size_t count = 100000;
    ColorPalette *p1, *p2;
    for(int i = 0; i < count; ++i) {
        vx_assert(debug_color_palette_test_hash(&p1, &p2));
    }
    vxlog_debug("Color palette lighting hash test passed with %d random generations", count);
#endif

#endif /* DEBUG */

    vxlog_trace("didFinishLaunching");

    std::unordered_map<std::string,std::string> environment;
    // Parse environment JSON string.
    if (envJSONStr != nullptr && strlen(envJSONStr) > 0) {
        vxlog_info("envJSONStr: %s", envJSONStr);
        if (!parseEnvironment(environment, envJSONStr)) {
            vxlog_error("Can't parse environment (not a valid JSON string)");
        }
        for (std::pair<std::string,std::string> e : environment) {
            vxlog_info("ENV: %s: %s", e.first.c_str(), e.second.c_str());
        }
    }

    // DEBUG DIRECT LINK
    // environment[GameCoordinator::ENV_FLAG_WORLD_ID] = "c02a3c8e-f54d-4de7-97d4-37c1cd64aca1";
    // environment[GameCoordinator::ENV_FLAG_SCRIPT] = "github.com/aduermael/cubzh-worlds/helloworld";
    // environment[GameCoordinator::ENV_FLAG_SCRIPT] = "github.com/aduermael/cubzh-worlds/helloworld/helloworld.lua";
    // environment[GameCoordinator::ENV_FLAG_SCRIPT] = "github.com/aduermael/cubzh-worlds/helloworld:6433fb6";
    // environment[GameCoordinator::ENV_FLAG_SCRIPT] = "huggingface.co/spaces/cubzh/ai-npcs";

    GameCoordinator::getInstance()->setEnvironmentToLaunch(environment);

    // track event APP_LAUNCH
    vx::tracking::TrackingClient::shared().trackEvent("User launches the app");

    // notifications
    vx::notification::setNeedsToPushToken(true);

    // -----------------
    // Update manager
    // -----------------

    if (UpdateManager::shared().firstVersionRun()) {
        // delete all files from storage/cache
        vx::fs::removeStorageDirectoryRecurse("cache");
        UpdateManager::shared().stampCurrentVersion();
    }

    // -----------------
    // SCREEN / BGFX / IMGUI
    // -----------------

    this->_windowWidth = windowWidth;
    this->_windowHeight = windowHeight;

    Screen::widthInPixels = windowWidth;
    Screen::heightInPixels = windowHeight;
    Screen::nbPixelsInOnePoint = nbPixelsInOnePoint;

    inputs_set_nb_pixels_in_one_point(Screen::nbPixelsInOnePoint);
    Screen::widthInPoints = Screen::widthInPixels / Screen::nbPixelsInOnePoint;
    Screen::heightInPoints = Screen::heightInPixels / Screen::nbPixelsInOnePoint;
    Screen::isLandscape = Screen::widthInPoints > Screen::heightInPoints;
    Screen::safeAreaInsets = safeAreaInsets;

#if !defined(P3S_CLIENT_HEADLESS)

    // create bgfx PlatformData struct
    bgfx::PlatformData pd;
    pd.nwh = nativeWindowHandle;
    pd.context = eglContext;

    // create bgfx Init struct
    bgfx::Init bgfxInit;
    switch(rendererType) {
        case 1: bgfxInit.type = bgfx::RendererType::Direct3D11; break;
        case 2:
        case 3: bgfxInit.type = bgfx::RendererType::OpenGLES; break;
        case 4: bgfxInit.type = bgfx::RendererType::OpenGL; break;
        case 5: bgfxInit.type = bgfx::RendererType::Metal; break;
        case 7: bgfxInit.type = bgfx::RendererType::Vulkan; break;
        default: bgfxInit.type = bgfx::RendererType::Count; break;
    }
    bgfxInit.resolution.width = this->_windowWidth;
    bgfxInit.resolution.height = this->_windowHeight;
    bgfxInit.resolution.reset = BGFX_RESET_NONE;
    bgfxInit.platformData = pd;
    bgfxInit.callback = &_bgfxCallback;

    if (bgfx::init(bgfxInit)) {
        bgfx::setDebug(BGFX_DEBUG_TEXT);
    } else {
        vxlog_error("bgfx init failed or already initialized");
    }

    _imguiContext->create();
    setupRenderer(windowWidth, windowHeight, nbPixelsInOnePoint);

    audio::AudioEngine::shared()->setVolume(Prefs::shared().masterVolume());

#endif

    State_SharedPtr currentState = GameCoordinator::getInstance()->getState();

    if (currentState == nullptr) {
        // set the game to the first state, in order to trigger the game loading
        GameCoordinator::getInstance()->setState(State::Value::startupLoading);
    }

    // if needed, retry uploading the push notification token to the Hub
    GameCoordinator::getInstance()->retryPushTokenUploadIfNeeded();

    this->_systemTerminated = false;
    vx::NotificationCenter::shared().notify(vx::NotificationName::didResize);
}

void VXApplication::didBecomeActive() {
    // dispatch the `didBecomeActive` logic to the main queue,
    // to make sure it is executed *after* `VXApplication::didFinishLaunching`.
    vx::OperationQueue::getMain()->dispatchFirst([this](){
        if (this->_systemTerminated) {
            return;
        }

        // if needed, retry uploading the push notification token to the Hub
        GameCoordinator::getInstance()->retryPushTokenUploadIfNeeded();

        // update favorite region
        LatencyTester::shared().ping();

        Game_SharedPtr g = GameCoordinator::getInstance()->getActiveGame();
        if (g != nullptr) {
            g->sendAppDidBecomeActive();
        }
    });

    // TODO: cancel local reminders

    vxlog_trace("didBecomeActive");
    vx::tracking::TrackingClient::shared().appDidBecomeActive();
    vx::tracking::TrackingClient::shared().trackEvent("App becomes active");
}

void VXApplication::willResignActive() {
    if (this->_systemTerminated) { return; }

    // close the keyboard
    Node::Manager::shared()->loseFocus();

    int arrSize = 0;
    Touch** downTouches = getDownTouches(&arrSize);

    if (arrSize >= 1 && downTouches[0] != nullptr) {
        Touch *t1 = downTouches[0];
        vx::OperationQueue::getMain()->dispatch([t1]() {
            postPointerEvent(PointerIDTouch1, PointerEventTypeUp, t1->x, t1->y, 0.0f, 0.0f);
        });
        postTouchEvent(0, t1->x, t1->y, 0.0f, 0.0f, TouchStateUp, false);
    }
    if (arrSize >= 2 && downTouches[1] != nullptr) {
        Touch *t2 = downTouches[1];
        vx::OperationQueue::getMain()->dispatch([t2]() {
            postPointerEvent(PointerIDTouch2, PointerEventTypeUp, t2->x, t2->y, 0.0f, 0.0f);
        });
        postTouchEvent(1, t2->x, t2->y, 0.0f, 0.0f, TouchStateUp, false);
    }

    free(downTouches);

    // TODO: schedule local reminders

    vxlog_trace("willResignActive");
    vx::tracking::TrackingClient::shared().appWillResignActive();
    vx::tracking::TrackingClient::shared().trackEvent("App resigns active");
}

void VXApplication::didEnterBackground() {
    vxlog_trace("didEnterBackground");
}

void VXApplication::willEnterForeground() {
    vxlog_trace("willEnterForeground");
}

void VXApplication::willTerminate() {
    vxlog_trace("willTerminate");

    // PARTIAL RELOAD
    // In case of a partial reload, all graphics resources will become obsolete.
    // To handle that case we immediately destroy all bgfx, ImGui, and game renderer resources,
    // so the game can be restarted cleanly while keeping its internal state
    _systemTerminated = true;

    if (this->_renderer != nullptr) {
        delete this->_renderer;
        this->_renderer = nullptr;
    }

#ifndef P3S_CLIENT_HEADLESS
    _imguiContext->destroy();
    bgfx::shutdown();
#endif
}

void VXApplication::tick(const double dt) {
    static Node_SharedPtr n;
    static std::vector<int> relFramesToSend;
    static Game_SharedPtr game = nullptr;

    n = nullptr;

    if (this->_systemTerminated) {
        return;
    }

    // Alter default style
#ifndef P3S_CLIENT_HEADLESS
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 0.0f;
    style.WindowBorderSize = 0.0f;
    style.AntiAliasedLines = false;
#endif

#ifndef P3S_CLIENT_HEADLESS
    /// RENDERER TICK
    if (this->_renderer != nullptr) {
        this->_renderer->tick(dt);
    }

    // call tick on all sounds
    audio::SoundsTicks::shared()->tick(dt);
#endif

    // Call the first blocks dispatched to the main thread OperationQueue
    // Note: glitches could be due to too many items in the queue,
    // if using a lot of notifications for instance.
    // In that case, 2 solutions:
    // - increase OPERATIONQUEUE_MAIN_PER_FRAME
    // - reduce the number of dispatch calls
    OperationQueue::getMain()->callFirstDispatchedBlocks(OPERATIONQUEUE_MAIN_PER_FRAME);

    game = GameCoordinator::getInstance()->getActiveGame();

    /// PROCESS INPUTS

    if (_currentMenu != nullptr) {
        _currentMenu->processInputs(dt);
    }

    /// UPDATE STATE IF NEEDED

    GameCoordinator::getInstance()->applyScheduledState();

    /// UPDATE MENUS
    // processing inputs can set _currentMenu to NULL
    // let's check again.

    if (_currentMenu != nullptr) {
        // Show popup to show
        Popup_SharedPtr popupToShow = GameCoordinator::getInstance()->popPopupToShow();
        if (popupToShow != nullptr) {
            _currentMenu->showPopup(popupToShow);
        }
    }

    /// Dispatch tick for all tick listeners (including VXMenu, VXGame)
    /// /!\ We need game tick to be executed AFTER anything that can trigger some Lua code to be executed,
    /// to ensure the scene is ready to be rendered w/o creating refresh delays
    ///
    /// NOTE: if it turns out that other tick listeners need to be called at different moments, then we'll
    /// split these into individual calls (or even explicitly call the end-of-frame refresh)
    TickDispatcher::getInstance().tick(dt);
}

void VXApplication::render() {
    if (this->_systemTerminated) { return; }

    // submit UI drawcalls to rendering backend (bgfx)
#ifndef P3S_CLIENT_HEADLESS
    Menu *menu = (_currentMenu != nullptr) ? _currentMenu.get() : nullptr;
    _imguiContext->beginFrame(menu, this->_windowWidth, this->_windowHeight, 0.0f, _renderer->getImguiView());
#endif
    if (_currentMenu != nullptr) {
        Node_SharedPtr n = _currentMenu->getContainerNode();
        if (n != nullptr) {
            n->render();
        }
    }
    _imguiContext->endFrame();

#ifndef P3S_CLIENT_HEADLESS
    if (this->_renderer != nullptr) {
        this->_renderer->render();
    }
#endif

    // advance to next frame
#ifndef P3S_CLIENT_HEADLESS
    bgfx::frame();
#endif
}

void VXApplication::didResize(const uint32_t width,
                              const uint32_t height,
                              const float nbPixelsInOnePoint,
                              const Insets safeAreaInsets) {

    if (this->_systemTerminated) return;

    this->_windowWidth = width;
    this->_windowHeight = height;

    Screen::widthInPixels = width;
    Screen::heightInPixels = height;
    Screen::nbPixelsInOnePoint = nbPixelsInOnePoint;

    inputs_set_nb_pixels_in_one_point(Screen::nbPixelsInOnePoint);
    Screen::widthInPoints = Screen::widthInPixels / Screen::nbPixelsInOnePoint;
    Screen::heightInPoints = Screen::heightInPixels / Screen::nbPixelsInOnePoint;
    Screen::safeAreaInsets = safeAreaInsets;

    // std::cout << "WIDTH: " << Screen::widthInPoints << " HEIGHT: " <<  Screen::heightInPoints  << " (points)" << std::endl;

#ifndef P3S_CLIENT_HEADLESS
    if (this->_renderer != nullptr) {
        this->_renderer->refreshViews(width, height, nbPixelsInOnePoint);
    }
#endif

#if defined(__VX_PLATFORM_WINDOWS) || defined(__VX_PLATFORM_MACOS) || defined(__VX_PLATFORM_LINUX)
    // release mouse buttons after resize, to prevent bugs because a mouse
    // down event could have been reveiced when manually resizing the window
    postMouseEvent(-1.0f, -1.0f, 0.0f, 0.0f, MouseButtonLeft, false, false);
    postMouseEvent(-1.0f, -1.0f, 0.0f, 0.0f, MouseButtonRight, false, false);
#endif

    const bool isLandscape = Screen::widthInPoints > Screen::heightInPoints;
    if (Screen::isLandscape != isLandscape) {
        Screen::isLandscape = isLandscape;
        vx::NotificationCenter::shared().notify(vx::NotificationName::orientationChanged);
    }

    vx::NotificationCenter::shared().notify(vx::NotificationName::didResize);
}

void VXApplication::didReceiveMemoryWarning() {
    if (this->_systemTerminated) { return; }
    vxlog_trace("didReceiveMemoryWarning");
}

void VXApplication::didReceiveNotification(const std::string& title,
                            const std::string& body,
                            const std::string& category,
                            uint32_t badge) {
    vx::OperationQueue::getMain()->dispatchFirst([title, body, category, badge](){
        Game_SharedPtr g = GameCoordinator::getInstance()->getActiveGame();
        if (g != nullptr) {
            g->sendDidReceivePushNotification(title, body, category, badge);
        }
    });
}

void VXApplication::didRegisterForRemoteNotifications(const std::string& tokenType,
                                                      const std::string& tokenValue) {
    GameCoordinator::getInstance()->didRegisterForRemoteNotifications(tokenType, tokenValue);
}

// cubzh://world/123
// cubzh://server/123
// cubzh://link/123 (could be a world or a server, contains user ID, author of link)
bool VXApplication::openURL(const std::string urlStr) {
    vxlog_trace("[VXApplication] openURL: %s", urlStr.c_str());

    // parse URL string
    const vx::URL url = vx::URL::make(std::string(urlStr));
    if (url.isValid() == false) {
        return false; // URL wasn't handled
    }

    const std::string urlQueryParamKeyWorldID = "worldID";

    // handle https://app.cu.bzh?worldID=123abc
    if (url.scheme() == "https" &&
        url.host() == "app.cu.bzh" &&
        url.queryValueCountForKey(urlQueryParamKeyWorldID) == 1) {

        // convert params into world environment
        const QueryParams& qp = url.queryParams();

        std::unordered_map<std::string,std::string> environment;

        for (std::pair<std::string,std::unordered_set<std::string>> entry : qp) {
            if (entry.second.empty() == false) {
                environment[entry.first] = *entry.second.begin();
            }
        }

        // look for worldID's value
        auto it = environment.find(urlQueryParamKeyWorldID);
        if (it != environment.end()) {
            // Key found, access the value using iterator
            vx::HubClient::getInstance().updateWorldViews(it->second, 1, nullptr);
        }

        GameCoordinator::getInstance()->setEnvironmentToLaunch(environment);
        Game_SharedPtr activeGame = GameCoordinator::getInstance()->getActiveGame();
        if (activeGame != nullptr) {
            activeGame->sendReceivedEnvToLaunch();
        }

        return true; // URL was handled
    }

    return false; // URL wasn't handled
}

void VXApplication::showKeyboard(Size keyboardSize, float duration) {
    GameCoordinator::getInstance()->showKeyboard(keyboardSize, duration);
}

void VXApplication::hideKeyboard(float duration) {
    GameCoordinator::getInstance()->hideKeyboard(duration);
}

VXApplication *VXApplication::_instance = nullptr;

void VXApplication::setupRenderer(uint16_t width, uint16_t height, float devicePPP) {
    if (this->_renderer == nullptr) {
        // default is High
        QualityTier tier = QualityTier_High;

#if defined(__VX_PLATFORM_ANDROID)
        tier = static_cast<QualityTier>(vx::device::getPerformanceTier());
#elif defined(__VX_PLATFORM_IOS)
        // select renderer quality tier based on device performance tier
        const vx::device::PerformanceTier perfTier = vx::device::getPerformanceTier();
        switch (perfTier) {
            case vx::device::PerformanceTier_Minimal:
                tier = QualityTier_Compatibility;
                break;
            case vx::device::PerformanceTier_Low:
                tier = QualityTier_Low;
                break;
            case vx::device::PerformanceTier_Medium:
                tier = QualityTier_Medium;
                break;
            case vx::device::PerformanceTier_High:
                tier = QualityTier_High;
                break;
        }
#elif defined(__VX_PLATFORM_WASM)
        tier = QualityTier_Low;
#endif

        int prefTier = Prefs::shared().renderQualityTier();
        if (prefTier != RENDER_QUALITY_TIER_INVALID) {
            tier = static_cast<QualityTier>(prefTier - 1);
        }

        // HACK, for marketing captures
        // tier = QualityTier_Ultra;

        this->_renderer = GameRenderer::newGameRenderer(tier, width, height, devicePPP);
    }
}

std::unordered_map<std::string, std::string> parseQueryString(const std::string& queryString) {
    std::unordered_map<std::string, std::string> parameters;

    std::istringstream inputStream(queryString);

    std::string token;
    while (std::getline(inputStream, token, '&')) {
        std::size_t equalPos = token.find('=');
        if (equalPos != std::string::npos) {
            std::string key = token.substr(0, equalPos);
            std::string value = token.substr(equalPos + 1);
            parameters[key] = value;
        }
    }

    return parameters;
}

bool VXApplication::didReceiveInstallReferrerInfo(const std::string jsonStr) {

    cJSON *json = cJSON_Parse(jsonStr.c_str());
    if (json == nullptr) {
        return false;
    }

    std::string origin;
    bool ok = vx::json::readStringField(json, "origin", origin);
    if (ok == false) {
        return false;
    }

    if (origin == "google-play") {
        std::string referrerURL;
        ok = vx::json::readStringField(json, "referrerUrl", referrerURL);
        if (ok == false) {
            return false;
        }

        // Create a map to store key-value pairs
        std::unordered_map<std::string, std::string> keyValuePairs = parseQueryString(referrerURL);

        tracking::TrackingClient::shared().trackEvent("App receives install referrer info", keyValuePairs);

        return true; // always true for now...
    }

    return false;
}
