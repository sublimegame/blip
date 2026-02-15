// -------------------------------------------------------------
//  Cubzh
//  lua_mesh.cpp
//  Created by Arthur Cormerais on February 14, 2025.
// -------------------------------------------------------------

#include "lua_mesh.hpp"

#include <cstring>

#include "VXGame.hpp"

#include "lua_utils.hpp"
#include "lua_object.hpp"
#include "lua_number3.hpp"
#include "lua_constants.h"
#include "lua_number2.hpp"
#include "lua_color.hpp"
#include "lua_box.hpp"

#include "weakptr.h"
#include "game.h"
#include "scene.h"
#include "camera.h"
#include "colors.h"

// Mesh instance properties
#define LUA_MESH_FIELD_PIVOT       "Pivot"         // Number3
#define LUA_MESH_FIELD_LAYERS      "Layers"        // integer or table of integers (0 to CAMERA_LAYERS_MASK_API_BITS)
#define LUA_MESH_FIELD_VCOUNT      "VertexCount"   // integer (read-only)
#define LUA_MESH_FIELD_SHADOW      "Shadow"        // boolean
#define LUA_MESH_FIELD_GETVERTEX   "GetVertex"     // function
#define LUA_MESH_FIELD_FLIP        "FlipTriangles" // function
#define LUA_MESH_FIELD_BOUNDINGBOX "BoundingBox"   // Box (read-only)
#define LUA_MESH_FIELD_MIN         "Min"           // Number3 (read-only)
#define LUA_MESH_FIELD_CENTER      "Center"        // Number3 (read-only)
#define LUA_MESH_FIELD_MAX         "Max"           // Number3 (read-only)
#define LUA_MESH_FIELD_SIZE        "Size"          // Number3 (read-only)
#define LUA_MESH_FIELD_WIDTH       "Width"         // number (read-only)
#define LUA_MESH_FIELD_HEIGHT      "Height"        // number (read-only)
#define LUA_MESH_FIELD_DEPTH       "Depth"         // number (read-only)
#define LUA_MESH_FIELD_LOCALAABB   "ComputeLocalBoundingBox" // function
#define LUA_MESH_FIELD_WORLDAABB   "ComputeWorldBoundingBox" // function
#define LUA_MESH_FIELD_MATERIAL    "Material"      // table
#define LUA_MESH_FIELD_UNLIT       "IsUnlit"       // boolean

typedef struct mesh_userdata {
    // Userdata instance requirements
    vx::UserDataStore *store;
    mesh_userdata *next;
    mesh_userdata *previous;

    Mesh *mesh;
} mesh_userdata;

// MARK: - Private functions prototypes -

// MARK: Metatable
static int _g_mesh_metatable_index(lua_State * const L);
static int _g_mesh_metatable_newindex(lua_State * const L);
static int _g_mesh_metatable_call(lua_State * const L);
static int _mesh_metatable_index(lua_State * const L);
static int _mesh_metatable_newindex(lua_State * const L);
static void _mesh_gc(void *_ud);

// MARK: Lua fields
Mesh* _lua_mesh_get_ptr_from_number3_handler(const number3_C_handler_userdata *userdata);
void _lua_mesh_set_pivot(Mesh *m, const float *x, const float *y, const float *z);
void _lua_mesh_get_pivot(Mesh *m, float *x, float *y, float *z);
bool _lua_mesh_get_or_create_field_pivot(lua_State *L, int idx, const Mesh* m);
void _lua_mesh_set_pivot_handler(const float *x, const float *y, const float *z, lua_State * const L, const number3_C_handler_userdata *userdata);
void _lua_mesh_get_pivot_handler(float *x, float *y, float *z, lua_State * const L, const number3_C_handler_userdata *userdata);
bool _lua_mesh_get_or_create_field_bounding_box(lua_State *L, int idx, const Mesh* m);
void _lua_mesh_set_bounding_box_handler(const float3 *min, const float3 *max, lua_State * const L, void *userdata);
void _lua_mesh_get_bounding_box_handler(float3 *min, float3 *max, lua_State * const L, const void *userdata);
void _lua_mesh_free_bounding_box_handler(void *value);
void _lua_mesh_set_bounding_box_min_n3_handler(const float *x, const float *y, const float *z, lua_State * const L, const number3_C_handler_userdata *userdata);
void _lua_mesh_get_bounding_box_min_n3_handler(float *x, float *y, float *z, lua_State * const L, const number3_C_handler_userdata *userdata);
void _lua_mesh_set_bounding_box_max_n3_handler(const float *x, const float *y, const float *z, lua_State * const L, const number3_C_handler_userdata *userdata);
void _lua_mesh_get_bounding_box_max_n3_handler(float *x, float *y, float *z, lua_State * const L, const number3_C_handler_userdata *userdata);

