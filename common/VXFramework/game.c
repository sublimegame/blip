// -------------------------------------------------------------
//  Cubzh Core
//  game.c
//  Created by Adrien Duermael on May 30, 2019.
// -------------------------------------------------------------

#include "game.h"

#include <float.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "cache.h"
#include "camera.h"
#include "color_atlas.h"
#include "fifo_list.h"
#include "fileutils.h"
#include "game_config.h"
#include "inputs.h"
#include "ray.h"
#include "serialization.h"
#include "timer.h"
#include "vxconfig.h"
#include "vxlog.h"
#include "xptools.h"

#ifdef __VX_PLATFORM_WINDOWS
#include "ParticubesWin.h"
#endif

typedef struct collision_callback_parameters {
    Transform *self;
    Transform *other;
    float3 wNormal;
    uint8_t type;
} collision_callback_parameters;

// static CGame *_game_new_for_item_editor(void);
static void _game_tick_processDelayedCalls(CGame *g);
static void _game_rigidbody_collision_callback(CollisionCallbackType type,
                                               Transform *self,
                                               RigidBody *selfRb,
                                               Transform *other,
                                               RigidBody *otherRb,
                                               float3 wNormal,
                                               void *callbackData);

typedef enum {
    _gameFunction_onCollision,
} _game_function_id;

// Used to store parameters, to delay function calls
struct _game_function_params {
    _game_function_id function_id;
    void *parameters;
};

typedef struct _game_function_params _game_function_params;

void _game_function_params_free(void *p) {
    if (p == NULL)
        return;
    _game_function_params *params = (_game_function_params *)p;

    switch (params->function_id) {
        case _gameFunction_onCollision: {
            collision_callback_parameters *parameters =
                (collision_callback_parameters *)(params->parameters);

            transform_release(parameters->self);
            transform_release(parameters->other);
            free(parameters);
            params->parameters = NULL;

            break;
        }
    }

    vx_assert(params->parameters == NULL);
    free(p);
}

///
struct _CGame {
    Weakptr *wptr;

    /// Game's configuration
    GameConfig *config;

    ///
    GameUIState uiState;

    /// A list of calls that should be done within tick
    DoublyLinkedList *tickCallRequests;

    /// Helpers for transforms hierarchy
    Scene *scene;

    ///
    Camera *camera;

    ///
    ColorAtlas* colorAtlas;

    ///
    void *gameCppWrapper;

    // Player actions aren't defined by default.
    pointer_game_onCollision scripting_onCollision_ptr;

    pointer_game_close_and_free_lua_state game_close_and_free_lua_state_ptr;

    DoublyLinkedList *luaTimers;
    DoublyLinkedList *luaTimersCopy;

    DoublyLinkedList *objectsTick; // transforms weakptr
    FifoList *objectsTick_Removal;
    pointer_object_tick objectTickFunc;

    // Store for destroyed transform ID managed by CGame,
    // used to give opportunity to cleanup attached metadata
    // before recycling those IDs for future use.
    FiloListUInt16 *managedTransformIDsToRemove;

    // char pad[5];
};

// MARK: - Private functions -

void _game_rigidbody_collision_callback(CollisionCallbackType type,
                                        Transform *self,
                                        RigidBody *selfRb,
                                        Transform *other,
                                        RigidBody *otherRb,
                                        float3 wNormal,
                                        void *callbackData) {
    
    if (callbackData == NULL) return;
    CGame *g = (CGame*)callbackData;

    // use a delayed call & have all callbacks executed after physics, right before lua tick
    _game_function_params *p = (_game_function_params *)malloc(sizeof(_game_function_params));
    p->function_id = _gameFunction_onCollision;

    collision_callback_parameters *params =
        (collision_callback_parameters *)malloc(sizeof(collision_callback_parameters));
    transform_retain(self);
    transform_retain(other);
    params->self = self;
    params->other = other;
    params->wNormal = wNormal;
    params->type = (uint8_t)type;
    p->parameters = params;

    doubly_linked_list_push_first(g->tickCallRequests, (void *)(p));
}

