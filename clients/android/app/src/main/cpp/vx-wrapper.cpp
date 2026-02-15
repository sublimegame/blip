//
// vx-wrapper.cpp
// Blip
//

// C++
#include <string>

// Android
#include <android/keycodes.h>
#include <android/log.h>
#include <android/native_window_jni.h>
#include <EGL/egl.h>
#include <jni.h>

#include "VXApplication.hpp"
#include "VXImGui.hpp"
#include <OperationQueue.hpp>

// vx::tools
#include "compat_android.hpp"
#include "filesystem.hpp"
#include "JNIUtils.hpp"
#include "notifications.hpp"
#include "passkey.hpp"
#include "textinput.h"

extern "C" {
#include "inputs.h"
}

static const std::string pushTokenTypeFirebase = "firebase";

// Android log function wrappers
static const char *kTAG = "vx-wrapper";
#define LOG_INFO(...) ((void)__android_log_print(ANDROID_LOG_INFO, kTAG, __VA_ARGS__))
#define LOG_DEBUG(...) ((void)__android_log_print(ANDROID_LOG_DEBUG, kTAG, __VA_ARGS__))
#define LOG_ERROR(...) ((void)__android_log_print(ANDROID_LOG_ERROR, kTAG, __VA_ARGS__))

/**
 * JNI_OnLoad is called when the vx-wrapper library is loaded by the Java VM
 * https://docs.oracle.com/javase/7/docs/technotes/guides/jni/spec/invocation.html
 * @param vm
 * @param reserved
 * @return the version of JNI API required by the library
 */
JNIEXPORT jint JNI_OnLoad(JavaVM *vm, void *reserved) {
    vx::tools::JNIUtils::getInstance()->setJavaVM(vm);
    return JNI_VERSION_1_4;
}

using namespace vx;

Input mapKeycodeToInputKey(jint keycode);

extern "C" JNIEXPORT void JNICALL Java_com_voxowl_blip_MainActivity_init(JNIEnv *env,
                                                                         jobject thiz,
                                                                         jobject assetManager,
                                                                         jstring appStoragePath) {}

extern "C" JNIEXPORT void JNICALL Java_com_voxowl_blip_MainActivity_tick(JNIEnv *env,
                                                                         jobject thiz,
                                                                         jdouble deltaTime) {
    VXApplication::getInstance()->tick(deltaTime);
}

extern "C" JNIEXPORT void JNICALL Java_com_voxowl_blip_MainActivity_willTerminate(JNIEnv *env,
                                                                                  jobject thiz) {
    VXApplication::getInstance()->willTerminate();
}

extern "C" JNIEXPORT void JNICALL Java_com_voxowl_blip_MainActivity_didBecomeActive(JNIEnv *env,
                                                                                    jobject thiz) {
    VXApplication::getInstance()->didBecomeActive();
}

extern "C" JNIEXPORT void JNICALL Java_com_voxowl_blip_MainActivity_willResignActive(JNIEnv *env,
                                                                                     jobject thiz) {
    VXApplication::getInstance()->willResignActive();
}

extern "C" JNIEXPORT void JNICALL
Java_com_voxowl_blip_MainActivity_didReceiveMemoryWarning(JNIEnv *env, jobject thiz) {
    VXApplication::getInstance()->didReceiveMemoryWarning();
}

extern "C" JNIEXPORT void JNICALL
Java_com_voxowl_blip_MainActivity_postKeyboardInput(JNIEnv *env,
                                                    jobject thiz,
                                                    jint charcode,
                                                    jint key,
                                                    jint modifiers,
                                                    jint state) {
    const Input input = mapKeycodeToInputKey(key);
    postKeyboardInput(static_cast<uint32_t>(charcode),
                      input,
                      static_cast<uint8_t>(modifiers),
                      static_cast<KeyState>(state));
}

