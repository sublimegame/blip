//
//  lua_utils.cpp
//  Cubzh
//
//  Created by Gaetan de Villele on 20/08/2019.
//  Copyright © 2019 Voxowl Inc. All rights reserved.
//

#include "lua_utils.hpp"

#include "lua.hpp"
#include "lua_constants.h"

#include "VXGame.hpp"

#define LUA_REGISTRY_GAME_PTR_KEY "_cgptr"
#define LUA_REGISTRY_GAME_CPP_WRAPPER_PTR_KEY "_gptr"

#define LUA_REGISTRY_SYSTEM_GLOBALS_KEY "_sg"
#define LUA_REGISTRY_ROOT_GLOBALS_KEY "_rg"

static struct GlobalOptions {
    int optimizationLevel = 1;
    int debugLevel = 1;
} globalOptions;

static Luau::CompileOptions copts() {
    Luau::CompileOptions result = {};
    result.optimizationLevel = globalOptions.optimizationLevel;
    result.debugLevel = globalOptions.debugLevel;
    result.typeInfoLevel = 1;
    // result.coverageLevel = coverageActive() ? 2 : 0;
    result.coverageLevel = 0;
    return result;
}

//static int luau_loadstring(lua_State* L) {
//    size_t l = 0;
//    const char* s = luaL_checklstring(L, 1, &l);
//    const char* chunkname = luaL_optstring(L, 2, s);
//
//    // lua_setsafeenv(L, LUA_ENVIRONINDEX, false);
//
//    std::string bytecode = Luau::compile(std::string(s, l), copts());
//    if (luau_load(L, chunkname, bytecode.data(), bytecode.size(), 0) == 0) {
//        // int status = lua_resume(L, nullptr, 0);
//        return 1;
//    }
//
//    lua_pushnil(L);
//    lua_insert(L, -2); // put before error message
//    return 2;          // return nil plus error message
//}

// pushes function on success, error on failure
bool luau_loadstring(lua_State* L, const std::string& script, const std::string& chunkname) {
    std::string bytecode = Luau::compile(script, copts());
    if (luau_load(L, chunkname.c_str(), bytecode.data(), bytecode.size(), 0) != 0) {
        return false;
    }
    return true;
}

bool luau_dostring(lua_State* L, const std::string& script, const std::string& chunkname) {
    // lua_setsafeenv(L, LUA_ENVIRONINDEX, false);
    std::string bytecode = Luau::compile(script, copts());
    if (luau_load(L, chunkname.c_str(), bytecode.data(), bytecode.size(), 0) != 0) {
        return false;
    }
    if (lua_pcall(L, 0, LUA_MULTRET, 0) != 0) {
        std::string err = "";
        if (lua_isstring(L, -1)) {
            err = lua_tostring(L, -1);
        } else {
            err = "require() error";
        }
        lua_pop(L, 1); // pop error message
        vxlog_error("dostring error: %s", err.c_str());
        return false;
    }
    return true;
}

std::string luau_compile(const std::string& script) {
    return Luau::compile(script, copts());
}

//static bool luau_dostring(lua_State* L) {
//    size_t l = 0;
//    const char* s = luaL_checklstring(L, 1, &l);
//    const char* chunkname = luaL_optstring(L, 2, s);
//
//    // lua_setsafeenv(L, LUA_ENVIRONINDEX, false);
//
//    std::string bytecode = Luau::compile(std::string(s, l), copts());
//    if (luau_load(L, chunkname, bytecode.data(), bytecode.size(), 0) != 0) {
//        return false;
//    }
//
//    lua_call(L, 0, 0);
//    return true;
//}


///
bool lua_utils_setPointerInRegistry(lua_State *L, const char *key, void *ptr) {
    vx_assert(L != nullptr);
    if (key == nullptr) {
        return false;
    }
    lua_pushlightuserdata(L, ptr);
    lua_setfield(L, LUA_REGISTRYINDEX, key);
    return true;
}