void _init_map_shape(Shape *s) {
    shape_set_local_scale(s, MAP_DEFAULT_SCALE, MAP_DEFAULT_SCALE, MAP_DEFAULT_SCALE);
    shape_set_pivot(s, 0, 0, 0);

    RigidBody *rb;
    transform_ensure_rigidbody(shape_get_transform(s), RigidbodyMode_StaticPerBlock, PHYSICS_GROUP_DEFAULT_MAP, PHYSICS_COLLIDESWITH_DEFAULT_MAP, &rb);
    rigidbody_set_mass(rb, PHYSICS_MASS_MAP);
    for (FACE_INDEX_INT_T i = 0; i < FACE_COUNT; ++i) {
        rigidbody_set_friction(rb, i, PHYSICS_FRICTION_MAP);
        rigidbody_set_bounciness(rb, i, PHYSICS_BOUNCINESS_MAP);
    }

    shape_set_shadow(s, true);
}

Shape* _new_empty_map(CGame *g) {
    Shape *s = shape_new_2(false);
    shape_set_palette(s, color_palette_new(game_get_color_atlas(g)), false);
    _init_map_shape(s);
    return s;
}

void _game_objects_tick(CGame *g, const TICK_DELTA_MS_T dt) {
    // Process objects tick removal
    DoublyLinkedListNode *n;
    Weakptr *wptr = fifo_list_pop(g->objectsTick_Removal);
    while (wptr != NULL) {
        n = doubly_linked_list_find(g->objectsTick, wptr);
        if (n != NULL) {
            doubly_linked_list_delete_node(g->objectsTick, n);
        }
        weakptr_release(wptr);

        wptr = fifo_list_pop(g->objectsTick_Removal);
    }

    // Call objects tick, their Lua code may remove objects tick, which will be processed next frame
    n = doubly_linked_list_first(g->objectsTick);
    Transform *self;
    while (n != NULL) {
        self = weakptr_get_or_release(doubly_linked_list_node_pointer(n));
        if (self == NULL) {
            n = doubly_linked_list_delete_node(g->objectsTick, n);
        } else {
            if (g->objectTickFunc != NULL) {
                g->objectTickFunc(g, self, dt);
            }
            n = doubly_linked_list_node_next(n);
        }
    }
}

void object_tick_release(void *wptr) {
     weakptr_release((Weakptr*)wptr);
}

///
void game_free(CGame *g) {
    if (g == NULL) {
        return;
    }
    weakptr_invalidate(g->wptr);

    game_config_free(g->config);
    g->config = NULL;

    doubly_linked_list_flush(g->tickCallRequests, _game_function_params_free);
    doubly_linked_list_free(g->tickCallRequests);
    g->tickCallRequests = NULL;

    scene_free(g->scene);
    camera_release(g->camera);
    color_atlas_free(g->colorAtlas);

    // free all remaining timers and the list of timers itself
    doubly_linked_list_flush(g->luaTimers, timer_free_std);
    doubly_linked_list_free(g->luaTimers);
    g->luaTimers = NULL;

    doubly_linked_list_flush(g->luaTimersCopy, timer_free_std);
    doubly_linked_list_free(g->luaTimersCopy);
    g->luaTimersCopy = NULL;

    doubly_linked_list_flush(g->objectsTick, object_tick_release);
    doubly_linked_list_free(g->objectsTick);
    fifo_list_free(g->objectsTick_Removal, object_tick_release);

    filo_list_uint16_free(g->managedTransformIDsToRemove);

    // free Game structure
    free(g);
}

GameConfig *game_getConfig(const CGame *g) {
    return g->config;
}

CGame *game_new(void *gameCppWrapper) {

    CGame *g = (CGame *)malloc(sizeof(CGame));
    if (g == NULL) {
        return NULL;
    }
    g->wptr = weakptr_new(g);

    g->config = game_config_new();

    // UI STATE
    g->uiState.pointerVisible = false;
    g->uiState.forcePointerVisible = false;

    g->tickCallRequests = doubly_linked_list_new();

    g->scene = scene_new(g->wptr);
    float3 a = (float3){0.0f, PHYSICS_GRAVITY, 0.0f};
    game_set_constant_acceleration(g, &(a.x), &(a.y), &(a.z));
    
    g->camera = camera_new();
    camera_set_order(g->camera, CAMERA_ORDER_MAIN);
    camera_set_enabled(g->camera, true);
    
    g->colorAtlas = color_atlas_new();

    // by default, Game has an empty map
    Shape *empty = _new_empty_map(g);
    scene_add_map(g->scene, empty);
    shape_release(empty); // passed ownership to g->scene

    g->gameCppWrapper = gameCppWrapper;
    g->game_close_and_free_lua_state_ptr = NULL;

    g->luaTimers = doubly_linked_list_new();
    g->luaTimersCopy = doubly_linked_list_new();
    g->objectsTick = doubly_linked_list_new();
    g->objectsTick_Removal = fifo_list_new();
    g->objectTickFunc = NULL;

    g->managedTransformIDsToRemove = filo_list_uint16_new();

    return g;
}

