//
//  lua_client.cpp
//  Cubzh
//
//  Created by Gaetan de Villele on 20/12/2019.
//  Copyright © 2019 Voxowl Inc. All rights reserved.
//

#include "lua_client.hpp"

// Lua
#include "lua.hpp"
#include "lua_utils.hpp"

// sandbox
#include "lua_camera.hpp"
#include "lua_constants.h"
#include "lua_inputs.hpp"
#include "lua_ostextinput.hpp"
#include "lua_nilSilentExtra.hpp"
#include "lua_player.hpp"
#include "lua_clouds.hpp"
#include "lua_fog.hpp"
#include "lua_sky.hpp"
#include "lua_screen.hpp"
#include "lua_local_event.hpp"
#include "lua_players.hpp"
#include "sbs.hpp"

// xptools
#include "xptools.h"

// vx
#include "GameCoordinator.hpp"
#include "VXPrefs.hpp"

#define LUA_CLIENT_FIELD_IS_LOGGED_IN "LoggedIn"         // boolean
#define LUA_CLIENT_FIELD_QUALITYTIER  "QualityTier"      // string (read-only)

#define LUA_CLIENT_FIELD_HAPTICFEEDBACK "HapticFeedback" // function (read-only)
#define LUA_CLIENT_FIELD_SHOWVIRTUALKEYBOARD "ShowVirtualKeyboard" // function (read-only)
#define LUA_CLIENT_FIELD_HIDEVIRTUALKEYBOARD "HideVirtualKeyboard" // function (read-only)
// VirtualKeyboardShown / VirtualKeyboardHidden LocalEvent received after requests sent

#define LUA_CLIENT_PREFERRED_LANGUAGES "PreferredLanguages" // table with ordered preferred languages

// --------------------------------------------------
//
// MARK: - Static functions' prototypes -
//
// --------------------------------------------------

static int _client_metatable_index(lua_State * const L);
static int _client_metatable_newindex(lua_State * const L);

static int _client_haptic_feedback(lua_State * const L);
static int _client_show_virtual_keyboard(lua_State * const L);
static int _client_hide_virtual_keyboard(lua_State * const L);

// --------------------------------------------------
//
// MARK: - Exposed functions -
//
// --------------------------------------------------

void lua_client_createAndPush(lua_State * const L) {
    vx_assert(L != nullptr);
    LUAUTILS_STACK_SIZE(L)

    lua_newtable(L); // global "Client" table
    lua_newtable(L); // metatable
    {
        lua_pushstring(L, "__attrs");
        lua_newtable(L);
        lua_rawset(L, -3);

        lua_pushstring(L, "__index");
        lua_pushcfunction(L, _client_metatable_index, "");
        lua_rawset(L, -3);

        lua_pushstring(L, "__newindex");
        lua_pushcfunction(L, _client_metatable_newindex, "");
        lua_rawset(L, -3);

        lua_pushstring(L, "__metatable"); // hide the metatable from the Lua sandbox user
        lua_pushboolean(L, false);
        lua_rawset(L, -3);

        lua_pushstring(L, P3S_LUA_OBJ_METAFIELD_TYPE);
        lua_pushinteger(L, ITEM_TYPE_G_CLIENT);
        lua_rawset(L, -3);
    }
    lua_setmetatable(L, -2);

    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
}

/// Pushes the "Client" global table onto the lua stack
bool lua_client_pushMetaAttrs(lua_State * const L) {
    vx_assert(L != nullptr);
    LUAUTILS_STACK_SIZE(L)

    // push Global onto the stack
    lua_utils_pushRootGlobalsFromRegistry(L);

    if (LUA_GET_METAFIELD_AND_RETURN_TYPE(L, -1, P3S_LUA_G_CLIENT) == LUA_TNIL) {
        vxlog_error("can't get Global Client metafield");
        lua_pop(L, 1); // pop Global
        LUAUTILS_STACK_SIZE_ASSERT(L, 0)
        return false;
    }
    lua_remove(L, -2); // remove Global

    if (LUA_GET_METAFIELD_AND_RETURN_TYPE(L, -1, "__attrs") == LUA_TNIL) {
        vxlog_error("Client has no metatable.__attrs");
        lua_pop(L, 1); // pop Client
        LUAUTILS_STACK_SIZE_ASSERT(L, 0)
        return false;
    }
    lua_remove(L, -2); // remove Client

    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
    return true;
}

