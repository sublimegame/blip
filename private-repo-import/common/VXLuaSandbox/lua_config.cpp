//
//  lua_config.cpp
//  Cubzh
//
//  Created by Gaetan de Villele on 16/07/2019.
//  Copyright © 2019 voxowl. All rights reserved.
//

#include "lua_config.hpp"

// Lua
#include "lua.hpp"
#include "lua_utils.hpp"
#include "lua_items.hpp"
#include "lua_constants.h"
#include "lua_number3.hpp"
#include "lua_palette.hpp"
#include "lua_logs.hpp"

// Blip
#include "game.h"
#include "game_config.h"
#include "GameCoordinator.hpp"
#include "vxlog.h"

// Macros

#define LUA_CONFIG_FIELD_MAP "Map" // string
#define LUA_CONFIG_FIELD_MAP_PALETTE "MapPaletteOverrides" // DEPRECATED
#define LUA_CONFIG_FIELD_ITEMS "Items" // strings
#define LUA_CONFIG_FIELD_CONSTANT_ACCELERATION "ConstantAcceleration" // number
#define LUA_CONFIG_FIELD_USE_PBR "UsePBR" // boolean

///
static int config_metatable_index(lua_State *L);

///
static int config_metatable_newindex(lua_State *L);

///
static void _lua_config_set_constant_acceleration(const float *x,
                                                  const float *y,
                                                  const float *z,
                                                  lua_State *L,
                                                  const number3_C_handler_userdata *userdata);

///
static void _lua_config_get_constant_acceleration(float *x,
                                                  float *y,
                                                  float *z,
                                                  lua_State *L,
                                                  const number3_C_handler_userdata *userdata);

// Set Config object metatable
void _set_config_metatable(lua_State *L) {
    LUAUTILS_STACK_SIZE(L)
    lua_newtable(L); // metatable

    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
    lua_pushstring(L, "__index");
    lua_pushcfunction(L, config_metatable_index, "");
    LUAUTILS_STACK_SIZE_ASSERT(L, 3)
    lua_settable(L, -3);

    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
    lua_pushstring(L, "__newindex");
    lua_pushcfunction(L, config_metatable_newindex, "");
    LUAUTILS_STACK_SIZE_ASSERT(L, 3)
    lua_settable(L, -3);

    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
    lua_pushstring(L, "__metatable");
    lua_pushboolean(L, false);
    LUAUTILS_STACK_SIZE_ASSERT(L, 3)
    lua_settable(L, -3);

    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
    lua_pushstring(L, LUA_CONFIG_FIELD_ITEMS);
    if (lua_items_createAndPush(L) == false) {
        vxlog_error("failed to create item Lua table");
    }
    LUAUTILS_STACK_SIZE_ASSERT(L, 3)
    lua_settable(L, -3);
    
    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
    lua_pushstring(L, LUA_CONFIG_FIELD_CONSTANT_ACCELERATION); // used to log tables
    lua_number3_pushNewObject(L, 0.0f, 0.0f, 0.0f);
    number3_C_handler_userdata userdata = {}; // not useful, but lua_number3_setHandlers needs one
    lua_number3_setHandlers(L, -1,
                            _lua_config_set_constant_acceleration,
                            _lua_config_get_constant_acceleration,
                            userdata, false);
    LUAUTILS_STACK_SIZE_ASSERT(L, 3)
    lua_rawset(L, -3);

    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
    lua_pushstring(L, P3S_LUA_OBJ_METAFIELD_TYPE); // used to log tables
    lua_pushinteger(L, ITEM_TYPE_CONFIG);
    LUAUTILS_STACK_SIZE_ASSERT(L, 3)
    lua_rawset(L, -3);

    lua_setmetatable(L, -2);
}

