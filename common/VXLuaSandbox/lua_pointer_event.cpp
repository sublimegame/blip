//
//  lua_pointer_event.cpp
//  Cubzh
//
//  Created by Xavier Legland on 7/29/20.
//

#include "lua_pointer_event.hpp"

// Lua sandbox
#include "lua.hpp"
#include "lua_constants.h"
#include "lua_impact.hpp"
#include "lua_utils.hpp"
#include "lua_shape.hpp"
#include "lua_number3.hpp"
#include "lua_ray.hpp"

// xptools
#include "xptools.h"

// engine
#include "camera.h"
#include "game.h"

// Functions
#define LUA_POINTEREVENT_FIELD_CASTRAY "CastRay" // function (read-only)

// Properties
#define LUA_POINTEREVENT_FIELD_X "X"
#define LUA_POINTEREVENT_FIELD_Y "Y"
#define LUA_POINTEREVENT_FIELD_DX "DX"
#define LUA_POINTEREVENT_FIELD_DY "DY"
#define LUA_POINTEREVENT_FIELD_POSITION "Position"
#define LUA_POINTEREVENT_FIELD_DIRECTION "Direction"
#define LUA_POINTEREVENT_FIELD_DOWN "Down"
#define LUA_POINTEREVENT_FIELD_INDEX "Index"

static int _g_pointer_event_index(lua_State * const L);
static int _g_pointer_event_newindex(lua_State * const L);
static int _g_pointer_event_call(lua_State *L);

static int _pointer_event_newindex(lua_State *L);
static int _pointer_event_castRay(lua_State *L);

bool lua_g_pointer_event_create_and_push(lua_State *L) {
    vx_assert(L != nullptr);
    
    LUAUTILS_STACK_SIZE(L)
    
    lua_newtable(L);
    lua_newtable(L);
    {
        lua_pushstring(L, "__index");
        lua_pushcfunction(L, _g_pointer_event_index, "");
        lua_rawset(L, -3);
        
        lua_pushstring(L, "__newindex");
        lua_pushcfunction(L, _g_pointer_event_newindex, "");
        lua_rawset(L, -3);

        lua_pushstring(L, "__call");
        lua_pushcfunction(L, _g_pointer_event_call, "");
        lua_rawset(L, -3);

        lua_pushstring(L, "__metatable");
        lua_pushboolean(L, false);
        lua_rawset(L, -3);

        lua_pushstring(L, P3S_LUA_OBJ_METAFIELD_TYPE);
        lua_pushinteger(L, ITEM_TYPE_G_POINTEREVENT);
        lua_rawset(L, -3);
    }
    lua_setmetatable(L, -2);

    LUAUTILS_STACK_SIZE_ASSERT(L, 1);
    return true;
}


void lua_pointer_event_create_and_push_table(lua_State *L,
                                             const float x, const float y,
                                             const float dx, const float dy,
                                             const float3 * const position,
                                             const float3 * const direction,
                                             const bool down,
                                             const uint8_t index) {
    vx_assert(L != nullptr);
    LUAUTILS_STACK_SIZE(L)
    
    lua_newtable(L); // PointerEvent instance
    lua_newtable(L); // PointerEvent instance metatable
    {
        lua_pushstring(L, "__index");
        lua_newtable(L); // index table
        {
            lua_pushstring(L, LUA_POINTEREVENT_FIELD_X);
            lua_pushnumber(L, static_cast<lua_Number>(x));
            lua_rawset(L, -3);
            
            lua_pushstring(L, LUA_POINTEREVENT_FIELD_Y);
            lua_pushnumber(L, static_cast<lua_Number>(y));
            lua_rawset(L, -3);
            
            lua_pushstring(L, LUA_POINTEREVENT_FIELD_DX);
            lua_pushnumber(L, static_cast<lua_Number>(dx));
            lua_rawset(L, -3);
            
            lua_pushstring(L, LUA_POINTEREVENT_FIELD_DY);
            lua_pushnumber(L, static_cast<lua_Number>(dy));
            lua_rawset(L, -3);
            
            lua_pushstring(L, LUA_POINTEREVENT_FIELD_POSITION);
            lua_number3_pushNewObject(L, position->x, position->y, position->z);
            lua_rawset(L, -3);
            
            lua_pushstring(L, LUA_POINTEREVENT_FIELD_DIRECTION);
            lua_number3_pushNewObject(L, direction->x, direction->y, direction->z);
            lua_rawset(L, -3);
            
            lua_pushstring(L, LUA_POINTEREVENT_FIELD_DOWN);
            lua_pushboolean(L, down);
            lua_rawset(L, -3);

            lua_pushstring(L, LUA_POINTEREVENT_FIELD_INDEX);
            lua_pushinteger(L, index);
            lua_rawset(L, -3);
            
            lua_pushstring(L, LUA_POINTEREVENT_FIELD_CASTRAY);
            lua_pushcfunction(L, _pointer_event_castRay, "");
            lua_rawset(L, -3);
        }
        lua_rawset(L, -3);
        
        lua_pushstring(L, "__newindex");
        lua_pushcfunction(L, _pointer_event_newindex, "");
        lua_rawset(L, -3);
        
        lua_pushstring(L, "__metatable");
        lua_pushboolean(L, false);
        lua_rawset(L, -3);

        lua_pushstring(L, P3S_LUA_OBJ_METAFIELD_TYPE);
        lua_pushinteger(L, ITEM_TYPE_POINTEREVENT);
        lua_rawset(L, -3);
    }
    lua_setmetatable(L, -2);
    
    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
}