/// Retrieves the given function and pushes it onto the Lua stack.
/// Returns whether the operation has succeeded.
/// If the operation succeeds, the function has been pushed onto the
/// Lua stack, otherwise nothing has been pushed on the stack.
bool lua_client_pushFunc(lua_State * const L, const char *function) {
    vx_assert(L != nullptr);
    LUAUTILS_STACK_SIZE(L)

    if (lua_client_pushMetaAttrs(L) == false) {
        LUAUTILS_STACK_SIZE_ASSERT(L, 0)
        return false;
    }

    // push key
    lua_pushstring(L, function);
    if (lua_rawget(L, -2) != LUA_TFUNCTION) {
        lua_pop(L, 2); // pop function name and Client metatable
        LUAUTILS_STACK_SIZE_ASSERT(L, 0)
        return false;
    }

    lua_remove(L, -2); // pop Client's meta __attrs
    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
    return true;
}

bool lua_client_pushField(lua_State * const L, const char *key) {
    vx_assert(L != nullptr);
    LUAUTILS_STACK_SIZE(L)

    lua_utils_pushRootGlobalsFromRegistry(L);

    if (LUA_GET_METAFIELD_AND_RETURN_TYPE(L, -1, P3S_LUA_G_CLIENT) == LUA_TNIL) {
        vxlog_error("can't get Global Client metafield");
        lua_pop(L, 1); // pop Global
        LUAUTILS_STACK_SIZE_ASSERT(L, 0)
        return false;
    }
    lua_remove(L, -2); // remove Global

    lua_getfield(L, -1, key);

    lua_remove(L, -2); // pop Client
    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
    return true;
}

// --------------------------------------------------
//
// MARK: - Static functions -
//
// --------------------------------------------------

