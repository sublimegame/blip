//
//  lua_json.cpp
//  Cubzh
//
//  Created by Corentin Cailleaud on 05/03/2022.
//  Copyright 2022 Voxowl Inc. All rights reserved.
//

#include "lua_json.hpp"
#include "cJSON.h"
#include "config.h" // for vx_assert

#include <sstream>
#include <math.h>

#include "lua.hpp"
#include "lua_utils.hpp"
#include "lua_data.hpp"

#include "lua_constants.h"

#define LUA_G_JSON_FIELD_DECODE "Decode" // function
#define LUA_G_JSON_FIELD_ENCODE "Encode" // function

static int _g_json_newindex(lua_State *const L);
static int _g_json_decode(lua_State *const L);
static int _g_json_encode(lua_State *const L);
static void json_to_luatable(lua_State *const L, const cJSON *json);
static std::string escape(const std::string& input);

// json is expected to be valid (non NULL, decoded calling cJSON_Parse, error handle before calling this function)
void json_to_luatable(lua_State *const L, const cJSON *json) {
    if (cJSON_IsObject(json) || cJSON_IsArray(json)) {
        lua_newtable(L);
        cJSON *child = json->child;
        int tableIndex = 1;
        while (child) {
            if (cJSON_IsArray(json)) { // numeric key
                lua_pushinteger(L, tableIndex++);
            } else { // string key
                lua_pushstring(L, child->string);
            }
            // recursively create an object or an array
            json_to_luatable(L, child);
            if (cJSON_IsArray(json)) {
                // no key associated in ->string so the function at the end of
                // this function is not called, we need to do it here
                lua_settable(L, -3);
            }
            child = child->next;
        }
    }

    if (cJSON_IsBool(json)) {
        lua_pushboolean(L, cJSON_IsTrue(json));
    }
    if (cJSON_IsNull(json)) {
        lua_pushnil(L);
    }
    if (cJSON_IsNumber(json)) {
        if (abs(json->valuedouble - static_cast<double>(json->valueint)) < 1e-12) {
            lua_pushinteger(L, json->valueint);
        } else {
            lua_pushnumber(L, json->valuedouble);
        }
    }
    if (cJSON_IsString(json)) {
        lua_pushstring(L, json->valuestring);
    }

    // only if not root of json
    if (json->string) {
        lua_settable(L, -3);
    }
}

bool lua_g_json_create_and_push(lua_State *L) {
    vx_assert(L != nullptr);

    LUAUTILS_STACK_SIZE(L)

    lua_newtable(L);
    lua_newtable(L);
    {
        lua_pushstring(L, "__index");
        lua_newtable(L); // table for read-only keys
        {
            lua_pushstring(L, LUA_G_JSON_FIELD_DECODE);
            lua_pushcfunction(L, _g_json_decode, "");
            lua_rawset(L, -3);

            lua_pushstring(L, LUA_G_JSON_FIELD_ENCODE);
            lua_pushcfunction(L, _g_json_encode, "");
            lua_rawset(L, -3);
        }
        lua_rawset(L, -3);

        lua_pushstring(L, "__newindex");
        lua_pushcfunction(L, _g_json_newindex, "");
        lua_rawset(L, -3);

        lua_pushstring(L, "__metatable");
        lua_pushboolean(L, false);
        lua_settable(L, -3);

        lua_pushstring(L, P3S_LUA_OBJ_METAFIELD_TYPE);
        lua_pushinteger(L, ITEM_TYPE_G_JSON);
        lua_settable(L, -3);
    }
    lua_setmetatable(L, -2);

    LUAUTILS_STACK_SIZE_ASSERT(L, 1);
    return true;
}

static int _g_json_newindex(lua_State *L) {
    LUAUTILS_STACK_SIZE(L)
    LUAUTILS_ERROR(L, "JSON is read-only"); // returns
    LUAUTILS_STACK_SIZE_ASSERT(L, 0)
    return 0;
}

