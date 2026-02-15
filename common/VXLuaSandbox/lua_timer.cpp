//
//  lua_timer.cpp
//  Cubzh
//
//  Created by Xavier Legland on 01/27/2021.
//  Copyright © 2020 Voxowl Inc. All rights reserved.
//

#include "lua_timer.hpp"

// C++
#include <cstring>

// Lua
#include "lua.hpp"
#include "lua_utils.hpp"

// engine
#include "timer.h"
#include "game.h"

#include "config.h"
#include "lua_constants.h"
#include "lua_logs.hpp"

#define LUA_TIMER_FIELD_CANCEL          "Cancel"        // function
#define LUA_TIMER_FIELD_RESET           "Reset"         // function
#define LUA_TIMER_FIELD_PAUSE           "Pause"         // function
#define LUA_TIMER_FIELD_RESUME          "Resume"        // function
#define LUA_TIMER_FIELD_TIME            "Time"          // number
#define LUA_TIMER_FIELD_REMAINING_TIME  "RemainingTime" // number

#define LUA_TIMER_CALLBACK "callback"
#define LUA_TIMER_ARRAY_TIMERS "timers_array"

#include "VXGame.hpp"

typedef struct timer_userdata {
    // Userdata instance requirements
    vx::UserDataStore *store;
    timer_userdata *next;
    timer_userdata *previous;

    Timer *timer;
    TICK_DELTA_SEC_T time;
} timer_userdata;

static void callback(Timer *const timerAddr, void *userData);

static int _g_timer_index(lua_State *const L);
static int _g_timer_newindex(lua_State *const L);
static int _g_timer_call(lua_State *const L);
static int _timer_index(lua_State *const L);
static int _timer_newindex(lua_State *const L);
static int _timer_cancel(lua_State *const L);
static int _timer_reset(lua_State *const L);
static int _timer_pause(lua_State *const L);
static int _timer_resume(lua_State *const L);
static void _timer_gc(void *_ud);
static Timer * _timer_get_ptr(lua_State *const L, const int idx);
static void _timer_pushNewTable(lua_State *const L, const TICK_DELTA_SEC_T time, const bool repeat, const int callbackIdx);
static void _timer_insertLuaTimerInRegistry(lua_State *const L, Timer *const cTimer);
static void _timer_removeLuaTimerFromRegistry(lua_State *const L, Timer *const cTimer);

// --------------------------------------------------
//
// MARK: - Exposed functions -
//
// --------------------------------------------------

bool lua_g_timer_create_and_push(lua_State *const L) {
    vx_assert(L != nullptr);

    LUAUTILS_STACK_SIZE(L)

    lua_newuserdata(L, 1);
    lua_newtable(L);
    {
        lua_pushstring(L, "__index");
        lua_pushcfunction(L, _g_timer_index, "");
        lua_rawset(L, -3);

        lua_pushstring(L, "__newindex");
        lua_pushcfunction(L, _g_timer_newindex, "");
        lua_rawset(L, -3);

        lua_pushstring(L, "__call");
        lua_pushcfunction(L, _g_timer_call, "");
        lua_rawset(L, -3);

        lua_pushstring(L, "__metatable");
        lua_pushboolean(L, false);
        lua_rawset(L, -3);

        lua_pushstring(L, "__type");
        lua_pushstring(L, "TimerInterface");
        lua_rawset(L, -3);

        lua_pushstring(L, "__typen");
        lua_pushinteger(L, ITEM_TYPE_G_TIMER);
        lua_rawset(L, -3);

        // create Timers array
        lua_pushstring(L, LUA_TIMER_ARRAY_TIMERS);
        lua_newtable(L);
        lua_rawset(L, -3);
    }
    lua_setmetatable(L, -2);

    LUAUTILS_STACK_SIZE_ASSERT(L, 1);
    return true;
}

// --------------------------------------------------
//
// MARK: - static functions
//
// --------------------------------------------------

