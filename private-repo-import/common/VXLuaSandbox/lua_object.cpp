// -------------------------------------------------------------
//  Cubzh
//  lua_object.cpp
//  Created by Arthur Cormerais on March 12, 2021.
// -------------------------------------------------------------

// LIFECYCLE:
/*
 - TRANSFORM CREATED
 - TRANSFORM REFERENCED IN LUA (OBJECT)
     - ADDED WEAK REFERENCE IN REGISTRY TO ALWAYS RETURN SAME OBJECT FOR SAME TRANSFORM
     - IF METATADA IS ADDED : REFERENCED BY TRANSFORM ID IN OTHER DEDICATED REGISTRY
         - WHEN THAT HAPPENS -> transform_set_has_external_metadata(t)
 - OBJECT LOSES ALL REFERENCES
     - TRANSFORM COULD STILL BE PARENTED AND THUS RETAINED AT C LEVEL
 - TRANSFORM IS FREED
     - IT MEANS THERE'S NO LUA REFERENCE (otherwise couldn't be freed)
     - IF NO EXTERNAL METADATA: ID RECYCLED RIGHT AWAY
     - ELSE: ID STORED IN "REMOVED TRANSFORMS WITH EXTERNAL METADATA" POOL
     - METADATA CLEANED UP DURING NEXT CYCLE (AND RETAINED TRANSFORM IDS RECYCLED)
 - AT THE END OF THE GAME: RELEASE ALL TRANSFORMS STILL REFERENCED BY LUA SANDBOX (userdata store)
*/

#include "lua_object.hpp"

#include <float.h>

// Lua
#include "lua.hpp"
#include "lua_audioListener.hpp"
#include "lua_audioSource.hpp"
#include "lua_block.hpp"
#include "lua_number3.hpp"
#include "lua_pointer_event.hpp"
#include "lua_items.hpp"
#include "lua_constants.h"
#include "lua_utils.hpp"
#include "lua_shape.hpp"
#include "lua_camera.hpp"
#include "lua_player.hpp"
#include "lua_collision_groups.hpp"
#include "lua_logs.hpp"
#include "lua_mutableShape.hpp"
#include "lua_box.hpp"
#include "lua_color.hpp"
#include "lua_face.hpp"
#include "lua_light.hpp"
#include "lua_text.hpp"
#include "lua_physicsMode.hpp"
#include "lua_quad.hpp"
#include "lua_rotation.hpp"
#include "lua_audioSource.hpp"
#include "lua_data.hpp"
#include "lua_mesh.hpp"

// engine
#include "cache.h"
#include "game.h"
#include "serialization.h"
#include "shape.h"
#include "rigidBody.h"
#include "scene.h"

// vx
#include "xptools.h"
#include "VXGameRenderer.hpp"
#include "VXFont.hpp"
#include "VXConfig.hpp"

// Object instance fields

// Functions
#define LUA_OBJECT_FIELD_ADDCHILD               "AddChild"
#define LUA_OBJECT_FIELD_APPLYFORCE             "ApplyForce"
#define LUA_OBJECT_FIELD_COLLIDESWITH           "CollidesWith"
#define LUA_OBJECT_FIELD_GETCHILD               "GetChild"
#define LUA_OBJECT_FIELD_GETPARENT              "GetParent"
#define LUA_OBJECT_FIELD_POSITIONLTW            "PositionLocalToWorld"
#define LUA_OBJECT_FIELD_POSITIONWTL            "PositionWorldToLocal"
#define LUA_OBJECT_FIELD_REMOVECHILD            "RemoveChild"
#define LUA_OBJECT_FIELD_REMOVECHILDREN         "RemoveChildren"
#define LUA_OBJECT_FIELD_REMOVEFROMPARENT       "RemoveFromParent"
#define LUA_OBJECT_FIELD_ROTATELOCAL            "RotateLocal"
#define LUA_OBJECT_FIELD_ROTATEWORLD            "RotateWorld"
#define LUA_OBJECT_FIELD_ROTATIONLTW            "RotationLocalToWorld"
#define LUA_OBJECT_FIELD_ROTATIONWTL            "RotationWorldToLocal"
#define LUA_OBJECT_FIELD_SETPARENT              "SetParent"
#define LUA_OBJECT_FIELD_RESETCOLLIDER          "ResetCollisionBox"
#define LUA_OBJECT_FIELD_FIND_FIRST             "FindFirst"
#define LUA_OBJECT_FIELD_FIND                   "Find"
#define LUA_OBJECT_FIELD_RECURSE                "Recurse"
#define LUA_OBJECT_FIELD_COPY                   "Copy"

// releases and sets transform in userdata to NULL
#define LUA_OBJECT_FIELD_DESTROY "Destroy"
#define LUA_OBJECT_FIELD_IS_DESTROYED "IsDestroyed"
#define LUA_OBJECT_FIELD_ID "ID"

// Properties
#define LUA_OBJECT_FIELD_ACCELERATION           "Acceleration"         // Number3 object
#define LUA_OBJECT_FIELD_BACKWARD               "Backward"             // Number3 object
#define LUA_OBJECT_FIELD_BOUNCINESS             "Bounciness"           // number
#define LUA_OBJECT_FIELD_CHILDREN               "Children"             // array (Object)
#define LUA_OBJECT_FIELD_CHILDRENCOUNT          "ChildrenCount"        // integer (read-only)
#define LUA_OBJECT_FIELD_COLLIDESWITHGROUPS     "CollidesWithGroups"   // array (collision groups)
#define LUA_OBJECT_FIELD_COLLISIONBOX           "CollisionBox"         // Box
#define LUA_OBJECT_FIELD_COLLISIONGROUPS        "CollisionGroups"      // array (collision groups)
#define LUA_OBJECT_FIELD_DOWN                   "Down"                 // Number3 object
#define LUA_OBJECT_FIELD_FORWARD                "Forward"              // Number3 object
#define LUA_OBJECT_FIELD_FRICTION               "Friction"             // number
#define LUA_OBJECT_FIELD_ISHIDDEN               "IsHidden"             // boolean
#define LUA_OBJECT_FIELD_ISHIDDENSELF           "IsHiddenSelf"         // boolean
#define LUA_OBJECT_FIELD_ISONGROUND             "IsOnGround"           // boolean (read-only)
#define LUA_OBJECT_FIELD_LEFT                   "Left"                 // Number3 object
#define LUA_OBJECT_FIELD_LOCALPOSITION          "LocalPosition"        // Number3 object
#define LUA_OBJECT_FIELD_LOCALROTATION          "LocalRotation"        // Number3 object
#define LUA_OBJECT_FIELD_LOCALSCALE             "LocalScale"           // number3 object
#define LUA_OBJECT_FIELD_LOSSYSCALE             "LossyScale"           // number3 object (read-only)
#define LUA_OBJECT_FIELD_MASS                   "Mass"                 // number
#define LUA_OBJECT_FIELD_MOTION                 "Motion"               // Number3 object
#define LUA_OBJECT_FIELD_PARENT                 "Parent"               // table containing a weak ref to the Parent Object
#define LUA_OBJECT_FIELD_PHYSICS                "Physics"              // boolean
#define LUA_OBJECT_FIELD_POSITION               "Position"             // Number3 object
#define LUA_OBJECT_FIELD_RIGHT                  "Right"                // Number3 object
#define LUA_OBJECT_FIELD_ROTATION               "Rotation"             // Number3 object
#define LUA_OBJECT_FIELD_UP                     "Up"                   // Number3 object
#define LUA_OBJECT_FIELD_VELOCITY               "Velocity"             // Number3 object
#define LUA_OBJECT_FIELD_TICK                   "Tick"                 // function
#define LUA_OBJECT_FIELD_NAME                   "Name"                 // string
#define LUA_OBJECT_FIELD_SHADOWCOOKIE           "ShadowCookie"         // number
#define LUA_OBJECT_FIELD_ANIMATIONS             "Animations"

// Not documented
#define LUA_OBJECT_FIELD_COLLISIONGROUPSMASK    "CollisionGroupsMask"  // [deprecated]
#define LUA_OBJECT_FIELD_COLLIDESWITHMASK       "CollidesWithMask"     // [deprecated]
#define LUA_OBJECT_FIELD_SCALE                  "Scale"                // [deprecated] replaced by LocalScale
#define LUA_OBJECT_FIELD_AWAKE                  "Awake"
#define LUA_OBJECT_FIELD_REFRESH                "Refresh"
#define LUA_OBJECT_FIELD_TEXTBUBBLE             "TextBubble"           // [deprecated]
#define LUA_OBJECT_FIELD_CLEARTEXTBUBBLE        "ClearTextBubble"      // [deprecated]
#define LUA_OBJECT_FIELD_CONTACTRIGHT           "PrivateContactRight"
#define LUA_OBJECT_FIELD_CONTACTLEFT            "PrivateContactLeft"
#define LUA_OBJECT_FIELD_CONTACTUP              "PrivateContactUp"
#define LUA_OBJECT_FIELD_CONTACTDOWN            "PrivateContactDown"
#define LUA_OBJECT_FIELD_CONTACTFORWARD         "PrivateContactForward"
#define LUA_OBJECT_FIELD_CONTACTBACKWARD        "PrivateContactBackward"

#define LUA_OBJECT_DEBUG "Debug"

#define LUA_G_OBJECT_REGISTRY_INDEX "__g_object"

#define REGISTRY_OBJECTS_INDEXED_BY_TRANSFORM_ID "__obj_tid"
#define REGISTRY_OBJECTS_STORAGE "__obj_storage"

#define LUA_G_OBJECT_FIELD_LOAD "Load"

#define LUA_OBJECT_FIELD_READ_ONLY "__ro"

typedef struct object_userdata {
    // Userdata instance requirements
    vx::UserDataStore *store;
    object_userdata *next;
    object_userdata *previous;

    Transform *transform;
} object_userdata;

// MARK: Metatable

static int _g_object_metatable_index(lua_State *L);
static int _g_object_metatable_newindex(lua_State *L);
static int _g_object_metatable_call(lua_State *L);
static int _object_metatable_index(lua_State *L);
static int _object_metatable_newindex(lua_State *L);

static void _object_release_transform(void *_ud);
static void _object_gc(void *_ud);

// MARK: Objects storage

typedef void (*pointer_transform_get_float3_value_func)(Transform *t, float3 *out);
static void _object_storage_get_or_create_number3(lua_State *L, const char* name,
                                                  number3_C_handler_set set, number3_C_handler_get get,
                                                  pointer_transform_get_float3_value_func initValue);
static void _object_storage_set_or_create_number3(lua_State *L, const char* name, const int idx,
                                                  number3_C_handler_set set, number3_C_handler_get get,
                                                  pointer_transform_get_float3_value_func initValue);
typedef Quaternion* (*pointer_transform_get_quaternion_func)(Transform *t);
static void _object_storage_get_or_create_rotation(lua_State *L, const char* name,
                                                   rotation_handler_set set, rotation_handler_get get,
                                                   pointer_transform_get_quaternion_func init);
static void _object_storage_set_or_create_rotation(lua_State *L, const char* name, const int idx,
                                                   rotation_handler_set set, rotation_handler_get get,
                                                   pointer_transform_get_quaternion_func init);

// MARK: Number per face table

static const char* number_per_face_keys[FACE_COUNT] = { "right", "left", "front", "back", "top", "bottom" };

void _lua_object_get_number_per_face(lua_State *L, const int idx, float faces[FACE_COUNT], float nullValue);
void _lua_object_push_number_per_face(lua_State *L, const float faces[FACE_COUNT], Transform *t);
static int _lua_object_number_per_face_tostring(lua_State *L);

// MARK: Lua fields

Transform* _lua_object_get_transform_from_number3_handler(const number3_C_handler_userdata *userdata);
Transform* _lua_object_get_transform_from_rotation_handler(const rotation_handler_userdata *userdata);

void _lua_object_set_position(Transform *t, const float *x, const float *y, const float *z);
void _lua_object_get_position(Transform *t, float *x, float *y, float *z);
void _lua_object_set_position_handler(const float *x, const float *y, const float *z, lua_State *L, const number3_C_handler_userdata *userdata);
void _lua_object_get_position_handler(float *x, float *y, float *z, lua_State *L, const number3_C_handler_userdata *userdata);

void _lua_object_set_local_position(Transform *t, const float *x, const float *y, const float *z);
void _lua_object_get_local_position(Transform *t, float *x, float *y, float *z);
void _lua_object_set_local_position_handler(const float *x, const float *y, const float *z, lua_State *L, const number3_C_handler_userdata *userdata);
void _lua_object_get_local_position_handler(float *x, float *y, float *z, lua_State *L, const number3_C_handler_userdata *userdata);

void _lua_object_set_rotation_handler(Quaternion *q, lua_State *L, const rotation_handler_userdata *userdata);
void _lua_object_get_rotation_handler(Quaternion *q, lua_State *L, const rotation_handler_userdata *userdata);

void _lua_object_set_local_rotation_handler(Quaternion *q, lua_State *L, const rotation_handler_userdata *userdata);
void _lua_object_get_local_rotation_handler(Quaternion *q, lua_State *L, const rotation_handler_userdata *userdata);

void _lua_object_set_scale(Transform *t, const float *x, const float *y, const float *z);
void _lua_object_get_scale(Transform *t, float *x, float *y, float *z);
void _lua_object_set_localscale_handler(const float *x, const float *y, const float *z, lua_State *L, const number3_C_handler_userdata *userdata);
void _lua_object_get_localscale_handler(float *x, float *y, float *z, lua_State *L, const number3_C_handler_userdata *userdata);

void _lua_object_get_lossyscale(Transform *t, float *x, float *y, float *z);
void _lua_object_get_lossyscale_handler(float *x, float *y, float *z, lua_State *L, const number3_C_handler_userdata *userdata);

void _lua_object_set_right(Transform *t, const float *x, const float *y, const float *z);
void _lua_object_get_right(Transform *t, float *x, float *y, float *z);
void _lua_object_set_right_handler(const float *x, const float *y, const float *z, lua_State *L, const number3_C_handler_userdata *userdata);
void _lua_object_get_right_handler(float *x, float *y, float *z, lua_State *L, const number3_C_handler_userdata *userdata);

void _lua_object_set_up(Transform * const t, const float * const x, const float * const y, const float * const z);
void _lua_object_get_up(Transform *t, float *x, float *y, float *z);
void _lua_object_set_up_handler(const float * const x, const float * const y, const float * const z, lua_State * const L, const number3_C_handler_userdata * const userdata);
void _lua_object_get_up_handler(float *x, float *y, float *z, lua_State *L, const number3_C_handler_userdata *userdata);

void _lua_object_set_forward(Transform *t, const float *x, const float *y, const float *z);
void _lua_object_get_forward(Transform *t, float *x, float *y, float *z);
void _lua_object_set_forward_handler(const float *x, const float *y, const float *z, lua_State *L, const number3_C_handler_userdata *userdata);
void _lua_object_get_forward_handler(float *x, float *y, float *z, lua_State *L, const number3_C_handler_userdata *userdata);

void _lua_object_set_left_handler(const float *x, const float *y, const float *z, lua_State *L, const number3_C_handler_userdata *userdata);
void _lua_object_get_left_handler(float *x, float *y, float *z, lua_State *L, const number3_C_handler_userdata *userdata);

void _lua_object_set_down_handler(const float *x, const float *y, const float *z, lua_State *L, const number3_C_handler_userdata *userdata);
void _lua_object_get_down_handler(float *x, float *y, float *z, lua_State *L, const number3_C_handler_userdata *userdata);

void _lua_object_set_backward_handler(const float *x, const float *y, const float *z, lua_State *L, const number3_C_handler_userdata *userdata);
void _lua_object_get_backward_handler(float *x, float *y, float *z, lua_State *L, const number3_C_handler_userdata *userdata);

RigidBody* _lua_object_ensure_rigidbody(Transform *t);
void _lua_object_non_kinematic_reset(Transform *t);
void _lua_object_set_physics(Transform *t, RigidbodyMode value);
uint16_t _lua_object_get_groups(Transform *t);
void _lua_object_set_groups(Transform *t, const uint16_t value);
uint16_t _lua_object_get_collides_with(Transform *t);
void _lua_object_set_collides_with(Transform *t, const uint16_t value);
float _lua_object_get_mass(Transform *t);
void _lua_object_set_mass(Transform *t, const float value);
void _lua_object_get_friction(Transform *t, float faces[FACE_COUNT]);
void _lua_object_set_friction(Transform *t, const FACE_INDEX_INT_T face, const float value);
void _lua_object_get_bounciness(Transform *t, float faces[FACE_COUNT]);
void _lua_object_set_bounciness(Transform *t, const FACE_INDEX_INT_T face, const float value);
void _lua_object_toggle_collision_callback(Transform *t, CollisionCallbackType type, bool value);

void _lua_object_set_velocity(Transform *t, const float *x, const float *y, const float *z);
void _lua_object_get_velocity(Transform *t, float *x, float *y, float *z);
void _lua_object_set_velocity_handler(const float *x, const float *y, const float *z, lua_State *L, const number3_C_handler_userdata *userdata);
void _lua_object_get_velocity_handler(float *x, float *y, float *z, lua_State *L, const number3_C_handler_userdata *userdata);

void _lua_object_set_motion(Transform *t, const float *x, const float *y, const float *z);
void _lua_object_get_motion(Transform *t, float *x, float *y, float *z);
void _lua_object_set_motion_handler(const float *x, const float *y, const float *z, lua_State *L, const number3_C_handler_userdata *userdata);
void _lua_object_get_motion_handler(float *x, float *y, float *z, lua_State *L, const number3_C_handler_userdata *userdata);

void _lua_object_set_acceleration(Transform *t, const float *x, const float *y, const float *z);
void _lua_object_get_acceleration(Transform *t, float *x, float *y, float *z);
void _lua_object_set_acceleration_handler(const float *x, const float *y, const float *z, lua_State *L, const number3_C_handler_userdata *userdata);
void _lua_object_get_acceleration_handler(float *x, float *y, float *z, lua_State *L, const number3_C_handler_userdata *userdata);

void _lua_object_set_collider_origin(Transform *t, const float *x, const float *y, const float *z);
void _lua_object_get_collider_origin(Transform *t, float *x, float *y, float *z);
void _lua_object_set_collider_origin_handler(const float *x, const float *y, const float *z, lua_State *L, const number3_C_handler_userdata *userdata);
void _lua_object_get_collider_origin_handler(float *x, float *y, float *z, lua_State *L, const number3_C_handler_userdata *userdata);

void _lua_object_set_collider_size(Transform *t, const float *x, const float *y, const float *z);
void _lua_object_get_collider_size(Transform *t, float *x, float *y, float *z);
void _lua_object_set_collider_size_handler(const float *x, const float *y, const float *z, lua_State *L, const number3_C_handler_userdata *userdata);
void _lua_object_get_collider_size_handler(float *x, float *y, float *z, lua_State *L, const number3_C_handler_userdata *userdata);

void _lua_object_set_collision_box(Transform * const t, const float3 * const min, const float3 * const max);
void _lua_object_get_collision_box(Transform * const t, float3 * const min, float3 * const max);
void _lua_object_set_collision_box_handler(const float3 *min, const float3 *max, lua_State *L, void *userdata);
void _lua_object_get_collision_box_handler(float3 *min, float3 *max, lua_State *L, const void *userdata);
void _lua_object_free_collision_box_handler(void * const userdata);
void _lua_object_set_collision_box_min_n3_handler(const float *x, const float *y, const float *z,
                                                  lua_State *L, const number3_C_handler_userdata *userdata);
void _lua_object_get_collision_box_min_n3_handler(float *x, float *y, float *z, lua_State *L,
                                                  const number3_C_handler_userdata *userdata);
void _lua_object_set_collision_box_max_n3_handler(const float *x, const float *y, const float *z,
                                                  lua_State *L, const number3_C_handler_userdata *userdata);
void _lua_object_get_collision_box_max_n3_handler(float *x, float *y, float *z, lua_State *L,
                                                  const number3_C_handler_userdata *userdata);
static int _children_newindex(lua_State *L);

// MARK: Lua functions

static int _object_setParent(lua_State *L);
static int _object_removeParent(lua_State *L);
static int _object_getParent(lua_State *L);
static int _object_addChild(lua_State *L);
static int _object_removeChild(lua_State *L);
static int _object_removeChildren(lua_State *L);
static int _object_getChild(lua_State *L);
static int _object_localToWorld(lua_State *L, bool isPos);
static int _object_worldToLocal(lua_State *L, bool isPos);
static int _object_positionLtw(lua_State *L);
static int _object_positionWtl(lua_State *L);
static int _object_rotationLtw(lua_State *L);
static int _object_rotationWtl(lua_State *L);
static int _object_rotate(lua_State * const L, const bool isLocal);
static int _object_rotateLocal(lua_State * const L);
static int _object_rotateWorld(lua_State * const L);
static int _object_collidesWith(lua_State *L);
static int _object_applyForce(lua_State *L);
static int _object_awake(lua_State *L);
static int _object_refresh(lua_State *L);
static int _object_resetCollider(lua_State *L);
static int _object_find_first(lua_State * const L);
static int _object_find(lua_State * const L);
static int _object_recurse(lua_State *L);
static int _object_copy(lua_State *L);
static int _object_destroy(lua_State *L);

// --------------------------------------------------
// MARK: - Object global table -
// --------------------------------------------------

void lua_g_object_createAndPush(lua_State * const L) {
    vx_assert(L != nullptr);
    LUAUTILS_STACK_SIZE(L)
    
    lua_newuserdata(L, 1); // global "Object" table
    lua_newtable(L); // metatable
    {
        lua_pushstring(L, "__index");
        lua_pushcfunction(L, _g_object_metatable_index, "");
        lua_rawset(L, -3);
        
        lua_pushstring(L, "__newindex");
        lua_pushcfunction(L, _g_object_metatable_newindex, "");
        lua_rawset(L, -3);
        
        lua_pushstring(L, "__call");
        lua_pushcfunction(L, _g_object_metatable_call, "");
        lua_rawset(L, -3);
        
        // hide the metatable from the Lua sandbox user
        lua_pushstring(L, "__metatable");
        lua_pushboolean(L, false);
        lua_rawset(L, -3);
        
        lua_pushstring(L, "__type");
        lua_pushstring(L, "ObjectInterface");
        lua_rawset(L, -3);

        lua_pushstring(L, "__typen");
        lua_pushinteger(L, ITEM_TYPE_G_OBJECT);
        lua_rawset(L, -3);
    }
    lua_setmetatable(L, -2);

    // REGISTRY ENTRIES:

    // A table to index Objects by transform ID.
    // Used to avoid creating several Objects representing the same transform.
    // Keeps week references on values to avoid retaining Objects that aren't
    // referenced anywhere else.
    // Used by all kinds of Objects (Shape, MutableShape, AudioSource, etc.)
    lua_pushstring(L, REGISTRY_OBJECTS_INDEXED_BY_TRANSFORM_ID);
    lua_newtable(L);
    lua_newtable(L); // metatable
    {
        lua_pushstring(L, "__mode");
        lua_pushstring(L, "v");
        lua_rawset(L, -3);
    }
    lua_setmetatable(L, -2);
    lua_rawset(L, LUA_REGISTRYINDEX);

    // A table used to store Lua values associated with transforms.
    // Not using weak references I we want those values to persist
    // as long as the transform exists.
    // Entries are destroyed upon corresponding transform's destruction.
    // Used by all kinds of Objects (Shape, MutableShape, AudioSource, etc.)
    lua_pushstring(L, REGISTRY_OBJECTS_STORAGE);
    lua_newtable(L);
    lua_rawset(L, LUA_REGISTRYINDEX);

    // insert in registry for easy access to `Object` from any thread
    lua_pushstring(L, LUA_G_OBJECT_REGISTRY_INDEX);
    lua_pushvalue(L, -2);
    lua_rawset(L, LUA_REGISTRYINDEX);

    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
}