///
bool lua_utils_getPointerInRegistry(lua_State *L, const char *key, void **ptrRef) {
    vx_assert(L != nullptr);
    if (key == nullptr) {
        return false;
    }
    if (ptrRef == nullptr) {
        return false;
    }
    lua_getfield(L, LUA_REGISTRYINDEX, key);
    if (lua_islightuserdata(L, -1) == false) {
        lua_pop(L, 1); // pop the pushed value (can be nil)
        return false;
    }
    *ptrRef = lua_touserdata(L, -1);
    lua_pop(L, 1); // pop the light userdata
    return true;
}

///
bool lua_utils_setGamePtr(lua_State *L, CGame *g) {
    lua_State *GL = lua_mainthread(L); // enforcing main thread for function to work from any Lua thread
    return lua_utils_setPointerInRegistry(GL, LUA_REGISTRY_GAME_PTR_KEY, g);
}

bool lua_utils_setGameCppWrapperPtr(lua_State *L, void *cppGame) {
    lua_State *GL = lua_mainthread(L); // enforcing main thread for function to work from any Lua thread
    return lua_utils_setPointerInRegistry(GL, LUA_REGISTRY_GAME_CPP_WRAPPER_PTR_KEY, cppGame);
}

///
bool lua_utils_getGamePtr(lua_State *L, CGame **gRef) {
    lua_State *GL = lua_mainthread(L); // enforcing main thread for function to work from any Lua thread
    return lua_utils_getPointerInRegistry(GL, LUA_REGISTRY_GAME_PTR_KEY, reinterpret_cast<void **>(gRef));
}

bool lua_utils_getGameCppWrapperPtr(lua_State *L, vx::Game **cppGameRef) {
    lua_State *GL = lua_mainthread(L); // enforcing main thread for function to work from any Lua thread
    void *ref = nullptr;
    bool found = lua_utils_getPointerInRegistry(GL, LUA_REGISTRY_GAME_CPP_WRAPPER_PTR_KEY, &ref);
    if (found) {
        *cppGameRef = reinterpret_cast<vx::Game*>(ref);
    }
    return found;
}

void lua_utils_setRootGlobalsInRegistry(lua_State *L, const int idx) {
    LUAUTILS_STACK_SIZE(L)
    lua_pushvalue(L, idx);
    lua_setfield(L, LUA_REGISTRYINDEX, LUA_REGISTRY_ROOT_GLOBALS_KEY);
    LUAUTILS_STACK_SIZE_ASSERT(L, 0)
}

void lua_utils_pushRootGlobalsFromRegistry(lua_State *L) {
    LUAUTILS_STACK_SIZE(L)
    lua_getfield(L, LUA_REGISTRYINDEX, LUA_REGISTRY_ROOT_GLOBALS_KEY);
    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
}

void lua_utils_setSystemGlobalsInRegistry(lua_State *L, const int idx) {
    LUAUTILS_STACK_SIZE(L)
    lua_pushvalue(L, idx);
    lua_setfield(L, LUA_REGISTRYINDEX, LUA_REGISTRY_SYSTEM_GLOBALS_KEY);
    LUAUTILS_STACK_SIZE_ASSERT(L, 0)
}

void lua_utils_pushSystemGlobalsFromRegistry(lua_State *L) {
    LUAUTILS_STACK_SIZE(L)
    lua_getfield(L, LUA_REGISTRYINDEX, LUA_REGISTRY_SYSTEM_GLOBALS_KEY);
    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
}

int lua_utils_get_metafield_and_return_type(lua_State* L, int idx, const char* name, bool pushNilIfNotFound) {
    if (luaL_getmetafield(L, idx, name) == 0) {
        if (pushNilIfNotFound) {
            lua_pushnil(L);
        }
        return LUA_TNIL;
    } else {
        return lua_type(L, -1);
    }
}

void lua_utils_get_metafield(lua_State* L, int idx, const char* name, bool pushNilIfNotFound) {
    if (luaL_getmetafield(L, idx, name) == 0) {
        if (pushNilIfNotFound) {
            lua_pushnil(L);
        }
    }
}

int lua_get_global_and_return_type(lua_State* L, const char* name) {
    lua_getglobal(L, name);
    return lua_type(L, -1);
}