extern "C" JNIEXPORT void JNICALL
Java_com_voxowl_blip_MainActivity_callbackImportFile(JNIEnv *env,
                                                     jobject thiz,
                                                     jbyteArray javaBytes,
                                                     jint statusInt) {
    // convert the status
    vx::fs::ImportFileCallbackStatus status;
    switch (statusInt) {
        case 0:
            status = vx::fs::ImportFileCallbackStatus::OK;
            break;
        case 1:
            status = vx::fs::ImportFileCallbackStatus::ERR;
            break;
        case 2:
            status = vx::fs::ImportFileCallbackStatus::CANCELLED;
            break;
        default:
            status = vx::fs::ImportFileCallbackStatus::ERR;
            break;
    }

    // get the number of bytes in the Java array
    const jsize bytesCount = env->GetArrayLength(javaBytes);
    std::string bytes;
    if (bytesCount > 0) {
        // get the ByteArray in the native form
        // nullptr: do not check if it was copied or not
        jbyte *jniBytes = env->GetByteArrayElements(javaBytes, nullptr);

        // copy bytes to the `bytes` C++ buffer
        bytes.assign(reinterpret_cast<const char*>(jniBytes), bytesCount);

        // release jniBytes
        // JNI_ABORT: do not copy jniBytes to its Java equivalent (javaBytes)
        env->ReleaseByteArrayElements(javaBytes, jniBytes, JNI_ABORT);
    }

    // call the stored callback
    fs::callCurrentImportCallback(status, bytes);
}

extern "C" JNIEXPORT void JNICALL
Java_com_voxowl_blip_MainActivity_keyboardDidOpen(JNIEnv *env,
                                                  jobject thiz,
                                                  jfloat width,
                                                  jfloat height,
                                                  jfloat duration) {
    vx::VXApplication::getInstance()->showKeyboard(
        vx::Size(width / Screen::nbPixelsInOnePoint, height / Screen::nbPixelsInOnePoint),
        duration);
}

extern "C" JNIEXPORT void JNICALL
Java_com_voxowl_blip_MainActivity_keyboardDidClose(JNIEnv *env, jobject thiz, jfloat duration) {
    vx::VXApplication::getInstance()->hideKeyboard(duration);
}

extern "C" JNIEXPORT void JNICALL
Java_com_voxowl_blip_MainActivity_setStartupAppLink(JNIEnv *env, jobject thiz, jstring urlJStr) {
    const char *urlCStr = env->GetStringUTFChars(urlJStr, NULL);
    if (urlCStr == nullptr) {
        return;
    }
    const std::string startupAppLinkURL(urlCStr);

    // construct environment using startupAppLinkURL if it's not empty
    std::unordered_map<std::string, std::string> launchEnv;
    std::string key = "worldID";
    if (!startupAppLinkURL.empty()) {
        vx::URL url = vx::URL::make(startupAppLinkURL);
        if (url.isValid() && url.host() == "app.cu.bzh" && url.queryContainsKey(key)) {
            std::string worldID = *url.queryValuesForKey(key).begin();
            launchEnv[key] = worldID;
        }
    }

    GameCoordinator::getInstance()->setEnvironmentToLaunch(launchEnv);
}

extern "C" JNIEXPORT void JNICALL Java_com_voxowl_blip_MainActivity_openAppLink(JNIEnv *env,
                                                                                jobject thiz,
                                                                                jstring urlJStr) {
    const char *urlCStr = env->GetStringUTFChars(urlJStr, NULL);
    if (urlCStr == nullptr) {
        return;
    }
    const std::string urlStr(urlCStr);
    vx::VXApplication::getInstance()->openURL(urlStr);
}

