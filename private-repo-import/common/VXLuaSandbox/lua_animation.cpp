//
//  lua_animation.cpp
//  Cubzh
//
//  Created by Corentin Cailleaud on 01/08/2023.
//

#include "lua_animation.hpp"

// C++
#include <cstring>

// Lua
#include "lua.hpp"
#include "lua_utils.hpp"
#include "animation.h"
#include "lua_constants.h"
#include "float3.h"
#include "lua_number3.hpp"
#include "keyframes.h"
#include "lua_object.hpp"

#include "config.h"

#define LUA_ANIMATION_METAFIELD_ON_PLAY_CALLBACKS "_oac"
#define LUA_ANIMATION_METAFIELD_ON_STOP_CALLBACKS "_orc"
#define LUA_ANIMATION_METAFIELD_GROUPS "_groups"

#define LUA_ANIMATION_FIELD_PLAY                     "Play"                 // function
#define LUA_ANIMATION_FIELD_PLAY_REVERSE             "PlayReverse"          // function
#define LUA_ANIMATION_FIELD_PAUSE                    "Pause"                // function
#define LUA_ANIMATION_FIELD_STOP                     "Stop"                 // function
#define LUA_ANIMATION_FIELD_TICK                     "Tick"                 // function
#define LUA_ANIMATION_FIELD_ADDFRAMEINGROUP          "AddFrameInGroup"      // function
#define LUA_ANIMATION_FIELD_GROUPS                   "Groups"               // table (read-only)
#define LUA_ANIMATION_FIELD_BIND                     "Bind"                 // function
#define LUA_ANIMATION_FIELD_TOGGLE                   "Toggle"               // function
#define LUA_ANIMATION_FIELD_DURATION                 "Duration"             // number
#define LUA_ANIMATION_FIELD_SPEED                    "Speed"                // number
#define LUA_ANIMATION_FIELD_COUNT                    "Count"                // number
#define LUA_ANIMATION_FIELD_LOOPS                    "Loops"                // number or boolean, undocumented (TODO: to remove? it does the same thing as animation.Count, or deprecate Count)
#define LUA_ANIMATION_FIELD_PRIORITY                 "Priority"             // integer, undocumented (TODO: to remove? animation:Toggle(groupName, boolean) is supposed to let coder have control over that, albeit more manually ; but a priority field will just lead to each successive coder incrementing the number until "it works")
#define LUA_ANIMATION_FIELD_REMOVE_WHEN_DONE         "RemoveWhenDone"       // boolean, undocumented (TODO: to remove? unclear naming, seems like a particular case that should be handled differently, or renamed/included in another field)
#define LUA_ANIMATION_FIELD_ISPLAYING                "IsPlaying"            // boolean
#define LUA_ANIMATION_FIELD_ADD_ON_PLAY_CALLBACK     "AddOnPlayCallback"    // function
#define LUA_ANIMATION_FIELD_REMOVE_ON_PLAY_CALLBACK  "RemoveOnPlayCallback" // function
#define LUA_ANIMATION_FIELD_ADD_ON_STOP_CALLBACK     "AddOnStopCallback"    // function
#define LUA_ANIMATION_FIELD_REMOVE_ON_STOP_CALLBACK  "RemoveOnStopCallback" // function

typedef struct animation_userdata {
    Animation *animation;
    bool groupsCached;
} animation_userdata;

// MARK: - static functions prototypes-

static int _g_animation_metatable_index(lua_State * const L);
static int _g_animation_metatable_newindex(lua_State * const L);
static int _g_animation_metatable_call(lua_State * const L);

static bool _animation_pushNewInstance(lua_State * const L, Animation *anim);
static int _animation_metatable_index(lua_State * const L);
static int _animation_metatable_newindex(lua_State * const L);
static void _animation_metatable_gc(void *ud);

bool _lua_animation_metatable_index(lua_State * const L, Animation *anim, const char *key);
bool _lua_animation_metatable_newindex(lua_State * const L, Animation *as, const char *key);

int _animation_play(lua_State * const L);
int _animation_playreverse(lua_State * const L);
int _animation_pause(lua_State * const L);
int _animation_tick(lua_State * const L);
int _animation_stop(lua_State * const L);
int _animation_addframeingroup(lua_State * const L);
int _animation_bind(lua_State * const L);
int _animation_toggle(lua_State * const L);

static int _animation_addOnPlayCallback(lua_State *L);
static int _animation_removeOnPlayCallback(lua_State *L);
static int _animation_addOnStopCallback(lua_State *L);
static int _animation_removeOnStopCallback(lua_State *L);

