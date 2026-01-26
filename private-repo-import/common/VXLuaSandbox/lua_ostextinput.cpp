//
//  lua_ostextinput.cpp
//  Blip
//
//  Created by Adrien Duermael on 10/04/2024.
//

#include "lua_ostextinput.hpp"

#include <string.h>

#include "lua_utils.hpp"
#include "lua_constants.h"
#include "config.h"

#include "textinput.hpp"

static int _g_ostextinput_index(lua_State *L);
static int _g_ostextinput_newindex(lua_State *L);

static int _g_ostextinput_request(lua_State *L);
static int _g_ostextinput_update(lua_State *L);
static int _g_ostextinput_close(lua_State *L);

#define LUA_OSTEXTINPUT_FIELD_REQUEST "Request"
#define LUA_OSTEXTINPUT_FIELD_UPDATE "Update"
#define LUA_OSTEXTINPUT_FIELD_CLOSE "Close"

bool lua_g_ostextinputs_pushNewTable(lua_State *L) {
    vx_assert(L != nullptr);
    LUAUTILS_STACK_SIZE(L)

    lua_newtable(L);
    lua_newtable(L); // metatable
    {
        lua_pushstring(L, "__index");
        lua_pushcfunction(L, _g_ostextinput_index, "");
        lua_settable(L, -3);

        lua_pushstring(L, "__newindex");
        lua_pushcfunction(L, _g_ostextinput_newindex, "");
        lua_settable(L, -3);

        lua_pushstring(L, "__metatable");
        lua_pushboolean(L, false);
        lua_settable(L, -3);

        lua_pushstring(L, P3S_LUA_OBJ_METAFIELD_TYPE); // used to log tables
        lua_pushinteger(L, ITEM_TYPE_G_OSTEXTINPUT);
        lua_settable(L, -3);
    }
    lua_setmetatable(L, -2);

    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
    return true;
}

static int _g_ostextinput_index(lua_State *L) {
    vx_assert(L != nullptr);
    LUAUTILS_ASSERT_ARGS_COUNT(L, 2)
    LUAUTILS_ASSERT_ARG_TYPE(L, 1, ITEM_TYPE_G_OSTEXTINPUT)
    LUAUTILS_STACK_SIZE(L)

    const char *key = lua_tostring(L, 2);

    if (strcmp(key, LUA_OSTEXTINPUT_FIELD_REQUEST) == 0) {
        lua_pushcfunction(L, _g_ostextinput_request, "");
    } else if (strcmp(key, LUA_OSTEXTINPUT_FIELD_UPDATE) == 0) {
        lua_pushcfunction(L, _g_ostextinput_update, "");
    } else if (strcmp(key, LUA_OSTEXTINPUT_FIELD_CLOSE) == 0) {
        lua_pushcfunction(L, _g_ostextinput_close, "");
    } else {
        lua_pushnil(L);
    }

    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
    return 1; // the __index function always has 1 return value
}

static int _g_ostextinput_newindex(lua_State *L) {
    LUAUTILS_ERROR(L, "OSTextInput is read-only")
    return 0;
}

