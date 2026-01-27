//
//  lua_bin_serializer.hpp
//  Cubzh
//
//  Created by Adrien Duermael on 05/08/2023.
//

#pragma once

// C++
#include <cstddef>
#include <cstdint>
#include <string>

typedef struct lua_State lua_State;

// returns true on success, with serialized bytes and buffer size.
// error message can optionally be obtained through error parameter.
bool lua_bin_serializer_encode_table(lua_State *L, const int idx, uint8_t **bytes, size_t *size, std::string& error);

// pushed decoded value on top of the stack or raises error
bool lua_bin_serializer_decode_and_push_table(lua_State *L, const uint8_t *bytes, const size_t size, std::string& error);

bool lua_bin_serializer_encode_value(lua_State * const L,
                                     const int idx,
                                     uint8_t ** const bytes,
                                     size_t * const size,
                                     std::string& error);

bool lua_bin_serializer_decode_and_push_value(lua_State * const L,
                                              const uint8_t * const bytes,
                                              const size_t size,
                                              std::string& error);
