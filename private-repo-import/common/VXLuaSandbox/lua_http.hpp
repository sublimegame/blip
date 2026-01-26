//
//  lua_http.hpp
//  Cubzh
//
//  Created by Corentin Cailleaud on 05/03/2022.
//  Copyright � 2020 Voxowl Inc. All rights reserved.
//

#pragma once

// C++
#include <cstddef>
#include <memory>

// xptools
#include "HttpClient.hpp"

#define CUBZH_API_SERVER_URL "api.cu.bzh"
#define CUBZH_CODEGEN_SERVER_DOMAIN "codegen.cu.bzh"
#define CUBZH_API_SERVER_DEV_ADDR "192.168.1.16"

typedef struct lua_State lua_State;

/// Creates and pushes a new "HTTP" table onto the Lua stack
bool lua_g_http_create_and_push(lua_State * const L);

/// Lua print function for global HTTP
int lua_g_http_snprintf(lua_State * const L, char *result, const size_t maxLen, const bool spacePrefix);

void lua_http_request_create_and_push(lua_State *L, const int callbackIdx, const vx::HttpRequest_SharedPtr& ptr);

void lua_http_parse_headers(std::unordered_map<std::string, std::string> &headers, lua_State *L, int tableIndex);

int lua_http_httprequest_callback(lua_State * const L,
                                  vx::HttpRequest_SharedPtr const &req,
                                  vx::HttpResponse &resp);

int lua_http_kvs_get_callback(lua_State * const L, vx::HttpRequest_SharedPtr const &req);

int lua_http_kvs_set_callback(lua_State * const L, vx::HttpRequest_SharedPtr const &req);

bool lua_http_parse_string_set(lua_State * const L,
                               const int arrayIdx,
                               std::unordered_set<std::string> &result);

// Lua map -> C++ map
bool lua_http_encode_kv_map(lua_State * const L,
                            const int mapIdx,
                            std::unordered_map<std::string, std::string> &result);

// C++ map -> Lua map
bool lua_http_decode_kv_map(lua_State * const L,
                            const std::unordered_map<std::string, std::string> &keyValueMap);
