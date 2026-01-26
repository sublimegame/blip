//
//  lua_bin_serializer.cpp
//  Cubzh
//
//  Created by Adrien Duermael on 05/08/2023.
//

#include "lua_bin_serializer.hpp"

#include "lua.hpp"
#include "lua_utils.hpp"
#include "lua_constants.h"
#include "lua_number3.hpp"
#include "lua_number2.hpp"
#include "lua_color.hpp"
#include "lua_rotation.hpp"
#include "lua_physicsMode.hpp"
#include "lua_data.hpp"
#include "lua_collision_groups.hpp"

#include "config.h" // for vx_assert
#include "float3.h"

#include <memory>
#include <unordered_map>
#include <cstring>
#include <stdlib.h>

// Arbitrary value, should not exceed max vehicle message size.
#define DATA_MAX_LEN 50000000 // 50MB
#define DATA_LEN_TYPE uint32_t
#define LONG_STRING_LIMIT 1000000 // 1MB

#define SERIALIZABLE_CHUNK_NIL 0 // no data (used when skipping incorrect value with correct key)
#define SERIALIZABLE_CHUNK_NUMBER 1 // encoded as double
#define SERIALIZABLE_CHUNK_INTEGER_8 2 // encoded as int8_t (integers between INT8_MIN & INT8_MAX)  (1 byte)
#define SERIALIZABLE_CHUNK_INTEGER_16 3 // encoded as int16_t (integers between INT16_MIN & INT16_MAX) (2 bytes)
#define SERIALIZABLE_CHUNK_INTEGER_32 4 // encoded as int32_t (integers between INT32_MIN & INT32_MAX) (4 bytes)
#define SERIALIZABLE_CHUNK_INTEGER_64 5 // encoded as int64_t (integers between INT64_MIN & INT64_MAX) (8 bytes)
#define SERIALIZABLE_CHUNK_SHORT_STRING 6 // followed by size (uint8), 256 chars max
#define SERIALIZABLE_CHUNK_STRING 7 // followed by size (uint16), 65535 chars max
#define SERIALIZABLE_CHUNK_BOOLEAN_TRUE 8
#define SERIALIZABLE_CHUNK_BOOLEAN_FALSE 9
#define SERIALIZABLE_CHUNK_FLOAT3 10
#define SERIALIZABLE_CHUNK_DATA 11 // followed by size (uint16), 65535 bytes max
#define SERIALIZABLE_CHUNK_TABLE_BEGIN 12 // directly followed by another chunk index
#define SERIALIZABLE_CHUNK_TABLE_END 13 // directly followed by another chunk index
#define SERIALIZABLE_CHUNK_ROTATION 14
#define SERIALIZABLE_CHUNK_FLOAT2 15
#define SERIALIZABLE_CHUNK_COLOR 16
#define SERIALIZABLE_CHUNK_PHYSICS_MODE 17
#define SERIALIZABLE_CHUNK_LONG_STRING 18 // followed by size (uint32), custom limit 1M chars max
#define SERIALIZABLE_CHUNK_COLLLISION_GROUPS 19

//
// MARK: - Local functions' signatures -
//

static void _lbs_realloc_if_needed(const size_t& requiredSize, uint8_t **bytes, uint8_t **cursor, size_t *buf_size);
// serialize given values into bytes
static void _lbs_nil_serialize(uint8_t **bytes, uint8_t **cursor, size_t *buf_size);
static void _lbs_integer_serialize(const int64_t *value, uint8_t **bytes, uint8_t **cursor, size_t *buf_size);
static void _lbs_number_serialize(const double *value, uint8_t **bytes, uint8_t **cursor, size_t *buf_size);
static void _lbs_float3_serialize(const float3 *f3, uint8_t **bytes, uint8_t **cursor, size_t *buf_size);
static void _lbs_float2_serialize(const float *x, const float *y, uint8_t **bytes, uint8_t **cursor, size_t *buf_size);
static void _lbs_rotation_serialize(const float3 *f3, uint8_t **bytes, uint8_t **cursor, size_t *buf_size);
static void _lbs_data_serialize(const void *data, const size_t len, uint8_t **bytes, uint8_t **cursor, size_t *buf_size);
static void _lbs_string_serialize(const char *str, uint8_t **bytes, uint8_t **cursor, size_t *buf_size);
static void _lbs_boolean_serialize(const bool *value, uint8_t **bytes, uint8_t **cursor, size_t *buf_size);
static void _lbs_color_serialize(const uint8_t *r, const uint8_t *g, const uint8_t *b, const uint8_t *a, bool *light, uint8_t **bytes, uint8_t **cursor, size_t *buf_size);
static void _lbs_physics_mode_serialize(const RigidbodyMode *mode, uint8_t **bytes, uint8_t **cursor, size_t *buf_size);
static void _lbs_collision_groups_serialize(uint16_t mask, uint8_t **bytes, uint8_t **cursor, size_t *buf_size);
static void _lbs_table_begin_serialize(uint8_t **bytes, uint8_t **cursor, size_t *buf_size);
static void _lbs_table_end_serialize(uint8_t **bytes, uint8_t **cursor, size_t *buf_size);
static bool _encode_table(lua_State * const L, const int idx, uint8_t **bytes, size_t *size, std::string& error);
static bool _decode_and_push_table(lua_State * const L, const uint8_t * const bytes, const size_t size, std::string& error);

//
// MARK: - Exposed functions' implementation -
//