static int _g_json_decode(lua_State *L) {
    LUAUTILS_STACK_SIZE(L)

    const int argsCount = lua_gettop(L);

    if (argsCount < 1 || lua_utils_getObjectType(L, 1) != ITEM_TYPE_G_JSON) {
        LUAUTILS_ERROR(L, "JSON:Decode - function should be called with `:`"); // returns
    }
    if (argsCount > 2) {
        LUAUTILS_ERROR(L, "JSON:Decode - incorrect argument count"); // returns
    }

    std::string str = "";

    if (lua_utils_getObjectType(L, 2) == ITEM_TYPE_DATA) {
        str = lua_data_getDataAsString(L, 2);

    } else if (lua_isstring(L, 2)) {
        str = std::string(lua_tostring(L, 2));
    } else {
        LUAUTILS_ERROR(L, "JSON:Decode - argument 1 should be a string or a Data instance"); // returns
    }

    cJSON *decodedJson = cJSON_Parse(str.c_str());
    if (decodedJson == nullptr) {
        // Add nil result, then error message (local json,err = JSON:Decode(data))
        lua_pushnil(L);
        const char *error_ptr = cJSON_GetErrorPtr();
        if (error_ptr != nullptr) {
            lua_pushfstring(L, "JSON:Decode - Invalid JSON, before: %s", error_ptr);
        } else {
            lua_pushfstring(L, "JSON:Decode - Invalid JSON");
        }
    } else { // successfully decoded, decodedJson is 100% valid
        json_to_luatable(L, decodedJson);
        // success, add nil as err (local json,err = JSON:Decode(data))
        lua_pushnil(L);

        cJSON_Delete(decodedJson);
    }

    LUAUTILS_STACK_SIZE_ASSERT(L, 2)
    return 2;
}

void lua_table_to_json_string(std::ostringstream &encoded, lua_State * const L, const int tableIndex) {
    LUAUTILS_STACK_SIZE(L)

    if (lua_istable(L, tableIndex) == false) {
        LUAUTILS_ERROR(L, "JSON:Encode(table) - `table` should be a table"); // returns
    }

    std::ostringstream stringStream;
    int keyType;
    int valueType;

    double numberValue;

    int len = static_cast<int>(lua_objlen(L, tableIndex));

    if (len > 0) { // array
        for (lua_Integer i = 1; i <= len; ++i) {
            lua_rawgeti(L, tableIndex, i);
            valueType = lua_type(L, -1);

            // trigger error on unsupported value types
            // we can't skip them as we do with maps (json objects)
            // as it would offset the integer index.
            if (valueType != LUA_TSTRING && valueType != LUA_TNUMBER && valueType != LUA_TBOOLEAN && valueType != LUA_TTABLE) {
                lua_pop(L, 1);
                LUAUTILS_ERROR_F(L, "JSON:Encode(table) - unsupported value type in array: %s", lua_typename(L, valueType)); // returns
            }

            // Add comma between values
            if (i > 1) {
                stringStream << ",";
            }

            // write value
            if (lua_utils_isStringStrict(L, -1)) {
                std::string str = lua_tostring(L, -1);
                str = escape(str);
                stringStream << "\"" << str << "\"";
            } else if (lua_isnumber(L, -1)) {
                numberValue = lua_tonumber(L, -1);
                if (abs(floor(numberValue) - numberValue) < 1e-12)  {
                    stringStream << static_cast<long long>(numberValue);
                } else {
                    stringStream << numberValue;
                }
            } else if (lua_istable(L, -1)) {
                lua_table_to_json_string(stringStream, L, -1);
            } else if (lua_isboolean(L, -1)) {
                stringStream << (lua_toboolean(L, -1) ? "true" : "false");
            }

            lua_pop(L, 1); // pop value
        }
        encoded << "[" << stringStream.str() << "]";

    } else { // object
        lua_pushvalue(L, tableIndex); // place table on top
        lua_pushnil(L); // first key
        // nil: -1, table: -2

        bool firstValue = true;
        while (lua_next(L, -2) != 0) { // lua_next pops key at -1 (nil at first iteration)
            // value: -1, key: -2, table: -3

            valueType = lua_type(L, -1);
            keyType = lua_type(L, -2);

            // skip unsupported key types:
            if (keyType != LUA_TSTRING) {
                // just skip key if not a string, no error message
                // NOTE: it could be an option to fail on unsupported key types
                // (pop value, keep key for next iteration)
                lua_pop(L, 1);
                continue;
            }

            // skip unsupported value types:
            if (valueType != LUA_TSTRING && valueType != LUA_TNUMBER && valueType != LUA_TBOOLEAN && valueType != LUA_TTABLE) {
                // just skip value when encountering unsupported type, no error message
                // NOTE: it could be an option to fail on unsupported value types
                // (pop value, keep key for next iteration)
                lua_pop(L, 1);
                continue;
            }
            
            if (firstValue) {
                firstValue = false;
            } else {
                stringStream << ",";
            }

            // write key
            std::string str = lua_tostring(L, -2);
            str = escape(str);
            stringStream << "\"" << str << "\":";

            // write value
            if (lua_utils_isStringStrict(L, -1)) {
                std::string str = lua_tostring(L, -1);
                str = escape(str);
                stringStream << "\"" << str << "\"";
            } else if (lua_isnumber(L, -1)) {
                numberValue = lua_tonumber(L, -1);
                if (abs(floor(numberValue) - numberValue) < 1e-12)  {
                    stringStream << static_cast<long long>(numberValue);
                } else {
                    stringStream << numberValue;
                }
            } else if (lua_istable(L, -1)) {
                lua_table_to_json_string(stringStream, L, -1);
            } else if (lua_isboolean(L, -1)) {
                stringStream << (lua_toboolean(L, -1) ? "true" : "false");
            }
            // pop value, keep key for next iteration
            lua_pop(L, 1);
        }
        encoded << "{" << stringStream.str() << "}";
        lua_pop(L, 1); // pop table
    }

    LUAUTILS_STACK_SIZE_ASSERT(L, 0)
}