// MARK: - exposed functions -

void lua_g_animation_create_and_push(lua_State * const L) {
    vx_assert(L != nullptr);
    LUAUTILS_STACK_SIZE(L)

    lua_newuserdata(L, 1); // global "Animation" table
    lua_newtable(L); // metatable
    {
        lua_pushstring(L, "__index");
        lua_pushcfunction(L, _g_animation_metatable_index, "");
        lua_rawset(L, -3);

        lua_pushstring(L, "__newindex");
        lua_pushcfunction(L, _g_animation_metatable_newindex, "");
        lua_rawset(L, -3);

        lua_pushstring(L, "__call");
        lua_pushcfunction(L, _g_animation_metatable_call, "");
        lua_rawset(L, -3);

        // hide the metatable from the Lua sandbox user
        lua_pushstring(L, "__metatable");
        lua_pushboolean(L, false);
        lua_rawset(L, -3);

        lua_pushstring(L, "__type");
        lua_pushstring(L, "AnimationInterface");
        lua_rawset(L, -3);

        lua_pushstring(L, "__typen");
        lua_pushinteger(L, ITEM_TYPE_G_ANIMATION);
        lua_rawset(L, -3);
    }
    lua_setmetatable(L, -2);

    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
}

int lua_g_animation_snprintf(lua_State * const L, char *result, size_t maxLen, bool spacePrefix) {
    vx_assert(L != nullptr);
    return snprintf(result, maxLen, "%s[Animation] (global)", spacePrefix ? " " : "");
}

int lua_animation_snprintf(lua_State * const L, char *result, size_t maxLen, bool spacePrefix) {
    vx_assert(L != nullptr);
    return snprintf(result, maxLen, "%s[Animation]", spacePrefix ? " " : "");
}

// --------------------------------------------------
// MARK: - static functions -
// --------------------------------------------------

// always return nil
static int _g_animation_metatable_index(lua_State * const L) {
    vx_assert(L != nullptr);
    LUAUTILS_STACK_SIZE(L)
    lua_pushnil(L);
    LUAUTILS_STACK_SIZE_ASSERT(L, 1);
    return 1;
}

// do nothing, Animation global table is read-only
static int _g_animation_metatable_newindex(lua_State * const L) {
    vx_assert(L != nullptr);
    LUAUTILS_STACK_SIZE(L)
    // nothing
    LUAUTILS_STACK_SIZE_ASSERT(L, 0);
    return 0;
}

static void _lua_animation_parse_config(lua_State * const L, Animation *anim, int idx) {
    lua_getfield(L, idx, "duration");
    if (lua_isnumber(L, -1)) {
        animation_set_duration(anim, static_cast<float>(lua_tointeger(L, -1)));
    }
    lua_pop(L, 1);

    lua_getfield(L, idx, "loops");
    if (lua_isnumber(L, -1)) {
        uint8_t loops = static_cast<uint8_t>(lua_tonumber(L, -1));
        animation_set_count(anim, loops);
    } else if (lua_isboolean(L, -1)) {
        bool loops = lua_toboolean(L, -1);
        animation_set_count(anim, loops ? 0 : 1);
    }
    lua_pop(L, 1);

    lua_getfield(L, idx, "priority");
    if (lua_isnumber(L, -1)) {
        int priority = static_cast<int>(lua_tointeger(L, -1));
        if (priority < 0 || priority > 255) {
            LUAUTILS_ERROR(L, "Animation's priority must be between 0 and 255");
        }
        animation_set_priority(anim, static_cast<uint8_t>(priority));
    }
    lua_pop(L, 1);

    lua_getfield(L, idx, "removeWhenDone");
    if (lua_isboolean(L, -1)) {
        bool b = static_cast<int>(lua_toboolean(L, -1));
        animation_set_remove_when_done(anim, b);
    }
    lua_pop(L, 1);

    lua_getfield(L, idx, "speed");
    if (lua_isnumber(L, -1)) {
        animation_set_speed(anim, static_cast<float>(lua_tonumber(L, -1)));
    }
    lua_pop(L, 1);

    // NOTE: we could only keep loops
    lua_getfield(L, idx, "count");
    if (lua_isnumber(L, -1)) {
        uint8_t count = static_cast<uint8_t>(lua_tointeger(L, -1));
        animation_set_count(anim, count);
    }
    lua_pop(L, 1);

}