bool lua_bin_serializer_encode_table(lua_State *L, const int idx, uint8_t **bytes, size_t *size, std::string& error) {
    vx_assert(bytes != nullptr);
    vx_assert(*bytes == nullptr);
    vx_assert(size != nullptr);
    LUAUTILS_STACK_SIZE(L)

    const int initialTop = lua_gettop(L);

    if (lua_istable(L, idx) == false && lua_isuserdata(L, idx) == false) {
        error = "only tables can be serialized";
        return false;
    }

    double number; // 8 bytes
    int64_t integer; // 8 bytes
    const char *string = nullptr;
    bool b;
    int type;
    uint8_t ui8;
    uint8_t color_r;
    uint8_t color_g;
    uint8_t color_b;
    float x;
    float y;
    float3 f3 = float3_zero;
    void *data = nullptr;
    size_t dataLen = 0;
    int objectType; // system Object type

    // start with a buffer of buf_size
    // double size (at least) when more space is needed
    size_t buf_size = 64;
    uint8_t *buf = static_cast<uint8_t*>(malloc(buf_size));
    uint8_t *cursor = buf;

    // used to detect circular references
    const void *tablePtr;
    std::unordered_map<const void*, bool> tables;
    tables[lua_topointer(L, idx)] = true;

    int level = 0; // table level

    lua_pushvalue(L, idx);
    lua_pushnil(L); // current table at -2, nil at -1

parent_table:
    LUAUTILS_STACK_SIZE_ASSERT(L, 2 + level * 2);

    vx_assert(lua_istable(L, -2) || lua_isuserdata(L, -2));

    while(lua_next(L, -2) != 0) {
        // table at -3, key at -2, value at -1

        // ENCODE KEY
        type = lua_type(L, -2);
        switch (type) {
            case LUA_TNUMBER:
            {
                if (lua_utils_isIntegerStrict(L, -1)) {
                    integer = lua_tointeger(L, -2);
                    _lbs_integer_serialize(&integer, &buf, &cursor, &buf_size);
                } else {
                    number = lua_tonumber(L, -2);
                    _lbs_number_serialize(&number, &buf, &cursor, &buf_size);
                }
                break;
            }
            case LUA_TSTRING:
            {
                string = lua_tostring(L, -2);
                _lbs_string_serialize(string, &buf, &cursor, &buf_size);
                break;
            }
            default:
            {
                error = "can't serialize table (unsupported key type)";
                goto return_false;
            }
        }

        // vxlog_debug("KEY: %s", lua_tostring(L, -2));

        // ENCODE VALUE

        type = lua_type(L, -1);

        switch (type) {
            case LUA_TNUMBER:
            {
                if (lua_utils_isIntegerStrict(L, -1)) {
                    integer = lua_tointeger(L, -1);
                    _lbs_integer_serialize(&integer, &buf, &cursor, &buf_size);

                } else {
                    number = lua_tonumber(L, -1);
                    _lbs_number_serialize(&number, &buf, &cursor, &buf_size);
                }

                // vxlog_debug("VALUE: %d", lua_tointeger(L, -1));
                break;
            }
            case LUA_TSTRING:
            {
                string = lua_tostring(L, -1);
                _lbs_string_serialize(string, &buf, &cursor, &buf_size);

                // vxlog_debug("VALUE: %s", lua_tostring(L, -1));
                break;
            }
            case LUA_TBOOLEAN:
            {
                b = lua_toboolean(L, -1);
                _lbs_boolean_serialize(&b, &buf, &cursor, &buf_size);

                // vxlog_debug("VALUE: %s", lua_toboolean(L, -1) ? "true" : "false");
                break;
            }
            case LUA_TNIL:
            {
                _lbs_nil_serialize(&buf, &cursor, &buf_size);
                break;
            }
            case LUA_TTABLE:
            case LUA_TUSERDATA:
            {
                objectType = lua_utils_getObjectType(L, -1);

                if (objectType == ITEM_TYPE_NONE) {
                    // vxlog_debug("VALUE: TABLE");

                    // see if table is a circular reference
                    // NOTE: this could be improved
                    // any table referenced twice in serialized table is considered
                    // as circular reference though it may not be the case.
                    // Also, we could allow this assigning IDs for each table and using
                    // a dedicated chunk to refer to a table that's already been serialized.
                    // Circular references would not even be a problem.
                    tablePtr = lua_topointer(L, -1);
                    if (tables.find(tablePtr) != tables.end()) {
                        error = "can't serialize value for key: " + std::string(lua_tostring(L, -2)) + " (circular reference)";
                        goto return_false;
                    }

                    tables[tablePtr] = true;

                    _lbs_table_begin_serialize(&buf, &cursor, &buf_size);
                    ++level;

                    lua_pushnil(L); // current table at -2, nil at -1
                    goto parent_table;
                } else {
                    // vxlog_debug("VALUE: SYSTEM OBJECT");

                    switch (objectType) {
                        case ITEM_TYPE_DATA:
                        {
                            // Data
                            data = const_cast<void*>(lua_data_getBuffer(L, -1, &dataLen));
                            if (dataLen > DATA_MAX_LEN) {
                                error = "can't serialize Data for key: " + std::string(lua_tostring(L, -2)) + " (" + std::to_string(-DATA_MAX_LEN + dataLen) + " bytes too long)";
                                goto return_false;
                            }
                            _lbs_data_serialize(data, dataLen, &buf, &cursor, &buf_size);
                            break;
                        }
                        case ITEM_TYPE_NUMBER3:
                        {
                            if (lua_number3_getXYZ(L, -1, &(f3.x), &(f3.y), &(f3.z))) {
                                _lbs_float3_serialize(&f3, &buf, &cursor, &buf_size);
                            }
                            break;
                        }
                        case ITEM_TYPE_NUMBER2:
                        {
                            if (lua_number2_getXY(L, -1, &x, &y)) {
                                _lbs_float2_serialize(&x, &y, &buf, &cursor, &buf_size);
                            }
                            break;
                        }
                        case ITEM_TYPE_ROTATION:
                        {
                            if (lua_rotation_get_euler(L, -1, &(f3.x), &(f3.y), &(f3.z))) {
                                _lbs_rotation_serialize(&f3, &buf, &cursor, &buf_size);
                            }
                            break;
                        }
                        case ITEM_TYPE_COLOR:
                        {
                            if (lua_color_getRGBAL(L, -1, &color_r, &color_g, &color_b, &ui8, &b)) {
                                _lbs_color_serialize(&color_r, &color_g, &color_b, &ui8, &b, &buf, &cursor, &buf_size);
                            }
                            break;
                        }
                        case ITEM_TYPE_PHYSICSMODE:
                        {
                            RigidbodyMode mode = lua_physicsMode_get(L, -1);
                            _lbs_physics_mode_serialize(&mode, &buf, &cursor, &buf_size);
                            break;
                        }
                        case ITEM_TYPE_COLLISIONGROUPS:
                        {
                            uint16_t mask;
                            if (lua_collision_groups_get_mask(L, -1, &mask)) {
                                _lbs_collision_groups_serialize(mask, &buf, &cursor, &buf_size);
                            }
                            break;
                        }
                        default:
                        {
                            // unsupported system type
                            error = "can't serialize value for key: " + std::string(lua_tostring(L, -2)) + " (unsupported value type)";
                            goto return_false;
                        }
                    }
                }
                break;
            }
            default:
            {
                // Unsupported Lua type
                error = "can't serialize value for key: " + std::string(lua_tostring(L, -2)) + " (unsupported value type)";
                goto return_false;
            }
        }
        lua_pop(L, 1); // pop value, leave key on top
    }
    lua_pop(L, 1); // pop table

    if (level > 0) {
        --level;
        _lbs_table_end_serialize(&buf, &cursor, &buf_size);
        goto parent_table;
    }

    *size = cursor - buf;
    *bytes = buf;

    LUAUTILS_STACK_SIZE_ASSERT(L, 0)
    return true;

return_false:
    lua_pop(L, lua_gettop(L) - initialTop);
    LUAUTILS_STACK_SIZE_ASSERT(L, 0)
    free(buf);
    return false;
}