int lua_g_object_snprintf(lua_State *L, char *result, size_t maxLen, bool spacePrefix) {
    RETURN_VALUE_IF_NULL(L, 0)
    return snprintf(result, maxLen, "%s[Object] (global)", spacePrefix ? " " : "");
}

void lua_g_object_pushObjectsIndexedByTransformID(lua_State *L) {
    vx_assert(L != nullptr);
    LUAUTILS_STACK_SIZE(L)
    lua_pushstring(L, REGISTRY_OBJECTS_INDEXED_BY_TRANSFORM_ID);
    lua_rawget(L, LUA_REGISTRYINDEX);
    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
}

bool lua_g_object_addIndexEntry(lua_State *L, Transform *t, const int idx) {
    vx_assert(L != nullptr);
    vx_assert(t != nullptr);
    vx_assert(lua_object_isObject(L, idx));
    LUAUTILS_STACK_SIZE(L)

    lua_g_object_pushObjectsIndexedByTransformID(L);

    LUAUTILS_STACK_SIZE_ASSERT(L, 1)

    lua_pushvalue(L, idx < 0 ? idx - 1 : idx); // Object
    lua_rawseti(L, -2, transform_get_id(t));

    lua_pop(L, 1); // pop index table

    LUAUTILS_STACK_SIZE_ASSERT(L, 0)
    return true;
}

bool lua_g_object_removeIndexEntry(lua_State *L, Transform *t) {
    vx_assert(L != nullptr);
    vx_assert(t != nullptr);
    LUAUTILS_STACK_SIZE(L)
    
    lua_g_object_pushObjectsIndexedByTransformID(L);

    LUAUTILS_STACK_SIZE_ASSERT(L, 1)

    lua_pushnil(L); // set to nil to remove entry
    lua_rawseti(L, -2, transform_get_id(t));

    lua_pop(L, 1); // pop index table
    
    LUAUTILS_STACK_SIZE_ASSERT(L, 0)
    return true;
}

bool lua_g_object_getIndexEntry(lua_State *L, Transform *t) {
    vx_assert(L != nullptr);
    vx_assert(t != nullptr);
    LUAUTILS_STACK_SIZE(L)
    
    lua_g_object_pushObjectsIndexedByTransformID(L);

    LUAUTILS_STACK_SIZE_ASSERT(L, 1)

    lua_rawgeti(L, -1, transform_get_id(t));

    LUAUTILS_STACK_SIZE_ASSERT(L, 2)
    
    lua_remove(L, -2); // remove index table

    if (lua_object_isObject(L, -1) == false) {
        lua_pop(L, 1);
        LUAUTILS_STACK_SIZE_ASSERT(L, 0)
        return false;
    }
    
    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
    return true;
}

bool lua_g_object_getOrCreateInstance(lua_State *L, Transform *t) {
    if (lua_g_object_getIndexEntry(L, t)) {
        return true;
    } else {
        switch(transform_get_type(t)) {
            case HierarchyTransform:
                return false;
            case PointTransform:
                return lua_object_pushNewInstance(L, t);
            case ShapeTransform: {
                Shape *s = static_cast<Shape *>(transform_get_ptr(t));
                const bool created = (shape_is_lua_mutable(s) ?
                                     lua_mutableShape_pushNewInstance(L, s) :
                                     lua_shape_pushNewInstance(L, s));
                return created;
            }
            case QuadTransform: {
                Quad *q = static_cast<Quad *>(transform_get_ptr(t));
                return lua_quad_pushNewInstance(L, q);
            }
            case WorldTextTransform: {
                WorldText *wt = static_cast<WorldText *>(transform_get_ptr(t));
                return lua_text_pushNewInstance(L, wt);
            }
            case AudioListenerTransform: {
#if !defined(P3S_CLIENT_HEADLESS)
                lua_g_audioListener_push(L); // only one AudioListener per scene
                return true;
#else
                return false;
#endif
            }
            case AudioSourceTransform: {
#if !defined(P3S_CLIENT_HEADLESS)
                AudioSource *audio = static_cast<AudioSource *>(transform_get_ptr(t));
                return lua_audioSource_pushNewInstance(L, audio);
#else
                return false;
#endif
            }
            case LightTransform: {
                Light *l = static_cast<Light *>(transform_get_ptr(t));
                return lua_light_pushNewInstance(L, l);
            }
            case CameraTransform: {
                Camera *c = static_cast<Camera *>(transform_get_ptr(t));
                return lua_camera_pushNewInstance(L, c);
            }
            case MeshTransform: {
                Mesh *m = static_cast<Mesh *>(transform_get_ptr(t));
                return lua_mesh_pushNewInstance(L, m);
            }
            default: {
                vxlog_warning("lua_g_object_getOrCreateInstance called & failed for type %d, this shouldn't happen",
                              transform_get_type(t));
                return false;
            }
        }
    }
}

// --------------------------------------------------
// MARK: - Object instances tables -
// --------------------------------------------------

bool lua_object_pushNewInstance(lua_State *L, Transform *t) {
    vx_assert(L != nullptr);
    if (t == nullptr) return false;

    // lua Object takes ownership
    if (transform_retain(t) == false) {
        LUAUTILS_INTERNAL_ERROR(L);
        // return false;
    }

    LUAUTILS_STACK_SIZE(L)

    vx::Game *g = nullptr;
    if (lua_utils_getGameCppWrapperPtr(L, &g) == false) {
        LUAUTILS_INTERNAL_ERROR(L);
    }

    object_userdata* ud = static_cast<object_userdata*>(lua_newuserdatadtor(L, sizeof(object_userdata), _object_gc));
    ud->transform = t;

    // connect to userdata store
    ud->store = g->userdataStoreForObjects;
    ud->previous = nullptr;
    object_userdata* next = static_cast<object_userdata*>(ud->store->add(ud));
    ud->next = next;
    if (next != nullptr) {
        next->previous = ud;
    }

    lua_newtable(L); // metatable
    {
        // NOTE: fields shared by all objects could be put in a core metatable (shared ref)
        lua_pushstring(L, "__index");
        lua_pushcfunction(L, _object_metatable_index, "");
        lua_rawset(L, -3);
        
        lua_pushstring(L, "__newindex");
        lua_pushcfunction(L, _object_metatable_newindex, "");
        lua_rawset(L, -3);
        
        // hide the metatable from the Lua sandbox user
        lua_pushstring(L, "__metatable");
        lua_pushboolean(L, false);
        lua_rawset(L, -3);

        lua_pushstring(L, "__type");
        lua_pushstring(L, "Object");
        lua_rawset(L, -3);

        lua_pushstring(L, "__typen");
        lua_pushinteger(L, ITEM_TYPE_OBJECT);
        lua_rawset(L, -3);
    }
    lua_setmetatable(L, -2);
    
    if (lua_g_object_addIndexEntry(L, t, -1) == false) {
        vxlog_error("Failed to add Lua Object to registry");
        lua_pop(L, 1); // pop Lua Object
        LUAUTILS_STACK_SIZE_ASSERT(L, 0)
        return false;
    }

    // manage resources in lua object storage
    scene_register_managed_transform(game_get_scene(g->getCGame()), t);

    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
    return true;
}

bool lua_object_isObject(lua_State *L, const int idx) {
    vx_assert(L != nullptr);
    if (lua_type(L, idx) != LUA_TUSERDATA && lua_type(L, idx) != LUA_TTABLE) {
        return false;
    }
    return lua_object_isObjectType(lua_utils_getObjectType(L, idx));
}

bool lua_object_isObjectType(const int type) {
    return (type == ITEM_TYPE_OBJECT
            || lua_shape_isShapeType(type)
            || type == ITEM_TYPE_PLAYER
            || type == ITEM_TYPE_CAMERA
            || type == ITEM_TYPE_WORLD
            || type == ITEM_TYPE_LIGHT
            || type == ITEM_TYPE_AUDIOSOURCE
            || type == ITEM_TYPE_G_AUDIOLISTENER
            || type == ITEM_TYPE_TEXT
            || type == ITEM_TYPE_QUAD
            || type == ITEM_TYPE_MESH);
}

LUA_ITEM_TYPE lua_object_getTransformPtr(lua_State *L, const int idx, Transform **root) {
    vx_assert(L != nullptr);
    vx_assert(root != nullptr);
    LUAUTILS_STACK_SIZE(L)

    *root = nullptr;

    if (lua_isuserdata(L, idx) == false && lua_istable(L, idx) == false) {
        return ITEM_TYPE_NOT_AN_OBJECT;
    }

    LUA_ITEM_TYPE objectType = lua_utils_getObjectType(L, idx);

    if (objectType == ITEM_TYPE_OBJECT) {
        *root = static_cast<object_userdata*>(lua_touserdata(L, idx))->transform;
    } else if (objectType == ITEM_TYPE_SHAPE || objectType == ITEM_TYPE_MUTABLESHAPE) {
        Shape *s = lua_shape_getShapePtr(L, idx);
        if (s != nullptr) {
            *root = shape_get_transform(s);
        }
    } else if (objectType == ITEM_TYPE_CAMERA) {
        Camera *c = lua_camera_getCameraPtr(L, idx);
        if (c != nullptr) {
            *root = camera_get_view_transform(c);
        }
    } else if (objectType == ITEM_TYPE_MAP) {
        CGame *g = nullptr;
        if (lua_utils_getGamePtr(L, &g) == false || g == nullptr) {
            LUAUTILS_INTERNAL_ERROR(L);
        }
        const Shape *s = game_get_map_shape(g);
        if (s != nullptr) {
            *root = shape_get_transform(s);
        }
    } else if (objectType == ITEM_TYPE_WORLD) {
        CGame *g = nullptr;
        if (lua_utils_getGamePtr(L, &g) == false || g == nullptr) {
            LUAUTILS_INTERNAL_ERROR(L);
        }
        Scene *sc = game_get_scene(g);
        *root = scene_get_root(sc);
    } else if (objectType == ITEM_TYPE_LIGHT) {
        Light *l = lua_light_getLightPtr(L, idx);
        if (l != nullptr) {
            *root = light_get_transform(l);
        }
    }
#if !defined(P3S_CLIENT_HEADLESS)
    else if (objectType == ITEM_TYPE_AUDIOSOURCE) {
        AudioSource *as = lua_audioSource_getAudioSourcePtr(L, idx);
        if (as != nullptr) {
            *root = audioSource_getTransform(as);
        }
    } else if (objectType == ITEM_TYPE_G_AUDIOLISTENER) {
        AudioListener *al = lua_g_audioListener_getAudioListenerPtr(L, idx);
        if (al != nullptr) {
            *root = audioListener_getTransform(al);
        }
    }
#endif
    else if (objectType == ITEM_TYPE_TEXT) {
        WorldText *wt = lua_text_getWorldTextPtr(L, idx);
        if (wt != nullptr) {
            *root = world_text_get_transform(wt);
        }
    } else if (objectType == ITEM_TYPE_QUAD) {
        Quad *q = lua_quad_getQuadPtr(L, idx);
        if (q != nullptr) {
            *root = quad_get_transform(q);
        }
    } else if (objectType == ITEM_TYPE_PLAYER) {
        // Custom Object (see player.lua)
        *root = static_cast<object_userdata*>(lua_touserdata(L, idx))->transform;
    } else if (objectType == ITEM_TYPE_MESH) {
        Mesh *m = lua_mesh_getMeshPtr(L, idx);
        if (m != nullptr) {
            *root = mesh_get_transform(m);
        }
    } else {
        // ! \\ not an object!
        LUAUTILS_STACK_SIZE_ASSERT(L, 0)
        return ITEM_TYPE_NOT_AN_OBJECT;
    }

    if (*root == nullptr) {
        objectType = ITEM_TYPE_OBJECT_DESTROYED;
    }

    LUAUTILS_STACK_SIZE_ASSERT(L, 0)
    return objectType;
}

int lua_object_snprintf(lua_State *L, char *result, size_t maxLen, bool spacePrefix) {
    RETURN_VALUE_IF_NULL(L, 0)
    Transform *root; lua_object_getTransformPtr(L, -1, &root);
    if (root == nullptr) {
        return snprintf(result, maxLen, "%s[Object (destroyed)]", spacePrefix ? " " : "");
    }
    return snprintf(result, maxLen, "%s[Object %d Name:%s] (instance)", spacePrefix ? " " : "",
                    transform_get_id(root), transform_get_name(root));
}

bool lua_object_get_and_call_collision_callback(lua_State *L,
                                                Transform *self,
                                                Transform *other,
                                                float3 wNormal,
                                                uint8_t callbackType) {
    vx_assert(L != nullptr);
    vx_assert(self != nullptr && other != nullptr);
    
    LUAUTILS_STACK_SIZE(L)
    
    // get the callback function for Lua Object "self"
    std::string funcName;
    switch(static_cast<CollisionCallbackType>(callbackType)) {
        case CollisionCallbackType_Begin: funcName = LUA_OBJECT_FIELD_ONCOLLISIONBEGIN; break;
        case CollisionCallbackType_Tick: funcName = LUA_OBJECT_FIELD_ONCOLLISION; break;
        case CollisionCallbackType_End: funcName = LUA_OBJECT_FIELD_ONCOLLISIONEND; break;
    }

    lua_g_object_storage_get_field(L, transform_get_id(self), funcName.c_str());
    if (lua_isnil(L, -1)) {
        lua_pop(L, 1); // pop nil value
        LUAUTILS_STACK_SIZE_ASSERT(L, 0)
        return false;
    }
    
    LUAUTILS_STACK_SIZE_ASSERT(L, 1)

    // get Lua Object "self" from registry
    if (lua_g_object_getOrCreateInstance(L, self) == false) {
        lua_pop(L, 1);
        vxlog_error("failed to retrieve Lua Object in index");
        LUAUTILS_STACK_SIZE_ASSERT(L, 0)
        return false;
    }
    
    LUAUTILS_STACK_SIZE_ASSERT(L, 2)
    
    // (parameter 2) get Lua Object "other" from registry
    if (lua_g_object_getOrCreateInstance(L, other) == false) {
        lua_pop(L, 2);
        vxlog_error("failed to retrieve Lua Object in index");
        LUAUTILS_STACK_SIZE_ASSERT(L, 0)
        return false;
    }
    
    LUAUTILS_STACK_SIZE_ASSERT(L, 3)

    uint8_t argCount;
    if (callbackType != CollisionCallbackType_End) {
        // (parameter 3) push lua Number3 "worldNormal"
        lua_number3_pushNewObject(L, wNormal.x, wNormal.y, wNormal.z);
        LUAUTILS_STACK_SIZE_ASSERT(L, 4)
        argCount = 3;
    } else {
        argCount = 2;
    }
    
    // execute the callback
    const int state = lua_pcall(L, argCount, 0, 0);
    if (state != LUA_OK) {
        if (lua_utils_isStringStrict(L, -1)) {
            lua_log_error_CStr(L, lua_tostring(L, -1));
        } else {
            lua_log_error(L, "Object callback error");
        }
        lua_pop(L, 1); // pops the error
    }
    
    LUAUTILS_STACK_SIZE_ASSERT(L, 0)
    return true;
}

bool lua_object_call_tick(lua_State * const L, Transform *self, const TICK_DELTA_SEC_T dt) {
    vx_assert(L != nullptr);
    LUAUTILS_STACK_SIZE(L)
    
    // get the Lua Object tick callback
    lua_g_object_storage_get_field(L, transform_get_id(self), LUA_OBJECT_FIELD_TICK);
    if (lua_isnil(L, -1)) {
        lua_pop(L, 1); // pop nil value
        LUAUTILS_STACK_SIZE_ASSERT(L, 0)
        return false;
    }
    
    LUAUTILS_STACK_SIZE_ASSERT(L, 1)

    // get Lua Object "self" from registry
    if (lua_g_object_getOrCreateInstance(L, self) == false) {
        lua_pop(L, 1);
        vxlog_error("failed to retrieve Lua Object in index");
        LUAUTILS_STACK_SIZE_ASSERT(L, 0)
        return false;
    }

    LUAUTILS_STACK_SIZE_ASSERT(L, 2)

    // (parameter 2) push dt
    lua_pushnumber(L, static_cast<double>(dt));
    
    LUAUTILS_STACK_SIZE_ASSERT(L, 3)
    
    // execute the callback
    const int state = lua_pcall(L, 2, 0, 0);
    if (state != LUA_OK) {
        if (lua_utils_isStringStrict(L, -1)) {
            lua_log_error_CStr(L, lua_tostring(L, -1));
        } else {
            lua_log_error(L, "Object callback error");
        }
        lua_pop(L, 1); // pops the error
    }
    
    LUAUTILS_STACK_SIZE_ASSERT(L, 0)
    return true;
}

// --------------------------------------------------
// MARK: - Internal -
// --------------------------------------------------

typedef struct s_search_descendant {
    Transform *t;
    const char *name;
} t_search_descendant;

bool _find_descendant_by_name(Transform *t, void *ptr) {
    t_search_descendant *searchDescendant = static_cast<t_search_descendant *>(ptr);
    const char *name = transform_get_name(t);
    if (name != nullptr && strcmp(name, searchDescendant->name) == 0) {
        searchDescendant->t = t;
        return true;
    }
    return false;
}

