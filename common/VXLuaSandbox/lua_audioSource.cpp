//
//  lua_audioSource.cpp
//  Cubzh
//
//  Created by Gaetan de Villele on 17/08/2022.
//

#include "lua_audioSource.hpp"

// C++
#include <cstring>

// Lua
#include "lua.hpp"
#include "lua_utils.hpp"
#include "lua_constants.h"
#include "lua_object.hpp"
#include "lua_data.hpp"

#include "VXGame.hpp"

#include "config.h"
#include "audioSource.hpp"
#include "game.h"

#define LUA_AUDIOSOURCE_FIELD_LOOP        "Loop"        //
#define LUA_AUDIOSOURCE_FIELD_PAN         "Pan"         //
#define LUA_AUDIOSOURCE_FIELD_PAUSE       "Pause"       //
#define LUA_AUDIOSOURCE_FIELD_PLAY        "Play"        //
#define LUA_AUDIOSOURCE_FIELD_SOUND       "Sound"       //
#define LUA_AUDIOSOURCE_FIELD_STARTAT     "StartAt"     //
#define LUA_AUDIOSOURCE_FIELD_STOP        "Stop"        //
#define LUA_AUDIOSOURCE_FIELD_STOPAT      "StopAt"      //
#define LUA_AUDIOSOURCE_FIELD_VOLUME      "Volume"      //
#define LUA_AUDIOSOURCE_FIELD_SPATIALIZED "Spatialized" //
#define LUA_AUDIOSOURCE_FIELD_RADIUS      "Radius"      //
#define LUA_AUDIOSOURCE_FIELD_MIN_RADIUS  "MinRadius"   //
#define LUA_AUDIOSOURCE_FIELD_PITCH       "Pitch"       //
#define LUA_AUDIOSOURCE_FIELD_LENGTH      "Length"      // number (read-only)
#define LUA_AUDIOSOURCE_FIELD_IS_PLAYING  "IsPlaying"   // booleam (read-only)

#ifndef P3S_CLIENT_HEADLESS

typedef struct audiosource_userdata {
    // Userdata instance requirements
    vx::UserDataStore *store;
    audiosource_userdata *next;
    audiosource_userdata *previous;

    AudioSource *audioSource;
} audiosource_userdata;

static int _g_audioSource_metatable_index(lua_State * const L);
static int _g_audioSource_metatable_newindex(lua_State * const L);
static int _g_audioSource_metatable_call(lua_State * const L);

static int _audioSource_metatable_index(lua_State * const L);
static int _audioSource_metatable_newindex(lua_State * const L);
static void _audioSource_gc(void *ud);

bool _lua_audioSource_metatable_index(lua_State * const L, AudioSource *as, const char *key);
bool _lua_audioSource_metatable_newindex(lua_State * const L, AudioSource *as, const char *key);

int _audioSource_pause(lua_State * const L);
int _audioSource_play(lua_State * const L);
int _audioSource_stop(lua_State * const L);

