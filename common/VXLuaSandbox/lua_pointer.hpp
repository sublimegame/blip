//
//  lua_pointer.hpp
//  Cubzh
//
//  Created by Xavier Legland on 7/28/20.
//

#pragma once

typedef struct lua_State lua_State;

#define LUA_POINTER_FIELD_LONGPRESS		 "LongPress"
#define LUA_POINTER_FIELD_DRAG_BEGIN     "DragBegin"
#define LUA_POINTER_FIELD_DRAG           "Drag"
#define LUA_POINTER_FIELD_DRAG_END       "DragEnd"

#define LUA_POINTER_FIELD_DRAG2_BEGIN    "Drag2Begin"
#define LUA_POINTER_FIELD_DRAG2          "Drag2"
#define LUA_POINTER_FIELD_DRAG2_END      "Drag2End"
#define LUA_POINTER_FIELD_ZOOM           "Zoom"
#define LUA_POINTER_FIELD_CANCEL         "Cancel"

/// Creates a new "Pointer" table and pushes it onto the Lua stack.
bool lua_g_pointer_pushNewTable(lua_State * const L);

///
bool lua_g_pointer_pushFunc(lua_State * const L, const char *function);
