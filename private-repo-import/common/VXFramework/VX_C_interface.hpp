//
//  VX_C_interface.hpp
//  macos
//
//  Created by Adrien Duermael on 2/12/20.
//

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

struct vx_insets {
    float top;
    float bottom;
    float left;
    float right;
};

struct vx_size {
    float width;
    float height;
};

///
void vxapplication_init();

///
void vxapplication_didResize(const uint32_t widthPx,
                             const uint32_t heightPx,
                             const float nbPixelsInOnePoint,
                             struct vx_insets safeAreaInsets);

void vxapplication_didFinishLaunching(void *nativeWindowHandle,
                                      void *eglContext,
                                      uint32_t windowWidth,
                                      uint32_t windowHeight,
                                      float nbPixelsInOnePoint,
                                      struct vx_insets safeAreaInsets,
                                      const char *environment);

bool vxapplication_openURL(const char *url);

void vxapplication_didBecomeActive();
void vxapplication_willResignActive();
void vxapplication_didEnterBackground();
void vxapplication_willEnterForeground();
void vxapplication_willTerminate();
void vxapplication_tick(const double dt);

bool vx_is_mouse_hidden();

void vxapplication_show_keyboard(struct vx_size keyboardSize, float duration);
void vxapplication_hide_keyboard(float duration);

// called when return key is pressed on soft keyboard
// (called even if return key shows "done", "next", etc.)
void vxapplication_soft_keyboard_return_key_up();

///
void vxapplication_didReceiveNotification(const char *title,
                                          const char *body,
                                          const char *category,
                                          uint32_t badge);

///
void vxapplication_didRegisterForRemoteNotifications(const char *tokenTypeCStr,
                                                     const char *tokenValueCStr);

#ifdef __cplusplus
}
#endif