static int _g_animation_metatable_call(lua_State * const L) {
    vx_assert(L != nullptr);
    LUAUTILS_STACK_SIZE(L)

    const int nbArgs = lua_gettop(L);
    if (nbArgs > 3) {
        LUAUTILS_ERROR(L, "Animation: function does not accept more than 2 arguments");
    }

    // create underlying Animation object
    Animation * const anim = animation_new();
    if (_animation_pushNewInstance(L, anim) == false) {
        animation_free(anim);
        lua_pushnil(L);
    }

    // set animation name if specified
    if (nbArgs >= 2) {
        if (lua_utils_isStringStrict(L, 2) == false) {
            LUAUTILS_ERROR(L, "Animation: argument 1 should be a string");
        }
        const char *name = lua_tostring(L, 2);
        animation_set_name(anim, name);
    }

    // set animation config if specified
    if (nbArgs >= 3) {
        if (lua_istable(L, 3) == false) {
            LUAUTILS_ERROR(L, "Animation: argument 2 should be a table");
        }
        _lua_animation_parse_config(L, anim, 3);
    }

    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
    return 1;
}

Animation *lua_animation_getAnimationPtr(lua_State * const L, const int idx) {
    vx_assert(L != nullptr);
    animation_userdata *ud = static_cast<animation_userdata*>(lua_touserdata(L, idx));
    return ud->animation;
}

static bool _animation_pushNewInstance(lua_State * const L, Animation * const anim) {
    vx_assert(L != nullptr);
    vx_assert(anim != nullptr);

    LUAUTILS_STACK_SIZE(L)

    animation_userdata *ud = static_cast<animation_userdata*>(lua_newuserdatadtor(L, sizeof(animation_userdata), _animation_metatable_gc));
    ud->animation = anim;
    ud->groupsCached = false;

    // TODO: all Animation instances should share same metatable
    // but it's currently not doable as metatables are used to
    // store instance fields.
    lua_newtable(L); // metatable
    {
        lua_pushstring(L, "__index");
        lua_pushcfunction(L, _animation_metatable_index, "");
        lua_rawset(L, -3);

        lua_pushstring(L, "__newindex");
        lua_pushcfunction(L, _animation_metatable_newindex, "");
        lua_rawset(L, -3);

        // hide the metatable from the Lua sandbox user
        lua_pushstring(L, "__metatable");
        lua_pushboolean(L, false);
        lua_rawset(L, -3);


        lua_pushstring(L, "__type");
        lua_pushstring(L, "Animation");
        lua_rawset(L, -3);

        lua_pushstring(L, "__typen");
        lua_pushinteger(L, ITEM_TYPE_ANIMATION);
        lua_rawset(L, -3);
    }
    lua_setmetatable(L, -2);

    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
    return true;
}

static int _animation_metatable_index(lua_State * const L) {
    vx_assert(L != nullptr);
    LUAUTILS_ASSERT_ARGS_COUNT(L, 2)
    LUAUTILS_ASSERT_ARG_TYPE(L, 1, ITEM_TYPE_ANIMATION)
    LUAUTILS_STACK_SIZE(L)

    // retrieve underlying Animation struct
    Animation *as = lua_animation_getAnimationPtr(L, 1);
    if (as == nullptr) {
        lua_pushnil(L);
        LUAUTILS_STACK_SIZE_ASSERT(L, 1)
        return 1;
    }

    const char *key = lua_tostring(L, 2);
    if (_lua_animation_metatable_index(L, as, key) == false) {
        // key is not known and starts with an uppercase letter
        // LUAUTILS_ERROR_F(L, "Animation: %s field does not exist", key);
        lua_pushnil(L);
    }
    
    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
    return 1;
}

static int _animation_metatable_newindex(lua_State * const L) {
    vx_assert(L != nullptr);
    LUAUTILS_ASSERT_ARGS_COUNT(L, 3)
    LUAUTILS_ASSERT_ARG_TYPE(L, 1, ITEM_TYPE_ANIMATION)
    LUAUTILS_STACK_SIZE(L)

    // retrieve underlying Animation struct
    Animation * const anim = lua_animation_getAnimationPtr(L, 1);
    if (anim == nullptr) {
        lua_pushnil(L);
        LUAUTILS_STACK_SIZE_ASSERT(L, 1)
        return 1;
    }

    const char *key = lua_tostring(L, 2);
    if (_lua_animation_metatable_newindex(L, anim, key) == false) {
        // key not found
        LUAUTILS_ERROR_F(L, "Animation: %s field is not settable", key);
    }

    LUAUTILS_STACK_SIZE_ASSERT(L, 0)
    return 0;
}