bool lua_config_push_items(lua_State *L) {
    vx_assert(L != nullptr);
    LUAUTILS_ASSERT_ARG_TYPE(L, -1, ITEM_TYPE_CONFIG)
    LUAUTILS_STACK_SIZE(L)
    
    if (LUA_GET_METAFIELD_AND_RETURN_TYPE(L, -1, LUA_CONFIG_FIELD_ITEMS) == LUA_TNIL) {
        return false;
    }
    
    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
    return true;
}

void lua_config_create_default_and_push(lua_State * const L) {
    LUAUTILS_STACK_SIZE(L)
    lua_newtable(L);
    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
    _set_config_metatable(L);
    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
}

void lua_config_set_from_table(lua_State *L, const int tableIdx) {
    vx_assert(L != nullptr);
    LUAUTILS_ASSERT_ARG_TYPE(L, -1, ITEM_TYPE_CONFIG)
    LUAUTILS_STACK_SIZE(L)
    
    // check value is a table
    if (lua_istable(L, tableIdx) == false) {
        LUAUTILS_STACK_SIZE_ASSERT(L, 0)
        return;
    }
    
    vx::Game *cppGame = nullptr;
    if (lua_utils_getGameCppWrapperPtr(L, &cppGame) == false) {
        LUAUTILS_INTERNAL_ERROR(L) // returns
    }
    
    GameConfig *config = game_getConfig(cppGame->getCGame());
    
    game_config_reset(config);
    
    lua_pushnil(L);  /* first key, now at -1 */
    while (lua_next(L, tableIdx) != 0) { // lua_next pops key at -1 (nil at first iteration)
        // key now at -2, value at -1
        // Config now at -3
        // printf("%s - %s\n", lua_typename(L, lua_type(L, -2)), lua_typename(L, lua_type(L, -1)));
        
        // trying to set built-in key (uppercase first letter variable)
        if (lua_utils_isStringStrictStartingWithUppercase(L, -2)) {
            
            const char *key = lua_tostring(L, -2);
            
            if (strcmp(key, LUA_CONFIG_FIELD_MAP) == 0) {
                
                if (lua_utils_isStringStrict(L, -1) == false) {
                    // ignore the key
                    // TODO: warn the user
                    lua_pop(L, 1);
                    continue;
                }
                
                if (lua_getmetatable(L, -3) == LUA_TNIL) {
                    LUAUTILS_INTERNAL_ERROR(L)
                }
                
                lua_pushstring(L, LUA_CONFIG_FIELD_MAP);
                lua_item_createAndPushMap(L, lua_tostring(L, -3));
                lua_rawset(L, -3);
                
                lua_pop(L, 1); // pop metatable
                
            } else if (strcmp(key, LUA_CONFIG_FIELD_MAP_PALETTE) == 0) {

                lua_log_warning(L, "MapPaletteOverrides is deprecated, please use Map.Palette instead");
                lua_pushnil(L);
                
            } else if (strcmp(key, LUA_CONFIG_FIELD_ITEMS) == 0) {
                
                if (LUA_GET_METAFIELD_AND_RETURN_TYPE(L, -3, LUA_CONFIG_FIELD_ITEMS) == LUA_TNIL) {
                    LUAUTILS_INTERNAL_ERROR(L) // returns
                }
                // value now at -2 (expecting a table)
                lua_items_set_all(L, -2);
                lua_pop(L, 1); // pop Items

            } else if (strcmp(key, LUA_CONFIG_FIELD_USE_PBR) == 0) {
                if (lua_isboolean(L, -1)) {
                    vx::GameRenderer * const renderer = vx::GameRenderer::getGameRenderer();
                    if (renderer != nullptr) {
                        renderer->togglePBR(lua_toboolean(L, -1));
                    }
                } else {
                    lua_log_warning(L, "Config.UsePBR must be a boolean");
                }                
            } else {
                vxlog_warning("Config key not handled: %s", key);
            }
            
        } else {
            // accept other variables without checking content
            // user defined Config variable
            // Config: -3, key: -2, value: -1
            // just need to call lua_rawset
            // it's necessary though to push both key and value to the very top of the stack
            // because the value is automatically popped at the end of the loop and because
            // we need to keep the key at the top not to break iteration.
            lua_pushvalue(L, -2); // key
            lua_pushvalue(L, -2); // value
            // Config now at -5
            lua_rawset(L, -5);
        }
        
        // pop value, keep key for next iteration
        lua_pop(L, 1);
    }
    
    LUAUTILS_STACK_SIZE_ASSERT(L, 0)
}

