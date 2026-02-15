//
//  lua_ai.cpp
//  Cubzh
//
//  Created by Corentin Cailleaud on 24/05/2023.
//

#include "lua_ai.hpp"

#include "config.h"
#include "lua_utils.hpp"
#include "lua_constants.h"
#include "VXGame.hpp"

#define LUA_G_AI_FIELD_CREATE_CHAT "CreateChat"
#define LUA_G_AI_FIELD_CREATE_IMAGE "CreateImage"

static int _g_ai_metatable_index(lua_State *L);
static int _g_ai_metatable_newindex(lua_State *L);

void lua_g_ai_create_and_push(lua_State * const L) {
    vx_assert(L != nullptr);
    LUAUTILS_STACK_SIZE(L)

    lua_newtable(L); // global "AI" table
    lua_newtable(L); // metatable
    {
        lua_pushstring(L, "__index");
        lua_pushcfunction(L, _g_ai_metatable_index, "");
        lua_rawset(L, -3);

        lua_pushstring(L, "__newindex");
        lua_pushcfunction(L, _g_ai_metatable_newindex, "");
        lua_rawset(L, -3);

        // hide the metatable from the Lua sandbox user
        lua_pushstring(L, "__metatable");
        lua_pushboolean(L, false);
        lua_rawset(L, -3);
    }
    lua_setmetatable(L, -2);

    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
}

static int _g_ai_metatable_index(lua_State *L) {
    vx_assert(L != nullptr);
    LUAUTILS_STACK_SIZE(L)

    vx::Game *cppGame = nullptr;
    if (lua_utils_getGameCppWrapperPtr(L, &cppGame) == false || cppGame == nullptr) {
        LUAUTILS_INTERNAL_ERROR(L)
    }
    vx_assert(cppGame != nullptr);
    
    vx::GameRendererState_SharedPtr grs = cppGame->getGameRendererState();

    const char *key = lua_tostring(L, 2);
    if (strcmp(key, LUA_G_AI_FIELD_CREATE_CHAT) == 0) {
        if (luau_dostring(L, R"""(
return function(self, context)
    return require("ai"):CreateChat(context)
end
        )""", "AI.CreateChat") == false) {
            lua_pushnil(L);
        }
    } else if (strcmp(key, LUA_G_AI_FIELD_CREATE_IMAGE) == 0) {
        if(luau_dostring(L, R"""(
return function(self, prompt, optionsOrCallback, callback)
    return require("ai"):CreateImage(prompt, optionsOrCallback, callback)
end
        )""", "AI.CreateImage")) {
            lua_pushnil(L);
        }

    } else {
        lua_pushnil(L);
    }

    LUAUTILS_STACK_SIZE_ASSERT(L, 1);
    return 1;
}

static int _g_ai_metatable_newindex(lua_State *L) {
    LUAUTILS_STACK_SIZE(L)
    LUAUTILS_ERROR(L, "AI is read-only"); // returns
    LUAUTILS_STACK_SIZE_ASSERT(L, 0)
    return 0;
}
