//  ParticubesWin.cpp : Définit le point d'entrée de l'application Windows.
//
//  Frédéric Nespoulous 07/02/2020
//
//  Copyright © 2020 voxowl. All rights reserved.

#include "ParticubesWin.h"

// C++
#include <cstdlib>
#include <chrono>
#include <iostream>
#include <ostream>
#include <sstream>
#include <thread>

// Windows
#include "targetver.h"
#define WIN32_LEAN_AND_MEAN // Exclure les en-têtes Windows rarement utilisés
#include <malloc.h>
#include <memory.h>
#include <resource.h>
#include <tchar.h>
#include <windows.h>
#include <shellapi.h>

// bgfx
#include <bx/bx.h>
#include <bx/file.h>

// files management
#include <fileapi.h>
#include <Shlobj.h>
#include <sys/stat.h>

// glfw
#include <GLFW/glfw3.h>
#if BX_PLATFORM_LINUX
#define GLFW_EXPOSE_NATIVE_X11
#elif BX_PLATFORM_WINDOWS
#define GLFW_EXPOSE_NATIVE_WIN32
#endif
#include <GLFW/glfw3native.h>

// particubes
#include "GameCoordinator.hpp"
#include "VX_C_interface.hpp"
#include "VXAccount.hpp"
#include "VXApplication.hpp"
#include "VXPrefs.hpp"
#include "xptools.h"

#if DEBUG && DEBUG_UNCAPPED
#define TARGET_TICK_DURATION 0.001f
#else
#define TARGET_TICK_DURATION 0.016f
#endif
#define SLEEP_THRESHOLD_MS 10

// Identifier for Timer used to set focus to the text control
// when the application regains focus
#define APP_FOCUS_TIMER_ID 42

// --------------------------------------------------
//
// Types
//
// --------------------------------------------------

class Pointer final {
  public:
    inline Pointer() : xpos(0), ypos(0), mustIgnoreNextDeltaPosition(true){};
    ~Pointer() = default;
    float xpos;
    float ypos;
    bool mustIgnoreNextDeltaPosition;
};

class KeyInputCombo {
  public:
    inline KeyInputCombo() : keyState(KeyStateUnknown), input(InputNone), keyName(), modifiers(0) {}
    inline KeyInputCombo(KeyState ks, Input in, std::string kn, uint8_t mod)
        : keyState(ks), input(in), keyName(kn), modifiers(mod) {}
    inline ~KeyInputCombo(){};

    KeyState keyState; // down, up, unknown
    Input input;
    std::string keyName;
    uint8_t modifiers;
};

// --------------------------------------------------
//
// Global variables
//
// --------------------------------------------------

// Handle to the application main window
HWND win32Window = nullptr;

bool enableEditChangeNotification = true;
HWND hEditSingleline = nullptr;
HWND hEditMultiline = nullptr;
bool ignoreKillFocus = false;

///
static Input keystoinput[GLFW_KEY_LAST];

/// used for CTRL combos
static std::unordered_map<std::string, Input> _keynameToInput;

/// Used to store a KEY event while we wait for the CHAR event.
static std::shared_ptr<KeyInputCombo> cachedKeyInput = nullptr;

/// Store the pressed state of mouse buttons
static bool _mouseButtonLeftPressed = false;
static bool _mouseButtonRightPressed = false;

// --------------------------------------------------
//
// Function prototypes
//
// --------------------------------------------------
void _setPointer(GLFWwindow *w, const float x, const float y);
void _centerPointer(GLFWwindow *w);
KeyState _convertActionToKeyState(const int action);
int _convertModifiers(int mods);
int _displayFileAccessErrorPopup();
void _initKeyInputTranslations();
void _initKeynameToInputTranslation();
void _loadAndSetAppIcon(GLFWwindow *window);

// this is for redirecting the cout to a VS2019 output
class dbg_stream_for_cout : public std::stringbuf {
  public:
    ~dbg_stream_for_cout() { sync(); }
    int sync() {
        ::OutputDebugStringA(str().c_str());
        str(std::string()); // Clear the string buffer
        return 0;
    }
};
dbg_stream_for_cout g_DebugStreamFor_cout;

// Global variables
HINSTANCE hInst; // current instance of the app

static GLFWwindow *window = nullptr;
// actual dimensions of the window
static int width = 0;
static int height = 0;
// last dimensions when the app was windowed
static int windowedWidth = 0;
static int windowedHeight = 0;

// Pixel density (number of pixels per point)
static float pixelsPerPoint = 1.0f;

static Pointer _pointer;
static bool isMouseHidden = false;
static void createDirectoryRecursively(const std::wstring &directory);

static void switchToFullscreen();
static bool _fullscreenOn = false;

// Déclarations anticipées des fonctions incluses dans ce module de code :
BOOL InitInstance(HINSTANCE, int, int);

int WINAPI wWinMain(_In_ HINSTANCE hInstance,
                    _In_opt_ HINSTANCE hPrevInstance,
                    _In_ LPWSTR lpCmdLine,
                    _In_ int nCmdShow) {

    // timeBeginPeriod(1);
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    // Install the app bundle
    const bool ok = vx::fs::unzipBundle();
    if (ok == false) {
        // track this event to see if it is happening
        vx::tracking::TrackingClient::shared().trackEvent("windowsFailedToUnzipBundle");
        _displayFileAccessErrorPopup();
        return false;
    }

    // Get command line parameters
    int rendererType = -1;
    int argsCount = 0;
    LPWSTR *args = CommandLineToArgvW(lpCmdLine, &argsCount);
    if (args != nullptr) {
        for (int i = 0; i < argsCount; ++i) {
            if (wcscmp(L"--dx11", args[i]) == 0) {
                rendererType = 1;
            } else if (wcscmp(L"--opengl", args[i]) == 0) {
                rendererType = 4;
            } else if (wcscmp(L"--vulkan", args[i]) == 0) {
                rendererType = 7;
            }
        }
    }

    // Windows app init
    if (!InitInstance(hInstance, nCmdShow, rendererType)) {
        return false; // get OUT !
    }

    // variables needed for delta time computation
    std::chrono::time_point<std::chrono::steady_clock>
        startTime = std::chrono::high_resolution_clock::now();
    std::chrono::time_point<std::chrono::steady_clock>
        currentTime = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> elapsedTime = std::chrono::duration<double>(0.0);

    // Main Loop

    while (glfwWindowShouldClose(window) == false) {
        glfwPollEvents(); // frd pour pouvoir fermer la fenêtre directement

        // Handle window resize.
        int oldWidth = width, oldHeight = height;
        glfwGetWindowSize(window, &width, &height);
        if (width != oldWidth || height != oldHeight) {
            if (vx::Prefs::shared().fullscreen() == false) {
                // update the windowed dimensions
                windowedWidth = width;
                windowedHeight = height;
            }
            vx::VXApplication::getInstance()->didResize(width,
                                                        height,
                                                        pixelsPerPoint,
                                                        vx::Insets{0, 0, 0, 0});
        }

        if (_fullscreenOn != vx::Prefs::shared().fullscreen()) {
            if (vx::Prefs::shared().fullscreen()) {
                switchToFullscreen();
            } else {
                // go back to previous dimensions
                width = windowedWidth;
                height = windowedHeight;
                glfwSetWindowMonitor(window, nullptr, 50, 50, width, height, GLFW_DONT_CARE);
            }
            vx::VXApplication::getInstance()->didResize(width,
                                                        height,
                                                        pixelsPerPoint,
                                                        vx::Insets{0, 0, 0, 0});
            _fullscreenOn = vx::Prefs::shared().fullscreen();
        }

        // compute delta time
        currentTime = std::chrono::high_resolution_clock::now();
        elapsedTime = currentTime - startTime;

        // enforce target framerate
        while (elapsedTime.count() < TARGET_TICK_DURATION) {
            elapsedTime = std::chrono::high_resolution_clock::now() - startTime;
        }
        startTime = std::chrono::high_resolution_clock::now();

        // Note: sleep_for seems to frequently overshoot because the sleep time seems to be:
        // - at around the double of what we ask,
        // - at a minimum ~10ms whatever we ask...
        // In the commented code below, I tried using a single sleep if we can (more than
        // SLEEP_THRESHOLD_MS remaining),
        // - if so we ask for the remaining time divided by 2 or 4... to account for sleep_for
        // vastly overshooting,
        // - else if remaining time is small, we revert to a good old while to wait precisely til
        // the target time This still didn't work, I could observe frames with slept time of 20+ ms
        // eventhough we ask for 7ms...

        /*
        while (elapsedTime.count() < TARGET_TICK_DURATION) {
            auto timeToSleep = std::chrono::duration<float>(TARGET_TICK_DURATION) - elapsedTime;
            int milli = (int)(timeToSleep.count() * 1000.0f);
            vxlog(LOG_SEVERITY_TRACE, LOG_COMPONENT, "MILLI %d", milli);

            if (milli > SLEEP_THRESHOLD_MS) {
                vxlog(LOG_SEVERITY_TRACE, LOG_COMPONENT, "SLEEP FOR %d", milli / 4);
                auto start = std::chrono::high_resolution_clock::now();
                std::this_thread::sleep_for(std::chrono::milliseconds(milli / 4));
                auto end = std::chrono::high_resolution_clock::now();
                std::chrono::duration<float, std::milli> sleepTime = end - start;
                vxlog(LOG_SEVERITY_TRACE, LOG_COMPONENT, "SLEPT FOR %d", (int)sleepTime.count());
                elapsedTime = std::chrono::high_resolution_clock::now() - startTime;
            } else {
                vxlog(LOG_SEVERITY_TRACE, LOG_COMPONENT, "WHILE");
                while (elapsedTime.count() < TARGET_TICK_DURATION) {
                    elapsedTime = std::chrono::high_resolution_clock::now() - startTime;
                }
            }
        }
        vxlog(LOG_SEVERITY_TRACE, LOG_COMPONENT, "TICK %f
        =============================================================", elapsedTime.count());
        */

        vx::VXApplication::getInstance()->tick(elapsedTime.count());
        vx::VXApplication::getInstance()->render();

        const bool wantMouseCursorHidden = vx_is_mouse_hidden();
        if (isMouseHidden != wantMouseCursorHidden) {
            isMouseHidden = wantMouseCursorHidden;
            if (isMouseHidden) {
                // hide the cursor
                glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
            } else {
                // if cursor becomes visible, we must ignore the delta position since the moment it
                // was hidden
                _pointer.mustIgnoreNextDeltaPosition = true;
                // show the cursor
                glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
            }
        }
        if (isMouseHidden) {
            // keep the cursor on the center of the window
            // when not shown to avoid Button's onHover
            _centerPointer(window);
        }
    }

    vx::VXApplication::getInstance()->willTerminate();

    return true;
}