static int config_metatable_index(lua_State *L) {
    vx_assert(L != nullptr);
    LUAUTILS_ASSERT_ARGS_COUNT(L, 2)
    LUAUTILS_ASSERT_ARG_TYPE(L, 1, ITEM_TYPE_CONFIG)
    LUAUTILS_STACK_SIZE(L)
    
    // get 2nd argument: key string
    if (lua_utils_isStringStrict(L, 2) == false) {
        // Config does not support non-string keys
        // just return nil
        lua_pushnil(L);
        LUAUTILS_STACK_SIZE_ASSERT(L, 1)
        return 1;
    }
    
    const char *key = lua_tostring(L, 2);
    LUAUTILS_STACK_SIZE_ASSERT(L, 0)

    vx::Game *g = nullptr;
    if (lua_utils_getGameCppWrapperPtr(L, &g) == false) {
        LUAUTILS_INTERNAL_ERROR(L) // returns
    }

    if (lua_utils_isStringStrictStartingWithUppercase(L, 2)) {
        
        if (strcmp(key, LUA_CONFIG_FIELD_ITEMS) == 0) {
            LUA_GET_METAFIELD_AND_RETURN_TYPE_PUSHING_NIL_IF_NOT_FOUND(L, 1, LUA_CONFIG_FIELD_ITEMS);

        } else if (strcmp(key, LUA_CONFIG_FIELD_CONSTANT_ACCELERATION) == 0) {
            LUA_GET_METAFIELD_AND_RETURN_TYPE_PUSHING_NIL_IF_NOT_FOUND(L, 1, LUA_CONFIG_FIELD_CONSTANT_ACCELERATION);

        } else if (strcmp(key, LUA_CONFIG_FIELD_MAP) == 0) {
            LUA_GET_METAFIELD_AND_RETURN_TYPE_PUSHING_NIL_IF_NOT_FOUND(L, 1, LUA_CONFIG_FIELD_MAP);

        } else if (strcmp(key, LUA_CONFIG_FIELD_MAP_PALETTE) == 0) {
        
            lua_log_warning(L, "MapPaletteOverrides is deprecated, please use Map.Palette instead");
            lua_pushnil(L);
            
        } else if (strcmp(key, LUA_CONFIG_FIELD_USE_PBR) == 0) {
            if (vx::GameRenderer::getGameRenderer() == nullptr) {
                LUAUTILS_STACK_SIZE_ASSERT(L, 0)
                return 0;
            }
            lua_pushboolean(L, vx::GameRenderer::getGameRenderer()->isPBR());
        } else {
            lua_pushnil(L); // if key is unknown, return nil
        }
        
    } else { // user defined key
        vxlog_warning("🔥 accessing Config user defined key: %s, not implemented", key);
        lua_pushnil(L);
    }
    
    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
    return 1;
}

