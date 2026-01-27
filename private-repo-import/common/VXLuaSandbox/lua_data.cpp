//
//  lua_data.cpp
//  Cubzh
//
//  Created by Xavier Legland on 17/03/2022.
//  Copyright 2022 Voxowl Inc. All rights reserved.
//

#include "lua_data.hpp"

// C++
#include <cmath>
#include <cstring>
#include <stdexcept>
#include <float.h>

// Lua
#include "lua.hpp"
#include "lua_utils.hpp"
#include "lua_constants.h"
#include "lua_logs.hpp"
#include "lua_number3.hpp"
#include "lua_rotation.hpp"
#include "lua_color.hpp"
#include "lua_physicsMode.hpp"
#include "lua_collision_groups.hpp"
#include "lua_bin_serializer.hpp"

#include "config.h"
#include "utils.h"
#include "xptools.h"
#include "encoding_base64.hpp"

#define LUA_G_DATA_FIELD_FROMBUNDLE "FromBundle"             // function

#define LUA_DATA_FIELD_LEN "Len"                             // number (deprecated)
#define LUA_DATA_FIELD_LENGTH "Length"                       // number
#define LUA_DATA_FIELD_CURSOR "Cursor"                       // number

#define LUA_DATA_FIELD_TO_STRING "ToString"                  // function
#define LUA_DATA_FIELD_TO_TABLE "ToTable"                    // function

#define LUA_DATA_FIELD_WRITE_BYTE "WriteByte"                // function
#define LUA_DATA_FIELD_WRITE_STRING "WriteString"            // function
#define LUA_DATA_FIELD_WRITE_INTEGER "WriteInteger"          // function
#define LUA_DATA_FIELD_WRITE_NUMBER "WriteNumber"            // function
#define LUA_DATA_FIELD_WRITE_NUMBER3 "WriteNumber3"          // function
#define LUA_DATA_FIELD_WRITE_ROTATION "WriteRotation"        // function
#define LUA_DATA_FIELD_WRITE_COLOR "WriteColor"              // function
#define LUA_DATA_FIELD_WRITE_PHYSICSMODE "WritePhysicsMode"  // function
#define LUA_DATA_FIELD_WRITE_COLLISION_GROUPS "WriteCollisionGroups"  // function
#define LUA_DATA_FIELD_WRITE_INT8 "WriteInt8"                // function
#define LUA_DATA_FIELD_WRITE_INT16 "WriteInt16"              // function
#define LUA_DATA_FIELD_WRITE_INT32 "WriteInt32"              // function
#define LUA_DATA_FIELD_WRITE_UINT8 "WriteUInt8"              // function
#define LUA_DATA_FIELD_WRITE_UINT16 "WriteUInt16"            // function
#define LUA_DATA_FIELD_WRITE_UINT32 "WriteUInt32"            // function
#define LUA_DATA_FIELD_WRITE_FLOAT "WriteFloat"              // function
#define LUA_DATA_FIELD_WRITE_DOUBLE "WriteDouble"            // function

#define LUA_DATA_FIELD_READ_BYTE "ReadByte"                  // function
#define LUA_DATA_FIELD_READ_STRING "ReadString"              // function
#define LUA_DATA_FIELD_READ_INTEGER "ReadInteger"            // function
#define LUA_DATA_FIELD_READ_NUMBER "ReadNumber"              // function
#define LUA_DATA_FIELD_READ_NUMBER3 "ReadNumber3"            // function
#define LUA_DATA_FIELD_READ_ROTATION "ReadRotation"          // function
#define LUA_DATA_FIELD_READ_COLOR "ReadColor"                // function
#define LUA_DATA_FIELD_READ_PHYSICSMODE "ReadPhysicsMode"    // function
#define LUA_DATA_FIELD_READ_COLLISION_GROUPS "ReadCollisionGroups"    // function
#define LUA_DATA_FIELD_READ_INT8 "ReadInt8"                  // function
#define LUA_DATA_FIELD_READ_INT16 "ReadInt16"                // function
#define LUA_DATA_FIELD_READ_INT32 "ReadInt32"                // function
#define LUA_DATA_FIELD_READ_UINT8 "ReadUInt8"                // function
#define LUA_DATA_FIELD_READ_UINT16 "ReadUInt16"              // function
#define LUA_DATA_FIELD_READ_UINT32 "ReadUInt32"              // function
#define LUA_DATA_FIELD_READ_FLOAT "ReadFloat"                // function
#define LUA_DATA_FIELD_READ_DOUBLE "ReadDouble"              // function

#define LUA_DATA_FIELD_DECODE "Decode" // function: returns a Lua value (can be anything)

#define MAX_DATA_LEN 50000000 // 50MB
#define ALLOCATION_STEP 128 // each time an allocation is needed, it is done with this value

/// Struct used to store bytes in the Lua `Data` type
typedef struct {
    uint8_t* bytes;
    size_t length;
    size_t allocated;
    size_t cursor;
} Buffer;

typedef struct data_userdata {
    Buffer *buffer;
} data_userdata;

// --------------------------------------------------
//
// MARK: - Static functions' prototypes -
//
// --------------------------------------------------

static int _g_data_metatable_index(lua_State *L);
static int _g_data_metatable_newindex(lua_State *const L);
static int _g_data_metatable_call(lua_State *const L);

static int _g_data_from_bundle(lua_State *L);

static int _data_index(lua_State *const L);
static int _data_newindex(lua_State *const L);
static void _data_gc(void *_ud);
static int _data_decode(lua_State *const L);
static int _data_to_string(lua_State *const L);
static int _data_to_table(lua_State *const L);

static int _data_write_byte(lua_State *const L);
static int _data_write_string(lua_State *const L);
static int _data_write_integer(lua_State *const L);
static int _data_write_number(lua_State *const L);
static int _data_write_number3(lua_State *const L);
static int _data_write_rotation(lua_State *const L);
static int _data_write_color(lua_State *const L);
static int _data_write_physicsMode(lua_State *const L);
static int _data_write_collisionGroups(lua_State *const L);
static int _data_write_int8(lua_State *const L);
static int _data_write_int16(lua_State *const L);
static int _data_write_int32(lua_State *const L);
static int _data_write_uint8(lua_State *const L);
static int _data_write_uint16(lua_State *const L);
static int _data_write_uint32(lua_State *const L);
static int _data_write_float(lua_State *const L);
static int _data_write_double(lua_State *const L);