static void _animation_metatable_gc(void *_ud) {
    animation_userdata *ud = static_cast<animation_userdata*>(_ud);
    Animation * const anim = ud->animation;
    if (anim != nullptr) {
        animation_free(anim);
    }
}

bool _lua_animation_metatable_index(lua_State * const L, Animation *anim, const char *key) {
    if (strcmp(key, LUA_ANIMATION_FIELD_PLAY) == 0) {
        lua_pushcfunction(L, _animation_play, "");
    } else if (strcmp(key, LUA_ANIMATION_FIELD_PLAY_REVERSE) == 0) {
        lua_pushcfunction(L, _animation_playreverse, "");
    } else if (strcmp(key, LUA_ANIMATION_FIELD_PAUSE) == 0) {
        lua_pushcfunction(L, _animation_pause, "");
    } else if (strcmp(key, LUA_ANIMATION_FIELD_STOP) == 0) {
        lua_pushcfunction(L, _animation_stop, "");
    } else if (strcmp(key, LUA_ANIMATION_FIELD_TICK) == 0) {
        lua_pushcfunction(L, _animation_tick, "");
    } else if (strcmp(key, LUA_ANIMATION_FIELD_ADDFRAMEINGROUP) == 0) {
        lua_pushcfunction(L, _animation_addframeingroup, "");
    } else if (strcmp(key, LUA_ANIMATION_FIELD_GROUPS) == 0) {
        LUAUTILS_STACK_SIZE(L)
        animation_userdata *ud = static_cast<animation_userdata*>(lua_touserdata(L, 1));
        Animation *animation = ud->animation;
        if (animation == nullptr) {
            LUAUTILS_ERROR(L, "Animation.Groups error");
        }

        bool foundCache = false;
        lua_getmetatable(L, 1);

        if (ud->groupsCached) {
            lua_pushstring(L, LUA_ANIMATION_METAFIELD_GROUPS);
            lua_rawget(L, -2);
            if (lua_istable(L, -1)) {
                foundCache = true;
            } else {
                lua_pop(L, 1);
            }
        }

        if (foundCache == false) {
            const char **names = nullptr;
            size_t len = 0;
            animation_get_keyframes_group_names(animation, &names, &len);

            lua_newtable(L);
            for (size_t i = 0; i < len; ++i) {
                lua_pushstring(L, names[i]);
                lua_rawseti(L, -2, static_cast<int>(i + 1));
            }
            free(names);

            // metatable: -2
            // groups: -1

            // cache groups
            lua_pushstring(L, LUA_ANIMATION_METAFIELD_GROUPS);
            lua_pushvalue(L, -2);
            lua_rawset(L, -4);
            ud->groupsCached = true;
        }

        lua_remove(L, -2); // remove metatable
        LUAUTILS_STACK_SIZE_ASSERT(L, 1)

    } else if (strcmp(key, LUA_ANIMATION_FIELD_BIND) == 0) {
        lua_pushcfunction(L, _animation_bind, "");
    } else if (strcmp(key, LUA_ANIMATION_FIELD_TOGGLE) == 0) {
        lua_pushcfunction(L, _animation_toggle, "");
    } else if (strcmp(key, LUA_ANIMATION_FIELD_DURATION) == 0) {
        lua_pushnumber(L, static_cast<double>(animation_get_duration(anim)));
    } else if (strcmp(key, LUA_ANIMATION_FIELD_SPEED) == 0) {
        lua_pushnumber(L, static_cast<double>(animation_get_speed(anim)));
    } else if (strcmp(key, LUA_ANIMATION_FIELD_COUNT) == 0) {
        lua_pushinteger(L, animation_get_count(anim));
    } else if (strcmp(key, LUA_ANIMATION_FIELD_LOOPS) == 0) {
        lua_pushinteger(L, animation_get_count(anim));
    } else if (strcmp(key, LUA_ANIMATION_FIELD_PRIORITY) == 0) {
        lua_pushinteger(L, animation_get_priority(anim));
    } else if (strcmp(key, LUA_ANIMATION_FIELD_REMOVE_WHEN_DONE) == 0) {
        lua_pushboolean(L, animation_get_remove_when_done(anim));
    } else if (strcmp(key, LUA_ANIMATION_FIELD_ISPLAYING) == 0) {
        lua_pushboolean(L, animation_is_playing(anim));
    } else if (strcmp(key, LUA_ANIMATION_FIELD_ADD_ON_PLAY_CALLBACK) == 0) {
        lua_pushcfunction(L, _animation_addOnPlayCallback, "");
    } else if (strcmp(key, LUA_ANIMATION_FIELD_REMOVE_ON_PLAY_CALLBACK) == 0) {
        lua_pushcfunction(L, _animation_removeOnPlayCallback, "");
    } else if (strcmp(key, LUA_ANIMATION_FIELD_ADD_ON_STOP_CALLBACK) == 0) {
        lua_pushcfunction(L, _animation_addOnStopCallback, "");
    } else if (strcmp(key, LUA_ANIMATION_FIELD_REMOVE_ON_STOP_CALLBACK) == 0) {
        lua_pushcfunction(L, _animation_removeOnStopCallback, "");
    } else {
        return false;
    }
    return true;
}