// SPACE KEY = key event but no char event
static KeyInputCombo handleKeyEvent(const int glfw_key,
                                    const int glfw_scancode,
                                    const int glfw_action,
                                    const int glfw_modifiers) {

    //
    const KeyState keyState = _convertActionToKeyState(glfw_action);

    // Obtain key name
    // It will be used as the printable characters of the KeyInputCombo
    std::string keyName;
    {
        const char *keyNameCStr = glfwGetKeyName(glfw_key, glfw_scancode);
        if (keyNameCStr != nullptr) {
            keyName.assign(keyNameCStr);
        }
    }
    if (keyName.empty()) {
        if (glfw_key == GLFW_KEY_LEFT_SHIFT || glfw_key == GLFW_KEY_RIGHT_SHIFT) {
            keyName = "shift";
        } else if (glfw_key == GLFW_KEY_LEFT_CONTROL || glfw_key == GLFW_KEY_RIGHT_CONTROL) {
            keyName = "ctrl";
        } else if (glfw_key == GLFW_KEY_LEFT_ALT || glfw_key == GLFW_KEY_RIGHT_ALT) {
            keyName = "alt";
        } else if (glfw_key == GLFW_KEY_LEFT_SUPER || glfw_key == GLFW_KEY_RIGHT_SUPER) {
            keyName = "super";
        }
    }

    // translate key input
    const Input input = keystoinput[glfw_key];

    // translate modifiers
    const int modifiers = _convertModifiers(glfw_modifiers);

    // Construct result
    KeyInputCombo r;
    r.keyState = keyState;
    r.keyName = keyName;
    r.input = input;
    r.modifiers = modifiers;

    return r;
}

// callback for keyboard input
// this receives the key number in US QWERTY keyboard
// press Z on Azerty keyboard says "key W pressed"
// press Q on Azerty keyboard says "key A pressed"
static void keyCallback(GLFWwindow *w, int key, int scancode, int action, int modifiers) {

    //
    KeyInputCombo ki = handleKeyEvent(key, scancode, action, modifiers);

    // Catch F11 key presses for fullscreen switch
    if (ki.keyState == KeyStateDown && ki.input == InputF11) {
        vxlog_debug("F11 detected");

        // toggle fullscreen
        vx::Prefs::shared().setFullscreen(vx::Prefs::shared().fullscreen() == false);

        // update app window
        if (vx::Prefs::shared().fullscreen()) {
            switchToFullscreen();
        } else { // windowed
            // go back to previous dimensions
            width = windowedWidth;
            height = windowedHeight;
            _fullscreenOn = false;

            // the 50 / 50 offset is set so we can see the top Windows bar
            glfwSetWindowMonitor(window, nullptr, 50, 50, width, height, GLFW_DONT_CARE);
        }

        // trigger didResize
        vx::VXApplication::getInstance()->didResize(width,
                                                    height,
                                                    pixelsPerPoint,
                                                    vx::Insets{0, 0, 0, 0});
    }

    if (ki.input != InputNone) {

        // Key down
        if (ki.keyState == KeyStateDown) {

            bool isPrintableChar = inputs_is_key_printable(ki.input);

            // post key input

            // handle CTRL combos
            if (ki.modifiers == (ModifierCtrl | ModifierAlt)) {
                // modifiers: CTRL + ALT (AltGr key does that)
            } else if (ki.modifiers == ModifierCtrl) {
                // modifiers: CTRL only
                const char *keyNameCStr = glfwGetKeyName(key, scancode);
                if (keyNameCStr != nullptr) {
                    std::string keyName(keyNameCStr);
                    // vxlog_debug("           key name: %s", keyName.c_str());
                    if (_keynameToInput.find(keyName) != _keynameToInput.end()) {
                        ki.input = _keynameToInput.at(keyName);

                        // we set this to false, so that the keyboard input is posted right away
                        // (since the system char callback won't be called)
                        isPrintableChar = false;
                    }
                }
            }

            postKeyEvent(ki.input, ki.modifiers, ki.keyState);

            // vxlog_debug("[KEY] ===> %d %d", ki.input, ki.modifiers);
            // vxlog_debug("           %s", isPrintableChar ? "printable" : "not printable");

            if (isPrintableChar) {
                // vxlog_debug("           cached");
                // store key input, waiting for the glfw char event callback
                KeyInputCombo *kiCopyPtr = new KeyInputCombo(ki);
                cachedKeyInput = std::shared_ptr<KeyInputCombo>(kiCopyPtr);

            } else {
                // vxlog_debug("           posted");
                postKeyboardInput(0, ki.input, ki.modifiers, ki.keyState);
            }

            // NOTE : we are obligated to consume the event on Windows
            // do not consume event when down key is a modifier
            // if ((r.modifiers & (uint8_t)(ModifierSuper)) > 0 ) {}

        } else if (ki.keyState == KeyStateUp) {

            postKeyEvent(ki.input, ki.modifiers, ki.keyState);
            postKeyboardInput(0, ki.input, ki.modifiers, ki.keyState);
        }
    } else { // input is InputNone

        // A modifier key has been pressed or released
        if (ki.keyName == "shift" || ki.keyName == "ctrl" || ki.keyName == "alt" ||
            ki.keyName == "super") {

            uint8_t modifiers = ki.modifiers;

            if (ki.keyName == "shift") {
                modifiers |= ModifierShift;
            } else if (ki.keyName == "ctrl") {
                modifiers |= ModifierCtrl;
            } else if (ki.keyName == "alt") {
                modifiers |= ModifierAlt;
            } else if (ki.keyName == "super") {
                modifiers |= ModifierSuper;
            }

            postKeyEvent(InputNone, modifiers, ki.keyState);
            postKeyboardInput(0, InputNone, modifiers, ki.keyState);
        }

        // store key input, waiting for the glfw char event callback
        KeyInputCombo *kiCopyPtr = new KeyInputCombo(ki);
        cachedKeyInput = std::shared_ptr<KeyInputCombo>(kiCopyPtr);
    }
}