bool lua_bin_serializer_decode_and_push_table(lua_State *L, const uint8_t *bytes, const size_t size, std::string& error) {
    vx_assert(L != nullptr);
    vx_assert(bytes != nullptr);
    LUAUTILS_STACK_SIZE(L)

    const uint8_t *cursor = bytes;

    double number; // 8 bytes

    int8_t int8; // 1 byte
    int16_t int16; // 2 bytes
    int32_t int32; // 4 bytes
    int64_t int64; // 8 bytes

    size_t strLen;

    uint8_t header;

    const int initialTop = lua_gettop(L);

    // decode and push table

    lua_newtable(L); // empty table

    while (static_cast<size_t>(cursor - bytes) < size) {

        // KEY
        header = *cursor;
        cursor += sizeof(uint8_t);

        switch (header) {
            case SERIALIZABLE_CHUNK_TABLE_END:
            {
                // not a key, but a statement indicating next key is going to be for parent table.
                lua_pop(L, 1); // pops current table, moving back to parent table for next loop
                continue;
            }
            case SERIALIZABLE_CHUNK_INTEGER_8:
            {
                memcpy(&int8, cursor, sizeof(int8_t));
                cursor += sizeof(int8_t);
                lua_pushinteger(L, int8);
                break;
            }
            case SERIALIZABLE_CHUNK_INTEGER_16:
            {
                memcpy(&int16, cursor, sizeof(int16_t));
                cursor += sizeof(int16_t);
                lua_pushinteger(L, int16);
                break;
            }
            case SERIALIZABLE_CHUNK_INTEGER_32:
            {
                memcpy(&int32, cursor, sizeof(int32_t));
                cursor += sizeof(int32_t);
                lua_pushinteger(L, int32);
                break;
            }
            case SERIALIZABLE_CHUNK_INTEGER_64:
            {
                memcpy(&int64, cursor, sizeof(int64_t));
                cursor += sizeof(int64_t);
                lua_pushinteger(L, int64);
                break;
            }
            case SERIALIZABLE_CHUNK_NUMBER:
            {
                memcpy(&number, cursor, sizeof(double));
                cursor += sizeof(double);
                lua_pushnumber(L, number);
                break;
            }
            case SERIALIZABLE_CHUNK_STRING:
            {
                strLen = static_cast<size_t>(*(reinterpret_cast<const uint16_t*>(cursor)));
                cursor += sizeof(uint16_t);

                lua_pushlstring(L, reinterpret_cast<const char*>(cursor), strLen);
                cursor += strLen;
                break;
            }
            case SERIALIZABLE_CHUNK_SHORT_STRING:
            {
                strLen = static_cast<size_t>(*(reinterpret_cast<const uint8_t*>(cursor)));
                cursor += sizeof(uint8_t);

                lua_pushlstring(L, reinterpret_cast<const char*>(cursor), strLen);
                cursor += strLen;
                break;
            }
            default:
            {
                error = "decode: unsupported key type";
                goto return_false;
            }
        }

        // VALUE
        header = *cursor;
        cursor += sizeof(uint8_t);

        switch (header) {
            case SERIALIZABLE_CHUNK_INTEGER_8:
            {
                memcpy(&int8, cursor, sizeof(int8_t));
                cursor += sizeof(int8_t);
                lua_pushinteger(L, int8);
                break;
            }
            case SERIALIZABLE_CHUNK_INTEGER_16:
            {
                memcpy(&int16, cursor, sizeof(int16_t));
                cursor += sizeof(int16_t);
                lua_pushinteger(L, int16);
                break;
            }
            case SERIALIZABLE_CHUNK_INTEGER_32:
            {
                memcpy(&int32, cursor, sizeof(int32_t));
                cursor += sizeof(int32_t);
                lua_pushinteger(L, int32);
                break;
            }
            case SERIALIZABLE_CHUNK_INTEGER_64:
            {
                memcpy(&int64, cursor, sizeof(int64_t));
                cursor += sizeof(int64_t);
                lua_pushinteger(L, static_cast<lua_Integer>(int64));
                break;
            }
            case SERIALIZABLE_CHUNK_NUMBER:
            {
                memcpy(&number, cursor, sizeof(double));
                cursor += sizeof(double);
                lua_pushnumber(L, number);
                break;
            }
            case SERIALIZABLE_CHUNK_STRING:
            {
                strLen = static_cast<size_t>(*(reinterpret_cast<const uint16_t*>(cursor)));
                cursor += sizeof(uint16_t);

                lua_pushlstring(L, reinterpret_cast<const char*>(cursor), strLen);
                cursor += strLen;
                break;
            }
            case SERIALIZABLE_CHUNK_SHORT_STRING:
            {
                strLen = static_cast<size_t>(*(reinterpret_cast<const uint8_t*>(cursor)));
                cursor += sizeof(uint8_t);

                lua_pushlstring(L, reinterpret_cast<const char*>(cursor), strLen);
                cursor += strLen;
                break;
            }
            case SERIALIZABLE_CHUNK_LONG_STRING:
            {
                strLen = static_cast<size_t>(*(reinterpret_cast<const uint32_t*>(cursor)));
                cursor += sizeof(uint32_t);

                lua_pushlstring(L, reinterpret_cast<const char*>(cursor), strLen);
                cursor += strLen;
                break;
            }
            case SERIALIZABLE_CHUNK_BOOLEAN_TRUE:
            {
                lua_pushboolean(L, true);
                break;
            }
            case SERIALIZABLE_CHUNK_BOOLEAN_FALSE:
            {
                lua_pushboolean(L, false);
                break;
            }
            case SERIALIZABLE_CHUNK_FLOAT3:
            {
                float3 f3 = float3_zero;
                memcpy(&f3, cursor, sizeof(float) * 3);

                lua_number3_pushNewObject(L, f3.x, f3.y, f3.z);
                cursor += sizeof(float) * 3;
                break;
            }
            case SERIALIZABLE_CHUNK_FLOAT2:
            {
                float x = 0.0f;
                float y = 0.0f;

                memcpy(&x, cursor, sizeof(float));
                cursor += sizeof(float);

                memcpy(&y, cursor, sizeof(float));
                cursor += sizeof(float);

                lua_number2_pushNewObject(L, x, y);
                break;
            }
            case SERIALIZABLE_CHUNK_COLOR:
            {
                uint8_t r, g, b, a;
                bool light;

                memcpy(&r, cursor, sizeof(uint8_t));
                cursor += sizeof(uint8_t);

                memcpy(&g, cursor, sizeof(uint8_t));
                cursor += sizeof(uint8_t);

                memcpy(&b, cursor, sizeof(uint8_t));
                cursor += sizeof(uint8_t);

                memcpy(&a, cursor, sizeof(uint8_t));
                cursor += sizeof(uint8_t);

                memcpy(&light, cursor, sizeof(bool));
                cursor += sizeof(bool);

                lua_color_create_and_push_table(L, r, g, b, a, light, false);
                break;
            }
            case SERIALIZABLE_CHUNK_PHYSICS_MODE:
            {
                uint8_t mode;

                memcpy(&mode, cursor, sizeof(uint8_t));
                cursor += sizeof(uint8_t);

                if (lua_physicsMode_pushTable(L, mode) == false) {
                    lua_pushnil(L);
                }
                break;
            }
            case SERIALIZABLE_CHUNK_COLLLISION_GROUPS:
            {
                uint16_t mask;

                memcpy(&mask, cursor, sizeof(uint16_t));
                cursor += sizeof(uint16_t);

                lua_collision_groups_create_and_push(L, mask);
                break;
            }
            case SERIALIZABLE_CHUNK_ROTATION:
            {
                float3 f3 = float3_zero;
                memcpy(&f3, cursor, sizeof(float) * 3);

                lua_rotation_push_new_euler(L, f3.x, f3.y, f3.z);
                cursor += sizeof(float) * 3;
                break;
            }
            case SERIALIZABLE_CHUNK_DATA:
            {
                size_t len = static_cast<size_t>(*(reinterpret_cast<const DATA_LEN_TYPE*>(cursor)));
                cursor += sizeof(DATA_LEN_TYPE);
                lua_data_pushNewTable(L, reinterpret_cast<const void*>(cursor), len);
                cursor += len;
                break;

            }
            case SERIALIZABLE_CHUNK_NIL:
            {
                lua_pushnil(L);
                break;
            }
            case SERIALIZABLE_CHUNK_TABLE_BEGIN:
            {
                lua_newtable(L);
                // t : -3, k: -2, v: -1
                lua_pushvalue(L, -2);
                // t : -4, k: -3, v: -2, k: -1
                lua_pushvalue(L, -2);
                // t : -5, k: -4, v: -3, k: -2, v: -1
                lua_rawset(L, -5);
                // t : -3, k: -2, v: -1
                lua_remove(L, -2);
                // t : -2, v: -1 (v is new table, t is parent table)
                // next loop will populate new table
                continue;
            }
            default:
            {
                error = "decode: unsupported value type";
                goto return_false;
            }
        }

        // set key/value
        lua_rawset(L, -3);
    }

    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
    return true;

return_false:
    lua_pop(L, lua_gettop(L) - initialTop);
    LUAUTILS_STACK_SIZE_ASSERT(L, 0)
    return false;
}