// C function calling Kotlin function
void textinputRequestCallbackPtr(const char *str,
                                 size_t strLen,
                                 bool strDidChange,
                                 size_t cursorStart,
                                 size_t cursorEnd,
                                 bool multiline,
                                 TextInputKeyboardType keyboardType,
                                 TextInputReturnKeyType returnKeyType,
                                 bool suggestions) {
    // LOGE("⭐️[INPUT] [vx-wrapper] [Request]");

    // find Java/Kotlin function to call
    bool just_attached = false;
    vx::tools::JniMethodInfo methodInfo;
    if (!vx::tools::JNIUtils::getInstance()->getMethodInfo(&just_attached,
                                                           &methodInfo,
                                                           "com/voxowl/tools/TextInput",
                                                           "textinputRequest",
                                                           "([BZJJZII)V")) {
        assert(false); // crash the program
    }

    // convert arguments to Java
    const jbyteArray jStrBytes = vx::tools::JNIUtils::getInstance()->createJByteArrayFromCString(
        methodInfo.env,
        str);
    if (jStrBytes == nullptr) {
        return; // abort
    }
    const jboolean jStrDidChange = strDidChange;
    const jlong jCursorStart = cursorStart;
    const jlong jCursorEnd = cursorEnd;
    const jboolean jMultiline = (multiline) ? JNI_TRUE : JNI_FALSE;
    const jint jKeyboardType = keyboardType;
    const jint jReturnKeyType = returnKeyType;

    // call Java function
    methodInfo.env->CallStaticVoidMethod(methodInfo.classID,
                                         methodInfo.methodID,
                                         jStrBytes,
                                         jStrDidChange,
                                         jCursorStart,
                                         jCursorEnd,
                                         jMultiline,
                                         jKeyboardType,
                                         jReturnKeyType);

    // free args
    methodInfo.env->DeleteLocalRef(jStrBytes);

    // free class
    methodInfo.env->DeleteLocalRef(methodInfo.classID);

    // detach thread if necessary
    if (just_attached) {
        vx::tools::JNIUtils::getInstance()->getJavaVM()->DetachCurrentThread();
    }
}

void textinputUpdateCallbackPtr(const char *str,
                                size_t strLen,
                                bool strDidChange,
                                size_t cursorStart,
                                size_t cursorEnd) {
    // LOGE("⭐️[INPUT] [vx-wrapper] [Update]");

    // find Java/Kotlin function to call
    bool just_attached = false;
    vx::tools::JniMethodInfo methodInfo;
    if (!vx::tools::JNIUtils::getInstance()->getMethodInfo(&just_attached,
                                                           &methodInfo,
                                                           "com/voxowl/tools/TextInput",
                                                           "textinputUpdate",
                                                           "([BZJJ)V")) {
        LOG_ERROR("%s %d: error to get methodInfo", __FILE__, __LINE__);
        assert(false); // crash the program
    }

    // convert arguments to Java
    const jbyteArray jStrBytes = vx::tools::JNIUtils::getInstance()->createJByteArrayFromCString(
        methodInfo.env,
        str);
    if (jStrBytes == nullptr) {
        return; // abort
    }
    const jboolean jStrDidChange = strDidChange;
    const jlong jCursorStart = cursorStart;
    const jlong jCursorEnd = cursorEnd;

    // call Java function
    methodInfo.env->CallStaticVoidMethod(methodInfo.classID,
                                         methodInfo.methodID,
                                         jStrBytes,
                                         jStrDidChange,
                                         jCursorStart,
                                         jCursorEnd);

    // free args
    methodInfo.env->DeleteLocalRef(jStrBytes);

    // free class
    methodInfo.env->DeleteLocalRef(methodInfo.classID);

    // detach thread if necessary
    if (just_attached) {
        vx::tools::JNIUtils::getInstance()->getJavaVM()->DetachCurrentThread();
    }
}