// callback for char input : no modifiers key detected (alt, SUPER, Win)
// action is always press
// shifted chars are received
static void charCallback(GLFWwindow *window, unsigned int codepoint) {
    // vxlog_debug("[CHAR] ==> %d %c", codepoint, static_cast<char>(codepoint));

    // always post the char event
    postCharEvent(codepoint);

    // if there was a key event stored, then post keyboard event as well
    if (cachedKeyInput != nullptr) {
        // vxlog_debug("[COMBO]");
        postKeyboardInput(codepoint,
                          cachedKeyInput->input,
                          cachedKeyInput->modifiers,
                          cachedKeyInput->keyState);
        cachedKeyInput = nullptr; // consume cached input
    }
    /*else {
        vxlog_debug("[CHAR ONLY]");
        postKeyboardInput(codepoint, InputNone, 0, KeyStateUnknown);
    }*/
}

// NOTE: mouse events are now received in pixels (not points anymore)
static void cursorPositionCallback(GLFWwindow *window, double xpos, double ypos) {
    static float xposf;
    static float yposf;
    static float dx;
    static float dy;

    // convert to float
    xposf = static_cast<float>(xpos);
    yposf = static_cast<float>(ypos);

    // invert Y position
    yposf = static_cast<float>(height) - yposf;

    // compute delta position since last event
    dx = xposf - _pointer.xpos;
    dy = yposf - _pointer.ypos;

    if (_pointer.mustIgnoreNextDeltaPosition == true) {
        dx = 0.0f;
        dy = 0.0f;
        _pointer.mustIgnoreNextDeltaPosition = false;
    }

    PointerID pointerIDToUse = PointerIDMouse;
    if (_mouseButtonLeftPressed) {
        pointerIDToUse = PointerIDMouseButtonLeft;
    } else if (_mouseButtonRightPressed) {
        pointerIDToUse = PointerIDMouseButtonRight;
    }

    postPointerEvent(pointerIDToUse,
                     PointerEventTypeMove,
                     xposf,
                     yposf,
                     dx, // apply scale factor ?
                     dy  // apply scale factor ?
    );

    postMouseEvent(xposf, yposf, dx, dy, MouseButtonNone, false, true);

    _pointer.xpos = xposf;
    _pointer.ypos = yposf;
}

// Mouse wheel callback
static void scrollCallback(GLFWwindow *window, double xoffset, double yoffset) {
    postPointerEvent(PointerIDMouse,
                     PointerEventTypeWheel,
                     0.0f,
                     0.0f,
                     0.0f,
                     static_cast<float>(yoffset) * 15.0f);

    postMouseEvent(_pointer.xpos,
                   _pointer.ypos,
                   static_cast<float>(xoffset * 15),
                   static_cast<float>(yoffset * 15),
                   MouseButtonScroll,
                   false,
                   false);
}

static void mouseButtonCallback(GLFWwindow *window, int button, int action, int mods) {

    const bool down = (action == GLFW_PRESS);
    const PointerEventType pointerEventType = down ? PointerEventTypeDown : PointerEventTypeUp;
    PointerID pointerID = PointerIDMouse; // no specific button
    MouseButton mb = MouseButtonLeft;

    // left right middle to left middle right
    if (button == GLFW_MOUSE_BUTTON_LEFT) {
        pointerID = PointerIDMouseButtonLeft;
        _mouseButtonLeftPressed = down; // update cached button state
    } else if (button == GLFW_MOUSE_BUTTON_RIGHT) {
        pointerID = PointerIDMouseButtonRight;
        mb = MouseButtonRight;
        _mouseButtonRightPressed = down; // update caches button state
    } else if (button == GLFW_MOUSE_BUTTON_MIDDLE) {
        pointerID = PointerIDMouse; // TODO: gaetan: should this be different?
        mb = MouseButtonMiddle;
    }

    postPointerEvent(pointerID,
                     pointerEventType,
                     _pointer.xpos,
                     _pointer.ypos,
                     _pointer.xpos,
                     _pointer.ypos);

    postMouseEvent(_pointer.xpos, _pointer.ypos, _pointer.xpos, _pointer.ypos, mb, down, false);
}

// --------------------------------------------------
// String conversion functions
// --------------------------------------------------

// Converts a UTF-8 string to a wide string
std::wstring _convertUTF8StringToWideString(const std::string &str) {
    if (str.empty()) {
        return std::wstring();
    }

    // Get the length of the resulting wide string
    const int size_needed = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, nullptr, 0);
    if (size_needed <= 0) {
        throw std::runtime_error("Error converting UTF-8 string to wide string.");
    }

    // Allocate a buffer for the resulting wide string
    std::wstring wstr(size_needed, 0);

    // Perform the conversion
    const int bytes_written = MultiByteToWideChar(CP_UTF8,
                                                  0,
                                                  str.c_str(),
                                                  -1,
                                                  &wstr[0],
                                                  size_needed);
    if (bytes_written <= 0) {
        throw std::runtime_error("Error converting UTF-8 string to wide string.");
    }

    // Remove the null terminator added by MultiByteToWideChar
    wstr.resize(bytes_written - 1);

    return wstr;
}

typedef struct {
    std::wstring wStr;
    size_t wStart;
    size_t wEnd;
} WideString;

// Converts a UTF-8 string to a wide string
WideString ConvertUTF8StringToWideString(const std::string &str,
                                         const size_t start,
                                         const size_t end) {
    WideString result = {L"", 0, 0};

    if (str.empty()) {
        return result;
    }

    // convert string
    result.wStr = _convertUTF8StringToWideString(str);

    // convert cursors positions

    // string from begining to first cursor
    std::string beforeCursorUTF8 = str.substr(0, start);
    std::wstring beforeCursorWide = _convertUTF8StringToWideString(beforeCursorUTF8);
    result.wStart = beforeCursorWide.size();

    if (start == end) {
        result.wEnd = result.wStart;
    } else {
        // string from first cursor to second cursor
        std::string betweenCursorsUTF8 = str.substr(start, end - start);
        std::wstring betweenCursorsWide = _convertUTF8StringToWideString(betweenCursorsUTF8);
        result.wEnd = result.wStart + betweenCursorsWide.size();
    }

    return result;
}

// Converts a wide string to a UTF-8 string
std::string _convertWideStringToUTF8String(const std::wstring &wstr) {
    if (wstr.empty()) {
        return std::string();
    }

    // Get the length of the resulting UTF-8 string
    const int size_needed = WideCharToMultiByte(CP_UTF8,
                                                0,
                                                wstr.c_str(),
                                                -1,
                                                nullptr,
                                                0,
                                                nullptr,
                                                nullptr);
    if (size_needed <= 0) {
        throw std::runtime_error("Error converting wide string to UTF-8 string.");
    }

    // Allocate a buffer for the resulting UTF-8 string
    std::string utf8Str(size_needed, 0);

    // Perform the conversion
    const int bytes_written = WideCharToMultiByte(CP_UTF8,
                                                  0,
                                                  wstr.c_str(),
                                                  -1,
                                                  &utf8Str[0],
                                                  size_needed,
                                                  nullptr,
                                                  nullptr);
    if (bytes_written <= 0) {
        throw std::runtime_error("Error converting wide string to UTF-8 string.");
    }

    // Remove the null terminator added by WideCharToMultiByte
    utf8Str.resize(bytes_written - 1);

    return utf8Str;
}