// MARK: Lua functions
static int _mesh_getVertex(lua_State * const L);
static int _mesh_flipTriangles(lua_State * const L);
static int _mesh_getLocalAABB(lua_State * const L);
static int _mesh_getWorldAABB(lua_State * const L);

// MARK: - Mesh global table -

void lua_g_mesh_createAndPush(lua_State * const L) {
    vx_assert(L != nullptr);
    LUAUTILS_STACK_SIZE(L)
    
    lua_newtable(L); // global "Mesh" table
    lua_newtable(L); // metatable
    {
        lua_pushstring(L, "__index");
        lua_pushcfunction(L, _g_mesh_metatable_index, "_g_mesh_metatable_index");
        lua_rawset(L, -3);
        
        lua_pushstring(L, "__newindex");
        lua_pushcfunction(L, _g_mesh_metatable_newindex, "_g_mesh_metatable_newindex");
        lua_rawset(L, -3);
        
        lua_pushstring(L, "__call");
        lua_pushcfunction(L, _g_mesh_metatable_call, "_g_mesh_metatable_call");
        lua_rawset(L, -3);
        
        // hide the metatable from the Lua sandbox user
        lua_pushstring(L, "__metatable");
        lua_pushboolean(L, false);
        lua_rawset(L, -3);
        
        // used to log tables
        lua_pushstring(L, P3S_LUA_OBJ_METAFIELD_TYPE);
        lua_pushinteger(L, ITEM_TYPE_G_MESH);
        lua_rawset(L, -3);
    }
    lua_setmetatable(L, -2);
    
    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
}

int lua_g_mesh_snprintf(lua_State *L, char *result, size_t maxLen, bool spacePrefix) {
    RETURN_VALUE_IF_NULL(L, 0)
    return snprintf(result, maxLen, "%s[Mesh] (global)", spacePrefix ? " " : "");
}

// MARK: - Mesh instances tables -

bool lua_mesh_pushNewInstance(lua_State * const L, Mesh *m) {
    vx_assert(L != nullptr);
    vx_assert(m != nullptr);
    
    Transform *t = mesh_get_transform(m);
    
    // lua Mesh takes ownership
    if (transform_retain(t) == false) {
        LUAUTILS_INTERNAL_ERROR(L);
    }
    
    LUAUTILS_STACK_SIZE(L)

    vx::Game *g = nullptr;
    if (lua_utils_getGameCppWrapperPtr(L, &g) == false) {
        LUAUTILS_INTERNAL_ERROR(L);
    }

    mesh_userdata *ud = static_cast<mesh_userdata *>(lua_newuserdatadtor(L, sizeof(mesh_userdata), _mesh_gc));
    ud->mesh = m;

    // connect to userdata store
    ud->store = g->userdataStoreForMeshes;
    ud->previous = nullptr;
    mesh_userdata* next = static_cast<mesh_userdata*>(ud->store->add(ud));
    ud->next = next;
    if (next != nullptr) {
        next->previous = ud;
    }

    lua_newtable(L); // metatable
    {
        lua_pushstring(L, "__index");
        lua_pushcfunction(L, _mesh_metatable_index, "_mesh_metatable_index");
        lua_rawset(L, -3);
        
        lua_pushstring(L, "__newindex");
        lua_pushcfunction(L, _mesh_metatable_newindex, "_mesh_metatable_newindex");
        lua_rawset(L, -3);
        
        // hide the metatable from the Lua sandbox user
        lua_pushstring(L, "__metatable");
        lua_pushboolean(L, false);
        lua_rawset(L, -3);

        lua_pushstring(L, "__type");
        lua_pushstring(L, "Mesh");
        lua_rawset(L, -3);

        lua_pushstring(L, "__typen");
        lua_pushinteger(L, ITEM_TYPE_MESH);
        lua_rawset(L, -3);
    }
    lua_setmetatable(L, -2);
    
    if (lua_g_object_addIndexEntry(L, t, -1) == false) {
        vxlog_error("Failed to add Lua Mesh to Object registry");
        lua_pop(L, 1); // pop Lua Mesh
        LUAUTILS_STACK_SIZE_ASSERT(L, 0)
        return false;
    }

    scene_register_managed_transform(game_get_scene(g->getCGame()), t);

    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
    return true;
}

bool lua_mesh_getOrCreateInstance(lua_State * const L, Mesh *m) {
    if (lua_g_object_getIndexEntry(L, mesh_get_transform(m))) {
        return true;
    } else {
        return lua_mesh_pushNewInstance(L, m);
    }
}