bool lua_bin_serializer_encode_value(lua_State * const L,
                                     const int idx,
                                     uint8_t ** const bytes,
                                     size_t * const size,
                                     std::string& error) {
    vx_assert(bytes != nullptr);
    vx_assert(*bytes == nullptr);
    vx_assert(size != nullptr);
    LUAUTILS_STACK_SIZE(L)

    const int valueType = lua_type(L, idx);

    const int objectType = lua_utils_getObjectType(L, idx);
    if (valueType == LUA_TTABLE && objectType == ITEM_TYPE_NONE) {
        return _encode_table(L, idx, bytes, size, error);
    }

    // start with a buffer of buf_size
    // double size (at least) when more space is needed
    size_t buf_size = 64;
    uint8_t *buf = static_cast<uint8_t*>(malloc(buf_size));
    uint8_t *cursor = buf;

    switch (valueType) {
        case LUA_TSTRING:
        {
            const char *string = lua_tostring(L, idx);
            _lbs_string_serialize(string, &buf, &cursor, &buf_size);
            break;
        }
        case LUA_TNUMBER:
        {
            if (lua_utils_isIntegerStrict(L, idx)) {
                int64_t integer = lua_tointeger(L, idx);
                _lbs_integer_serialize(&integer, &buf, &cursor, &buf_size);

            } else {
                double number = lua_tonumber(L, idx);
                _lbs_number_serialize(&number, &buf, &cursor, &buf_size);
            }
            break;
        }
        case LUA_TBOOLEAN:
        {
            bool b = lua_toboolean(L, idx);
            _lbs_boolean_serialize(&b, &buf, &cursor, &buf_size);
            break;
        }
        case LUA_TNIL:
        {
            _lbs_nil_serialize(&buf, &cursor, &buf_size);
            break;
        }
        case LUA_TTABLE:
        case LUA_TUSERDATA:
        {
            void *data = nullptr;
            size_t dataLen = 0;
            float3 f3 = float3_zero;
            float x;
            float y;
            uint8_t color_r;
            uint8_t color_g;
            uint8_t color_b;
            uint8_t ui8;
            bool b;
            switch (objectType) {
                case ITEM_TYPE_DATA:
                {
                    // std::string dataBytes = lua_data_getDataAsString(L, -1);
                    // _lbs_data_serialize(dataBytes, &buf, &cursor, &buf_size);
                    
                    // Data
                    data = const_cast<void*>(lua_data_getBuffer(L, -1, &dataLen));
                    if (dataLen > DATA_MAX_LEN) {
                        error = "can't serialize Data for key: " + std::string(lua_tostring(L, -2)) + " (" + std::to_string(-DATA_MAX_LEN + dataLen) + " bytes too long)";
                        free(buf);
                        cursor = nullptr;
                        buf = nullptr;
                        LUAUTILS_STACK_SIZE_ASSERT(L, 0)
                        return true;
                        // goto return_false;
                    }
                    _lbs_data_serialize(data, dataLen, &buf, &cursor, &buf_size);
                    break;
                }
                case ITEM_TYPE_NUMBER3:
                {
                    if (lua_number3_getXYZ(L, -1, &(f3.x), &(f3.y), &(f3.z))) {
                        _lbs_float3_serialize(&f3, &buf, &cursor, &buf_size);
                    }
                    break;
                }
                case ITEM_TYPE_NUMBER2:
                {
                    if (lua_number2_getXY(L, -1, &x, &y)) {
                        _lbs_float2_serialize(&x, &y, &buf, &cursor, &buf_size);
                    }
                    break;
                }
                case ITEM_TYPE_ROTATION:
                {
                    if (lua_rotation_get_euler(L, -1, &(f3.x), &(f3.y), &(f3.z))) {
                        _lbs_rotation_serialize(&f3, &buf, &cursor, &buf_size);
                    }
                    break;
                }
                case ITEM_TYPE_COLOR:
                {
                    if (lua_color_getRGBAL(L, -1, &color_r, &color_g, &color_b, &ui8, &b)) {
                        _lbs_color_serialize(&color_r, &color_g, &color_b, &ui8, &b, &buf, &cursor, &buf_size);
                    }
                    break;
                }
                case ITEM_TYPE_PHYSICSMODE:
                {
                    RigidbodyMode mode = lua_physicsMode_get(L, -1);
                    _lbs_physics_mode_serialize(&mode, &buf, &cursor, &buf_size);
                    break;
                }
                case ITEM_TYPE_COLLISIONGROUPS:
                {
                    uint16_t mask;
                    if (lua_collision_groups_get_mask(L, -1, &mask)) {
                        _lbs_collision_groups_serialize(mask, &buf, &cursor, &buf_size);
                    }
                    break;
                }
                default:
                {
                    // unsupported system type
                    error = "can't serialize value for key: " + std::string(lua_tostring(L, -2)) + " (unsupported value type)";
                    free(buf);
                    cursor = nullptr;
                    buf = nullptr;
                    LUAUTILS_STACK_SIZE_ASSERT(L, 0)
                    return true;
                    // goto return_false;
                }
            }
            break;
        }
        default:
        {
            error = "only tables, strings, numbers, booleans can be serialized";
            free(buf);
            cursor = nullptr;
            buf = nullptr;
            LUAUTILS_STACK_SIZE_ASSERT(L, 0)
            return false;
        }
    }

    *size = cursor - buf;
    *bytes = buf;

    LUAUTILS_STACK_SIZE_ASSERT(L, 0)
    return true;
}

bool lua_bin_serializer_decode_and_push_value(lua_State * const L,
                                              const uint8_t * const bytes,
                                              const size_t size,
                                              std::string& error) {
    vx_assert(L != nullptr);
    vx_assert(bytes != nullptr);
    LUAUTILS_STACK_SIZE(L)

    const uint8_t *cursor = bytes;

    // read top level header
    uint8_t header = *cursor;

    if (header == SERIALIZABLE_CHUNK_TABLE_BEGIN) {
        return _decode_and_push_table(L, bytes, size, error);
    }

    cursor += sizeof(uint8_t);

    double number; // 8 bytes
    int8_t int8; // 1 byte
    int16_t int16; // 2 bytes
    int32_t int32; // 4 bytes
    int64_t int64; // 8 bytes
    size_t strLen;

    switch (header) {
        case SERIALIZABLE_CHUNK_INTEGER_8:
        {
            memcpy(&int8, cursor, sizeof(int8_t));
            // cursor += sizeof(int8_t);
            lua_pushinteger(L, int8);
            break;
        }
        case SERIALIZABLE_CHUNK_INTEGER_16:
        {
            memcpy(&int16, cursor, sizeof(int16_t));
            // cursor += sizeof(int16_t);
            lua_pushinteger(L, int16);
            break;
        }
        case SERIALIZABLE_CHUNK_INTEGER_32:
        {
            memcpy(&int32, cursor, sizeof(int32_t));
            // cursor += sizeof(int32_t);
            lua_pushinteger(L, int32);
            break;
        }
        case SERIALIZABLE_CHUNK_INTEGER_64:
        {
            memcpy(&int64, cursor, sizeof(int64_t));
            // cursor += sizeof(int64_t);
            lua_pushinteger(L, int64);
            break;
        }
        case SERIALIZABLE_CHUNK_NUMBER:
        {
            memcpy(&number, cursor, sizeof(double));
            // cursor += sizeof(double);
            lua_pushnumber(L, number);
            break;
        }
        case SERIALIZABLE_CHUNK_STRING:
        {
            strLen = static_cast<size_t>(*(reinterpret_cast<const uint16_t*>(cursor)));
            cursor += sizeof(uint16_t);

            lua_pushlstring(L, reinterpret_cast<const char*>(cursor), strLen);
            // cursor += strLen;
            break;
        }
        case SERIALIZABLE_CHUNK_SHORT_STRING:
        {
            strLen = static_cast<size_t>(*(reinterpret_cast<const uint8_t*>(cursor)));
            cursor += sizeof(uint8_t);

            lua_pushlstring(L, reinterpret_cast<const char*>(cursor), strLen);
            // cursor += strLen;
            break;
        }
        case SERIALIZABLE_CHUNK_LONG_STRING:
        {
            strLen = static_cast<size_t>(*(reinterpret_cast<const uint32_t*>(cursor)));
            cursor += sizeof(uint32_t);

            lua_pushlstring(L, reinterpret_cast<const char*>(cursor), strLen);
            // cursor += strLen;
            break;
        }
        case SERIALIZABLE_CHUNK_BOOLEAN_TRUE:
        {
            lua_pushboolean(L, true);
            break;
        }
        case SERIALIZABLE_CHUNK_BOOLEAN_FALSE:
        {
            lua_pushboolean(L, false);
            break;
        }
        case SERIALIZABLE_CHUNK_FLOAT3:
        {
            float3 f3 = float3_zero;
            memcpy(&f3, cursor, sizeof(float) * 3);

            lua_number3_pushNewObject(L, f3.x, f3.y, f3.z);
            // cursor += sizeof(float) * 3;
            break;
        }
        case SERIALIZABLE_CHUNK_FLOAT2:
        {
            float x = 0.0f;
            float y = 0.0f;

            memcpy(&x, cursor, sizeof(float));
            cursor += sizeof(float);

            memcpy(&y, cursor, sizeof(float));
            // cursor += sizeof(float);

            lua_number2_pushNewObject(L, x, y);
            break;
        }
        case SERIALIZABLE_CHUNK_COLOR:
        {
            uint8_t r, g, b, a;
            bool light;

            memcpy(&r, cursor, sizeof(uint8_t));
            cursor += sizeof(uint8_t);

            memcpy(&g, cursor, sizeof(uint8_t));
            cursor += sizeof(uint8_t);

            memcpy(&b, cursor, sizeof(uint8_t));
            cursor += sizeof(uint8_t);

            memcpy(&a, cursor, sizeof(uint8_t));
            cursor += sizeof(uint8_t);

            memcpy(&light, cursor, sizeof(bool));
            // cursor += sizeof(bool);

            lua_color_create_and_push_table(L, r, g, b, a, light, false);
            break;
        }
        case SERIALIZABLE_CHUNK_PHYSICS_MODE:
        {
            uint8_t mode;

            memcpy(&mode, cursor, sizeof(uint8_t));
            // cursor += sizeof(uint8_t);

            if (lua_physicsMode_pushTable(L, mode) == false) {
                lua_pushnil(L);
            }
            break;
        }
        case SERIALIZABLE_CHUNK_COLLLISION_GROUPS:
        {
            uint16_t mask;

            memcpy(&mask, cursor, sizeof(uint16_t));
            // cursor += sizeof(uint16_t);

            lua_collision_groups_create_and_push(L, mask);
            break;
        }
        case SERIALIZABLE_CHUNK_ROTATION:
        {
            float3 f3 = float3_zero;
            memcpy(&f3, cursor, sizeof(float) * 3);

            lua_rotation_push_new_euler(L, f3.x, f3.y, f3.z);
            // cursor += sizeof(float) * 3;
            break;
        }
        case SERIALIZABLE_CHUNK_DATA:
        {
            size_t len = static_cast<size_t>(*(reinterpret_cast<const DATA_LEN_TYPE*>(cursor)));
            cursor += sizeof(DATA_LEN_TYPE);
            lua_data_pushNewTable(L, reinterpret_cast<const void*>(cursor), len);
            // cursor += len;
            break;

        }
        case SERIALIZABLE_CHUNK_NIL:
        {
            lua_pushnil(L);
            break;
        }
        default:
        {
            error = "decode: unsupported key type";
            vxlog_debug("decode: unsupported key type: %d", header);
            goto return_false;
        }
    }

    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
    return true;

return_false:
    LUAUTILS_STACK_SIZE_ASSERT(L, 0);
    return false;
}