bool _lua_object_metatable_index(lua_State *L, const char *key) {
    LUAUTILS_STACK_SIZE(L)
    
    if (lua_getmetatable(L, 1) == 0) {
        LUAUTILS_INTERNAL_ERROR(L);
    } // metatable at -1
    
    bool readOnly = _lua_object_is_readonly(L, false);

    if (lua_utils_isStringStrictStartingWithUppercase(L, 2) == false) {
        // When is key anything but a string starting with uppercase:
        // access value from object's custom storage.
        Transform *t; LUA_ITEM_TYPE type = lua_object_getTransformPtr(L, 1, &t);
        if (t == nullptr) {
            if (type == ITEM_TYPE_OBJECT_DESTROYED) {
                LUAUTILS_ERROR_F(L, "can't get destroyed Object field: \"%s\"", lua_tostring(L, 2));
            } else {
                LUAUTILS_INTERNAL_ERROR(L);
            }
        }
        lua_g_object_storage_get_field(L, transform_get_id(t), key);
        goto return_true;
    } else if (strcmp(key, LUA_OBJECT_FIELD_POSITION) == 0) {
        _object_storage_get_or_create_number3(L, LUA_OBJECT_FIELD_POSITION,
                                              readOnly ? nullptr : _lua_object_set_position_handler,
                                              _lua_object_get_position_handler,
                                              [](Transform *t, float3 *out){ *out = *transform_get_position(t, true); });
        goto return_true;
        
    } else if (strcmp(key, LUA_OBJECT_FIELD_LOCALPOSITION) == 0) {
        _object_storage_get_or_create_number3(L, LUA_OBJECT_FIELD_LOCALPOSITION,
                                              readOnly ? nullptr : _lua_object_set_local_position_handler,
                                              _lua_object_get_local_position_handler,
                                              [](Transform *t, float3 *out){ *out = *transform_get_local_position(t, true); });
        goto return_true;
    } else if (strcmp(key, LUA_OBJECT_FIELD_ROTATION) == 0) {
        _object_storage_get_or_create_rotation(L, LUA_OBJECT_FIELD_ROTATION,
                                               readOnly ? nullptr : _lua_object_set_rotation_handler,
                                               _lua_object_get_rotation_handler,
                                               &transform_get_rotation);
        goto return_true;
    } else if (strcmp(key, LUA_OBJECT_FIELD_LOCALROTATION) == 0) {
        _object_storage_get_or_create_rotation(L, LUA_OBJECT_FIELD_LOCALROTATION,
                                               readOnly ? nullptr : _lua_object_set_local_rotation_handler,
                                               _lua_object_get_local_rotation_handler,
                                               &transform_get_local_rotation);
        goto return_true;
    } else if (strcmp(key, LUA_OBJECT_FIELD_SCALE) == 0 ||
               strcmp(key, LUA_OBJECT_FIELD_LOCALSCALE) == 0) {
        _object_storage_get_or_create_number3(L, LUA_OBJECT_FIELD_LOCALSCALE,
                                              readOnly ? nullptr : _lua_object_set_localscale_handler,
                                              _lua_object_get_localscale_handler,
                                              [](Transform *t, float3 *out){ *out = *transform_get_local_scale(t); });
        goto return_true;
    } else if (strcmp(key, LUA_OBJECT_FIELD_LOSSYSCALE) == 0) {
        _object_storage_get_or_create_number3(L, LUA_OBJECT_FIELD_LOSSYSCALE,
                                              nullptr,
                                              _lua_object_get_lossyscale_handler,
                                              [](Transform *t, float3 *out){ transform_get_lossy_scale(t, out, true); });
        goto return_true;
    } else if (strcmp(key, LUA_OBJECT_FIELD_SETPARENT) == 0) {
        lua_pushcfunction(L, _object_setParent, "");
        goto return_true;
    } else if (strcmp(key, LUA_OBJECT_FIELD_REMOVEFROMPARENT) == 0) {
        lua_pushcfunction(L, _object_removeParent, "");
        goto return_true;
    } else if (strcmp(key, LUA_OBJECT_FIELD_GETPARENT) == 0) {
        lua_pushcfunction(L, _object_getParent, "");
        goto return_true;
    } else if (strcmp(key, LUA_OBJECT_FIELD_ADDCHILD) == 0) {
        lua_pushcfunction(L, _object_addChild, "");
        goto return_true;
    } else if (strcmp(key, LUA_OBJECT_FIELD_REMOVECHILD) == 0) {
        lua_pushcfunction(L, _object_removeChild, "");
        goto return_true;
    } else if (strcmp(key, LUA_OBJECT_FIELD_REMOVECHILDREN) == 0) {
        lua_pushcfunction(L, _object_removeChildren, "");
        goto return_true;
    } else if (strcmp(key, LUA_OBJECT_FIELD_GETCHILD) == 0) {
        lua_pushcfunction(L, _object_getChild, "");
        goto return_true;
    } else if (strcmp(key, LUA_OBJECT_FIELD_CHILDREN) == 0) {
        lua_newtable(L); // Children table
        {
            Transform *root; lua_object_getTransformPtr(L, 1, &root);
            if (root == nullptr) {
                goto return_false;
            }

            DoublyLinkedListNode *n = transform_get_children_iterator(root);
            int count = 0;
            Transform *child = nullptr;
            while (n != nullptr) {
                child = static_cast<Transform*>(doubly_linked_list_node_pointer(n));
                // hide transforms reserved for engine
                if (transform_get_type(child) != HierarchyTransform) {
                    count++;
                    lua_g_object_getOrCreateInstance(L, child);
                    lua_rawseti(L, -2, count);
                }
                n = doubly_linked_list_node_next(n);
            }
            goto return_true;
        }
        lua_newtable(L);
        {
            lua_pushstring(L, "__newindex");
            lua_pushcfunction(L, _children_newindex, "");
            lua_rawset(L, -3);

            lua_pushstring(L, "__metatable");
            lua_pushboolean(L, false);
            lua_rawset(L, -3);
        }
        lua_setmetatable(L, -2);



        goto return_true;
    } else if (strcmp(key, LUA_OBJECT_FIELD_PARENT) == 0) {
        lua_pushcfunction(L, _object_getParent, "");
        lua_pushvalue(L, 1); // push Object
        lua_call(L, 1, 1);
        goto return_true;
    } else if (strcmp(key, LUA_OBJECT_FIELD_CHILDRENCOUNT) == 0) {
        Transform *root; GET_OBJECT_ROOT_TRANSFORM_WITH_FIELD_ERROR(1, root, "can't get destroyed Object field: \"%s\"", key);
        DoublyLinkedListNode *n = transform_get_children_iterator(root);
        int count = 0;
        Transform *child = nullptr;
        while (n != nullptr) {
            child = static_cast<Transform*>(doubly_linked_list_node_pointer(n));
            // hide transforms reserved for engine
            if (transform_get_type(child) != HierarchyTransform) {
                count++;
            }
            n = doubly_linked_list_node_next(n);
        }
        lua_pushinteger(L, static_cast<lua_Integer>(count));
        goto return_true;
    } else if (strcmp(key, LUA_OBJECT_FIELD_POSITIONLTW) == 0) {
        lua_pushcfunction(L, _object_positionLtw, "");
        goto return_true;
    } else if (strcmp(key, LUA_OBJECT_FIELD_POSITIONWTL) == 0) {
        lua_pushcfunction(L, _object_positionWtl, "");
        goto return_true;
    } else if (strcmp(key, LUA_OBJECT_FIELD_ROTATIONLTW) == 0) {
        lua_pushcfunction(L, _object_rotationLtw, "");
        goto return_true;
    } else if (strcmp(key, LUA_OBJECT_FIELD_ROTATIONWTL) == 0) {
        lua_pushcfunction(L, _object_rotationWtl, "");
        goto return_true;
    } else if (strcmp(key, LUA_OBJECT_FIELD_ROTATELOCAL) == 0) {
        lua_pushcfunction(L, _object_rotateLocal, "");
        goto return_true;
    } else if (strcmp(key, LUA_OBJECT_FIELD_ROTATEWORLD) == 0) {
        lua_pushcfunction(L, _object_rotateWorld, "");
        goto return_true;
    } else if (strcmp(key, LUA_OBJECT_FIELD_SHADOWCOOKIE) == 0) {
        Transform *root; GET_OBJECT_ROOT_TRANSFORM_WITH_FIELD_ERROR(1, root, "can't get destroyed Object field: \"%s\"", key);
        lua_pushnumber(L, static_cast<double>(transform_get_shadow_decal(root)));
        return true;
    } else if (strcmp(key, LUA_OBJECT_FIELD_ANIMATIONS) == 0) {
        lua_pushstring(L, LUA_OBJECT_FIELD_ANIMATIONS);
        lua_rawget(L, -2);
        goto return_true;
    } else if (strcmp(key, LUA_OBJECT_FIELD_RIGHT) == 0) {
        _object_storage_get_or_create_number3(L, LUA_OBJECT_FIELD_RIGHT,
                                              readOnly ? nullptr : _lua_object_set_right_handler,
                                              _lua_object_get_right_handler,
                                              [](Transform *t, float3 *out){ transform_get_right(t, out, true); });
        goto return_true;
    } else if (strcmp(key, LUA_OBJECT_FIELD_UP) == 0) {
        _object_storage_get_or_create_number3(L, LUA_OBJECT_FIELD_UP,
                                              readOnly ? nullptr : _lua_object_set_up_handler,
                                              _lua_object_get_up_handler,
                                              [](Transform *t, float3 *out){ transform_get_up(t, out, true); });
        goto return_true;
    } else if (strcmp(key, LUA_OBJECT_FIELD_FORWARD) == 0) {
        _object_storage_get_or_create_number3(L, LUA_OBJECT_FIELD_FORWARD,
                                              readOnly ? nullptr : _lua_object_set_forward_handler,
                                              _lua_object_get_forward_handler,
                                              [](Transform *t, float3 *out){ transform_get_forward(t, out, true); });
        goto return_true;
    } else if (strcmp(key, LUA_OBJECT_FIELD_LEFT) == 0) {
        _object_storage_get_or_create_number3(L, LUA_OBJECT_FIELD_LEFT,
                                              readOnly ? nullptr : _lua_object_set_left_handler,
                                              _lua_object_get_left_handler,
                                              [](Transform *t, float3 *out){ transform_utils_get_left(t, out, true); });
        goto return_true;
    } else if (strcmp(key, LUA_OBJECT_FIELD_DOWN) == 0) {
        _object_storage_get_or_create_number3(L, LUA_OBJECT_FIELD_DOWN,
                                              readOnly ? nullptr : _lua_object_set_down_handler,
                                              _lua_object_get_down_handler,
                                              [](Transform *t, float3 *out){ transform_utils_get_down(t, out, true); });
        goto return_true;
    } else if (strcmp(key, LUA_OBJECT_FIELD_BACKWARD) == 0) {
        _object_storage_get_or_create_number3(L, LUA_OBJECT_FIELD_BACKWARD,
                                              readOnly ? nullptr : _lua_object_set_backward_handler,
                                              _lua_object_get_backward_handler,
                                              [](Transform *t, float3 *out){ transform_utils_get_backward(t, out, true); });
        goto return_true;
    } else if (strcmp(key, LUA_OBJECT_FIELD_PHYSICS) == 0) {
        Transform *root; GET_OBJECT_ROOT_TRANSFORM_WITH_FIELD_ERROR(1, root, "can't get destroyed Object field: \"%s\"", key);
        lua_physicsMode_pushTable(L, static_cast<RigidbodyMode>(rigidbody_get_simulation_mode(transform_get_rigidbody(root))));
        goto return_true;
    } else if (strcmp(key, LUA_OBJECT_FIELD_VELOCITY) == 0) {
        _object_storage_get_or_create_number3(L, LUA_OBJECT_FIELD_VELOCITY,
                                              readOnly ? nullptr : _lua_object_set_velocity_handler,
                                              _lua_object_get_velocity_handler,
                                              [](Transform *t, float3 *out){ if (transform_utils_get_velocity(t) != nullptr) *out = *transform_utils_get_velocity(t); });
        goto return_true;
    } else if (strcmp(key, LUA_OBJECT_FIELD_MOTION) == 0) {
        _object_storage_get_or_create_number3(L, LUA_OBJECT_FIELD_MOTION,
                                              readOnly ? nullptr : _lua_object_set_motion_handler,
                                              _lua_object_get_motion_handler,
                                              [](Transform *t, float3 *out){ if (transform_utils_get_motion(t) != nullptr) *out = *transform_utils_get_motion(t); });
        goto return_true;
    } else if (strcmp(key, LUA_OBJECT_FIELD_ACCELERATION) == 0) {
        _object_storage_get_or_create_number3(L, LUA_OBJECT_FIELD_ACCELERATION,
                                              readOnly ? nullptr : _lua_object_set_acceleration_handler,
                                              _lua_object_get_acceleration_handler,
                                              [](Transform *t, float3 *out){ if (transform_utils_get_acceleration(t) != nullptr) *out = *transform_utils_get_acceleration(t); });
        goto return_true;
    } else if (strcmp(key, LUA_OBJECT_FIELD_ISHIDDEN) == 0) {
        Transform *root; GET_OBJECT_ROOT_TRANSFORM_WITH_FIELD_ERROR(1, root, "can't get destroyed Object field: \"%s\"", key);
        lua_pushboolean(L, transform_is_hidden_branch(root));
        goto return_true;
    } else if (strcmp(key, LUA_OBJECT_FIELD_NAME) == 0) {
        Transform *root; GET_OBJECT_ROOT_TRANSFORM_WITH_FIELD_ERROR(1, root, "can't get destroyed Object field: \"%s\"", key);
        const char* name = transform_get_name(root);
        if (name == nullptr) {
            lua_pushnil(L);
        } else {
            lua_pushstring(L, transform_get_name(root));
        }
        goto return_true;
    } else if (strcmp(key, LUA_OBJECT_FIELD_ID) == 0) {
        Transform *root; GET_OBJECT_ROOT_TRANSFORM_WITH_FIELD_ERROR(1, root, "can't get destroyed Object field: \"%s\"", key);
        lua_pushinteger(L, transform_get_id(root));
        goto return_true;
    } else if (strcmp(key, LUA_OBJECT_FIELD_ISHIDDENSELF) == 0) {
        Transform *root; GET_OBJECT_ROOT_TRANSFORM_WITH_FIELD_ERROR(1, root, "can't get destroyed Object field: \"%s\"", key);
        lua_pushboolean(L, transform_is_hidden_self(root));
        goto return_true;
    } else if (strcmp(key, LUA_OBJECT_FIELD_CONTACTRIGHT) == 0) {
        Transform *root; GET_OBJECT_ROOT_TRANSFORM_WITH_FIELD_ERROR(1, root, "can't get destroyed Object field: \"%s\"", key);
        lua_pushboolean(L, rigidbody_has_contact(transform_get_rigidbody(root), AxesMaskX));
        goto return_true;
    } else if (strcmp(key, LUA_OBJECT_FIELD_CONTACTLEFT) == 0) {
        Transform *root; GET_OBJECT_ROOT_TRANSFORM_WITH_FIELD_ERROR(1, root, "can't get destroyed Object field: \"%s\"", key);
        lua_pushboolean(L, rigidbody_has_contact(transform_get_rigidbody(root), AxesMaskNX));
        goto return_true;
    } else if (strcmp(key, LUA_OBJECT_FIELD_CONTACTUP) == 0) {
        Transform *root; GET_OBJECT_ROOT_TRANSFORM_WITH_FIELD_ERROR(1, root, "can't get destroyed Object field: \"%s\"", key);
        lua_pushboolean(L, rigidbody_has_contact(transform_get_rigidbody(root), AxesMaskY));
        goto return_true;
    } else if (strcmp(key, LUA_OBJECT_FIELD_CONTACTDOWN) == 0) {
        Transform *root; GET_OBJECT_ROOT_TRANSFORM_WITH_FIELD_ERROR(1, root, "can't get destroyed Object field: \"%s\"", key);
        lua_pushboolean(L, rigidbody_has_contact(transform_get_rigidbody(root), AxesMaskNY));
        goto return_true;
    } else if (strcmp(key, LUA_OBJECT_FIELD_CONTACTFORWARD) == 0) {
        Transform *root; GET_OBJECT_ROOT_TRANSFORM_WITH_FIELD_ERROR(1, root, "can't get destroyed Object field: \"%s\"", key);
        lua_pushboolean(L, rigidbody_has_contact(transform_get_rigidbody(root), AxesMaskZ));
        goto return_true;
    } else if (strcmp(key, LUA_OBJECT_FIELD_CONTACTBACKWARD) == 0) {
        Transform *root; GET_OBJECT_ROOT_TRANSFORM_WITH_FIELD_ERROR(1, root, "can't get destroyed Object field: \"%s\"", key);
        lua_pushboolean(L, rigidbody_has_contact(transform_get_rigidbody(root), AxesMaskNZ));
        goto return_true;
    } else if (strcmp(key, LUA_OBJECT_FIELD_COLLISIONBOX) == 0) {
        
        lua_pushstring(L, LUA_OBJECT_FIELD_COLLISIONBOX);
        if (lua_rawget(L, -2) == LUA_TNIL) {
            lua_pop(L, 1); // pop nil value
            // never been accessed, create it
            lua_pushstring(L, LUA_OBJECT_FIELD_COLLISIONBOX);
            Transform *root; GET_OBJECT_ROOT_TRANSFORM_WITH_FIELD_ERROR(1, root, "can't get destroyed Object field: \"%s\"", key);
            lua_box_pushNewTable(L,
                                 static_cast<void *>(transform_get_and_retain_weakptr(root)),
                                 _lua_object_set_collision_box_handler,
                                 _lua_object_get_collision_box_handler,
                                 _lua_object_free_collision_box_handler,
                                 _lua_object_set_collision_box_min_n3_handler,
                                 _lua_object_get_collision_box_min_n3_handler,
                                 _lua_object_set_collision_box_max_n3_handler,
                                 _lua_object_get_collision_box_max_n3_handler);
            lua_rawset(L, -3);
            
            // try again, now supposed to work
            lua_pushstring(L, LUA_OBJECT_FIELD_COLLISIONBOX);
            lua_rawget(L, -2);
        }
        
        goto return_true;
    } else if (strcmp(key, LUA_OBJECT_FIELD_COLLISIONGROUPSMASK) == 0) {
        Transform *root; GET_OBJECT_ROOT_TRANSFORM_WITH_FIELD_ERROR(1, root, "can't get destroyed Object field: \"%s\"", key);
        lua_pushinteger(L, static_cast<lua_Integer>(_lua_object_get_groups(root)));
        goto return_true;
    } else if (strcmp(key, LUA_OBJECT_FIELD_COLLIDESWITHMASK) == 0) {
        Transform *root; GET_OBJECT_ROOT_TRANSFORM_WITH_FIELD_ERROR(1, root, "can't get destroyed Object field: \"%s\"", key);
        lua_pushinteger(L, static_cast<lua_Integer>(_lua_object_get_collides_with(root)));
        goto return_true;
    } else if (strcmp(key, LUA_OBJECT_FIELD_COLLISIONGROUPS) == 0) {
        Transform *root; GET_OBJECT_ROOT_TRANSFORM_WITH_FIELD_ERROR(1, root, "can't get destroyed Object field: \"%s\"", key);
        lua_collision_groups_create_and_push(L, _lua_object_get_groups(root));
        goto return_true;
    } else if (strcmp(key, LUA_OBJECT_FIELD_COLLIDESWITHGROUPS) == 0) {
        Transform *root; GET_OBJECT_ROOT_TRANSFORM_WITH_FIELD_ERROR(1, root, "can't get destroyed Object field: \"%s\"", key);
        lua_collision_groups_create_and_push(L, _lua_object_get_collides_with(root));
        goto return_true;
    } else if (strcmp(key, LUA_OBJECT_FIELD_COLLIDESWITH) == 0) {
        lua_pushcfunction(L, _object_collidesWith, "");
        goto return_true;
    } else if (strcmp(key, LUA_OBJECT_FIELD_MASS) == 0) {
        Transform *root; GET_OBJECT_ROOT_TRANSFORM_WITH_FIELD_ERROR(1, root, "can't get destroyed Object field: \"%s\"", key);
        lua_pushnumber(L, static_cast<lua_Number>(_lua_object_get_mass(root)));
        goto return_true;
    } else if (strcmp(key, LUA_OBJECT_FIELD_FRICTION) == 0) {
        Transform *root; GET_OBJECT_ROOT_TRANSFORM_WITH_FIELD_ERROR(1, root, "can't get destroyed Object field: \"%s\"", key);
        float faces[FACE_COUNT]; _lua_object_get_friction(root, faces);
        _lua_object_push_number_per_face(L, faces, root);
        goto return_true;
    } else if (strcmp(key, LUA_OBJECT_FIELD_BOUNCINESS) == 0) {
        Transform *root; GET_OBJECT_ROOT_TRANSFORM_WITH_FIELD_ERROR(1, root, "can't get destroyed Object field: \"%s\"", key);
        float faces[FACE_COUNT]; _lua_object_get_bounciness(root, faces);
        _lua_object_push_number_per_face(L, faces, root);
        goto return_true;
    } else if (strcmp(key, LUA_OBJECT_FIELD_APPLYFORCE) == 0) {
        lua_pushcfunction(L, _object_applyForce, "");
        goto return_true;
    } else if (strcmp(key, LUA_OBJECT_FIELD_ONCOLLISIONBEGIN) == 0) {
        Transform *root; GET_OBJECT_ROOT_TRANSFORM_WITH_FIELD_ERROR(1, root, "can't get destroyed Object field: \"%s\"", key);
        lua_g_object_storage_get_field(L, transform_get_id(root), key);
        goto return_true;
    } else if (strcmp(key, LUA_OBJECT_FIELD_ONCOLLISION) == 0) {
        Transform *root; GET_OBJECT_ROOT_TRANSFORM_WITH_FIELD_ERROR(1, root, "can't get destroyed Object field: \"%s\"", key);
        lua_g_object_storage_get_field(L, transform_get_id(root), key);
        goto return_true;
    } else if (strcmp(key, LUA_OBJECT_FIELD_ONCOLLISIONEND) == 0) {
        Transform *root; GET_OBJECT_ROOT_TRANSFORM_WITH_FIELD_ERROR(1, root, "can't get destroyed Object field: \"%s\"", key);
        lua_g_object_storage_get_field(L, transform_get_id(root), key);
        goto return_true;
    } else if (strcmp(key, LUA_OBJECT_FIELD_ISONGROUND) == 0) {
        Transform *root; GET_OBJECT_ROOT_TRANSFORM_WITH_FIELD_ERROR(1, root, "can't get destroyed Object field: \"%s\"", key);
        RigidBody *rb = transform_get_rigidbody(root);
        if (rb == nullptr) {
            lua_pushboolean(L, false);
            goto return_true;
        }

        CGame *g = nullptr;
        if (lua_utils_getGamePtr(L, &g) == false || g == nullptr) {
            LUAUTILS_INTERNAL_ERROR(L);
        }
        Scene *sc = game_get_scene(g);

        // Use a box based on object's current collider
        Box collider; transform_get_or_compute_world_aligned_collider(root, &collider, true);

        // expand box below
        collider.min.y -= EPSILON_CONTACT;
        // reduce box on all other sides
        collider.max.x -= EPSILON_CONTACT;
        collider.max.y -= EPSILON_CONTACT;
        collider.max.z -= EPSILON_CONTACT;
        collider.min.x += EPSILON_CONTACT;
        collider.min.z += EPSILON_CONTACT;

        // Perform box overlap query, then exclude self and trigger objects from results
        bool anyHit = false;
        FifoList *query = fifo_list_new();
        if (scene_overlap_box(sc, &collider,
                              rigidbody_get_groups(rb),
                              rigidbody_get_collides_with(rb),
                              nullptr,
                              query) > 0) {

            OverlapResult *overlap = static_cast<OverlapResult *>(fifo_list_pop(query));
            while (overlap != nullptr && anyHit == false) {
                if (overlap->hitTr != root) {
                    anyHit = rigidbody_is_dynamic(transform_get_rigidbody(overlap->hitTr))
                        || rigidbody_is_static(transform_get_rigidbody(overlap->hitTr));
                }
                free(overlap);
                overlap = static_cast<OverlapResult *>(fifo_list_pop(query));
            }
        }
        fifo_list_free(query, free);

        lua_pushboolean(L, anyHit);
        goto return_true;
    } else if (strcmp(key, LUA_OBJECT_FIELD_TEXTBUBBLE) == 0) {
        // prototype:
        // function(self, text, <duration>, <offset>, <color>, <bgColor>, <tail>)
        // but all parameters except `text` are optional
        // checking types to associate parameters.
        
        if(luau_dostring(L, R"""(
return function(self, text, ...)
    if __textBubbleModule == nil then __textBubbleModule = require("textbubbles") end
        
    local duration = nil
    local offset = nil
    local color = nil
    local bgColor = nil
    local tail = nil
        
    local args = {...}
        
    for i,v in ipairs(args) do
        if duration == nil and type(v) == "number" then
            duration = v
        elseif offset == nil and typeof(v) == "Number3" then
            offset = v
        elseif color == nil and typeof(v) == "Color" then
            color = v
        elseif bgColor == nil and typeof(v) == "Color" then
            bgColor = v
        elseif tail == nil and type(v) == "boolean" then
            tail = v
        end
    end

    if args[1] == -1 then
        duration = -1
    end
        
    local h = 0
    if typeof(self) == "Player" then
        h = 35
    elseif typeof(self) == "Shape" or typeof(self) == "MutableShape" then
        h = self.BoundingBox.Max.Y or 0
        if self.Pivot then
            h = h - self.Pivot.Y
        end
        h = h + 1
    end

    local tailOrDefault = tail ~= nil and tail or false

    __textBubbleModule.set(self, text, duration or 3, offset or {0, h, 0}, color, bgColor, tailOrDefault)
end
        )""", "Object.TextBubble") == false) {
            goto return_false;
        }

        goto return_true;
    } else if (strcmp(key, LUA_OBJECT_FIELD_CLEARTEXTBUBBLE) == 0) {

        if (luau_dostring(L, R"""(
return function(self)
    if __textBubbleModule == nil then __textBubbleModule = require("textbubbles") end
    __textBubbleModule.clear(self)
end
        )""", "Object.ClearTextBubble") == false) {
            goto return_false;
        }

        goto return_true;
    } else if (strcmp(key, LUA_OBJECT_FIELD_TICK) == 0) {
        Transform *t; GET_OBJECT_ROOT_TRANSFORM_WITH_FIELD_ERROR(1, t, "can't get destroyed Object field: \"%s\"", key);
        lua_g_object_storage_get_field(L, transform_get_id(t), key);
        goto return_true;
    } else if (strcmp(key, LUA_OBJECT_FIELD_AWAKE) == 0) {
        lua_pushcfunction(L, _object_awake, "");
        goto return_true;
    } else if (strcmp(key, LUA_OBJECT_FIELD_REFRESH) == 0) {
        lua_pushcfunction(L, _object_refresh, "");
        goto return_true;
    } else if (strcmp(key, LUA_OBJECT_FIELD_RESETCOLLIDER) == 0) {
        lua_pushcfunction(L, _object_resetCollider, "");
        goto return_true;
    } else if (strcmp(key, LUA_OBJECT_FIELD_FIND_FIRST) == 0) {
        lua_pushcfunction(L, _object_find_first, "");
        goto return_true;
    } else if (strcmp(key, LUA_OBJECT_FIELD_FIND) == 0) {
        lua_pushcfunction(L, _object_find, "");
        goto return_true;
    } else if (strcmp(key, LUA_OBJECT_FIELD_RECURSE) == 0) {
        lua_pushcfunction(L, _object_recurse, "");
        goto return_true;
    } else if (strcmp(key, LUA_OBJECT_FIELD_COPY) == 0) {
        lua_pushcfunction(L, _object_copy, "_object_copy");
        goto return_true;
    } else if (strcmp(key, LUA_OBJECT_FIELD_DESTROY) == 0) {
        lua_pushcfunction(L, _object_destroy, "");
        goto return_true;
    } else if (strcmp(key, LUA_OBJECT_FIELD_IS_DESTROYED) == 0) {
        Transform *t; LUA_ITEM_TYPE type = lua_object_getTransformPtr(L, 1, &t);
        lua_pushboolean(L, t == nullptr && type == ITEM_TYPE_OBJECT_DESTROYED);
        goto return_true;
    } else {
        goto return_false;
    }

return_true:
    lua_remove(L, -2); // remove metatable
    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
    return true;

return_false:
    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
    lua_remove(L, -1); // remove metatable
    return false;
}