static int _data_read_byte(lua_State *const L);
static int _data_read_string(lua_State *const L);
static int _data_read_integer(lua_State *const L);
static int _data_read_number(lua_State *const L);
static int _data_read_number3(lua_State *const L);
static int _data_read_rotation(lua_State *const L);
static int _data_read_color(lua_State *const L);
static int _data_read_physicsMode(lua_State *const L);
static int _data_read_collisionGroups(lua_State *const L);
static int _data_read_int8(lua_State *const L);
static int _data_read_int16(lua_State *const L);
static int _data_read_int32(lua_State *const L);
static int _data_read_uint8(lua_State *const L);
static int _data_read_uint16(lua_State *const L);
static int _data_read_uint32(lua_State *const L);
static int _data_read_float(lua_State *const L);
static int _data_read_double(lua_State *const L);

static Buffer *buffer_new();
static Buffer *buffer_new_with_copy(const void *const data, const size_t len);
static bool buffer_reallocate(Buffer *const b, size_t newSize);
static bool buffer_push_data(Buffer *buffer, const void *data, size_t length);

// --------------------------------------------------
//
// MARK: - Exposed functions -
//
// --------------------------------------------------

bool lua_g_data_createAndPush(lua_State *const L) {
    vx_assert(L != nullptr);
    
    LUAUTILS_STACK_SIZE(L)
    
    lua_newuserdata(L, 1);
    lua_newtable(L); // metatable
    {
        lua_pushstring(L, "__index");
        lua_pushcfunction(L, _g_data_metatable_index, "");
        lua_rawset(L, -3);

        lua_pushstring(L, "__newindex");
        lua_pushcfunction(L, _g_data_metatable_newindex, "");
        lua_rawset(L, -3);

        lua_pushstring(L, "__call");
        lua_pushcfunction(L, _g_data_metatable_call, "");
        lua_rawset(L, -3);
        
        lua_pushstring(L, "__metatable");
        lua_pushboolean(L, false);
        lua_rawset(L, -3);

        lua_pushstring(L, "__type");
        lua_pushstring(L, "DataInterface");
        lua_rawset(L, -3);

        lua_pushstring(L, "__typen");
        lua_pushinteger(L, ITEM_TYPE_G_DATA);
        lua_rawset(L, -3);
    }
    lua_setmetatable(L, -2);
    
    LUAUTILS_STACK_SIZE_ASSERT(L, 1);
    return true;
}

bool lua_data_pushNewTable(lua_State *const L,
                           const void *data,
                           const size_t len) {
    vx_assert(L != nullptr);
    
    Buffer *dataBufferPtr;
    if (data != nullptr) {
        dataBufferPtr = buffer_new_with_copy(data, len);
    } else {
        dataBufferPtr = buffer_new();
    }

    if (dataBufferPtr == nullptr) {
        vxlog_error("Lua Data : failed to alloc data buffer");
        return false;
    }
    
    LUAUTILS_STACK_SIZE(L)

    data_userdata *ud = static_cast<data_userdata*>(lua_newuserdatadtor(L, sizeof(data_userdata), _data_gc));
    ud->buffer = dataBufferPtr;

    lua_newtable(L); // metatable
    {
        lua_pushstring(L, "__index");
        lua_pushcfunction(L, _data_index, "");
        lua_rawset(L, -3);
        
        lua_pushstring(L, "__newindex");
        lua_pushcfunction(L, _data_newindex, "");
        lua_rawset(L, -3);
        
        lua_pushstring(L, "__metatable");
        lua_pushboolean(L, false);
        lua_rawset(L, -3);

        lua_pushstring(L, "__type");
        lua_pushstring(L, "Data");
        lua_rawset(L, -3);

        lua_pushstring(L, "__typen");
        lua_pushinteger(L, ITEM_TYPE_DATA);
        lua_rawset(L, -3);
    }
    lua_setmetatable(L, -2);
    
    LUAUTILS_STACK_SIZE_ASSERT(L, 1);
    return true;
}

std::string lua_data_getDataAsString(lua_State * const L, const int idx, const std::string& format) {
    RETURN_VALUE_IF_NULL(L, "");
    LUAUTILS_STACK_SIZE(L)
    
    std::string out = "";
    
    if (lua_utils_getObjectType(L, idx) != ITEM_TYPE_DATA) {
        vxlog_error("Lua Data - no Data object at idx");
        LUAUTILS_STACK_SIZE_ASSERT(L, 0)
        return out;
    }

    data_userdata *ud = static_cast<data_userdata*>(lua_touserdata(L, idx));
    Buffer *storedData = ud->buffer;

    if (storedData == nullptr) {
        LUAUTILS_STACK_SIZE_ASSERT(L, 0)
        return out;
    }

    if (format == "utf-8") {
        out.assign(reinterpret_cast<char*>(storedData->bytes), storedData->length);
    } else if (format == "base64") {
        const std::string input(reinterpret_cast<char *>(storedData->bytes), storedData->length);
        out = vx::encoding::base64::encode(input);
    }
    
    LUAUTILS_STACK_SIZE_ASSERT(L, 0)
    return out;
}

const void *lua_data_getBuffer(lua_State * const L, const int idx, size_t *len) {
    RETURN_VALUE_IF_NULL(L, nullptr)
    LUAUTILS_STACK_SIZE(L)
    
    if (lua_utils_getObjectType(L, idx) != ITEM_TYPE_DATA) {
        vxlog_error("Lua Data - no Data object at idx");
        LUAUTILS_STACK_SIZE_ASSERT(L, 0)
        return nullptr;
    }

    data_userdata *ud = static_cast<data_userdata*>(lua_touserdata(L, idx));
    Buffer *storedData = ud->buffer;

    if (storedData == nullptr) {
        LUAUTILS_STACK_SIZE_ASSERT(L, 0)
        return nullptr;
    }
    
    void *out = static_cast<uint8_t*>(storedData->bytes);
    if (len != nullptr) {
        *len = storedData->length;
    }
    
    LUAUTILS_STACK_SIZE_ASSERT(L, 0)
    return out;
}

// --------------------------------------------------
//
// MARK: - static functions
//
// --------------------------------------------------

static int _g_data_metatable_index(lua_State *L) {
    vx_assert(L != nullptr);
    LUAUTILS_STACK_SIZE(L)

    const char *key = lua_tostring(L, 2);
    if (strcmp(key, LUA_G_DATA_FIELD_FROMBUNDLE) == 0) {
        lua_pushcfunction(L, _g_data_from_bundle, "");
    } else {
        lua_pushnil(L);
    }

    LUAUTILS_STACK_SIZE_ASSERT(L, 1);
    return 1;
}