typedef struct {
    std::string uStr;
    size_t uStart;
    size_t uEnd;
} UTF8String;

// Converts a UTF-8 string to a wide string
UTF8String ConvertWideStringToUTF8String(const std::wstring &wstr,
                                         const unsigned int start,
                                         const unsigned int end) {
    UTF8String result = {"", 0, 0};

    if (wstr.empty()) {
        return result;
    }

    // convert string
    result.uStr = _convertWideStringToUTF8String(wstr);

    // convert cursors positions

    // string from begining to first cursor
    std::wstring beforeCursorWide = wstr.substr(0, start);
    std::string beforeCursorUTF8 = _convertWideStringToUTF8String(beforeCursorWide);
    result.uStart = beforeCursorUTF8.size();

    if (start == end) {
        result.uEnd = result.uStart;
    } else {
        // string from first cursor to second cursor
        std::wstring betweenCursorsWide = wstr.substr(start, end - start);
        std::string betweenCursorsUTF8 = _convertWideStringToUTF8String(betweenCursorsWide);
        result.uEnd = result.uStart + betweenCursorsUTF8.size();
    }

    return result;
}

// --------------------------------------------------
// Edit control utility functions
// --------------------------------------------------

// last values sent to Blip code
std::string currentText;
static std::pair<int, int> currentCursorPosition = std::make_pair(0, 0);

// Returns the cursor position for the edit control provided
// (wide string value, not UTF8 bytes)
std::pair<int, int> _getEditCursorPosition(HWND hwndEdit) {
    if (hwndEdit == nullptr) {
        return std::make_pair(0, 0);
    }

    // Send EM_GETSEL message to get the current selection or caret position
    DWORD start = 0;
    DWORD end = 0;
    SendMessage(hwndEdit, EM_GETSEL, (WPARAM)&start, (LPARAM)&end);

    return std::make_pair(start, end);
}

// Returns true if the selection has changed
bool _updateCursorPosition(const size_t start, const size_t end) {
    if (currentCursorPosition.first == start && currentCursorPosition.second == end) {
        return false;
    }
    currentCursorPosition = std::make_pair(start, end);
    return true;
}

std::wstring _getEditWideString(HWND hwndEdit) {
    const int length = GetWindowTextLength(hwndEdit);
    std::wstring result(length, '\n');
    GetWindowText(hwndEdit, &result[0], length + 1);
    return result;
}

bool _updateText(const std::string &newText) {
    if (newText == currentText) {
        return false;
    }
    currentText = newText;
    return true;
}

void notifyTextOrCursorDidChangeIfNeeded(HWND hwndEdit) {
    if (hwndEdit != GetFocus() || (hwndEdit != hEditSingleline && hwndEdit != hEditMultiline)) {
        vxlog_debug(
            "[!][notifyTextDidChangeIfNeeded] SKIPPED - not focused or not our text control");
        return;
    }

    // get text content
    const std::wstring editControlWideString = _getEditWideString(hwndEdit);

    // get cursor position
    const std::pair<int, int> editControlCursorPos = _getEditCursorPosition(hwndEdit);

    // convert from wide string to UTF8 string
    const UTF8String utf8String = ConvertWideStringToUTF8String(editControlWideString,
                                                                editControlCursorPos.first,
                                                                editControlCursorPos.second);

    const bool updatedCursor = _updateCursorPosition(utf8String.uStart, utf8String.uEnd);
    const bool updatedText = _updateText(utf8String.uStr);

    if (updatedText || updatedCursor) {
        // Send text/cursor update to the app
        hostPlatformTextInputUpdate(currentText.c_str(),
                                    currentText.length(),
                                    currentCursorPosition.first,
                                    currentCursorPosition.second);
    }
}

void textInputRequestCallback(const char *str,
                              size_t strLen,
                              bool strDidchange,
                              size_t cursorStart,
                              size_t cursorEnd,
                              bool multiline,
                              TextInputKeyboardType keyboardType,
                              TextInputReturnKeyType returnKeyType,
                              bool suggestions) {
    // Convert string from UTF8 to wide string
    WideString wideString = ConvertUTF8StringToWideString(std::string(str, strLen),
                                                          cursorStart,
                                                          cursorEnd);

    ignoreKillFocus = true;
    if (multiline) {
        // Focus the multiline edit control
        SetFocus(hEditMultiline);

        // Set the text in the edit control
        SetWindowText(hEditMultiline, wideString.wStr.c_str());

        // Set the cursor position
        SendMessage(hEditMultiline, EM_SETSEL, wideString.wStart, wideString.wEnd);

    } else {
        // singleline
        SetFocus(hEditSingleline);

        // Set the text in the edit control
        SetWindowText(hEditSingleline, wideString.wStr.c_str());

        // Set the cursor position
        SendMessage(hEditSingleline, EM_SETSEL, wideString.wStart, wideString.wEnd);
    }
}

void textInputUpdateCallback(const char *str,
                             size_t strLen,
                             bool strDidchange,
                             size_t cursorStart,
                             size_t cursorEnd) {
    // Get the edit control that has the focus
    HWND hEdit = GetFocus();
    if (hEdit == hEditSingleline || hEdit == hEditMultiline) {
        enableEditChangeNotification = false;

        // Convert string from UTF8 to wide string
        WideString wideString = ConvertUTF8StringToWideString(std::string(str, strLen),
                                                              cursorStart,
                                                              cursorEnd);

        // Set the text in the edit control
        SetWindowText(hEdit, wideString.wStr.c_str());

        // Set the cursor position
        SendMessage(hEdit, EM_SETSEL, wideString.wStart, wideString.wEnd);

        enableEditChangeNotification = true;
    }
}

void textInputActionCallback(TextInputAction action) {

    HWND edit = GetFocus();
    if (edit != hEditSingleline && edit != hEditMultiline) {
        vxlog_debug(">>>>> INPUT ACTION UNKNOWN EDIT IS FOCUSED %p", edit);
        return;
    }

    switch (action) {
        case TextInputAction_Close: {
            // Unfocus the edit control (we have to focus the main window in order to do that)
            if (win32Window != nullptr) {
                SetFocus(win32Window);
            }
        } break;
        case TextInputAction_Copy:
            vxlog_debug(">>>>> INPUT ACTION %s", "COPY");
            break;
        case TextInputAction_Paste:
            vxlog_debug(">>>>> INPUT ACTION %s", "PASTE");
            break;
        case TextInputAction_Cut:
            vxlog_debug(">>>>> INPUT ACTION %s", "CUT");
            break;
        case TextInputAction_Undo:
            vxlog_debug(">>>>> INPUT ACTION %s", "UNDO");
            break;
        case TextInputAction_Redo:
            vxlog_debug(">>>>> INPUT ACTION %s", "REDO");
            break;
    }
}

// --------------------------------------------------
// Native (Windows) implementation of text inputs
// --------------------------------------------------

#define EDIT_CONTROL_SINGLELINE_ID 1
#define EDIT_CONTROL_MULTILINE_ID 2

// original event callback rer the main window
WNDPROC originalMainWindowProc;
// original event callback for the single line edit control
WNDPROC originalSinglelineProc;
// original event callback for the multi line edit control
WNDPROC originalMultilineProc;

HWND lastEditControlFocused = nullptr;