bool _lua_animation_metatable_newindex(lua_State * const L, Animation *anim, const char *key) {
    if (strcmp(key, LUA_ANIMATION_FIELD_DURATION) == 0) {
        if (lua_isnumber(L, 3)) {
            animation_set_duration(anim, static_cast<float>(lua_tonumber(L, 3)));
        } else {
            LUAUTILS_ERROR(L, "animation.Duration should be of type number");
        }
    } else if (strcmp(key, LUA_ANIMATION_FIELD_SPEED) == 0) {
        if (lua_isnumber(L, 3)) {
            animation_set_speed(anim,  static_cast<float>(lua_tonumber(L, 3)));
        } else {
            LUAUTILS_ERROR(L, "animation.Speed should be of type number");
        }
    } else if (strcmp(key, LUA_ANIMATION_FIELD_COUNT) == 0) {
        if (lua_isnumber(L, 3)) {
            animation_set_count(anim, static_cast<uint8_t>(lua_tointeger(L, 3)));
        } else {
            LUAUTILS_ERROR(L, "animation.Count should be of type number");
        }
    } else if (strcmp(key, LUA_ANIMATION_FIELD_LOOPS) == 0) {
        if (lua_isnumber(L, 3)) {
            uint8_t loops = static_cast<uint8_t>(lua_tointeger(L, 3));
            animation_set_count(anim, loops);
        } else if (lua_isboolean(L, 3)) {
            bool loops = lua_toboolean(L, 3);
            animation_set_count(anim, loops ? 0 : 1);
        } else {
            LUAUTILS_ERROR(L, "animation.Count should be of type number");
        }
    } else {
        return false;
    }
    return true;
}

int _animation_play(lua_State * const L) {
    vx_assert(L != nullptr);
    LUAUTILS_STACK_SIZE(L)

    const int nbArgs = lua_gettop(L);

    Animation *anim = lua_animation_getAnimationPtr(L, 1);
    if (anim == nullptr) {
        LUAUTILS_ERROR(L, "Animation:Play - function should be called with `:`");
    }
    
    // set animation config if specified
    if (nbArgs >= 2) {
        if (lua_istable(L, 2) == false) {
            LUAUTILS_ERROR(L, "Animation:Play - argument 1 should be a table");
        }
        _lua_animation_parse_config(L, anim, 2);
    }
    animation_play(anim);

    lua_getmetatable(L, 1);
    vx_assert(lua_istable(L, -1));

    lua_pushstring(L, LUA_ANIMATION_METAFIELD_ON_PLAY_CALLBACKS);
    lua_rawget(L, -2);

    // call on play callbacks if found
    if (lua_isnil(L, -1)) {
        lua_pop(L, 1);
    } else {
        lua_pushnil(L); // callbacks at -2, nil at -1
        while(lua_next(L, -2) != 0) {
            // callbacks at -3, key at -2, value at -1
            if (lua_isfunction(L, -1)) {
                lua_pushvalue(L, 1);
                lua_call(L, 1, 0);
                // value popped by function
            } else {
                lua_pop(L, 1); // pop value
            }
        }
        lua_pop(L, 1); // pop callbacks
    }

    lua_pop(L, 1); // pop metatable

    LUAUTILS_STACK_SIZE_ASSERT(L, 0)
    return true;
}

int _animation_playreverse(lua_State * const L) {
    vx_assert(L != nullptr);
    LUAUTILS_STACK_SIZE(L)

    Animation *anim = lua_animation_getAnimationPtr(L, 1);
    if (anim == nullptr) {
        LUAUTILS_ERROR(L, "Animation:PlayReverse - function should be called with `:`");
    }
    
    animation_play(anim);
    LUAUTILS_STACK_SIZE_ASSERT(L, 0)
    return true;
}

int _animation_pause(lua_State * const L) {
    vx_assert(L != nullptr);
    LUAUTILS_STACK_SIZE(L)

    Animation *anim = lua_animation_getAnimationPtr(L, 1);
    if (anim == nullptr) {
        LUAUTILS_ERROR(L, "Animation:Pause - function should be called with `:`");
    }
    
    animation_pause(anim);
    LUAUTILS_STACK_SIZE_ASSERT(L, 0)
    return true;
}