static int _g_data_metatable_newindex(lua_State *const L) {
    vx_assert(L != nullptr);
    return 0;
}

static int _g_data_metatable_call(lua_State *const L) {
    vx_assert(L != nullptr);
    LUAUTILS_STACK_SIZE(L)

    const int nbArgs = lua_gettop(L);
    if (nbArgs > 3) {
        LUAUTILS_ERROR(L, "Data: function does not accept more than 2 argument");
    }
    if (nbArgs == 2 && lua_utils_isStringStrict(L, 2) == false && lua_istable(L, 2) == false) {
        LUAUTILS_ERROR(L, "Data: argument 1 should be a string or a table");
    }

    switch (nbArgs) {
        case 1:
        {
            // empty data
            lua_data_pushNewTable(L);
            break;
        }
        case 2:
        {
            if (lua_isstring(L, 2) == false && lua_istable(L, 2) == false) {
                LUAUTILS_ERROR(L, "Data() - first argument must be a string or a table")
            }
            
            if (lua_isstring(L, 2)) {
                const char *str = lua_tostring(L, 2);
                const size_t len = strlen(str);
                
                if (len > MAX_DATA_LEN) {
                    LUAUTILS_ERROR(L, "Data() - string is too long")
                }
                
                lua_data_pushNewTable(L, static_cast<const void*>(str), len);
            } else {
                uint8_t *bytes = nullptr;
                size_t size = 0;
                
                std::string err = "";
                if (lua_bin_serializer_encode_value(L, 2, &bytes, &size, err) == false) {
                    LUAUTILS_ERROR_F(L, "Data() - error: %s", err.c_str());
                }
                
                if (size > MAX_DATA_LEN) {
                    LUAUTILS_ERROR(L, "Data() - table is too big")
                }
                lua_data_pushNewTable(L, static_cast<const void*>(bytes), size);
            }
            break;
        }
        case 3:
        {
            if (lua_isstring(L, 2) == false && lua_istable(L, 2) == false) {
                LUAUTILS_ERROR(L, "Data() - first argument must be a string or a table")
            }

            if (lua_istable(L, 3) == false) {
                LUAUTILS_ERROR(L, "Data() - second argument must be a table")
            }

            std::string format = "utf-8";
            if (lua_getfield(L, 3, "format") != LUA_TNIL) {
                format = lua_tostring(L, -1);
            }
            lua_pop(L, 1);
            
            if (lua_isstring(L, 2)) {
                std::string str = lua_tostring(L, 2);
                if (format == "base64") {
                    std::string out;
                    std::string err = vx::encoding::base64::decode(str, out);
                    if (err.empty() == false) {
                        LUAUTILS_ERROR_F(L, "Data() - error: %s", err.c_str());
                    }
                    str = out;
                } else if (format == "utf-8") {
                    // do nothing
                } else {
                    LUAUTILS_ERROR(L, "Data() - invalid format")
                }
                
                const size_t len = str.size();
                if (len > MAX_DATA_LEN) {
                    LUAUTILS_ERROR(L, "Data() - string is too long")
                }
                lua_data_pushNewTable(L, static_cast<const void*>(str.c_str()), len);
            } else {
                if (format != "table") {
                    LUAUTILS_ERROR(L, "Data() - invalid format")
                }

                uint8_t *bytes = nullptr;
                size_t size = 0;
                
                std::string err = "";
                if (lua_bin_serializer_encode_table(L, 2, &bytes, &size, err) == false) {
                    LUAUTILS_ERROR_F(L, "Data() - error: %s", err.c_str());
                }
                
                if (size > MAX_DATA_LEN) {
                    LUAUTILS_ERROR(L, "Data() - table is too big")
                }
                lua_data_pushNewTable(L, static_cast<const void*>(bytes), size);
            }
            break;
        }
    default:
    {
        LUAUTILS_ERROR(L, "Data() - wrong number of arguments")
        break;
    }
    }

    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
    return 1;
}

static int _g_data_from_bundle(lua_State *L) {
    LUAUTILS_STACK_SIZE(L)

    const int argsCount = lua_gettop(L);

    if (argsCount < 1 || lua_utils_getObjectType(L, 1) != ITEM_TYPE_G_DATA) {
        LUAUTILS_ERROR(L, "Data:FromBundle - function should be called with `:`")
    }

    if (argsCount != 2) {
        LUAUTILS_ERROR(L, "Data:FromBundle - argument must be a string")
    }

    const char *filepath = lua_tostring(L, 2);

    FILE *file = vx::fs::openBundleFile(filepath);
    if (file == nullptr) {
        lua_pushnil(L);
        LUAUTILS_STACK_SIZE_ASSERT(L, 1)
        return 1;
    }
    size_t len;
    void *content = vx::fs::getFileContent(file, &len);

    lua_data_pushNewTable(L, content, len);

    free(content); // copied by lua_data_pushNewTable

    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
    return 1;
}

