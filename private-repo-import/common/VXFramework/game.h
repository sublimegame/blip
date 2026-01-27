// -------------------------------------------------------------
//  Cubzh Core
//  game.h
//  Created by Adrien Duermael on May 30, 2019.
// -------------------------------------------------------------

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "block.h"
#include "config.h"
#include "doubly_linked_list.h"
#include "doubly_linked_list_uint8.h"
#include "filo_list_uint16.h"
#include "int3.h"
#include "rigidBody.h"
#include "timer.h"
#include "ray.h"
#include "transform.h"
#include "scene.h"

#define GAME_PLAYERID_MIN 0      // minimum value of playerID
#define GAME_PLAYERID_MAX 63    // maximum value of playerID
#define GAME_PLAYER_COUNT_MAX 64 // maximum number of players in a game

/// A Game is the top level instance that stores and manages all elements.
/// It dispatches ticks, stores players, the scene, etc.
typedef struct _Camera Camera;
typedef struct _CGame CGame;
typedef struct _GameConfig GameConfig;
typedef struct _Map Map;
typedef struct _Scene Scene;
typedef struct _Shape Shape;
typedef struct _stringArray_t stringArray_t;
typedef struct _RigidBody RigidBody;
typedef struct _Transform Transform;

typedef struct {
    bool pointerVisible;
    bool forcePointerVisible;
} GameUIState;

// ---------------------------------

typedef enum {
    GameStateError_none,
    GameStateError_internal,
    GameStateError_network,
    GameStateError_unknown,
} GameStateError;

typedef enum {
    // default (client with distant server)
    GameMode_Client = 0,
    // client with local server, considering resources that can be found in local cache
    GameMode_ClientOffline = 2,
    // default server mode (getting resources from Hub)
    GameMode_Server = 3,
} GameMode;

// function pointers' types
typedef void (*pointer_object_tick)(CGame *g, Transform *self, const TICK_DELTA_SEC_T dt);
typedef void (*pointer_game_onCollision)(CGame *g,
                                         Transform *self,
                                         Transform *other,
                                         float3 wNormal,
                                         uint8_t type);
typedef bool (*pointer_game_load_script)(void *cppGamePtr, const char *script, const char *mapBase64);
typedef void (*pointer_game_close_and_free_lua_state)(CGame *g);

typedef void (*pointer_game_didReceiveEvent)(CGame *const g,
                                             const uint32_t eventType,
                                             const uint8_t senderID,
                                             const DoublyLinkedListUint8 *recipients,
                                             const uint8_t *data,
                                             const size_t size);

///
void game_free(CGame *g);

GameConfig *game_getConfig(const CGame *g);

/** Allocates a new game.
 script can be NULL, it's useful to be able to pass it here when using a local one.
 */
CGame *game_new(void *gameCppWrapper);

//
bool game_set_lua_script_string(CGame *g, const char *script);
bool game_set_map_base64_string(CGame *g, const char *mapBase64);

void game_tick(CGame *g, const TICK_DELTA_MS_T dt);
void game_tick_end(CGame *g, const TICK_DELTA_MS_T dt);

void game_toggle_object_tick(CGame *g, Transform *self, bool toggle);
void game_set_object_tick_func(CGame *g, pointer_object_tick f);

/// Returns true if recipientID is a valid player ID
/// or a special ID to represent the server, all players or other players.
bool game_isRecipientIDValid(const uint8_t recipientID);

///
bool game_isPlayerIDValid(const uint8_t playerID);

// ------------------------
//  C++ pointer bindings
// ------------------------

void *game_get_cpp_wrapper(CGame *g);

void game_setFuncPtr_ScriptingOnCollision(CGame *const g, pointer_game_onCollision funcPtr);
void game_setFuncPtr_ScriptingOnDestroy(pointer_transform_destroyed_func funcPtr);

void game_setFuncPtr_CloseAndFreeLuaState(CGame *const g,
                                          pointer_game_close_and_free_lua_state funcPtr);

const char *game_get_script(const CGame *g);
const char *game_get_mapBase64(const CGame *g);

// UI state

bool game_ui_state_get_isPointerVisible(const CGame *const g);
void game_ui_state_set_isPointerVisible(CGame *const g, const bool visible);

// Accessors

Shape* game_get_map_shape(const CGame *g);

Camera *game_get_camera(const CGame *g);
ColorAtlas *game_get_color_atlas(const CGame *g);
Scene *game_get_scene(const CGame *g);

const float3 *game_get_constant_acceleration(const CGame *g);
void game_set_constant_acceleration(CGame *g, const float *x, const float *y, const float *z);

// Sets map that's been loaded among with other resources
bool game_set_map_from_game_config(CGame *g, bool computeLight);

///
void game_add_timer(CGame * const g, Timer * const t);

FiloListUInt16 * game_get_managedTransformIDsToRemove(const CGame *g);
void game_recycle_managedTransformIDsToRemove(const CGame *g);

#ifdef __cplusplus
} // extern "C"
#endif
