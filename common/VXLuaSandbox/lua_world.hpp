
#pragma once

typedef struct lua_State lua_State;
typedef struct _CGame CGame;
typedef struct _Scene Scene;

// C++
#include <cstddef>

/// Creates global World table fields w/ given scene
bool lua_g_world_create_and_push(lua_State * const L, Scene * const sc);

/// Prints World instance
int lua_world_snprintf(lua_State * const L,
                       char *result,
                       size_t maxLen,
                       bool spacePrefix);

// MARK: - Utils -

Scene* lua_world_getSceneFromGame(lua_State * const L, CGame **g = nullptr);