bool lua_mesh_isMesh(lua_State * const L, const int idx) {
    vx_assert(L != nullptr);
    return lua_utils_getObjectType(L, idx) == ITEM_TYPE_MESH; // currently no lua Mesh extensions
}

Mesh *lua_mesh_getMeshPtr(lua_State * const L, const int idx) {
    vx_assert(L != nullptr);
    LUAUTILS_STACK_SIZE(L)
    
    if (lua_utils_getObjectType(L, idx) == ITEM_TYPE_MESH) {
        mesh_userdata *ud = static_cast<mesh_userdata *>(lua_touserdata(L, idx));
        return ud->mesh;
    }
    
    LUAUTILS_STACK_SIZE_ASSERT(L, 0)
    return nullptr;
}

int lua_mesh_snprintf(lua_State *L, char *result, size_t maxLen, bool spacePrefix) {
    vx_assert(L != nullptr);
    Transform *root; GET_OBJECT_ROOT_TRANSFORM(-1, root);
    return snprintf(result, maxLen, "%s[Mesh %d Name:%s] (instance)", spacePrefix ? " " : "",
                    transform_get_id(root), transform_get_name(root));
}

// MARK: - Private functions -

static int _g_mesh_metatable_index(lua_State *L) {
    vx_assert(L != nullptr);
    LUAUTILS_STACK_SIZE(L)
    
    lua_pushnil(L);
    
    LUAUTILS_STACK_SIZE_ASSERT(L, 1);
    return 1;
}

static int _g_mesh_metatable_newindex(lua_State *L) {
    vx_assert(L != nullptr);
    return 0;
}

static int _g_mesh_metatable_call(lua_State *L) {
    vx_assert(L != nullptr);
    LUAUTILS_STACK_SIZE(L)
    
    Mesh *m = mesh_new();
    if (lua_mesh_pushNewInstance(L, m) == false) {
        lua_pushnil(L); // GC will release mesh
    }
    mesh_release(m); // now owned by lua Mesh
    
    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
    return 1;
}

static int _mesh_metatable_index(lua_State * const L) {
    vx_assert(L != nullptr);
    LUAUTILS_ASSERT_ARGS_COUNT(L, 2)
    LUAUTILS_ASSERT_ARG_TYPE(L, 1, ITEM_TYPE_MESH)
    LUAUTILS_STACK_SIZE(L)
    
    Mesh *m = lua_mesh_getMeshPtr(L, 1);
    if (m == nullptr) {
        lua_pushnil(L);
        LUAUTILS_STACK_SIZE_ASSERT(L, 1)
        return 1;
    }
    
    const char *key = lua_tostring(L, 2);
    if (_lua_object_metatable_index(L, key)) {
        // a field from Object was pushed onto the stack
    } else if (_lua_mesh_metatable_index(L, m, key) == false) {
        lua_pushnil(L);
    }
    
    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
    return 1;
}

static int _mesh_metatable_newindex(lua_State * const L) {
    vx_assert(L != nullptr);
    LUAUTILS_ASSERT_ARGS_COUNT(L, 3)
    LUAUTILS_ASSERT_ARG_TYPE(L, 1, ITEM_TYPE_MESH)
    LUAUTILS_STACK_SIZE(L)
    
    Mesh *m = lua_mesh_getMeshPtr(L, 1);
    if (m == nullptr) {
        lua_pushnil(L);
        LUAUTILS_STACK_SIZE_ASSERT(L, 1)
        return 1;
    }
    Transform *t = mesh_get_transform(m);
    
    const char *key = lua_tostring(L, 2);
    
    if (_lua_object_metatable_newindex(L, t, key) == false) {
        if (_lua_mesh_metatable_newindex(L, m, key) == false) {
            LUAUTILS_ERROR_F(L, "Mesh: %s field is not settable", key);
        }
    }
    
    LUAUTILS_STACK_SIZE_ASSERT(L, 0)
    return 0;
}

