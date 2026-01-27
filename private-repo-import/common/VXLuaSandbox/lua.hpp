// lua.hpp
// Lua header files for C++
// <<extern "C">> not supplied automatically because Lua also compiles as C++

//extern "C" {
//#include "lua.h"
//#include "lualib.h"
////#include "lauxlib.h"
//}

#pragma once

#include "lua.h"
#include "lualib.h"

#include "Luau/Common.h"
#include "Luau/Compiler.h"
#include "Luau/Ast.h"
#include "Luau/Parser.h"
#include "Luau/ParseOptions.h"
#include "Luau/Allocator.h"
#include "Luau/Module.h"
#include "Luau/Linter.h"
#include "Luau/TypeArena.h"
#include "Luau/Type.h"
#include "Luau/Frontend.h"
#include "Luau/TypeInfer.h"
#include "Luau/ToString.h"
#include "Luau/TypeChecker2.h"
#include "Luau/NotNull.h"
#include "Luau/BuiltinDefinitions.h"
#include "Luau/FileUtils.h"
#include "Luau/Require.h"
#include "Luau/TypeAttach.h"
#include "Luau/Transpiler.h"
//#include "luau_utils.hpp"

//struct GlobalOptions {
//    int optimizationLevel = 1;
//    int debugLevel = 1;
//} globalOptions;
//
//static Luau::CompileOptions copts() {
//    Luau::CompileOptions result = {};
//    result.optimizationLevel = globalOptions.optimizationLevel;
//    result.debugLevel = globalOptions.debugLevel;
//    result.typeInfoLevel = 1;
//    // result.coverageLevel = coverageActive() ? 2 : 0;
//    result.coverageLevel = 0;
//    return result;
//}

//static int lua_loadstring(lua_State* L) {
//    size_t l = 0;
//    const char* s = luaL_checklstring(L, 1, &l);
//    const char* chunkname = luaL_optstring(L, 2, s);
//
//    lua_setsafeenv(L, LUA_ENVIRONINDEX, false);
//
//    std::string bytecode = Luau::compile(std::string(s, l), copts());
//    if (luau_load(L, chunkname, bytecode.data(), bytecode.size(), 0) == 0) {
//        return 1;
//    }
//
//    lua_pushnil(L);
//    lua_insert(L, -2); // put before error message
//    return 2;          // return nil plus error message
//}

//#define luaL_loadstring(L, s) luau_load(L, "chunk", s, strlen(s), 0)

// TODO: pcall might be necessary after load
//#define luaL_dostring(L, s) luau_load(L, "chunk", s, strlen(s), 0)