void lua_g_audioSource_createAndPush(lua_State * const L) {
    vx_assert(L != nullptr);
    LUAUTILS_STACK_SIZE(L)
    
    lua_newuserdata(L, 1); // global "AudioSource" table
    lua_newtable(L); // metatable
    {
        lua_pushstring(L, "__index");
        lua_pushcfunction(L, _g_audioSource_metatable_index, "");
        lua_rawset(L, -3);
        
        lua_pushstring(L, "__newindex");
        lua_pushcfunction(L, _g_audioSource_metatable_newindex, "");
        lua_rawset(L, -3);
        
        lua_pushstring(L, "__call");
        lua_pushcfunction(L, _g_audioSource_metatable_call, "");
        lua_rawset(L, -3);
        
        // hide the metatable from the Lua sandbox user
        lua_pushstring(L, "__metatable");
        lua_pushboolean(L, false);
        lua_rawset(L, -3);

        lua_pushstring(L, "__type");
        lua_pushstring(L, "AudioSourceInterface");
        lua_rawset(L, -3);

        lua_pushstring(L, "__typen");
        lua_pushinteger(L, ITEM_TYPE_G_AUDIOSOURCE);
        lua_rawset(L, -3);

        lua_pushstring(L, "__imt"); // instance metatable
        lua_newtable(L); // metatable
        {
            lua_pushstring(L, "__index");
            lua_pushcfunction(L, _audioSource_metatable_index, "");
            lua_rawset(L, -3);

            lua_pushstring(L, "__newindex");
            lua_pushcfunction(L, _audioSource_metatable_newindex, "");
            lua_rawset(L, -3);

            // hide the metatable from the Lua sandbox user
            lua_pushstring(L, "__metatable");
            lua_pushboolean(L, false);
            lua_rawset(L, -3);

            lua_pushstring(L, "__type");
            lua_pushstring(L, "AudioSource");
            lua_rawset(L, -3);

            lua_pushstring(L, "__typen");
            lua_pushinteger(L, ITEM_TYPE_AUDIOSOURCE);
            lua_rawset(L, -3);
        }
        lua_rawset(L, -3);
    }
    lua_setmetatable(L, -2);
    
    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
}

bool lua_audioSource_pushNewInstance(lua_State * const L, AudioSource * const as) {
    vx_assert(L != nullptr);
    if (as == nullptr) return false;
    
    Transform *t = audioSource_getTransform(as);
    
    // lua AudioSource takes ownership
    if (transform_retain(t) == false) {
        LUAUTILS_INTERNAL_ERROR(L);
        // return false;
    }

    LUAUTILS_STACK_SIZE(L)

    vx::Game *g = nullptr;
    if (lua_utils_getGameCppWrapperPtr(L, &g) == false) {
        LUAUTILS_INTERNAL_ERROR(L);
    }

    audiosource_userdata *ud = static_cast<audiosource_userdata*>(lua_newuserdatadtor(L, sizeof(audiosource_userdata), _audioSource_gc));
    ud->audioSource = as;

    // connect to userdata store
    ud->store = g->userdataStoreForAudioSources;
    ud->previous = nullptr;
    audiosource_userdata* next = static_cast<audiosource_userdata*>(ud->store->add(ud));
    ud->next = next;
    if (next != nullptr) {
        next->previous = ud;
    }

    lua_getglobal(L, LUA_G_AUDIOSOURCE);
    LUA_GET_METAFIELD(L, -1, "__imt");
    lua_remove(L, -2);
    lua_setmetatable(L, -2);
    
    if (lua_g_object_addIndexEntry(L, audioSource_getTransform(as), -1) == false) {
        vxlog_error("failed to add Lua AudioSource to Object registry");
        lua_pop(L, 1); // pop Lua AudioSource
        LUAUTILS_STACK_SIZE_ASSERT(L, 0)
        return false;
    }

    scene_register_managed_transform(game_get_scene(g->getCGame()), t);

    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
    return true;
}

AudioSource *lua_audioSource_getAudioSourcePtr(lua_State * const L, const int idx) {
    vx_assert(L != nullptr);
    audiosource_userdata *ud = static_cast<audiosource_userdata*>(lua_touserdata(L, idx));
    return ud->audioSource;
}

int lua_g_audioSource_snprintf(lua_State * const L, char *result, size_t maxLen, bool spacePrefix) {
    vx_assert(L != nullptr);
    return snprintf(result, maxLen, "%s[AudioSource] (global)", spacePrefix ? " " : "");
}

int lua_audioSource_snprintf(lua_State * const L, char *result, size_t maxLen, bool spacePrefix) {
    vx_assert(L != nullptr);
    return snprintf(result, maxLen, "%s[AudioSource]", spacePrefix ? " " : "");
}

// always return nil
static int _g_audioSource_metatable_index(lua_State * const L) {
    vx_assert(L != nullptr);
    LUAUTILS_STACK_SIZE(L)
    lua_pushnil(L);
    LUAUTILS_STACK_SIZE_ASSERT(L, 1);
    return 1;
}