//
// MARK: - Local functions' implementation -
//

static void _lbs_realloc_if_needed(const size_t& requiredSize, uint8_t **bytes, uint8_t **cursor, size_t *buf_size) {
    // how many bytes were written on the buffer
    const size_t written = (*cursor) - (*bytes);

    // Count of bytes still available in the `bytes` buffer.
    const size_t available = (*buf_size) - written;

    if (available < requiredSize) {
        if (requiredSize < (*buf_size)) {
            *buf_size = (*buf_size) * 2;
        } else {
            // may be a bit more than what's needed, but it's ok,
            // less reallocs is good.
            *buf_size = (*buf_size) + requiredSize;
        }
        *bytes = static_cast<uint8_t*>(realloc(*bytes, (*buf_size)));
        *cursor = *bytes + written;
    }
}

static void _lbs_nil_serialize(uint8_t **bytes, uint8_t **cursor, size_t *buf_size) {
    static uint8_t header = SERIALIZABLE_CHUNK_NIL;
    static size_t requiredSize = sizeof(header);

    _lbs_realloc_if_needed(requiredSize, bytes, cursor, buf_size);

    // copy only the header
    memcpy(*cursor, &header, sizeof(header));
    (*cursor) += sizeof(header);
}

static void _lbs_integer_serialize(const int64_t *value, uint8_t **bytes, uint8_t **cursor, size_t *buf_size) {
    static uint8_t header_8 = SERIALIZABLE_CHUNK_INTEGER_8;
    static uint8_t header_16 = SERIALIZABLE_CHUNK_INTEGER_16;
    static uint8_t header_32 = SERIALIZABLE_CHUNK_INTEGER_32;
    static uint8_t header_64 = SERIALIZABLE_CHUNK_INTEGER_64;

    static size_t header_size = sizeof(uint8_t);

    static size_t size_8 = sizeof(int8_t);
    static size_t size_16 = sizeof(int16_t);
    static size_t size_32 = sizeof(int32_t);
    static size_t size_64 = sizeof(int64_t);

    static size_t requiredSize_8 = header_size + size_8;
    static size_t requiredSize_16 = header_size + size_16;
    static size_t requiredSize_32 = header_size + size_32;
    static size_t requiredSize_64 = header_size + size_64;

    if (*value >= INT8_MIN && *value <= INT8_MAX) {

        _lbs_realloc_if_needed(requiredSize_8, bytes, cursor, buf_size);
        memcpy(*cursor, &header_8, header_size);
        (*cursor) += header_size;
        memcpy(*cursor, value, size_8);
        (*cursor) += size_8;

    } else if (*value >= INT16_MIN && *value <= INT16_MAX) {

        _lbs_realloc_if_needed(requiredSize_16, bytes, cursor, buf_size);
        memcpy(*cursor, &header_16, header_size);
        (*cursor) += header_size;
        memcpy(*cursor, value, size_16);
        (*cursor) += size_16;

    } else if (*value >= INT32_MIN && *value <= INT32_MAX) {

        _lbs_realloc_if_needed(requiredSize_32, bytes, cursor, buf_size);
        memcpy(*cursor, &header_32, header_size);
        (*cursor) += header_size;
        memcpy(*cursor, value, size_32);
        (*cursor) += size_32;

    } else {

        _lbs_realloc_if_needed(requiredSize_64, bytes, cursor, buf_size);
        memcpy(*cursor, &header_64, header_size);
        (*cursor) += header_size;
        memcpy(*cursor, value, size_64);
        (*cursor) += size_64;

    }
}

static void _lbs_number_serialize(const double *value, uint8_t **bytes, uint8_t **cursor, size_t *buf_size) {
    static uint8_t header = SERIALIZABLE_CHUNK_NUMBER;
    static size_t requiredSize = sizeof(header) + sizeof(double);

    _lbs_realloc_if_needed(requiredSize, bytes, cursor, buf_size);

    memcpy(*cursor, &header, sizeof(header));
    (*cursor) += sizeof(header);

    memcpy(*cursor, value, sizeof(double));
    (*cursor) += sizeof(double);
}

static void _lbs_float3_serialize(const float3 *f3, uint8_t **bytes, uint8_t **cursor, size_t *buf_size) {
    static uint8_t header = SERIALIZABLE_CHUNK_FLOAT3;
    static size_t requiredSize = sizeof(header) + sizeof(float3);

    _lbs_realloc_if_needed(requiredSize, bytes, cursor, buf_size);

    memcpy(*cursor, &header, sizeof(header));
    (*cursor) += sizeof(header);

    memcpy(*cursor, f3, sizeof(float3));
    (*cursor) += sizeof(float3);
}

static void _lbs_float2_serialize(const float *x, const float *y, uint8_t **bytes, uint8_t **cursor, size_t *buf_size) {
    static uint8_t header = SERIALIZABLE_CHUNK_FLOAT2;
    static size_t requiredSize = sizeof(header) + sizeof(float) * 2;

    _lbs_realloc_if_needed(requiredSize, bytes, cursor, buf_size);

    memcpy(*cursor, &header, sizeof(header));
    (*cursor) += sizeof(header);

    memcpy(*cursor, x, sizeof(float));
    (*cursor) += sizeof(float);

    memcpy(*cursor, y, sizeof(float));
    (*cursor) += sizeof(float);
}

static void _lbs_rotation_serialize(const float3 *f3, uint8_t **bytes, uint8_t **cursor, size_t *buf_size) {
    static uint8_t header = SERIALIZABLE_CHUNK_ROTATION;
    static size_t requiredSize = sizeof(header) + sizeof(float3);

    _lbs_realloc_if_needed(requiredSize, bytes, cursor, buf_size);

    memcpy(*cursor, &header, sizeof(header));
    (*cursor) += sizeof(header);

    memcpy(*cursor, f3, sizeof(float3));
    (*cursor) += sizeof(float3);
}

static void _lbs_data_serialize(const void *data, const size_t len, uint8_t **bytes, uint8_t **cursor, size_t *buf_size) {
    static uint8_t header = SERIALIZABLE_CHUNK_DATA;
    size_t requiredSize = sizeof(uint8_t) + sizeof(DATA_LEN_TYPE) + len;

    _lbs_realloc_if_needed(requiredSize, bytes, cursor, buf_size);

    memcpy(*cursor, &header, sizeof(header));
    (*cursor) += sizeof(header);

    DATA_LEN_TYPE l = static_cast<DATA_LEN_TYPE>(len);
    memcpy(*cursor, &l, sizeof(l));
    (*cursor) += sizeof(l);

    memcpy(*cursor, data, len);
    (*cursor) += len;
}