int _animation_tick(lua_State * const L) {
    vx_assert(L != nullptr);
    LUAUTILS_STACK_SIZE(L)

    Animation *anim = lua_animation_getAnimationPtr(L, 1);
    if (anim == nullptr) {
        LUAUTILS_ERROR(L, "Animation:Tick - function should be called with `:`");
    }
    
    if (!lua_isnumber(L, 2)) {
        LUAUTILS_ERROR(L, "Animation:Tick - parameter should be dt");
        return false;
    }
    const float dt = static_cast<float>(lua_tonumber(L, 2));

    animation_tick(anim, dt);
    LUAUTILS_STACK_SIZE_ASSERT(L, 0)
    return true;
}

int _animation_stop(lua_State * const L) {
    vx_assert(L != nullptr);
    LUAUTILS_STACK_SIZE(L)

    const int nbArgs = lua_gettop(L);

    Animation *anim = lua_animation_getAnimationPtr(L, 1);
    if (anim == nullptr) {
        LUAUTILS_ERROR(L, "Animation:Stop - function should be called with `:`");
    }
    
    bool reset = true;
    if (nbArgs >= 2) {
        if (!lua_isboolean(L, 2)) {
            LUAUTILS_ERROR(L, "Animation:Stop - first parameter is a boolean (reset, true by default)");
            return false;
        }
        reset = lua_toboolean(L, 2);
    }

    animation_stop(anim, reset);

    lua_getmetatable(L, 1);
    vx_assert(lua_istable(L, -1));

    lua_pushstring(L, LUA_ANIMATION_METAFIELD_ON_STOP_CALLBACKS);
    lua_rawget(L, -2);

    // call on stop callbacks if found
    if (lua_isnil(L, -1)) {
        lua_pop(L, 1);
    } else {
        lua_pushnil(L); // callbacks at -2, nil at -1
        while(lua_next(L, -2) != 0) {
            // callbacks at -3, key at -2, value at -1
            if (lua_isfunction(L, -1)) {
                lua_pushvalue(L, 1);
                lua_call(L, 1, 0);
                // value popped by function
            } else {
                lua_pop(L, 1); // pop value
            }
        }
        lua_pop(L, 1); // pop callbacks
    }

    lua_pop(L, 1); // pop metatable

    LUAUTILS_STACK_SIZE_ASSERT(L, 0)
    return true;
}

int _animation_addframeingroup(lua_State * const L) {
    vx_assert(L != nullptr);
    LUAUTILS_STACK_SIZE(L)

    animation_userdata *ud = static_cast<animation_userdata*>(lua_touserdata(L, 1));
    Animation *anim = ud->animation;
    if (anim == nullptr) {
        LUAUTILS_ERROR(L, "Animation:AddFrameInGroup - function should be called with `:`");
    }

    if (!lua_utils_isStringStrict(L, 2) || !lua_isnumber(L, 3) || !lua_istable(L, 4)) {
        LUAUTILS_ERROR(L, "Animation:AddFrameInGroup - parameters must be groupName, weight and { position, rotation, scale }")
    }
    
    const char *keyframeName = lua_tostring(L, 2);

    const float weight = static_cast<float>(lua_tonumber(L, 3));

    lua_getfield(L, 4, "position");
    float3 position;
    const bool positionDefined = lua_number3_or_table_getXYZ(L, -1, &position.x, &position.y, &position.z);
    lua_pop(L, 1);
    
    lua_getfield(L, 4, "rotation");
    float3 rotation;
    const bool rotationDefined = lua_number3_or_table_getXYZ(L, -1, &rotation.x, &rotation.y, &rotation.z);
    lua_pop(L, 1);

    lua_getfield(L, 4, "scale");
    float3 scale;
    const bool scaleDefined = lua_number3_or_table_getXYZ(L, -1, &scale.x, &scale.y, &scale.z);
    lua_pop(L, 1);

    Keyframes *keyframes = animation_get_keyframes(anim, keyframeName);
    if (keyframes == nullptr) {
        keyframes = keyframes_new(nullptr, keyframeName);
        animation_add(anim, keyframes);
        ud->groupsCached = false; // invalidate cache
    }
    float3 *positionPtr = positionDefined ? &position : nullptr;
    float3 *rotationPtr = rotationDefined ? &rotation : nullptr;
    float3 *scalePtr = scaleDefined ? &scale : nullptr;
    keyframes_add_new(keyframes, weight, positionPtr, rotationPtr, scalePtr, nullptr);

    LUAUTILS_STACK_SIZE_ASSERT(L, 0);
    return 0;
}