// do nothing, AudioSource global table is read-only
static int _g_audioSource_metatable_newindex(lua_State * const L) {
    vx_assert(L != nullptr);
    LUAUTILS_STACK_SIZE(L)
    // nothing
    LUAUTILS_STACK_SIZE_ASSERT(L, 0);
    return 0;
}

static int _g_audioSource_metatable_call(lua_State * const L) {
    vx_assert(L != nullptr);
    LUAUTILS_STACK_SIZE(L)
    
    const int nbArgs = lua_gettop(L);
    if (nbArgs > 2) {
        LUAUTILS_ERROR(L, "AudioSource: function does not accept more than 1 argument");
    }
    if (nbArgs == 2 && lua_utils_isStringStrict(L, 2) == false) {
        LUAUTILS_ERROR(L, "AudioSource: argument 1 should be a string");
    }
    
    // create underlying AudioSource object
    AudioSource * const as = audioSource_new();
    if (lua_audioSource_pushNewInstance(L, as) == false) {
        lua_pushnil(L); // GC will release transform
    }
    transform_release(audioSource_getTransform(as)); // now owned by lua AudioSource
    
    // set sound if specified
    if (nbArgs == 2) {
        const char *name = lua_tostring(L, 2);
        audioSource_setSound(as, name);
    }
    
    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
    return 1;
}

static int _audioSource_metatable_index(lua_State * const L) {
    vx_assert(L != nullptr);
    LUAUTILS_ASSERT_ARGS_COUNT(L, 2)
    LUAUTILS_ASSERT_ARG_TYPE(L, 1, ITEM_TYPE_AUDIOSOURCE)
    LUAUTILS_STACK_SIZE(L)

    const char *key = lua_tostring(L, 2);
    if (_lua_object_metatable_index(L, key) == false) {
        // no field found in Object
        // retrieve underlying AudioSource struct
        AudioSource *as = lua_audioSource_getAudioSourcePtr(L, 1);
        if (as == nullptr) {
            lua_pushnil(L);
        } else if (_lua_audioSource_metatable_index(L, as, key) == false) {
            // key is not known and starts with an uppercase letter
            lua_pushnil(L);
        }
    }
    
    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
    return 1;
}

static int _audioSource_metatable_newindex(lua_State * const L) {
    vx_assert(L != nullptr);
    LUAUTILS_ASSERT_ARGS_COUNT(L, 3)
    LUAUTILS_ASSERT_ARG_TYPE(L, 1, ITEM_TYPE_AUDIOSOURCE)
    LUAUTILS_STACK_SIZE(L)
    
    // retrieve underlying AudioSource struct
    AudioSource * const as = lua_audioSource_getAudioSourcePtr(L, 1);
    if (as == nullptr) {
        lua_pushnil(L);
        LUAUTILS_STACK_SIZE_ASSERT(L, 1)
        return 1;
    }
    Transform *t = audioSource_getTransform(as);
    
    const char *key = lua_tostring(L, 2);
    
    if (_lua_object_metatable_newindex(L, t, key) == true) {
        // a key from Object has been set
    } else { // the key is not from Object
        if (_lua_audioSource_metatable_newindex(L, as, key) == false) {
            // key not found
            LUAUTILS_ERROR_F(L, "AudioSource: %s field is not settable", key);
        }
    }
    
    LUAUTILS_STACK_SIZE_ASSERT(L, 0)
    return 0;
}

void lua_audioSource_release_transform(void *_ud) {
    audiosource_userdata *ud = static_cast<audiosource_userdata*>(_ud);
    AudioSource * const as = ud->audioSource;
    if (as != nullptr) {
        audioSource_free(as); // transform_release
        ud->audioSource = nullptr;
    }
}