LRESULT CALLBACK customMainWindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {

    if (enableEditChangeNotification == false) {
        return CallWindowProc(originalSinglelineProc, hwnd, uMsg, wParam, lParam);
    }

    switch (uMsg) {
        case WM_ACTIVATEAPP: {
            // Handle app focus changes
            const bool isActive = wParam != FALSE;
            if (isActive) {
                vx::VXApplication::getInstance()->willEnterForeground();
                vx::VXApplication::getInstance()->didBecomeActive();

                if (lastEditControlFocused != nullptr &&
                    (lastEditControlFocused == hEditSingleline ||
                     lastEditControlFocused == hEditMultiline)) {
                    // application regained focus, start a timer to set focus later
                    // to the edit control (if done right away, it doesn't work)
                    const UINT delayMs = 100;
                    SetTimer(hwnd, APP_FOCUS_TIMER_ID, delayMs, nullptr);
                }

            } else {
                lastEditControlFocused = nullptr;
                const HWND focused = GetFocus();
                if (focused != nullptr &&
                    (focused == hEditSingleline || focused == hEditMultiline)) {
                    lastEditControlFocused = focused;
                    // vxlog_debug("[APP][TEXT INPUT][SAVED FOCUS] %p", focused);
                }
                vx::VXApplication::getInstance()->willResignActive();
                vx::VXApplication::getInstance()->didEnterBackground();
            }
        } break;
        case WM_TIMER: {
            if (wParam == APP_FOCUS_TIMER_ID) {
                // Timer triggered, set focus to the text control
                HWND lastEditControl = lastEditControlFocused;
                if (lastEditControl != nullptr &&
                    (lastEditControl == hEditSingleline || lastEditControl == hEditMultiline)) {
                    SetFocus(lastEditControl);
                    // vxlog_debug("[APP][TEXT INPUT][RESTORED FOCUS] %p", lastEditControlFocused);
                    lastEditControlFocused = nullptr;
                }
                KillTimer(hwnd, APP_FOCUS_TIMER_ID); // Stop the timer
            }
            return 0;
        } break;
        case WM_COMMAND: {
            WORD hiWordWParam = HIWORD(wParam);
            if (hiWordWParam == EN_CHANGE) {
                HWND hEdit = (HWND)lParam;
                HWND focused = GetFocus();
                if (focused == hEdit && (focused == hEditSingleline || focused == hEditMultiline)) {
                    notifyTextOrCursorDidChangeIfNeeded(hEdit);
                }
            }
        } break;
        case WM_KILLFOCUS:
            // SetFocus() on a window (eg. edit texts) makes the previously focused window recieve this event,
            // in this case, absorb it to keep the app focused ; otherwise it may be from an ALT+TAB,
            // let it be processed normally
            if (ignoreKillFocus) {
                ignoreKillFocus = false;
                return 0;
            }
            break;
        // case WM_KEYDOWN:
        // case WM_KEYUP:
        //     return 0; // Suppress default processing
        //     break;
    }
    return CallWindowProc(originalMainWindowProc, hwnd, uMsg, wParam, lParam);
}

// Custom event callback for the single line edit control
LRESULT CALLBACK customSinglelineProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {

    if (enableEditChangeNotification == false) {
        return CallWindowProc(originalSinglelineProc, hwnd, uMsg, wParam, lParam);
    }

    HWND editControl = hEditSingleline;

    switch (uMsg) {
        case WM_KEYDOWN: {
            if (GetFocus() == editControl) {
                // cursor/selection may have changed
                notifyTextOrCursorDidChangeIfNeeded(editControl);

                if (wParam == VK_RETURN) {
                    hostPlatformTextInputDone();
                    return 0; // suppress default processing
                } else if (wParam == VK_ESCAPE) {
                    hostPlatformTextInputClose();
                    return 0; // suppress default processing
                } else if ((wParam == 'A') && (GetKeyState(VK_CONTROL) & 0x8000)) {
                    // Ctrl+A is pressed
                    SendMessage(hwnd, EM_SETSEL, 0, -1);
                    return 0; // suppress default processing
                } else if (wParam == VK_UP || wParam == VK_DOWN) {
                    Input input = (wParam == VK_UP) ? InputUp : InputDown;
                    postKeyEvent(input, 0, KeyStateDown);
                    postKeyboardInput(0, input, 0, KeyStateDown);
                    return 0; // suppress default processing
                }
            }
        } break;
        case WM_CHAR: {
            if ((wParam == 1) && (GetKeyState(VK_CONTROL) & 0x8000)) {
                // prevents ding sound
                return 0;
            }
        }
        case WM_KEYUP: {
            if (wParam == VK_RETURN) {
                // prevent ding sound
                return 0;
            }
        }
        case WM_LBUTTONDOWN:
        case WM_LBUTTONUP:
        case WM_LBUTTONDBLCLK:
        case WM_RBUTTONDOWN:
        case WM_RBUTTONUP:
        case WM_RBUTTONDBLCLK:
        case WM_MBUTTONDOWN:
        case WM_MBUTTONUP:
        case WM_MBUTTONDBLCLK:
        case EM_SETSEL: {
            // cursor/selection may have changed
            if (GetFocus() == editControl) {
                notifyTextOrCursorDidChangeIfNeeded(editControl);
            }
        } break;
    }

    return CallWindowProc(originalSinglelineProc, hwnd, uMsg, wParam, lParam);
}

// custom event callback for the multi line edit control
LRESULT CALLBACK customMultilineProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {

    if (enableEditChangeNotification == false) {
        return CallWindowProc(originalSinglelineProc, hwnd, uMsg, wParam, lParam);
    }

    HWND editControl = hEditMultiline;

    switch (uMsg) {
        case WM_KEYDOWN: {
            if (GetFocus() == editControl) {
                // cursor/selection may have changed
                notifyTextOrCursorDidChangeIfNeeded(editControl);

                if (wParam == VK_RETURN) {
                    hostPlatformTextInputDone();
                    return 0; // Suppress default processing
                } else if (wParam == VK_ESCAPE) {
                    hostPlatformTextInputClose();
                    return 0; // Suppress default processing
                } else if ((wParam == 'A') && (GetKeyState(VK_CONTROL) & 0x8000)) {
                    // Ctrl+A is pressed
                    SendMessage(hwnd, EM_SETSEL, 0, -1);
                    // suppress further processing to prevent ding sound
                    return 0;
                }
            }
        } break;
        case WM_CHAR: {
            if ((wParam == 1) && (GetKeyState(VK_CONTROL) & 0x8000)) {
                // prevent ding sound
                return 0;
            }
        }
        case WM_KEYUP:
        case WM_LBUTTONDOWN:
        case WM_LBUTTONUP:
        case WM_LBUTTONDBLCLK:
        case WM_RBUTTONDOWN:
        case WM_RBUTTONUP:
        case WM_RBUTTONDBLCLK:
        case WM_MBUTTONDOWN:
        case WM_MBUTTONUP:
        case WM_MBUTTONDBLCLK:
        case EM_SETSEL: {
            // cursor/selection may have changed
            if (GetFocus() == editControl) {
                notifyTextOrCursorDidChangeIfNeeded(editControl);
            }
        } break;
    }

    return CallWindowProc(originalMultilineProc, hwnd, uMsg, wParam, lParam);
}