/// "Client" __index metamethod
static int _client_metatable_index(lua_State * const L) {
    vx_assert(L != nullptr);
    LUAUTILS_STACK_SIZE(L)
    LUAUTILS_ASSERT_ARGS_COUNT(L, 2)
    LUAUTILS_ASSERT_ARG_TYPE(L, 1, ITEM_TYPE_G_CLIENT)

    const char *key;
    key = lua_tostring(L, 2);

    if (strcmp(key, LUA_CLIENT_FIELD_IS_LOGGED_IN) == 0) {
        const vx::Account &account = vx::Account::active();
        // current conditions to consider user logged in:

        bool loggedIn = false;

        const bool hasDOB = account.hasDOB() || account.hasEstimatedDOB();

        // const bool hasPhone = account.hasVerifiedPhoneNumber() || account.isPhoneNumberExempted();
        // if (account.under13()) {
        //     loggedIn = hasDOB && account.parentApproved();
        // } else {
        //     loggedIn = hasDOB && hasPhone;
        // }

        loggedIn = hasDOB && account.hasUsername();

        lua_pushboolean(L, loggedIn);
        LUAUTILS_STACK_SIZE_ASSERT(L, 1)
        return 1;
    } else if (strcmp(key, LUA_CLIENT_FIELD_CONNECTED) == 0) {
        vx::Game *game = nullptr;
        if (lua_utils_getGameCppWrapperPtr(L, &game) == false || game == nullptr) {
            LUAUTILS_RETURN_INTERNAL_ERROR(L);
        }
        lua_pushboolean(L, game->isConnected());
        LUAUTILS_STACK_SIZE_ASSERT(L, 1)
        return 1;
    } else if (strcmp(key, LUA_CLIENT_PREFERRED_LANGUAGES) == 0) {

        std::vector<std::string> langs = vx::device::preferredLanguages();
        lua_newtable(L);
        int i = 1;
        for (std::string lang : langs) {
            lua_pushstring(L, lang.c_str());
            lua_rawseti(L, -2, i);
            ++i;
        }
        LUAUTILS_STACK_SIZE_ASSERT(L, 1)
        return 1;
    } else if (strcmp(key, LUA_CLIENT_FIELD_QUALITYTIER) == 0) {
        const QualityTier qt = vx::GameRenderer::getGameRenderer()->getQualityTier();
        switch(qt) {
            case vx::rendering::QualityTier_Compatibility:
            case vx::rendering::QualityTier_Low:
                lua_pushstring(L, "low");
                break;
            default:
            case vx::rendering::QualityTier_Medium:
                lua_pushstring(L, "medium");
                break;
            case vx::rendering::QualityTier_High:
            case vx::rendering::QualityTier_Ultra:
                lua_pushstring(L, "high");
                break;

        }
        LUAUTILS_STACK_SIZE_ASSERT(L, 1)
        return 1;
    }

    int type = LUA_GET_METAFIELD_AND_RETURN_TYPE(L, 1, "__attrs");
    vx_assert(type == LUA_TTABLE);

    if (lua_isstring(L, 2) == true) {

        lua_pushstring(L, key);
        type = lua_rawget(L, -2);
        if (type == LUA_TNIL) {
            lua_pop(L, 1); // pop nil
        } else {
            lua_remove(L, -2); // remove attributes
            LUAUTILS_STACK_SIZE_ASSERT(L, 1)
            return 1;
        }
    } else {
        lua_pop(L, 1); // pop __attrs
        LUAUTILS_STACK_SIZE_ASSERT(L, 0)
        return 0;
    }

    // entry not found, create it if needed

    // __attrs at the top of the stack here
    if (strcmp(key, LUA_CLIENT_FIELD_INPUTS) == 0) {

        lua_pushstring(L, LUA_CLIENT_FIELD_INPUTS);
        lua_g_inputs_pushNewTable(L);
        lua_rawset(L, -3);

    } else if (strcmp(key, LUA_CLIENT_FIELD_OSTEXTINPUT) == 0) {

        lua_pushstring(L, LUA_CLIENT_FIELD_OSTEXTINPUT);
        lua_g_ostextinputs_pushNewTable(L);
        lua_rawset(L, -3);

    } else if (strcmp(key, LUA_CLIENT_FIELD_SCREEN) == 0) {

        lua_pushstring(L, LUA_CLIENT_FIELD_SCREEN);
        lua_g_screen_create_and_push(L);
        lua_rawset(L, -3);

    } else if (strcmp(key, LUA_CLIENT_FIELD_CLOUDS) == 0) {

        lua_pushstring(L, LUA_CLIENT_FIELD_CLOUDS);
        lua_g_clouds_pushNewTable(L);
        lua_rawset(L, -3);

    } else if (strcmp(key, LUA_CLIENT_FIELD_FOG) == 0) {

        lua_pushstring(L, LUA_CLIENT_FIELD_FOG);
        lua_g_fog_pushNewTable(L);
        lua_rawset(L, -3);

    } else if (strcmp(key, LUA_CLIENT_FIELD_SKY) == 0) {

        lua_pushstring(L, LUA_CLIENT_FIELD_SKY);
        lua_g_sky_pushNewTable(L);
        lua_rawset(L, -3);

    } else if (strcmp(key, LUA_CLIENT_FIELD_HAPTICFEEDBACK) == 0) {

        lua_pushstring(L, LUA_CLIENT_FIELD_HAPTICFEEDBACK);
        lua_pushcfunction(L, _client_haptic_feedback, "");
        lua_rawset(L, -3);

    } else if (strcmp(key, LUA_CLIENT_FIELD_SHOWVIRTUALKEYBOARD) == 0) {

        lua_pushstring(L, LUA_CLIENT_FIELD_SHOWVIRTUALKEYBOARD);
        lua_pushcfunction(L, _client_show_virtual_keyboard, "");
        lua_rawset(L, -3);

    } else if (strcmp(key, LUA_CLIENT_FIELD_HIDEVIRTUALKEYBOARD) == 0) {

        lua_pushstring(L, LUA_CLIENT_FIELD_HIDEVIRTUALKEYBOARD);
        lua_pushcfunction(L, _client_hide_virtual_keyboard, "");
        lua_rawset(L, -3);

    } else if (strcmp(key, LUA_CLIENT_FIELD_OS_NAME) == 0) {

        lua_pushstring(L, LUA_CLIENT_FIELD_OS_NAME);
        lua_pushstring(L, vx::device::osName().c_str());
        lua_rawset(L, -3);

    } else if (strcmp(key, LUA_CLIENT_FIELD_OS_VERSION) == 0) {

        lua_pushstring(L, LUA_CLIENT_FIELD_OS_VERSION);
        lua_pushstring(L, vx::device::osVersion().c_str());
        lua_rawset(L, -3);

    } else if (strcmp(key, LUA_CLIENT_FIELD_IS_MOBILE) == 0) {

        lua_pushstring(L, LUA_CLIENT_FIELD_IS_MOBILE);
        lua_pushboolean(L, vx::device::isMobile());
        lua_rawset(L, -3);

    } else if (strcmp(key, LUA_CLIENT_FIELD_IS_PC) == 0) {

        lua_pushstring(L, LUA_CLIENT_FIELD_IS_PC);
        lua_pushboolean(L, vx::device::isPC());
        lua_rawset(L, -3);

    } else if (strcmp(key, LUA_CLIENT_FIELD_IS_CONSOLE) == 0) {

        lua_pushstring(L, LUA_CLIENT_FIELD_IS_CONSOLE);
        lua_pushboolean(L, vx::device::isConsole());
        lua_rawset(L, -3);

    } else if (strcmp(key, LUA_CLIENT_FIELD_HAS_TOUCHSCREEN) == 0) {

        lua_pushstring(L, LUA_CLIENT_FIELD_HAS_TOUCHSCREEN);
        lua_pushboolean(L, vx::device::hasTouchScreen());
        lua_rawset(L, -3);

    } else if (strcmp(key, LUA_CLIENT_FIELD_APP_VERSION) == 0) {

        lua_pushstring(L, LUA_CLIENT_FIELD_APP_VERSION);
        lua_pushstring(L, vx::device::appVersionCached().c_str());
        lua_rawset(L, -3);

    } else if (strcmp(key, LUA_CLIENT_FIELD_BUILD_NUMBER) == 0) {

        lua_pushstring(L, LUA_CLIENT_FIELD_BUILD_NUMBER);
        lua_pushinteger(L, static_cast<int>(vx::device::appBuildNumber()));
        lua_rawset(L, -3);

    } else if (strcmp(key, LUA_CLIENT_FIELD_PLAYER) == 0) {
        vx::Game *game = nullptr;
        if (lua_utils_getGameCppWrapperPtr(L, &game) == false || game == nullptr) {
            LUAUTILS_RETURN_INTERNAL_ERROR(L);
        }

        game->addOrUpdateLocalPlayer(nullptr, nullptr, nullptr);

        lua_pushstring(L, LUA_CLIENT_FIELD_PLAYER);

        if (lua_g_players_pushPlayerTable(L, game->getLocalPlayerID()) == false) {
            LUAUTILS_INTERNAL_ERROR(L);
        }
        lua_rawset(L, -3);

    } else {
        // key not recognized
        lua_pop(L, 1); // pop __attrs
        LUAUTILS_STACK_SIZE_ASSERT(L, 0)
        return 0;
    }

    lua_pushstring(L, key);
    lua_rawget(L, -2); // pushes nil if nothing found

    lua_remove(L, -2); // remove __attrs

    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
    return 1;
}

