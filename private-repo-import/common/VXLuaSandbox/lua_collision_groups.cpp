//
//  lua_collision_groups.cpp
//  Cubzh
//
//  Created by Adrien Duermael on 19/09/2021.
//

#include "lua_collision_groups.hpp"

// Lua
#include "lua.hpp"
#include "lua_utils.hpp"
#include "lua_constants.h"
#include "sbs.hpp"

// engine
#include "rigidBody.h"
#include "transform.h"

#include <string>

static int _g_collision_groups_index(lua_State *L);
static int _g_collision_groups_newindex(lua_State *L);
static int _g_collision_groups_call(lua_State *L);
static int _collision_groups_newindex(lua_State *L);
static int _collision_groups_index(lua_State *L);
static int _collision_groups_len(lua_State *L);
static int _collision_groups_iter(lua_State *L);
static int _collision_groups_eq(lua_State *L);
static int _collision_groups_add(lua_State *L);
static int _collision_groups_sub(lua_State *L);

#define COLLISIONGROUPS_INSTANCE_METATABLE "__imt"

// Creates and pushes global CollisionGroups table
void lua_g_collision_groups_create_and_push(lua_State *L) {
    vx_assert(L != nullptr);
    LUAUTILS_STACK_SIZE(L)
    
    lua_newuserdata(L, 1); // global CollisionGroups
    lua_newtable(L); // metatable
    {
        lua_pushstring(L, "__index");
        lua_pushcfunction(L, _g_collision_groups_index, "");
        lua_rawset(L, -3);
        
        lua_pushstring(L, "__newindex");
        lua_pushcfunction(L, _g_collision_groups_newindex, "");
        lua_rawset(L, -3);
        
        lua_pushstring(L, "__call");
        lua_pushcfunction(L, _g_collision_groups_call, "");
        lua_rawset(L, -3);

        lua_pushstring(L, "__metatable");
        lua_pushboolean(L, false);
        lua_rawset(L, -3);

        lua_pushstring(L, "__type");
        lua_pushstring(L, "CollisionGroupsInterface");
        lua_rawset(L, -3);

        lua_pushstring(L, "__typen");
        lua_pushinteger(L, ITEM_TYPE_G_COLLISIONGROUPS);
        lua_rawset(L, -3);

        lua_pushstring(L, COLLISIONGROUPS_INSTANCE_METATABLE);
        lua_newtable(L);
        {
            lua_pushstring(L, "__index");
            lua_pushcfunction(L, _collision_groups_index, "");
            lua_rawset(L, -3);

            lua_pushstring(L, "__newindex");
            lua_pushcfunction(L, _collision_groups_newindex, "");
            lua_rawset(L, -3);

            lua_pushstring(L, "__len");
            lua_pushcfunction(L, _collision_groups_len, "");
            lua_rawset(L, -3);

            lua_pushstring(L, "__iter");
            lua_pushcfunction(L, _collision_groups_iter, "");
            lua_rawset(L, -3);

            lua_pushstring(L, "__eq");
            lua_pushcfunction(L, _collision_groups_eq, "");
            lua_rawset(L, -3);

            lua_pushstring(L, "__add");
            lua_pushcfunction(L, _collision_groups_add, "");
            lua_rawset(L, -3);

            lua_pushstring(L, "__sub");
            lua_pushcfunction(L, _collision_groups_sub, "");
            lua_rawset(L, -3);

            lua_pushstring(L, "__metatable");
            lua_pushboolean(L, false);
            lua_rawset(L, -3);

            lua_pushstring(L, "__type");
            lua_pushstring(L, "CollisionGroups");
            lua_rawset(L, -3);

            lua_pushstring(L, "__typen");
            lua_pushinteger(L, ITEM_TYPE_COLLISIONGROUPS);
            lua_rawset(L, -3);
        }
        lua_rawset(L, -3);

    }
    lua_setmetatable(L, -2);
    
    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
    return;
}