//
//   FONCTION : InitInstance(HINSTANCE, int, int)
//
//   OBJECTIF : enregistre le handle d'instance et crée une fenêtre principale
//
//   COMMENTAIRES :
//        Dans cette fonction, nous enregistrons le handle de l'instance dans
//        une variable globale, puis nous créons et affichons la fenêtre
//        principale du programme.
//
BOOL InitInstance(HINSTANCE hInstance, int nCmdShow, int rendererType) {

    // Stocke le handle d'instance dans la variable globale
    hInst = hInstance;

    // Redirect std::cout to OutputDebugString!
    std::cout.rdbuf(&g_DebugStreamFor_cout);

    _initKeyInputTranslations();
    _initKeynameToInputTranslation();

    glfwInit();

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

    // Obtain the primary monitor size/density/scale
    GLFWmonitor *primaryMonitor = glfwGetPrimaryMonitor();
    // int monitorWidth = 0; // in millimeters
    // int monitorHeight = 0; // in millimeters
    // glfwGetMonitorPhysicalSize(primaryMonitor, &monitorWidth, &monitorHeight);
    float monitorXScale = 0.0f;
    float monitorYScale = 0.0f;
    glfwGetMonitorContentScale(primaryMonitor, &monitorXScale, &monitorYScale);
    pixelsPerPoint = monitorXScale;

    // default window size
    width = 800 * static_cast<int>(monitorXScale);
    height = 600 * static_cast<int>(monitorYScale);
    windowedWidth = width;
    windowedHeight = height;

    window = glfwCreateWindow(width, height, "Blip", nullptr, nullptr);
    if (window == nullptr) {
        return false;
    }

    // ------------------------------
    // Native text field
    // ------------------------------

    // Retrieve native win32 window from GLFW window
    win32Window = glfwGetWin32Window(window);

    originalMainWindowProc = (WNDPROC)SetWindowLongPtr(win32Window,
                                                       GWLP_WNDPROC,
                                                       (LONG_PTR)customMainWindowProc);

    // Create text field (single line)
    // ES_AUTOHSCROLL : allow text overflow
    // https://learn.microsoft.com/en-us/windows/win32/controls/about-edit-controls#scrolling-styles
    // WS_VISIBLE : make the edit control visible
    hEditSingleline = CreateWindowEx(0,
                                     L"EDIT", // Predefined class; Unicode assumed
                                     NULL,    // No window name
                                     WS_CHILD | ES_LEFT |
                                         ES_AUTOHSCROLL, // | WS_VISIBLE // field styles
                                     0,                  // X
                                     100,                // Y
                                     400,                // Width
                                     25,                 // Height
                                     win32Window,        // Parent window
                                     (HMENU)EDIT_CONTROL_SINGLELINE_ID, // Edit control ID
                                     (HINSTANCE)GetWindowLongPtr(win32Window, GWLP_HINSTANCE),
                                     NULL); // Pointer not needed

    // Set the text limit to a much higher value (10 million characters)
    // This allows for much larger code files while still preventing excessive memory usage
    SendMessage(hEditSingleline, EM_LIMITTEXT, (WPARAM)10000000, 0);

    // Subclass the edit control
    originalSinglelineProc = (WNDPROC)SetWindowLongPtr(hEditSingleline,
                                                       GWLP_WNDPROC,
                                                       (LONG_PTR)customSinglelineProc);

    // Create text field (multi line)
    // ES_AUTOHSCROLL : allow horizontal text overflow (disables line wrap)
    // https://learn.microsoft.com/en-us/windows/win32/controls/about-edit-controls#scrolling-styles
    // WS_VISIBLE : make the edit control visible
    hEditMultiline = CreateWindowEx(0,
                                    L"EDIT", // Predefined class; Unicode assumed
                                    NULL,    // No window title
                                    WS_CHILD | WS_BORDER | ES_LEFT | ES_MULTILINE | ES_AUTOVSCROLL |
                                        ES_AUTOHSCROLL,               // Text field styles
                                    0,                                // X
                                    100,                              // Y
                                    400,                              // Width
                                    100,                              // Height
                                    win32Window,                      // Parent window
                                    (HMENU)EDIT_CONTROL_MULTILINE_ID, // Edit control ID
                                    (HINSTANCE)GetWindowLongPtr(win32Window, GWLP_HINSTANCE),
                                    NULL); // Pointer not needed

    // Set the text limit to a much higher value (10 million characters)
    // This allows for much larger code files while still preventing excessive memory usage
    SendMessage(hEditMultiline, EM_LIMITTEXT, (WPARAM)10000000, 0);

    // Subclass the edit control
    originalMultilineProc = (WNDPROC)SetWindowLongPtr(hEditMultiline,
                                                      GWLP_WNDPROC,
                                                      (LONG_PTR)customMultilineProc);

    hostPlatformTextInputRegisterDelegate(textInputRequestCallback,
                                          textInputUpdateCallback,
                                          textInputActionCallback);

    // Load app icon and assign it to the window
    _loadAndSetAppIcon(window);
    
    // set the window on fullscreen if needed
    if (vx::Prefs::shared().fullscreen()) {
        switchToFullscreen();
    }

    // Sets window minimum size
    glfwSetWindowSizeLimits(window,
                            static_cast<int>(P3S_WINDOW_MINIMUM_WIDTH * pixelsPerPoint),
                            static_cast<int>(P3S_WINDOW_MINIMUM_HEIGHT * pixelsPerPoint),
                            GLFW_DONT_CARE,
                            GLFW_DONT_CARE);

    // Doc: https://www.glfw.org/docs/3.3/input_guide.html
    glfwSetKeyCallback(window, keyCallback);
    glfwSetCharCallback(window, charCallback);
    glfwSetCursorPosCallback(window, cursorPositionCallback);
    glfwSetScrollCallback(window, scrollCallback);
    glfwSetMouseButtonCallback(window, mouseButtonCallback);

    vx::VXApplication::getInstance()->init();

    vx::VXApplication::getInstance()->didFinishLaunching(
        win32Window,
        nullptr,
        width,
        height,
        pixelsPerPoint, // number of pixels per point
        vx::Insets{0, 0, 0, 0},
        "",
        rendererType);

    return TRUE;
}

// For monotonic, I just take the perf counter since a wall clock baseline is
// meaningless.

#define MS_PER_SEC 1000ULL // MS = milliseconds
#define US_PER_MS 1000ULL  // US = microseconds
#define HNS_PER_US 10ULL   // HNS = hundred-nanoseconds (e.g., 1 hns = 100 ns)
#define NS_PER_US 1000ULL

#define HNS_PER_SEC (MS_PER_SEC * US_PER_MS * HNS_PER_US)
#define NS_PER_HNS (100ULL) // NS = nanoseconds
#define NS_PER_SEC (MS_PER_SEC * US_PER_MS * NS_PER_US)

int clock_gettime_monotonic(struct timespec *tv) {
    static LARGE_INTEGER ticksPerSec;
    LARGE_INTEGER ticks;
    double seconds;

    if (!ticksPerSec.QuadPart) {
        QueryPerformanceFrequency(&ticksPerSec);
        if (!ticksPerSec.QuadPart) {
            errno = ENOTSUP;
            return -1;
        }
    }

    QueryPerformanceCounter(&ticks);

    seconds = (double)ticks.QuadPart / (double)ticksPerSec.QuadPart;
    tv->tv_sec = (time_t)seconds;
    tv->tv_nsec = (long)((ULONGLONG)(seconds * NS_PER_SEC) % NS_PER_SEC);

    return 0;
}

// and wall clock, based on GMT unlike the tempting and similar _ftime()
// function.

int clock_gettime_realtime(struct timespec *tv) {
    FILETIME ft;
    ULARGE_INTEGER hnsTime;

    GetSystemTimeAsFileTime(&ft);

    hnsTime.LowPart = ft.dwLowDateTime;
    hnsTime.HighPart = ft.dwHighDateTime;

    // To get POSIX Epoch as baseline, subtract the number of hns intervals from
    // Jan 1, 1601 to Jan 1, 1970.
    hnsTime.QuadPart -= (11644473600ULL * HNS_PER_SEC);

    // modulus by hns intervals per second first, then convert to ns, as not to
    // lose resolution
    tv->tv_nsec = (long)((hnsTime.QuadPart % HNS_PER_SEC) * NS_PER_HNS);
    tv->tv_sec = (long)(hnsTime.QuadPart / HNS_PER_SEC);

    return 0;
}

extern "C" {
//
int clock_gettime(int type, struct timespec *tp) {
    if (type == CLOCK_MONOTONIC) {
        return clock_gettime_monotonic(tp);
    } else if (type == CLOCK_MONOTONIC_RAW) {
        return clock_gettime_realtime(tp);
    }
    return -1;
}
} // extern "C"