static void _audioSource_gc(void *_ud) {
    audiosource_userdata *ud = static_cast<audiosource_userdata*>(_ud);
    AudioSource * const as = ud->audioSource;
    if (as != nullptr) {
        audioSource_free(as); // transform_release
        ud->audioSource = nullptr;
    }

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

bool _lua_audioSource_metatable_index(lua_State * const L, AudioSource *as, const char *key) {
    if (strcmp(key, LUA_AUDIOSOURCE_FIELD_PAN) == 0) {
        const float value = audioSource_getPan(as);
        lua_pushnumber(L, static_cast<lua_Number>(value));
        return true;
    } else if (strcmp(key, LUA_AUDIOSOURCE_FIELD_PAUSE) == 0) {
        lua_pushcfunction(L, _audioSource_pause, "");
        return true;
    } else if (strcmp(key, LUA_AUDIOSOURCE_FIELD_PLAY) == 0) {
        lua_pushcfunction(L, _audioSource_play, "");
        return true;
    } else if (strcmp(key, LUA_AUDIOSOURCE_FIELD_SOUND) == 0) {
        const char *soundName = audioSource_getSoundName(as);
        if (soundName == nullptr) {
            lua_pushnil(L);
        } else {
            lua_pushstring(L, soundName);
        }
        return true;
    } else if (strcmp(key, LUA_AUDIOSOURCE_FIELD_STARTAT) == 0) {
        if (audioSource_getStartAtEnabled(as)) {
            const float startAt = audioSource_getStartAt(as);
            lua_pushnumber(L, static_cast<lua_Number>(startAt));
        } else {
            lua_pushnil(L);
        }
        return true;
    } else if (strcmp(key, LUA_AUDIOSOURCE_FIELD_STOP) == 0) {
        lua_pushcfunction(L, _audioSource_stop, "");
        return true;
    } else if (strcmp(key, LUA_AUDIOSOURCE_FIELD_STOPAT) == 0) {
        if (audioSource_getStopAtEnabled(as)) {
            const float stopAt = audioSource_getStopAt(as);
            lua_pushnumber(L, static_cast<lua_Number>(stopAt));
        } else {
            lua_pushnil(L);
        }
        return true;
    } else if (strcmp(key, LUA_AUDIOSOURCE_FIELD_VOLUME) == 0) {
        const float volume = audioSource_getVolume(as);
        lua_pushnumber(L, static_cast<lua_Number>(volume));
        return true;
    } else if (strcmp(key, LUA_AUDIOSOURCE_FIELD_SPATIALIZED) == 0) {
        const bool value = audioSource_getSpatialized(as);
        lua_pushboolean(L, value);
        return true;
    } else if (strcmp(key, LUA_AUDIOSOURCE_FIELD_RADIUS) == 0) {
        const float value = audioSource_getRadius(as);
        lua_pushnumber(L, static_cast<lua_Number>(value));
        return true;
    } else if (strcmp(key, LUA_AUDIOSOURCE_FIELD_MIN_RADIUS) == 0) {
        const float value = audioSource_getMinRadius(as);
        lua_pushnumber(L, static_cast<lua_Number>(value));
        return true;
    } else if (strcmp(key, LUA_AUDIOSOURCE_FIELD_PITCH) == 0) {
        const float value = audioSource_getPitch(as);
        lua_pushnumber(L, static_cast<lua_Number>(value));
        return true;
        
    } else if (strcmp(key, LUA_AUDIOSOURCE_FIELD_LENGTH) == 0) {
        float duration = audioSource_getOriginalDuration(as);
        lua_pushnumber(L, static_cast<lua_Number>(duration));
        return true;
        
    } else if (strcmp(key, LUA_AUDIOSOURCE_FIELD_IS_PLAYING) == 0) {
        bool isPlaying = audioSource_isPlaying(as);
        lua_pushboolean(L, isPlaying);
        return true;
        
    } else if (strcmp(key, LUA_AUDIOSOURCE_FIELD_LOOP) == 0) {
        const bool value = audioSource_getLooping(as);
        lua_pushboolean(L, value);
        return true;
    }
    return false;
}

bool _lua_audioSource_metatable_newindex(lua_State * const L, AudioSource *as, const char *key) {
    if (strcmp(key, LUA_AUDIOSOURCE_FIELD_PAN) == 0) {
        if (lua_utils_isNumberStrict(L, 3)) {
            const lua_Number value = lua_tonumber(L, 3);
            audioSource_setPan(as, value);
        } else {
            LUAUTILS_ERROR(L, "AudioSource.Pan must be a number");
        }
        return true;
    } else if (strcmp(key, LUA_AUDIOSOURCE_FIELD_SOUND) == 0) {
        if (lua_utils_isStringStrict(L, 3)) {
            const char *soundName = lua_tostring(L, 3);
            audioSource_setSound(as, soundName);
        } else if (lua_utils_getObjectType(L, 3) == ITEM_TYPE_DATA) {
            // copy buffer from Data, not referencing Data itself
            size_t len = 0;
            const void *bufferPtr = lua_data_getBuffer(L, 3, &len);
            audioSource_setSoundBuffer(as, bufferPtr, len);
        } else {
            LUAUTILS_ERROR(L, "AudioSource.Sound must be a string");
        }
        return true;
        
    } else if (strcmp(key, LUA_AUDIOSOURCE_FIELD_STARTAT) == 0) {
        if (lua_isnil(L, 3)) {
            audioSource_disableStartAt(as);
            return true;
        }
        if (lua_isnumber(L, 3) == false) {
            LUAUTILS_ERROR(L, "AudioSource.StartAt must be a number or nil");
        }
        const lua_Number s = lua_tonumber(L, 3);
        if (audioSource_setStartAt(as, s) == false) {
            LUAUTILS_ERROR(L, "AudioSource.StartAt must lower than its Length");
        }
        return true;
        
    } else if (strcmp(key, LUA_AUDIOSOURCE_FIELD_STOPAT) == 0) {
        if (lua_isnil(L, 3)) {
            audioSource_disableStopAt(as);
            return true;
        }
        if (lua_isnumber(L, 3) == false) {
            LUAUTILS_ERROR(L, "AudioSource.StopAt must be an integer or nil");
        }
        const lua_Number s = lua_tonumber(L, 3);
        if (audioSource_setStopAt(as, s) == false) {
            LUAUTILS_ERROR(L, "AudioSource.StopAt must lower than its Length");
        }
        return true;
        
    } else if (strcmp(key, LUA_AUDIOSOURCE_FIELD_VOLUME) == 0) {
        if (lua_utils_isNumberStrict(L, 3)) {
            const lua_Number volume = lua_tonumber(L, 3);
            audioSource_setVolume(as, volume);
        } else {
            LUAUTILS_ERROR(L, "AudioSource.Volume must be a number");
        }
        return true;
    } else if (strcmp(key, LUA_AUDIOSOURCE_FIELD_SPATIALIZED) == 0) {
        if (lua_isboolean(L, 3)) {
            const bool value = lua_toboolean(L, 3);
            audioSource_setSpatialized(as, value);
        } else {
            LUAUTILS_ERROR(L, "AudioSource.Spatialized must be a boolean");
        }
        return true;
    } else if (strcmp(key, LUA_AUDIOSOURCE_FIELD_RADIUS) == 0) {
        if (lua_utils_isNumberStrict(L, 3)) {
            const lua_Number value = lua_tonumber(L, 3);
            audioSource_setRadius(as, value);
        } else {
            LUAUTILS_ERROR(L, "AudioSource.Radius must be a number");
        }
        return true;
    } else if (strcmp(key, LUA_AUDIOSOURCE_FIELD_MIN_RADIUS) == 0) {
        if (lua_utils_isNumberStrict(L, 3)) {
            const lua_Number value = lua_tonumber(L, 3);
            audioSource_setMinRadius(as, value);
        } else {
            LUAUTILS_ERROR(L, "AudioSource.MinRadius must be a number");
        }
        return true;
    } else if (strcmp(key, LUA_AUDIOSOURCE_FIELD_PITCH) == 0) {
        if (lua_utils_isNumberStrict(L, 3)) {
            const lua_Number value = lua_tonumber(L, 3);
            audioSource_setPitch(as, value);
        } else {
            LUAUTILS_ERROR(L, "AudioSource.Pitch must be a number");
        }
        return true;
        
    } else if (strcmp(key, LUA_AUDIOSOURCE_FIELD_LENGTH) == 0) {
        LUAUTILS_ERROR(L, "AudioSource.Length can't be set, use StartAt or StopAt instead");
        return false;
        
    } else if (strcmp(key, LUA_AUDIOSOURCE_FIELD_LOOP) == 0) {
        if (lua_isboolean(L, 3)) {
            const bool value = lua_toboolean(L, 3);
            audioSource_setLooping(as, value);
        } else {
            LUAUTILS_ERROR(L, "AudioSource.Loop must be a boolean");
        }
        return true;
    }
    return false;
}

int _audioSource_pause(lua_State * const L) {
    vx_assert(L != nullptr);
    LUAUTILS_STACK_SIZE(L)
    
    if (lua_utils_getObjectType(L, 1) != ITEM_TYPE_AUDIOSOURCE) {
        LUAUTILS_ERROR(L, "AudioSouce:Pause() must use `:`");
    }
    
    // retrieve underlying AudioSource struct
    AudioSource *as = lua_audioSource_getAudioSourcePtr(L, 1);
    if (as == nullptr) {
        LUAUTILS_INTERNAL_ERROR(L);
    }
    
    audioSource_pause(as);
    
    LUAUTILS_STACK_SIZE_ASSERT(L, 0);
    return 0;
}

int _audioSource_play(lua_State * const L) {
    vx_assert(L != nullptr);
    LUAUTILS_STACK_SIZE(L)
    
    if (lua_utils_getObjectType(L, 1) != ITEM_TYPE_AUDIOSOURCE) {
        LUAUTILS_ERROR(L, "AudioSouce:Play() must use `:`");
    }
    
    // retrieve underlying AudioSource struct
    AudioSource *as = lua_audioSource_getAudioSourcePtr(L, 1);
    if (as == nullptr) {
        LUAUTILS_INTERNAL_ERROR(L);
    }
    
    // force Position refresh before playing sound so that the spatialization
    // is not broken for the first note (while the tick hasn't refreshed the position)
    Transform *t = audioSource_getTransform(as);
    if (t != nullptr) {
        const float3 *position = transform_get_position(t, true);
        audioSource_setPosition(as, *position);
    }
    
    // play sound
    audioSource_play(as);
    
    LUAUTILS_STACK_SIZE_ASSERT(L, 0);
    return 0;
}

int _audioSource_stop(lua_State * const L) {
    vx_assert(L != nullptr);
    LUAUTILS_STACK_SIZE(L)
    
    if (lua_utils_getObjectType(L, 1) != ITEM_TYPE_AUDIOSOURCE) {
        LUAUTILS_ERROR(L, "AudioSouce:Stop() must use `:`");
    }
    
    // retrieve underlying AudioSource struct
    AudioSource *as = lua_audioSource_getAudioSourcePtr(L, 1);
    if (as == nullptr) {
        LUAUTILS_INTERNAL_ERROR(L);
    }
    
    audioSource_stop(as);
    
    LUAUTILS_STACK_SIZE_ASSERT(L, 0);
    return 0;
}

#else

void lua_g_audioSource_createAndPush(lua_State * const L) {}
int lua_g_audioSource_snprintf(lua_State * const L, char *result, size_t maxLen, bool spacePrefix) { return 0; }
bool lua_audioSource_pushNewInstance(lua_State * const L, AudioSource * const as) { return false; }
AudioSource *lua_audioSource_getAudioSourcePtr(lua_State * const L, const int idx) { return nullptr; }
int lua_audioSource_snprintf(lua_State * const L, char *result, size_t maxLen, bool spacePrefix) { return 0; }

#endif // P3S_CLIENT_HEADLESS