int _animation_bind(lua_State * const L) {
    vx_assert(L != nullptr);
    LUAUTILS_STACK_SIZE(L)

    Animation *anim = lua_animation_getAnimationPtr(L, 1);
    if (anim == nullptr) {
        LUAUTILS_ERROR(L, "Animation:Bind(groupName, object) - function should be called with `:`");
    }

    if (!lua_utils_isStringStrict(L, 2)) {
        LUAUTILS_ERROR(L, "Animation:Bind(groupName, object) - groupName should be a string")
    }

    if (!lua_object_isObject(L, 3)) {
        LUAUTILS_ERROR(L, "Animation:Bind(groupName, object) - object should be an Object")
    }

    const char *keyframeName = lua_tostring(L, 2);
    Keyframes *keyframes = animation_get_keyframes(anim, keyframeName);
    if (keyframes == nullptr) {
        LUAUTILS_ERROR_F(L, "Animation:Bind(groupName, object) - `%s` group not found", keyframeName);
    }

    Transform *transform;
    lua_object_getTransformPtr(L, 3, &transform);
    keyframes_set_transform(keyframes, transform);

    LUAUTILS_STACK_SIZE_ASSERT(L, 0);
    return 0;
}

int _animation_toggle(lua_State * const L) {
    vx_assert(L != nullptr);
    LUAUTILS_STACK_SIZE(L)

    Animation *anim = lua_animation_getAnimationPtr(L, 1);
    if (anim == nullptr) {
        LUAUTILS_ERROR(L, "Animation:Toggle - function should be called with `:`");
    }

    if (lua_isstring(L, 2) == false || lua_isboolean(L, 3) == false) {
        LUAUTILS_ERROR(L, "Animation:Toggle - parameters must be groupName, boolean")
    }
    
    const char *keyframeName = lua_tostring(L, 2);
    const bool enabled = lua_toboolean(L, 3);
    
    animation_toggle_keyframes(anim, keyframeName, enabled, false);
    
    LUAUTILS_STACK_SIZE_ASSERT(L, 0);
    return 0;
}

static int _animation_addOnPlayCallback(lua_State *L) {
    LUAUTILS_STACK_SIZE(L)

    const int argCount = lua_gettop(L);

    if (argCount < 1 || lua_utils_getObjectType(L, 1) != ITEM_TYPE_ANIMATION) {
        LUAUTILS_ERROR(L, "Animation:AddOnPlayCallback - function should be called with `:`");
    }

    if (argCount < 2 || lua_type(L, 2) != LUA_TFUNCTION) {
        LUAUTILS_ERROR(L, "Animation:AddOnPlayCallback - first argument should be a function");
    }

    lua_getmetatable(L, 1);
    vx_assert(lua_istable(L, -1));

    lua_pushstring(L, LUA_ANIMATION_METAFIELD_ON_PLAY_CALLBACKS);
    lua_rawget(L, -2);

    if (lua_isnil(L, -1)) {
        lua_pop(L, 1); // pop nil, metatable at -1
        lua_pushstring(L, LUA_ANIMATION_METAFIELD_ON_PLAY_CALLBACKS);
        lua_newtable(L);
        {
            lua_pushvalue(L, 2);
            lua_rawseti(L, -2, 1);
        }
        lua_rawset(L, -3);
    } else {
        lua_pushvalue(L, 2);
        lua_rawseti(L, -2, lua_objlen(L, -2) + 1);
        lua_pop(L, 1); // pop OnSet callbacks table
    }

    lua_pop(L, 1); // pop metatable

    LUAUTILS_STACK_SIZE_ASSERT(L, 0)
    return 0;
}