/// "Client" __newindex metamethod (for Client Lua env)
static int _client_metatable_newindex(lua_State * const L) {
    vx_assert(L != nullptr);
    LUAUTILS_STACK_SIZE(L)
    LUAUTILS_ASSERT_ARGS_COUNT(L, 3)
    LUAUTILS_ASSERT_ARG_TYPE(L, 1, ITEM_TYPE_G_CLIENT)

    // setting non-capitalized string keys is allowed
    if (lua_utils_isStringStrictStartingWithUppercase(L, 2) == false) {
        // store the key/value pair
        lua_pushvalue(L, 2); // key
        lua_pushvalue(L, 3); // value
        lua_rawset(L, 1);
    } else {
        // key is a capitalized string
        const char *key = lua_tostring(L, 2);
        // if the key is one of the allowed function
        if (strcmp(key, LUA_CLIENT_FIELD_TICK) == 0 ||
            strcmp(key, LUA_CLIENT_FIELD_DIDRECEIVEEVENT) == 0 ||
            strcmp(key, LUA_CLIENT_FIELD_ONPLAYERJOIN) == 0 ||
            strcmp(key, LUA_CLIENT_FIELD_ONPLAYERLEAVE) == 0 ||
            strcmp(key, LUA_CLIENT_FIELD_ONWORLDOBJECTLOAD) == 0 ||
            strcmp(key, LUA_CLIENT_FIELD_ONCHAT) == 0 ||
            strcmp(key, LUA_CLIENT_FIELD_ONSTART) == 0) {

            // make sure the new value is a function or nil
            if (lua_isfunction(L, 3) == false && lua_isnil(L, 3) == false) {
                LUAUTILS_ERROR_F(L, "Client.%s can only be a function or nil", key); // returns
            }

            // sets the new Client.xx function

            if (LUA_GET_METAFIELD_AND_RETURN_TYPE(L, 1, "__attrs") != LUA_TTABLE) {
                LUAUTILS_INTERNAL_ERROR(L) // returns
            }

            const std::string listenerKey = std::string(key) + "Listener";

            // event key
            int eventID = 0;
            bool priority = false;
            if (strcmp(key, LUA_CLIENT_FIELD_TICK) == 0) {
                eventID = LOCAL_EVENT_NAME_Tick;
            } else if (strcmp(key, LUA_CLIENT_FIELD_DIDRECEIVEEVENT) == 0) {
                eventID = LOCAL_EVENT_NAME_DidReceiveEvent;
            } else if (strcmp(key, LUA_CLIENT_FIELD_ONPLAYERJOIN) == 0) {
                eventID = LOCAL_EVENT_NAME_OnPlayerJoin;
            } else if (strcmp(key, LUA_CLIENT_FIELD_ONPLAYERLEAVE) == 0) {
                eventID = LOCAL_EVENT_NAME_OnPlayerLeave;
            } else if (strcmp(key, LUA_CLIENT_FIELD_ONWORLDOBJECTLOAD) == 0) {
                eventID = LOCAL_EVENT_NAME_OnWorldObjectLoad;
            } else if (strcmp(key, LUA_CLIENT_FIELD_ONCHAT) == 0) {
                eventID = LOCAL_EVENT_NAME_OnChat;
                priority = true;
            } else if (strcmp(key, LUA_CLIENT_FIELD_ONSTART) == 0) {
                eventID = LOCAL_EVENT_NAME_OnStart;
                priority = true;
            }

            // Remove LocalEvent Listener if set
            lua_pushstring(L, listenerKey.c_str());
            lua_rawget(L, -2);
            if (lua_isnil(L, -1)) {
                lua_pop(L, 1); // pop nil
            } else {
                LOCAL_EVENT_LISTENER_CALL_REMOVE(L, -1)
            }

            lua_pushstring(L, listenerKey.c_str());
            if (lua_isnil(L, 3) == false) { // value
                if (priority) {
                    LOCAL_EVENT_LISTEN_PRIORITY(L, eventID, 3);
                } else {
                    LOCAL_EVENT_LISTEN(L, eventID, 3); // callbackIdx: 3
                }
            } else {
                lua_pushnil(L);
            }
            lua_rawset(L, -3);

            lua_pushstring(L, key);
            lua_pushvalue(L, 3);
            lua_rawset(L, -3);

            lua_pop(L, 1); // pop "__attrs"

            LUAUTILS_STACK_SIZE_ASSERT(L, 0)

        } else if (strcmp(key, LUA_CLIENT_FIELD_ANALOGPAD) == 0 ||
                   strcmp(key, LUA_CLIENT_FIELD_CONNECTING_TO_SERVER) == 0 ||
                   strcmp(key, LUA_CLIENT_FIELD_SERVER_CONNECTION_LOST) == 0 ||
                   strcmp(key, LUA_CLIENT_FIELD_SERVER_CONNECTION_FAILED) == 0 ||
                   strcmp(key, LUA_CLIENT_FIELD_SERVER_CONNECTION_SUCCESS) == 0) {

            // make sure the new value is a function or nil
            if (lua_isfunction(L, 3) == false && lua_isnil(L, 3) == false) {
                LUAUTILS_ERROR_F(L, "Client.%s can only be a function or nil", key); // returns
            }

            // sets the new Client.xx function

            if (LUA_GET_METAFIELD_AND_RETURN_TYPE(L, 1, "__attrs") != LUA_TTABLE) {
                LUAUTILS_INTERNAL_ERROR(L) // returns
            }

            lua_pushvalue(L, 2); // key
            lua_pushvalue(L, 3); // value
            lua_rawset(L, -3);

            lua_pop(L, 1); // pop "__attrs"

        } else if (strcmp(key, LUA_CLIENT_FIELD_ACTION_1) == 0 ||
                   strcmp(key, LUA_CLIENT_FIELD_ACTION_1_RELEASE) == 0 ||
                   strcmp(key, LUA_CLIENT_FIELD_ACTION_2) == 0 ||
                   strcmp(key, LUA_CLIENT_FIELD_ACTION_2_RELEASE) == 0 ||
                   strcmp(key, LUA_CLIENT_FIELD_ACTION_3) == 0 ||
                   strcmp(key, LUA_CLIENT_FIELD_ACTION_3_RELEASE) == 0 ||
                   strcmp(key, LUA_CLIENT_FIELD_DIRECTIONALPAD) == 0) {

            // make sure the new value is a function
            if (lua_isfunction(L, 3) == false && lua_isnil(L, 3) == false) {
                LUAUTILS_ERROR_F(L, "Client.%s cannot be set to a non-function value", key); // returns
            }

            CGame *g;
            if (lua_utils_getGamePtr(L, &g) == false) {
                LUAUTILS_INTERNAL_ERROR(L)
            }

            // attribution of the value for boundActions
            if (strcmp(key, LUA_CLIENT_FIELD_ACTION_1) == 0) {
                if (LUA_GET_METAFIELD_AND_RETURN_TYPE(L, 1, "__attrs") != LUA_TTABLE) {
                    LUAUTILS_INTERNAL_ERROR(L) // returns
                }
                lua_pushvalue(L, 2); // key
                lua_pushvalue(L, 3); // value
                lua_rawset(L, -3);

                lua_pop(L, 1); // pop __attrs

                LOCAL_EVENT_SEND_PUSH(L, LOCAL_EVENT_NAME_Action1Set)
                lua_pushvalue(L, 3);
                LOCAL_EVENT_SEND_CALL(L, 1)

            } else if (strcmp(key, LUA_CLIENT_FIELD_ACTION_1_RELEASE) == 0) {
                if (LUA_GET_METAFIELD_AND_RETURN_TYPE(L, 1, "__attrs") != LUA_TTABLE) {
                    LUAUTILS_INTERNAL_ERROR(L) // returns
                }
                lua_pushvalue(L, 2); // key
                lua_pushvalue(L, 3); // value
                lua_rawset(L, -3);
                lua_pop(L, 1); // pop __attrs

                LOCAL_EVENT_SEND_PUSH(L, LOCAL_EVENT_NAME_Action1ReleaseSet)
                lua_pushvalue(L, 3);
                LOCAL_EVENT_SEND_CALL(L, 1)

            } else if (strcmp(key, LUA_CLIENT_FIELD_ACTION_2) == 0) {
                if (LUA_GET_METAFIELD_AND_RETURN_TYPE(L, 1, "__attrs") != LUA_TTABLE) {
                    LUAUTILS_INTERNAL_ERROR(L) // returns
                }
                lua_pushvalue(L, 2); // key
                lua_pushvalue(L, 3); // value
                lua_rawset(L, -3);
                lua_pop(L, 1); // pop __attrs

                LOCAL_EVENT_SEND_PUSH(L, LOCAL_EVENT_NAME_Action2Set)
                lua_pushvalue(L, 3);
                LOCAL_EVENT_SEND_CALL(L, 1)

            } else if (strcmp(key, LUA_CLIENT_FIELD_ACTION_2_RELEASE) == 0) {
                if (LUA_GET_METAFIELD_AND_RETURN_TYPE(L, 1, "__attrs") != LUA_TTABLE) {
                    LUAUTILS_INTERNAL_ERROR(L) // returns
                }
                lua_pushvalue(L, 2); // key
                lua_pushvalue(L, 3); // value
                lua_rawset(L, -3);
                lua_pop(L, 1); // pop __attrs

                LOCAL_EVENT_SEND_PUSH(L, LOCAL_EVENT_NAME_Action2ReleaseSet)
                lua_pushvalue(L, 3);
                LOCAL_EVENT_SEND_CALL(L, 1)

            } else if (strcmp(key, LUA_CLIENT_FIELD_ACTION_3) == 0) {
                if (LUA_GET_METAFIELD_AND_RETURN_TYPE(L, 1, "__attrs") != LUA_TTABLE) {
                    LUAUTILS_INTERNAL_ERROR(L) // returns
                }
                lua_pushvalue(L, 2); // key
                lua_pushvalue(L, 3); // value
                lua_rawset(L, -3);
                lua_pop(L, 1); // pop __attrs

                LOCAL_EVENT_SEND_PUSH(L, LOCAL_EVENT_NAME_Action3Set) // Action3Set
                lua_pushvalue(L, 3);
                LOCAL_EVENT_SEND_CALL(L, 1)

            } else if (strcmp(key, LUA_CLIENT_FIELD_ACTION_3_RELEASE) == 0) {
                if (LUA_GET_METAFIELD_AND_RETURN_TYPE(L, 1, "__attrs") != LUA_TTABLE) {
                    LUAUTILS_INTERNAL_ERROR(L) // returns
                }
                lua_pushvalue(L, 2); // key
                lua_pushvalue(L, 3); // value
                lua_rawset(L, -3);
                lua_pop(L, 1); // pop __attrs

                LOCAL_EVENT_SEND_PUSH(L, LOCAL_EVENT_NAME_Action3ReleaseSet) // Action3ReleaseSet
                lua_pushvalue(L, 3);
                LOCAL_EVENT_SEND_CALL(L, 1)

            } else if (strcmp(key, LUA_CLIENT_FIELD_DIRECTIONALPAD) == 0) {
                if (LUA_GET_METAFIELD_AND_RETURN_TYPE(L, 1, "__attrs") != LUA_TTABLE) {
                    LUAUTILS_INTERNAL_ERROR(L) // returns
                }
                lua_pushvalue(L, 2); // key
                lua_pushvalue(L, 3); // value
                lua_rawset(L, -3);
                lua_pop(L, 1); // pop __attrs

                LOCAL_EVENT_SEND_PUSH(L, LOCAL_EVENT_NAME_DirPadSet) // DirPadSet
                lua_pushvalue(L, 3);
                LOCAL_EVENT_SEND_CALL(L, 1)
            }

        } else {
            // field is not settable
            vxlog_error("Client.%s is not settable.", key);
        }
    }

    LUAUTILS_STACK_SIZE_ASSERT(L, 0)
    return 0;
}

