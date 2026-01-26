//
//  VXLinter.hpp
//  Cubzh
//
//  Created by Adrien Duermael on 12/08/2024.
//

#pragma once

// C++
#include <map>
#include <string>
#include <unordered_map>
#include <vector>

namespace vx {
class Linter final {
public:
    enum class LuaCheckIssueType {
        Warning,
        Error,
        Fatal
    };

    typedef struct {
        int line;
        int column;
        std::string code;
        std::string variableName;
        std::string message;
        LuaCheckIssueType type;
    } LuaCheckIssue;

    typedef struct {
        int nErrors;
        int nWarnings;
        std::vector<LuaCheckIssue> errors;
        std::vector<LuaCheckIssue> warnings;
    } LuaCheckIssues;

    typedef std::unordered_map<int, LuaCheckIssues> LuaCheckReport;

    static LuaCheckReport luaCheck(const std::string& script);
};
}

