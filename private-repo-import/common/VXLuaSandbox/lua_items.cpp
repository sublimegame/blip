//
//  lua_items.cpp
//  Cubzh
//
//  Created by Gaetan de Villele on 21/11/2019.
//  Copyright © 2019 Voxowl Inc. All rights reserved.
//

#include "lua_items.hpp"

// C++
#include <cstring>

// Lua
#include "lua.hpp"
#include "lua_utils.hpp"

#include "VXResource.hpp"

// engine
#include "game.h"
#include "game_config.h"

#include "VXGame.hpp"

// sandbox
#include "lua_constants.h"

#define LUA_ITEM_FIELD_FULLNAME "Fullname"
#define LUA_ITEM_FIELD_REPO "Repo"
#define LUA_ITEM_FIELD_NAME "Name"
#define LUA_ITEM_FIELD_TAG "Tag"

// Metatable keys
#define LUA_ITEMS_METAFIELD_REPOS "__repos"
#define LUA_ITEMS_METAFIELD_ITEMS "__items"

typedef struct item_userdata {
    Item *item;
} item_userdata;

/// --------------------------------------------------
///
/// MARK: - static functions prototypes -
///
/// --------------------------------------------------

static int _items_metatable_index(lua_State *L);
static int _items_metatable_newindex(lua_State *L);
static int _items_metatable_len(lua_State *L);
static int _item_metatable_index(lua_State *L);
static int _item_metatable_newindex(lua_State *L);
static void _item_gc(void *_ud);

// --------------------------------------------------
//
// MARK: - Exposed functions -
//
// --------------------------------------------------

bool lua_items_createAndPush(lua_State *L) {
    
    RETURN_VALUE_IF_NULL(L, false)
        
    LUAUTILS_STACK_SIZE(L)
    
    lua_newuserdata(L, 1); // Items table
    lua_newtable(L); // metatable
    {
        lua_pushstring(L, "__index");
        lua_pushcfunction(L, _items_metatable_index, "");
        lua_rawset(L, -3);
        
        lua_pushstring(L, "__newindex");
        lua_pushcfunction(L, _items_metatable_newindex, "");
        lua_rawset(L, -3);
        
        lua_pushstring(L, "__len");
        lua_pushcfunction(L, _items_metatable_len, "");
        lua_rawset(L, -3);

        // hide the metatable from the Lua sandbox user
        lua_pushstring(L, "__metatable");
        lua_pushboolean(L, false);
        lua_rawset(L, -3);

        lua_pushstring(L, "__type"); // used to log tables
        lua_pushstring(L, "Items");
        lua_rawset(L, -3);

        lua_pushstring(L, "__typen"); // used to log tables
        lua_pushinteger(L, ITEM_TYPE_ITEMS);
        lua_rawset(L, -3);
        
        // table to index Items by repo
        // each repo is a table with Items indexed by name
        lua_pushstring(L, LUA_ITEMS_METAFIELD_REPOS);
        lua_newtable(L);
        lua_rawset(L, -3);
        
        // table to index Items with integers (array)
        lua_pushstring(L, LUA_ITEMS_METAFIELD_ITEMS);
        lua_newtable(L);
        lua_rawset(L, -3);
    }
    lua_setmetatable(L, -2); // sets and pops metatable
    
    // Items -1
    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
    
    // Add default body parts
    LUA_GET_METAFIELD(L, -1, LUA_ITEMS_METAFIELD_REPOS);
    // Items.metatable.repos -1
    // Items -2

    LUA_GET_METAFIELD(L, -2, LUA_ITEMS_METAFIELD_ITEMS);
    // Items.metatable.items -1
    // Items.metatable.repos -2
    // Items -3
    
    lua_pop(L, 2);
    
    LUAUTILS_STACK_SIZE_ASSERT(L, 1)

    return true;
}