static void _mesh_gc(void *_ud) {
    mesh_userdata *ud = static_cast<mesh_userdata*>(_ud);
    if (ud->mesh != nullptr) {
        mesh_release(ud->mesh);
        ud->mesh = nullptr;
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

// MARK: - Exposed Internal Functions -

bool _lua_mesh_metatable_index(lua_State * const L, Mesh *m, const char *key) {
    if (strcmp(key, LUA_MESH_FIELD_PIVOT) == 0) {
        if (_lua_mesh_get_or_create_field_pivot(L, 1, m) == false) {
            lua_pushnil(L);
        }
        return true;
    } else if (strcmp(key, LUA_MESH_FIELD_LAYERS) == 0) {
        lua_pushinteger(L, mesh_get_layers(m));
        return true;
    } else if (strcmp(key, LUA_MESH_FIELD_VCOUNT) == 0) {
        lua_pushinteger(L, mesh_get_vertex_count(m));
        return true;
    } else if (strcmp(key, LUA_MESH_FIELD_SHADOW) == 0) {
        lua_pushboolean(L, mesh_has_shadow(m));
        return true;
    } else if (strcmp(key, LUA_MESH_FIELD_GETVERTEX) == 0) {
        lua_pushcfunction(L, _mesh_getVertex, "_mesh_getVertex");
        return true;
    } else if (strcmp(key, LUA_MESH_FIELD_FLIP) == 0) {
        lua_pushcfunction(L, _mesh_flipTriangles, "_mesh_flipTriangles");
        return true;
    } else if (strcmp(key, LUA_MESH_FIELD_BOUNDINGBOX) == 0) {
        if (_lua_mesh_get_or_create_field_bounding_box(L, 1, m) == false) {
            lua_pushnil(L);
        }
        return true;
    } else if (strcmp(key, LUA_MESH_FIELD_LOCALAABB) == 0) {
        lua_pushcfunction(L, _mesh_getLocalAABB, "_mesh_getLocalAABB");
        return true;
    } else if (strcmp(key, LUA_MESH_FIELD_WORLDAABB) == 0) {
        lua_pushcfunction(L, _mesh_getWorldAABB, "_mesh_getWorldAABB");
        return true;
    } else if (strcmp(key, LUA_MESH_FIELD_MATERIAL) == 0) {
        Material *mat = mesh_get_material(m);
        if (mat == nullptr) {
            lua_pushnil(L);
            return true;
        }

        lua_newtable(L);
        {
            RGBAColor color;

            lua_pushstring(L, "metallic");
            lua_pushnumber(L, material_get_metallic(mat));
            lua_rawset(L, -3);

            lua_pushstring(L, "roughness");
            lua_pushnumber(L, material_get_roughness(mat));
            lua_rawset(L, -3);

            lua_pushstring(L, "albedo");
            color = uint32_to_color(material_get_albedo(mat));
            lua_color_create_and_push_table(L, color.r, color.g, color.b, color.a, false, true);
            lua_rawset(L, -3);
            
            lua_pushstring(L, "emissive");
            color = uint32_to_color(material_get_emissive(mat));
            lua_color_create_and_push_table(L, color.r, color.g, color.b, color.a, false, true);
            lua_rawset(L, -3);

            lua_pushstring(L, "cutout");
            lua_pushnumber(L, material_get_alpha_cutout(mat));
            lua_rawset(L, -3);

            lua_pushstring(L, "opaque");
            lua_pushboolean(L, material_is_opaque(mat));
            lua_rawset(L, -3);

            lua_pushstring(L, "doublesided");
            lua_pushboolean(L, material_is_double_sided(mat));
            lua_rawset(L, -3);

            lua_pushstring(L, "unlit");
            lua_pushboolean(L, material_is_unlit(mat));
            lua_rawset(L, -3);

            lua_pushstring(L, "filtering");
            lua_pushboolean(L, material_has_filtering(mat));
            lua_rawset(L, -3);
        }
    
        return true;
    } else if (strcmp(key, LUA_MESH_FIELD_UNLIT) == 0) {
        Material *mat = mesh_get_material(m);
        if (mat == nullptr) {
            lua_pushboolean(L, false);
            return true;
        }
        lua_pushboolean(L, material_is_unlit(mat));
        return true;
    } else if (strcmp(key, LUA_MESH_FIELD_MIN) == 0) {
        const Box *aabb = mesh_get_model_aabb(m);
        lua_number3_pushNewObject(L, aabb->min.x, aabb->min.y, aabb->min.z);
        return true;
    } else if (strcmp(key, LUA_MESH_FIELD_MAX) == 0) {
        const Box *aabb = mesh_get_model_aabb(m);
        lua_number3_pushNewObject(L, aabb->max.x, aabb->max.y, aabb->max.z);
        return true;
    } else if (strcmp(key, LUA_MESH_FIELD_CENTER) == 0) {
        const Box *aabb = mesh_get_model_aabb(m);
        float3 center; box_get_center(aabb, &center);
        lua_number3_pushNewObject(L, center.x, center.y, center.z);
        return true;
    } else if (strcmp(key, LUA_MESH_FIELD_SIZE) == 0) {
        const Box *aabb = mesh_get_model_aabb(m);
        float3 size; box_get_size_float(aabb, &size);
        lua_number3_pushNewObject(L, size.x, size.y, size.z);
        return true;
    } else if (strcmp(key, LUA_MESH_FIELD_WIDTH) == 0) {
        const Box *aabb = mesh_get_model_aabb(m);
        lua_pushnumber(L, aabb->max.x - aabb->min.x);
        return true;
    } else if (strcmp(key, LUA_MESH_FIELD_HEIGHT) == 0) {
        const Box *aabb = mesh_get_model_aabb(m);
        lua_pushnumber(L, aabb->max.y - aabb->min.y);
        return true;
    } else if (strcmp(key, LUA_MESH_FIELD_DEPTH) == 0) {
        const Box *aabb = mesh_get_model_aabb(m);
        lua_pushnumber(L, aabb->max.z - aabb->min.z);
        return true;
    }
    return false;
}

bool _lua_mesh_metatable_newindex(lua_State * const L, Mesh *m, const char *key) {
    if (strcmp(key, LUA_MESH_FIELD_PIVOT) == 0) {
        float3 pivot;
        if (lua_number3_or_table_getXYZ(L, 3, &pivot.x, &pivot.y, &pivot.z)) {
            _lua_mesh_set_pivot(m, &pivot.x, &pivot.y, &pivot.z);
        } else {
            LUAUTILS_ERROR_F(L, "Mesh.%s cannot be set (new value is not a Number3)", key);
        }
        return true;
    } else if (strcmp(key, LUA_MESH_FIELD_LAYERS) == 0) {
        uint16_t mask = 0;
        if (lua_utils_getMask(L, 3, &mask, CAMERA_LAYERS_MASK_API_BITS) == false) {
            LUAUTILS_ERROR_F(L, "Mesh.%s cannot be set (value must be an integer or table of integers between 0 and %d)", key, CAMERA_LAYERS_MASK_API_BITS);
        }
        mesh_set_layers(m, mask);
        return true;
    } else if (strcmp(key, LUA_MESH_FIELD_VCOUNT) == 0) {
        LUAUTILS_ERROR_F(L, "Mesh.%s cannot be set (field is read-only)", key);
        return true;
    } else if (strcmp(key, LUA_MESH_FIELD_SHADOW) == 0) {
        if (lua_isboolean(L, 3)) {
            mesh_set_shadow(m, lua_toboolean(L, 3));
        } else {
            LUAUTILS_ERROR_F(L, "Mesh.%s cannot be set (new value is not a boolean)", key);
        }
        return true;
    } else if (strcmp(key, LUA_MESH_FIELD_MATERIAL) == 0) {
        if (lua_istable(L, 3) == false) {
            LUAUTILS_ERROR_F(L, "Mesh.%s cannot be set (new value is not a table)", key);
        }

        Material *mat = mesh_get_material(m);
        if (mat == nullptr) {
            mat = material_new();
            mesh_set_material(m, mat);
            material_release(mat); // owned by mesh
        }

        int type = lua_getfield(L, 3, "metallic");
        if (type == LUA_TNUMBER) {
            const float value = static_cast<float>(lua_tonumber(L, -1));
            if (value < 0.0f || value > 1.0f) {
                LUAUTILS_ERROR_F(L, "Mesh.%s.metallic must be between 0 and 1", key);
            }
            material_set_metallic(mat, value);
        }
        lua_pop(L, 1);

        type = lua_getfield(L, 3, "roughness");
        if (type == LUA_TNUMBER) {
            const float value = static_cast<float>(lua_tonumber(L, -1));
            if (value < 0.0f || value > 1.0f) {
                LUAUTILS_ERROR_F(L, "Mesh.%s.roughness must be between 0 and 1", key);
            }
            material_set_roughness(mat, value);
        }
        lua_pop(L, 1);

        type = lua_getfield(L, 3, "albedo");
        if (type == LUA_TUSERDATA) {
            RGBAColor color;
            if (lua_color_getRGBAL(L, -1, &color.r, &color.g, &color.b, &color.a, nullptr)) {
                material_set_albedo(mat, color_to_uint32(&color));
            } else {
                LUAUTILS_ERROR_F(L, "Mesh.%s.albedo must be a Color", key);
            }
        }
        lua_pop(L, 1);

        type = lua_getfield(L, 3, "emissive");
        if (type == LUA_TUSERDATA) {
            RGBAColor color;
            if (lua_color_getRGBAL(L, -1, &color.r, &color.g, &color.b, &color.a, nullptr)) {
                material_set_emissive(mat, color_to_uint32(&color));
            } else {
                LUAUTILS_ERROR_F(L, "Mesh.%s.emissive must be a Color", key);
            }
        }
        lua_pop(L, 1);

        type = lua_getfield(L, 3, "cutout");
        if (type == LUA_TNUMBER) {
            const float value = static_cast<float>(lua_tonumber(L, -1));
            if (value < 0.0f || value > 1.0f) {
                LUAUTILS_ERROR_F(L, "Mesh.%s.cutout must be between 0 and 1", key);
            }
            material_set_alpha_cutout(mat, value);
        }
        lua_pop(L, 1);

        type = lua_getfield(L, 3, "opaque");
        if (type == LUA_TBOOLEAN) {
            material_set_opaque(mat, lua_toboolean(L, -1));
        }
        lua_pop(L, 1);

        type = lua_getfield(L, 3, "doublesided");
        if (type == LUA_TBOOLEAN) {
            material_set_double_sided(mat, lua_toboolean(L, -1));
        }
        lua_pop(L, 1);

        type = lua_getfield(L, 3, "unlit");
        if (type == LUA_TBOOLEAN) {
            material_set_unlit(mat, lua_toboolean(L, -1));
        }
        lua_pop(L, 1);

        type = lua_getfield(L, 3, "filtering");
        if (type == LUA_TBOOLEAN) {
            material_set_filtering(mat, lua_toboolean(L, -1));
        }
        lua_pop(L, 1);
        
        return true;
    } else if (strcmp(key, LUA_MESH_FIELD_UNLIT) == 0) {
        if (lua_isboolean(L, 3) == false) {
            LUAUTILS_ERROR_F(L, "Mesh.%s cannot be set (new value is not a boolean)", key);
        }

        Material *mat = mesh_get_material(m);
        if (mat == nullptr) {
            mat = material_new();
            mesh_set_material(m, mat);
            material_release(mat); // owned by mesh
        }

        material_set_unlit(mat, lua_toboolean(L, 3));

        return true;
    }
    return false; // nothing was set
}

Mesh* _lua_mesh_get_ptr_from_number3_handler(const number3_C_handler_userdata *userdata) {
    Transform *t = static_cast<Transform*>(weakptr_get(static_cast<Weakptr*>(userdata->ptr)));
    return t == nullptr || transform_get_type(t) != MeshTransform ? nullptr : static_cast<Mesh*>(transform_get_ptr(t));
}

void _lua_mesh_set_pivot(Mesh *m, const float *x, const float *y, const float *z) {
    float3 pivot = mesh_get_pivot(m);
    
    // Get shift to apply it to children
    float3 diff;
    diff.x = (x != nullptr ? pivot.x - *x : 0);
    diff.y = (y != nullptr ? pivot.y - *y : 0);
    diff.z = (z != nullptr ? pivot.z - *z : 0);

    mesh_set_pivot(m,
                   x != nullptr ? *x : pivot.x,
                   y != nullptr ? *y : pivot.y,
                   z != nullptr ? *z : pivot.z);

    Transform *t = mesh_get_transform(m);
    DoublyLinkedListNode *n = transform_get_children_iterator(t);
    Transform *childTransform = nullptr;
    while (n != nullptr) {
        childTransform = static_cast<Transform *>(doubly_linked_list_node_pointer(n));
        const float3 *pos = transform_get_local_position(childTransform, true);
        transform_set_local_position(childTransform, pos->x + diff.x, pos->y + diff.y, pos->z + diff.z);
        n = doubly_linked_list_node_next(n);
    }
}

void _lua_mesh_get_pivot(Mesh *m, float *x, float *y, float *z) {
    float3 pivot = mesh_get_pivot(m);
    
    if (x != nullptr) { *x = pivot.x; }
    if (y != nullptr) { *y = pivot.y; }
    if (z != nullptr) { *z = pivot.z; }
}

bool _lua_mesh_get_or_create_field_pivot(lua_State *L, int idx, const Mesh* m) {
    if (LUA_GET_METAFIELD_AND_RETURN_TYPE(L, idx, LUA_MESH_FIELD_PIVOT) == LUA_TNIL) {
        if (lua_getmetatable(L, idx) == 0) {
            return false;
        }

        lua_pushstring(L, LUA_MESH_FIELD_PIVOT);
        const float3 pivot = mesh_get_pivot(m);
        lua_number3_pushNewObject(L, pivot.x, pivot.y, pivot.z);
        number3_C_handler_userdata userdata = number3_C_handler_userdata_zero;
        userdata.ptr = transform_get_and_retain_weakptr(mesh_get_transform(m));
        lua_number3_setHandlers(L, -1,
                               _lua_mesh_set_pivot_handler,
                               _lua_mesh_get_pivot_handler,
                               userdata, true);
        lua_rawset(L, -3);

        lua_pop(L, 1); // pop metatable

        if (LUA_GET_METAFIELD_AND_RETURN_TYPE(L, idx, LUA_MESH_FIELD_PIVOT) == LUA_TNIL) {
            return false;
        }
    }
    return true;
}

void _lua_mesh_set_pivot_handler(const float *x, const float *y, const float *z, lua_State * const L, const number3_C_handler_userdata *userdata) {
    Mesh *m = _lua_mesh_get_ptr_from_number3_handler(userdata);
    if (m == nullptr) {
        LUAUTILS_ERROR(L, "[Mesh.Pivot] original Mesh does not exist anymore");
    }
    
    _lua_mesh_set_pivot(m, x, y, z);
}

void _lua_mesh_get_pivot_handler(float *x, float *y, float *z, lua_State * const L, const number3_C_handler_userdata *userdata) {
    Mesh *m = _lua_mesh_get_ptr_from_number3_handler(userdata);
    if (m == nullptr) {
        LUAUTILS_ERROR(L, "[Mesh.Pivot] original Mesh does not exist anymore");
    }
    
    _lua_mesh_get_pivot(m, x, y, z);
}

bool _lua_mesh_get_or_create_field_bounding_box(lua_State *L, int idx, const Mesh* m) {
    if (LUA_GET_METAFIELD_AND_RETURN_TYPE(L, idx, LUA_MESH_FIELD_BOUNDINGBOX) == LUA_TNIL) {
        if (lua_getmetatable(L, idx) == 0) {
            return false;
        }

        lua_pushstring(L, LUA_MESH_FIELD_BOUNDINGBOX);
        lua_box_pushNewTable(L,
                            static_cast<void *>(transform_get_and_retain_weakptr(mesh_get_transform(m))),
                            _lua_mesh_set_bounding_box_handler,
                            _lua_mesh_get_bounding_box_handler,
                            _lua_mesh_free_bounding_box_handler,
                            _lua_mesh_set_bounding_box_min_n3_handler,
                            _lua_mesh_get_bounding_box_min_n3_handler,
                            _lua_mesh_set_bounding_box_max_n3_handler,
                            _lua_mesh_get_bounding_box_max_n3_handler);
        lua_rawset(L, -3);

        lua_pop(L, 1); // pop metatable

        if (LUA_GET_METAFIELD_AND_RETURN_TYPE(L, idx, LUA_MESH_FIELD_BOUNDINGBOX) == LUA_TNIL) {
            return false;
        }
    }
    return true;
}

void _lua_mesh_set_bounding_box_handler(const float3 *min, const float3 *max, lua_State * const L, void *userdata) {
    LUAUTILS_ERROR(L, "Mesh.BoundingBox cannot be set (field is read-only)");
}

void _lua_mesh_get_bounding_box_handler(float3 *min, float3 *max, lua_State * const L, const void *userdata) {
    Transform *t = static_cast<Transform *>(weakptr_get(static_cast<const Weakptr*>(userdata)));
    if (t == nullptr) {
        LUAUTILS_ERROR(L, "[Mesh.BoundingBox] original Mesh does not exist anymore");
    }
    Mesh *m = static_cast<Mesh*>(transform_get_ptr(t));

    const Box *aabb = mesh_get_model_aabb(m);
    if (min != nullptr) {
        *min = aabb->min;
    }
    if (max != nullptr) {
        *max = aabb->max;
    }
}

void _lua_mesh_free_bounding_box_handler(void *value) {
    weakptr_release(static_cast<Weakptr*>(value));
}

void _lua_mesh_set_bounding_box_min_n3_handler(const float *x, const float *y, const float *z, lua_State * const L, const number3_C_handler_userdata *userdata) {
    LUAUTILS_ERROR(L, "Mesh.BoundingBox.Min cannot be set (field is read-only)");
}

void _lua_mesh_get_bounding_box_min_n3_handler(float *x, float *y, float *z, lua_State * const L, const number3_C_handler_userdata *userdata) {
    Transform *t = static_cast<Transform*>(weakptr_get(static_cast<Weakptr*>(userdata->ptr)));
    if (t == nullptr) {
        LUAUTILS_ERROR(L, "[Mesh.BoundingBox.Min] original Mesh does not exist anymore");
    }
    Mesh *m = static_cast<Mesh*>(transform_get_ptr(t));

    const Box *aabb = mesh_get_model_aabb(m);
    if (x != nullptr) { *x = aabb->min.x; }
    if (y != nullptr) { *y = aabb->min.y; }
    if (z != nullptr) { *z = aabb->min.z; }
}

void _lua_mesh_set_bounding_box_max_n3_handler(const float *x, const float *y, const float *z, lua_State * const L, const number3_C_handler_userdata *userdata) {
    LUAUTILS_ERROR(L, "Mesh.BoundingBox.Max cannot be set (field is read-only)");
}

void _lua_mesh_get_bounding_box_max_n3_handler(float *x, float *y, float *z, lua_State * const L, const number3_C_handler_userdata *userdata) {
    Transform *t = static_cast<Transform*>(weakptr_get(static_cast<Weakptr*>(userdata->ptr)));
    if (t == nullptr) {
        LUAUTILS_ERROR(L, "[Mesh.BoundingBox.Max] original Mesh does not exist anymore");
    }
    Mesh *m = static_cast<Mesh*>(transform_get_ptr(t));

    const Box *aabb = mesh_get_model_aabb(m);
    if (x != nullptr) { *x = aabb->max.x; }
    if (y != nullptr) { *y = aabb->max.y; }
    if (z != nullptr) { *z = aabb->max.z; }
}

/// mesh:GetVertex(index)
/// @returns table containing vertex attributes (read-only)
static int _mesh_getVertex(lua_State * const L) {
    LUAUTILS_ASSERT_ARGS_COUNT(L, 2)
    LUAUTILS_STACK_SIZE(L)
    
    Mesh *m = lua_mesh_getMeshPtr(L, 1);
    if (m == nullptr) {
        LUAUTILS_ERROR(L, "Mesh:GetVertex - function should be called with `:`");
    }
    
    if (lua_utils_isIntegerStrict(L, 2) == false) {
        LUAUTILS_ERROR(L, "Mesh:GetVertex - argument must be an integer");
    }
    
    const int index = static_cast<int>(lua_tointeger(L, 2));
    if (index < 0 || index >= mesh_get_vertex_count(m)) {
        LUAUTILS_ERROR(L, "Mesh:GetVertex - index out of bounds");
    }
    
    const Vertex* vertices = mesh_get_vertex_buffer(m);
    if (vertices == nullptr) {
        lua_pushnil(L);
        return 1;
    }
    
    const Vertex* v = &vertices[index];
    
    lua_newtable(L);
    {
        lua_pushstring(L, "position");
        lua_number3_pushNewObject(L, v->x, v->y, v->z);
        lua_rawset(L, -3);
        
        lua_pushstring(L, "normal");
        lua_number3_pushNewObject(L, v->nx, v->ny, v->nz);
        lua_rawset(L, -3);
        
        lua_pushstring(L, "uv");
        lua_number2_pushNewObject(L, v->u, v->v);
        lua_rawset(L, -3);
        
        lua_pushstring(L, "color");
        const RGBAColor color = uint32_to_color(v->rgba);
        lua_color_create_and_push_table(L, color.r, color.g, color.b, color.a, false, true);
        lua_rawset(L, -3);
    }
    
    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
    return 1;
}

/// mesh:FlipTriangles()
static int _mesh_flipTriangles(lua_State * const L) {
    LUAUTILS_ASSERT_ARGS_COUNT(L, 1)
    LUAUTILS_STACK_SIZE(L)
    
    Mesh *m = lua_mesh_getMeshPtr(L, 1);
    if (m == nullptr) {
        LUAUTILS_ERROR(L, "Mesh:FlipTriangles - function should be called with `:`");
    }
    
    mesh_set_front_ccw(m, mesh_is_front_ccw(m) == false);
    
    LUAUTILS_STACK_SIZE_ASSERT(L, 0)
    return 0;
}

/// mesh:ComputeLocalBoundingBox()
static int _mesh_getLocalAABB(lua_State * const L) {
    LUAUTILS_ASSERT_ARGS_COUNT(L, 1)
    LUAUTILS_STACK_SIZE(L)
    
    Mesh *m = lua_mesh_getMeshPtr(L, 1);
    if (m == nullptr) {
        LUAUTILS_ERROR(L, "Mesh:ComputeLocalBoundingBox - function should be called with `:`");
    }
    
    Box box; mesh_get_local_aabb(m, &box);
    lua_box_standalone_pushNewTable(L, &box.min, &box.max, false);
    
    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
    return 1;
}

/// mesh:ComputeWorldBoundingBox()
static int _mesh_getWorldAABB(lua_State * const L) {
    LUAUTILS_ASSERT_ARGS_COUNT(L, 1)
    LUAUTILS_STACK_SIZE(L)
    
    Mesh *m = lua_mesh_getMeshPtr(L, 1);
    if (m == nullptr) {
        LUAUTILS_ERROR(L, "Mesh:ComputeWorldBoundingBox - function should be called with `:`");
    }
    
    Box box; mesh_get_world_aabb(m, &box, true);
    lua_box_standalone_pushNewTable(L, &box.min, &box.max, false);
    
    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
    return 1;
}

void lua_mesh_release_transform(void *_ud) {
    mesh_userdata *ud = static_cast<mesh_userdata*>(_ud);
    if (ud->mesh != nullptr) {
        mesh_release(ud->mesh);
        ud->mesh = nullptr;
    }
}