///
static int _pointer_event_newindex(lua_State *L) {
    LUAUTILS_ERROR(L, "PointerEvent is read-only");
    return 0;
}

///
static int _pointer_event_castRay(lua_State *L) {
    vx_assert(L != nullptr);
    LUAUTILS_STACK_SIZE(L)
    
    const int argCount = lua_gettop(L);
    
    if (argCount < 1 || lua_utils_getObjectType(L, 1) != ITEM_TYPE_POINTEREVENT) {
        LUAUTILS_ERROR(L, "PointerEvent:CastRay - function should be called with `:`");
    }
    
    if (argCount > 3) {
        LUAUTILS_ERROR(L, "PointerEvent:CastRay - incorrect argument count"); // returns
    }
    
    if (lua_getfield(L, 1, LUA_POINTEREVENT_FIELD_POSITION) == LUA_TNIL) {
        // a pointer event should always have a position
        LUAUTILS_INTERNAL_ERROR(L)
    }
    // Position at -1
    
    if (lua_getfield(L, 1, LUA_POINTEREVENT_FIELD_DIRECTION) == LUA_TNIL) {
        // a pointer event should always have a direction
        LUAUTILS_INTERNAL_ERROR(L)
    }
    // Position at -2
    // Direction at -1
    
    lua_ray_create_and_push(L, -2, -1);
    lua_remove(L, -2); // remove Direction
    lua_remove(L, -2); // remove Position
    // Ray at -1
    
    lua_ray_push_cast_function(L); // Ray at -2, function at -1
    lua_pushvalue(L, -2); // Ray at -1, function at -2, Ray at -3
    lua_remove(L, -3); // Ray at -1, function at -2
    if (argCount >= 2) lua_pushvalue(L, 2); // filterIn
    if (argCount == 3) lua_pushvalue(L, 3); // filterOut
    lua_call(L, argCount, 1); // pops all arguments + function (one value return, can be nil)
    
    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
    return 1;
}

static int _g_pointer_event_index(lua_State *L) {
    LUAUTILS_STACK_SIZE(L)
    lua_pushnil(L);
    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
    return 1;
}

static int _g_pointer_event_newindex(lua_State *L) {
    LUAUTILS_STACK_SIZE(L)
    LUAUTILS_STACK_SIZE_ASSERT(L, 0)
    return 0;
}

// global Timer(x,y,dx,dy,down,pointerIndex)
// x: from 0 to 1 (screen space)
// y: from 0 to 1 (screen space)
// dx: in points
// dy: in points
// down: boolean
// pointerIndex: integer
static int _g_pointer_event_call(lua_State *L) {
    vx_assert(L != nullptr);
    LUAUTILS_STACK_SIZE(L)
    
    if (lua_gettop(L) != 7) {
        LUAUTILS_ERROR(L, "PointerEvent(x,y,dx,dy,down,pointerIndex) expects 6 arguments");
    }
    
    if (lua_isnumber(L, 2) == false) {
        LUAUTILS_ERROR(L, "PointerEvent(x,y,dx,dy,down,pointerIndex) - x should be a number");
    }
    
    if (lua_isnumber(L, 3) == false) {
        LUAUTILS_ERROR(L, "PointerEvent(x,y,dx,dy,down,pointerIndex) - y should be a number");
    }
    
    if (lua_isnumber(L, 4) == false) {
        LUAUTILS_ERROR(L, "PointerEvent(x,y,dx,dy,down,pointerIndex) - dx should be a number");
    }
    
    if (lua_isnumber(L, 5) == false) {
        LUAUTILS_ERROR(L, "PointerEvent(x,y,dx,dy,down,pointerIndex) - dy should be a number");
    }
    
    if (lua_isboolean(L, 6) == false) {
        LUAUTILS_ERROR(L, "PointerEvent(x,y,dx,dy,down,pointerIndex) - down should be a boolean");
    }
    
    if (lua_utils_isIntegerStrict(L, 7) == false) {
        LUAUTILS_ERROR(L, "PointerEvent(x,y,dx,dy,down,pointerIndex) - pointerIndex should be an integer");
    }
    
    CGame *g = nullptr;
    if (lua_utils_getGamePtr(L, &g) == false) { LUAUTILS_INTERNAL_ERROR(L); }
    if (g == nullptr) { LUAUTILS_INTERNAL_ERROR(L); }
    
    const float relX = lua_tonumber(L, 2);
    const float relY = lua_tonumber(L, 3);
    
    const float dx = lua_tonumber(L, 4);
    const float dy = lua_tonumber(L, 5);
    
    const bool down = lua_toboolean(L, 6);
    const uint8_t pointerID = static_cast<uint8_t>(lua_tointeger(L, 7));

    float3 o, v; camera_unorm_screen_to_ray(game_get_camera(g), relX, relY, &o, &v);
    lua_pointer_event_create_and_push_table(L, relX, relY, dx, dy, &o, &v, down, pointerID);

    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
    return 1;
}