static int _data_index(lua_State *const L) {
    RETURN_VALUE_IF_NULL(L, 0)
    LUAUTILS_STACK_SIZE(L)
    
    LUAUTILS_ASSERT_ARGS_COUNT(L, 2)
    LUAUTILS_ASSERT_ARG_TYPE(L, 1, ITEM_TYPE_DATA)
    
    if (lua_isnumber(L, 2)) { // looking by array index
        const size_t idx = lua_tonumber(L, 2) - 1; // Lua arrays start at 1

        data_userdata *ud = static_cast<data_userdata*>(lua_touserdata(L, 1));
        Buffer *const storedData = ud->buffer;

        if (storedData != nullptr && idx <= storedData->length) {
            const uint8_t value = storedData->bytes[idx];
            lua_pushinteger(L, value);
        } else {
            lua_pushnil(L);
        }
        
    } else if (lua_utils_isStringStrict(L, 2)) { // looking by string key
        const char *key = lua_tostring(L, 2);
        
        if (strcmp(key, LUA_DATA_FIELD_LEN) == 0 || strcmp(key, LUA_DATA_FIELD_LENGTH) == 0) {
            if (strcmp(key, LUA_DATA_FIELD_LEN) == 0) {
                lua_log_warning(L, "data.Len is deprecated, use data.Length");
            }

            data_userdata *ud = static_cast<data_userdata*>(lua_touserdata(L, 1));
            Buffer *const storedData = ud->buffer;

            if (storedData != nullptr) {
                lua_pushinteger(L, static_cast<lua_Integer>(storedData->length));
            } else {
                lua_pushnil(L);
            }
            
        } else if (strcmp(key, LUA_DATA_FIELD_CURSOR) == 0) {
            data_userdata *ud = static_cast<data_userdata*>(lua_touserdata(L, 1));
            Buffer *const storedData = ud->buffer;

            if (storedData == nullptr) {
                LUAUTILS_ERROR(L, "Can't find buffer");
                return 0;
            }
            lua_pushinteger(L, static_cast<lua_Integer>(storedData->cursor));
        } else if (strcmp(key, LUA_DATA_FIELD_DECODE) == 0) {
            lua_pushcfunction(L, _data_decode, "");
        } else if (strcmp(key, LUA_DATA_FIELD_TO_STRING) == 0) {
            lua_pushcfunction(L, _data_to_string, "");
        } else if (strcmp(key, LUA_DATA_FIELD_TO_TABLE) == 0) {
            lua_pushcfunction(L, _data_to_table, "");
        } else if (strcmp(key, LUA_DATA_FIELD_WRITE_BYTE) == 0) {
            lua_pushcfunction(L, _data_write_byte, "");
        } else if (strcmp(key, LUA_DATA_FIELD_WRITE_STRING) == 0) {
            lua_pushcfunction(L, _data_write_string, "");
        } else if (strcmp(key, LUA_DATA_FIELD_WRITE_INTEGER) == 0) {
            lua_pushcfunction(L, _data_write_integer, "");
        } else if (strcmp(key, LUA_DATA_FIELD_WRITE_NUMBER) == 0) {
            lua_pushcfunction(L, _data_write_number, "");
        } else if (strcmp(key, LUA_DATA_FIELD_WRITE_NUMBER3) == 0) {
            lua_pushcfunction(L, _data_write_number3, "");
        } else if (strcmp(key, LUA_DATA_FIELD_WRITE_ROTATION) == 0) {
            lua_pushcfunction(L, _data_write_rotation, "");
        } else if (strcmp(key, LUA_DATA_FIELD_WRITE_COLOR) == 0) {
            lua_pushcfunction(L, _data_write_color, "");
        } else if (strcmp(key, LUA_DATA_FIELD_WRITE_PHYSICSMODE) == 0) {
            lua_pushcfunction(L, _data_write_physicsMode, "");
        } else if (strcmp(key, LUA_DATA_FIELD_WRITE_COLLISION_GROUPS) == 0) {
            lua_pushcfunction(L, _data_write_collisionGroups, "");
        } else if (strcmp(key, LUA_DATA_FIELD_WRITE_INT8) == 0) {
            lua_pushcfunction(L, _data_write_int8, "");
        } else if (strcmp(key, LUA_DATA_FIELD_WRITE_INT16) == 0) {
            lua_pushcfunction(L, _data_write_int16, "");
        } else if (strcmp(key, LUA_DATA_FIELD_WRITE_INT32) == 0) {
            lua_pushcfunction(L, _data_write_int32, "");
        } else if (strcmp(key, LUA_DATA_FIELD_WRITE_UINT8) == 0) {
            lua_pushcfunction(L, _data_write_uint8, "");
        } else if (strcmp(key, LUA_DATA_FIELD_WRITE_UINT16) == 0) {
            lua_pushcfunction(L, _data_write_uint16, "");
        } else if (strcmp(key, LUA_DATA_FIELD_WRITE_UINT32) == 0) {
            lua_pushcfunction(L, _data_write_uint32, "");
        } else if (strcmp(key, LUA_DATA_FIELD_WRITE_FLOAT) == 0) {
            lua_pushcfunction(L, _data_write_float, "");
        } else if (strcmp(key, LUA_DATA_FIELD_WRITE_DOUBLE) == 0) {
            lua_pushcfunction(L, _data_write_double, "");

        } else if (strcmp(key, LUA_DATA_FIELD_READ_BYTE) == 0) {
            lua_pushcfunction(L, _data_read_byte, "");
        } else if (strcmp(key, LUA_DATA_FIELD_READ_STRING) == 0) {
            lua_pushcfunction(L, _data_read_string, "");
        } else if (strcmp(key, LUA_DATA_FIELD_READ_INTEGER) == 0) {
            lua_pushcfunction(L, _data_read_integer, "");
        } else if (strcmp(key, LUA_DATA_FIELD_READ_NUMBER) == 0) {
            lua_pushcfunction(L, _data_read_number, "");
        } else if (strcmp(key, LUA_DATA_FIELD_READ_NUMBER3) == 0) {
            lua_pushcfunction(L, _data_read_number3, "");
        } else if (strcmp(key, LUA_DATA_FIELD_READ_ROTATION) == 0) {
            lua_pushcfunction(L, _data_read_rotation, "");
        } else if (strcmp(key, LUA_DATA_FIELD_READ_COLOR) == 0) {
            lua_pushcfunction(L, _data_read_color, "");
        } else if (strcmp(key, LUA_DATA_FIELD_READ_PHYSICSMODE) == 0) {
            lua_pushcfunction(L, _data_read_physicsMode, "");
        } else if (strcmp(key, LUA_DATA_FIELD_READ_COLLISION_GROUPS) == 0) {
            lua_pushcfunction(L, _data_read_collisionGroups, "");
        } else if (strcmp(key, LUA_DATA_FIELD_READ_INT8) == 0) {
            lua_pushcfunction(L, _data_read_int8, "");
        } else if (strcmp(key, LUA_DATA_FIELD_READ_INT16) == 0) {
            lua_pushcfunction(L, _data_read_int16, "");
        } else if (strcmp(key, LUA_DATA_FIELD_READ_INT32) == 0) {
            lua_pushcfunction(L, _data_read_int32, "");
        } else if (strcmp(key, LUA_DATA_FIELD_READ_UINT8) == 0) {
            lua_pushcfunction(L, _data_read_uint8, "");
        } else if (strcmp(key, LUA_DATA_FIELD_READ_UINT16) == 0) {
            lua_pushcfunction(L, _data_read_uint16, "");
        } else if (strcmp(key, LUA_DATA_FIELD_READ_UINT32) == 0) {
            lua_pushcfunction(L, _data_read_uint32, "");
        } else if (strcmp(key, LUA_DATA_FIELD_READ_FLOAT) == 0) {
            lua_pushcfunction(L, _data_read_float, "");
        } else if (strcmp(key, LUA_DATA_FIELD_READ_DOUBLE) == 0) {
            lua_pushcfunction(L, _data_read_double, "");
        } else {
            LUAUTILS_ERROR_F(L, "Data has no %s key", key)
        }
    } else {
        LUAUTILS_ERROR(L, "Data - incorrect key")
    }
    
    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
    return 1;
}