static int _g_ostextinput_request(lua_State *L) {
    LUAUTILS_STACK_SIZE(L)

    const int argsCount = lua_gettop(L);

    if (argsCount < 1 || lua_utils_getObjectType(L, 1) != ITEM_TYPE_G_OSTEXTINPUT) {
        LUAUTILS_ERROR(L, "OSTextInput:Request(config) - function should be called with `:`")
    }

    if (argsCount > 2) {
        LUAUTILS_ERROR(L, "OSTextInput:Request(config) - only one argument is needed")
    }

    if (argsCount == 2 && lua_istable(L, 2) == false) {
        LUAUTILS_ERROR(L, "OSTextInput:Request(config) - config should be a table")
    }

    bool cursorStartSet = false;
    bool cursorEndSet = false;

    const char* content = "";
    bool multiline = false;
    size_t cursorStart = 0;
    size_t cursorEnd = 0;
    TextInputKeyboardType keyboardType = TextInputKeyboardType_Default;
    TextInputReturnKeyType returnKeyType = TextInputReturnKeyType_Default;
    bool suggestions = false;

    if (argsCount == 2) {
        lua_getfield(L, 2, "content");
        if (lua_isstring(L, -1)) {
            content = lua_tostring(L, -1);
        }
        lua_pop(L, 1);

        lua_getfield(L, 2, "multiline");
        if (lua_isboolean(L, -1)) {
            multiline = lua_toboolean(L, -1);
        }
        lua_pop(L, 1);

        lua_getfield(L, 2, "returnKeyType");
        if (lua_isstring(L, -1)) {
            const char *v = lua_tostring(L, -1);
            if (strcmp(v, "done") == 0) {
                returnKeyType = TextInputReturnKeyType_Done;
            } else if (strcmp(v, "send") == 0) {
                returnKeyType = TextInputReturnKeyType_Send;
            } else if (strcmp(v, "next") == 0) {
                returnKeyType = TextInputReturnKeyType_Next;
            }
        }
        lua_pop(L, 1);

        lua_getfield(L, 2, "keyboardType");
        if (lua_isstring(L, -1)) {
            const char *v = lua_tostring(L, -1);
            if (strcmp(v, "email") == 0) {
                keyboardType = TextInputKeyboardType_Email;
            } else if (strcmp(v, "phone") == 0) {
                keyboardType = TextInputKeyboardType_Phone;
            } else if (strcmp(v, "oneTimeDigicode") == 0) {
                keyboardType = TextInputKeyboardType_OneTimeDigicode;
            } else if (strcmp(v, "numbers") == 0) {
                keyboardType = TextInputKeyboardType_Numbers;
            } else if (strcmp(v, "url") == 0) {
                keyboardType = TextInputKeyboardType_URL;
            } else if (strcmp(v, "ascii") == 0) {
                keyboardType = TextInputKeyboardType_ASCII;
            }
        }
        lua_pop(L, 1);

        lua_getfield(L, 2, "suggestions");
        if (lua_isboolean(L, -1)) {
            suggestions = lua_toboolean(L, -1);
        }
        lua_pop(L, 1);

        lua_getfield(L, 2, "cursorStart");
        if (lua_isnumber(L, -1) || lua_utils_isIntegerStrict(L, -1)) {
            cursorStart = static_cast<size_t>(lua_tonumber(L, -1));
            cursorStartSet = true;
        }
        lua_pop(L, 1);

        lua_getfield(L, 2, "cursorEnd");
        if (lua_isnumber(L, -1) || lua_utils_isIntegerStrict(L, -1)) {
            cursorEnd = static_cast<size_t>(lua_tonumber(L, -1));
            cursorEndSet = true;
        }
        lua_pop(L, 1);
    }

    if (cursorStartSet == true && cursorEndSet == false) {
        cursorEnd = cursorStart;
    } else if (cursorStartSet == false && cursorEndSet == true) {
        cursorStart = cursorEnd;
    } else if (cursorStartSet == false && cursorEndSet == false) {
        cursorStart = strlen(content);
        cursorEnd = cursorStart;
    }

    vx::textinput::textInputRequest(std::string(content), cursorStart, cursorEnd, multiline, keyboardType, returnKeyType, suggestions);

    LUAUTILS_STACK_SIZE_ASSERT(L, 0);
    return 0;
}

static int _g_ostextinput_update(lua_State *L) {
    LUAUTILS_STACK_SIZE(L)

    const int argsCount = lua_gettop(L);

    if (argsCount < 1 || lua_utils_getObjectType(L, 1) != ITEM_TYPE_G_OSTEXTINPUT) {
        LUAUTILS_ERROR(L, "OSTextInput:Update(config) - function should be called with `:`")
    }

    if (argsCount > 2) {
        LUAUTILS_ERROR(L, "OSTextInput:Update(config) - only one argument is needed")
    }

    if (argsCount == 2 && lua_istable(L, 2) == false) {
        LUAUTILS_ERROR(L, "OSTextInput:Update(config) - config should be a table")
    }

    bool cursorStartSet = false;
    bool cursorEndSet = false;

    const char* content = "";
    size_t cursorStart = 1;
    size_t cursorEnd = 1;

    if (argsCount == 2) {
        lua_getfield(L, 2, "content");
        if (lua_isstring(L, -1)) {
            content = lua_tostring(L, -1);
        }
        lua_pop(L, 1);

        lua_getfield(L, 2, "cursorStart");
        if (lua_isnumber(L, -1) || lua_utils_isIntegerStrict(L, -1)) {
            double d = lua_tonumber(L, -1);
            if (d < 1.0) { d = 1.0; } // index can't be below 1
            cursorStart = static_cast<size_t>(d);
            cursorStartSet = true;
        }
        lua_pop(L, 1);

        lua_getfield(L, 2, "cursorEnd");
        if (lua_isnumber(L, -1) || lua_utils_isIntegerStrict(L, -1)) {
            double d = lua_tonumber(L, -1);
            if (d < 1.0) { d = 1.0; } // index can't be below 1
            cursorEnd = static_cast<size_t>(d);
            cursorEndSet = true;
        }
        lua_pop(L, 1);
    }

    if (cursorStartSet == true && cursorEndSet == false) {
        cursorEnd = cursorStart;
    } else if (cursorStartSet == false && cursorEndSet == true) {
        cursorStart = cursorEnd;
    } else if (cursorStartSet == false && cursorEndSet == false) {
        // No cursor provided.
        cursorStart = strlen(content) + 1; // Add 1 to convert index from C to Lua
        cursorEnd = cursorStart;
    }

    if (cursorEnd < cursorStart) {
        const size_t n = cursorEnd;
        cursorEnd = cursorStart;
        cursorStart = n;
    }

    // indexes start at 1 in lua, 0 in C
    vx::textinput::textInputUpdate(std::string(content), cursorStart - 1, cursorEnd - 1);

    LUAUTILS_STACK_SIZE_ASSERT(L, 0);
    return 0;
}

static int _g_ostextinput_close(lua_State *L) {
    LUAUTILS_STACK_SIZE(L)
    vx::textinput::textInputAction(TextInputAction_Close);
    LUAUTILS_STACK_SIZE_ASSERT(L, 0);
    return 0;
}