void _setPointer(GLFWwindow *w, const float x, const float y) {
    _pointer.xpos = x;
    _pointer.ypos = y;

    glfwSetCursorPos(w, x, y);
}

/// Centers the mouse cursor in the app's window
///
/// </summary>
/// <param name="w"></param>
void _centerPointer(GLFWwindow *w) {
    if (w == nullptr) {
        return;
    }
    int width, height;
    glfwGetWindowSize(w, &width, &height);
    const float x = static_cast<float>(width) / 2.0f;
    const float y = static_cast<float>(height) / 2.0f;
    _setPointer(w, x, y);
}

/// Convert from GLFW key action to engine enum
KeyState _convertActionToKeyState(const int action) {
    KeyState ks = KeyStateUnknown;
    switch (action) {
        case GLFW_PRESS:
            ks = KeyStateDown;
            break;
        case GLFW_RELEASE:
            ks = KeyStateUp;
            break;
        case GLFW_REPEAT:
            ks = KeyStateDown;
            break;
        default:
            break;
    }
    return ks;
}

/// Convert from GLFW enum to engine enum
/// Doc: https://www.glfw.org/docs/3.3/group__mods.html
int _convertModifiers(int mods) {
    return 0 | ((mods & GLFW_MOD_ALT) != 0 ? ModifierAlt : 0) |
           ((mods & GLFW_MOD_CONTROL) != 0 ? ModifierCtrl : 0) |
           ((mods & GLFW_MOD_SHIFT) != 0 ? ModifierShift : 0) |
           ((mods & GLFW_MOD_SUPER) != 0 ? ModifierSuper : 0);
    // GLFW_MOD_CAPS_LOCK
    // GLFW_MOD_NUM_LOCK
}

// --------------------------------------------------
//
// MARK: - Local functions -
//
// --------------------------------------------------