/// returns true if a key has been set, otherwise key may be checked by extended types
bool _lua_object_metatable_newindex(lua_State *L, Transform *root, const char *key) {

    if (lua_getmetatable(L, 1) == 0) {
        LUAUTILS_INTERNAL_ERROR(L);
    } // metatable at -1

    bool readOnly = _lua_object_is_readonly(L, false);

    if (lua_utils_isStringStrictStartingWithUppercase(L, 2) == false) {
        Transform *t; GET_OBJECT_ROOT_TRANSFORM_WITH_FIELD_ERROR(1, t, "can't set destroyed Object field: \"%s\"", key);
        lua_g_object_storage_set_field(L, transform_get_id(t), key, 3);
        goto return_did_set;
    } else if (strcmp(key, LUA_OBJECT_FIELD_POSITION) == 0) {
        _object_storage_set_or_create_number3(L, LUA_OBJECT_FIELD_POSITION, 3,
                                              readOnly ? nullptr : _lua_object_set_position_handler,
                                              _lua_object_get_position_handler,
                                              [](Transform *t, float3 *out){ *out = *transform_get_position(t, true); });
        goto return_did_set;
    } else if (strcmp(key, LUA_OBJECT_FIELD_LOCALPOSITION) == 0) {
        _object_storage_set_or_create_number3(L, LUA_OBJECT_FIELD_LOCALPOSITION, 3,
                                              readOnly ? nullptr : _lua_object_set_local_position_handler,
                                              _lua_object_get_local_position_handler,
                                              [](Transform *t, float3 *out){ *out = *transform_get_local_position(t, true); });
        goto return_did_set;
    } else if (strcmp(key, LUA_OBJECT_FIELD_ROTATION) == 0) {
        _object_storage_set_or_create_rotation(L, LUA_OBJECT_FIELD_ROTATION, 3,
                                               readOnly ? nullptr : _lua_object_set_rotation_handler,
                                               _lua_object_get_rotation_handler,
                                               &transform_get_rotation);
        goto return_did_set;
    } else if (strcmp(key, LUA_OBJECT_FIELD_LOCALROTATION) == 0) {
        _object_storage_set_or_create_rotation(L, LUA_OBJECT_FIELD_LOCALROTATION, 3,
                                               readOnly ? nullptr : _lua_object_set_local_rotation_handler,
                                               _lua_object_get_local_rotation_handler,
                                               &transform_get_local_rotation);
        goto return_did_set;
    } else if (strcmp(key, LUA_OBJECT_FIELD_SCALE) == 0 ||
               strcmp(key, LUA_OBJECT_FIELD_LOCALSCALE) == 0) {
        const bool uniform = lua_utils_isNumberStrict(L, 3);
        if (uniform) {
            const float scale = lua_tonumber(L, 3);
            _object_storage_get_or_create_number3(L, LUA_OBJECT_FIELD_LOCALSCALE,
                                                  readOnly ? nullptr : _lua_object_set_localscale_handler,
                                                  _lua_object_get_localscale_handler,
                                                  [](Transform *t, float3 *out){ *out = *transform_get_local_scale(t); });
            lua_number3_setXYZ(L, -1, &scale, &scale, &scale);
            lua_pop(L, 1);
        } else {
            _object_storage_set_or_create_number3(L, LUA_OBJECT_FIELD_LOCALSCALE, 3,
                                                  readOnly ? nullptr : _lua_object_set_localscale_handler,
                                                  _lua_object_get_localscale_handler,
                                                  [](Transform *t, float3 *out){ *out = *transform_get_local_scale(t); });
        }
        goto return_did_set;
    } else if (strcmp(key, LUA_OBJECT_FIELD_LOSSYSCALE) == 0) {
        LUAUTILS_ERROR_F(L, "Object.%s cannot be set (field is read-only)", key);
    }  else if (strcmp(key, LUA_OBJECT_FIELD_RIGHT) == 0) {
        _object_storage_set_or_create_number3(L, LUA_OBJECT_FIELD_RIGHT, 3,
                                              readOnly ? nullptr : _lua_object_set_right_handler,
                                              _lua_object_get_right_handler,
                                              [](Transform *t, float3 *out){ transform_get_right(t, out, true); });
        goto return_did_set;
    } else if (strcmp(key, LUA_OBJECT_FIELD_UP) == 0) {
        _object_storage_set_or_create_number3(L, LUA_OBJECT_FIELD_UP, 3,
                                              readOnly ? nullptr : _lua_object_set_up_handler,
                                              _lua_object_get_up_handler,
                                              [](Transform *t, float3 *out){ transform_get_up(t, out, true); });
        goto return_did_set;
    } else if (strcmp(key, LUA_OBJECT_FIELD_FORWARD) == 0) {
        _object_storage_set_or_create_number3(L, LUA_OBJECT_FIELD_FORWARD, 3,
                                              readOnly ? nullptr : _lua_object_set_forward_handler,
                                              _lua_object_get_forward_handler,
                                              [](Transform *t, float3 *out){ transform_get_forward(t, out, true); });
        goto return_did_set;
    } else if (strcmp(key, LUA_OBJECT_FIELD_LEFT) == 0) {
        _object_storage_set_or_create_number3(L, LUA_OBJECT_FIELD_LEFT, 3,
                                              readOnly ? nullptr : _lua_object_set_left_handler,
                                              _lua_object_get_left_handler,
                                              [](Transform *t, float3 *out){ transform_utils_get_left(t, out, true); });
        goto return_did_set;
    } else if (strcmp(key, LUA_OBJECT_FIELD_DOWN) == 0) {
        _object_storage_set_or_create_number3(L, LUA_OBJECT_FIELD_DOWN, 3,
                                              readOnly ? nullptr : _lua_object_set_down_handler,
                                              _lua_object_get_down_handler,
                                              [](Transform *t, float3 *out){ transform_utils_get_down(t, out, true); });
        goto return_did_set;
    } else if (strcmp(key, LUA_OBJECT_FIELD_BACKWARD) == 0) {
        _object_storage_set_or_create_number3(L, LUA_OBJECT_FIELD_BACKWARD, 3,
                                              readOnly ? nullptr : _lua_object_set_backward_handler,
                                              _lua_object_get_backward_handler,
                                              [](Transform *t, float3 *out){ transform_utils_get_backward(t, out, true); });
        goto return_did_set;
    } else if (strcmp(key, LUA_OBJECT_FIELD_PHYSICS) == 0) {
        if (lua_isboolean(L, 3)) {
            _lua_object_set_physics(root, lua_toboolean(L, 3) ? RigidbodyMode_Dynamic : RigidbodyMode_Static);
        } else if (lua_utils_getObjectType(L, 3) == ITEM_TYPE_PHYSICSMODE) {
            _lua_object_set_physics(root, lua_physicsMode_get(L, 3));
        } else {
            LUAUTILS_ERROR_F(L, "Object.%s cannot be set (new value is not a PhysicsMode)", key);
        }
        goto return_did_set;
    } else if (strcmp(key, LUA_OBJECT_FIELD_VELOCITY) == 0) {
        _object_storage_set_or_create_number3(L, LUA_OBJECT_FIELD_VELOCITY, 3,
                                              readOnly ? nullptr : _lua_object_set_velocity_handler,
                                              _lua_object_get_velocity_handler,
                                              [](Transform *t, float3 *out){ if (transform_utils_get_velocity(t) != nullptr) *out = *transform_utils_get_velocity(t); });
        goto return_did_set;
    } else if (strcmp(key, LUA_OBJECT_FIELD_MOTION) == 0) {
        _object_storage_set_or_create_number3(L, LUA_OBJECT_FIELD_MOTION, 3,
                                              readOnly ? nullptr : _lua_object_set_motion_handler,
                                              _lua_object_get_motion_handler,
                                              [](Transform *t, float3 *out){ if (transform_utils_get_motion(t) != nullptr) *out = *transform_utils_get_motion(t); });
        goto return_did_set;
    } else if (strcmp(key, LUA_OBJECT_FIELD_ACCELERATION) == 0) {
        _object_storage_set_or_create_number3(L, LUA_OBJECT_FIELD_ACCELERATION, 3,
                                              readOnly ? nullptr : _lua_object_set_acceleration_handler,
                                              _lua_object_get_acceleration_handler,
                                              [](Transform *t, float3 *out){ if (transform_utils_get_acceleration(t) != nullptr) *out = *transform_utils_get_acceleration(t); });
        goto return_did_set;
    } else if (strcmp(key, LUA_OBJECT_FIELD_ISHIDDEN) == 0) {
        if (lua_isboolean(L, 3)) {
            transform_set_hidden_branch(root, lua_toboolean(L, 3));
        } else {
            LUAUTILS_ERROR_F(L, "Object.%s cannot be set (new value is not a boolean)", key);
        }
        goto return_did_set;
    } else if (strcmp(key, LUA_OBJECT_FIELD_ISHIDDENSELF) == 0) {
        if (lua_isboolean(L, 3)) {
            transform_set_hidden_self(root, lua_toboolean(L, 3));
        } else {
            LUAUTILS_ERROR_F(L, "Object.%s cannot be set (new value is not a boolean)", key);
        }
        goto return_did_set;
    } else if (strcmp(key, LUA_OBJECT_FIELD_NAME) == 0) {
        if (lua_isstring(L, 3)) {
            transform_set_name(root, lua_tostring(L, 3));
        } else if (lua_isnil(L, 3)) {
            transform_set_name(root, nullptr);
        } else {
            LUAUTILS_ERROR_F(L, "Object.%s cannot be set (new value is not a string)", key);
        }
        goto return_did_set;
    } else if (strcmp(key, LUA_OBJECT_FIELD_COLLISIONBOX) == 0) {
        Box newCollider = box_zero;
        if (lua_box_get_box(L, 3, &newCollider) == false) {
            LUAUTILS_ERROR_F(L, "Object.%s cannot be set (new value is not a Box)", key);
        }
        RigidBody *rb = _lua_object_ensure_rigidbody(root);
        vx_assert(rb != nullptr);
        // set the new box using
        rigidbody_set_collider(rb, &newCollider, true);
        goto return_did_set;
    } else if (strcmp(key, LUA_OBJECT_FIELD_COLLISIONGROUPSMASK) == 0) {
        if (lua_isnumber(L, 3) == false) {
            LUAUTILS_ERROR_F(L, "Object.%s cannot be set (new value is not a number)", key);
        }
        _lua_object_set_groups(root, static_cast<uint8_t>(lua_tonumber(L, 3)));
        goto return_did_set;
    } else if (strcmp(key, LUA_OBJECT_FIELD_COLLIDESWITHMASK) == 0) {
        if (lua_isnumber(L, 3) == false) {
            LUAUTILS_ERROR_F(L, "Object.%s cannot be set (new value is not a number)", key);
        }
        _lua_object_set_collides_with(root, static_cast<uint8_t>(lua_tonumber(L, 3)));
        goto return_did_set;
    } else if (strcmp(key, LUA_OBJECT_FIELD_COLLISIONGROUPS) == 0) {
        uint16_t mask = 0;
        if (lua_collision_groups_get_mask(L, 3, &mask) == false) {
            LUAUTILS_ERROR_F(L, "Object.%s cannot be set (new value should be an integer between 1 and %d or CollisionGroups)", key, PHYSICS_GROUP_MASK_API_BITS);
        }
        _lua_object_set_groups(root, mask);
        goto return_did_set;
    } else if (strcmp(key, LUA_OBJECT_FIELD_COLLIDESWITHGROUPS) == 0) {
        uint16_t mask = 0;
        if (lua_collision_groups_get_mask(L, 3, &mask) == false) {
            LUAUTILS_ERROR_F(L, "Object.%s cannot be set (new value should be an integer between 1 and %d or CollisionGroups)", key, PHYSICS_GROUP_MASK_API_BITS);
        }
        _lua_object_set_collides_with(root, mask);
        goto return_did_set;
    } else if (strcmp(key, LUA_OBJECT_FIELD_MASS) == 0) {
        if (lua_isnumber(L, 3) == false) {
            LUAUTILS_ERROR_F(L, "Object.%s cannot be set (new value is not a number)", key);
        }
        _lua_object_set_mass(root, static_cast<float>(lua_tonumber(L, 3)));
        goto return_did_set;
    } else if (strcmp(key, LUA_OBJECT_FIELD_FRICTION) == 0) {
        if (lua_isnumber(L, 3)) {
            for (FACE_INDEX_INT_T i = 0; i < FACE_COUNT; ++i) {
                _lua_object_set_friction(root, i, static_cast<float>(lua_tonumber(L, 3)));
            }
        } else if (lua_istable(L, 3)) {
            float faces[FACE_COUNT]; _lua_object_get_number_per_face(L, 3, faces, FLT_MAX);
            for (FACE_INDEX_INT_T i = 0; i < FACE_COUNT; ++i) {
                if (faces[i] != FLT_MAX) {
                    _lua_object_set_friction(root, i, faces[i]);
                }
            }
        } else {
            LUAUTILS_ERROR_F(L, "Object.%s cannot be set (new value is not a number or table)", key);
        }
        goto return_did_set;
    } else if (strcmp(key, LUA_OBJECT_FIELD_BOUNCINESS) == 0) {
        if (lua_isnumber(L, 3)) {
            for (FACE_INDEX_INT_T i = 0; i < FACE_COUNT; ++i) {
                _lua_object_set_bounciness(root, i, static_cast<float>(lua_tonumber(L, 3)));
            }
        } else if (lua_istable(L, 3)) {
            float faces[FACE_COUNT]; _lua_object_get_number_per_face(L, 3, faces, FLT_MAX);
            for (FACE_INDEX_INT_T i = 0; i < FACE_COUNT; ++i) {
                if (faces[i] != FLT_MAX) {
                    _lua_object_set_bounciness(root, i, faces[i]);
                }
            }
        } else {
            LUAUTILS_ERROR_F(L, "Object.%s cannot be set (new value is not a number or table)", key);
        }
        goto return_did_set;
    } else if (strcmp(key, LUA_OBJECT_FIELD_ONCOLLISIONBEGIN) == 0) {
        const bool isNil = lua_isnil(L, 3);
        if (lua_isfunction(L, 3) == false && isNil == false) {
            LUAUTILS_ERROR_F(L, "Object.%s cannot be set (new value should be a function or nil)", key);
        }
        Transform *t; GET_OBJECT_ROOT_TRANSFORM_WITH_FIELD_ERROR(1, t, "can't set destroyed Object field: \"%s\"", key);
        lua_g_object_storage_set_field(L, transform_get_id(t), key, 3);

        _lua_object_toggle_collision_callback(root, CollisionCallbackType_Begin, isNil == false);
        goto return_did_set;
    } else if (strcmp(key, LUA_OBJECT_FIELD_ONCOLLISION) == 0) {
        const bool isNil = lua_isnil(L, 3);
        if (lua_isfunction(L, 3) == false && isNil == false) {
            LUAUTILS_ERROR_F(L, "Object.%s cannot be set (new value should be a function or nil)", key);
        }
        Transform *t; GET_OBJECT_ROOT_TRANSFORM_WITH_FIELD_ERROR(1, t, "can't set destroyed Object field: \"%s\"", key);
        lua_g_object_storage_set_field(L, transform_get_id(t), key, 3);

        _lua_object_toggle_collision_callback(root, CollisionCallbackType_Tick, isNil == false);
        goto return_did_set;
    } else if (strcmp(key, LUA_OBJECT_FIELD_ONCOLLISIONEND) == 0) {
        const bool isNil = lua_isnil(L, 3);
        if (lua_isfunction(L, 3) == false && isNil == false) {
            LUAUTILS_ERROR_F(L, "Object.%s cannot be set (new value should be a function or nil)", key);
        }
        Transform *t; GET_OBJECT_ROOT_TRANSFORM_WITH_FIELD_ERROR(1, t, "can't set destroyed Object field: \"%s\"", key);
        lua_g_object_storage_set_field(L, transform_get_id(t), key, 3);

        _lua_object_toggle_collision_callback(root, CollisionCallbackType_End, isNil == false);
        goto return_did_set;
    } else if (strcmp(key, LUA_OBJECT_FIELD_TICK) == 0) {
        
        const bool isNil = lua_isnil(L, 3);
        if (lua_isfunction(L, 3) == false && isNil == false) {
            LUAUTILS_ERROR_F(L, "Object.%s cannot be set (new value should be a function or nil)", key);
        }
        Transform *t; GET_OBJECT_ROOT_TRANSFORM_WITH_FIELD_ERROR(1, t, "can't set destroyed Object field: \"%s\"", key);

        uint16_t transformID = transform_get_id(t);
        lua_g_object_storage_get_field(L, transformID, key);
        const bool wasNil = lua_isnil(L, -1);
        lua_pop(L, 1);
        
        lua_g_object_storage_set_field(L, transform_get_id(t), key, 3);

        if (isNil != wasNil) {
            CGame *g = nullptr;
            if (lua_utils_getGamePtr(L, &g) == false || g == nullptr) {
                LUAUTILS_ERROR(L, "cannot access game");
            }
            game_toggle_object_tick(g, t, isNil == false);
        }
        goto return_did_set;
    } else if (strcmp(key, LUA_OBJECT_FIELD_SHADOWCOOKIE) == 0) {
        if (lua_utils_isNumberStrict(L, 3)) {
            transform_set_shadow_decal(root, lua_tonumber(L, 3));
        } else {
            LUAUTILS_ERROR_F(L, "Object.%s cannot be set (new value is not a number)", key);
        }
        goto return_did_set;
    } else if (strcmp(key, LUA_OBJECT_FIELD_ANIMATIONS) == 0) {
        if (lua_getmetatable(L, 1) == 0) {
            LUAUTILS_INTERNAL_ERROR(L);
        }
        lua_pushstring(L, LUA_OBJECT_FIELD_ANIMATIONS);
        lua_pushvalue(L, 3);
        lua_rawset(L, -3);
        lua_pop(L, 1); // pop metatable
        goto return_did_set;
        
    } else if (strcmp(key, LUA_OBJECT_DEBUG) == 0) {
        if (lua_isboolean(L, 3) == false) {
            LUAUTILS_ERROR_F(L, "Object.%s must be a boolean", key);
        }
        Transform *t; GET_OBJECT_ROOT_TRANSFORM_WITH_FIELD_ERROR(1, t, "can't set destroyed Object field: \"%s\"", key);
        debug_transform_set_debug(t, lua_toboolean(L, 3));
    } else {
        goto return_not_set;
    }

return_did_set:
    lua_pop(L, 1); // pop metatable
    return true;

return_not_set:
    lua_pop(L, 1); // pop metatable
    return false;
}

// --------------------------------------------------
// MARK: - Private functions -
// --------------------------------------------------

static int _g_object_metatable_index(lua_State *L) {
    if (L == nullptr) {
        return 0;
    }
    LUAUTILS_STACK_SIZE(L)

    // validate 2nd argument: key
    if (lua_utils_isStringStrict(L, 2) == false) {
        lua_pushnil(L);
        LUAUTILS_STACK_SIZE_ASSERT(L, 1)
        return 1;
    }

    const char *key = lua_tostring(L, 2);
    if (strcmp(key, LUA_G_OBJECT_FIELD_LOAD) == 0) {
        if(luau_dostring(L, R"""(
return function(self, asset, callback, config)
    if self == nil then error("[Object:Load] function should be called with `:`") end
    if asset == nil then error("[Object:Load] asset must be an item name or Data") end
    if callback == nil then error("[Object:Load] expects a callback") end
    local req = Assets:Load(asset, function(assets)
        if assets == nil then callback(nil) return end
        local shapesNotParented = {}
        for _,v in assets do
            if v:GetParent() == nil then
                table.insert(shapesNotParented, v)
            end
        end
        
        local finalObject
        if #shapesNotParented == 1 then
            finalObject = shapesNotParented[1]
        elseif #shapesNotParented > 1 then
            local root = Object()
            for _,v in shapesNotParented do
                root:AddChild(v)
            end
            finalObject = root
        end
        callback(finalObject)
    end, AssetType.AnyObject, config)
    return req
end
        )""", "Object.Load") == false) {
            lua_pushnil(L);
        }
    } else {
        lua_pushnil(L);
    }

    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
    return 1;

}

static int _g_object_metatable_newindex(lua_State *L) {
    RETURN_VALUE_IF_NULL(L, 0);
    LUAUTILS_STACK_SIZE(L)
    // nothing
    LUAUTILS_STACK_SIZE_ASSERT(L, 0);
    return 0;
}

static int _g_object_metatable_call(lua_State *L) {
    RETURN_VALUE_IF_NULL(L, 0)
    LUAUTILS_STACK_SIZE(L)
    
    Transform *t = transform_new(PointTransform);
    if (lua_object_pushNewInstance(L, t) == false) {
        lua_pushnil(L); // GC will release transform
    }
    transform_release(t); // now owned by lua Object

    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
    return 1;
}

static int _object_metatable_index(lua_State *L) {
    vx_assert(L != nullptr);
    LUAUTILS_ASSERT_ARGS_COUNT(L, 2)
    // LUAUTILS_ASSERT_ARG_TYPE(L, 1, ITEM_TYPE_OBJECT)
    LUAUTILS_STACK_SIZE(L)

    const char *key = lua_tostring(L, 2);
    
    if (_lua_object_metatable_index(L, key) == false) {
        // key not found, push nil
        lua_pushnil(L);
    }

    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
    return 1;
}

static int _object_metatable_newindex(lua_State *L) {
    vx_assert(L != nullptr);
    LUAUTILS_ASSERT_ARGS_COUNT(L, 3)
    // LUAUTILS_ASSERT_ARG_TYPE(L, 1, ITEM_TYPE_OBJECT)
    LUAUTILS_STACK_SIZE(L)

    // retrieve underlying Transform struct
    Transform *t; lua_object_getTransformPtr(L, 1, &t);
    if (t == nullptr) {
        lua_pushnil(L);
        LUAUTILS_STACK_SIZE_ASSERT(L, 1)
        return 1;
    }

    const char *key = lua_tostring(L, 2);

    if (_lua_object_metatable_newindex(L, t, key) == false) {
        // key not found
        LUAUTILS_ERROR_F(L, "Object.%s is not settable", key);
    }

    LUAUTILS_STACK_SIZE_ASSERT(L, 0)
    return 0;
}

static void _object_release_transform(void *_ud) {
    object_userdata *ud = static_cast<object_userdata*>(_ud);
    if (ud->transform != nullptr) {
        transform_release(ud->transform);
        ud->transform = nullptr;
    }
}

static void _object_gc(void *_ud) {
    object_userdata *ud = static_cast<object_userdata*>(_ud);
    if (ud->transform != nullptr) {
        transform_release(ud->transform);
        ud->transform = nullptr;
    }

    // disconnect from userdata store
    {
        if (ud->previous != nullptr) {
            ud->previous->next = ud->next;
        }
        if (ud->next != nullptr) {
            ud->next->previous = ud->previous;
        }
        ud->store->removeWithoutKeepingID(ud, ud->next);
    }
}

void lua_object_gc_finalize(vx::UserDataStore *store, CGame *g, lua_State * const L) {

    // Recycle transform IDs, cleaning up associated metadata
    // only when transform are completely gone (not referenced in Lua AND not in scene hierarchy)

    FiloListUInt16Node *firstNode = filo_list_uint16_get_first(game_get_managedTransformIDsToRemove(g));
    if (firstNode == nullptr) { return; }

    if (L != nullptr) {
        LUAUTILS_STACK_SIZE(L)

        lua_g_object_pushObjectsIndexedByTransformID(L);
        FiloListUInt16Node *n = firstNode;
        while (n != nullptr) {
            lua_pushnil(L);
            lua_rawseti(L, -2, filo_list_uint16_node_get_value(n));
            n = filo_list_uint16_node_next(n);
        }
        lua_pop(L, 1);

        lua_g_object_pushObjectStorage(L);
        n = firstNode;
        while (n != nullptr) {
            lua_pushnil(L);
            lua_rawseti(L, -2, filo_list_uint16_node_get_value(n));
            n = filo_list_uint16_node_next(n);
        }
        lua_pop(L, 1);

        LUAUTILS_STACK_SIZE_ASSERT(L, 0);
    }

    game_recycle_managedTransformIDsToRemove(g);
}

void lua_g_object_pushObjectStorage(lua_State *L) {
    vx_assert(L != nullptr);
    LUAUTILS_STACK_SIZE(L)
    lua_pushstring(L, REGISTRY_OBJECTS_STORAGE);
    lua_rawget(L, LUA_REGISTRYINDEX);
    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
}