bool lua_items_set_all(lua_State *L, const int idx) {
    
    // Items -1
    
    LUAUTILS_ASSERT_ARG_TYPE(L, -1, ITEM_TYPE_ITEMS)
    LUAUTILS_STACK_SIZE(L)
    
    bool idxBelowItems = idx < -1;
    
    // check value is a idx
    // (should be an array of strings)
    if (lua_istable(L, idx) == false) {
        return false;
    }
    
    if (lua_getmetatable(L, -1) == 0) {
        return false;
    }
    
    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
    
    // Items metatable -1
    // Items -2
    
    // reset repos index
    lua_pushstring(L, LUA_ITEMS_METAFIELD_REPOS);
    lua_newtable(L);
    lua_rawset(L, -3);
    
    // reset items index
    lua_pushstring(L, LUA_ITEMS_METAFIELD_ITEMS);
    lua_newtable(L);
    lua_rawset(L, -3);
    
    if (lua_getfield(L, -1, LUA_ITEMS_METAFIELD_REPOS) != LUA_TTABLE) {
        return false;
    }
    
    // Repos index -1
    // Items metatable -2
    // Items -3
    
    if (lua_getfield(L, -2, LUA_ITEMS_METAFIELD_ITEMS) != LUA_TTABLE) {
        return false;
    }
    
    // Items array index -1
    // Repos index -2
    // Items metatable -3
    // Items -4
    
    lua_remove(L, -3); // remove metatable
    
    // Items array index -1
    // Repos index -2
    // Items -3
    
    lua_pushvalue(L, -3); // Items at -1 & -4
    
    // Items -1
    // Items array index -2
    // Repos index -3
    // Items -4
    
    LUAUTILS_STACK_SIZE_ASSERT(L, 3)
    
    lua_pushvalue(L, idxBelowItems ? idx - 3 : idx); // table arg now at -1, Items at -2
    
    lua_pushnil(L);  // first key, now at -1, items table now at -2
    
    // nil -1
    // table of fullname strings -2
    // Items -2
    // Items array index -3
    // Repos index -4
    // Items -5
    
    LUAUTILS_STACK_SIZE_ASSERT(L, 5)
    
    while (lua_next(L, -2) != 0) { // lua_next pops key at -1 (nil at first iteration)
        // Item key now at -2, value at -1 (key is supposed to be a number)
        
        // value -1
        // key -2
        // table of fullname strings -3
        // Items -4
        // Items array index -5
        // Repos index -6
        // Items -7
        
        //printf("====\n%s\n%s\n%s\n%s\n%s\n%s\n%s\n",
        //       lua_typename(L, lua_type(L, -1)),
        //       lua_typename(L, lua_type(L, -2)),
        //       lua_typename(L, lua_type(L, -3)),
        //       lua_typename(L, lua_type(L, -4)),
        //       lua_typename(L, lua_type(L, -5)),
        //       lua_typename(L, lua_type(L, -6)),
        //       lua_typename(L, lua_type(L, -7)));
        
        if (lua_utils_isStringStrict(L, -1)) {
            // item inserted within indexes within lua_item_createAndPush
            // unless items idx == 0 and/or repos idx == 0
            lua_item_createAndPush(L,
                                   lua_tostring(L, -1),
                                   -5 /* items idx*/,
                                   -6 /* repos idx*/);
            LUAUTILS_ASSERT_ARG_TYPE(L, -8, ITEM_TYPE_ITEMS)
            lua_pop(L, 1);
        }
        
        // pop value, keep key for next iteration
        lua_pop(L, 1);
    }

    lua_pop(L, 4); // pop table, Items & array index, repos index
    
    LUAUTILS_STACK_SIZE_ASSERT(L, 0)

    return true;
}