void lua_collision_groups_create_and_push(lua_State *L, uint16_t mask) {
    vx_assert(L != nullptr);
    LUAUTILS_STACK_SIZE(L)
    
    uint16_t *_mask = static_cast<uint16_t*>(lua_newuserdata(L, sizeof(uint16_t))); // CollisionGroups
    *_mask = mask;

    // set shared metatable
    lua_getglobal(L, P3S_LUA_G_COLLISIONGROUPS);
    LUA_GET_METAFIELD(L, -1, COLLISIONGROUPS_INSTANCE_METATABLE);
    lua_remove(L, -2);
    lua_setmetatable(L, -2);
    
    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
    return;
}

bool lua_collision_groups_create_and_push_with_group_number(lua_State *L, int groupNumber) {
    LUAUTILS_STACK_SIZE(L)
    if (groupNumber >= 1 && groupNumber <= PHYSICS_GROUP_MASK_API_BITS) {
        const uint16_t mask = 1 << (static_cast<uint16_t>(groupNumber) - 1);
        lua_collision_groups_create_and_push(L, mask);
        LUAUTILS_STACK_SIZE_ASSERT(L, 1)
        return true;
    }
    LUAUTILS_STACK_SIZE_ASSERT(L, 0)
    return false;
}

bool lua_collision_groups_group_number_is_valid(uint16_t groupNumber) {
    return groupNumber >= 1 && groupNumber <= PHYSICS_GROUP_MASK_API_BITS;
}

uint16_t lua_collision_groups_group_number_to_mask(uint16_t groupNumber) {
    return 1 << (groupNumber - 1);
}

bool lua_collision_groups_get_mask(lua_State *L, const int idx, uint16_t *mask) {
    vx_assert(L != nullptr);
    vx_assert(mask != nullptr);
    LUAUTILS_STACK_SIZE(L)
        
    *mask = PHYSICS_GROUP_NONE;
    
    if (lua_isnil(L, idx)) {
        LUAUTILS_STACK_SIZE_ASSERT(L, 0)
        return true; // mask remains 0
    } else if (lua_utils_getObjectType(L, idx) == ITEM_TYPE_COLLISIONGROUPS) {
        *mask = *static_cast<uint16_t*>(lua_touserdata(L, idx));
        return true;
    } else {
        return lua_utils_getMask(L, idx, mask, PHYSICS_GROUP_MASK_API_BITS);
    }
}

bool lua_collision_groups_push_from_table(lua_State *L, const int idx) {
    vx_assert(lua_istable(L, idx));
    LUAUTILS_STACK_SIZE(L)

    uint16_t mask = PHYSICS_GROUP_NONE;
    
    // iterate over table:
    lua_pushvalue(L, idx); // table at -1
    lua_pushnil(L); // table at -2, nil at -1
    long long v;
    while(lua_next(L, -2) != 0) {
        if(lua_isnumber(L, -1)) {
            v = lua_tonumber(L, -1);
            if (v >= 1 && v <= PHYSICS_GROUP_MASK_API_BITS) {
                mask |= 1 << (v - 1);
            } else {
                lua_pop(L, 3); // pop value, key, and table
                LUAUTILS_STACK_SIZE_ASSERT(L, 0)
                return false;
            }
        } else {
            lua_pop(L, 3); // pop value, key, and table
            LUAUTILS_STACK_SIZE_ASSERT(L, 0)
            return false;
        }
        lua_pop(L, 1);
    }
    lua_pop(L, 1); // pop table
    
    lua_collision_groups_create_and_push(L, mask);
    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
    return true;
}

int lua_g_collision_groups_snprintf(lua_State *L,
                                    char* result,
                                    size_t maxLen,
                                    bool spacePrefix) {
    vx_assert(L != nullptr);
    return snprintf(result, maxLen, "%s[CollisionGroups (class)]", spacePrefix ? " " : "");
}

int lua_collision_groups_snprintf(lua_State *L,
                                  char* result,
                                  size_t maxLen,
                                  bool spacePrefix) {
    vx_assert(L != nullptr);

    uint16_t mask = *static_cast<uint16_t*>(lua_touserdata(L, -1));

    std::string s = "";
    for (uint8_t i = 0; i < PHYSICS_GROUP_MASK_API_BITS; ++i) {
        if ((mask & (1 << i)) > 0) { if (s.empty() == false) { s += ", " + std::to_string(i + 1); } else { s += std::to_string(i + 1); }}
    }
    
    return snprintf(result,
                    maxLen,
                    "%s[CollisionGroups: {%s}]",
                    spacePrefix ? " " : "",
                    s.c_str());
}