static int _animation_removeOnPlayCallback(lua_State *L) {
    LUAUTILS_STACK_SIZE(L)

    const int argCount = lua_gettop(L);

    if (argCount < 1 || lua_utils_getObjectType(L, 1) != ITEM_TYPE_NUMBER3) {
        LUAUTILS_ERROR(L, "Animation:RemoveOnPlayCallback - function should be called with `:`");
    }

    if (argCount < 2 || lua_type(L, 2) != LUA_TFUNCTION) {
        LUAUTILS_ERROR(L, "Animation:RemoveOnPlayCallback - first argument should be a function");
    }

    lua_getmetatable(L, 1);
    vx_assert(lua_istable(L, -1));

    if (lua_isnil(L, -1)) {
        lua_pop(L, 1); // pop nil
    } else {
        lua_pushnil(L); // callbacks at -2, nil at -1
        bool found = false;
        while(lua_next(L, -2) != 0) {
            if (found) {
                // offset following values
                lua_pushvalue(L, -1);
                // callbacks at -4, key at -3, value at -2, value at -1
                lua_rawseti(L, -4, lua_tointeger(L, -3) - 1);
            } else {
                // callbacks at -3, key at -2, value at -1
                if (lua_topointer(L, 2) == lua_topointer(L, -1)) {
                    // found callback
                    found = true;
                }
            }

            if (found) {
                lua_pushnil(L);
                // callbacks at -4, key at -3, value at -2, nil at -1
                lua_rawseti(L, -4, lua_tointeger(L, -3));
            }
            lua_pop(L, 1); // pop value
        }
        lua_pop(L, 1); // pop callbacks
    }

    lua_pop(L, 1); // pop metatable

    LUAUTILS_STACK_SIZE_ASSERT(L, 0)
    return 0;
}

static int _animation_addOnStopCallback(lua_State *L) {
    LUAUTILS_STACK_SIZE(L)

    const int argCount = lua_gettop(L);

    if (argCount < 1 || lua_utils_getObjectType(L, 1) != ITEM_TYPE_ANIMATION) {
        LUAUTILS_ERROR(L, "Animation:AddOnStopCallback - function should be called with `:`");
    }

    if (argCount < 2 || lua_type(L, 2) != LUA_TFUNCTION) {
        LUAUTILS_ERROR(L, "Animation:AddOnStopCallback - first argument should be a function");
    }

    lua_getmetatable(L, 1);
    vx_assert(lua_istable(L, -1));

    lua_pushstring(L, LUA_ANIMATION_METAFIELD_ON_STOP_CALLBACKS);
    lua_rawget(L, -2);

    if (lua_isnil(L, -1)) {
        lua_pop(L, 1); // pop nil, metatbale at -1
        lua_pushstring(L, LUA_ANIMATION_METAFIELD_ON_STOP_CALLBACKS);
        lua_newtable(L);
        {
            lua_pushvalue(L, 2);
            lua_rawseti(L, -2, 1);
        }
        lua_rawset(L, -3);
    } else {
        lua_pushvalue(L, 2);
        lua_rawseti(L, -2, lua_objlen(L, -2) + 1);
        lua_pop(L, 1); // pop OnSet callbacks table
    }

    lua_pop(L, 1); // pop metatable

    LUAUTILS_STACK_SIZE_ASSERT(L, 0)
    return 0;
}

static int _animation_removeOnStopCallback(lua_State *L) {
    LUAUTILS_STACK_SIZE(L)

    const int argCount = lua_gettop(L);

    if (argCount < 1 || lua_utils_getObjectType(L, 1) != ITEM_TYPE_NUMBER3) {
        LUAUTILS_ERROR(L, "Animation:RemoveOnStopCallback - function should be called with `:`");
    }

    if (argCount < 2 || lua_type(L, 2) != LUA_TFUNCTION) {
        LUAUTILS_ERROR(L, "Animation:RemoveOnStopCallback - first argument should be a function");
    }

    lua_getmetatable(L, 1);
    vx_assert(lua_istable(L, -1));

    lua_pushstring(L, LUA_ANIMATION_METAFIELD_ON_STOP_CALLBACKS);
    lua_rawget(L, -2);

    if (lua_isnil(L, -1)) {
        lua_pop(L, 1); // pop nil
    } else {
        lua_pushnil(L); // callbacks at -2, nil at -1
        bool found = false;
        while(lua_next(L, -2) != 0) {
            if (found) {
                // offset following values
                lua_pushvalue(L, -1);
                // callbacks at -4, key at -3, value at -2, value at -1
                lua_rawseti(L, -4, lua_tointeger(L, -3) - 1);
            } else {
                // callbacks at -3, key at -2, value at -1
                if (lua_topointer(L, 2) == lua_topointer(L, -1)) {
                    // found callback
                    found = true;
                }
            }

            if (found) {
                lua_pushnil(L);
                // callbacks at -4, key at -3, value at -2, nil at -1
                lua_rawseti(L, -4, lua_tointeger(L, -3));
            }
            lua_pop(L, 1); // pop value
        }
        lua_pop(L, 1); // pop callbacks
    }

    lua_pop(L, 1); // pop metatable

    LUAUTILS_STACK_SIZE_ASSERT(L, 0)
    return 0;
}