void textinputActionCallbackPtr(TextInputAction action) {
    // LOGE("⭐️[INPUT] [vx-wrapper] [Action]");

    // find Java/Kotlin function to call
    bool just_attached = false;
    vx::tools::JniMethodInfo methodInfo;
    if (!vx::tools::JNIUtils::getInstance()->getMethodInfo(&just_attached,
                                                           &methodInfo,
                                                           "com/voxowl/tools/TextInput",
                                                           "textinputAction",
                                                           "(I)V")) {
        LOG_ERROR("%s %d: error to get methodInfo", __FILE__, __LINE__);
        assert(false); // crash the program
    }

    // convert arguments to Java
    const jint jAction = action;

    // call Java function
    methodInfo.env->CallStaticVoidMethod(methodInfo.classID, methodInfo.methodID, jAction);

    // free class
    methodInfo.env->DeleteLocalRef(methodInfo.classID);

    // detach thread if necessary
    if (just_attached) {
        vx::tools::JNIUtils::getInstance()->getJavaVM()->DetachCurrentThread();
    }
}

// Kotlin function calling C function
extern "C" JNIEXPORT void JNICALL
Java_com_voxowl_blip_MainActivity_textInputUpdate(JNIEnv *env,
                                                  jobject thiz,
                                                  jstring jStr,
                                                  jlong jCursorStart,
                                                  jlong jCursorEnd) {
    const std::string str = vx::tools::string::convertJNIStringToCPPString(env, jStr);
    // LOGE("⭐️[INPUT] UPDATE JAVA -> C++ >> %s", str.c_str());
    hostPlatformTextInputUpdate(str.c_str(), str.size(), jCursorStart, jCursorEnd);
}

extern "C" JNIEXPORT void JNICALL Java_com_voxowl_blip_MainActivity_textInputDone(JNIEnv *env,
                                                                                  jobject thiz) {
    hostPlatformTextInputDone();
}

extern "C" JNIEXPORT void JNICALL Java_com_voxowl_blip_MainActivity_textInputNext(JNIEnv *env,
                                                                                  jobject thiz) {
    hostPlatformTextInputNext();
}

extern "C" JNIEXPORT void JNICALL
Java_com_voxowl_blip_GameApplication_00024Companion_init(JNIEnv *env,
                                                         jobject thiz,
                                                         jobject assetManager,
                                                         jstring appStoragePath) {
    // Set Android Asset Manager
    vx::android::setAndroidAssetManager(env, assetManager);
    // create a std::string from the provided jstring
    const char *c_str = env->GetStringUTFChars(appStoragePath, NULL);
    vx::android::setAndroidStoragePath(std::string(c_str));

    //
    VXApplication::getInstance()->init();
}

extern "C" JNIEXPORT void JNICALL
Java_com_voxowl_blip_MainActivity_didFinishLaunching(JNIEnv *env,
                                                     jobject thiz,
                                                     jobject surface,
                                                     jint width,
                                                     jint height,
                                                     jfloat topInset,
                                                     jfloat bottomInset,
                                                     jfloat leftInset,
                                                     jfloat rightInset,
                                                     jfloat displayDensity) {

    // Init text input interface
    hostPlatformTextInputRegisterDelegate(textinputRequestCallbackPtr,
                                          textinputUpdateCallbackPtr,
                                          textinputActionCallbackPtr);

    ANativeWindow *window = ANativeWindow_fromSurface(env, surface);
    Insets insets = {topInset, bottomInset, leftInset, rightInset};

    VXApplication::getInstance()
        ->didFinishLaunching(window, nullptr, width, height, displayDensity, insets, "");
}

extern "C" JNIEXPORT void JNICALL Java_com_voxowl_blip_MainActivity_didResize(JNIEnv *env,
                                                                              jobject thiz,
                                                                              jint width,
                                                                              jint height,
                                                                              jfloat pixelsPerPoint,
                                                                              jfloat topInset,
                                                                              jfloat bottomInset,
                                                                              jfloat leftInset,
                                                                              jfloat rightInset) {

    Insets insets = {topInset, bottomInset, leftInset, rightInset};

    VXApplication::getInstance()->didResize(width, height, pixelsPerPoint, insets);
}

