//
//  VXLinter.cpp
//  Blip
//
//  Created by Adrien Duermael on 12/08/2024.
//

#include "VXLinter.hpp"
#include "lua.hpp"
#include "lua_utils.hpp"
#include "xptools.h"
#include "config.h"

static int _luacheck_require_metatable_index(lua_State *L);
static int _luacheck_require_metatable_newindex(lua_State *L);
static int _luacheck_require_metatable_call(lua_State *L);

static lua_State *_luacheck_L = nullptr;

vx::Linter::LuaCheckReport vx::Linter::luaCheck(const std::string& script) {

    LuaCheckReport report;

    lua_State *L = _luacheck_L;

    if (L == nullptr) {
        L = luaL_newstate();
        luaL_openlibs(L);

        LUAUTILS_STACK_SIZE(L)

        // set `require`
        lua_pushvalue(L, LUA_GLOBALSINDEX);
        {
            lua_pushstring(L, "require");
            lua_newtable(L); // global "require" table
            lua_newtable(L); // metatable
            {
                lua_pushstring(L, "__index");
                lua_pushcfunction(L, _luacheck_require_metatable_index, "");
                lua_rawset(L, -3);

                lua_pushstring(L, "__newindex");
                lua_pushcfunction(L, _luacheck_require_metatable_newindex, "");
                lua_rawset(L, -3);

                lua_pushstring(L, "__call");
                lua_pushcfunction(L, _luacheck_require_metatable_call, "");
                lua_rawset(L, -3);

                // hide the metatable from the Lua sandbox user
                lua_pushstring(L, "__metatable");
                lua_pushboolean(L, false);
                lua_rawset(L, -3);
                lua_setmetatable(L, -2);
            }
            lua_rawset(L, -3);

            // load luacheck
            lua_pushstring(L, "luacheck");
            {
                FILE *file = vx::fs::openBundleFile("luacheck/init.lua", "rb");
                std::string luaCheckScript = "";
                vx_assert(file != nullptr);

                if (vx::fs::getFileTextContentAsStringAndClose(file, luaCheckScript) == false) {
                    // LUAUTILS_ERROR_F(L, "require(): can't load %s", path.c_str()); // returns
                    vx_assert(false);
                }

                if (luau_loadstring(L, luaCheckScript, "luacheck") == false) {
                    std::string err(lua_tostring(L, -1));
                    lua_pop(L, 1); // pop error message
                    vx_assert(false);
                }

                int errfunc = 0;
                if (lua_pcall(L, 0, LUA_MULTRET, errfunc) != 0) {
                    std::string err = "";
                    if (lua_isstring(L, -1)) {
                        err = lua_tostring(L, -1);
                    } else {
                        err = "require() error";
                    }
                    lua_pop(L, 1); // pop error message
                    lua_remove(L, errfunc);
                    vx_assert(false);
                }
            }
            lua_rawset(L, -3);

            // options
            lua_pushstring(L, "options");
            lua_newtable(L);
            {
                lua_pushstring(L, "unused"); lua_pushboolean(L, true); lua_rawset(L, -3);
                lua_pushstring(L, "redefined "); lua_pushboolean(L, true); lua_rawset(L, -3);
                lua_pushstring(L, "max_line_length"); lua_pushboolean(L, false); lua_rawset(L, -3);
                lua_pushstring(L, "allow_defined"); lua_pushboolean(L, true); lua_rawset(L, -3);

                lua_pushstring(L, "ignore");
                lua_newtable(L);
                {
                    lua_pushinteger(L, 1); lua_pushstring(L, "421"); lua_rawset(L, -3); // Shadowing a local variable.
                    lua_pushinteger(L, 2); lua_pushstring(L, "422"); lua_rawset(L, -3); // Shadowing an argument.
                    lua_pushinteger(L, 3); lua_pushstring(L, "423"); lua_rawset(L, -3); // Shadowing a loop variable
                    lua_pushinteger(L, 4); lua_pushstring(L, "431"); lua_rawset(L, -3); // Shadowing an upvalue.
                    lua_pushinteger(L, 5); lua_pushstring(L, "432"); lua_rawset(L, -3); // Shadowing an upvalue argument.
                    lua_pushinteger(L, 6); lua_pushstring(L, "433"); lua_rawset(L, -3); // Shadowing an upvalue loop variable.

                    // fixed by formatter
                    lua_pushinteger(L, 7); lua_pushstring(L, "611"); lua_rawset(L, -3); // A line consists of nothing but whitespace.
                    lua_pushinteger(L, 8); lua_pushstring(L, "612"); lua_rawset(L, -3); // A line contains trailing whitespace.
                    lua_pushinteger(L, 9); lua_pushstring(L, "613"); lua_rawset(L, -3); // Trailing whitespace in a string.
                    lua_pushinteger(L, 10); lua_pushstring(L, "614"); lua_rawset(L, -3); // Trailing whitespace in a comment.
                    lua_pushinteger(L, 11); lua_pushstring(L, "621"); lua_rawset(L, -3); // Inconsistent indentation (SPACE followed by TAB).
                }
                lua_rawset(L, -3);

                lua_pushstring(L, "globals");
                lua_newtable(L);
                {
                    lua_pushinteger(L, 1); lua_pushstring(L, "Camera"); lua_rawset(L, -3);
                    lua_pushinteger(L, 2); lua_pushstring(L, "Client"); lua_rawset(L, -3);
                    lua_pushinteger(L, 3); lua_pushstring(L, "Clouds"); lua_rawset(L, -3);
                    lua_pushinteger(L, 4); lua_pushstring(L, "Config"); lua_rawset(L, -3);
                    lua_pushinteger(L, 5); lua_pushstring(L, "Dev"); lua_rawset(L, -3);
                    lua_pushinteger(L, 6); lua_pushstring(L, "Fog"); lua_rawset(L, -3);
                    lua_pushinteger(L, 7); lua_pushstring(L, "Light"); lua_rawset(L, -3);
                    lua_pushinteger(L, 8); lua_pushstring(L, "Menu"); lua_rawset(L, -3);
                    lua_pushinteger(L, 9); lua_pushstring(L, "Player"); lua_rawset(L, -3);
                    lua_pushinteger(L, 10); lua_pushstring(L, "Pointer"); lua_rawset(L, -3);
                    lua_pushinteger(L, 11); lua_pushstring(L, "Screen"); lua_rawset(L, -3);
                    lua_pushinteger(L, 12); lua_pushstring(L, "Sky"); lua_rawset(L, -3);
                    lua_pushinteger(L, 13); lua_pushstring(L, "System"); lua_rawset(L, -3);
                    lua_pushinteger(L, 14); lua_pushstring(L, "CollisionGroups"); lua_rawset(L, -3);
                    lua_pushinteger(L, 15); lua_pushstring(L, "Data"); lua_rawset(L, -3);
                    lua_pushinteger(L, 16); lua_pushstring(L, "Server"); lua_rawset(L, -3);
                    lua_pushinteger(L, 17); lua_pushstring(L, "Map"); lua_rawset(L, -3);
                }
                lua_rawset(L, -3);

                lua_pushstring(L, "read_globals");
                lua_newtable(L);
                {
                    lua_pushinteger(L, 1); lua_pushstring(L, "Animation"); lua_rawset(L, -3);
                    lua_pushinteger(L, 2); lua_pushstring(L, "Assets"); lua_rawset(L, -3);
                    lua_pushinteger(L, 3); lua_pushstring(L, "AssetType"); lua_rawset(L, -3);
                    lua_pushinteger(L, 4); lua_pushstring(L, "AudioListener"); lua_rawset(L, -3);
                    lua_pushinteger(L, 5); lua_pushstring(L, "AudioSource"); lua_rawset(L, -3);
                    lua_pushinteger(L, 6); lua_pushstring(L, "Box"); lua_rawset(L, -3);
                    lua_pushinteger(L, 7); lua_pushstring(L, "Color"); lua_rawset(L, -3);
                    lua_pushinteger(L, 8); lua_pushstring(L, "Event"); lua_rawset(L, -3);
                    lua_pushinteger(L, 9); lua_pushstring(L, "Environment"); lua_rawset(L, -3);
                    lua_pushinteger(L, 10); lua_pushstring(L, "Face"); lua_rawset(L, -3);
                    lua_pushinteger(L, 11); lua_pushstring(L, "File"); lua_rawset(L, -3);
                    lua_pushinteger(L, 12); lua_pushstring(L, "HTTP"); lua_rawset(L, -3);
                    lua_pushinteger(L, 13); lua_pushstring(L, "Items"); lua_rawset(L, -3);
                    lua_pushinteger(L, 14); lua_pushstring(L, "JSON"); lua_rawset(L, -3);
                    lua_pushinteger(L, 15); lua_pushstring(L, "KeyValueStore"); lua_rawset(L, -3);
                    lua_pushinteger(L, 16); lua_pushstring(L, "LightType"); lua_rawset(L, -3);
                    lua_pushinteger(L, 17); lua_pushstring(L, "LocalEvent"); lua_rawset(L, -3);
                    lua_pushinteger(L, 18); lua_pushstring(L, "Font"); lua_rawset(L, -3);
                    lua_pushinteger(L, 19); lua_pushstring(L, "MutableShape"); lua_rawset(L, -3);
                    lua_pushinteger(L, 20); lua_pushstring(L, "Number2"); lua_rawset(L, -3);
                    lua_pushinteger(L, 21); lua_pushstring(L, "Number3"); lua_rawset(L, -3);
                    lua_pushinteger(L, 22); lua_pushstring(L, "Object"); lua_rawset(L, -3);
                    lua_pushinteger(L, 23); lua_pushstring(L, "OtherPlayers"); lua_rawset(L, -3);
                    lua_pushinteger(L, 24); lua_pushstring(L, "PhysicsMode"); lua_rawset(L, -3);
                    lua_pushinteger(L, 25); lua_pushstring(L, "Players"); lua_rawset(L, -3);
                    lua_pushinteger(L, 26); lua_pushstring(L, "PointerEvent"); lua_rawset(L, -3);
                    lua_pushinteger(L, 27); lua_pushstring(L, "Private"); lua_rawset(L, -3);
                    lua_pushinteger(L, 28); lua_pushstring(L, "ProjectionMode"); lua_rawset(L, -3);
                    lua_pushinteger(L, 29); lua_pushstring(L, "Quad"); lua_rawset(L, -3);
                    lua_pushinteger(L, 30); lua_pushstring(L, "Ray"); lua_rawset(L, -3);
                    lua_pushinteger(L, 31); lua_pushstring(L, "Rotation"); lua_rawset(L, -3);
                    lua_pushinteger(L, 32); lua_pushstring(L, "Shape"); lua_rawset(L, -3);
                    lua_pushinteger(L, 33); lua_pushstring(L, "Text"); lua_rawset(L, -3);
                    lua_pushinteger(L, 34); lua_pushstring(L, "TextType"); lua_rawset(L, -3);
                    lua_pushinteger(L, 35); lua_pushstring(L, "Time"); lua_rawset(L, -3);
                    lua_pushinteger(L, 36); lua_pushstring(L, "Timer"); lua_rawset(L, -3);
                    lua_pushinteger(L, 37); lua_pushstring(L, "Type"); lua_rawset(L, -3);
                    lua_pushinteger(L, 38); lua_pushstring(L, "URL"); lua_rawset(L, -3);
                    lua_pushinteger(L, 39); lua_pushstring(L, "World"); lua_rawset(L, -3);
                }
                lua_rawset(L, -3);
            }
            lua_rawset(L, -3);

        }
        lua_pop(L, 1); // pop global

        _luacheck_L = L;
        // lua_close(L); // keep lua state active for following checks
        LUAUTILS_STACK_SIZE_ASSERT(L, 0)
    }

    LUAUTILS_STACK_SIZE(L)

    // docs: https://luacheck.readthedocs.io/en/stable/module.html
    lua_getglobal(L, "luacheck");
    lua_getfield(L, -1, "check_strings");
    vx_assert(lua_isfunction(L, -1));

    lua_newtable(L);
    {
        lua_pushstring(L, script.c_str()); lua_rawseti(L, -2, 1);
    }

    // options
    lua_getglobal(L, "options");

    // call check_strings
    if (lua_pcall(L, 2, 1, 0) != LUA_OK) {
        // NOT: check_strings failed
        // vxlog_error("check_strings FAILURE: error type: %s", lua_typename(L, lua_type(L, -1)));

        // When check_strings, it returns an error_wrapper table,
        // containing an err table, itself containing line and msg information.
        // Reading the docs, it doesn't seem like the expected behavior,
        // but logic when reading luacheck's code, diving from check_strings implementation.
        if (lua_istable(L, -1)) {
            lua_getfield(L, -1, "err");

            LuaCheckIssue issue;
            issue.code = "011"; // syntax error

            lua_getfield(L, -1, "line");
            if (lua_utils_isIntegerStrict(L, -1)) {
                issue.line = static_cast<int>(lua_tointeger(L, -1));
            }
            lua_pop(L, 1);

            lua_getfield(L, -1, "msg");
            if (lua_isstring(L, -1)) {
                issue.message = std::string(lua_tostring(L, -1));
            }
            lua_pop(L, 1);

            lua_pop(L, 1); // pop error_wrapper & err

            if (report.find(issue.line) == report.end()) {
                LuaCheckIssues issues{0, 0, std::vector<LuaCheckIssue>(), std::vector<LuaCheckIssue>()};
                report[issue.line] =  issues;
            }

            report[issue.line].nErrors += 1;
            report[issue.line].errors.push_back(issue);
        }


        lua_pop(L, 2); // pop error & luacheck
        LUAUTILS_STACK_SIZE_ASSERT(L, 0)
        return report;
    }

    LUAUTILS_STACK_SIZE_ASSERT(L, 2) // luacheck + reports
    vx_assert(lua_istable(L, -1));

    lua_pushnil(L); // reports at -2, nil at -1
    while(lua_next(L, -2) != 0) {
        // reports at -3, report_index at -2, report at -1
        if (lua_istable(L, -1)) {
            lua_pushnil(L);
            // reports at -4, report_index at -3, report at -2, nil at -1
            while(lua_next(L, -2) != 0) {
                // luacheck (-6), reports (-5), report_index (-4), report (-3), issue_index (-2), issue at (-1)

                LuaCheckIssue issue;

                lua_getfield(L, -6, "get_message");
                lua_pushvalue(L, -2);
                if (lua_pcall(L, 1, 1, 0) != LUA_OK) {
                    if (lua_utils_isStringStrict(L, -1)) {
                        fprintf(stderr, "Error (2): %s\n", lua_tostring(L, -1));
                    }
                    lua_pop(L, 1); // pop error
                } else {
                    issue.message = std::string(lua_tostring(L, -1));
                    lua_pop(L, 1); // pop message string
                }

                lua_getfield(L, -1, "line");
                if (lua_utils_isIntegerStrict(L, -1)) {
                    issue.line = static_cast<int>(lua_tointeger(L, -1));
                }
                lua_pop(L, 1);

                lua_getfield(L, -1, "column");
                if (lua_utils_isIntegerStrict(L, -1)) {
                    issue.column = static_cast<int>(lua_tointeger(L, -1));
                }
                lua_pop(L, 1);

                lua_getfield(L, -1, "code");
                if (lua_isstring(L, -1)) {
                    issue.code = std::string(lua_tostring(L, -1));
                }
                lua_pop(L, 1);

                lua_getfield(L, -1, "name");
                if (lua_isstring(L, -1)) {
                    issue.variableName = std::string(lua_tostring(L, -1));
                }
                lua_pop(L, 1);

                // unused global variables "Config", "Modules"
                if (issue.code == "131" && (issue.variableName == "Config" || issue.variableName == "Modules")) {
                    // ignore
                } else {
                    if (report.find(issue.line) == report.end()) {
                        LuaCheckIssues issues{0, 0, std::vector<LuaCheckIssue>(), std::vector<LuaCheckIssue>()};
                        report[issue.line] =  issues;
                    }

                    if (issue.code.front() == '0') {
                        report[issue.line].nErrors += 1;
                        report[issue.line].errors.push_back(issue);
                    } else {
                        report[issue.line].nWarnings += 1;
                        report[issue.line].warnings.push_back(issue);
                    }
                    // vxlog_error("L%d:%d %s - CODE: %s", issue.line, issue.column, issue.message.c_str(), issue.code.c_str());
                }

                lua_pop(L, 1); // pop issue
            }
        }
        lua_pop(L, 1); // pop report
    }

    LUAUTILS_STACK_SIZE_ASSERT(L, 2) // luacheck + reports
    lua_pop(L, 1); // pop reports
    lua_pop(L, 1); // remove luacheck table
    LUAUTILS_STACK_SIZE_ASSERT(L, 0)
    return report;
}