static int _g_collision_groups_index(lua_State *L) {
    // global CollisionGroups has no fields
    lua_pushnil(L);
    return 1;
}

static int _g_collision_groups_newindex(lua_State *L) {
    LUAUTILS_ERROR(L, "CollisionGroups class is read-only");
    return 0;
}

// CollisionGroups()
// CollisionGroups(table)
// CollisionGroups(1, 3, 4)
static int _g_collision_groups_call(lua_State *L) {
    vx_assert(L != nullptr);
    LUAUTILS_STACK_SIZE(L)
    
    int argCount = lua_gettop(L); // first arg has to be global CollisionGroups
    
    if (argCount == 1) {
        lua_collision_groups_create_and_push(L, PHYSICS_GROUP_NONE);
    } else if (argCount == 2 && lua_istable(L, 2)) {
        if (lua_collision_groups_push_from_table(L, 2) == false) {
            LUAUTILS_ERROR_F(L, "CollisionGroups - groups can only be integers between 1 and %d", PHYSICS_GROUP_MASK_API_BITS); // returns
        }
    } else if (argCount >= 2) {
        uint16_t mask = PHYSICS_GROUP_NONE;
        
        // iterate over arguments to find valid integers
        for (int i = 2; i <= argCount; ++i) {
            
            if (lua_utils_isIntegerStrict(L, i)) {
                long long v = lua_tointeger(L, i);
                if (v >= 1 && v <= PHYSICS_GROUP_MASK_API_BITS) {
                    mask |= 1 << (v - 1);
                } else {
                    LUAUTILS_ERROR_F(L, "CollisionGroups - groups can only be integers between 1 and %d", PHYSICS_GROUP_MASK_API_BITS); // returns
                }
            } else {
                LUAUTILS_ERROR_F(L, "CollisionGroups - groups can only be integers between 1 and %d", PHYSICS_GROUP_MASK_API_BITS); // returns
            }
        }
        lua_collision_groups_create_and_push(L, mask);
    }
    
    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
    return 1;
}

static int _collision_groups_newindex(lua_State *L) {
    vx_assert(L != nullptr);
    LUAUTILS_STACK_SIZE(L)
    LUAUTILS_ERROR(L, "CollisionGroups is read only");
    LUAUTILS_STACK_SIZE_ASSERT(L, 0);
    return 0;
}

static int _collision_groups_index(lua_State *L) {
    vx_assert(L != nullptr);
    LUAUTILS_ASSERT_ARGS_COUNT(L, 2)
    LUAUTILS_ASSERT_ARG_TYPE(L, 1, ITEM_TYPE_COLLISIONGROUPS)
    LUAUTILS_STACK_SIZE(L)

    if (lua_isnumber(L, 2) == false) {
        lua_pushnil(L);
        LUAUTILS_STACK_SIZE_ASSERT(L, 1);
        return 1;
    }
    
    int i = static_cast<int>(lua_tointeger(L, 2));
    if (i < 1 || i > PHYSICS_GROUP_MASK_API_BITS) {
        lua_pushnil(L);
        LUAUTILS_STACK_SIZE_ASSERT(L, 1);
        return 1;
    }

    uint16_t mask = *static_cast<uint16_t*>(lua_touserdata(L, 1));

    int current = 1;
    int shift = 0;
    
    while (shift < PHYSICS_GROUP_MASK_API_BITS) {
        if ((mask & (1 << shift)) > 0) {
            if (i == current) {
                lua_pushinteger(L, shift + 1);
                LUAUTILS_STACK_SIZE_ASSERT(L, 1);
                return 1;
            }
            ++current;
        }
        ++shift;
    }

    // not found
    lua_pushnil(L);
    LUAUTILS_STACK_SIZE_ASSERT(L, 1);
    return 1;
}