///
bool lua_item_createAndPush(lua_State *L, const char* fullname, const int itemsIdx, const int reposIdx) {
    
    RETURN_VALUE_IF_NULL(L, false)
    if (fullname == nullptr) { return false; }
    
    LUAUTILS_STACK_SIZE(L)

    vx::Game *g = nullptr;
    if (lua_utils_getGameCppWrapperPtr(L, &g) == false) {
        LUAUTILS_INTERNAL_ERROR(L);
    }

    bool itemsIdxBelowZero = itemsIdx < 0;
    bool reposIdxBelowZero = reposIdx < 0;

    GameConfig *config = game_getConfig(g->getCGame());
    Item *item = game_config_items_append(config, fullname);
    item_retain(item);

    item_userdata *ud = static_cast<item_userdata *>(lua_newuserdatadtor(L, sizeof(item_userdata), _item_gc));
    ud->item = item;

    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
    
    lua_newtable(L); // metatable
    {
        lua_pushstring(L, "__index");
        lua_pushcfunction(L, _item_metatable_index, "");
        lua_rawset(L, -3);

        lua_pushstring(L, "__newindex");
        lua_pushcfunction(L, _item_metatable_newindex, "");
        lua_rawset(L, -3);

        // hide the metatable from the Lua sandbox user
        lua_pushstring(L, "__metatable");
        lua_pushboolean(L, false);
        lua_rawset(L, -3);

        lua_pushstring(L, "__type");
        lua_pushstring(L, "Item");
        lua_rawset(L, -3);

        lua_pushstring(L, "__typen");
        lua_pushinteger(L, ITEM_TYPE_ITEM);
        lua_rawset(L, -3);
    }
    lua_setmetatable(L, -2); // sets and pops metatable
    
    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
    
    if (item != nullptr) {

        lua_pushvalue(L, itemsIdxBelowZero ? itemsIdx - 1 : itemsIdx); // items array index
        lua_Integer len = static_cast<lua_Integer>(lua_objlen(L, -1));
        lua_pushvalue(L, -2); // Item
        lua_rawseti(L, -2, len + 1);
        lua_pop(L, 1); // pop array index
        
        LUAUTILS_STACK_SIZE_ASSERT(L, 1)
        
        // Item at -1
        
        if (item->repo != nullptr) {
            lua_pushvalue(L, reposIdxBelowZero ? reposIdx - 1 : reposIdx); // repos index
            
            if (lua_getfield(L, -1, item->repo) == LUA_TNIL) {
                lua_pop(L, 1); // pop NIL
                lua_pushstring(L, item->repo);
                lua_newtable(L);
                lua_rawset(L, -3);
                if (lua_getfield(L, -1, item->repo) != LUA_TTABLE) {
                    LUAUTILS_INTERNAL_ERROR(L)
                }
            }
            
            lua_remove(L, -2); // remove repos index
            
            // repo table at the to of the stack
            
            LUAUTILS_STACK_SIZE_ASSERT(L, 2)
            // Item at -2
            
            lua_pushstring(L, item->name);
            LUAUTILS_ASSERT_ARG_TYPE(L, -3, ITEM_TYPE_ITEM)
            lua_pushvalue(L, -3); // Item
            lua_rawset(L, -3);
            lua_pop(L, 1); // pop repos table
        }
    }
    
    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
    return true;
}

bool lua_item_createAndPushMap(lua_State *L, const char* fullname) {
    
    RETURN_VALUE_IF_NULL(L, false)
    if (fullname == nullptr) { return false; }
    
    LUAUTILS_STACK_SIZE(L)
    
    vx::Game *g = nullptr;
    if (lua_utils_getGameCppWrapperPtr(L, &g) == false) {
        LUAUTILS_INTERNAL_ERROR(L);
    }

    GameConfig *config = game_getConfig(g->getCGame());
    Item *item = game_config_set_map(config, fullname);
    item_retain(item);

    item_userdata *ud = static_cast<item_userdata *>(lua_newuserdatadtor(L, sizeof(item_userdata), _item_gc));
    ud->item = item;

    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
    
    lua_newtable(L); // metatable
    {
        lua_pushstring(L, "__index");
        lua_pushcfunction(L, _item_metatable_index, "");
        lua_rawset(L, -3);

        lua_pushstring(L, "__newindex");
        lua_pushcfunction(L, _item_metatable_newindex, "");
        lua_rawset(L, -3);

        // hide the metatable from the Lua sandbox user
        lua_pushstring(L, "__metatable");
        lua_pushboolean(L, false);
        lua_rawset(L, -3);

        lua_pushstring(L, "__type");
        lua_pushstring(L, "Item");
        lua_rawset(L, -3);

        lua_pushstring(L, "__typen");
        lua_pushinteger(L, ITEM_TYPE_ITEM);
        lua_rawset(L, -3);
    }
    lua_setmetatable(L, -2); // sets and pops metatable
    
    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
    return true;
}

const char* lua_item_getName(lua_State *L, const int idx) {
    RETURN_VALUE_IF_NULL(L, nullptr)
    LUAUTILS_STACK_SIZE(L)
    
    if (lua_utils_getObjectType(L, idx) != ITEM_TYPE_ITEM) {
        vxlog_error("[%s:%d] Error: lua value at index %d is not an Item.", __FILE__, __LINE__, idx);
        LUAUTILS_STACK_SIZE_ASSERT(L, 0)
        return nullptr;
    }

    item_userdata *ud = static_cast<item_userdata*>(lua_touserdata(L, idx));
    vx_assert(ud->item != nullptr);

    LUAUTILS_STACK_SIZE_ASSERT(L, 0)
    return ud->item->fullname;
}