extern "C" JNIEXPORT void JNICALL Java_com_voxowl_blip_MainActivity_postTouchEvent(JNIEnv *env,
                                                                                   jobject thiz,
                                                                                   jint id,
                                                                                   jfloat x,
                                                                                   jfloat y,
                                                                                   jfloat dx,
                                                                                   jfloat dy,
                                                                                   jint state,
                                                                                   jboolean move) {

    PointerID pointerID;
    switch (id) {
        case 0: {
            pointerID = PointerIDTouch1;
            break;
        }
        case 1: {
            pointerID = PointerIDTouch2;
            break;
        }
        case 2: {
            pointerID = PointerIDTouch3;
            break;
        }
        default: {
            pointerID = PointerIDTouch1;
            break;
        }
    }

    PointerEventType pointerEventType = PointerEventTypeMove;
    switch (state) {
        case 1: { // down
            pointerEventType = PointerEventTypeDown;
            break;
        }
        case 2: { // up
            pointerEventType = PointerEventTypeUp;
            break;
        }
        default: {
        }
    }

    // vxlog_debug(">>> [POST POINTER EVENT]");
    postPointerEvent(pointerID, pointerEventType, x, y, dx, dy);
    postTouchEvent(id, x, y, dx, dy, (TouchState)state, move);
}

extern "C" JNIEXPORT void JNICALL Java_com_voxowl_blip_MainActivity_render(JNIEnv *env,
                                                                           jobject thiz) {
    VXApplication::getInstance()->render();
}

/// We only map android keycodes that are relevant for dear-imgui
Input mapKeycodeToInputKey(jint keycode) {
    switch (keycode) {
        case AKEYCODE_DEL:
            return InputBackspace;
        case AKEYCODE_ENTER:
            return InputReturn;
        case AKEYCODE_NUMPAD_ENTER:
            return InputReturn;
        case AKEYCODE_SPACE:
            return InputSpace;
        default:
            return InputNone;
    }
}

///
/// JAVA Class: com.voxowl.tools.Notifications
///

extern "C" JNIEXPORT void JNICALL
Java_com_voxowl_tools_Notifications_didRegisterForRemoteNotifications(JNIEnv *env,
                                                                      jclass clazz,
                                                                      jstring token) {
    const std::string tokenStr = vx::tools::string::convertJNIStringToCPPString(env, token);

    // sends new token to C++ layer
    vx::VXApplication::getInstance()->didRegisterForRemoteNotifications(pushTokenTypeFirebase,
                                                                        tokenStr);
}

extern "C" JNIEXPORT void JNICALL
Java_com_voxowl_tools_Notifications_setRemotePushAuthorization(JNIEnv *env, jclass clazz) {
    vx::notification::setRemotePushAuthorization();
}

extern "C" JNIEXPORT void JNICALL
Java_com_voxowl_tools_Notifications_didReplyToNotificationPermissionPopup(JNIEnv *env,
                                                                          jclass clazz,
                                                                          jint response) {
    int responseValue = response;
    vx::notification::didReplyToNotificationPermissionPopup(
        (vx::notification::NotificationAuthorizationResponse)responseValue);
}

///
/// JAVA Class: CubzhFirebaseInstanceIDService
///

extern "C" JNIEXPORT void JNICALL
Java_com_voxowl_blip_CubzhFirebaseInstanceIDService_onNewPushTokenCPP(JNIEnv *env,
                                                                      jobject thiz,
                                                                      jstring token) {
    const std::string tokenStr = vx::tools::string::convertJNIStringToCPPString(env, token);
    if (tokenStr.empty()) {
        return;
    }

    // sends new token to C++ layer
    vx::VXApplication::getInstance()->didRegisterForRemoteNotifications(pushTokenTypeFirebase,
                                                                        tokenStr);
}

///
/// Java Class: InstallReferrerUtil
///

extern "C" JNIEXPORT jboolean JNICALL
Java_com_voxowl_blip_InstallReferrerUtil_didReceiveInstallReferrerInfo(JNIEnv *env,
                                                                       jclass clazz,
                                                                       jstring json_string) {
    const std::string str = vx::tools::string::convertJNIStringToCPPString(env, json_string);
    // send to C++ layer
    return vx::VXApplication::getInstance()->didReceiveInstallReferrerInfo(str);
}