static int _data_newindex(lua_State *const L) {
    LUAUTILS_STACK_SIZE(L)
    RETURN_VALUE_IF_NULL(L, 0)

    if (lua_gettop(L) != 3) {
        LUAUTILS_ERROR(L, "Data - incorrect argument count")
    }
    
    if (lua_utils_getObjectType(L, 1) != ITEM_TYPE_DATA) {
        LUAUTILS_ERROR(L, "Data - incorrect argument type")
    }
    
    // Trying to set a system variable, it's only allowed for a few of them.
    if (lua_utils_isStringStrictStartingWithUppercase(L, 2)) {
        const char *key = lua_tostring(L, 2);
        if (strcmp(key, LUA_DATA_FIELD_CURSOR) == 0) {
            data_userdata *ud = static_cast<data_userdata*>(lua_touserdata(L, 1));
            Buffer *const storedData = ud->buffer;
            storedData->cursor = lua_tointeger(L, 3);
            return 0;
        } else if (strcmp(key, LUA_DATA_FIELD_LENGTH) == 0) {
            LUAUTILS_ERROR_F(L, "\"%s\" is read-only", key);
        } else {
            LUAUTILS_ERROR_F(L, "\"%s\" can't be set, variables starting with an uppercase letter are reserved", key);
        }
    } else {
        // user defined global variable
        lua_pushvalue(L, 2); // key
        lua_pushvalue(L, 3); // value
        lua_rawset(L, 1);
    }

    LUAUTILS_STACK_SIZE_ASSERT(L, 0)
    return 0;
}

static void _data_gc(void *_ud) {
    data_userdata *ud = static_cast<data_userdata*>(_ud);
    
    if (ud->buffer != nullptr) {
        // free bytes
        free(ud->buffer->bytes);
        ud->buffer->bytes = nullptr;
        ud->buffer->length = 0;
        // free Buffer struct
        free(ud->buffer);
        ud->buffer = nullptr;
    }
}

static int _data_decode(lua_State *const L) {
    RETURN_VALUE_IF_NULL(L, 0)
    LUAUTILS_STACK_SIZE(L)

    const int argsCount = lua_gettop(L);

    if (argsCount < 1 || lua_utils_getObjectType(L, 1) != ITEM_TYPE_DATA) {
        LUAUTILS_ERROR(L, "Data:Decode(config) - function should be called with `:`")
    }

    data_userdata *ud = static_cast<data_userdata*>(lua_touserdata(L, 1));
    Buffer *const storedData = ud->buffer;

    std::string err;
    if (lua_bin_serializer_decode_and_push_value(L, storedData->bytes, storedData->length, err) == false) {
        LUAUTILS_ERROR(L, "Data:Decode(config) - could not decode value")
    }

    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
    return 1;
}

static int _data_to_string(lua_State * const L) {
    RETURN_VALUE_IF_NULL(L, 0)
    LUAUTILS_STACK_SIZE(L)
    
    const int argsCount = lua_gettop(L);
    
    if (argsCount < 1 || lua_utils_getObjectType(L, 1) != ITEM_TYPE_DATA) {
        LUAUTILS_ERROR(L, "Data:ToString - function should be called with `:`")
    }
    
    if (argsCount > 2) {
        LUAUTILS_ERROR(L, "Data:ToString - only one argument is needed")
    }
    
    std::string format("utf-8");
    if (argsCount == 2) {
        if (lua_istable(L, 2) == false) {
            LUAUTILS_ERROR(L, "Data:ToString - first parameter must be a table")
        }
        if (lua_getfield(L, 2, "format") != LUA_TNIL) {
            format = lua_tostring(L, -1);
        }
        lua_pop(L, 1);
    }

    std::string str = lua_data_getDataAsString(L, 1, format);
    LUAUTILS_STACK_SIZE_ASSERT(L, 0)
    
    lua_pushlstring(L, str.data(), str.size());
    
    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
    return 1;
}

static int _data_to_table(lua_State * const L) {
    RETURN_VALUE_IF_NULL(L, 0)
    LUAUTILS_STACK_SIZE(L)
    
    const int argsCount = lua_gettop(L);
    
    if (argsCount < 1 || lua_utils_getObjectType(L, 1) != ITEM_TYPE_DATA) {
        LUAUTILS_ERROR(L, "Data:ToTable - function should be called with `:`")
    }
    
    if (argsCount > 1) {
        LUAUTILS_ERROR(L, "Data:ToTable - no argument needed")
    }
    
    data_userdata *ud = static_cast<data_userdata*>(lua_touserdata(L, 1));
    Buffer *const storedData = ud->buffer;

    std::string err;
    if (lua_bin_serializer_decode_and_push_table(L, storedData->bytes, storedData->length, err) == false) {
        LUAUTILS_ERROR(L, "Data:ToTable - Can't decode table")
    }

    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
    return 1;
}

static bool buffer_push_data(Buffer *buffer, const void *data, size_t length) {
    const size_t spaceLeft = buffer->allocated - buffer->cursor;
    if (spaceLeft < length) {
        if (buffer_reallocate(buffer, buffer->cursor + length) == false) {
            return false;
        }
    }
    memcpy(&(buffer->bytes[buffer->cursor]), data, length);
    buffer->cursor += length;
    // If cursor is greater than length, change the length.
    // Else, the cursor just changed data inside so the length is the same.
    if (buffer->cursor > buffer->length) {
        buffer->length = buffer->cursor;
    }
    return true;
}

template <typename T>
struct ValueInfo {
    T value;
    T *pointer;
    size_t size;
    ValueInfo() : value(T()), pointer(nullptr), size(0) {}
    ValueInfo(T value, size_t size, T *pointer = nullptr) : value(value), pointer(pointer), size(size) {}
};

template <>
struct ValueInfo<char> {
    char value;
    char *pointer;
    size_t size;
    ValueInfo() : value(0), pointer(nullptr), size(0) {}
    ValueInfo(const char* strValue, size_t size) : value(0), pointer(const_cast<char*>(strValue)), size(size) {}
};