/// __index
static int _items_metatable_index(lua_State *L) {
    vx_assert(L != nullptr);
    LUAUTILS_ASSERT_ARGS_COUNT(L, 2)
    LUAUTILS_ASSERT_ARG_TYPE(L, 1, ITEM_TYPE_ITEMS)
    LUAUTILS_STACK_SIZE(L)
    
    // LOOK BY INTEGER INDEX (ARRAY)
    if (lua_isnumber(L, 2)) {
        if (LUA_GET_METAFIELD_AND_RETURN_TYPE(L, 1, LUA_ITEMS_METAFIELD_ITEMS) == LUA_TNIL) {
            LUAUTILS_INTERNAL_ERROR(L) // returns
        }
        lua_rawgeti(L, -1, lua_tonumber(L, 2));
        lua_remove(L, -2); // remove __items
        
        LUAUTILS_STACK_SIZE_ASSERT(L, 1)
        return 1;
    }
    
    // validate type of 2nd argument (key must be a string)
    if (lua_utils_isStringStrict(L, 2) == false) {
        vxlog_error("Wrong 2nd argument type. Returning nil.");
        lua_pushnil(L);
        LUAUTILS_STACK_SIZE_ASSERT(L, 1)
        return 1;
    }
    
    // LOOK BY repo.name
    
    if (LUA_GET_METAFIELD_AND_RETURN_TYPE(L, 1, LUA_ITEMS_METAFIELD_REPOS) == LUA_TNIL) {
        LUAUTILS_INTERNAL_ERROR(L) // returns
    }
    
    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
    
    lua_pushvalue(L, 2); // push key
    lua_rawget(L, -2);
    
    if (lua_isnil(L, -1)) { // not found, try to get resource directly from official repo
        
        lua_pop(L, 1); // pop nil
        lua_pushstring(L, "official"); // key
        lua_rawget(L, -2);
        
        if (lua_isnil(L, -1)) {
            vxlog_error("can't get official repo");
            // keep nil on top of the stack, means no official items have been imported
        } else {
            lua_pushvalue(L, 2); // push key
            lua_rawget(L, -2);
            // now resource is supposed to be on top of the stack, or nil if not found
            lua_remove(L, -2); // remove official repo
        }
    }
    
    lua_remove(L, -2); // remove __repos
    
    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
    return 1;
}

static int _items_metatable_newindex(lua_State *L) {
    vx_assert(L != nullptr);
    LUAUTILS_ASSERT_ARGS_COUNT(L, 3)
    LUAUTILS_ASSERT_ARG_TYPE(L, 1, ITEM_TYPE_ITEMS)
    LUAUTILS_STACK_SIZE(L)
    
    // only accept integer indexes
    if (lua_utils_isIntegerStrict(L, 2) == false) {
        LUAUTILS_STACK_SIZE_ASSERT(L, 0)
        LUAUTILS_ERROR_F(L, "Items.%s cannot be set", lua_tostring(L, 2)); // returns
    }
    
    int i = static_cast<int>(lua_tointeger(L, 2));
    i = i - 1; // Lua arrays start at 1
    
    if (lua_utils_isStringStrict(L, 3) == false) {
        LUAUTILS_STACK_SIZE_ASSERT(L, 0)
        LUAUTILS_ERROR(L, "Items does not accept non-string values"); // returns
        return 0;
    }
    
    const char *itemFullname = lua_tostring(L, 3);
    
    CGame *g = nullptr;
    if (lua_utils_getGamePtr(L, &g) == false) {
        LUAUTILS_INTERNAL_ERROR(L) // returns
    }
    
    GameConfig *config = game_getConfig(g);
    
    // TODO: make sure item format is valid
    // TODO: support insert
    
    int len = game_config_items_len(config);
    
    if (i <= len) {
        game_config_items_set_at(config, itemFullname, i);
    } else {
        LUAUTILS_ERROR_F(L, "Items[%d] cannot be set, Items length: %d", i + 1, len); // returns
    }
    
    LUAUTILS_STACK_SIZE_ASSERT(L, 0)
    return 0;
}