///
static int config_metatable_newindex(lua_State *L) {
    vx_assert(L != nullptr);
    LUAUTILS_ASSERT_ARGS_COUNT(L, 3)
    LUAUTILS_ASSERT_ARG_TYPE(L, 1, ITEM_TYPE_CONFIG)
    LUAUTILS_STACK_SIZE(L)
    
    // setting non-string or non-capitalized string keys is allowed
    if (lua_utils_isStringStrictStartingWithUppercase(L, 2) == false) {
        // store the key/value pair
        lua_pushvalue(L, 2); // key
        lua_pushvalue(L, 3); // value
        lua_rawset(L, 1);
        LUAUTILS_STACK_SIZE_ASSERT(L, 0)
        return 0;
    }
    
    // key is a capitalized string
    const char *key = lua_tostring(L, 2);
    
    vx::Game *cppGame = nullptr;
    if (lua_utils_getGameCppWrapperPtr(L, &cppGame) == false) {
        LUAUTILS_INTERNAL_ERROR(L) // returns
    }
    
    if (strcmp(key, LUA_CONFIG_FIELD_MAP) == 0) {
        
        if (lua_utils_isStringStrict(L, 3) == false) {
            vxlog_error("⚠️ Config.%s cannot be set to a non-string value", key);
            LUAUTILS_STACK_SIZE_ASSERT(L, 0)
            return 0;
        }
        
        lua_getmetatable(L, 1);
        lua_pushstring(L, LUA_CONFIG_FIELD_MAP);
        lua_item_createAndPushMap(L, lua_tostring(L, 3));
        lua_rawset(L, -3);
        lua_pop(L, 1); // pop metatable
        
        LUAUTILS_STACK_SIZE_ASSERT(L, 0)
        
    } else if (strcmp(key, LUA_CONFIG_FIELD_ITEMS) == 0) {
        
        if (LUA_GET_METAFIELD_AND_RETURN_TYPE(L, 1, LUA_CONFIG_FIELD_ITEMS) == LUA_TNIL) {
            LUAUTILS_INTERNAL_ERROR(L) // returns
        }
        lua_items_set_all(L, 3);
        lua_pop(L, 1); // pop Items
        
        LUAUTILS_STACK_SIZE_ASSERT(L, 0)
        
    } else if (strcmp(key, LUA_CONFIG_FIELD_CONSTANT_ACCELERATION) == 0) {
        float3 f3;
        if (lua_number3_or_table_getXYZ(L, 3, &f3.x, &f3.y, &f3.z) == false) {
            lua_pushfstring(L, "Config.%s cannot be set (argument is not a Number3)", key);
            return -1; // error
        }
        game_set_constant_acceleration(cppGame->getCGame(), &f3.x, &f3.y, &f3.z);
        LUAUTILS_STACK_SIZE_ASSERT(L, 0)
        
    } else if (strcmp(key, LUA_CONFIG_FIELD_USE_PBR) == 0) {
        if (vx::GameRenderer::getGameRenderer() == nullptr) {
            LUAUTILS_STACK_SIZE_ASSERT(L, 0)
            return 0;
        }
        if (lua_isboolean(L, 3) == false) {
            lua_log_warning(L, "Config.UsePBR must be a boolean");
            LUAUTILS_STACK_SIZE_ASSERT(L, 0)
            return 0;
        }
        vx::GameRenderer::getGameRenderer()->togglePBR(lua_toboolean(L, 3));
    } else {
        // field is not settable
        vxlog_error("🌒⚠️ global Config table \"%s\" field is not settable.", key);
    }
    
    LUAUTILS_STACK_SIZE_ASSERT(L, 0)
    return 0;
}

static void _lua_config_set_constant_acceleration(const float *x,
                                                  const float *y,
                                                  const float *z,
                                                  lua_State *L,
                                                  const number3_C_handler_userdata *userdata) {
    CGame *g = nullptr;
    if (lua_utils_getGamePtr(L, &g) == false) return;
    
    game_set_constant_acceleration(g, x, y, z);
}

static void _lua_config_get_constant_acceleration(float *x,
                                                  float *y,
                                                  float *z,
                                                  lua_State *L,
                                                  const number3_C_handler_userdata *userdata) {
    CGame *g = nullptr;
    if (lua_utils_getGamePtr(L, &g) == false) return;
    
    const float3 *f3 = game_get_constant_acceleration(g);

    if (x != nullptr) *x = f3->x;
    if (y != nullptr) *y = f3->y;
    if (z != nullptr) *z = f3->z;
}
