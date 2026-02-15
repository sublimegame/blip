//
//  VXLuaExecutionLimiter.cpp
//  Cubzh
//
//  Created by Gaetan de Villele on 18/10/2021.
//

#include "VXLuaExecutionLimiter.hpp"

// Lua
#include "lua.hpp"
#include "lua_utils.hpp"
#include "game.h"
#include "VXGame.hpp"
#include "xptools.h"

using namespace vx;

#define LUA_EXECUTION_INSTRUCTIONS_LIMIT 50000000 // 50M instructions
// Checks time after that amount of instructions
#define LUA_EXECUTION_INSTRUCTIONS_TIME_CHECK 10000
// Aborts if execution takes more than given seconds
#define LUA_EXECUTION_INSTRUCTIONS_TIME_LIMIT 3 // secs (integer)

//
// MARK: - Local functions' prototypes -
//

void _luaHookInstructions(lua_State * const L, lua_Debug * const ar);



//
// MARK: - public -
//

LuaExecutionLimiter::LuaExecutionLimiter() :
_currentLuaFuncName(),
_disabledFuncNames(),
_blockAll(false) {}

LuaExecutionLimiter::~LuaExecutionLimiter() {}

bool LuaExecutionLimiter::isFuncDisabled(const std::string& funcName) {
    return _blockAll || (_disabledFuncNames.empty() == false &&
            _disabledFuncNames.find(funcName) != _disabledFuncNames.end());
}

bool LuaExecutionLimiter::isAllFuncsDisabled() {
    return _blockAll;
}

void LuaExecutionLimiter::startLimitation(lua_State * const L, const std::string& funcName) {
    // TODO: review this
    // setup timer for execution limiting
//    _currentLuaFuncName.assign(funcName);
//    
//    lua_pushvalue(L, LUA_GLOBALSINDEX);
//    lua_pushstring(L, "__leli"); // lue execution limiter instructions
//    lua_pushinteger(L, 0);
//    lua_rawset(L, -3);
//    
//    lua_pushstring(L, "__lelt"); // lue execution limiter time (in seconds)
//    lua_pushinteger(L, vx::device::timestampApple());
//    lua_rawset(L, -3);
//    lua_pop(L, 1); // pop global
//
//    lua_sethook(L, &_luaHookInstructions, LUA_MASKCOUNT, LUA_EXECUTION_INSTRUCTIONS_TIME_CHECK);
}

void LuaExecutionLimiter::cancelLimitation(lua_State * const L) {
    // TODO: review this
    // cancels Lua hook
//    lua_sethook(L, &_luaHookInstructions, 0, 0);
}

void LuaExecutionLimiter::setBlockAllFuncs(bool blockAll) {
    _blockAll = blockAll;
}

void LuaExecutionLimiter::disable(lua_State * const L, std::string funcName) {
    if (funcName.empty())
        funcName.assign(_currentLuaFuncName);
    _disabledFuncNames.insert(funcName);
}

void LuaExecutionLimiter::reset() {
    _currentLuaFuncName.clear();
    _disabledFuncNames.clear();
    _blockAll = false;
}



//
// MARK: - Local functions -
//

void _luaHookInstructions(lua_State * const L, lua_Debug * const ar) {
    // retrieve Game from Lua state
    lua_pushvalue(L, LUA_GLOBALSINDEX);
    lua_pushstring(L, "__leli"); // lue execution limiter instructions
    lua_rawget(L, -2);
    vx_assert(lua_utils_isIntegerStrict(L, -1));
    int operations = static_cast<int>(lua_tointeger(L, -1));
    lua_pop(L, 1);

    lua_pushstring(L, "__lelt"); // lue execution limiter time (in seconds)
    lua_rawget(L, -2);
    vx_assert(lua_utils_isIntegerStrict(L, -1));
    int startTime = static_cast<int>(lua_tointeger(L, -1));
    lua_pop(L, 1);
    
    operations = operations + LUA_EXECUTION_INSTRUCTIONS_TIME_CHECK;
    int elapsed = static_cast<int>(vx::device::timestampApple()) - startTime; // seconds
    
    lua_pushstring(L, "__leli"); // lue execution limiter instructions
    lua_pushinteger(L, operations);
    lua_rawset(L, -3);
    
    lua_pop(L, 1); // pop global
    
    if (operations >= LUA_EXECUTION_INSTRUCTIONS_LIMIT || elapsed > LUA_EXECUTION_INSTRUCTIONS_TIME_LIMIT) {
        LUAUTILS_ERROR(L, "execution taking too long");
    }
}