void lua_g_object_storage_set_field(lua_State *L, const uint16_t transformID, const char *name, const int valueIdx) {
    vx_assert(L != nullptr);
    LUAUTILS_STACK_SIZE(L)

    lua_g_object_pushObjectStorage(L);

    // Object storages: -1

    LUAUTILS_STACK_SIZE_ASSERT(L, 1);

    // get storage table for given transform
    lua_rawgeti(L, -1, transformID);

    // object storage table: -1
    // Object storages: -2

    if (lua_isnil(L, -1)) {
        lua_pop(L, 1); // pop nil
        // create storage table
        lua_newtable(L);
        lua_pushvalue(L, -1); // duplicate storage table, popped by lua_rawseti

        // object storage table: -1
        // object storage table: -2
        // Object storages: -3

        lua_rawseti(L, -3, transformID);
    }

    // object storage table: -1
    // Object storages: -2

    vx_assert(lua_istable(L, -1));

    // Set field in sub-table
    lua_pushstring(L, name);
    // field name: -1
    // object storage table: -2
    // Object storages: -3
    lua_pushvalue(L, valueIdx < 0 ? valueIdx - 3 : valueIdx);
    lua_rawset(L, -3);

    lua_pop(L, 2); // pop storage table & container

    LUAUTILS_STACK_SIZE_ASSERT(L, 0);
}

void lua_g_object_storage_get_field(lua_State *L, const uint16_t transformID, const char *name) {
    vx_assert(L != nullptr);
    LUAUTILS_STACK_SIZE(L)

    lua_g_object_pushObjectStorage(L);

    // get storage table for given transform
    lua_rawgeti(L, -1, transformID);

    lua_remove(L, -2); // remove global storage

    if (lua_isnil(L, -1)) {
        LUAUTILS_STACK_SIZE_ASSERT(L, 1);
        return;
    }

    lua_pushstring(L, name);
    lua_rawget(L, -2);

    lua_remove(L, -2); // remove storage table
    LUAUTILS_STACK_SIZE_ASSERT(L, 1);
}

static void _object_storage_get_or_create_number3(lua_State *L, const char* name,
                                                  number3_C_handler_set set, number3_C_handler_get get,
                                                  pointer_transform_get_float3_value_func initValue) {
    vx_assert(L != nullptr);
    LUAUTILS_STACK_SIZE(L)

    Transform *t; GET_OBJECT_ROOT_TRANSFORM_WITH_FIELD_ERROR(1, t, "can't get destroyed Object field: \"%s\"", name);
    const uint16_t id = transform_get_id(t);

    lua_g_object_storage_get_field(L, id, name);

    if (lua_utils_getObjectType(L, -1) == ITEM_TYPE_NUMBER3) {
        LUAUTILS_STACK_SIZE_ASSERT(L, 1);
        return;
    }

    lua_pop(L, 1); // nil or wrong type, pop and create Number3

    float3 pos = float3_zero;
    if (initValue != nullptr) {
        initValue(t, &pos);
    }

    // Create new Number3
    lua_number3_pushNewObject(L, pos.x, pos.y, pos.z);
    number3_C_handler_userdata userdata = number3_C_handler_userdata_zero;
    userdata.ptr = transform_get_and_retain_weakptr(t);
    lua_number3_setHandlers(L, -1, set, get, userdata, true);

    // insert in storage
    lua_g_object_storage_set_field(L, id, name, -1);

    LUAUTILS_STACK_SIZE_ASSERT(L, 1);
}

static void _object_storage_set_or_create_number3(lua_State *L, const char* name, const int idx,
                                                  number3_C_handler_set set, number3_C_handler_get get,
                                                  pointer_transform_get_float3_value_func initValue) {
    vx_assert(L != nullptr);
    LUAUTILS_STACK_SIZE(L)

    float3 f3;
    if (lua_number3_or_table_getXYZ(L, idx, &f3.x, &f3.y, &f3.z)) {
        _object_storage_get_or_create_number3(L, name, set, get, initValue);
        lua_number3_setXYZ(L, -1, &f3.x, &f3.y, &f3.z);
        lua_pop(L, 1);
    } else {
        LUAUTILS_ERROR_F(L, "Object.%s cannot be set (new value should be a Number3 or table of 3 numbers)", name);
    }

    LUAUTILS_STACK_SIZE_ASSERT(L, 0);
}

static void _object_storage_get_or_create_rotation(lua_State *L, const char* name,
                                                   rotation_handler_set set, rotation_handler_get get,
                                                   pointer_transform_get_quaternion_func init) {
    vx_assert(L != nullptr);
    LUAUTILS_STACK_SIZE(L)

    Transform *t; GET_OBJECT_ROOT_TRANSFORM_WITH_FIELD_ERROR(1, t, "can't get destroyed Object field: \"%s\"", name);
    const uint16_t id = transform_get_id(t);

    lua_g_object_storage_get_field(L, id, name);

    if (lua_utils_getObjectType(L, -1) == ITEM_TYPE_ROTATION) {
        LUAUTILS_STACK_SIZE_ASSERT(L, 1);
        return;
    }

    lua_pop(L, 1); // nil or wrong type, pop and create Number3

    lua_rotation_push_new(L, *init(t));
    rotation_handler_userdata userdata = { transform_get_and_retain_weakptr(t) };
    lua_rotation_set_handlers(L, -1, set, get, userdata, true);

    // insert in storage
    lua_g_object_storage_set_field(L, id, name, -1);

    LUAUTILS_STACK_SIZE_ASSERT(L, 1);
}

static void _object_storage_set_or_create_rotation(lua_State *L, const char* name, const int idx,
                                                   rotation_handler_set set, rotation_handler_get get,
                                                   pointer_transform_get_quaternion_func init) {
    vx_assert(L != nullptr);
    LUAUTILS_STACK_SIZE(L)

    Quaternion q;
    if (lua_rotation_or_table_get(L, idx, &q)) {
        _object_storage_get_or_create_rotation(L, name, set, get, init);
        lua_rotation_set(L, -1, &q);
        lua_pop(L, 1);
    } else {
        float3 euler;
        if (lua_number3_getXYZ(L, idx, &euler.x, &euler.y, &euler.z)) {
            _object_storage_get_or_create_rotation(L, name, set, get, init);
            lua_rotation_set_euler(L, -1, &euler.x, &euler.y, &euler.z);
            lua_pop(L, 1);
        } else {
            LUAUTILS_ERROR_F(L, "Object.%s cannot be set (new value should be a Rotation, a Number3 or table of 3 numbers)", name);
        }
    }

    LUAUTILS_STACK_SIZE_ASSERT(L, 0);
}

void _lua_object_get_number_per_face(lua_State *L, const int idx, float faces[FACE_COUNT], float nullValue) {
    vx_assert(L != NULL);
    vx_assert(faces != NULL);
    LUAUTILS_STACK_SIZE(L)

    lua_pushstring(L, "other");
    lua_rawget(L, idx);
    const float other = lua_isnumber(L, -1) ? lua_tonumber(L, -1) : nullValue;
    lua_pop(L, 1);

    for (FACE_INDEX_INT_T i = 0; i < FACE_COUNT; ++i) {
        lua_pushstring(L, number_per_face_keys[i]);
        lua_rawget(L, idx);
        faces[i] = lua_isnumber(L, -1) ? lua_tonumber(L, -1) : other;
        lua_pop(L, 1);
    }

    LUAUTILS_STACK_SIZE_ASSERT(L, 0)
}

void _lua_object_push_number_per_face(lua_State *L, const float faces[FACE_COUNT], Transform *t) {
    vx_assert(L != NULL);
    vx_assert(faces != NULL);
    LUAUTILS_STACK_SIZE(L)

    bool all = true;
    for (FACE_INDEX_INT_T i = 1; all && i < FACE_COUNT; ++i) {
        all &= float_isEqual(faces[0], faces[i], EPSILON_ZERO);
    }

    if (all) {
        lua_pushnumber(L, faces[0]);
    } else {
        lua_newtable(L);
        for (FACE_INDEX_INT_T i = 0; i < FACE_COUNT; ++i) {
            lua_pushstring(L, number_per_face_keys[i]);
            lua_pushnumber(L, faces[i]);
            lua_rawset(L, -3);
        }
        lua_newtable(L); // metatable
        {
            lua_pushstring(L, "__tostring");
            lua_pushcfunction(L, _lua_object_number_per_face_tostring, "");
            lua_rawset(L, -3);

            lua_pushstring(L, "__metatable");
            lua_pushboolean(L, false);
            lua_rawset(L, -3);
        }
        lua_setmetatable(L, -2);
    }

    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
}

static int _lua_object_number_per_face_tostring(lua_State *L) {
    vx_assert(L != nullptr);
    LUAUTILS_STACK_SIZE(L)

    const char* keys[FACE_COUNT] = { "right=%.2f", "left=%.2f", "front=%.2f", "back=%.2f", "top=%.2f", "bottom=%.2f" };

    float faces[FACE_COUNT]; _lua_object_get_number_per_face(L, 1, faces, FLT_MAX);
    std::string out = "{ ";
    for (FACE_INDEX_INT_T i = 0; i < FACE_COUNT; ++i) {
        if (faces[i] != FLT_MAX) {
            if (i > 0) {
                out += ", ";
            }
            out += keys[i];
        }
    }
    out += " }";

    const int buff_size = 1 + snprintf(nullptr, 0, out.c_str(), faces[0], faces[1], faces[2], faces[3], faces[4], faces[5]);
    std::string str(buff_size, '\0');
    snprintf(&str[0],
             buff_size,
             out.c_str(),
             static_cast<double>(faces[0]),
             static_cast<double>(faces[1]),
             static_cast<double>(faces[2]),
             static_cast<double>(faces[3]),
             static_cast<double>(faces[4]),
             static_cast<double>(faces[5]));
    lua_pushlstring(L, str.c_str(), buff_size);

    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
    return 1;
}

Transform* _lua_object_get_transform_from_number3_handler(const number3_C_handler_userdata *userdata) {
    return static_cast<Transform*>(weakptr_get(static_cast<Weakptr*>(userdata->ptr)));
}

Transform* _lua_object_get_transform_from_rotation_handler(const rotation_handler_userdata *userdata) {
    return static_cast<Transform*>(weakptr_get(static_cast<Weakptr*>(userdata->ptr)));
}

void _lua_object_set_position(Transform *t, const float *x, const float *y, const float *z) {
    float3 pos;
    float3_copy(&pos, transform_get_position(t, true));

    if (x != nullptr) { pos.x = *x; }
    if (y != nullptr) { pos.y = *y; }
    if (z != nullptr) { pos.z = *z; }

    transform_set_position_vec(t, &pos); // setting transform only once may reduce unneeded refreshes
    _lua_object_non_kinematic_reset(t);
}

void _lua_object_get_position(Transform *t, float *x, float *y, float *z) {
    const float3 *pos = transform_get_position(t, true);

    if (x != nullptr) { *x = pos->x; }
    if (y != nullptr) { *y = pos->y; }
    if (z != nullptr) { *z = pos->z; }
}

void _lua_object_set_position_handler(const float *x, const float *y, const float *z, lua_State *L, const number3_C_handler_userdata *userdata) {
    Transform *t = _lua_object_get_transform_from_number3_handler(userdata);
    if (t == nullptr) {
        LUAUTILS_ERROR(L, "[Object.Position] original Object does not exist anymore"); // returns
    }
    
    _lua_object_set_position(t, x, y, z);
}

void _lua_object_get_position_handler(float *x, float *y, float *z, lua_State *L, const number3_C_handler_userdata *userdata) {
    Transform *t = _lua_object_get_transform_from_number3_handler(userdata);
    if (t == nullptr) {
        LUAUTILS_ERROR(L, "[Object.Position] original Object does not exist anymore"); // returns
    }

    _lua_object_get_position(t, x, y, z);
}

void _lua_object_set_local_position(Transform *t, const float *x, const float *y, const float *z) {
    float3 pos; float3_copy(&pos, transform_get_local_position(t, true));

    if (x != nullptr) { pos.x = *x; }
    if (y != nullptr) { pos.y = *y; }
    if (z != nullptr) { pos.z = *z; }

    transform_set_local_position_vec(t, &pos); // setting transform only once may reduce unneeded refreshes
    _lua_object_non_kinematic_reset(t);
}

void _lua_object_get_local_position(Transform *t, float *x, float *y, float *z) {
    const float3 *pos = transform_get_local_position(t, true);

    if (x != nullptr) { *x = pos->x; }
    if (y != nullptr) { *y = pos->y; }
    if (z != nullptr) { *z = pos->z; }
}

void _lua_object_set_local_position_handler(const float *x, const float *y, const float *z, lua_State *L, const number3_C_handler_userdata *userdata) {
    Transform *t = _lua_object_get_transform_from_number3_handler(userdata);
    if (t == nullptr) {
        LUAUTILS_ERROR(L, "[Object.LocalPosition] original Object does not exist anymore"); // returns
    }

    _lua_object_set_local_position(t, x, y, z);
}

void _lua_object_get_local_position_handler(float *x, float *y, float *z, lua_State *L, const number3_C_handler_userdata *userdata) {
    Transform *t = _lua_object_get_transform_from_number3_handler(userdata);
    if (t == nullptr) {
        LUAUTILS_ERROR(L, "[Object.LocalPosition] original Object does not exist anymore"); // returns
    }

    _lua_object_get_local_position(t, x, y, z);
}

void _lua_object_set_rotation_handler(Quaternion *q, lua_State *L, const rotation_handler_userdata *userdata) {
    Transform *t = _lua_object_get_transform_from_rotation_handler(userdata);
    if (t == nullptr) {
        LUAUTILS_ERROR(L, "[Object.Rotation] original Object does not exist anymore"); // returns
    }

    transform_set_rotation(t, q);
}

void _lua_object_get_rotation_handler(Quaternion *q, lua_State *L, const rotation_handler_userdata *userdata) {
    Transform *t = _lua_object_get_transform_from_rotation_handler(userdata);
    if (t == nullptr) {
        LUAUTILS_ERROR(L, "[Object.Rotation] original Object does not exist anymore"); // returns
    }

    quaternion_set(q, transform_get_rotation(t));
}

void _lua_object_set_local_rotation_handler(Quaternion *q, lua_State *L, const rotation_handler_userdata *userdata) {
    Transform *t = _lua_object_get_transform_from_rotation_handler(userdata);
    if (t == nullptr) {
        LUAUTILS_ERROR(L, "[Object.LocalRotation] original Object does not exist anymore"); // returns
    }

    transform_set_local_rotation(t, q);
}

void _lua_object_get_local_rotation_handler(Quaternion *q, lua_State *L, const rotation_handler_userdata *userdata) {
    Transform *t = _lua_object_get_transform_from_rotation_handler(userdata);
    if (t == nullptr) {
        LUAUTILS_ERROR(L, "[Object.LocalRotation] original Object does not exist anymore"); // returns
    }

    quaternion_set(q, transform_get_local_rotation(t));
}

void _lua_object_set_scale(Transform *t, const float *x, const float *y, const float *z) {
    float3 scale; float3_copy(&scale, transform_get_local_scale(t));
    
    if (x != nullptr) { scale.x = *x; }
    if (y != nullptr) { scale.y = *y; }
    if (z != nullptr) { scale.z = *z; }
    
    transform_set_local_scale_vec(t, &scale); // setting transform only once may reduce unneeded refreshes
    _lua_object_non_kinematic_reset(t);
}

void _lua_object_get_scale(Transform *t, float *x, float *y, float *z) {
    const float3 *scale = transform_get_local_scale(t);
    
    if (x != nullptr) { *x = scale->x; }
    if (y != nullptr) { *y = scale->y; }
    if (z != nullptr) { *z = scale->z; }
}

void _lua_object_set_localscale_handler(const float *x, const float *y, const float *z, lua_State *L, const number3_C_handler_userdata *userdata) {
    Transform *t = _lua_object_get_transform_from_number3_handler(userdata);
    if (t == nullptr) {
        LUAUTILS_ERROR(L, "[Object.LocalScale] original Object does not exist anymore"); // returns
    }

    _lua_object_set_scale(t, x, y, z);
}

void _lua_object_get_localscale_handler(float *x, float *y, float *z, lua_State *L, const number3_C_handler_userdata *userdata) {
    Transform *t = _lua_object_get_transform_from_number3_handler(userdata);
    if (t == nullptr) {
        LUAUTILS_ERROR(L, "[Object.LocalScale] original Object does not exist anymore"); // returns
    }

    _lua_object_get_scale(t, x, y, z);
}

void _lua_object_get_lossyscale(Transform *t, float *x, float *y, float *z) {
    float3 scale; transform_get_lossy_scale(t, &scale, true);
    
    if (x != nullptr) { *x = scale.x; }
    if (y != nullptr) { *y = scale.y; }
    if (z != nullptr) { *z = scale.z; }
}

void _lua_object_get_lossyscale_handler(float *x, float *y, float *z,
                                        lua_State *L,
                                        const number3_C_handler_userdata *userdata) {
    Transform *t = _lua_object_get_transform_from_number3_handler(userdata);
    if (t == nullptr) {
        LUAUTILS_ERROR(L, "[Object.LossyScale] original Object does not exist anymore"); // returns
    }
    
    _lua_object_get_lossyscale(t, x, y, z);
}

void _lua_object_set_right(Transform *t, const float *x, const float *y, const float *z) {
    float3 right;
    if (x != nullptr && y != nullptr && z != nullptr) {
        float3_set(&right, *x, *y, *z);
    }
    // doesn't make much sense to set a unit vector individual components, but if the user does it,
    // let's oblige! we have to re-normalize & pretend it's gonna be the new unit vector
    else {
        transform_get_right(t, &right, true);

        if (x != nullptr) { right.x = *x; }
        if (y != nullptr) { right.y = *y; }
        if (z != nullptr) { right.z = *z; }
    }
    float3_normalize(&right);
    transform_set_right_vec(t, &right);
}

void _lua_object_get_right(Transform *t, float *x, float *y, float *z) {
    float3 right; transform_get_right(t, &right, true);

    if (x != nullptr) { *x = right.x; }
    if (y != nullptr) { *y = right.y; }
    if (z != nullptr) { *z = right.z; }
}

void _lua_object_set_right_handler(const float *x, const float *y, const float *z, lua_State *L, const number3_C_handler_userdata *userdata) {
    Transform *t = _lua_object_get_transform_from_number3_handler(userdata);
    if (t == nullptr) {
        LUAUTILS_ERROR(L, "[Object.Right] original Object does not exist anymore"); // returns
    }

    _lua_object_set_right(t, x, y, z);
}

void _lua_object_get_right_handler(float *x, float *y, float *z, lua_State *L, const number3_C_handler_userdata *userdata) {
    Transform *t = _lua_object_get_transform_from_number3_handler(userdata);
    if (t == nullptr) {
        LUAUTILS_ERROR(L, "[Object.Right] original Object does not exist anymore"); // returns
    }

    _lua_object_get_right(t, x, y, z);
}

void _lua_object_set_up(Transform * const t,
                        const float * const x,
                        const float * const y,
                        const float * const z) {
    float3 up;
    if (x != nullptr && y != nullptr && z != nullptr) {
        float3_set(&up, *x, *y, *z);
    }
    // doesn't make much sense to set a unit vector individual components, but if the user does it,
    // let's oblige! we have to re-normalize & pretend it's gonna be the new unit vector
    else {
        transform_get_up(t, &up, true);

        if (x != nullptr) { up.x = *x; }
        if (y != nullptr) { up.y = *y; }
        if (z != nullptr) { up.z = *z; }
    }
    float3_normalize(&up);
    transform_set_up_vec(t, &up);
}

void _lua_object_get_up(Transform *t, float *x, float *y, float *z) {
    float3 up; transform_get_up(t, &up, true);

    if (x != nullptr) { *x = up.x; }
    if (y != nullptr) { *y = up.y; }
    if (z != nullptr) { *z = up.z; }
}

void _lua_object_set_up_handler(const float * const x,
                                const float * const y,
                                const float * const z,
                                lua_State * const L,
                                const number3_C_handler_userdata * const userdata) {
    Transform *t = _lua_object_get_transform_from_number3_handler(userdata);
    if (t == nullptr) {
        LUAUTILS_ERROR(L, "[Object.Up] original Object does not exist anymore"); // returns
    }
    
    _lua_object_set_up(t, x, y, z);
}

void _lua_object_get_up_handler(float *x, float *y, float *z, lua_State *L, const number3_C_handler_userdata *userdata) {
    Transform *t = _lua_object_get_transform_from_number3_handler(userdata);
    if (t == nullptr) {
        LUAUTILS_ERROR(L, "[Object.Up] original Object does not exist anymore"); // returns
    }

    _lua_object_get_up(t, x, y, z);
}

void _lua_object_set_forward(Transform *t, const float *x, const float *y, const float *z) {
    float3 forward;
    if (x != nullptr && y != nullptr && z != nullptr) {
        float3_set(&forward, *x, *y, *z);
    }
    // doesn't make much sense to set a unit vector individual components, but if the user does it,
    // let's oblige! we have to re-normalize & pretend it's gonna be the new unit vector
    else {
        transform_get_forward(t, &forward, true);

        if (x != nullptr) { forward.x = *x; }
        if (y != nullptr) { forward.y = *y; }
        if (z != nullptr) { forward.z = *z; }
    }
    float3_normalize(&forward);
    transform_set_forward_vec(t, &forward);
}

void _lua_object_get_forward(Transform *t, float *x, float *y, float *z) {
    float3 forward; transform_get_forward(t, &forward, true);

    if (x != nullptr) { *x = forward.x; }
    if (y != nullptr) { *y = forward.y; }
    if (z != nullptr) { *z = forward.z; }
}

void _lua_object_set_forward_handler(const float *x, const float *y, const float *z, lua_State *L, const number3_C_handler_userdata *userdata) {
    Transform *t = _lua_object_get_transform_from_number3_handler(userdata);
    if (t == nullptr) {
        LUAUTILS_ERROR(L, "[Object.Forward] original Object does not exist anymore"); // returns
    }

    _lua_object_set_forward(t, x, y, z);
}

void _lua_object_get_forward_handler(float *x, float *y, float *z, lua_State *L, const number3_C_handler_userdata *userdata) {
    Transform *t = _lua_object_get_transform_from_number3_handler(userdata);
    if (t == nullptr) {
        LUAUTILS_ERROR(L, "[Object.Forward] original Object does not exist anymore"); // returns
    }

    _lua_object_get_forward(t, x, y, z);
}

void _lua_object_set_left_handler(const float *x, const float *y, const float *z, lua_State *L, const number3_C_handler_userdata *userdata) {
    Transform *t = _lua_object_get_transform_from_number3_handler(userdata);
    if (t == nullptr) {
        LUAUTILS_ERROR(L, "[Object.Left] original Object does not exist anymore"); // returns
    }

    // negate inputs
    const bool bx = x != nullptr;
    const bool by = y != nullptr;
    const bool bz = z != nullptr;
    const float nx = bx ? -(*x) : 0.0f;
    const float ny = by ? -(*y) : 0.0f;
    const float nz = bz ? -(*z) : 0.0f;

    _lua_object_set_right(t, bx ? &nx : nullptr, by ? &ny : nullptr, bz ? &nz : nullptr);
}

void _lua_object_get_left_handler(float *x, float *y, float *z, lua_State *L, const number3_C_handler_userdata *userdata) {
    Transform *t = _lua_object_get_transform_from_number3_handler(userdata);
    if (t == nullptr) {
        LUAUTILS_ERROR(L, "[Object.Left] original Object does not exist anymore"); // returns
    }

    _lua_object_get_right(t, x, y, z);

    if (x != nullptr) *x = -(*x);
    if (y != nullptr) *y = -(*y);
    if (z != nullptr) *z = -(*z);
}