extern "C" JNIEXPORT void JNICALL
Java_com_voxowl_blip_MainActivity_willEnterForeground(JNIEnv *env, jobject thiz) {
    LOG_INFO("willEnterForeground");
    VXApplication::getInstance()->willEnterForeground();
}

extern "C" JNIEXPORT void JNICALL
Java_com_voxowl_blip_MainActivity_didEnterBackground(JNIEnv *env, jobject thiz) {
    LOG_INFO("didEnterBackground");
    VXApplication::getInstance()->didEnterBackground();
}

extern "C" JNIEXPORT void JNICALL
Java_com_voxowl_blip_CubzhFirebaseInstanceIDService_onMessageReceivedCPP(JNIEnv *env,
                                                                         jobject thiz,
                                                                         jstring title,
                                                                         jstring body,
                                                                         jstring category,
                                                                         jint badge) {
    const std::string titleCpp = vx::tools::string::convertJNIStringToCPPString(env, title);
    const std::string bodyCpp = vx::tools::string::convertJNIStringToCPPString(env, body);
    const std::string categoryCpp = vx::tools::string::convertJNIStringToCPPString(env, category);
    VXApplication::getInstance()->didReceiveNotification(titleCpp, bodyCpp, categoryCpp, badge);
}

///
/// Java Class: com.voxowl.tools.Passkey
///

// C++ implementation of Java function
extern "C" JNIEXPORT void JNICALL
Java_com_voxowl_tools_Passkey_onPasskeyCreated(JNIEnv *env,
                                               jclass clazz,
                                               jstring attestationResponseJson) {
    // convert JNI string to C++ string
    const std::string str = vx::tools::string::convertJNIStringToCPPString(env, attestationResponseJson);
    // LOG_INFO("🔑[JNI][Passkey] created successfully: %s", str.c_str());

    // Call the application's passkey creation success handler
    vx::auth::PassKey::onPasskeyCreateSuccess(str);
}

// C++ implementation of Java function
extern "C" JNIEXPORT void JNICALL
Java_com_voxowl_tools_Passkey_onPasskeyCreationFailed(JNIEnv *env,
                                                      jclass clazz,
                                                      jstring errorMessage) {
    // convert JNI string to C++ string
    const std::string err = vx::tools::string::convertJNIStringToCPPString(env, errorMessage);
    // LOG_ERROR("🔑 [JNI][Passkey] creation failed: %s", err.c_str());

    // Call the application's passkey creation error handler
    vx::auth::PassKey::onPasskeyCreateFailure(err);
}

// C++ implementation of Java function
extern "C" JNIEXPORT void JNICALL
Java_com_voxowl_tools_Passkey_onPasskeyLoginSucceeded(JNIEnv *env,
                                                      jclass clazz,
                                                      jstring authenticationResponseJson) {
    // convert JNI string to C++ string
    const std::string jsonStr = vx::tools::string::convertJNIStringToCPPString(env, authenticationResponseJson);
    LOG_INFO("🔑[JNI][Passkey] login successfully: %s", jsonStr.c_str());

    // Call the application's passkey login success handler
    vx::auth::PassKey::onPasskeyLoginSuccess(jsonStr);
}

// C++ implementation of Java function
extern "C" JNIEXPORT void JNICALL
Java_com_voxowl_tools_Passkey_onPasskeyLoginFailed(JNIEnv *env,
                                                   jclass clazz,
                                                   jstring errorMessage) {
    // convert JNI string to C++ string
    const std::string err = vx::tools::string::convertJNIStringToCPPString(env, errorMessage);
    LOG_ERROR("🔑 [JNI][Passkey] login failed: %s", err.c_str());

    // Call the application's passkey login error handler
    vx::auth::PassKey::onPasskeyLoginFailure(err);
}
