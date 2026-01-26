//
//  lua_audioListener.cpp
//  Cubzh
//
//  Created by Gaetan de Villele on 17/08/2022.
//

#include "lua_audioListener.hpp"

#include "audioListener.hpp"

#if !defined(P3S_CLIENT_HEADLESS)

// C++
#include <cstring>

// Lua
#include "lua.hpp"
#include "lua_constants.h"
#include "lua_number3.hpp"
#include "lua_object.hpp"
#include "lua_utils.hpp"

#include "config.h"

typedef struct audioListener_userdata {
    AudioListener *listener;
} audioListener_userdata;

// MARK: - static functions prototypes -

static int _g_audiolistener_metatable_index(lua_State * const L);
static int _g_audiolistener_metatable_newindex(lua_State * const L);

static void _g_audiolistener_gc(void *_ud);

bool _lua_g_audioListener_metatable_index(lua_State * const L, AudioListener *al, const char *key);
bool _lua_g_audioListener_metatable_newindex(lua_State * const L, AudioListener *al, const char *key);

// MARK: - exposed functions -

void lua_g_audiolistener_createAndPush(lua_State * const L) {
    vx_assert(L != nullptr);
    LUAUTILS_STACK_SIZE(L)
    
    // create underlying AudioSource object
    AudioListener * const al = audioListener_new();
    if (al == nullptr) {
        lua_pushnil(L);
        LUAUTILS_STACK_SIZE_ASSERT(L, 1)
        return;
    }

    audioListener_userdata *ud = static_cast<audioListener_userdata *>(lua_newuserdatadtor(L, sizeof(audioListener_userdata), _g_audiolistener_gc));
    ud->listener = al;

    lua_newtable(L); // metatable
    {
        lua_pushstring(L, "__index");
        lua_pushcfunction(L, _g_audiolistener_metatable_index, "");
        lua_rawset(L, -3);

        lua_pushstring(L, "__newindex");
        lua_pushcfunction(L, _g_audiolistener_metatable_newindex, "");
        lua_rawset(L, -3);
        
        // hide the metatable from the Lua sandbox user
        lua_pushstring(L, "__metatable");
        lua_pushboolean(L, false);
        lua_rawset(L, -3);

        lua_pushstring(L, "__type");
        lua_pushstring(L, "AudioListener");
        lua_rawset(L, -3);

        lua_pushstring(L, "__typen");
        lua_pushinteger(L, ITEM_TYPE_G_AUDIOLISTENER);
        lua_rawset(L, -3);
    }
    lua_setmetatable(L, -2);

    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
}

void lua_g_audioListener_push(lua_State * const L) {
    LUAUTILS_STACK_SIZE(L)

    lua_utils_pushRootGlobalsFromRegistry(L);
    if (lua_getmetatable(L, -1) != 1) {
        vxlog_error("Couldn't get globals table metatable");
        LUAUTILS_INTERNAL_ERROR(L)
    }
    if (lua_getfield(L, -1, LUA_G_AUDIOLISTENER) != LUA_TUSERDATA) {
        vxlog_error("Couldn't get global table AudioListener");
        LUAUTILS_INTERNAL_ERROR(L)
    }
    lua_remove(L, -2); // remove globals metatable
    lua_remove(L, -2); // remove globals table

    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
}

AudioListener *lua_g_audioListener_getAudioListenerPtr(lua_State * const L, const int idx) {
    audioListener_userdata *ud = static_cast<audioListener_userdata*>(lua_touserdata(L, idx));
    return ud->listener;
}

int lua_g_audiolistener_snprintf(lua_State * const L, char *result, size_t maxLen, bool spacePrefix) {
    vx_assert(L != nullptr);
    return snprintf(result, maxLen, "%s[AudioListener] (global)", spacePrefix ? " " : "");
}

// MARK: - static functions -

static int _g_audiolistener_metatable_index(lua_State * const L) {
    vx_assert(L != nullptr);
    LUAUTILS_ASSERT_ARGS_COUNT(L, 2)
    LUAUTILS_ASSERT_ARG_TYPE(L, 1, ITEM_TYPE_G_AUDIOLISTENER)
    LUAUTILS_STACK_SIZE(L)

    const char *key = lua_tostring(L, 2);
    if (_lua_object_metatable_index(L, key) == false) {
        // no field found in Object
        // retrieve underlying AudioListener struct
        AudioListener *al = lua_g_audioListener_getAudioListenerPtr(L, 1);
        if (al == nullptr) {
            lua_pushnil(L);
        } else if (_lua_g_audioListener_metatable_index(L, al, key) == false) {
            // key is not known and starts with an uppercase letter
            lua_pushnil(L);
        }
    }
    
    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
    return 1;
}

// do nothing, AudioListener global table is read-only
static int _g_audiolistener_metatable_newindex(lua_State * const L) {
    vx_assert(L != nullptr);
    LUAUTILS_ASSERT_ARGS_COUNT(L, 3)
    LUAUTILS_ASSERT_ARG_TYPE(L, 1, ITEM_TYPE_G_AUDIOLISTENER)
    LUAUTILS_STACK_SIZE(L)

    // retrieve underlying AudioSource struct
    AudioListener * const al = lua_g_audioListener_getAudioListenerPtr(L, 1);
    if (al == nullptr) {
        lua_pushnil(L);
        LUAUTILS_STACK_SIZE_ASSERT(L, 1)
        return 1;
    }
    
    Transform *t = audioListener_getTransform(al);

    const char *key = lua_tostring(L, 2);

    if (_lua_object_metatable_newindex(L, t, key) == true) {
        // a key from Object has been set
    } else { // the key is not from Object
        if (_lua_g_audioListener_metatable_newindex(L, al, key) == false) {
            // key not found
            LUAUTILS_ERROR_F(L, "AudioListener: %s field is not settable", key);
        }
    }
    
    LUAUTILS_STACK_SIZE_ASSERT(L, 0)
    return 0;
}

static void _g_audiolistener_gc(void *_ud) {
    audioListener_userdata *ud = static_cast<audioListener_userdata*>(_ud);
    AudioListener * const al = ud->listener;
    if (al != nullptr) {
        audioListener_free(al);
        ud->listener = nullptr;
    }
}

bool _lua_g_audioListener_metatable_index(lua_State * const L, AudioListener *al, const char *key) {
    vx_assert(L != nullptr);
    vx_assert(al != nullptr);
    return false;
}

bool _lua_g_audioListener_metatable_newindex(lua_State * const L, AudioListener *al, const char *key) {
    vx_assert(L != nullptr);
    vx_assert(al != nullptr);
    return false;
}

#else

void lua_g_audiolistener_createAndPush(lua_State * const L) {}
void lua_g_audioListener_push(lua_State * const L) { }
AudioListener *lua_g_audioListener_getAudioListenerPtr(lua_State * const L, const int idx) { return nullptr; }
int lua_g_audiolistener_snprintf(lua_State * const L, char *result, size_t maxLen, bool spacePrefix) { return 0; }

#endif