static int _item_metatable_index(lua_State *L) {
    vx_assert(L != nullptr);
    LUAUTILS_ASSERT_ARGS_COUNT(L, 2)
    LUAUTILS_ASSERT_ARG_TYPE(L, 1, ITEM_TYPE_ITEM)
    LUAUTILS_STACK_SIZE(L)
    
    // get 2nd argument: key string
    if (lua_utils_isStringStrict(L, 2) == false) {
        // Item does not support non-string keys
        // just return nil
        lua_pushnil(L);
        LUAUTILS_STACK_SIZE_ASSERT(L, 1)
        return 1;
    }
    
    const char *key = lua_tostring(L, 2);
    
    if (strcmp(key, LUA_ITEM_FIELD_FULLNAME) == 0) {
        item_userdata *ud = static_cast<item_userdata*>(lua_touserdata(L, 1));
        Item *item = ud->item;
        
        if (item != nullptr) {
            lua_pushstring(L, item->fullname);
        } else {
            lua_pushnil(L);
        }
        
    } else if (strcmp(key, LUA_ITEM_FIELD_REPO) == 0) {
        item_userdata *ud = static_cast<item_userdata*>(lua_touserdata(L, 1));
        Item *item = ud->item;

        if (item != nullptr) {
            lua_pushstring(L, item->repo);
        } else {
            lua_pushnil(L);
        }
        
    } else if (strcmp(key, LUA_ITEM_FIELD_NAME) == 0) {
        item_userdata *ud = static_cast<item_userdata*>(lua_touserdata(L, 1));
        Item *item = ud->item;

        if (item != nullptr) {
            lua_pushstring(L, item->name);
        } else {
            lua_pushnil(L);
        }
        
    } else if (strcmp(key, LUA_ITEM_FIELD_TAG) == 0) {
        item_userdata *ud = static_cast<item_userdata*>(lua_touserdata(L, 1));
        Item *item = ud->item;

        if (item != nullptr) {
            lua_pushstring(L, item->tag);
        } else {
            lua_pushnil(L);
        }
        
    } else {
        // if key is unknown, return nil
        lua_pushnil(L);
    }
    
    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
    return 1;
}

static int _item_metatable_newindex(lua_State *L) {
    vx_assert(L != nullptr);
    // Item objects are read-only, therefore this does nothing.
    return 0;
}

static void _item_gc(void *_ud) {
    item_userdata *ud = static_cast<item_userdata*>(_ud);
    if (ud->item != nullptr) {
        item_release(ud->item);
        ud->item = nullptr;
    }
}

static int _items_metatable_len(lua_State *L) {
    vx_assert(L != nullptr);
    // docs seems to say only one argument is expected, but I get 2...
    // https://www.lua.org/manual/5.3/manual.html (see __len)
    LUAUTILS_ASSERT_ARGS_COUNT(L, 2)
    LUAUTILS_ASSERT_ARG_TYPE(L, 1, ITEM_TYPE_ITEMS)
    LUAUTILS_STACK_SIZE(L)
    
    CGame *g = nullptr;
    if (lua_utils_getGamePtr(L, &g) == false) {
        LUAUTILS_INTERNAL_ERROR(L) // returns
    }
    
    GameConfig *config = game_getConfig(g);
    
    int len = game_config_items_len(config);
    
    lua_pushinteger(L, len);
    
    LUAUTILS_STACK_SIZE_ASSERT(L, 1);
    return 1;
}

int lua_item_snprintf(lua_State *L, char *result, size_t maxLen, bool spacePrefix) {
    vx_assert(L != nullptr);
    LUAUTILS_STACK_SIZE(L)
    item_userdata *ud = static_cast<item_userdata*>(lua_touserdata(L, 1));
    Item *item = ud->item;
    LUAUTILS_STACK_SIZE_ASSERT(L, 0)
    return snprintf(result, maxLen, "%s[Item %s]", spacePrefix ? " " : "", item->fullname);
}

int lua_items_snprintf(lua_State *L, char *result, size_t maxLen, bool spacePrefix) {
    vx_assert(L != nullptr);
    LUAUTILS_STACK_SIZE(L)
    LUAUTILS_STACK_SIZE_ASSERT(L, 0)
    return snprintf(result, maxLen, "%s[Items]", spacePrefix ? " " : "");
}
