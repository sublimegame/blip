//
//  lua_local_event.hpp
//  Cubzh
//
//  Created by Adrien Duermael on 23/05/2023.
//

// LocalEvent is defined as a Lua module.
// This is just a set of defines to easily use it in C++.

#pragma once

#include "lua_logs.hpp"
#include "lua_system.hpp"

#define LOCAL_EVENT_NAME_Tick 1
#define LOCAL_EVENT_NAME_AvatarLoaded 2
#define LOCAL_EVENT_NAME_KeyboardInput 3
#define LOCAL_EVENT_NAME_VirtualKeyboardShown 4
#define LOCAL_EVENT_NAME_VirtualKeyboardHidden 5
#define LOCAL_EVENT_NAME_ScreenDidResize 6
#define LOCAL_EVENT_NAME_ClientFieldSet 7
#define LOCAL_EVENT_NAME_PointerShown 8
#define LOCAL_EVENT_NAME_PointerHidden 9
#define LOCAL_EVENT_NAME_PointerDown 10
#define LOCAL_EVENT_NAME_PointerUp 11
#define LOCAL_EVENT_NAME_PointerDragBegin 12
#define LOCAL_EVENT_NAME_PointerDrag 13
#define LOCAL_EVENT_NAME_PointerDragEnd 14
#define LOCAL_EVENT_NAME_HomeMenuOpened 15
#define LOCAL_EVENT_NAME_HomeMenuClosed 16
#define LOCAL_EVENT_NAME_PointerClick 17
#define LOCAL_EVENT_NAME_PointerWheel 18
#define LOCAL_EVENT_NAME_PointerCancel 19
#define LOCAL_EVENT_NAME_PointerLongPress 20
#define LOCAL_EVENT_NAME_PointerMove 21
#define LOCAL_EVENT_NAME_Action1Set 22
#define LOCAL_EVENT_NAME_Action2Set 23
#define LOCAL_EVENT_NAME_Action3Set 24
#define LOCAL_EVENT_NAME_Action1ReleaseSet 25
#define LOCAL_EVENT_NAME_Action2ReleaseSet 26
#define LOCAL_EVENT_NAME_Action3ReleaseSet 27
#define LOCAL_EVENT_NAME_DirPadSet 28
#define LOCAL_EVENT_NAME_AnalogPadSet 29
#define LOCAL_EVENT_NAME_AnalogPad 30
#define LOCAL_EVENT_NAME_DirPad 31
// #define LOCAL_EVENT_NAME_CloseChat 32 // REMOVED AFTER 0.0.53
#define LOCAL_EVENT_NAME_OnPlayerJoin 33
#define LOCAL_EVENT_NAME_OnPlayerLeave 34
#define LOCAL_EVENT_NAME_DidReceiveEvent 35
#define LOCAL_EVENT_NAME_InfoMessage 36
#define LOCAL_EVENT_NAME_WarningMessage 37
#define LOCAL_EVENT_NAME_ErrorMessage 38
#define LOCAL_EVENT_NAME_SensitivityUpdated 39
#define LOCAL_EVENT_NAME_OnChat 40
#define LOCAL_EVENT_NAME_OnStart 41
#define LOCAL_EVENT_NAME_CppMenuStateChanged 42
#define LOCAL_EVENT_NAME_LocalAvatarUpdate 43
#define LOCAL_EVENT_NAME_ReceivedEnvironmentToLaunch 44
#define LOCAL_EVENT_NAME_ChatMessage 45
#define LOCAL_EVENT_NAME_FailedToLoadWorld 46
#define LOCAL_EVENT_NAME_ServerConnectionSuccess 47
#define LOCAL_EVENT_NAME_ServerConnectionLost 48
#define LOCAL_EVENT_NAME_ServerConnectionFailed 49
#define LOCAL_EVENT_NAME_ServerConnectionStart 50
#define LOCAL_EVENT_NAME_OnWorldObjectLoad 51
#define LOCAL_EVENT_NAME_Log 52
#define LOCAL_EVENT_NAME_ChatMessageACK 53
#define LOCAL_EVENT_NAME_ActiveTextInputUpdate 54
#define LOCAL_EVENT_NAME_ActiveTextInputClose 55
#define LOCAL_EVENT_NAME_ActiveTextInputDone 56
#define LOCAL_EVENT_NAME_ActiveTextInputNext 57
#define LOCAL_EVENT_NAME_AppDidBecomeActive 58
#define LOCAL_EVENT_NAME_DidReceivePushNotification 59
#define LOCAL_EVENT_NAME_NotificationCountDidChange 60
#define LOCAL_EVENT_NAME_WorldRequested 61