static int _luacheck_require_metatable_index(lua_State *L) {
    LUAUTILS_STACK_SIZE(L)
    LUAUTILS_ERROR(L, "require has no property, call require(\"libraryName\")"); // returns
    LUAUTILS_STACK_SIZE_ASSERT(L, 0)
    return 0;
}

static int _luacheck_require_metatable_newindex(lua_State *L) {
    LUAUTILS_STACK_SIZE(L)
    LUAUTILS_ERROR(L, "require is read-only"); // returns
    LUAUTILS_STACK_SIZE_ASSERT(L, 0)
    return 0;
}

static int _luacheck_require_metatable_call(lua_State *L) {
    LUAUTILS_STACK_SIZE(L)
    std::string moduleName(lua_tostring(L, 2));

    std::string path = moduleName;
    bool b = true;
    while (b) {
        b = vx::str::replaceStringInString(path, ".", "/");
    }

    bool isDir;
    bool exists = vx::fs::bundleFileExists(path, isDir);

    if (exists == false) {
        // try with .lua extension
        path = path + ".lua";
        exists = vx::fs::bundleFileExists(path, isDir);
        vx_assert(exists);
    }

    if (isDir) {
        path = path + "/init.lua";
        exists = vx::fs::bundleFileExists(path, isDir);
        vx_assert(exists);
    }

    FILE *file = vx::fs::openBundleFile(path, "rb");
    std::string script = "";

    vx_assert(file != nullptr);


    if (vx::fs::getFileTextContentAsStringAndClose(file, script) == false) {
        // LUAUTILS_ERROR_F(L, "require(): can't load %s", path.c_str()); // returns
        vx_assert(false);
    }

    const int stackSizeBefore = lua_gettop(L);

    if (luau_loadstring(L, script, "luacheck call") == false) {
        std::string err(lua_tostring(L, -1));
        lua_pop(L, 1); // pop error message
        vx_assert(false);
    }

    int errfunc = 0;
    if (lua_pcall(L, 0, LUA_MULTRET, errfunc) != 0) {
        std::string err = "";
        if (lua_isstring(L, -1)) {
            err = lua_tostring(L, -1);
        } else {
            err = "require() error";
        }
        lua_pop(L, 1); // pop error message
        lua_remove(L, errfunc);
        vx_assert(false);
    }

    const int stackSizeAfter = lua_gettop(L);
    const int stackSize = stackSizeAfter - stackSizeBefore;

    LUAUTILS_STACK_SIZE_ASSERT(L, stackSize)
    return stackSize;
}

