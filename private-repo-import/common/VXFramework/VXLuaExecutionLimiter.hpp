//
//  VXLuaExecutionLimiter.hpp
//  Cubzh
//
//  Created by Gaetan de Villele on 18/10/2021.
//

#pragma once

// C++
#include <string>
#include <unordered_set>

// Lua
typedef struct lua_Debug lua_Debug;
typedef struct lua_State lua_State;

// xptools
#include "Macros.h"

namespace vx {

/// This type isn't thread-safe.
/// Each instance of this class should always be used in the same thread.
class LuaExecutionLimiter final {
    
public:
    
    /// Constructor
    LuaExecutionLimiter();
    
    /// Destructor
    ~LuaExecutionLimiter();
    
    bool isFuncDisabled(const std::string& funcName);
    void startLimitation(lua_State * const L, const std::string& funcName);
    void cancelLimitation(lua_State * const L);
    void setBlockAllFuncs(bool blockAll);
    bool isAllFuncsDisabled();

    /// used internally
    void disable(lua_State * const L, std::string funcName = "");
    
    ///
    void reset();
    
protected:
    
    VX_DISALLOW_COPY_AND_ASSIGN(LuaExecutionLimiter)
    
private:
    
    ///
    std::string _currentLuaFuncName;
    
    /// Names of the Lua function that have been disabled.
    std::unordered_set<std::string> _disabledFuncNames;
    
    /// Block all functions
    bool _blockAll;
};

}
