
#include "lua_projectionMode.hpp"

#include "config.h"
#include "lua_utils.hpp"
#include "sbs.hpp"
#include "vxlog.h"

#define LUA_PROJECTIONMODE_FIELD_PERSPECTIVE "Perspective"
#define LUA_PROJECTIONMODE_FIELD_ORTHOGRAPHIC "Orthographic"

// MARK: - Private functions prototypes -

bool _lua_projectionMode_createAndpushTable(lua_State *L, ProjectionMode type);
int _lua_projectionMode_g_metatable_newindex(lua_State *L);
int _lua_projectionMode_instance_metatable_index(lua_State *L);
int _lua_projectionMode_instance_metatable_newindex(lua_State *L);

// MARK: - Exposed functions -

bool lua_g_projectionMode_createAndPush(lua_State *L) {
    vx_assert(L != nullptr);
    LUAUTILS_STACK_SIZE(L)
    
    lua_newtable(L); // global ProjectionMode table
    lua_newtable(L); // metatable
    {
        lua_pushstring(L, "__index");
        lua_newtable(L);
        {
            lua_pushstring(L, LUA_PROJECTIONMODE_FIELD_PERSPECTIVE);
            _lua_projectionMode_createAndpushTable(L, Perspective);
            lua_rawset(L, -3);
            
            lua_pushstring(L, LUA_PROJECTIONMODE_FIELD_ORTHOGRAPHIC);
            _lua_projectionMode_createAndpushTable(L, Orthographic);
            lua_rawset(L, -3);
        }
        lua_rawset(L, -3);
        
        lua_pushstring(L, "__newindex");
        lua_pushcfunction(L, _lua_projectionMode_g_metatable_newindex, "");
        lua_rawset(L, -3);
        
        lua_pushstring(L, "__metatable");
        lua_pushboolean(L, false);
        lua_rawset(L, -3);
        
        lua_pushstring(L, P3S_LUA_OBJ_METAFIELD_TYPE);
        lua_pushinteger(L, ITEM_TYPE_G_PROJECTIONMODE);
        lua_rawset(L, -3);
    }
    lua_setmetatable(L, -2);

    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
    return true;
}
    
bool lua_projectionMode_pushTable(lua_State *L, ProjectionMode mode) {
    vx_assert(L != nullptr);
    LUAUTILS_STACK_SIZE(L)

    lua_getglobal(L, LUA_G_PROJECTIONMODE);

    switch(mode) {
        case Perspective:
            lua_getfield(L, -1, LUA_PROJECTIONMODE_FIELD_PERSPECTIVE);
            break;
        case Orthographic:
            lua_getfield(L, -1, LUA_PROJECTIONMODE_FIELD_ORTHOGRAPHIC);
            break;
    }

    lua_remove(L, -2); // remove global ProjectionMode
    
    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
    return true;
}

ProjectionMode lua_projectionMode_get(lua_State *L, int idx) {
    vx_assert(L != nullptr);
    LUAUTILS_STACK_SIZE(L)

    if (LUA_GET_METAFIELD_AND_RETURN_TYPE(L, idx, LUA_PROJECTIONMODE_VALUE) == LUA_TNIL) {
        LUAUTILS_INTERNAL_ERROR(L)
    }

    ProjectionMode mode = static_cast<ProjectionMode>(lua_tointeger(L, -1));
    lua_pop(L, 1);

    LUAUTILS_STACK_SIZE_ASSERT(L, 0)
    return mode;
}

int lua_projectionMode_snprintf(lua_State *L, char* result, size_t maxLen, bool spacePrefix) {
    vx_assert(L != nullptr);
    LUAUTILS_STACK_SIZE(L)

    // printing the global table
    if (LUA_GET_METAFIELD_AND_RETURN_TYPE(L, -1, LUA_PROJECTIONMODE_VALUE) == LUA_TNIL) {
        LUAUTILS_STACK_SIZE_ASSERT(L, 0)
        return snprintf(result, maxLen, "%s[ProjectionMode]", spacePrefix ? " " : "");
    }
    // else, printing one of its field

    ProjectionMode mode = static_cast<ProjectionMode>(lua_tointeger(L, -1));

    lua_pop(L, 1);
    LUAUTILS_STACK_SIZE_ASSERT(L, 0)

    switch (mode) {
        default:
        case Perspective:
            return snprintf(result, maxLen, "%s[ProjectionMode: Perspective]", spacePrefix ? " " : "");
        case Orthographic:
            return snprintf(result, maxLen, "%s[ProjectionMode: Orthographic]", spacePrefix ? " " : "");
    }
}

// MARK: - private functions -

bool _lua_projectionMode_createAndpushTable(lua_State *L, ProjectionMode mode) {
    vx_assert(L != nullptr);
    LUAUTILS_STACK_SIZE(L)
    
    lua_newtable(L); // ProjectionMode table
    lua_newtable(L); // metatable
    {
        lua_pushstring(L, "__index");
        lua_pushcfunction(L, _lua_projectionMode_instance_metatable_index, "");
        lua_rawset(L, -3);

        lua_pushstring(L, "__newindex");
        lua_pushcfunction(L, _lua_projectionMode_instance_metatable_newindex, "");
        lua_rawset(L, -3);

        lua_pushstring(L, "__metatable");
        lua_pushboolean(L, false);
        lua_rawset(L, -3);

        lua_pushstring(L, LUA_PROJECTIONMODE_VALUE);
        lua_pushinteger(L, static_cast<int>(mode));
        lua_rawset(L, -3);

        lua_pushstring(L, P3S_LUA_OBJ_METAFIELD_TYPE);
        lua_pushinteger(L, ITEM_TYPE_PROJECTIONMODE);
        lua_rawset(L, -3);
    }
    lua_setmetatable(L, -2);
    
    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
    return true;
}

int _lua_projectionMode_g_metatable_newindex(lua_State *L) {
    LUAUTILS_ERROR(L, "ProjectionMode is read only")
    return 0;
}

int _lua_projectionMode_instance_metatable_index(lua_State *L) {
    LUAUTILS_STACK_SIZE(L)
    lua_pushnil(L);
    LUAUTILS_STACK_SIZE_ASSERT(L, 1);
    return 1;
}

int _lua_projectionMode_instance_metatable_newindex(lua_State *L) {
    LUAUTILS_ERROR(L, "ProjectionMode is read only")
    return 0;
}