static int _client_haptic_feedback(lua_State* const L) {
    LUAUTILS_STACK_SIZE(L)

    const int argCount = lua_gettop(L);
    if (argCount < 1 || lua_utils_getObjectType(L, 1) != ITEM_TYPE_G_CLIENT) {
        LUAUTILS_ERROR(L, "Client:HapticFeedback() - function should be called with `:`")
        LUAUTILS_STACK_SIZE_ASSERT(L, 0);
        return 0;
    }

    if (vx::Prefs::shared().canVibrate()) {
        vx::device::hapticImpactLight();
    }

    LUAUTILS_STACK_SIZE_ASSERT(L, 0)
    return 0;
}

static int _client_show_virtual_keyboard(lua_State* const L) {
    LUAUTILS_STACK_SIZE(L)

    // TODO: get keyboard type

    vx::Game *game = nullptr;
    if (lua_utils_getGameCppWrapperPtr(L, &game) == false || game == nullptr) {
        LUAUTILS_INTERNAL_ERROR(L);
        LUAUTILS_STACK_SIZE_ASSERT(L, 0);
        return 0;
    }

    game->setActiveKeyboard(vx::KeyboardTypeDoneOnReturn);

    LUAUTILS_STACK_SIZE_ASSERT(L, 0)
    return 0;
}

static int _client_hide_virtual_keyboard(lua_State* const L) {
    LUAUTILS_STACK_SIZE(L)

    vx::Game *game = nullptr;
    if (lua_utils_getGameCppWrapperPtr(L, &game) == false || game == nullptr) {
        LUAUTILS_INTERNAL_ERROR(L);
        LUAUTILS_STACK_SIZE_ASSERT(L, 0);
        return 0;
    }

    game->setActiveKeyboard(vx::KeyboardTypeNone);

    LUAUTILS_STACK_SIZE_ASSERT(L, 0)
    return 0;
}