void _game_tick(CGame * const g, const TICK_DELTA_SEC_T dt) {

    // Physics+engine tick
    scene_refresh(g->scene, dt, g);

    // Inputs
    _game_tick_processDelayedCalls(g);
    
    // lua Timers management
    
    // make a copy of the timers collection
    // (new timers can be created & added to the list within timer callbacks)
    doubly_linked_list_copy(g->luaTimersCopy, g->luaTimers);
    
    // loop on copy and increment timers
    {
        DoublyLinkedListNode *node = doubly_linked_list_first(g->luaTimersCopy);
        while (node != NULL) {
            // get value of stored pointer
            Timer *t = (Timer *)doubly_linked_list_node_pointer(node);
            if (t != NULL) {
                timer_increment(t, dt);
            }
            // pop node
            doubly_linked_list_pop_first(g->luaTimersCopy);
            node = doubly_linked_list_first(g->luaTimersCopy);
        }
    }
    
    // loop on g->luaTimers and do necessary cleanup
    {
        DoublyLinkedListNode *node = doubly_linked_list_first(g->luaTimers);
        while (node != NULL) {
            // get value of stored pointer
            Timer *t = (Timer *)doubly_linked_list_node_pointer(node);
            if (t == NULL || timer_isFlaggedForDeletion(t)) {
                // remove node from list and delete the timer
                node = doubly_linked_list_delete_node(g->luaTimers, node);
                timer_free(t);
            }
            // move to next node
            node = doubly_linked_list_node_next(node);
        }
    }
}

void _game_tick_end(CGame * const g, const TICK_DELTA_SEC_T dt) {
    _game_objects_tick(g, dt);
}

void game_tick(CGame *g, const TICK_DELTA_MS_T dt) {
    _game_tick(g, dt);
}

// NOTE: Lua tick called between game_tick and game_tick_end

void game_tick_end(CGame *g, const TICK_DELTA_MS_T dt) {
    _game_tick_end(g, dt);
}

void game_toggle_object_tick(CGame *g, Transform *self, bool toggle) {
    if (toggle) {
        doubly_linked_list_push_last(g->objectsTick, transform_get_and_retain_weakptr(self));
    } else {
        fifo_list_push(g->objectsTick_Removal, transform_get_and_retain_weakptr(self));
    }
}

void game_set_object_tick_func(CGame *g, pointer_object_tick f) {
    g->objectTickFunc = f;
}

bool game_isRecipientIDValid(const uint8_t recipientID) {
    return (recipientID >= GAME_PLAYERID_MIN && recipientID <= GAME_PLAYERID_MAX) ||
           recipientID == PLAYER_ID_ALL || recipientID == PLAYER_ID_ALL_BUT_SELF ||
           recipientID == PLAYER_ID_SERVER;
}

///
bool game_isPlayerIDValid(const uint8_t playerID) {
    return (playerID >= GAME_PLAYERID_MIN && playerID <= GAME_PLAYERID_MAX) || playerID == PLAYER_ID_NOT_ATTRIBUTED;
}

void *game_get_cpp_wrapper(CGame *g) {
    if (g == NULL) {
        return NULL;
    }
    return g->gameCppWrapper;
}

void game_setFuncPtr_ScriptingOnCollision(CGame *const g, pointer_game_onCollision funcPtr) {
    if (g == NULL) {
        return;
    }
    g->scripting_onCollision_ptr = funcPtr;
    rigidbody_set_collision_callback(_game_rigidbody_collision_callback);
}

void game_setFuncPtr_ScriptingOnDestroy(pointer_transform_destroyed_func funcPtr) {
    transform_set_destroy_callback(funcPtr);
}

void game_setFuncPtr_CloseAndFreeLuaState(CGame *const g,
                                          pointer_game_close_and_free_lua_state funcPtr) {
    vx_assert(g != NULL);
    g->game_close_and_free_lua_state_ptr = funcPtr;
}

bool game_ui_state_get_isPointerVisible(const CGame *const g) {
    return g->uiState.pointerVisible || g->uiState.forcePointerVisible;
}

void game_ui_state_set_isPointerVisible(CGame *const g, const bool visible) {
    g->uiState.pointerVisible = visible;
}

Camera *game_get_camera(const CGame *g) {
    if (g == NULL) {
        return NULL;
    }
    return g->camera;
}