static int _collision_groups_len(lua_State *L) {
    vx_assert(L != nullptr);
    // docs seems to say only one argument is expected, but I get 2...
    // https://www.lua.org/manual/5.3/manual.html (see __len)
    LUAUTILS_ASSERT_ARGS_COUNT(L, 2)
    LUAUTILS_ASSERT_ARG_TYPE(L, 1, ITEM_TYPE_COLLISIONGROUPS)
    LUAUTILS_STACK_SIZE(L)
    
    uint16_t mask = *static_cast<uint16_t*>(lua_touserdata(L, 1));

    int len = 0;
    int shift = 0;
    
    while (shift < PHYSICS_GROUP_MASK_API_BITS) {
        if ((mask & (1 << shift)) > 0) {
            ++len;
        }
        ++shift;
    }
    
    lua_pushinteger(L, len);
    
    LUAUTILS_STACK_SIZE_ASSERT(L, 1);
    return 1;
}

static int _collision_groups_next(lua_State * const L) {
    LUAUTILS_ASSERT_ARGS_COUNT(L, 2) // table, current index
    LUAUTILS_STACK_SIZE(L)

    int index = 1;

    if (lua_isnil(L, 2) == false) { // not first index
        if (lua_isnumber(L, 2) == false) {
            // index should a number
            LUAUTILS_INTERNAL_ERROR(L) // returns
        }

        index = static_cast<int>(lua_tointeger(L, 2));
        index = index + 1;
    }

    if (index < 1 || index > PHYSICS_GROUP_MASK_API_BITS) {
        // out of range, don't return anything
        LUAUTILS_STACK_SIZE_ASSERT(L, 0);
        return 0;
    }

    uint16_t mask = *static_cast<uint16_t*>(lua_touserdata(L, 1));

    int current = 1;
    int shift = 0;

    while (shift < PHYSICS_GROUP_MASK_API_BITS) {
        if ((mask & (1 << shift)) > 0) {
            if (index == current) {
                lua_pushinteger(L, index);
                lua_pushinteger(L, shift + 1);
                // current index & value on top of the stack
                LUAUTILS_STACK_SIZE_ASSERT(L, 2);
                return 2;
            }
            ++current;
        }
        ++shift;
    }

    // not found / reached the end
    LUAUTILS_STACK_SIZE_ASSERT(L, 0);
    return 0;
}

static int _collision_groups_iter(lua_State *L) {
    vx_assert(L != nullptr);
    LUAUTILS_ASSERT_ARGS_COUNT(L, 1) // ARG 1: CollisionGroups
    LUAUTILS_STACK_SIZE(L)

    lua_pushcfunction(L, _collision_groups_next, "_collision_groups_next");
    lua_pushvalue(L, 1); // CollisionGroups
    lua_pushnil(L); // starting point

    LUAUTILS_STACK_SIZE_ASSERT(L, 3)
    return 3;
}

static int _collision_groups_stateless_iter(lua_State *L) {
    vx_assert(L != nullptr);
    LUAUTILS_ASSERT_ARGS_COUNT(L, 2) // table, current index
    LUAUTILS_ASSERT_ARG_TYPE(L, 1, ITEM_TYPE_COLLISIONGROUPS)
    LUAUTILS_STACK_SIZE(L)
    
    // function receives previous index OR nil (first iteration)
    // we have to push the next index (+1), that's why it's a good thing to start at 0.
    int i = 0;
    
    if (lua_isnil(L, 2) == false) { // not first index
        
        if (lua_utils_isIntegerStrict(L, 2) == false) {
            // id should be an integer
            LUAUTILS_INTERNAL_ERROR(L) // returns
        }
        
        // index that's been processed
        i = static_cast<int>(lua_tointeger(L, 2));
    }
    
    uint16_t mask = *static_cast<uint16_t*>(lua_touserdata(L, 1));

    int current = 0;
    int shift = 0;
    
    while (shift < PHYSICS_GROUP_MASK_API_BITS) {
        if ((mask & (1 << current)) > 0) {
            if (i == current) {
                lua_pushinteger(L, i + 1); // +1 -> next index
                lua_pushinteger(L, shift + 1); // value
                LUAUTILS_STACK_SIZE_ASSERT(L, 2)
                return 2;
            }
            ++current;
        }
        ++shift;
    }
    
    // reached the end, don't return anything
    LUAUTILS_STACK_SIZE_ASSERT(L, 0)
    return 0;
}