#define LOCAL_EVENT_SEND_PUSH(L, eventName) lua_getglobal(L, "LocalEvent"); \
lua_getfield(L, -1, "Send"); \
lua_pushvalue(L, -2); \
lua_remove(L, -3); \
lua_pushinteger(L, eventName);

#define LOCAL_EVENT_SEND_PUSH_WITH_SYSTEM(L, eventName) lua_getglobal(L, "LocalEvent"); \
lua_getfield(L, -1, "Send"); \
lua_pushvalue(L, -2); \
lua_remove(L, -3); \
lua_pushinteger(L, eventName); \
lua_system_table_push(L);

// LocalEvent:Send(self, name, ...) +2 -> self and name parameters
//#define LOCAL_EVENT_SEND_CALL(L, nbArgs) if (lua_pcall(L, nbArgs + 2, 0, 0) != LUA_OK) { \
//if (lua_utils_isStringStrict(L, -1)) { \
//    lua_log_error_CStr(L, lua_tostring(L, -1)); \
//} else { \
//   lua_log_error(L, "LocalEvent.Send callback error"); \
//} \
//lua_pop(L, 1); \
//}

#define LOCAL_EVENT_SEND_CALL(L, nbArgs) if (lua_pcall(L, nbArgs + 2, 0, 0) != LUA_OK) { \
lua_pop(L, 1); \
}

// LocalEvent:Send(self, name, ...) +2 -> self and name parameters
#define LOCAL_EVENT_SEND_CALL_WITH_SYSTEM(L, nbArgs) if (lua_pcall(L, nbArgs + 3, 0, 0) != LUA_OK) { \
if (lua_utils_isStringStrict(L, -1)) { \
    lua_log_error_CStr(L, lua_tostring(L, -1)); \
} else { \
    lua_log_error(L, "LocalEvent.Send callback error"); \
} \
lua_pop(L, 1); \
}

// pushes Listener on the stack
#define LOCAL_EVENT_LISTEN(L, eventName, callbackIdx) lua_getglobal(L, "LocalEvent"); \
lua_getfield(L, -1, "Listen"); \
lua_pushvalue(L, -2); \
lua_remove(L, -3); \
lua_pushinteger(L, eventName); \
lua_pushvalue(L, callbackIdx); \
if (lua_pcall(L, 3, 1, 0) != LUA_OK) { \
    if (lua_utils_isStringStrict(L, -1)) { \
        lua_log_error_CStr(L, lua_tostring(L, -1)); \
    } else { \
        lua_log_error(L, "LocalEvent.Listen: can't set listener"); \
    } \
    lua_pop(L, 1); \
}


// pushes Listener on the stack
#define LOCAL_EVENT_LISTEN_PRIORITY(L, eventName, callbackIdx) lua_getglobal(L, "LocalEvent"); \
lua_getfield(L, -1, "Listen"); \
lua_pushvalue(L, -2); \
lua_remove(L, -3); \
lua_pushinteger(L, eventName); \
lua_pushvalue(L, callbackIdx); \
lua_newtable(L); \
{ \
    lua_pushstring(L, "topPritoty"); \
    lua_pushboolean(L, true); \
    lua_rawset(L, -3); \
} \
if (lua_pcall(L, 4, 1, 0) != LUA_OK) { \
    if (lua_utils_isStringStrict(L, -1)) { \
        lua_log_error_CStr(L, lua_tostring(L, -1)); \
    } else { \
        lua_log_error(L, "LocalEvent.Listen: can't set listener"); \
    } \
    lua_pop(L, 1); \
}

#define LOCAL_EVENT_LISTENER_CALL_REMOVE(L, listenerIdx) lua_getfield(L, listenerIdx, "Remove"); \
lua_pushvalue(L, listenerIdx < 0 ? listenerIdx - 1 : listenerIdx); \
if (listenerIdx < 0) { lua_remove(L, listenerIdx - 2); } \
if (lua_pcall(L, 1, 0, 0) != LUA_OK) { \
    if (lua_utils_isStringStrict(L, -1)) { \
        lua_log_error_CStr(L, lua_tostring(L, -1)); \
    } else { \
        lua_log_error(L, "LocalEvent.Listener.Remove error"); \
    } \
    lua_pop(L, 1); \
}