static void _lbs_string_serialize(const char *str, uint8_t **bytes, uint8_t **cursor, size_t *buf_size) {
    static uint8_t shortStringHeader = SERIALIZABLE_CHUNK_SHORT_STRING;
    static uint8_t stringHeader = SERIALIZABLE_CHUNK_STRING;
    static uint8_t longStringHeader = SERIALIZABLE_CHUNK_STRING;

    const size_t len = strlen(str);

    if (len <= UINT8_MAX) { // does len fit in a 8 bit integer?

        // header + string size + len
        const size_t requiredSize = sizeof(shortStringHeader) + sizeof(uint8_t) + len;
        _lbs_realloc_if_needed(requiredSize, bytes, cursor, buf_size);

        memcpy(*cursor, &shortStringHeader, sizeof(shortStringHeader));
        (*cursor) += sizeof(shortStringHeader);

        // string size as uin8_t
        uint8_t l = static_cast<uint8_t>(len);
        memcpy(*cursor, &l, sizeof(uint8_t));
        (*cursor) += sizeof(uint8_t);

        memcpy(*cursor, str, len);
        (*cursor) += len;

    } else if (len <= UINT16_MAX) { // use a larger len

        // string size is stored in a uint16_t
        const size_t requiredSize = sizeof(stringHeader) + sizeof(uint16_t) + len;
        _lbs_realloc_if_needed(requiredSize, bytes, cursor, buf_size);

        memcpy(*cursor, &stringHeader, sizeof(stringHeader));
        (*cursor) += sizeof(stringHeader);

        // string size as uint16_t
        uint16_t l = static_cast<uint16_t>(len);
        memcpy(*cursor, &l, sizeof(uint16_t));
        (*cursor) += sizeof(uint16_t);

        memcpy(*cursor, str, len);
        (*cursor) += len;

    } else if (len <= LONG_STRING_LIMIT) { // use a larger len

        // long string size is stored in a uint32_t, max 1M
        const size_t requiredSize = sizeof(longStringHeader) + sizeof(uint32_t) + len;
        _lbs_realloc_if_needed(requiredSize, bytes, cursor, buf_size);

        memcpy(*cursor, &longStringHeader, sizeof(longStringHeader));
        (*cursor) += sizeof(longStringHeader);

        // long string size as uint32_t
        uint32_t l = static_cast<uint32_t>(len);
        memcpy(*cursor, &l, sizeof(uint32_t));
        (*cursor) += sizeof(uint32_t);

        memcpy(*cursor, str, len);
        (*cursor) += len;

    } else {
        // if string is too long, store nil...
        vxlog_error("string is too long");
        _lbs_nil_serialize(bytes, cursor, buf_size);
    }
}

static void _lbs_boolean_serialize(const bool *value, uint8_t **bytes, uint8_t **cursor, size_t *buf_size) {
    static uint8_t headerTrue = SERIALIZABLE_CHUNK_BOOLEAN_TRUE;
    static uint8_t headerFalse = SERIALIZABLE_CHUNK_BOOLEAN_FALSE;
    static size_t requiredSize = sizeof(uint8_t);

    _lbs_realloc_if_needed(requiredSize, bytes, cursor, buf_size);

    memcpy(*cursor, *value ? &headerTrue : &headerFalse, requiredSize);
    (*cursor) += requiredSize;
}

static void _lbs_color_serialize(const uint8_t *r,
                                 const uint8_t *g,
                                 const uint8_t *b,
                                 const uint8_t *a,
                                 bool *light,
                                 uint8_t **bytes,
                                 uint8_t **cursor,
                                 size_t *buf_size) {

    static uint8_t header = SERIALIZABLE_CHUNK_COLOR;
    static size_t requiredSize = sizeof(header) + sizeof(uint8_t) * 4 + sizeof(bool);

    _lbs_realloc_if_needed(requiredSize, bytes, cursor, buf_size);

    memcpy(*cursor, &header, sizeof(header));
    (*cursor) += sizeof(header);

    memcpy(*cursor, r, sizeof(uint8_t));
    (*cursor) += sizeof(uint8_t);

    memcpy(*cursor, g, sizeof(uint8_t));
    (*cursor) += sizeof(uint8_t);

    memcpy(*cursor, b, sizeof(uint8_t));
    (*cursor) += sizeof(uint8_t);

    memcpy(*cursor, a, sizeof(uint8_t));
    (*cursor) += sizeof(uint8_t);

    memcpy(*cursor, light, sizeof(bool));
    (*cursor) += sizeof(bool);
}

static void _lbs_physics_mode_serialize(const RigidbodyMode *mode, uint8_t **bytes, uint8_t **cursor, size_t *buf_size) {
    static uint8_t header = SERIALIZABLE_CHUNK_PHYSICS_MODE;
    static size_t requiredSize = sizeof(header) + sizeof(uint8_t);

    _lbs_realloc_if_needed(requiredSize, bytes, cursor, buf_size);

    memcpy(*cursor, &header, sizeof(header));
    (*cursor) += sizeof(header);

    uint8_t m = static_cast<uint8_t>(*mode);

    memcpy(*cursor, &m, sizeof(uint8_t));
    (*cursor) += sizeof(uint8_t);
}

static void _lbs_collision_groups_serialize(uint16_t mask, uint8_t **bytes, uint8_t **cursor, size_t *buf_size) {
    static uint8_t header = SERIALIZABLE_CHUNK_COLLLISION_GROUPS;
    static size_t requiredSize = sizeof(header) + sizeof(uint16_t);

    _lbs_realloc_if_needed(requiredSize, bytes, cursor, buf_size);

    memcpy(*cursor, &header, sizeof(header));
    (*cursor) += sizeof(header);

    memcpy(*cursor, &mask, sizeof(uint16_t));
    (*cursor) += sizeof(uint16_t);
}

static void _lbs_table_begin_serialize(uint8_t **bytes, uint8_t **cursor, size_t *buf_size) {
    static uint8_t header = SERIALIZABLE_CHUNK_TABLE_BEGIN;
    static size_t requiredSize = sizeof(uint8_t);

    _lbs_realloc_if_needed(requiredSize, bytes, cursor, buf_size);

    memcpy(*cursor, &header, requiredSize);
    (*cursor) += requiredSize;
}

static void _lbs_table_end_serialize(uint8_t **bytes, uint8_t **cursor, size_t *buf_size) {
    static uint8_t header = SERIALIZABLE_CHUNK_TABLE_END;
    static size_t requiredSize = sizeof(uint8_t);

    _lbs_realloc_if_needed(requiredSize, bytes, cursor, buf_size);

    memcpy(*cursor, &header, requiredSize);
    (*cursor) += requiredSize;
}