static void callback(Timer * const c_timer, void *userData) {
    // get lua state from userdata
    lua_State *L = static_cast<lua_State *>(userData);
    L = lua_mainthread(L);
    LUAUTILS_STACK_SIZE(L)

    lua_utils_pushRootGlobalsFromRegistry(L);
    lua_getfield(L, -1, P3S_LUA_G_TIMER);
    vx_assert(lua_type(L, -1) == LUA_TUSERDATA);
    lua_remove(L, -2); // remove root globals

    int type = LUA_GET_METAFIELD_AND_RETURN_TYPE(L, -1, LUA_TIMER_ARRAY_TIMERS);
    vx_assert(type == LUA_TTABLE);

    // get the Timer stored in global Timer with its address
    lua_pushlightuserdata(L, c_timer); // using pointer as key
    type = lua_rawget(L, -2);

    if (type == LUA_TNIL) {
        lua_pop(L, 3);
        LUAUTILS_STACK_SIZE_ASSERT(L, 0)
        return;
    }

    vx_assert(type == LUA_TUSERDATA);
    vx_assert(lua_utils_getObjectType(L, -1) == ITEM_TYPE_TIMER);

    type = LUA_GET_METAFIELD_AND_RETURN_TYPE(L, -1, LUA_TIMER_CALLBACK); // push the function on top of the stack
    vx_assert(type == LUA_TFUNCTION);

    if (lua_pcall(L, 0, 0, 0) != LUA_OK) {
        // lua_pcall pushes the error onto the Lua stack
        if (lua_utils_isStringStrict(L, -1)) {
            lua_log_error_CStr(L, lua_tostring(L, -1));
        } else {
            lua_log_error_CStr(L, "Timer: error on callback");
        }
        lua_pop(L, 1);
    }

    lua_pop(L, 3);

    if (timer_get_repeat(c_timer) == false) {
        _timer_removeLuaTimerFromRegistry(L, c_timer);
    }
    
    LUAUTILS_STACK_SIZE_ASSERT(L, 0)
}

static void _timer_pushNewTable(lua_State *const L,
                                const TICK_DELTA_SEC_T time,
                                const bool repeat,
                                const int callbackIdx) {
    vx_assert(L != nullptr);
    LUAUTILS_STACK_SIZE(L)

    vx::Game *g = nullptr;
    if (lua_utils_getGameCppWrapperPtr(L, &g) == false) {
        LUAUTILS_INTERNAL_ERROR(L);
    }

    Timer *newTimer = timer_new(time, repeat, callback, lua_mainthread(L));
    vx_assert(newTimer != nullptr);

    game_add_timer(g->getCGame(), newTimer);

    timer_userdata *ud = static_cast<timer_userdata*>(lua_newuserdatadtor(L, sizeof(timer_userdata), _timer_gc));
    ud->timer = newTimer;
    ud->time = time;

    // connect to userdata store
    ud->store = g->userdataStoreForTimers;
    ud->previous = nullptr;
    timer_userdata* next = static_cast<timer_userdata*>(ud->store->add(ud));
    ud->next = next;
    if (next != nullptr) {
        next->previous = ud;
    }

    lua_newtable(L); // metatable
    {
        lua_pushstring(L, LUA_TIMER_FIELD_CANCEL);
        lua_pushcfunction(L, _timer_cancel, "");
        lua_rawset(L, -3);

        lua_pushstring(L, LUA_TIMER_FIELD_RESET);
        lua_pushcfunction(L, _timer_reset, "");
        lua_rawset(L, -3);

        lua_pushstring(L, LUA_TIMER_FIELD_PAUSE);
        lua_pushcfunction(L, _timer_pause, "");
        lua_rawset(L, -3);

        lua_pushstring(L, LUA_TIMER_FIELD_RESUME);
        lua_pushcfunction(L, _timer_resume, "");
        lua_rawset(L, -3);

        lua_pushstring(L, "__index");
        lua_pushcfunction(L, _timer_index, "");
        lua_rawset(L, -3);

        lua_pushstring(L, "__newindex");
        lua_pushcfunction(L, _timer_newindex, "");
        lua_rawset(L, -3);

        lua_pushstring(L, "__metatable");
        lua_pushboolean(L, false);
        lua_rawset(L, -3);

        lua_pushstring(L, LUA_TIMER_CALLBACK);
        lua_pushvalue(L, callbackIdx);
        lua_rawset(L, -3);

        lua_pushstring(L, "__type");
        lua_pushstring(L, "Timer");
        lua_rawset(L, -3);

        lua_pushstring(L, "__typen");
        lua_pushinteger(L, ITEM_TYPE_TIMER);
        lua_rawset(L, -3);
    }
    lua_setmetatable(L, -2);

    _timer_insertLuaTimerInRegistry(L, newTimer);

    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
}