void _lua_object_set_down_handler(const float *x, const float *y, const float *z, lua_State *L, const number3_C_handler_userdata *userdata) {
    Transform *t = _lua_object_get_transform_from_number3_handler(userdata);
    if (t == nullptr) {
        LUAUTILS_ERROR(L, "[Object.Down] original Object does not exist anymore"); // returns
    }

    // negate inputs
    const bool bx = x != nullptr;
    const bool by = y != nullptr;
    const bool bz = z != nullptr;
    const float nx = bx ? -(*x) : 0.0f;
    const float ny = by ? -(*y) : 0.0f;
    const float nz = bz ? -(*z) : 0.0f;

    _lua_object_set_up(t, bx ? &nx : nullptr, by ? &ny : nullptr, bz ? &nz : nullptr);
}

void _lua_object_get_down_handler(float *x, float *y, float *z, lua_State *L, const number3_C_handler_userdata *userdata) {
    Transform *t = _lua_object_get_transform_from_number3_handler(userdata);
    if (t == nullptr) {
        LUAUTILS_ERROR(L, "[Object.Down] original Object does not exist anymore"); // returns
    }

    _lua_object_get_up(t, x, y, z);

    if (x != nullptr) *x = -(*x);
    if (y != nullptr) *y = -(*y);
    if (z != nullptr) *z = -(*z);
}

void _lua_object_set_backward_handler(const float *x, const float *y, const float *z, lua_State *L, const number3_C_handler_userdata *userdata) {
    Transform *t = _lua_object_get_transform_from_number3_handler(userdata);
    if (t == nullptr) {
        LUAUTILS_ERROR(L, "[Object.Backward] original Object does not exist anymore"); // returns
    }

    // negate inputs
    const bool bx = x != nullptr;
    const bool by = y != nullptr;
    const bool bz = z != nullptr;
    const float nx = bx ? -(*x) : 0.0f;
    const float ny = by ? -(*y) : 0.0f;
    const float nz = bz ? -(*z) : 0.0f;

    _lua_object_set_forward(t, bx ? &nx : nullptr, by ? &ny : nullptr, bz ? &nz : nullptr);
}

void _lua_object_get_backward_handler(float *x, float *y, float *z, lua_State *L, const number3_C_handler_userdata *userdata) {
    Transform *t = _lua_object_get_transform_from_number3_handler(userdata);
    if (t == nullptr) {
        LUAUTILS_ERROR(L, "[Object.Backward] original Object does not exist anymore"); // returns
    }

    _lua_object_get_forward(t, x, y, z);

    if (x != nullptr) *x = -(*x);
    if (y != nullptr) *y = -(*y);
    if (z != nullptr) *z = -(*z);
}

RigidBody* _lua_object_ensure_rigidbody(Transform *t) {
    RigidBody *rb = transform_get_rigidbody(t);
    if (rb == nullptr) {
        transform_ensure_rigidbody(t,
                                   RigidbodyMode_Static,
                                   PHYSICS_GROUP_DEFAULT_OBJECT,
                                   PHYSICS_COLLIDESWITH_DEFAULT_OBJECT,
                                   &rb);
    }
    return rb;
}

void _lua_object_non_kinematic_reset(Transform *t) {
    RigidBody *rb = transform_get_rigidbody(t);
    if (rb != nullptr) {
        // any direct change to transform or physics properties is a non-kinematic use of the Object :
        // it is used outside of physics simulation (such as applying force or velocity), and requires
        // simulation state to be reset
        rigidbody_non_kinematic_reset(rb);
    }
}

void _lua_object_set_physics(Transform *t, RigidbodyMode value) {
    RigidBody *rb = _lua_object_ensure_rigidbody(t);
    rigidbody_set_simulation_mode(rb, value);

    if (value == RigidbodyMode_Dynamic) {
        // automatically disable children that would collide with this object
        transform_recurse_depth(t, [](Transform *child, void *ptr, uint32_t depth) {
            if (depth >= LUA_MAX_RECURSION_DEPTH) {
                return true; // stop recursion
            }
            RigidBody *childRb = transform_get_rigidbody(child);
            if (childRb != nullptr &&
                (rigidbody_is_static(childRb) || rigidbody_is_dynamic(childRb)) &&
                rigidbody_collides_with_rigidbody(childRb, (RigidBody*)ptr)) {

                rigidbody_set_simulation_mode(childRb, RigidbodyMode_Disabled);
            }
            return false; // don't stop recursion
        }, rb, false, false, 0);
    }
}

uint16_t _lua_object_get_groups(Transform *t) {
    RigidBody *rb = transform_get_rigidbody(t);
    if (rb == nullptr) return PHYSICS_GROUP_NONE;
    
    return rigidbody_get_groups(rb);
}

void _lua_object_set_groups(Transform *t, const uint16_t value) {
    RigidBody *rb = _lua_object_ensure_rigidbody(t);
    if (rb == nullptr) return;
    
    rigidbody_set_groups(rb, value);
    _lua_object_non_kinematic_reset(t);
}

uint16_t _lua_object_get_collides_with(Transform *t) {
    RigidBody *rb = transform_get_rigidbody(t);
    if (rb == nullptr) return PHYSICS_GROUP_NONE;
    
    return rigidbody_get_collides_with(rb);
}

void _lua_object_set_collides_with(Transform *t, const uint16_t value) {
    RigidBody *rb = _lua_object_ensure_rigidbody(t);
    if (rb == nullptr) return;
    
    rigidbody_set_collides_with(rb, value);
    _lua_object_non_kinematic_reset(t);
}

float _lua_object_get_mass(Transform *t) {
    RigidBody *rb = transform_get_rigidbody(t);
    if (rb == nullptr) return PHYSICS_MASS_DEFAULT;
    
    return rigidbody_get_mass(rb);
}

void _lua_object_set_mass(Transform *t, const float value) {
    RigidBody *rb = _lua_object_ensure_rigidbody(t);
    if (rb == nullptr) return;
    
    rigidbody_set_mass(rb, value);
}

void _lua_object_get_friction(Transform *t, float faces[FACE_COUNT]) {
    vx_assert(faces != NULL);

    RigidBody *rb = transform_get_rigidbody(t);
    for (FACE_INDEX_INT_T i = 0; i < FACE_COUNT; ++i) {
        if (rb == nullptr) {
            faces[i] = 1.0f - PHYSICS_FRICTION_DEFAULT;
        } else {
            faces[i] = 1.0f - rigidbody_get_friction(rb, i);
        }
    }
}

void _lua_object_set_friction(Transform *t, const FACE_INDEX_INT_T face, const float value) {
    RigidBody *rb = _lua_object_ensure_rigidbody(t);
    if (rb == nullptr) return;
    
    rigidbody_set_friction(rb, face, 1.0f - value);
}

void _lua_object_get_bounciness(Transform *t, float faces[FACE_COUNT]) {
    vx_assert(faces != NULL);

    RigidBody *rb = transform_get_rigidbody(t);
    for (FACE_INDEX_INT_T i = 0; i < FACE_COUNT; ++i) {
        if (rb == nullptr) {
            faces[i] = PHYSICS_BOUNCINESS_DEFAULT;
        } else {
            faces[i] = rigidbody_get_bounciness(rb, i);
        }
    }
}

void _lua_object_set_bounciness(Transform *t, const FACE_INDEX_INT_T face, const float value) {
    RigidBody *rb = _lua_object_ensure_rigidbody(t);
    if (rb == nullptr) return;
    
    rigidbody_set_bounciness(rb, face, value);
}

void _lua_object_toggle_collision_callback(Transform *t, CollisionCallbackType type, bool value) {
    RigidBody *rb = transform_get_rigidbody(t);
    if (rb == nullptr) return;
    
    rigidbody_toggle_collision_callback(rb, type, value);
}

void _lua_object_set_velocity(Transform *t, const float *x, const float *y, const float *z) {
    RigidBody *rb = _lua_object_ensure_rigidbody(t);
    if (rb == nullptr) return;
    
    float3 velocity; float3_copy(&velocity, rigidbody_get_velocity(rb));
    if (x != nullptr) { velocity.x = *x; }
    if (y != nullptr) { velocity.y = *y; }
    if (z != nullptr) { velocity.z = *z; }
    rigidbody_set_velocity(rb, &velocity);
}

void _lua_object_get_velocity(Transform *t, float *x, float *y, float *z) {
    const RigidBody *rb = transform_get_rigidbody(t);
    if (rb == nullptr) return;
    
    const float3 *velocity = rigidbody_get_velocity(rb);
    if (x != nullptr) { *x = velocity->x; }
    if (y != nullptr) { *y = velocity->y; }
    if (z != nullptr) { *z = velocity->z; }
}

void _lua_object_set_velocity_handler(const float *x, const float *y, const float *z, lua_State *L, const number3_C_handler_userdata *userdata) {
    Transform *t = _lua_object_get_transform_from_number3_handler(userdata);
    if (t == nullptr) {
        LUAUTILS_ERROR(L, "[Object.Velocity] original Object does not exist anymore"); // returns
    }

    _lua_object_set_velocity(t, x, y, z);
}

void _lua_object_get_velocity_handler(float *x, float *y, float *z, lua_State *L, const number3_C_handler_userdata *userdata) {
    Transform *t = _lua_object_get_transform_from_number3_handler(userdata);
    if (t == nullptr) {
        LUAUTILS_ERROR(L, "[Object.Velocity] original Object does not exist anymore"); // returns
    }

    _lua_object_get_velocity(t, x, y, z);
}

void _lua_object_set_motion(Transform *t, const float *x, const float *y, const float *z) {
    RigidBody *rb = _lua_object_ensure_rigidbody(t);
    if (rb == nullptr) return;

    float3 motion; float3_copy(&motion, rigidbody_get_motion(rb));
    if (x != nullptr) { motion.x = *x; }
    if (y != nullptr) { motion.y = *y; }
    if (z != nullptr) { motion.z = *z; }
    rigidbody_set_motion(rb, &motion);
}

void _lua_object_get_motion(Transform *t, float *x, float *y, float *z) {
    const RigidBody *rb = transform_get_rigidbody(t);
    if (rb == nullptr) return;

    const float3 *motion = rigidbody_get_motion(rb);
    if (x != nullptr) { *x = motion->x; }
    if (y != nullptr) { *y = motion->y; }
    if (z != nullptr) { *z = motion->z; }
}

void _lua_object_set_motion_handler(const float *x, const float *y, const float *z, lua_State *L, const number3_C_handler_userdata *userdata) {
    Transform *t = _lua_object_get_transform_from_number3_handler(userdata);
    if (t == nullptr) {
        LUAUTILS_ERROR(L, "[Object.Motion] original Object does not exist anymore"); // returns
    }

    _lua_object_set_motion(t, x, y, z);
}

void _lua_object_get_motion_handler(float *x, float *y, float *z, lua_State *L, const number3_C_handler_userdata *userdata) {
    Transform *t = _lua_object_get_transform_from_number3_handler(userdata);
    if (t == nullptr) {
        LUAUTILS_ERROR(L, "[Object.Motion] original Object does not exist anymore"); // returns
    }

    _lua_object_get_motion(t, x, y, z);
}

void _lua_object_set_acceleration(Transform *t, const float *x, const float *y, const float *z) {
    RigidBody *rb = _lua_object_ensure_rigidbody(t);
    if (rb == nullptr) return;
    
    float3 acceleration; float3_copy(&acceleration, rigidbody_get_constant_acceleration(rb));
    if (x != nullptr) { acceleration.x = *x; }
    if (y != nullptr) { acceleration.y = *y; }
    if (z != nullptr) { acceleration.z = *z; }
    rigidbody_set_constant_acceleration(rb, &acceleration);
}

void _lua_object_get_acceleration(Transform *t, float *x, float *y, float *z) {
    const RigidBody *rb = transform_get_rigidbody(t);
    if (rb == nullptr) return;
    
    const float3 *acceleration = rigidbody_get_constant_acceleration(rb);
    if (x != nullptr) { *x = acceleration->x; }
    if (y != nullptr) { *y = acceleration->y; }
    if (z != nullptr) { *z = acceleration->z; }
}

void _lua_object_set_acceleration_handler(const float *x, const float *y, const float *z, lua_State *L, const number3_C_handler_userdata *userdata) {
    Transform *t = _lua_object_get_transform_from_number3_handler(userdata);
    if (t == nullptr) {
        LUAUTILS_ERROR(L, "[Object.Acceleration] original Object does not exist anymore"); // returns
    }

    _lua_object_set_acceleration(t, x, y, z);
}

void _lua_object_get_acceleration_handler(float *x, float *y, float *z, lua_State *L, const number3_C_handler_userdata *userdata) {
    Transform *t = _lua_object_get_transform_from_number3_handler(userdata);
    if (t == nullptr) {
        LUAUTILS_ERROR(L, "[Object.Acceleration] original Object does not exist anymore"); // returns
    }

    _lua_object_get_acceleration(t, x, y, z);
}

void _lua_object_set_collider_origin(Transform *t, const float *x, const float *y, const float *z) {
    RigidBody *rb = _lua_object_ensure_rigidbody(t);
    Box collider = *rigidbody_get_collider(rb);
    
    float3 pivot;
    if (transform_get_type(t) == ShapeTransform) {
        pivot = shape_get_pivot(transform_utils_get_shape(t));
    } else if (transform_get_type(t) == MeshTransform) {
        pivot = mesh_get_pivot(static_cast<Mesh*>(transform_get_ptr(t)));
    } else {
        pivot = float3_zero;
    }
    
    // collider is exposed as a local point relative to pivot
    if (x != nullptr) {
        collider.max.x = *x + collider.max.x - collider.min.x + pivot.x;
        collider.min.x = *x + pivot.x;
    }
    if (y != nullptr) {
        collider.max.y = *y + collider.max.y - collider.min.y + pivot.y;
        collider.min.y = *y + pivot.y;
    }
    if (z != nullptr) {
        collider.max.z = *z + collider.max.z - collider.min.z + pivot.z;
        collider.min.z = *z + pivot.z;
    }
    rigidbody_set_collider(rb, &collider, true);
}

void _lua_object_get_collider_origin(Transform *t, float *x, float *y, float *z) {
    const RigidBody *rb = _lua_object_ensure_rigidbody(t);
    const Box *collider = rigidbody_get_collider(rb);
    
    float3 pivot;
    if (transform_get_type(t) == ShapeTransform) {
        pivot = shape_get_pivot(transform_utils_get_shape(t));
    } else if (transform_get_type(t) == MeshTransform) {
        pivot = mesh_get_pivot(static_cast<Mesh*>(transform_get_ptr(t)));
    } else {
        pivot = float3_zero;
    }
    
    // collider is exposed as a local point relative to pivot
    if (x != nullptr) { *x = collider->min.x - pivot.x; }
    if (y != nullptr) { *y = collider->min.y - pivot.y; }
    if (z != nullptr) { *z = collider->min.z - pivot.z; }
}

void _lua_object_set_collider_origin_handler(const float *x, const float *y, const float *z, lua_State *L, const number3_C_handler_userdata *userdata) {
    Transform *t = _lua_object_get_transform_from_number3_handler(userdata);
    if (t == nullptr) {
        LUAUTILS_ERROR(L, "[Object.ColliderOrigin] original Object does not exist anymore"); // returns
    }
    
    _lua_object_set_collider_origin(t, x, y, z);
}

void _lua_object_get_collider_origin_handler(float *x, float *y, float *z, lua_State *L, const number3_C_handler_userdata *userdata) {
    Transform *t = _lua_object_get_transform_from_number3_handler(userdata);
    if (t == nullptr) {
        LUAUTILS_ERROR(L, "[Object.ColliderOrigin] original Object does not exist anymore"); // returns
    }
    
    _lua_object_get_collider_origin(t, x, y, z);
}

void _lua_object_set_collider_size(Transform *t, const float *x, const float *y, const float *z) {
    RigidBody *rb = _lua_object_ensure_rigidbody(t);
    Box collider = *rigidbody_get_collider(rb);
    if (x != nullptr) { collider.max.x = *x + collider.min.x; }
    if (y != nullptr) { collider.max.y = *y + collider.min.y; }
    if (z != nullptr) { collider.max.z = *z + collider.min.z; }
    rigidbody_set_collider(rb, &collider, true);
}

void _lua_object_get_collider_size(Transform *t, float *x, float *y, float *z) {
    const RigidBody *rb = _lua_object_ensure_rigidbody(t);
    const Box *collider = rigidbody_get_collider(rb);
    if (x != nullptr) { *x = collider->max.x - collider->min.x; }
    if (y != nullptr) { *y = collider->max.y - collider->min.y; }
    if (z != nullptr) { *z = collider->max.z - collider->min.z; }
}

void _lua_object_set_collider_size_handler(const float *x, const float *y, const float *z, lua_State *L, const number3_C_handler_userdata *userdata) {
    Transform *t = _lua_object_get_transform_from_number3_handler(userdata);
    if (t == nullptr) {
        LUAUTILS_ERROR(L, "[Object.ColliderSize] original Object does not exist anymore"); // returns
    }
    
    _lua_object_set_collider_size(t, x, y, z);
}

void _lua_object_get_collider_size_handler(float *x, float *y, float *z, lua_State *L, const number3_C_handler_userdata *userdata) {
    Transform *t = _lua_object_get_transform_from_number3_handler(userdata);
    if (t == nullptr) {
        LUAUTILS_ERROR(L, "[Object.ColliderSize] original Object does not exist anymore"); // returns
    }
    
    _lua_object_get_collider_size(t, x, y, z);
}

void _lua_object_set_collision_box(Transform * const t,
                                   const float3 * const min,
                                   const float3 * const max) {
    RigidBody *rb = _lua_object_ensure_rigidbody(t);
    vx_assert(rb != nullptr);
    
    // construct new box value
    Box newCollider = *rigidbody_get_collider(rb);
    if (min != nullptr) { newCollider.min = *min; }
    if (max != nullptr) { newCollider.max = *max; }
    
    // set the new box using
    rigidbody_set_collider(rb, &newCollider, true);
}

void _lua_object_get_collision_box(Transform * const t, float3 * const min, float3 * const max) {
    const RigidBody *rb = _lua_object_ensure_rigidbody(t);
    vx_assert(rb != nullptr);

    const Box *collider = rigidbody_get_collider(rb);
    vx_assert(collider != nullptr);
    
    if (min != nullptr) { *min = collider->min; }
    if (max != nullptr) { *max = collider->max; }
}

void _lua_object_set_collision_box_handler(const float3 *min, const float3 *max, lua_State *L, void *userdata) {
    Transform *t = static_cast<Transform *>(weakptr_get(static_cast<Weakptr*>(userdata)));
    if (t == nullptr) {
        LUAUTILS_ERROR(L, "[Object.CollisionBox] original Object does not exist anymore"); // returns
    }
    _lua_object_set_collision_box(t, min, max);
}

void _lua_object_get_collision_box_handler(float3 *min, float3 *max, lua_State *L, const void *userdata) {
    Transform *t = static_cast<Transform *>(weakptr_get(static_cast<const Weakptr*>(userdata)));
    if (t == nullptr) {
        LUAUTILS_ERROR(L, "[Object.CollisionBox] original Object does not exist anymore"); // returns
    }
    
    _lua_object_get_collision_box(t, min, max);
}

void _lua_object_free_collision_box_handler(void * const userdata) {
    weakptr_release(static_cast<Weakptr*>(userdata));
}

void _lua_object_set_collision_box_min_n3_handler(const float *x, const float *y, const float *z,
                                                  lua_State *L, const number3_C_handler_userdata *userdata) {
    vx_assert(userdata != nullptr);

    float3 newMin = float3_zero;
    box_C_handler *handler = static_cast<box_C_handler *>(weakptr_get(static_cast<Weakptr*>(userdata->ptr)));
    if (handler == nullptr) {
        LUAUTILS_ERROR(L, "[Box.Min] original Box does not exist anymore"); // returns
        return; // return statement not to confuse code analyzer
    }
    
    handler->get(&newMin, nullptr, L, handler->userdata);
    
    if (x != nullptr) { newMin.x = *x; }
    if (y != nullptr) { newMin.y = *y; }
    if (z != nullptr) { newMin.z = *z; }

    handler->set(&newMin, nullptr, L, handler->userdata);
}

void _lua_object_get_collision_box_min_n3_handler(float *x, float *y, float *z, lua_State *L,
                                                  const number3_C_handler_userdata *userdata) {
    vx_assert(userdata != nullptr);
    
    float3 minValue = float3_zero;
    box_C_handler *handler = static_cast<box_C_handler *>(weakptr_get(static_cast<Weakptr*>(userdata->ptr)));
    
    if (handler == nullptr) {
        LUAUTILS_ERROR(L, "[Box.Min] original Box does not exist anymore"); // returns
        return; // return statement not to confuse code analyzer
    }
    
    handler->get(&minValue, nullptr, L, handler->userdata);
    if (x != nullptr) { *x = minValue.x; }
    if (y != nullptr) { *y = minValue.y; }
    if (z != nullptr) { *z = minValue.z; }
}

void _lua_object_set_collision_box_max_n3_handler(const float *x, const float *y, const float *z,
                                                  lua_State *L, const number3_C_handler_userdata *userdata) {
    vx_assert(userdata != nullptr);
    
    float3 newMax = float3_zero;
    box_C_handler *handler = static_cast<box_C_handler *>(weakptr_get(static_cast<Weakptr*>(userdata->ptr)));
    if (handler == nullptr) {
        LUAUTILS_ERROR(L, "[Box.Max] original Box does not exist anymore"); // returns
        return; // return statement not to confuse code analyzer
    };
    
    handler->get(nullptr, &newMax, L, handler->userdata);
    
    if (x != nullptr) { newMax.x = *x; }
    if (y != nullptr) { newMax.y = *y; }
    if (z != nullptr) { newMax.z = *z; }
    
    handler->set(nullptr, &newMax, L, handler->userdata);
}

void _lua_object_get_collision_box_max_n3_handler(float *x, float *y, float *z, lua_State *L,
                                                  const number3_C_handler_userdata *userdata) {
    vx_assert(userdata != nullptr);
    
    float3 maxValue = float3_zero;
    box_C_handler *handler = static_cast<box_C_handler *>(weakptr_get(static_cast<Weakptr*>(userdata->ptr)));

    if (handler == nullptr) {
        LUAUTILS_ERROR(L, "[Box.Max] original Box does not exist anymore"); // returns
        return; // return statement not to confuse code analyzer
    }
    
    handler->get(nullptr, &maxValue, L, handler->userdata);
    if (x != nullptr) { *x = maxValue.x; }
    if (y != nullptr) { *y = maxValue.y; }
    if (z != nullptr) { *z = maxValue.z; }
}