// function done by danzek: https://gist.github.com/danzek
static void createDirectoryRecursively(const std::wstring &directory) {
    static const std::wstring separators(L"\\/");

    // If the specified directory name doesn't exist, do our thing
    DWORD fileAttributes = ::GetFileAttributesW(directory.c_str());
    if (fileAttributes == INVALID_FILE_ATTRIBUTES) {

        // Recursively do it all again for the parent directory, if any
        std::size_t slashIndex = directory.find_last_of(separators);
        if (slashIndex != std::wstring::npos) {
            createDirectoryRecursively(directory.substr(0, slashIndex));
        }

        // Create the last directory on the path (the recursive calls will have
        // taken care of the parent directories by now)
        BOOL result = ::CreateDirectoryW(directory.c_str(), nullptr);
        if (result == FALSE) {
            throw std::runtime_error("Could not create directory");
        }

    } else { // Specified directory name already exists as a file or directory

        bool isDirectoryOrJunction = ((fileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0) ||
                                     ((fileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0);

        if (!isDirectoryOrJunction) {
            throw std::runtime_error("Could not create directory because a "
                                     "file with the same name exists");
        }
    }
}

static void switchToFullscreen() {
    GLFWmonitor *primaryMonitor = glfwGetPrimaryMonitor();
    const GLFWvidmode *mode = glfwGetVideoMode(primaryMonitor);

    // save previous dimensions
    windowedWidth = width;
    windowedHeight = height;

    // fit the dimensions to the monitor
    width = mode->width;
    height = mode->height;

    _fullscreenOn = true;
    glfwSetWindowMonitor(window, primaryMonitor, 0, 0, mode->width, mode->height, GLFW_DONT_CARE);
}

int _displayFileAccessErrorPopup() {
    int msgboxID = MessageBox(nullptr,
                              (LPCWSTR)L"Failed to write files in your \\AppData\\Roaming "
                                       L"folder.\n\nDo you want to open "
                                       L"the Particubes HELP page about this issue?",
                              (LPCWSTR)L"Particubes : can't write files",
                              MB_ICONINFORMATION | MB_YESNO | MB_DEFBUTTON1);

    if (msgboxID == IDYES) {
        vx::Web::open("https://cu.bzh/help/windows-file-permissions");
    }

    return msgboxID;
}

void _initKeyInputTranslations() {

    memset(keystoinput, InputNone, sizeof(int) * GLFW_KEY_LAST);

    keystoinput[GLFW_KEY_SPACE] = InputSpace;
    keystoinput[GLFW_KEY_APOSTROPHE] = InputQuote;           // 39  /* ' */
    keystoinput[GLFW_KEY_COMMA] = InputComma;                // 44  /* , */
    keystoinput[GLFW_KEY_MINUS] = InputMinus;                // 45  /* - */
    keystoinput[GLFW_KEY_PERIOD] = InputPeriod;              // 46  /* . */
    keystoinput[GLFW_KEY_SLASH] = InputSlash;                // 47  /* / */
    keystoinput[GLFW_KEY_0] = InputKey0;                     // 48
    keystoinput[GLFW_KEY_1] = InputKey1;                     // 49
    keystoinput[GLFW_KEY_2] = InputKey2;                     // 50
    keystoinput[GLFW_KEY_3] = InputKey3;                     // 51
    keystoinput[GLFW_KEY_4] = InputKey4;                     // 52
    keystoinput[GLFW_KEY_5] = InputKey5;                     // 53
    keystoinput[GLFW_KEY_6] = InputKey6;                     // 54
    keystoinput[GLFW_KEY_7] = InputKey7;                     // 55
    keystoinput[GLFW_KEY_8] = InputKey8;                     // 56
    keystoinput[GLFW_KEY_9] = InputKey9;                     // 57
    keystoinput[GLFW_KEY_SEMICOLON] = InputSemicolon;        // 59  /* ; */
    keystoinput[GLFW_KEY_EQUAL] = InputEqual;                // 61  /* = */
    keystoinput[GLFW_KEY_A] = InputKeyA;                     // 65
    keystoinput[GLFW_KEY_B] = InputKeyB;                     //                  66
    keystoinput[GLFW_KEY_C] = InputKeyC;                     //                  67
    keystoinput[GLFW_KEY_D] = InputKeyD;                     //                  68
    keystoinput[GLFW_KEY_E] = InputKeyE;                     //                  69
    keystoinput[GLFW_KEY_F] = InputKeyF;                     //                  70
    keystoinput[GLFW_KEY_G] = InputKeyG;                     //                  71
    keystoinput[GLFW_KEY_H] = InputKeyH;                     //                  72
    keystoinput[GLFW_KEY_I] = InputKeyI;                     //                  73
    keystoinput[GLFW_KEY_J] = InputKeyJ;                     //                  74
    keystoinput[GLFW_KEY_K] = InputKeyK;                     //                  75
    keystoinput[GLFW_KEY_L] = InputKeyL;                     //                  76
    keystoinput[GLFW_KEY_M] = InputKeyM;                     //                  77
    keystoinput[GLFW_KEY_N] = InputKeyN;                     //                  78
    keystoinput[GLFW_KEY_O] = InputKeyO;                     //                  79
    keystoinput[GLFW_KEY_P] = InputKeyP;                     //                  80
    keystoinput[GLFW_KEY_Q] = InputKeyQ;                     //                  81
    keystoinput[GLFW_KEY_R] = InputKeyR;                     //                  82
    keystoinput[GLFW_KEY_S] = InputKeyS;                     //                  83
    keystoinput[GLFW_KEY_T] = InputKeyT;                     //                  84
    keystoinput[GLFW_KEY_U] = InputKeyU;                     //                  85
    keystoinput[GLFW_KEY_V] = InputKeyV;                     //                  86
    keystoinput[GLFW_KEY_W] = InputKeyW;                     //                  87
    keystoinput[GLFW_KEY_X] = InputKeyX;                     //                  88
    keystoinput[GLFW_KEY_Y] = InputKeyY;                     //                  89
    keystoinput[GLFW_KEY_Z] = InputKeyZ;                     //                  90
    keystoinput[GLFW_KEY_LEFT_BRACKET] = InputLeftBracket;   //   91  /* [ */
    keystoinput[GLFW_KEY_BACKSLASH] = InputBackslash;        // 92  /* \ */
    keystoinput[GLFW_KEY_RIGHT_BRACKET] = InputRightBracket; // 93  /* ] */
    keystoinput[GLFW_KEY_GRAVE_ACCENT] = InputQuote;         // 96  /* ` */
    keystoinput[GLFW_KEY_WORLD_1] = InputNone;               // 161 /* non-US #1 */
    keystoinput[GLFW_KEY_WORLD_2] = InputNone;               // 162 /* non-US #2 */

    /* Function keys */
    keystoinput[GLFW_KEY_ESCAPE] = InputEsc;          // 256
    keystoinput[GLFW_KEY_ENTER] = InputReturn;        // 257
    keystoinput[GLFW_KEY_TAB] = InputTab;             // 258
    keystoinput[GLFW_KEY_BACKSPACE] = InputBackspace; // 259
    keystoinput[GLFW_KEY_INSERT] = InputInsert;       // 260
    keystoinput[GLFW_KEY_DELETE] = InputDelete;       // 261
    keystoinput[GLFW_KEY_RIGHT] = InputRight;         // 262
    keystoinput[GLFW_KEY_LEFT] = InputLeft;           // 263
    keystoinput[GLFW_KEY_DOWN] = InputDown;           // 264
    keystoinput[GLFW_KEY_UP] = InputUp;               // 265
    keystoinput[GLFW_KEY_PAGE_UP] = InputPageUp;      // 266
    keystoinput[GLFW_KEY_PAGE_DOWN] = InputPageDown;  // 267
    keystoinput[GLFW_KEY_HOME] = InputHome;           // 268
    keystoinput[GLFW_KEY_END] = InputEnd;             // 269
    keystoinput[GLFW_KEY_CAPS_LOCK] = InputNone;      // 280
    keystoinput[GLFW_KEY_SCROLL_LOCK] = InputNone;    // 281
    keystoinput[GLFW_KEY_NUM_LOCK] = InputNone;       // 282
    keystoinput[GLFW_KEY_PRINT_SCREEN] = InputPrint;  // 283
    keystoinput[GLFW_KEY_PAUSE] = InputNone;          // 284
    keystoinput[GLFW_KEY_F1] = InputF1;               // 290
    keystoinput[GLFW_KEY_F2] = InputF2;               // 291
    keystoinput[GLFW_KEY_F3] = InputF3;               // 292
    keystoinput[GLFW_KEY_F4] = InputF4;               // 293
    keystoinput[GLFW_KEY_F5] = InputF5;               // 294
    keystoinput[GLFW_KEY_F6] = InputF6;               // 295
    keystoinput[GLFW_KEY_F7] = InputF7;               // 296
    keystoinput[GLFW_KEY_F8] = InputF8;               // 297
    keystoinput[GLFW_KEY_F9] = InputF9;               // 298
    keystoinput[GLFW_KEY_F10] = InputF10;             // 299
    keystoinput[GLFW_KEY_F11] = InputF11;             // 300
    keystoinput[GLFW_KEY_F12] = InputF12;             // 301

    // Numpad
    keystoinput[GLFW_KEY_KP_0] = InputNumPad0; // 320
    keystoinput[GLFW_KEY_KP_1] = InputNumPad1; // 321
    keystoinput[GLFW_KEY_KP_2] = InputNumPad2; // 322
    keystoinput[GLFW_KEY_KP_3] = InputNumPad3; // 323
    keystoinput[GLFW_KEY_KP_4] = InputNumPad4; // 324
    keystoinput[GLFW_KEY_KP_5] = InputNumPad5; // 325
    keystoinput[GLFW_KEY_KP_6] = InputNumPad6; // 326
    keystoinput[GLFW_KEY_KP_7] = InputNumPad7; // 327
    keystoinput[GLFW_KEY_KP_8] = InputNumPad8; // 328
    keystoinput[GLFW_KEY_KP_9] = InputNumPad9; // 329

    keystoinput[GLFW_KEY_KP_DECIMAL] = InputDecimal;      // 330
    keystoinput[GLFW_KEY_KP_DIVIDE] = InputDivide;        // 331
    keystoinput[GLFW_KEY_KP_MULTIPLY] = InputMultiply;    // 332
    keystoinput[GLFW_KEY_KP_SUBTRACT] = InputNumPadMinus; // 333
    keystoinput[GLFW_KEY_KP_ADD] = InputNumPadPlus;       // 334
    keystoinput[GLFW_KEY_KP_ENTER] = InputReturnKP;       // 335
    keystoinput[GLFW_KEY_KP_EQUAL] = InputNumPadEqual;    // 336
}

void _initKeynameToInputTranslation() {
    if (_keynameToInput.empty()) {
        _keynameToInput.emplace("a", InputKeyA);
        _keynameToInput.emplace("b", InputKeyB);
        _keynameToInput.emplace("c", InputKeyC);
        _keynameToInput.emplace("d", InputKeyD);
        _keynameToInput.emplace("e", InputKeyE);
        _keynameToInput.emplace("f", InputKeyF);
        _keynameToInput.emplace("g", InputKeyG);
        _keynameToInput.emplace("h", InputKeyH);
        _keynameToInput.emplace("i", InputKeyI);
        _keynameToInput.emplace("j", InputKeyJ);
        _keynameToInput.emplace("k", InputKeyK);
        _keynameToInput.emplace("l", InputKeyL);
        _keynameToInput.emplace("m", InputKeyM);
        _keynameToInput.emplace("n", InputKeyN);
        _keynameToInput.emplace("o", InputKeyO);
        _keynameToInput.emplace("p", InputKeyP);
        _keynameToInput.emplace("q", InputKeyQ);
        _keynameToInput.emplace("r", InputKeyR);
        _keynameToInput.emplace("s", InputKeyS);
        _keynameToInput.emplace("t", InputKeyT);
        _keynameToInput.emplace("u", InputKeyU);
        _keynameToInput.emplace("v", InputKeyV);
        _keynameToInput.emplace("w", InputKeyW);
        _keynameToInput.emplace("x", InputKeyX);
        _keynameToInput.emplace("y", InputKeyY);
        _keynameToInput.emplace("z", InputKeyZ);
        // ...
    }
}

// Loads app icon from the resources and assign it to the window.
// Returns an empty string on success, or an error message on failure.
void _loadAndSetAppIcon(GLFWwindow *window) {
    // Load the icon from resources
    HICON hBigIcon = LoadIcon(hInst, L"GLFW_ICON");
    SetClassLongPtr(win32Window, GCLP_HICON, (LONG_PTR)hBigIcon);
    SetClassLongPtr(win32Window, GCLP_HICONSM, (LONG_PTR)hBigIcon);
}

//bool _loadResourceToMemory(int resourceID, const std::string resourceType, std::string &outData) {
//
//    // convert resourceType to wide string
//    // std::wstring wResourceType(resourceType.begin(), resourceType.end());
//
//    HRSRC hResource = FindResourceA(NULL, MAKEINTRESOURCEA(resourceID), resourceType.c_str());
//    if (!hResource) {
//        vxlog_error("Failed to find resource: %d", resourceID);
//        return false;
//    }
//
//    HGLOBAL hLoadedResource = LoadResource(NULL, hResource);
//    if (!hLoadedResource) {
//        vxlog_error("Failed to load resource: %d", resourceID);
//        return false;
//    }
//
//    DWORD dwResourceSize = SizeofResource(NULL, hResource);
//    if (dwResourceSize == 0) {
//        vxlog_error("Failed to get resource size: %d", resourceID);
//        return false;
//    }
//
//    const void *pResourceData = LockResource(hLoadedResource);
//    if (!pResourceData) {
//        vxlog_error("Failed to lock resource: %d", resourceID);
//        return false;
//    }
//
//    outData.resize(dwResourceSize);
//    memcpy(outData.data(), pResourceData, dwResourceSize);
//
//    return true;
//}