// --------------------------------------------------
//
// MARK: - static functions
//
// --------------------------------------------------

static int _g_timer_index(lua_State *const L) {
    LUAUTILS_STACK_SIZE(L)
    lua_pushnil(L);
    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
    return 1;
}

static int _g_timer_newindex(lua_State *const L) {
    LUAUTILS_STACK_SIZE(L)
    LUAUTILS_STACK_SIZE_ASSERT(L, 0)
    return 0;
}

// global Timer()
// time number
// repeat boolean (optional)
// callback function
static int _g_timer_call(lua_State *const L) {
    vx_assert(L != nullptr);
    LUAUTILS_STACK_SIZE(L)

    bool hasRepeat = false;
    bool repeat = false;
    int functionIdx = 3;

    // check the number of arguments
    if (lua_gettop(L) == 4) {
        hasRepeat = true;
    } else if (lua_gettop(L) == 3) {
        hasRepeat = false;
    } else {
        LUAUTILS_ERROR(L, "Timer: use 2 arguments (number and function)");
    }

    if (lua_isnumber(L, 2) == false) {
        LUAUTILS_ERROR(L, "Timer: first argument should be a number");
    }
    TICK_DELTA_SEC_T time = lua_tonumber(L, 2);

    if (hasRepeat) {
        if (lua_isboolean(L, 3) == false) {
            LUAUTILS_ERROR(L, "Timer: second argument should be a boolean");
        }
        repeat = lua_toboolean(L, 3);

        functionIdx = 4;

    } else {
        functionIdx = 3;
    }

    if (lua_isfunction(L, functionIdx) == false) {
        LUAUTILS_ERROR_F(L,
                         "Timer: %s argument should be a function",
                         functionIdx == 3 ? "second" : "third");
    }
    _timer_pushNewTable(L, time, repeat, functionIdx);

    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
    return 1;
}

static int _timer_index(lua_State *const L) {
    vx_assert(L != nullptr);
    LUAUTILS_ASSERT_ARGS_COUNT(L, 2)
    LUAUTILS_ASSERT_ARG_TYPE(L, 1, ITEM_TYPE_TIMER)
    LUAUTILS_STACK_SIZE(L)

    if (lua_isstring(L, 2) == false) {
        LUAUTILS_ERROR(L, "Timer only has string keys");
    }

    const char *key = lua_tostring(L, 2);

    if (strcmp(key, LUA_TIMER_FIELD_REMAINING_TIME) == 0) {
        Timer *t = _timer_get_ptr(L, 1);

        timer_state state = timer_get_state(t);
        if (state == running || state == paused) {
            lua_pushnumber(L, static_cast<double>(timer_get_remaining_time(t)));
        } else {
            lua_pushnil(L);
        }
        
    } else if (strcmp(key, LUA_TIMER_FIELD_TIME) == 0) {
        timer_userdata *ud = static_cast<timer_userdata*>(lua_touserdata(L, 1));
        lua_pushnumber(L, ud->time);
    } else {
        LUA_GET_METAFIELD(L, 1, key);
    }

    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
    return 1;
}

static int _timer_newindex(lua_State *const L) {
    LUAUTILS_STACK_SIZE(L)
    LUAUTILS_STACK_SIZE_ASSERT(L, 0)
    return 0;
}

static int _timer_cancel(lua_State *const L) {
    LUAUTILS_STACK_SIZE(L)

    if (lua_gettop(L) < 1) {
        LUAUTILS_ERROR(L, "Timer:Cancel() should be called with :");
    } else if (lua_gettop(L) != 1) {
        LUAUTILS_ERROR(L, "Timer:Cancel() doesn't accept arguments");
    }

    Timer *const cTimer = _timer_get_ptr(L, 1);
    timer_cancel(cTimer);

    _timer_removeLuaTimerFromRegistry(L, cTimer);

    LUAUTILS_STACK_SIZE_ASSERT(L, 0)
    return 0;
}