template <typename T>
static int _data_write_generic(lua_State * const L, const char* functionName, ValueInfo<T>(*getterFunc)(lua_State*, int)) {
    RETURN_VALUE_IF_NULL(L, 0)
    LUAUTILS_STACK_SIZE(L)
    
    const int argsCount = lua_gettop(L);
    
    if (argsCount < 1 || lua_utils_getObjectType(L, 1) != ITEM_TYPE_DATA) {
        LUAUTILS_ERROR_F(L, "data:%s - function should be called with `:`", functionName)
        return 0;
    }
    
    if (argsCount != 2) {
        LUAUTILS_ERROR_F(L, "data:%s - function should be called with one parameter", functionName)
        return 0;
    }

    ValueInfo<T> valueInfo;
    try {
        valueInfo = getterFunc(L, 2);
    } catch (const std::runtime_error& e) {
        LUAUTILS_ERROR(L, e.what());
        return 0;
    }
    
    data_userdata *ud = static_cast<data_userdata*>(lua_touserdata(L, 1));
    Buffer *const storedData = ud->buffer;

    const void *valueData = static_cast<const void *>(valueInfo.pointer ? valueInfo.pointer : &valueInfo.value);
    if (buffer_push_data(storedData, valueData, valueInfo.size) == false) {
        vxlog_error("Can't allocate more memory");
        LUAUTILS_STACK_SIZE_ASSERT(L, 0)
        return 0;
    }

    LUAUTILS_STACK_SIZE_ASSERT(L, 0)
    return 0;
}

static int _data_write_byte(lua_State * const L) {
    return _data_write_generic<uint8_t>(L, "WriteByte", [](lua_State* L, int idx) -> ValueInfo<uint8_t> {
        const lua_Integer value = lua_tointeger(L, idx);
        if (value < 0 || value > UCHAR_MAX) {
            throw std::runtime_error("data:WriteByte - first argument must be a number between 0 and 255");
        }
        ValueInfo<uint8_t> valueInfo(static_cast<uint8_t>(value), sizeof(uint8_t));
        return valueInfo;
    });
}

static int _data_write_string(lua_State * const L) {
    return _data_write_generic<char>(L, "WriteString", [](lua_State* L, int idx) -> ValueInfo<char> {
        if (lua_utils_isStringStrict(L, idx) == false) {
            throw std::runtime_error("data:WriteString - first argument must be a string");
        }
        std::string str = lua_tostring(L, idx);
        ValueInfo<char> valueInfo(string_new_copy(str.c_str()), str.size());
        return valueInfo;
    });
}

static int _data_write_integer(lua_State * const L) {
    return _data_write_generic<lua_Integer>(L, "WriteInteger", [](lua_State* L, int idx) -> ValueInfo<lua_Integer> {
        if (lua_utils_isNumberStrict(L, idx) == false) {
            throw std::runtime_error("data:WriteInteger - first argument must be a number");
        }
        ValueInfo<lua_Integer> valueInfo(lua_tointeger(L, idx), sizeof(lua_Integer));
        return valueInfo;
    });
}

static int _data_write_number(lua_State * const L) {
    return _data_write_generic<lua_Number>(L, "WriteNumber", [](lua_State* L, int idx) -> ValueInfo<lua_Number> {
        if (lua_utils_isNumberStrict(L, idx) == false) {
            throw std::runtime_error("data:WriteNumber - first argument must be a number");
        }
        lua_Number number = lua_tonumber(L, idx);
        ValueInfo<lua_Number> valueInfo(number, sizeof(lua_Number));
        return valueInfo;
    });
}

static int _data_write_number3(lua_State * const L) {
    return _data_write_generic<float3>(L, "WriteNumber3", [](lua_State* L, int idx) -> ValueInfo<float3> {
        float3 value;
        if (lua_number3_or_table_or_numbers_getXYZ(L, idx, &value.x, &value.y, &value.z) == false) {
            throw std::runtime_error("data:WriteNumber3 - argument should be either a Number3 or a table of 3 numbers");
        }
        ValueInfo<float3> valueInfo(value, sizeof(float3));
        return valueInfo;
    });
}

static int _data_write_rotation(lua_State * const L) {
    return _data_write_generic<float3>(L, "WriteRotation", [](lua_State* L, int idx) -> ValueInfo<float3> {
        float3 value;
        if (lua_rotation_or_table_get_euler(L, idx, &(value.x), &(value.y), &(value.z)) == false) {
            throw std::runtime_error("data:WriteRotation - argument should be either a Rotation or a table of 3 numbers");
        }
        ValueInfo<float3> valueInfo(value, sizeof(float3));
        return valueInfo;
    });
}

static int _data_write_color(lua_State * const L) {
    return _data_write_generic<RGBAColor>(L, "WriteColor", [](lua_State* L, int idx) -> ValueInfo<RGBAColor> {
        RGBAColor value;
        if (lua_color_getRGBAL(L, -1, &value.r, &value.g, &value.b, &value.a, nullptr) == false) {
            throw std::runtime_error("data:WriteColor - argument should be a Color");
        }
        ValueInfo<RGBAColor> valueInfo(value, sizeof(RGBAColor));
        return valueInfo;
    });
}

static int _data_write_physicsMode(lua_State * const L) {
    return _data_write_generic<RigidbodyMode>(L, "WritePhysicsMode", [](lua_State* L, int idx) -> ValueInfo<RigidbodyMode> {
        if (lua_utils_getObjectType(L, idx) != ITEM_TYPE_PHYSICSMODE) {
            throw std::runtime_error("data:WritePhysicsMode - argument should be a PhysicsMode");
        }
        ValueInfo<RigidbodyMode> valueInfo(lua_physicsMode_get(L, idx), sizeof(RigidbodyMode));
        return valueInfo;
    });
}

static int _data_write_collisionGroups(lua_State * const L) {
    return _data_write_generic<uint16_t>(L, "WriteCollisionGroups", [](lua_State* L, int idx) -> ValueInfo<uint16_t> {
        if (lua_utils_getObjectType(L, idx) != ITEM_TYPE_COLLISIONGROUPS) {
            throw std::runtime_error("data:WriteCollisionGroups - argument should be a CollisionGroups");
        }
        uint16_t mask;
        lua_collision_groups_get_mask(L, idx, &mask);
        ValueInfo<uint16_t> valueInfo(mask, sizeof(uint16_t));
        return valueInfo;
    });
}

static int _data_write_int8(lua_State * const L) {
    return _data_write_generic<int8_t>(L, "WriteInt8", [](lua_State* L, int idx) -> ValueInfo<int8_t> {
        const lua_Integer value = lua_tointeger(L, idx);
        if (value < SCHAR_MIN || value > SCHAR_MAX) {
            throw std::runtime_error("data:WriteInt8 - first argument must be a number between -128 and 127");
        }
        ValueInfo<int8_t> valueInfo(static_cast<int8_t>(value), sizeof(int8_t));
        return valueInfo;
    });
}