ColorAtlas *game_get_color_atlas(const CGame *g) {
    if (g == NULL) {
        return NULL;
    }
    return g->colorAtlas;
}

Scene *game_get_scene(const CGame *g) {
    if (g == NULL) {
        return NULL;
    }
    return g->scene;
}

const float3 *game_get_constant_acceleration(const CGame *g) {
    vx_assert(g != NULL);
    return scene_get_constant_acceleration(game_get_scene(g));
}

void game_set_constant_acceleration(CGame *g, const float *x, const float *y, const float *z) {
    vx_assert(g != NULL);
    scene_set_constant_acceleration(game_get_scene(g), x, y, z);
}

Shape* game_get_map_shape(const CGame *g) {
    if (g == NULL) {
        return NULL;
    }
    return transform_utils_get_shape(scene_get_map(g->scene));
}

bool game_set_map_from_game_config(CGame *g, bool computeLight) {
    RETURN_VALUE_IF_NULL(g, false);
    RETURN_VALUE_IF_NULL(g->config, false);

    GameConfig *config = game_getConfig(g);
    ColorAtlas *colorAtlas = game_get_color_atlas(g);
    Item *map = game_config_get_map(config);
    if (map == NULL) { return false; }

    Shape *m = NULL;
    
    if (map == NULL) {
        m = _new_empty_map(g);
    } else {
        ShapeSettings mapSettings;
        mapSettings.lighting = true;
        mapSettings.isMutable = true;

        // check if file is in cache
        if (cache_isFileInCache(map->fullname, NULL)) {
            m = serialization_load_shape_from_cache(map->fullname,
                                                    map->fullname,
                                                    colorAtlas,
                                                    &mapSettings);
        } else {
            // file not found in cache, checking in bundle...
            char *sanitized_path = cache_ensure_path_format(map->fullname, false);
            char *bundleRelPath =
            string_new_join("cache", c_getPathSeparatorCStr(), sanitized_path, NULL);

            free(sanitized_path);
            sanitized_path = NULL;

            m = serialization_load_shape_from_assets(bundleRelPath,
                                                     map->fullname,
                                                     colorAtlas,
                                                     &mapSettings);
            free(bundleRelPath);
            bundleRelPath = NULL;
        }
    }
    
    if (m == NULL) {
        vxlog_error("map is null, cannot set map");
        return false;
    }
    
    _init_map_shape(m);

    scene_add_map(g->scene, m);
    shape_release(m); // passed ownership to g->scene

    // compute light
    if (computeLight) {
        // if baked lighting wasn't serialized w/ the shape, load/create baked file
        if (shape_uses_baked_lighting(m) == false) {
#if GLOBAL_LIGHTING_BAKE_LOAD_ENABLED
            if (cache_load_or_clear_baked_file(m) == false) {
                shape_compute_baked_lighting(m);

#if GLOBAL_LIGHTING_BAKE_SAVE_ENABLED
                cache_save_baked_file(m);
#endif
            }
#else
            shape_compute_baked_lighting(m);
#endif
            shape_refresh_vertices(m);
        }
    }

    return true;
}

void game_add_timer(CGame * const g, Timer * const t) {
    vx_assert(g != NULL);
    vx_assert(t != NULL);
    doubly_linked_list_push_last(g->luaTimers, (void *)t);
}

FiloListUInt16 * game_get_managedTransformIDsToRemove(const CGame *g) {
    return g->managedTransformIDsToRemove;
}

void game_recycle_managedTransformIDsToRemove(const CGame *g) {
    uint16_t id;
    FiloListUInt16 *l = g->managedTransformIDsToRemove;
    while (filo_list_uint16_pop(l, &id)) {
        transform_recycle_id(id);
    }
}

static void _game_tick_processDelayedCalls(CGame *g) {
    // delayed calls
    void *req = doubly_linked_list_pop_first(g->tickCallRequests);

    while (req != NULL) {
        _game_function_params *params = (_game_function_params *)(req);

        bool done = false;

        switch (params->function_id) {
            case _gameFunction_onCollision: {
                collision_callback_parameters *p =
                    (collision_callback_parameters *)(params->parameters);
                if (g->scripting_onCollision_ptr != NULL) {
                    g->scripting_onCollision_ptr(g, p->self, p->other, p->wNormal, p->type);
                }
                done = true;

                break;
            }
        }

        _game_function_params_free(params);

        if (done == false) {
            vxlog_error("couldn't call game function");
            break;
        }

        req = doubly_linked_list_pop_first(g->tickCallRequests);
    }
}