/// Object:SetParent(self, [parent], [keepWorld])
/// - "self" : lua object w/ Object support
/// - "parent" : lua object w/ Object support, or nil to remove parent (default: remove parent)
/// - "keepWorld" : boolean, whether to keep world or local pos/rot (default: false)
/// returns : true if the parent was set or removed, false if nothing happened
///
/// Valid calls are:
/// obj:SetParent()
/// obj:SetParent(nil/otherObj)
/// obj:SetParent(true/false)
/// obj:SetParent(nil/otherObj, true/false)
static int _object_setParent(lua_State *L) {
    vx_assert(L != nullptr);
    LUAUTILS_STACK_SIZE(L)

    const int argsCount = lua_gettop(L);
    if (argsCount < 1 || argsCount > 3) {
        LUAUTILS_ERROR(L, "[object:SetParent] incorrect argument count"); // returns
    }

    Transform *self; lua_object_getTransformPtr(L, 1, &self);
    if (self == nullptr) {
        LUAUTILS_ERROR(L, "[object:SetParent] argument 1 should have an Object component"); // returns
    }
    
    CGame *g = nullptr;
    if (lua_utils_getGamePtr(L, &g) == false || g == nullptr) {
        LUAUTILS_ERROR(L, "cannot access game");
    }
    Scene *sc = game_get_scene(g);

    Transform *parent = nullptr; // remove parent by default
    bool keepWorld = false; // keep local by default
    if (argsCount == 2) {
        if (lua_isboolean(L, 2)) {
            keepWorld = lua_toboolean(L, 2);
        } else {
            lua_object_getTransformPtr(L, 2,  &parent);
        }
    } else if (argsCount >= 3) {
        lua_object_getTransformPtr(L, 2, &parent);
        keepWorld = lua_isboolean(L, 3) ? lua_toboolean(L, 3) : keepWorld;
    }

    bool result = true;
    if (parent != nullptr) {
        if (transform_set_parent(self, parent, keepWorld) == false) {
            LUAUTILS_ERROR(L, "object:SetParent - objects were not parented to prevent cyclic hierarchy");
        }
    } else {
        result = scene_remove_transform(sc, self, keepWorld);
    }
    if (result) {
        _lua_object_non_kinematic_reset(self);
    }

    lua_pushboolean(L, result);
    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
    return 1;
}

/// Object:RemoveParent(self, [keepGlobal])
/// - "self" : lua object w/ Object support
/// - "keepWorld" : boolean, whether to keep world or local pos/rot (default: false)
/// returns : true if the parent was removed, false if nothing happened
///
/// Equivalent to calling obj:SetParent w/o parent or nil
static int _object_removeParent(lua_State *L) {
    RETURN_VALUE_IF_NULL(L, 0)
    LUAUTILS_STACK_SIZE(L)

    const int argsCount = lua_gettop(L);
    if (argsCount < 1 || argsCount > 2) {
        LUAUTILS_ERROR(L, "[object:RemoveParent] incorrect argument count"); // returns
    }

    Transform *self; lua_object_getTransformPtr(L, 1, &self);
    if (self == nullptr) {
        LUAUTILS_ERROR(L, "[object:RemoveParent] argument 1 should have an Object component"); // returns
    }

    bool keepWorld = false; // keep local by default
    if (argsCount >= 2 && lua_isboolean(L, 2)) {
        keepWorld = lua_toboolean(L, 2);
    }
    
    CGame *g = nullptr;
    if (lua_utils_getGamePtr(L, &g) == false || g == nullptr) {
        LUAUTILS_ERROR(L, "cannot access game");
    }
    Scene *sc = game_get_scene(g);
    
    bool result = scene_remove_transform(sc, self, keepWorld);
    if (result) {
        _lua_object_non_kinematic_reset(self);
    }
    
    lua_pushboolean(L, result);
    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
    return 1;
}

/// Object:GetParent(self)
/// - "self" : lua object w/ Object support
/// returns : parent lua Object or nil
static int _object_getParent(lua_State *L) {
    vx_assert(L != nullptr);
    LUAUTILS_STACK_SIZE(L)
    
    const int argsCount = lua_gettop(L);
    if (argsCount != 1) {
        LUAUTILS_ERROR(L, "Object:GetParent - function should be called with `:`"); // returns
    }
    
    Transform *self; lua_object_getTransformPtr(L, 1, &self);
    if (self == nullptr) {
        LUAUTILS_ERROR(L, "Object:GetParent - function should be called with `:`"); // returns
    }
    
    if (transform_is_parented(self)) {
        Transform *p = transform_get_parent(self);

        // if found through here, system root is returned as standalone object
        CGame *game = nullptr;
        if (lua_utils_getGamePtr(L, &game) == false || game == nullptr) {
            LUAUTILS_INTERNAL_ERROR(L)
        }
        const Transform *system = scene_get_system_root(game_get_scene(game));
    
        // hide transforms reserved for engine
        while (p != nullptr && transform_get_type(p) == HierarchyTransform && p != system) {
            p = transform_get_parent(p);
        }
        
        if (p == nullptr) {
            lua_pushnil(L);
        } else if (p == system) {
            lua_object_pushNewInstance(L, p);
        } else if (lua_g_object_getOrCreateInstance(L, transform_get_parent(self)) == false) {
            LUAUTILS_ERROR(L, "Failed to retrieve Object instance"); // returns
        }
    } else {
        lua_pushnil(L);
    }
    
    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
    return 1;
}

/// Object:AddChild(self, child, [keepGlobal])
/// - "self" : lua object w/ Object support
/// - "child" : lua object w/ Object support
/// - "keepWorld" : boolean, whether to keep the child world or local pos/rot (default: false)
static int _object_addChild(lua_State *L) {
    vx_assert(L != nullptr);
    LUAUTILS_STACK_SIZE(L)

    const int argsCount = lua_gettop(L);
    if (argsCount < 2 || argsCount > 3) {
        LUAUTILS_ERROR(L, "object:AddChild - incorrect argument count"); // returns
    }

    Transform *self; lua_object_getTransformPtr(L, 1, &self);
    if (self == nullptr) {
        LUAUTILS_ERROR(L, "object:AddChild - argument 1 should have an Object component"); // returns
    }
    
    if (lua_utils_getObjectType(L, 2) == ITEM_TYPE_WORLD) {
        LUAUTILS_ERROR(L, "object:AddChild - cannot add World as a child"); // returns
    }
    
    Transform *child; lua_object_getTransformPtr(L, 2, &child);
    if (child == nullptr) {
        LUAUTILS_ERROR(L, "object:AddChild - argument 2 should have an Object component"); // returns
    }

    bool keepWorld = false; // keep local by default
    if (argsCount >= 3 && lua_isboolean(L, 3)) {
        keepWorld = lua_toboolean(L, 3);
    }

    if (transform_set_parent(child, self, keepWorld) == false) {
        LUAUTILS_ERROR(L, "object:AddChild - objects were not parented to prevent cyclic hierarchy");
    }
    _lua_object_non_kinematic_reset(child);
    
    LUAUTILS_STACK_SIZE_ASSERT(L, 0)
    return 0;
}

/// Object:RemoveChild(self, child/index, [keepGlobal])
/// - "self" : lua object w/ Object support
/// - "child/index" : lua object w/ Object support, or number (index)
/// - "keepWorld" : boolean, whether to keep the child world or local pos/rot (default: false)
/// returns: true if the child was removed, false if nothing happened
static int _object_removeChild(lua_State *L) {
    RETURN_VALUE_IF_NULL(L, 0)
    LUAUTILS_STACK_SIZE(L)

    const int argsCount = lua_gettop(L);
    if (argsCount < 2 || argsCount > 3) {
        LUAUTILS_ERROR(L, "[object:RemoveChild] incorrect argument count"); // returns
    }

    Transform *self; lua_object_getTransformPtr(L, 1, &self);
    if (self == nullptr) {
        LUAUTILS_ERROR(L, "[object:RemoveChild] argument 1 should have an Object component"); // returns
    }

    bool keepWorld = false; // keep local by default
    if (argsCount >= 3 && lua_isboolean(L, 3)) {
        keepWorld = lua_toboolean(L, 3);
    }
    
    CGame *g = nullptr;
    if (lua_utils_getGamePtr(L, &g) == false || g == nullptr) {
        LUAUTILS_ERROR(L, "cannot access game");
    }
    Scene *sc = game_get_scene(g);

    Transform *child; lua_object_getTransformPtr(L, 2, &child);
    bool result = false;
    if (child == nullptr) {
        if (lua_isnumber(L, 2)) {
            int idx = static_cast<int>(lua_tonumber(L, 2)) - 1;
            int count = 0;
            DoublyLinkedListNode *n = transform_get_children_iterator(self);
            while (n != nullptr) {
                child = static_cast<Transform*>(doubly_linked_list_node_pointer(n));
                // hide transforms reserved for engine
                if (transform_get_type(child) != HierarchyTransform) {
                    if (idx == count) {
                        result = scene_remove_transform(sc, child, keepWorld);
                        break;
                    } else {
                        count++;
                    }
                }
                n = doubly_linked_list_node_next(n);
            }
        } else {
            LUAUTILS_ERROR(L, "[object:RemoveChild] argument 2 should have an Object component or be a Number"); // returns
        }
    } else if (transform_get_parent(child) == self) {
        result = scene_remove_transform(sc, child, keepWorld);
    }
    if (result && child != nullptr) {
        _lua_object_non_kinematic_reset(child);
    }
    
    LUAUTILS_STACK_SIZE_ASSERT(L, 0)
    
    lua_pushboolean(L, result);
    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
    return 1;
}

/// Object:RemoveChildren(self, [keepGlobal])
/// - "self" : lua object w/ Object support
/// - "keepWorld" : boolean, whether to keep the children world or local pos/rot (default: false)
static int _object_removeChildren(lua_State *L) {
    RETURN_VALUE_IF_NULL(L, 0)
    LUAUTILS_STACK_SIZE(L)

    const int argsCount = lua_gettop(L);
    if (argsCount < 1 || argsCount > 2) {
        LUAUTILS_ERROR(L, "[object:RemoveChildren] incorrect argument count"); // returns
    }

    Transform *self; lua_object_getTransformPtr(L, 1, &self);
    if (self == nullptr) {
        LUAUTILS_ERROR(L, "[object:RemoveChildren] argument 1 should have an Object component"); // returns
    }

    bool keepWorld = false; // keep local by default
    if (argsCount >= 2 && lua_isboolean(L, 2)) {
        keepWorld = lua_toboolean(L, 2);
    }
    
    CGame *g = nullptr;
    if (lua_utils_getGamePtr(L, &g) == false || g == nullptr) {
        LUAUTILS_ERROR(L, "cannot access game");
    }
    Scene *sc = game_get_scene(g);

    size_t count = 0;
    Transform_Array children = transform_get_children_copy(self, &count);
    for (size_t i = 0; i < count; ++i) {
        // hide transforms reserved for engine
        if (transform_get_type(children[i]) != HierarchyTransform) {
            if (scene_remove_transform(sc, children[i], keepWorld)) {
                _lua_object_non_kinematic_reset(children[i]);
            }
        }
    }
    free(children);

    LUAUTILS_STACK_SIZE_ASSERT(L, 0)
    return 0;
}

/// Object:GetChild(self, index)
/// - "self" : lua object w/ Object support
/// - "index" : child index
/// returns: the child as a NEW Object wrapper
static int _object_getChild(lua_State *L) {
    vx_assert(L != nullptr);
    LUAUTILS_STACK_SIZE(L)
    
    const int argCount = lua_gettop(L);

    if (argCount < 1) {
        LUAUTILS_ERROR(L, "Object:GetChild - function should be called with `:`");
    }

    Transform *self; lua_object_getTransformPtr(L, 1, &self);
    if (self == nullptr) {
        LUAUTILS_ERROR(L, "Object:GetChild - function should be called with `:`");
    }
    
    if (argCount != 2) {
        LUAUTILS_ERROR(L, "Object:GetChild - wrong number of arguments");
    }
    
    if (lua_isnumber(L, 2) == false) {
        LUAUTILS_ERROR(L, "Object:GetChild - argument should be a number"); // returns
    }

    const int idx = static_cast<int>(lua_tonumber(L, 2));
    int count = 1; // indexing starts at 1 in Lua
    DoublyLinkedListNode *n = transform_get_children_iterator(self);
    Transform *child = nullptr;
    
    while (n != nullptr) {
        child = static_cast<Transform*>(doubly_linked_list_node_pointer(n));
        // hide transforms reserved for engine
        if (transform_get_type(child) != HierarchyTransform) {
            if (idx == count) {
                if (lua_g_object_getOrCreateInstance(L, child) == false) {
                    LUAUTILS_ERROR(L, "Failed to retrieve Object instance"); // returns
                }
                LUAUTILS_STACK_SIZE_ASSERT(L, 1)
                return 1;
            } else {
                ++count;
            }
        }
        n = doubly_linked_list_node_next(n);
    }

    // not found
    lua_pushnil(L);

    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
    return 1;
}

/// Object:PositionLocalToWorld(self, value) / Object:RotationLocalToWorld(self, value)
/// - "self" : lua object w/ Object support
/// - "value" : Number3, table of 3 numbers, or 3 individual numbers / or Rotation
/// returns: transformed Number3 / Rotation
static int _object_localToWorld(lua_State *L, bool isPos) {
    RETURN_VALUE_IF_NULL(L, 0)
    LUAUTILS_STACK_SIZE(L)
    
    const int argCount = lua_gettop(L);
    
    if (argCount < 1 || lua_object_isObject(L, 1) == false) {
        LUAUTILS_ERROR_F(L, "object:%sLocalToWorld - function should be called with `:`", isPos ? "Position" : "Rotation");
    }

    Transform *self; lua_object_getTransformPtr(L, 1, &self);
    if (self == nullptr) {
        LUAUTILS_INTERNAL_ERROR(L) // returns
    }

    transform_refresh(self, false, true); // refresh ltw for intra-frame calculations

    if (isPos) {
        float3 pos;
        if (lua_number3_or_table_or_numbers_getXYZ(L, 2, &pos.x, &pos.y, &pos.z) == false) {
            LUAUTILS_ERROR(L, "object:PositionLocalToWorld - argument should be either a Number3, a table of 3 numbers, or 3 individual numbers");
        }
        float3 result; transform_utils_position_ltw(self, &pos, &result);
        lua_number3_pushNewObject(L, result.x, result.y, result.z);
    } else {
        Quaternion q;
        if (lua_rotation_or_table_get(L, 2, &q) == false) {
            float3 euler;
            if (lua_number3_or_table_or_numbers_getXYZ(L, 2, &euler.x, &euler.y, &euler.z) == false) {
                LUAUTILS_ERROR(L, "object:RotationLocalToWorld - argument should be either a Number3, a Rotation, a table of 3 numbers, or 3 individual numbers");
            }
            euler_to_quaternion_vec(&euler, &q);
        }

        Quaternion result; transform_utils_rotation_ltw(self, &q, &result);
        lua_rotation_push_new(L, result);
    }

    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
    return 1;
}

/// Object:PositionWorldToLocal(self, value) / Object:RotationWorldToLocal(self, value)
/// - "self" : lua object w/ Object support
/// - "value" : number3
/// returns: transformed number3
static int _object_worldToLocal(lua_State *L, bool isPos) {
    RETURN_VALUE_IF_NULL(L, 0)
    LUAUTILS_STACK_SIZE(L)

    const int argCount = lua_gettop(L);
    
    if (argCount < 1 || lua_object_isObject(L, 1) == false) {
        LUAUTILS_ERROR_F(L, "object:%sWorldToLocal - function should be called with `:`", isPos ? "Position" : "Rotation");
    }

    Transform *self; lua_object_getTransformPtr(L, 1, &self);
    if (self == nullptr) {
        LUAUTILS_INTERNAL_ERROR(L) // returns
    }

    transform_refresh(self, false, true); // refresh ltw for intra-frame calculations

    if (isPos) {
        float3 pos;
        if (lua_number3_or_table_or_numbers_getXYZ(L, 2, &pos.x, &pos.y, &pos.z) == false) {
            LUAUTILS_ERROR(L, "object:PositionWorldToLocal - argument should be either a Number3, a table of 3 numbers, or 3 individual numbers");
        }
        float3 result; transform_utils_position_wtl(self, &pos, &result);
        lua_number3_pushNewObject(L, result.x, result.y, result.z);
    } else {
        Quaternion q;
        if (lua_rotation_or_table_get(L, 2, &q) == false) {
            float3 euler;
            if (lua_number3_or_table_or_numbers_getXYZ(L, 2, &euler.x, &euler.y, &euler.z) == false) {
                LUAUTILS_ERROR(L, "object:RotationWorldToLocal - argument should be either a Number3, a Rotation, a table of 3 numbers, or 3 individual numbers");
            }
            euler_to_quaternion_vec(&euler, &q);
        }

        Quaternion result; transform_utils_rotation_wtl(self, &q, &result);
        lua_rotation_push_new(L, result);
    }

    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
    return 1;
}

static int _object_positionLtw(lua_State *L) { return _object_localToWorld(L, true); }
static int _object_positionWtl(lua_State *L) { return _object_worldToLocal(L, true); }
static int _object_rotationLtw(lua_State *L) { return _object_localToWorld(L, false); }
static int _object_rotationWtl(lua_State *L) { return _object_worldToLocal(L, false); }

/// Object:RotateLocal(number3) / Object:RotateWorld(number3) -- euler angles
/// Object:RotateLocal(number3, number) / Object:RotateWorld(number3, number) -- axis angle
static int _object_rotate(lua_State * const L, const bool isLocal) {
    RETURN_VALUE_IF_NULL(L, 0)
    LUAUTILS_STACK_SIZE(L)

    const int argCount = lua_gettop(L);
    
    if (argCount < 1 || lua_object_isObject(L, 1) == false) {
        LUAUTILS_ERROR_F(L, "object:Rotate%s - function should be called with `:`", isLocal ? "Local" : "World");
    }

    Transform *self; lua_object_getTransformPtr(L, 1, &self);
    if (self == nullptr) {
        LUAUTILS_INTERNAL_ERROR(L) // returns
    }

    Quaternion q;
    float3 f3;
    float angle = 0.0;
    bool axisAngle = false, rotType = false;
    if (argCount >= 4) { // first parameter as 3 numbers
        if (lua_isnumber(L, 2) == false) {
            LUAUTILS_ERROR_F(L, "Object:Rotate%s - argument 1 should be a number", isLocal ? "Local" : "World");
        }
        f3.x = lua_tonumber(L, 2);
        if (lua_isnumber(L, 3) == false) {
            LUAUTILS_ERROR_F(L, "Object:Rotate%s - argument 2 should be a number", isLocal ? "Local" : "World");
        }
        f3.y = lua_tonumber(L, 3);
        if (lua_isnumber(L, 4) == false) {
            LUAUTILS_ERROR_F(L, "Object:Rotate%s - argument 3 should be a number", isLocal ? "Local" : "World");
        }
        f3.z = lua_tonumber(L, 4);

        if (argCount == 5) { // axis/angle signature
            if (lua_isnumber(L, 5)) {
                axisAngle = true;
                angle = lua_tonumber(L, 5);
            } else {
                LUAUTILS_ERROR_F(L, "Object:Rotate%s - argument 4 should be a number", isLocal ? "Local" : "World");
            }
        }
    } else if (argCount >= 2) { // first parameter as Number3, Rotation, or table of numbers
        if (lua_rotation_get(L, 2, &q)) {
            rotType = true;
        } else if (lua_number3_or_table_getXYZ(L, 2, &f3.x, &f3.y, &f3.z) == false) {
            LUAUTILS_ERROR_F(L, "Object:Rotate%s - argument 1 should be a Number3 or table of numbers", isLocal ? "Local" : "World");
        }
        if (argCount == 3) { // axis/angle signature
            if (rotType) {
                LUAUTILS_ERROR_F(L, "Object:Rotate%s - wrong number of arguments", isLocal ? "Local" : "World");
            }
            if (lua_isnumber(L, 3)) {
                axisAngle = true;
                angle = lua_tonumber(L, 3);
            } else {
                LUAUTILS_ERROR_F(L, "Object:Rotate%s - argument 2 should be a number", isLocal ? "Local" : "World");
            }
        }
    } else {
        LUAUTILS_ERROR_F(L, "Object:Rotate%s - wrong number of arguments", isLocal ? "Local" : "World");
    }
    
    if (axisAngle) {
        axis_angle_to_quaternion(&f3, angle, &q);
    } else if (rotType == false) {
        euler_to_quaternion_vec(&f3, &q);
    }
    
    transform_utils_rotate(self, &q, &q, isLocal);
    if (isLocal) {
        transform_set_local_rotation(self, &q);
    } else {
        transform_set_rotation(self, &q);
    }

    LUAUTILS_STACK_SIZE_ASSERT(L, 0)
    return 0;
}

static int _object_rotateLocal(lua_State * const L) {
    return _object_rotate(L, true);
}

static int _object_rotateWorld(lua_State * const L) {
    return _object_rotate(L, false);
}

/// Object:CollidesWith(self, other)
/// - "self" : lua object w/ Object support
/// - "other" : lua object w/ Object support
static int _object_collidesWith(lua_State *L) {
    RETURN_VALUE_IF_NULL(L, 0)
    LUAUTILS_STACK_SIZE(L)
    
    const int argsCount = lua_gettop(L);
    if (argsCount != 2) {
        LUAUTILS_ERROR(L, "[object:CollidesWith] incorrect argument count"); // returns
    }
    
    Transform *self; lua_object_getTransformPtr(L, 1, &self);
    if (self == nullptr) {
        LUAUTILS_INTERNAL_ERROR(L) // returns
    }
    
    Transform *other; lua_object_getTransformPtr(L, 2, &other);
    if (other == nullptr) {
        LUAUTILS_ERROR(L, "[object:CollidesWith] argument 1 should have an Object component"); // returns
    }
    
    const RigidBody *selfRb = transform_get_rigidbody(self);
    const RigidBody *otherRb = transform_get_rigidbody(other);
    lua_pushboolean(L, selfRb != nullptr && otherRb != nullptr && rigidbody_collides_with_rigidbody(selfRb, otherRb));
    
    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
    return 1;
}

/// Object:ApplyForce(self, value)
/// - "self" : lua object w/ Object support
/// - "value" : number3
static int _object_applyForce(lua_State *L) {
    RETURN_VALUE_IF_NULL(L, 0)
    LUAUTILS_STACK_SIZE(L)
    
    const int argsCount = lua_gettop(L);
    if (argsCount != 2) {
        LUAUTILS_ERROR(L, "[object:ApplyForce] incorrect argument count"); // returns
    }
    
    Transform *self; lua_object_getTransformPtr(L, 1, &self);
    if (self == nullptr) {
        LUAUTILS_INTERNAL_ERROR(L) // returns
    }
    
    float3 f3;
    if (lua_number3_or_table_getXYZ(L, 2, &f3.x, &f3.y, &f3.z) == false) {
        LUAUTILS_ERROR(L, "[object:ApplyForce] argument 1 should be a Number3"); // returns
    }
    
    RigidBody *rb = _lua_object_ensure_rigidbody(self);
    if (rb != nullptr) {
        rigidbody_apply_force_impulse(rb, &f3);
    }
    
    LUAUTILS_STACK_SIZE_ASSERT(L, 0)
    return 0;
}

/// Object:Awake()
static int _object_awake(lua_State *L) {
    RETURN_VALUE_IF_NULL(L, 0)
    LUAUTILS_ASSERT_ARGS_COUNT(L, 1)
    LUAUTILS_STACK_SIZE(L)
    
    Transform *self; lua_object_getTransformPtr(L, 1, &self);
    if (self == nullptr) {
        LUAUTILS_INTERNAL_ERROR(L) // returns
    }
    
    RigidBody *rb = _lua_object_ensure_rigidbody(self);
    if (rb != nullptr) {
        rigidbody_set_awake(rb);
    }
    
    LUAUTILS_STACK_SIZE_ASSERT(L, 0)
    return 0;
}

/// Object:Refresh()
static int _object_refresh(lua_State *L) {
    RETURN_VALUE_IF_NULL(L, 0)
    LUAUTILS_ASSERT_ARGS_COUNT(L, 1)
    LUAUTILS_STACK_SIZE(L)

    Transform *self; lua_object_getTransformPtr(L, 1, &self);
    if (self == nullptr) {
        LUAUTILS_INTERNAL_ERROR(L) // returns
    }

    transform_refresh(self, false, true);

    LUAUTILS_STACK_SIZE_ASSERT(L, 0)
    return 0;
}