static int _timer_reset(lua_State *const L) {
    LUAUTILS_STACK_SIZE(L)

    if (lua_gettop(L) < 1) {
        LUAUTILS_ERROR(L, "Timer:Reset() should be called with :");
    } else if (lua_gettop(L) != 1) {
        LUAUTILS_ERROR(L, "Timer:Reset() doesn't accept arguments");
    }

    Timer *const cTimer = _timer_get_ptr(L, 1);
    timer_reset(cTimer);

    _timer_insertLuaTimerInRegistry(L, cTimer);

    LUAUTILS_STACK_SIZE_ASSERT(L, 0)
    return 0;
}

static int _timer_pause(lua_State *const L) {
    LUAUTILS_STACK_SIZE(L)

    if (lua_gettop(L) < 1) {
        LUAUTILS_ERROR(L, "Timer:Pause() should be called with :");
    } else if (lua_gettop(L) != 1) {
        LUAUTILS_ERROR(L, "Timer:Pause() doesn't accept arguments");
    }

    Timer *const cTimer = _timer_get_ptr(L, 1);
    timer_pause(cTimer);

    _timer_removeLuaTimerFromRegistry(L, cTimer);

    LUAUTILS_STACK_SIZE_ASSERT(L, 0)
    return 0;
}

static int _timer_resume(lua_State *const L) {
    LUAUTILS_STACK_SIZE(L)

    if (lua_gettop(L) < 1) {
        LUAUTILS_ERROR(L, "Timer:Resume() should be called with :");
    } else if (lua_gettop(L) != 1) {
        LUAUTILS_ERROR(L, "Timer:Resume() doesn't accept arguments");
    }

    Timer *const cTimer = _timer_get_ptr(L, 1);
    timer_resume(cTimer);

    _timer_insertLuaTimerInRegistry(L, cTimer);

    LUAUTILS_STACK_SIZE_ASSERT(L, 0)
    return 0;
}

static void _timer_gc(void *_ud) {
    timer_userdata *ud = static_cast<timer_userdata*>(_ud);
    timer_flagForDeletion(ud->timer);

    // disconnect from userdata store
    {
        if (ud->previous != nullptr) {
            ud->previous->next = ud->next;
        }
        if (ud->next != nullptr) {
            ud->next->previous = ud->previous;
        }
        ud->store->removeWithoutKeepingID(ud, ud->next);
    }
}

static Timer * _timer_get_ptr(lua_State *const L, const int idx) {
    timer_userdata *ud = static_cast<timer_userdata*>(lua_touserdata(L, idx));
    return ud->timer;
}

static void _timer_insertLuaTimerInRegistry(lua_State *const L, Timer *const cTimer) {
    vx_assert(L != nullptr);
    LUAUTILS_STACK_SIZE(L)
    // store the Timer in the global Timer table
    int type = LUA_GET_GLOBAL_AND_RETURN_TYPE(L, P3S_LUA_G_TIMER);
    vx_assert(type == LUA_TUSERDATA);
    type = LUA_GET_METAFIELD_AND_RETURN_TYPE(L, -1, LUA_TIMER_ARRAY_TIMERS);
    vx_assert(type == LUA_TTABLE);
    lua_pushlightuserdata(L, cTimer); // using pointer as key
    lua_pushvalue(L, -4);
    lua_rawset(L, -3);
    lua_pop(L, 2);
    LUAUTILS_STACK_SIZE_ASSERT(L, 0);
}

static void _timer_removeLuaTimerFromRegistry(lua_State *const L, Timer *const cTimer) {
    vx_assert(L != nullptr);
    LUAUTILS_STACK_SIZE(L)
    // remove Timer from global table, to allow it to be GC'ed
    int type = LUA_GET_GLOBAL_AND_RETURN_TYPE(L, P3S_LUA_G_TIMER);
    vx_assert(type == LUA_TUSERDATA);
    type = LUA_GET_METAFIELD_AND_RETURN_TYPE(L, -1, LUA_TIMER_ARRAY_TIMERS);
    vx_assert(type == LUA_TTABLE);
    lua_pushlightuserdata(L, cTimer);
    lua_pushnil(L);
    lua_rawset(L, -3);
    lua_pop(L, 2);
    LUAUTILS_STACK_SIZE_ASSERT(L, 0);
}