static bool _encode_table(lua_State * const L, const int idx, uint8_t **bytes, size_t *size, std::string& error) {
    vx_assert(bytes != nullptr);
    vx_assert(*bytes == nullptr);
    vx_assert(size != nullptr);
    LUAUTILS_STACK_SIZE(L)

    const int initialTop = lua_gettop(L);

    if (lua_istable(L, idx) == false && lua_isuserdata(L, idx) == false) {
        error = "only tables can be serialized";
        return false;
    }

    double number; // 8 bytes
    int64_t integer; // 8 bytes
    const char *string = nullptr;
    bool b;
    int keyType;
    int valueType;
    uint8_t ui8;
    uint8_t color_r;
    uint8_t color_g;
    uint8_t color_b;
    float x;
    float y;
    float3 f3 = float3_zero;
    void *data = nullptr;
    size_t dataLen = 0;
    int objectType; // system Object type

    // start with a buffer of buf_size
    // double size (at least) when more space is needed
    size_t buf_size = 64;
    uint8_t *buf = static_cast<uint8_t*>(malloc(buf_size));
    uint8_t *cursor = buf;

    // used to detect circular references
    const void *tablePtr;
    std::unordered_map<const void*, bool> tables;
    tables[lua_topointer(L, idx)] = true;

    int level = 0; // table level

    lua_pushvalue(L, idx);
    lua_pushnil(L); // current table at -2, nil at -1

    _lbs_table_begin_serialize(&buf, &cursor, &buf_size);

parent_table:
    LUAUTILS_STACK_SIZE_ASSERT(L, 2 + level * 2);

    vx_assert(lua_istable(L, -2) || lua_isuserdata(L, -2));

    while(lua_next(L, -2) != 0) {
        // table at -3, key at -2, value at -1

        keyType = lua_type(L, -2);
        valueType = lua_type(L, -1);

        // if value is nil, we don't serialize the key-value pair
        if (valueType != LUA_TNIL) {

            // ENCODE KEY
            switch (keyType) {
                case LUA_TNUMBER:
                {
                    if (lua_utils_isIntegerStrict(L, -1)) {
                        integer = lua_tointeger(L, -2);
                        _lbs_integer_serialize(&integer, &buf, &cursor, &buf_size);
                    } else {
                        number = lua_tonumber(L, -2);
                        _lbs_number_serialize(&number, &buf, &cursor, &buf_size);
                    }
                    break;
                }
                case LUA_TSTRING:
                {
                    string = lua_tostring(L, -2);
                    _lbs_string_serialize(string, &buf, &cursor, &buf_size);
                    break;
                }
                default:
                {
                    error = "can't serialize table (unsupported key type)";
                    goto return_false;
                }
            }

            // vxlog_debug("KEY: %s", lua_tostring(L, -2));

            // ENCODE VALUE

            switch (valueType) {
                case LUA_TNUMBER:
                {
                    if (lua_utils_isIntegerStrict(L, -1)) {
                        integer = lua_tointeger(L, -1);
                        _lbs_integer_serialize(&integer, &buf, &cursor, &buf_size);

                    } else {
                        number = lua_tonumber(L, -1);
                        _lbs_number_serialize(&number, &buf, &cursor, &buf_size);
                    }

                    // vxlog_debug("VALUE: %d", lua_tointeger(L, -1));
                    break;
                }
                case LUA_TSTRING:
                {
                    string = lua_tostring(L, -1);
                    _lbs_string_serialize(string, &buf, &cursor, &buf_size);

                    // vxlog_debug("VALUE: %s", lua_tostring(L, -1));
                    break;
                }
                case LUA_TBOOLEAN:
                {
                    b = lua_toboolean(L, -1);
                    _lbs_boolean_serialize(&b, &buf, &cursor, &buf_size);

                    // vxlog_debug("VALUE: %s", lua_toboolean(L, -1) ? "true" : "false");
                    break;
                }
                case LUA_TNIL:
                {

                    // _lbs_nil_serialize(&buf, &cursor, &buf_size);
                    break;
                }
                case LUA_TTABLE:
                case LUA_TUSERDATA:
                {
                    objectType = lua_utils_getObjectType(L, -1);

                    if (objectType == ITEM_TYPE_NONE) {
                        // vxlog_debug("VALUE: TABLE");

                        // see if table is a circular reference
                        // NOTE: this could be improved
                        // any table referenced twice in serialized table is considered
                        // as circular reference though it may not be the case.
                        // Also, we could allow this assigning IDs for each table and using
                        // a dedicated chunk to refer to a table that's already been serialized.
                        // Circular references would not even be a problem.
                        tablePtr = lua_topointer(L, -1);
                        if (tables.find(tablePtr) != tables.end()) {
                            error = "can't serialize value for key: " + std::string(lua_tostring(L, -2)) + " (circular reference)";
                            goto return_false;
                        }

                        tables[tablePtr] = true;

                        _lbs_table_begin_serialize(&buf, &cursor, &buf_size);
                        ++level;

                        lua_pushnil(L); // current table at -2, nil at -1
                        goto parent_table;
                    } else {
                        // vxlog_debug("VALUE: SYSTEM OBJECT");

                        switch (objectType) {
                            case ITEM_TYPE_DATA:
                            {
                                // Data
                                data = const_cast<void*>(lua_data_getBuffer(L, -1, &dataLen));
                                if (dataLen > DATA_MAX_LEN) {
                                    error = "can't serialize Data for key: " + std::string(lua_tostring(L, -2)) + " (" + std::to_string(-DATA_MAX_LEN + dataLen) + " bytes too long)";
                                    goto return_false;
                                }
                                _lbs_data_serialize(data, dataLen, &buf, &cursor, &buf_size);
                                break;
                            }
                            case ITEM_TYPE_NUMBER3:
                            {
                                if (lua_number3_getXYZ(L, -1, &(f3.x), &(f3.y), &(f3.z))) {
                                    _lbs_float3_serialize(&f3, &buf, &cursor, &buf_size);
                                }
                                break;
                            }
                            case ITEM_TYPE_NUMBER2:
                            {
                                if (lua_number2_getXY(L, -1, &x, &y)) {
                                    _lbs_float2_serialize(&x, &y, &buf, &cursor, &buf_size);
                                }
                                break;
                            }
                            case ITEM_TYPE_ROTATION:
                            {
                                if (lua_rotation_get_euler(L, -1, &(f3.x), &(f3.y), &(f3.z))) {
                                    _lbs_rotation_serialize(&f3, &buf, &cursor, &buf_size);
                                }
                                break;
                            }
                            case ITEM_TYPE_COLOR:
                            {
                                if (lua_color_getRGBAL(L, -1, &color_r, &color_g, &color_b, &ui8, &b)) {
                                    _lbs_color_serialize(&color_r, &color_g, &color_b, &ui8, &b, &buf, &cursor, &buf_size);
                                }
                                break;
                            }
                            case ITEM_TYPE_PHYSICSMODE:
                            {
                                RigidbodyMode mode = lua_physicsMode_get(L, -1);
                                _lbs_physics_mode_serialize(&mode, &buf, &cursor, &buf_size);
                                break;
                            }
                            case ITEM_TYPE_COLLISIONGROUPS:
                            {
                                uint16_t mask;
                                if (lua_collision_groups_get_mask(L, -1, &mask)) {
                                    _lbs_collision_groups_serialize(mask, &buf, &cursor, &buf_size);
                                }
                                break;
                            }
                            default:
                            {
                                // unsupported system type
                                error = "can't serialize value for key: " + std::string(lua_tostring(L, -2)) + " (unsupported value type)";
                                goto return_false;
                            }
                        }
                    }
                    break;
                }
                default:
                {
                    // Unsupported Lua type
                    error = "can't serialize value for key: " + std::string(lua_tostring(L, -2)) + " (unsupported value type)";
                    goto return_false;
                }
            }
        } // if valueType != LUA_TNIL
        lua_pop(L, 1); // pop value, leave key on top
    }
    lua_pop(L, 1); // pop table

    if (level > 0) {
        --level;
        _lbs_table_end_serialize(&buf, &cursor, &buf_size);
        goto parent_table;
    }

    _lbs_table_end_serialize(&buf, &cursor, &buf_size);

    *size = cursor - buf;
    *bytes = buf;

    LUAUTILS_STACK_SIZE_ASSERT(L, 0)
    return true;

return_false:
    lua_pop(L, lua_gettop(L) - initialTop);
    LUAUTILS_STACK_SIZE_ASSERT(L, 0)
    free(buf);
    return false;
}