static int _g_json_encode(lua_State *L) {
    LUAUTILS_STACK_SIZE(L)

    const int argsCount = lua_gettop(L);

    if (argsCount < 1 || lua_utils_getObjectType(L, 1) != ITEM_TYPE_G_JSON) {
        LUAUTILS_ERROR(L, "JSON:Encode - function should be called with `:`"); // returns
    }
    if (argsCount > 2) {
        LUAUTILS_ERROR(L, "JSON:Encode - incorrect argument count"); // returns
    }

    if (lua_istable(L, 2) == false) {
        LUAUTILS_ERROR(L, "JSON:Encode - argument 1 should be a table"); // returns
    }

    std::ostringstream encodedStream;
    lua_table_to_json_string(encodedStream, L, 2);

    std::string encoded = encodedStream.str();
    lua_pushstring(L, encoded.c_str());

    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
    return 1;
}

int lua_g_json_snprintf(lua_State *L, char *result, size_t maxLen,
                        bool spacePrefix) {
    LUAUTILS_STACK_SIZE(L)
    LUAUTILS_STACK_SIZE_ASSERT(L, 0)
    return snprintf(result, maxLen, "%s[JSON]", spacePrefix ? " " : "");
}

std::string escape(const std::string& input) {
    // Using cJSON's print function to correctly escape characters.
    // NOTE: when parsing JSON, cJSON does unescape characters automatically,
    // no need to unescape after parsing.
    // Create a cJSON string object
    cJSON* json_str = cJSON_CreateString(input.c_str());
    if (json_str == nullptr) {
        return ""; // Handle error
    }

    char* printed = cJSON_PrintUnformatted(json_str);
    cJSON_Delete(json_str);

    if (printed == nullptr) {
        return ""; // Handle error
    }

    std::string result(printed);

    cJSON_free(printed);

    // remove the surrounding quotes
    if (result.length() >= 2 && result.front() == '"' && result.back() == '"') {
        result = result.substr(1, result.length() - 2);
    }

    return result;
}
