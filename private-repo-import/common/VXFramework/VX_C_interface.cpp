//
//  VX_C_interface.cpp
//  macos
//
//  Created by Adrien Duermael on 2/12/20.
//

#include "VX_C_interface.hpp"

// vx
#include "VXApplication.hpp"
#include "GameCoordinator.hpp"
#include "VXNotifications.hpp"
#include "HttpClient.hpp"

#ifdef __cplusplus
extern "C" {
#endif

void vxapplication_init() {
    vx::VXApplication::getInstance()->init();
}

void vxapplication_didResize(const uint32_t widthPx,
                             const uint32_t heightPx,
                             const float nbPixelsInOnePoint,
                             struct vx_insets safeAreaInsets) {
    vx::VXApplication::getInstance()->didResize(widthPx,
                                                heightPx,
                                                nbPixelsInOnePoint,
                                                vx::Insets{safeAreaInsets.top, safeAreaInsets.bottom, safeAreaInsets.left, safeAreaInsets.right});
}

void vxapplication_didFinishLaunching(void *nativeWindowHandle, void *eglContext,
                                      uint32_t windowWidth, uint32_t windowHeight,
                                      float nbPixelsInOnePoint,
                                      struct vx_insets safeAreaInsets,
                                      const char *environment) {
    
    vx::VXApplication::getInstance()->didFinishLaunching(nativeWindowHandle,
                                                         nullptr,
                                                         windowWidth,
                                                         windowHeight,
                                                         nbPixelsInOnePoint,
                                                         vx::Insets{safeAreaInsets.top, safeAreaInsets.bottom, safeAreaInsets.left, safeAreaInsets.right},
                                                         environment);
}

bool vxapplication_openURL(const char *urlCStr) {
    if (urlCStr == nullptr) {
        return false; // URL was not handled
    }
    return vx::VXApplication::getInstance()->openURL(std::string(urlCStr));
}

void vxapplication_didBecomeActive() {
    vx::VXApplication::getInstance()->didBecomeActive();
}

void vxapplication_willResignActive() {
    vx::VXApplication::getInstance()->willResignActive();
}

void vxapplication_didEnterBackground() {
    vx::VXApplication::getInstance()->didEnterBackground();
}

void vxapplication_willEnterForeground() {
    vx::VXApplication::getInstance()->willEnterForeground();
}

void vxapplication_willTerminate() {
    vx::VXApplication::getInstance()->willTerminate();
}

void vxapplication_tick(const double dt) {
    vx::VXApplication::getInstance()->tick(dt);
    // On macOS and iOS, the rendering is performed in the main thread.
    vx::VXApplication::getInstance()->render();
}

bool vx_is_mouse_hidden() {
    return vx::GameCoordinator::shouldMouseCursorBeHidden();
}

void vxapplication_show_keyboard(struct vx_size keyboardSize, float duration) {
    vx::VXApplication::getInstance()->showKeyboard(vx::Size(keyboardSize.width, keyboardSize.height), duration);
}

void vxapplication_hide_keyboard(float duration) {
    vx::VXApplication::getInstance()->hideKeyboard(duration);
}

void vxapplication_soft_keyboard_return_key_up() {
    vx::NotificationCenter::shared().notify(vx::NotificationName::softReturnKeyUp);
}

void vxapplication_didReceiveNotification(const char *titleCStr,
                                          const char *bodyCStr,
                                          const char *categoryCStr,
                                          uint32_t badge) {
    vx_assert(titleCStr != nullptr);
    vx_assert(bodyCStr != nullptr);
    vx_assert(categoryCStr != nullptr);

    std::string title(titleCStr);
    std::string body(bodyCStr);
    std::string category(categoryCStr);

    vx::VXApplication::getInstance()->didReceiveNotification(title,
                                                             body,
                                                             category,
                                                             badge);
}

void vxapplication_didRegisterForRemoteNotifications(const char *tokenTypeCStr,
                                                     const char *tokenValueCStr) {
    vx_assert(tokenTypeCStr != nullptr);
    vx_assert(tokenValueCStr != nullptr);

    std::string tokenType(tokenTypeCStr);
    std::string tokenValue(tokenValueCStr);

    vx_assert(tokenType.empty() == false);

    vx::VXApplication::getInstance()->didRegisterForRemoteNotifications(tokenType, tokenValue);
}

#ifdef __cplusplus
} // extern "C"
#endif