static bool _decode_and_push_table(lua_State * const L, const uint8_t * const bytes, const size_t size, std::string& error) {
    vx_assert(L != nullptr);
    vx_assert(bytes != nullptr);
    LUAUTILS_STACK_SIZE(L)

    const uint8_t *cursor = bytes;

    double number; // 8 bytes

    int8_t int8; // 1 byte
    int16_t int16; // 2 bytes
    int32_t int32; // 4 bytes
    int64_t int64; // 8 bytes

    size_t strLen;

    uint8_t header;

    const int initialTop = lua_gettop(L);

    LUAUTILS_STACK_SIZE_ASSERT(L, 0)

    header = *cursor;
    cursor += sizeof(uint8_t);
    assert(header == SERIALIZABLE_CHUNK_TABLE_BEGIN);

    lua_newtable(L);

    LUAUTILS_STACK_SIZE_ASSERT(L, 1)

    int depth = 1; // 1 is top level

    while (static_cast<size_t>(cursor - bytes) < size) {

        // KEY
        header = *cursor;
        cursor += sizeof(uint8_t);

        switch (header) {
            case SERIALIZABLE_CHUNK_TABLE_END:
            {
                if (depth > 1) {
                    // not a key, but a statement indicating next key is going to be for parent table.
                    lua_pop(L, 1); // pops current table, moving back to parent table for next loop
                    --depth;
                    continue;
                } else {
                    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
                    return true;
                }
            }
            case SERIALIZABLE_CHUNK_INTEGER_8:
            {
                memcpy(&int8, cursor, sizeof(int8_t));
                cursor += sizeof(int8_t);
                lua_pushinteger(L, int8);
                break;
            }
            case SERIALIZABLE_CHUNK_INTEGER_16:
            {
                memcpy(&int16, cursor, sizeof(int16_t));
                cursor += sizeof(int16_t);
                lua_pushinteger(L, int16);
                break;
            }
            case SERIALIZABLE_CHUNK_INTEGER_32:
            {
                memcpy(&int32, cursor, sizeof(int32_t));
                cursor += sizeof(int32_t);
                lua_pushinteger(L, int32);
                break;
            }
            case SERIALIZABLE_CHUNK_INTEGER_64:
            {
                memcpy(&int64, cursor, sizeof(int64_t));
                cursor += sizeof(int64_t);
                lua_pushinteger(L, int64);
                break;
            }
            case SERIALIZABLE_CHUNK_NUMBER:
            {
                memcpy(&number, cursor, sizeof(double));
                cursor += sizeof(double);
                lua_pushnumber(L, number);
                break;
            }
            case SERIALIZABLE_CHUNK_STRING:
            {
                strLen = static_cast<size_t>(*(reinterpret_cast<const uint16_t*>(cursor)));
                cursor += sizeof(uint16_t);

                lua_pushlstring(L, reinterpret_cast<const char*>(cursor), strLen);
                cursor += strLen;
                break;
            }
            case SERIALIZABLE_CHUNK_SHORT_STRING:
            {
                strLen = static_cast<size_t>(*(reinterpret_cast<const uint8_t*>(cursor)));
                cursor += sizeof(uint8_t);

                lua_pushlstring(L, reinterpret_cast<const char*>(cursor), strLen);
                cursor += strLen;
                break;
            }
            default:
            {
                error = "decode: unsupported key type";
                goto return_false;
            }
        }

        // VALUE
        header = *cursor;
        cursor += sizeof(uint8_t);

        switch (header) {
            case SERIALIZABLE_CHUNK_INTEGER_8:
            {
                memcpy(&int8, cursor, sizeof(int8_t));
                cursor += sizeof(int8_t);
                lua_pushinteger(L, int8);
                break;
            }
            case SERIALIZABLE_CHUNK_INTEGER_16:
            {
                memcpy(&int16, cursor, sizeof(int16_t));
                cursor += sizeof(int16_t);
                lua_pushinteger(L, int16);
                break;
            }
            case SERIALIZABLE_CHUNK_INTEGER_32:
            {
                memcpy(&int32, cursor, sizeof(int32_t));
                cursor += sizeof(int32_t);
                lua_pushinteger(L, int32);
                break;
            }
            case SERIALIZABLE_CHUNK_INTEGER_64:
            {
                memcpy(&int64, cursor, sizeof(int64_t));
                cursor += sizeof(int64_t);
                lua_pushinteger(L, int64);
                break;
            }
            case SERIALIZABLE_CHUNK_NUMBER:
            {
                memcpy(&number, cursor, sizeof(double));
                cursor += sizeof(double);
                lua_pushnumber(L, number);
                break;
            }
            case SERIALIZABLE_CHUNK_STRING:
            {
                strLen = static_cast<size_t>(*(reinterpret_cast<const uint16_t*>(cursor)));
                cursor += sizeof(uint16_t);

                lua_pushlstring(L, reinterpret_cast<const char*>(cursor), strLen);
                cursor += strLen;
                break;
            }
            case SERIALIZABLE_CHUNK_SHORT_STRING:
            {
                strLen = static_cast<size_t>(*(reinterpret_cast<const uint8_t*>(cursor)));
                cursor += sizeof(uint8_t);

                lua_pushlstring(L, reinterpret_cast<const char*>(cursor), strLen);
                cursor += strLen;
                break;
            }
            case SERIALIZABLE_CHUNK_LONG_STRING:
            {
                strLen = static_cast<size_t>(*(reinterpret_cast<const uint32_t*>(cursor)));
                cursor += sizeof(uint32_t);

                lua_pushlstring(L, reinterpret_cast<const char*>(cursor), strLen);
                cursor += strLen;
                break;
            }
            case SERIALIZABLE_CHUNK_BOOLEAN_TRUE:
            {
                lua_pushboolean(L, true);
                break;
            }
            case SERIALIZABLE_CHUNK_BOOLEAN_FALSE:
            {
                lua_pushboolean(L, false);
                break;
            }
            case SERIALIZABLE_CHUNK_FLOAT3:
            {
                float3 f3 = float3_zero;
                memcpy(&f3, cursor, sizeof(float) * 3);

                lua_number3_pushNewObject(L, f3.x, f3.y, f3.z);
                cursor += sizeof(float) * 3;
                break;
            }
            case SERIALIZABLE_CHUNK_FLOAT2:
            {
                float x = 0.0f;
                float y = 0.0f;

                memcpy(&x, cursor, sizeof(float));
                cursor += sizeof(float);

                memcpy(&y, cursor, sizeof(float));
                cursor += sizeof(float);

                lua_number2_pushNewObject(L, x, y);
                break;
            }
            case SERIALIZABLE_CHUNK_COLOR:
            {
                uint8_t r, g, b, a;
                bool light;

                memcpy(&r, cursor, sizeof(uint8_t));
                cursor += sizeof(uint8_t);

                memcpy(&g, cursor, sizeof(uint8_t));
                cursor += sizeof(uint8_t);

                memcpy(&b, cursor, sizeof(uint8_t));
                cursor += sizeof(uint8_t);

                memcpy(&a, cursor, sizeof(uint8_t));
                cursor += sizeof(uint8_t);

                memcpy(&light, cursor, sizeof(bool));
                cursor += sizeof(bool);

                lua_color_create_and_push_table(L, r, g, b, a, light, false);
                break;
            }
            case SERIALIZABLE_CHUNK_PHYSICS_MODE:
            {
                uint8_t mode;

                memcpy(&mode, cursor, sizeof(uint8_t));
                cursor += sizeof(uint8_t);

                if (lua_physicsMode_pushTable(L, mode) == false) {
                    lua_pushnil(L);
                }
                break;
            }
            case SERIALIZABLE_CHUNK_COLLLISION_GROUPS:
            {
                uint16_t mask;

                memcpy(&mask, cursor, sizeof(uint16_t));
                cursor += sizeof(uint16_t);

                lua_collision_groups_create_and_push(L, mask);
                break;
            }
            case SERIALIZABLE_CHUNK_ROTATION:
            {
                float3 f3 = float3_zero;
                memcpy(&f3, cursor, sizeof(float) * 3);

                lua_rotation_push_new_euler(L, f3.x, f3.y, f3.z);
                cursor += sizeof(float) * 3;
                break;
            }
            case SERIALIZABLE_CHUNK_DATA:
            {
                size_t len = static_cast<size_t>(*(reinterpret_cast<const DATA_LEN_TYPE*>(cursor)));
                cursor += sizeof(DATA_LEN_TYPE);
                lua_data_pushNewTable(L, reinterpret_cast<const void*>(cursor), len);
                cursor += len;
                break;

            }
            case SERIALIZABLE_CHUNK_NIL:
            {
                lua_pushnil(L);
                break;
            }
            case SERIALIZABLE_CHUNK_TABLE_BEGIN:
            {
                lua_newtable(L);
                // t : -3, k: -2, v: -1
                lua_pushvalue(L, -2);
                // t : -4, k: -3, v: -2, k: -1
                lua_pushvalue(L, -2);
                // t : -5, k: -4, v: -3, k: -2, v: -1
                lua_rawset(L, -5);
                // t : -3, k: -2, v: -1
                lua_remove(L, -2);
                // t : -2, v: -1 (v is new table, t is parent table)

                depth += 1;

                // next loop will populate new table
                continue;
            }
            default:
            {
                error = "decode: unsupported value type";
                goto return_false;
            }
        }

        // set key/value
        lua_rawset(L, -3);
    }

    // this should never be reached
    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
    return true;

return_false:
    lua_pop(L, lua_gettop(L) - initialTop);
    LUAUTILS_STACK_SIZE_ASSERT(L, 0)
    return false;
}