static int _data_write_int16(lua_State * const L) {
    return _data_write_generic<int16_t>(L, "WriteInt16", [](lua_State* L, int idx) -> ValueInfo<int16_t> {
        const lua_Integer value = lua_tointeger(L, idx);
        if (value < SHRT_MIN || value > SHRT_MAX) {
            throw std::runtime_error("data:WriteInt16 - first argument must be a number between -32768 and 32767");
        }
        ValueInfo<int16_t> valueInfo(static_cast<int16_t>(value), sizeof(int16_t));
        return valueInfo;
    });
}

static int _data_write_int32(lua_State * const L) {
    return _data_write_generic<int32_t>(L, "WriteInt32", [](lua_State* L, int idx) -> ValueInfo<int32_t> {
        const lua_Integer value = lua_tointeger(L, idx);
        if (value < INT_MIN || value > INT_MAX) {
            throw std::runtime_error("data:WriteInt32 - first argument must be a number between -2147483648 and 2147483647");
        }
        ValueInfo<int32_t> valueInfo(static_cast<int32_t>(value), sizeof(int32_t));
        return valueInfo;
    });
}

static int _data_write_uint8(lua_State * const L) {
    return _data_write_generic<uint8_t>(L, "WriteUInt8", [](lua_State* L, int idx) -> ValueInfo<uint8_t> {
        const lua_Integer value = lua_tointeger(L, idx);
        if (value < 0 || value > UCHAR_MAX) {
            throw std::runtime_error("data:WriteUInt8 - first argument must be a number between 0 and 255");
        }
        ValueInfo<uint8_t> valueInfo(static_cast<uint8_t>(value), sizeof(uint8_t));
        return valueInfo;
    });
}

static int _data_write_uint16(lua_State * const L) {
    return _data_write_generic<uint16_t>(L, "WriteUInt16", [](lua_State* L, int idx) -> ValueInfo<uint16_t> {
        const lua_Integer value = lua_tointeger(L, idx);
        if (value < 0 || value > USHRT_MAX) {
            throw std::runtime_error("data:WriteUInt16 - first argument must be a number between 0 and 65535");
        }
        ValueInfo<uint16_t> valueInfo(static_cast<uint16_t>(value), sizeof(uint16_t));
        return valueInfo;
    });
}

static int _data_write_uint32(lua_State * const L) {
    return _data_write_generic<uint32_t>(L, "WriteUInt32", [](lua_State* L, int idx) -> ValueInfo<uint32_t> {
        const lua_Integer value = lua_tointeger(L, idx);
        if (value < 0 || value > UINT_MAX) {
            throw std::runtime_error("data:WriteUInt32 - first argument must be a number between 0 and 4294967295");
        }
        ValueInfo<uint32_t> valueInfo(static_cast<uint32_t>(value), sizeof(uint32_t));
        return valueInfo;
    });
}

static int _data_write_float(lua_State * const L) {
    return _data_write_generic<float>(L, "WriteFloat", [](lua_State* L, int idx) -> ValueInfo<float> {
        const float value = lua_tonumber(L, idx);
        ValueInfo<float> valueInfo(static_cast<float>(value), sizeof(float));
        return valueInfo;
    });
}

static int _data_write_double(lua_State * const L) {
    return _data_write_generic<double>(L, "WriteDouble", [](lua_State* L, int idx) -> ValueInfo<double> {
        const double value = lua_tonumber(L, idx);
        ValueInfo<double> valueInfo(value, sizeof(double));
        return valueInfo;
    });
}

template <typename T>
static int _data_read_generic(lua_State * const L, const char* funcName, T* outputValue, size_t dataSize, uint8_t maxArgsCount = 0) {
    RETURN_VALUE_IF_NULL(L, 0)

    const int argsCount = lua_gettop(L);

    // Check if the function is called with `:`
    if (argsCount < 1 || lua_utils_getObjectType(L, 1) != ITEM_TYPE_DATA) {
        LUAUTILS_ERROR_F(L, "%s - function should be called with `:`", funcName);
    }

    // Assert if there are any additional arguments
    if (maxArgsCount > maxArgsCount + 1) {
        LUAUTILS_ERROR_F(L, "%s - function takes %d parameters", funcName, maxArgsCount);
    }

    data_userdata *ud = static_cast<data_userdata*>(lua_touserdata(L, 1));
    Buffer *const storedData = ud->buffer;

    if (storedData->cursor + dataSize > storedData->length) {
        LUAUTILS_ERROR_F(L, "%s - trying to read %d bytes where there is only %d bytes to read", funcName,
                         static_cast<int>(dataSize), static_cast<int>(storedData->length - storedData->cursor));
    }

    // Copy the data to the output value
    memcpy(outputValue, &storedData->bytes[storedData->cursor], dataSize);
    storedData->cursor += dataSize;
    return 1;
}

static int _data_read_byte(lua_State * const L) {
    LUAUTILS_STACK_SIZE(L)
    uint8_t value = 0;
    _data_read_generic(L, "data:ReadByte", &value, sizeof(uint8_t));
    lua_pushinteger(L, value);
    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
    return 1;
}

static int _data_read_string(lua_State * const L) {
    RETURN_VALUE_IF_NULL(L, 0)
    LUAUTILS_STACK_SIZE(L)

    const int argsCount = lua_gettop(L);
    
    if (argsCount < 1 || lua_utils_getObjectType(L, 1) != ITEM_TYPE_DATA) {
        LUAUTILS_ERROR(L, "data:ReadString - function should be called with `:`")
    }

    if (argsCount != 2 || lua_utils_isNumberStrict(L, 2) == false) {
        LUAUTILS_ERROR(L, "data:ReadString - first argument is the number of char to read")
    }
    
    const size_t length = lua_tointeger(L, 2);

    const size_t size = length * sizeof(char);
    char *value = static_cast<char *>(malloc(size + 1));
    if (!value) {
        LUAUTILS_ERROR(L, "Can't read buffer, not enough memory");
        return 0;
    }
    memset(value, 0, size + 1);

    _data_read_generic(L, "data:ReadString", value, size);
    lua_pushstring(L, value);
    free(value);
    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
    return 1;
}

static int _data_read_integer(lua_State * const L) {
    LUAUTILS_STACK_SIZE(L)
    lua_Integer value = 0;
    _data_read_generic(L, "data:ReadInteger", &value, sizeof(lua_Integer));
    lua_pushinteger(L, value);
    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
    return 1;
}

static int _data_read_number(lua_State * const L) {
    LUAUTILS_STACK_SIZE(L)
    lua_Number value = 0;
    _data_read_generic(L, "data:ReadNumber", &value, sizeof(lua_Number));
    lua_pushnumber(L, value);
    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
    return 1;
}