bool lua_utils_get8BitsMask(lua_State *L, const int idx, uint8_t *mask) {
    vx_assert(L != nullptr);
    vx_assert(mask != nullptr);
    LUAUTILS_STACK_SIZE(L)

    *mask = 0;

    if (lua_isnil(L, idx)) {
        LUAUTILS_STACK_SIZE_ASSERT(L, 0)
        return true; // mask remains 0
    }

    if (lua_utils_isIntegerStrict(L, idx)) { // single integer
        long long v = lua_tointeger(L, idx);
        if (v >= 1 && v <= 8) {
            *mask = 1 << (v - 1);
            LUAUTILS_STACK_SIZE_ASSERT(L, 0)
            return true;
        } else {
            LUAUTILS_STACK_SIZE_ASSERT(L, 0)
            return false;
        }
    } else if lua_istable(L, idx) { // table of integers
        // iterate over table:
        lua_pushvalue(L, idx); // table at -1
        lua_pushnil(L); // table at -2, nil at -1
        long long v;
        while(lua_next(L, -2) != 0) {
            if(lua_isnumber(L, -1)) {
                v = lua_tointeger(L, -1);
                if (v >= 1 && v <= 8) {
                    *mask |= 1 << (v - 1);
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
    }

    LUAUTILS_STACK_SIZE_ASSERT(L, 0)
    return true;
}

bool lua_utils_getMask(lua_State *L, const int idx, uint16_t *mask, uint8_t bits) {
    vx_assert(L != nullptr);
    vx_assert(mask != nullptr);
    LUAUTILS_STACK_SIZE(L)

    *mask = 0;

    if (lua_isnil(L, idx)) {
        LUAUTILS_STACK_SIZE_ASSERT(L, 0)
        return true; // mask remains 0
    }

    if (lua_utils_isIntegerStrict(L, idx)) { // single integer
        long long v = lua_tointeger(L, idx);
        if (v >= 1 && v <= bits) {
            *mask = 1 << (v - 1);
            LUAUTILS_STACK_SIZE_ASSERT(L, 0)
            return true;
        } else {
            LUAUTILS_STACK_SIZE_ASSERT(L, 0)
            return false;
        }
    } else if lua_istable(L, idx) { // table of integers
        // iterate over table:
        lua_pushvalue(L, idx); // table at -1
        lua_pushnil(L); // table at -2, nil at -1
        long long v;
        while(lua_next(L, -2) != 0) {
            if(lua_isnumber(L, -1)) {
                v = lua_tointeger(L, -1);
                if (v >= 1 && v <= bits) {
                    *mask |= 1 << (v - 1);
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
    }

    LUAUTILS_STACK_SIZE_ASSERT(L, 0)
    return true;
}

void lua_utils_pushMaskAsTable(lua_State *L, const uint16_t mask, uint8_t bits) {
    vx_assert(L != nullptr);
    LUAUTILS_STACK_SIZE(L)

    uint8_t numbers[8];
    uint8_t count = 0;
    for (int i = 0; i < bits; ++i) {
        if ((mask & (1 << i)) != 0) {
            numbers[count] = i + 1;
            ++count;
        }
    }

    
    lua_createtable(L, count, 0);

    for (int i = 0; i < count; ++i) {
        lua_pushinteger(L, i + 1);
        lua_pushnumber(L, numbers[i]);
        lua_settable(L, -3);
    }

    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
}

LUA_ITEM_TYPE lua_utils_getObjectType(lua_State *L, const int idx) {
    vx_assert(L != nullptr);
    
    LUA_ITEM_TYPE result = ITEM_TYPE_NONE;

    if (lua_isnil(L, idx)) {
        return result;
    }

    if (lua_istable(L, idx) == false && lua_isuserdata(L, idx) == false) {
        return result;
    }

    LUAUTILS_STACK_SIZE(L)

    // type enums now stored within __typen for quick comparison
    if (luaL_getmetafield(L, idx, "__typen") != 0) {
        if (lua_utils_isIntegerStrict(L, -1)) {
            result = static_cast<LUA_ITEM_TYPE>(lua_tointeger(L, -1));
            lua_pop(L, 1);
            LUAUTILS_STACK_SIZE_ASSERT(L, 0)
            return result;
        } else {
            lua_pop(L, 1);
        }
    }

    if (luaL_getmetafield(L, idx, P3S_LUA_OBJ_METAFIELD_TYPE) == 0) {
        // the metatable or metafield could not be found
        LUAUTILS_STACK_SIZE_ASSERT(L, 0)
        return result;
    }
    
    if (lua_utils_isIntegerStrict(L, -1) == false) {
        // the __type value is not an integer
        lua_pop(L, 1);
        LUAUTILS_STACK_SIZE_ASSERT(L, 0)
        return result;
    }
    
    result = static_cast<LUA_ITEM_TYPE>(lua_tointeger(L, -1));
    lua_pop(L, 1);
    
    LUAUTILS_STACK_SIZE_ASSERT(L, 0)
    return result;
}

bool lua_utils_isStringStrict(lua_State *L, const int idx) {
    if (L == nullptr) { return false; }
    return lua_type(L, idx) == LUA_TSTRING;
}

bool lua_utils_isNumberStrict(lua_State *L, const int idx) {
    if (L == nullptr) { return false; }
    return lua_type(L, idx) == LUA_TNUMBER;
}

bool lua_utils_isIntegerStrict(lua_State *L, const int idx) {
    if (L == nullptr) { return false; }
    if (lua_type(L, idx) != LUA_TNUMBER) return false;
    lua_Number n = lua_tonumber(L, idx);
    return n == floor(n);
}

bool lua_utils_isStringStrictStartingWithUppercase(lua_State *L, const int idx) {
    if (lua_utils_isStringStrict(L, idx) == false) {
        return false;
    }
    size_t len = 0;
    const char *str = luaL_checklstring(L, idx, &len);
    if (str == nullptr) {
        vxlog_error("failed to read string from stack");
        return false;
    }
    if (len == 0) {
        // empty string
        return false;
    }
    return isupper(str[0]);
}

///
bool lua_utils_checkArgsCountOld(lua_State *L,
                                 const int expectedArgsCount,
                                 const char *functionName) {
    const int n = lua_gettop(L); // number of arguments received by the function
    if (n != expectedArgsCount) {
        vxlog_error("[%s] incorrect args count. Got %u, expected %u", functionName, n, expectedArgsCount);
        return false;
    }
    return true;
}

#define MODULE_IDENTIFIER_LINES 1
#define MODULE_SCOPE_PREFIX_LINES 0
void luautils_error(lua_State *L, const char *msg) {
    // TODO: fix this, show full error stack
    
//    lua_Debug d;
//    if (lua_getstack(L, 1, &d) == 1) {
//        lua_getinfo(L, "Sl", &d);
//        if (d.currentline > 0) {
//            if (strncmp(d.source, "--__MODULE__", 12) == 0) {
//                const char *newlineChar = strchr(d.source, '\n');
//                if (newlineChar != nullptr) {
//                    size_t length = newlineChar - (d.source + 12);
//                    char moduleName[255];
//                    strncpy(moduleName, &d.source[12], length);
//                    moduleName[length] = '\0';
//                    lua_pushfstring(L, "%s:L%d: %s", moduleName, d.currentline - MODULE_IDENTIFIER_LINES - MODULE_SCOPE_PREFIX_LINES, msg);
//                } else {
//                    lua_pushfstring(L, "MODULE:L%d: %s", d.currentline - MODULE_IDENTIFIER_LINES - MODULE_SCOPE_PREFIX_LINES, msg);
//                }
//            } else {
//                lua_pushfstring(L, "L%d: %s", d.currentline, msg);
//            }
//        } else {
//            lua_pushfstring(L, "L%d: %s", d.currentline, msg);
//        }
//    } else {
//        lua_pushfstring(L, "%s", msg);
//    }
//    lua_error(L);

    std::string s = std::string(msg) + "\n" + lua_debugtrace(L);
    lua_pushfstring(L, "%s", s.c_str());

//    lua_pushfstring(L, "%s", msg);
    lua_error(L);
}