/// Object:ResetCollisionBox()
static int _object_resetCollider(lua_State *L) {
    RETURN_VALUE_IF_NULL(L, 0)
    LUAUTILS_ASSERT_ARGS_COUNT(L, 1)
    LUAUTILS_STACK_SIZE(L)
    
    Transform *self; lua_object_getTransformPtr(L, 1, &self);
    if (self == nullptr) {
        LUAUTILS_INTERNAL_ERROR(L) // returns
    }
    
    RigidBody *rb = _lua_object_ensure_rigidbody(self);
    if (rb != nullptr) {
        if (transform_get_type(self) == ShapeTransform) {
            shape_reset_box(static_cast<Shape *>(transform_get_ptr(self)));
        } else if (transform_get_type(self) == MeshTransform) {
            mesh_reset_model_aabb(static_cast<Mesh *>(transform_get_ptr(self)));
        } else {
            rigidbody_set_collider(rb, &box_one, false);
        }
    }
    
    LUAUTILS_STACK_SIZE_ASSERT(L, 0)
    return 0;
}

bool _lua_object_is_readonly(lua_State *L, bool pushMetatable) {
    if (pushMetatable) {
        if (lua_getmetatable(L, -1) == 0) {
            LUAUTILS_INTERNAL_ERROR(L);
        }
    }
    
    bool readOnly = false;
    lua_pushstring(L, LUA_OBJECT_FIELD_READ_ONLY);
    if (lua_rawget(L, -2) == LUA_TBOOLEAN) {
        readOnly = lua_toboolean(L, -1);
    }
    
    lua_pop(L, pushMetatable ? 2 : 1);
    
    return readOnly;
}

void _lua_object_set_readonly(lua_State *L, bool b, bool pushMetatable) {
    if (pushMetatable) {
        if (lua_getmetatable(L, -1) == 0) {
            LUAUTILS_INTERNAL_ERROR(L);
        }
    }
    
    lua_pushstring(L, LUA_OBJECT_FIELD_READ_ONLY);
    lua_pushboolean(L, b);
    lua_rawset(L, -3);
    
    if (pushMetatable) {
        lua_pop(L, 1);
    }
}

static int _object_find_first(lua_State *L) {
    vx_assert(L != nullptr);
    LUAUTILS_STACK_SIZE(L)

    const int argsCount = lua_gettop(L);

    if (argsCount < 1) {
        LUAUTILS_ERROR(L, "Object:FindFirst(searchFunction) should be called with `:`");
    }

    Transform *self; lua_object_getTransformPtr(L, 1, &self);
    if (self == nullptr) {
        LUAUTILS_ERROR(L, "Object:FindFirst(searchFunction) should be called with `:`");
    }

    if (argsCount < 2) {
        LUAUTILS_ERROR(L, "Object:FindFirst(searchCallback) - searchCallback should be a function");
    }

    if (argsCount > 3) {
        LUAUTILS_ERROR(L, "Object:FindFirst(searchCallback, [config]) doesn't accept more than 2 parameters");
    }

    if (lua_isfunction(L, 2) == false) {
        LUAUTILS_ERROR(L, "Object:FindFirst(searchCallback) - searchCallback should be a function");
    }

    // Parse options table if provided
    bool deepFirst = false;
    bool includeRoot = false;
    int maxDepth = LUA_MAX_RECURSION_DEPTH;
    if (argsCount == 3) {
        if (lua_istable(L, 3) == false) {
            LUAUTILS_ERROR(L, "Object:FindFirst(searchCallback, [config]) - config should be a table");
        }

        lua_getfield(L, 3, "deepFirst");
        if (lua_isnil(L, -1) == false) {
            if (lua_isboolean(L, -1) == false) {
                lua_pop(L, 1);
                LUAUTILS_ERROR(L, "Object:FindFirst(searchCallback, [config]) - config.deepFirst must be a boolean"); // returns
            }
            deepFirst = lua_toboolean(L, -1);
        }
        lua_pop(L, 1);

        lua_getfield(L, 3, "includeRoot");
        if (lua_isnil(L, -1) == false) {
            if (lua_isboolean(L, -1) == false) {
                lua_pop(L, 1);
                LUAUTILS_ERROR(L, "Object:FindFirst(searchCallback, [config]) - config.includeRoot must be a boolean"); // returns
            }
            includeRoot = lua_toboolean(L, -1);
        }
        lua_pop(L, 1);

        // Get depth option
        lua_getfield(L, 3, "depth");
        if (lua_isnil(L, -1) == false) {
            if (lua_utils_isIntegerStrict(L, -1) == false) {
                lua_pop(L, 1);
                LUAUTILS_ERROR(L, "Object:FindFirst(searchCallback, [config]) - config.depth must be an integer"); // returns
            }
            maxDepth = minimum(static_cast<int>(lua_tointeger(L, -1)), LUA_MAX_RECURSION_DEPTH);
        }
        lua_pop(L, 1);
    }

    LUAUTILS_STACK_SIZE_ASSERT(L, 0)

    struct RecurseData {
        lua_State *L;
        int maxDepth;
        bool found;
    } data = { L, maxDepth, false };

    auto recurseCallback = [](Transform *t, void *ptr, uint32_t depth) -> bool {
        RecurseData* data = static_cast<RecurseData*>(ptr);
        lua_State *L = data->L;

        if (data->maxDepth >= 0 && static_cast<int>(depth) > data->maxDepth) {
            return true; // stop recursion
        }

        // Get callback
        lua_pushvalue(L, 2);

        if (lua_g_object_getOrCreateInstance(L, t) == false) {
            // transform can't be turned into a Lua object but the recursion
            // should not stop as other siblings or descendant still could.
            lua_pop(L, 1); // pop callback function
            return false; // continue recursion
        }

        lua_call(L, 1, 1);
        if (lua_isboolean(L, -1) && lua_toboolean(L, -1) == true) {
            lua_pop(L, 1);
            lua_g_object_getOrCreateInstance(L, t);
            data->found = true;
            return true; // stop recursion
        }
        lua_pop(L, 1);

        return false; // continue recursion
    };

    if (includeRoot) {
        recurseCallback(self, &data, 0);
    }
    transform_recurse_depth(self, recurseCallback, &data, deepFirst, false, 0);

    if (data.found) {
        LUAUTILS_STACK_SIZE_ASSERT(L, 1)
        return 1;
    }

    LUAUTILS_STACK_SIZE_ASSERT(L, 0)
    return 0;
}

static int _object_find(lua_State *L) {
    vx_assert(L != nullptr);
    LUAUTILS_STACK_SIZE(L)

    const int argsCount = lua_gettop(L);

    if (argsCount < 1) {
        LUAUTILS_ERROR(L, "Object:Find(searchFunction) should be called with `:`");
    }

    Transform *self; lua_object_getTransformPtr(L, 1, &self);
    if (self == nullptr) {
        LUAUTILS_ERROR(L, "Object:Find(searchFunction) should be called with `:`");
    }

    if (argsCount < 2) {
        LUAUTILS_ERROR(L, "Object:Find(searchCallback) - searchCallback should be a function");
    }

    if (argsCount > 3) {
        LUAUTILS_ERROR(L, "Object:Find(searchCallback, [config]) doesn't accept more than 2 parameters");
    }

    if (lua_isfunction(L, 2) == false) {
        LUAUTILS_ERROR(L, "Object:Find(searchCallback) - searchCallback should be a function");
    }

    // Parse options table if provided
    bool deepFirst = false;
    bool includeRoot = false;
    int maxDepth = LUA_MAX_RECURSION_DEPTH;
    if (argsCount == 3) {
        if (lua_istable(L, 3) == false) {
            LUAUTILS_ERROR(L, "Object:Find(searchCallback, [config]) - config should be a table");
        }

        lua_getfield(L, 3, "deepFirst");
        if (lua_isnil(L, -1) == false) {
            if (lua_isboolean(L, -1) == false) {
                lua_pop(L, 1);
                LUAUTILS_ERROR(L, "Object:Find(searchCallback, [config]) - config.deepFirst must be a boolean"); // returns
            }
            deepFirst = lua_toboolean(L, -1);
        }
        lua_pop(L, 1);

        lua_getfield(L, 3, "includeRoot");
        if (lua_isnil(L, -1) == false) {
            if (lua_isboolean(L, -1) == false) {
                lua_pop(L, 1);
                LUAUTILS_ERROR(L, "Object:Find(searchCallback, [config]) - config.includeRoot must be a boolean"); // returns
            }
            includeRoot = lua_toboolean(L, -1);
        }
        lua_pop(L, 1);

        // Get depth option
        lua_getfield(L, 3, "depth");
        if (lua_isnil(L, -1) == false) {
            if (lua_utils_isIntegerStrict(L, -1) == false) {
                lua_pop(L, 1);
                LUAUTILS_ERROR(L, "Object:Find(searchCallback, [config]) - config.depth must be an integer"); // returns
            }
            maxDepth = minimum(static_cast<int>(lua_tointeger(L, -1)), LUA_MAX_RECURSION_DEPTH);
        }
        lua_pop(L, 1);
    }

    LUAUTILS_STACK_SIZE_ASSERT(L, 0)

    struct RecurseData {
        lua_State *L;
        int maxDepth;
        int results;

    } data = { L, maxDepth, 0 };

    lua_newtable(L); // results

    auto recurseCallback = [](Transform *t, void *ptr, uint32_t depth) -> bool {
        RecurseData* data = static_cast<RecurseData*>(ptr);
        lua_State *L = data->L;

        if (data->maxDepth >= 0 && static_cast<int>(depth) > data->maxDepth) {
            return true; // stop recursion
        }

        // Get callback
        lua_pushvalue(L, 2);

        if (lua_g_object_getOrCreateInstance(L, t) == false) {
            // transform can't be turned into a Lua object but the recursion
            // should not stop as other siblings or descendant still could.
            lua_pop(L, 1); // pop callback function
            return false; // continue recursion
        }

        // object: -1
        // search function: -2
        // results: -3

        lua_call(L, 1, 1);
        if (lua_isboolean(L, -1) && lua_toboolean(L, -1) == true) {
            lua_pop(L, 1);
            lua_g_object_getOrCreateInstance(L, t);

            // object: -1
            // results: -2
            data->results += 1;
            lua_rawseti(L, -2, data->results);

            return false; // stop recursion
        }
        lua_pop(L, 1);

        return false; // continue recursion
    };

    if (includeRoot) {
        recurseCallback(self, &data, 0);
    }
    transform_recurse_depth(self, recurseCallback, &data, deepFirst, false, 0);

    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
    return 1;
}

/// Object:Recurse(f, options)
/// Recursively traverses the object hierarchy, calling f(object) for each child
/// - "f" : function that takes an object as parameter
/// - "options" : optional table with parameters:
///   - deepFirst : boolean, if true uses depth-first traversal (default: false)
///   - includeRoot : boolean, if true calls function on root object (default: true)
///   - depth : number, maximum recursion depth (default: infinite)
static int _object_recurse(lua_State *L) {
    vx_assert(L != nullptr);
    LUAUTILS_STACK_SIZE(L)

    const int argsCount = lua_gettop(L);
    if (argsCount < 2 || argsCount > 3) {
        LUAUTILS_ERROR(L, "object:Recurse - expects 1-2 arguments: function and optional options table"); // returns
    }

    if (lua_isfunction(L, 2) == false) {
        LUAUTILS_ERROR(L, "object:Recurse - first argument must be a function"); // returns
    }

    bool deepFirst = false;
    bool includeRoot = true;
    int maxDepth = LUA_MAX_RECURSION_DEPTH;
    if (argsCount == 3) {
        if (lua_istable(L, 3) == false) {
            LUAUTILS_ERROR(L, "object:Recurse - second argument must be a config table"); // returns
        }

        lua_getfield(L, 3, "deepFirst");
        if (lua_isnil(L, -1) == false) {
            if (lua_isboolean(L, -1) == false) {
                lua_pop(L, 1);
                LUAUTILS_ERROR(L, "object:Recurse - config.deepFirst must be a boolean"); // returns
            }
            deepFirst = lua_toboolean(L, -1);
        }
        lua_pop(L, 1);

        lua_getfield(L, 3, "includeRoot");
        if (lua_isnil(L, -1) == false) {
            if (lua_isboolean(L, -1) == false) {
                lua_pop(L, 1);
                LUAUTILS_ERROR(L, "object:Recurse - config.includeRoot must be a boolean"); // returns
            }
            includeRoot = lua_toboolean(L, -1);
        }
        lua_pop(L, 1);

        lua_getfield(L, 3, "depth");
        if (lua_isnil(L, -1) == false) {
            if (lua_utils_isIntegerStrict(L, -1) == false) {
                lua_pop(L, 1);
                LUAUTILS_ERROR(L, "object:Recurse - config.depth must be an integer"); // returns
            }
            maxDepth = minimum(static_cast<int>(lua_tointeger(L, -1)), LUA_MAX_RECURSION_DEPTH);
        }
        lua_pop(L, 1);
    }

    Transform *self; lua_object_getTransformPtr(L, 1, &self);
    if (self == nullptr) {
        LUAUTILS_INTERNAL_ERROR(L) // returns
    }

    CGame *g = nullptr;
    if (lua_utils_getGamePtr(L, &g) == false || g == nullptr) {
        LUAUTILS_INTERNAL_ERROR(L); // not supposed to happen
    }
    Scene *sc = game_get_scene(g);

    struct RecurseData {
        lua_State *L;
        int maxDepth;
    } data = { L, maxDepth };

    auto recurseCallback = [](Transform *t, void *ptr, uint32_t depth) -> bool {
        auto *data = static_cast<RecurseData*>(ptr);
        lua_State *L = data->L;

        if (data->maxDepth >= 0 && static_cast<int>(depth) > data->maxDepth) {
            return true; // stop recursion
        }

        // Get callback
        lua_pushvalue(L, 2);

        if (lua_g_object_getOrCreateInstance(L, t) == false) {
            // transform can't be turned into a Lua object but the recursion
            // should not stop as other siblings or descendant still could.
            lua_pop(L, 1); // pop callback function
            return false; // continue recursion
        }

        // Call function(object)
        if (lua_pcall(L, 1, 0, 0) != LUA_OK) {
            if (lua_isstring(L, -1)) {
                lua_log_error_CStr(L, lua_tostring(L, -1));
            }
            lua_pop(L, 1); // pop error message
            return true; // stop recursion
        }

        return false; // continue recursion
    };

    if (scene_toggle_recursion_lock(sc, true) == false) {
        LUAUTILS_ERROR(L, "object:Recurse - maximum nested calls reached"); // returns
    }
    {
        if (includeRoot && deepFirst == false) {
            recurseCallback(self, &data, 0);
        }
        transform_recurse_depth(self, recurseCallback, &data, deepFirst, false, 0);
        if (includeRoot && deepFirst) {
            recurseCallback(self, &data, 0);
        }
    }
    scene_toggle_recursion_lock(sc, false);

    LUAUTILS_STACK_SIZE_ASSERT(L, 0)
    return 0;
}

const char* lua_object_copy(lua_State *L, const int objectIdx, const int configIdx) {
    bool recurse = false;

    if (configIdx != 0) {
        if (lua_istable(L, configIdx) == false) {
            return "config should be a table";
        }

        lua_getfield(L, configIdx, "recurse");
        if (lua_isnil(L, -1) == false) {
            if (lua_isboolean(L, -1) == false) {
                lua_pop(L, 1);
                LUAUTILS_ERROR(L, "object:Copy - config.recurse must be a boolean"); // returns
            }
            recurse = lua_toboolean(L, -1);
        } else {
            // if `recurse` is nil, look for legacy `includeChildren` (deprecated)
            lua_pop(L, 1); // pop nil
            lua_getfield(L, configIdx, "includeChildren");
            if (lua_isnil(L, -1) == false) {
                if (lua_isboolean(L, -1) == false) {
                    lua_pop(L, 1);
                    LUAUTILS_ERROR(L, "object:Copy - config.includeChildren must be a boolean"); // returns
                }
                recurse = lua_toboolean(L, -1);
            }
        }
        lua_pop(L, 1);
    }

    Transform *self; lua_object_getTransformPtr(L, objectIdx, &self);
    if (self == nullptr) {
        LUAUTILS_INTERNAL_ERROR(L) // returns
    }

    Transform *copy = recurse ? transform_utils_copy_recurse(self) : transform_new_copy(self);

    switch(transform_get_type(copy)) {
        case AudioSourceTransform: // TODO see transform_new_copy
        case AudioListenerTransform:
        case HierarchyTransform:
        case PointTransform:
            lua_object_pushNewInstance(L, copy);
            break;
        case ShapeTransform:
        {
            Shape *s = static_cast<Shape*>(transform_get_ptr(copy));
            if (shape_is_lua_mutable(s)) {
                lua_mutableShape_pushNewInstance(L, s);
            } else {
                lua_shape_pushNewInstance(L, s);
            }
            break;
        }
        case QuadTransform:
            lua_quad_pushNewInstance(L, static_cast<Quad*>(transform_get_ptr(copy)));
            break;
        case CameraTransform:
            lua_camera_pushNewInstance(L, static_cast<Camera*>(transform_get_ptr(copy)));
            break;
        case LightTransform:
            lua_light_pushNewInstance(L, static_cast<Light*>(transform_get_ptr(copy)));
            break;
        case WorldTextTransform:
            lua_text_pushNewInstance(L, static_cast<WorldText*>(transform_get_ptr(copy)));
            break;
        case MeshTransform:
            lua_mesh_pushNewInstance(L, static_cast<Mesh*>(transform_get_ptr(copy)));
            break;
        default:
            vx_assert(false); // always implement for new types
    }
    transform_release(copy); // owned by lua object
    return nullptr;
}

static int _object_copy(lua_State *L) {
    vx_assert(L != nullptr);
    LUAUTILS_STACK_SIZE(L)

    const int argsCount = lua_gettop(L);
    if (argsCount < 1 || lua_object_isObject(L, 1) == false) {
        LUAUTILS_ERROR(L, "Object:Copy(config) should be called with `:`"); // returns
    }

    if (argsCount > 2) {
        LUAUTILS_ERROR(L, "Object:Copy(config) - expects up to one argument"); // returns
    }

    bool hasConfigTable = false;
    if (argsCount == 2) {
        if (lua_istable(L, 2)) {
            hasConfigTable = true;
        } else {
            LUAUTILS_ERROR(L, "Object:Copy(config) - config must be a table"); // returns
        }
    }

    const char *err = lua_object_copy(L, 1, hasConfigTable ? 2 : 0);
    if (err != nullptr) {
        LUAUTILS_ERROR_F(L, "Object:Copy(config) - %s", err); // returns
    }

    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
    return 1;
}

static int _object_destroy(lua_State *L) {
    vx_assert(L != nullptr);
    LUAUTILS_STACK_SIZE(L)

    const int argsCount = lua_gettop(L);
    if (argsCount < 1) {
        LUAUTILS_ERROR(L, "Object:Destroy() should be called with `:`"); // returns
    }

    if (argsCount > 1) {
        LUAUTILS_ERROR(L, "Object:Destroy() - wrong number of arguments"); // returns
    }

    Transform *self; LUA_ITEM_TYPE type = lua_object_getTransformPtr(L, 1, &self);
    if (self == nullptr) {
        if (type == ITEM_TYPE_OBJECT_DESTROYED) {
            LUAUTILS_ERROR(L, "Object:Destroy() - object has already been destroyed"); // returns
        } else if (type == ITEM_TYPE_NOT_AN_OBJECT) {
            LUAUTILS_ERROR(L, "Object:Destroy() - trying to destroy something that's not an Object"); // returns
        }
        LUAUTILS_INTERNAL_ERROR(L); // not supposed to happen
    }

    // temporarily retaining here in case not parented,
    // to avoid it being freed while we release each object transform below
    transform_retain(self);

    CGame *g = nullptr;
    if (lua_utils_getGamePtr(L, &g) == false || g == nullptr) {
        LUAUTILS_INTERNAL_ERROR(L); // not supposed to happen
    }
    Scene *sc = game_get_scene(g);

    FifoList *to_remove = fifo_list_new();

    auto recurseDestroy = [](Transform *t, void *ptr, uint32_t depth) -> bool {
        if (depth >= LUA_MAX_RECURSION_DEPTH) {
            return true; // stop recursion
        }
        if (transform_is_managed(t)) {
            fifo_list_push(static_cast<FifoList*>(ptr), t);
        }
        return false; // continue recursion
    };

    bool deepFirst = true;
    transform_recurse_depth(self, recurseDestroy, to_remove, deepFirst, false, 0);
    recurseDestroy(self, to_remove, 0); // remove root

    Transform *t = static_cast<Transform*>(fifo_list_pop(to_remove));

    while (t != nullptr) {
        uint16_t transformID = transform_get_id(t);
        // vxlog_debug("DESTROY %d", transformID);
        transform_unset_managed_ptr(t);

        // Remove Object from index by transform ID
        lua_g_object_pushObjectsIndexedByTransformID(L);
        lua_rawgeti(L, -1, transformID);
        if (lua_isnil(L, -1)) {
            lua_pop(L, 1);
        } else {
            LUA_ITEM_TYPE type = lua_utils_getObjectType(L, -1);
            // call appropriate _gc functions, releasing the transforms
            // (the Object Lua reference can still exist but Object.IsDestroyed becomes true)
            switch (type) {
                case ITEM_TYPE_OBJECT:
                {
                    _object_release_transform(lua_touserdata(L, -1));
                    break;
                }
                case ITEM_TYPE_SHAPE:
                case ITEM_TYPE_MUTABLESHAPE:
                {
                    lua_shape_release_transform(lua_touserdata(L, -1));
                    break;
                }
                case ITEM_TYPE_QUAD:
                {
                    lua_quad_release_transform(lua_touserdata(L, -1));
                    break;
                }
                case ITEM_TYPE_TEXT:
                {
                    lua_text_release_transform(lua_touserdata(L, -1));
                    break;
                }
                case ITEM_TYPE_AUDIOSOURCE:
                {
#if !defined(P3S_CLIENT_HEADLESS)
                    lua_audioSource_release_transform(lua_touserdata(L, -1));
#endif
                    break;
                }
                case ITEM_TYPE_LIGHT:
                {
                    lua_light_release_transform(lua_touserdata(L, -1));
                    break;
                }
                case ITEM_TYPE_MESH: {
                    lua_mesh_release_transform(lua_touserdata(L, -1));
                    break;
                }
                case ITEM_TYPE_G_AUDIOLISTENER:
                {
                    LUAUTILS_ERROR(L, "AudioListener can't be destroyed"); // returns
                }
                    // TODO: handle other types
                default:
                    vxlog_debug("DESTROY OBJECT TYPE NOT SUPPORTED: %d", type);
                    break;
            }
            lua_pop(L, 1); // pop Object
            lua_pushnil(L);
            lua_rawseti(L, -2, transformID);
        }
        lua_pop(L, 1);

        // remove Object's storage
        lua_g_object_pushObjectStorage(L);
        lua_pushnil(L);
        lua_rawseti(L, -2,  transformID);
        lua_pop(L, 1);

        t = static_cast<Transform*>(fifo_list_pop(to_remove));
    }

    fifo_list_free(to_remove, nullptr);

    scene_remove_transform(sc, self, false);

    transform_release(self); // from _object_destroy

    LUAUTILS_STACK_SIZE_ASSERT(L, 0)
    return 0;
}

static int _children_newindex(lua_State *L) {
    LUAUTILS_ERROR(L, "Children table is read-only");
    return 0;
}