static int _data_read_number3(lua_State * const L) {
    LUAUTILS_STACK_SIZE(L)
    float3 value = float3_zero;
    _data_read_generic(L, "data:ReadNumber3", &value, sizeof(float3));
    lua_number3_pushNewObject(L, value.x, value.y, value.z);
    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
    return 1;
}

static int _data_read_rotation(lua_State * const L) {
    LUAUTILS_STACK_SIZE(L)
    float3 value = float3_zero;
    _data_read_generic(L, "data:ReadRotation", &value, sizeof(float3));
    lua_rotation_push_new_euler(L, value.x, value.y, value.z);
    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
    return 1;
}

static int _data_read_color(lua_State * const L) {
    LUAUTILS_STACK_SIZE(L)
    RGBAColor value = RGBAColor_clear;
    _data_read_generic(L, "data:ReadColor", &value, sizeof(RGBAColor));
    lua_color_create_and_push_table(L, value.r, value.g, value.b, value.a, false, false);
    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
    return 1;
}

static int _data_read_physicsMode(lua_State * const L) {
    LUAUTILS_STACK_SIZE(L)
    RigidbodyMode value = RigidbodyMode_Disabled;
    _data_read_generic(L, "data:ReadPhysicsMode", &value, sizeof(RigidbodyMode));
    lua_physicsMode_pushTable(L, value);
    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
    return 1;
}

static int _data_read_collisionGroups(lua_State * const L) {
    LUAUTILS_STACK_SIZE(L)
    uint16_t mask;
    _data_read_generic(L, "data:ReadCollisionGroups", &mask, sizeof(uint16_t));
    lua_collision_groups_create_and_push(L, mask);
    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
    return 1;
}

static int _data_read_int8(lua_State * const L) {
    LUAUTILS_STACK_SIZE(L)
    int8_t value = 0;
    _data_read_generic(L, "data:ReadInt8", &value, sizeof(int8_t));
    lua_pushinteger(L, value);
    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
    return 1;
}

static int _data_read_int16(lua_State * const L) {
    LUAUTILS_STACK_SIZE(L)
    int16_t value = 0;
    _data_read_generic(L, "data:ReadInt16", &value, sizeof(int16_t));
    lua_pushinteger(L, value);
    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
    return 1;
}

static int _data_read_int32(lua_State * const L) {
    LUAUTILS_STACK_SIZE(L)
    int32_t value = 0;
    _data_read_generic(L, "data:ReadInt32", &value, sizeof(int32_t));
    lua_pushinteger(L, value);
    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
    return 1;
}

static int _data_read_uint8(lua_State * const L) {
    LUAUTILS_STACK_SIZE(L)
    uint8_t value = 0;
    _data_read_generic(L, "data:ReadUInt8", &value, sizeof(uint8_t));
    lua_pushinteger(L, value);
    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
    return 1;
}

static int _data_read_uint16(lua_State * const L) {
    LUAUTILS_STACK_SIZE(L)
    uint16_t value = 0;
    _data_read_generic(L, "data:ReadUInt16", &value, sizeof(uint16_t));
    lua_pushinteger(L, value);
    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
    return 1;
}

static int _data_read_uint32(lua_State * const L) {
    LUAUTILS_STACK_SIZE(L)
    uint32_t value = 0;
    _data_read_generic(L, "data:ReadUInt32", &value, sizeof(uint32_t));
    lua_pushinteger(L, value);
    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
    return 1;
}

static int _data_read_float(lua_State * const L) {
    LUAUTILS_STACK_SIZE(L)
    float value = 0.0f;
    _data_read_generic(L, "data:ReadFloat", &value, sizeof(float));
    lua_pushnumber(L, static_cast<double>(value));
    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
    return 1;
}

static int _data_read_double(lua_State * const L) {
    LUAUTILS_STACK_SIZE(L)
    double value = 0.0;
    _data_read_generic(L, "data:ReadDouble", &value, sizeof(double));
    lua_pushnumber(L, value);
    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
    return 1;
}

// --------------------------------------------------
//
// MARK: - local functions -
//
// --------------------------------------------------

/// Allocates a Buffer struct, copy the provided data into it
/// if allocateLenOnly is true then len bytes will be allocated
/// otherwise it will be a multiple of ALLOCATION_STEP
static Buffer* buffer_new_with_copy(const void* const data,
                                    const size_t len) {
    Buffer* buf = reinterpret_cast<Buffer*>(malloc(sizeof(Buffer)));
    if (buf == nullptr) {
        return nullptr;
    }
    buf->length = len;

    buf->allocated = len;
    buf->bytes = reinterpret_cast<uint8_t *>(malloc(sizeof(uint8_t) * buf->allocated));
    buf->cursor = 0;

    if (buf->bytes == nullptr) {
        free(buf);
        return nullptr;
    }
    if (data != nullptr) {
        memcpy(buf->bytes, data, len);
    } else {
        memset(buf->bytes, 0, len);
    }
    return buf;
}

static Buffer* buffer_new() {
    Buffer* buf = reinterpret_cast<Buffer*>(malloc(sizeof(Buffer)));
    if (buf == nullptr) {
        return nullptr;
    }
    buf->length = 0;
    buf->cursor = 0;
    buf->allocated = ALLOCATION_STEP;
    buf->bytes = reinterpret_cast<uint8_t *>(malloc(sizeof(uint8_t) * buf->allocated));
    if (buf->bytes == nullptr) {
        free(buf);
        return nullptr;
    }
    memset(buf->bytes, 0, buf->allocated);
    return buf;
}

static bool buffer_reallocate(Buffer *const b, size_t newSize) {
    if (newSize <= b->allocated) {
        vxlog_warning("Tried to reallocate a buffer with a lower size than before.");
        return false;
    }
    const size_t realSize = (newSize / ALLOCATION_STEP + 1) * ALLOCATION_STEP;
    b->allocated = realSize;
    uint8_t *oldBytes = b->bytes;
    b->bytes = static_cast<uint8_t*>(realloc(b->bytes, b->allocated));
    if (b->bytes == nullptr) {
        free(oldBytes);
        return false;
    }
    // fill the rest with 0
    memset(&(b->bytes[b->length]), 0, b->allocated - b->length);
    return true;
}

int lua_data_snprintf(lua_State * const L,
                      char *result,
                      size_t maxLen,
                      bool spacePrefix) {
    LUAUTILS_STACK_SIZE(L)
    LUAUTILS_STACK_SIZE_ASSERT(L, 0)
    return snprintf(result, maxLen, "%s[Data]", spacePrefix ? " " : "");
}