static int _collision_groups_eq(lua_State *L) {
    vx_assert(L != nullptr);
    LUAUTILS_ASSERT_ARGS_COUNT(L, 2)
    LUAUTILS_STACK_SIZE(L)

    uint16_t mask1 = *static_cast<uint16_t*>(lua_touserdata(L, 1));
    uint16_t mask2 = *static_cast<uint16_t*>(lua_touserdata(L, 2));

    lua_pushboolean(L, mask1 == mask2);
    
    LUAUTILS_STACK_SIZE_ASSERT(L, 1);
    return 1;
}

static int _collision_groups_add(lua_State *L) {
    vx_assert(L != nullptr);
    LUAUTILS_ASSERT_ARGS_COUNT(L, 2)
    LUAUTILS_STACK_SIZE(L)

    uint16_t mask1 = 0;
    
    if (lua_utils_getObjectType(L, 1) == ITEM_TYPE_COLLISIONGROUPS) {
        mask1 = *static_cast<uint16_t*>(lua_touserdata(L, 1));
    } else {
        if (lua_istable(L, 1)) {
            if (lua_collision_groups_push_from_table(L, 1) == false) {
                LUAUTILS_ERROR(L, "CollisionGroups - impossible addition"); // returns
            }
            mask1 = *static_cast<uint16_t*>(lua_touserdata(L, -1));
            lua_pop(L, 1); // pop CollisionGroups
        } else {
            LUAUTILS_ERROR(L, "CollisionGroups - impossible addition"); // returns
        }
    }

    uint16_t mask2 = 0;
    
    if (lua_utils_getObjectType(L, 2) == ITEM_TYPE_COLLISIONGROUPS) {
        mask2 = *static_cast<uint16_t*>(lua_touserdata(L, 2));
    } else {
        if (lua_istable(L, 2)) {
            if (lua_collision_groups_push_from_table(L, 2) == false) {
                LUAUTILS_ERROR(L, "CollisionGroups - impossible addition"); // returns
            }
            mask2 = *static_cast<uint16_t*>(lua_touserdata(L, -1));
            lua_pop(L, 1); // pop CollisionGroups
        } else {
            LUAUTILS_ERROR(L, "CollisionGroups - impossible addition"); // returns
        }
    }
    
    uint16_t mask = mask1 | mask2;
    lua_collision_groups_create_and_push(L, mask);
    
    LUAUTILS_STACK_SIZE_ASSERT(L, 1);
    return 1;
}

///
static int _collision_groups_sub(lua_State *L) {
    vx_assert(L != nullptr);
    LUAUTILS_ASSERT_ARGS_COUNT(L, 2)
    LUAUTILS_STACK_SIZE(L)

    uint16_t mask1 = 0;
    
    if (lua_utils_getObjectType(L, 1) == ITEM_TYPE_COLLISIONGROUPS) {
        mask1 = *static_cast<uint16_t*>(lua_touserdata(L, 1));
    } else {
        if (lua_istable(L, 1)) {
            if (lua_collision_groups_push_from_table(L, 1) == false) {
                LUAUTILS_ERROR(L, "CollisionGroups - impossible substraction"); // returns
            }
            mask1 = *static_cast<uint16_t*>(lua_touserdata(L, -1));
            lua_pop(L, 1); // pop CollisionGroups
        } else {
            LUAUTILS_ERROR(L, "CollisionGroups - impossible substraction"); // returns
        }
    }

    uint16_t mask2 = 0;
    
    if (lua_utils_getObjectType(L, 2) == ITEM_TYPE_COLLISIONGROUPS) {
        mask2 = *static_cast<uint16_t*>(lua_touserdata(L, 2));
    } else {
        if (lua_istable(L, 2)) {
            if (lua_collision_groups_push_from_table(L, 2) == false) {
                LUAUTILS_ERROR(L, "CollisionGroups - impossible substraction"); // returns
            }
            mask2 = *static_cast<uint16_t*>(lua_touserdata(L, -1));
            lua_pop(L, 1); // pop CollisionGroups
        } else {
            LUAUTILS_ERROR(L, "CollisionGroups - impossible substraction"); // returns
        }
    }
    
    uint16_t mask = mask1 & (~mask2);
    lua_collision_groups_create_and_push(L, mask);
    
    LUAUTILS_STACK_SIZE_ASSERT(L, 1);
    return 1;
}
