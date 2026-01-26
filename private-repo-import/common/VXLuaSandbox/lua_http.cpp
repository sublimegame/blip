//
//  lua_http.cpp
//  Cubzh
//
//  Created by Corentin Cailleaud on 05/03/2022.
//  Copyright 2022 Voxowl Inc. All rights reserved.
//

#include "lua_http.hpp"

// C++
#include <regex>

// Lua
#include "lua.hpp"
#include "lua_data.hpp"
#include "lua_json.hpp"
#include "lua_utils.hpp"
#include "lua_bin_serializer.hpp"

// xptools
#include "OperationQueue.hpp"
#include "encoding_base64.hpp"

// engine
#include "VXGame.hpp"
#include "game.h"

// Lua sandbox
#include "lua_constants.h"
#include "lua_logs.hpp"

#define LUA_G_HTTP_FIELD_GET "Get"                                   // function
#define LUA_G_HTTP_FIELD_GET_STREAM "GetStream"                      // function
#define LUA_G_HTTP_FIELD_POST "Post"                                 // function
#define LUA_G_HTTP_FIELD_PATCH "Patch"                               // function
#define LUA_G_HTTP_FIELD_DELETE "Delete"                             // function
#define LUA_G_HTTP_PRIVATE_FIELD_PENDING_REQUESTS "pending_requests" // table
#define LUA_G_HTTP_STATUS_PROCESSING "StatusProcessing"              // integer
#define LUA_G_HTTP_STATUS_CANCELLED "StatusCancelled"                // integer
#define LUA_G_HTTP_STATUS_DONE "StatusDone"                          // integer

#define LUA_HTTP_REQUEST_STATUS "Status"                             // function
#define LUA_HTTP_REQUEST_CANCEL "Cancel"                             // function
#define LUA_HTTP_REQUEST_CALLBACK "callback"                         // functionmacro

typedef struct httpRequest_userdata {
    vx::HttpRequest_SharedPtr* req;
} httpRequest_userdata;

static int _g_http_newindex(lua_State * const L);
static int _g_http_get(lua_State * const L);
static int _g_http_get_stream(lua_State * const L);
// TODO: gaetan: the following functions' code can be factorized
static int _g_http_post(lua_State * const L);
static int _g_http_patch(lua_State * const L);
static int _g_http_delete(lua_State * const L);

static int _httprequest_index(lua_State * const L);
static void _httprequest_gc(void *_ud);
static int _httprequest_cancel(lua_State * const L);

static void _httpresponse_pushNewTable(lua_State *L,
                                       const int statusCode,
                                       const std::string &body,
                                       const bool endOfStream,
                                       const std::unordered_map<std::string, std::string> &headers);
static int _httpresponse_headers_index(lua_State *L);
static int _httpresponse_newindex(lua_State *L);
static bool _urlHasPrivilegedPathPrefix(const std::string &url);

static bool _parseRequestConfig(lua_State *L,
                                const int idx,
                                std::string &url,
                                std::string &body,
                                std::unordered_map<std::string, std::string> &headers,
                                vx::HttpRequestOpts &opts,
                                std::string &err);

// --------------------------------------------------
//
// MARK: - Exposed functions -
//
// --------------------------------------------------

bool lua_g_http_create_and_push(lua_State * const L) {
    vx_assert(L != nullptr);
    LUAUTILS_STACK_SIZE(L)

    lua_newuserdata(L, 1);
    lua_newtable(L);
    {
        lua_pushstring(L, "__index");
        lua_newtable(L); // table for read-only keys
        {
            lua_pushstring(L, LUA_G_HTTP_FIELD_GET);
            lua_pushcfunction(L, _g_http_get, "");
            lua_rawset(L, -3);

            lua_pushstring(L, LUA_G_HTTP_FIELD_GET_STREAM);
            lua_pushcfunction(L, _g_http_get_stream, "");
            lua_rawset(L, -3);

            lua_pushstring(L, LUA_G_HTTP_FIELD_POST);
            lua_pushcfunction(L, _g_http_post, "");
            lua_rawset(L, -3);

            lua_pushstring(L, LUA_G_HTTP_FIELD_PATCH);
            lua_pushcfunction(L, _g_http_patch, "");
            lua_rawset(L, -3);

            lua_pushstring(L, LUA_G_HTTP_FIELD_DELETE);
            lua_pushcfunction(L, _g_http_delete, "");
            lua_rawset(L, -3);

            lua_pushstring(L, LUA_G_HTTP_STATUS_PROCESSING);
            lua_pushnumber(L, vx::HttpRequest::Status::PROCESSING);
            lua_rawset(L, -3);

            lua_pushstring(L, LUA_G_HTTP_STATUS_CANCELLED);
            lua_pushnumber(L, vx::HttpRequest::Status::CANCELLED);
            lua_rawset(L, -3);

            lua_pushstring(L, LUA_G_HTTP_STATUS_DONE);
            lua_pushnumber(L, vx::HttpRequest::Status::DONE);
            lua_rawset(L, -3);
        }
        lua_rawset(L, -3);

        lua_pushstring(L, "__newindex");
        lua_pushcfunction(L, _g_http_newindex, "");
        lua_rawset(L, -3);

        lua_pushstring(L, "__metatable");
        lua_pushboolean(L, false);
        lua_rawset(L, -3);

        lua_pushstring(L, LUA_G_HTTP_PRIVATE_FIELD_PENDING_REQUESTS);
        lua_newtable(L);
        lua_rawset(L, -3);

        lua_pushstring(L, "__type");
        lua_pushstring(L, "HTTP");
        lua_rawset(L, -3);

        lua_pushstring(L, "__typen");
        lua_pushinteger(L, ITEM_TYPE_G_HTTP);
        lua_rawset(L, -3);
    }
    lua_setmetatable(L, -2);

    LUAUTILS_STACK_SIZE_ASSERT(L, 1);
    return true;
}

// --------------------------------------------------
//
// MARK: - static functions
//
// --------------------------------------------------

static int _g_http_newindex(lua_State * const L) {
    LUAUTILS_STACK_SIZE(L)
    LUAUTILS_ERROR(L, "HTTP is read-only"); // returns
    LUAUTILS_STACK_SIZE_ASSERT(L, 0)
    return 0;
}

static int _httprequest_cancel(lua_State * const L) {
    LUAUTILS_STACK_SIZE(L)

    if (lua_gettop(L) < 1) {
        LUAUTILS_ERROR(L, "HttpRequest:Cancel() should be called with :");
    } else if (lua_gettop(L) != 1) {
        LUAUTILS_ERROR(L, "HttpRequest:Cancel() doesn't accept arguments");
    }

    httpRequest_userdata *ud = static_cast<httpRequest_userdata *>(lua_touserdata(L, 1));
    vx_assert(ud != nullptr);

    vx::HttpRequest_SharedPtr* req = ud->req;
    vx_assert(req != nullptr);

    (*req)->cancel();

    // Set function callback to nil.
    // A reference to the HttpRequest itself may be kept.
    // Since HttpRequest can't be reset, we know the callback
    // can't be triggered once again. Setting it to nil
    // allows to liberate potentially captured up values.
    lua_getmetatable(L, 1);
    lua_pushstring(L, LUA_HTTP_REQUEST_CALLBACK);
    lua_pushnil(L);
    lua_rawset(L, -3);

    lua_pushstring(L, LUA_HTTP_REQUEST_STATUS);
    lua_pushinteger(L, vx::HttpRequest::Status::CANCELLED);
    lua_rawset(L, -3);

    lua_pop(L, 1); // pop metatable

    // remove from pending requests
    int type = LUA_GET_GLOBAL_AND_RETURN_TYPE(L, LUA_G_HTTP);
    vx_assert(lua_utils_getObjectType(L, -1) == ITEM_TYPE_G_HTTP);

    type = LUA_GET_METAFIELD_AND_RETURN_TYPE(L, -1, LUA_G_HTTP_PRIVATE_FIELD_PENDING_REQUESTS);
    vx_assert(type == LUA_TTABLE);

    lua_pushlightuserdata(L, req->get());
    lua_pushnil(L);
    lua_rawset(L, -3);
    lua_pop(L, 2);

    LUAUTILS_STACK_SIZE_ASSERT(L, 0)
    return 0;
}

void lua_http_request_create_and_push(lua_State *L, const int callbackIdx, const vx::HttpRequest_SharedPtr& ptr) {
    vx_assert(L != nullptr);
    LUAUTILS_STACK_SIZE(L)

    vx::Game *g = nullptr;
    if (lua_utils_getGameCppWrapperPtr(L, &g) == false) {
        LUAUTILS_INTERNAL_ERROR(L);
    }

    vx::HttpRequest_SharedPtr* pointer = new vx::HttpRequest_SharedPtr(ptr);

    httpRequest_userdata *ud = static_cast<httpRequest_userdata*>(lua_newuserdatadtor(L, sizeof(httpRequest_userdata), _httprequest_gc));
    ud->req = pointer;

    lua_newtable(L); // metatable
    {
        lua_pushstring(L, "__index");
        lua_pushcfunction(L, _httprequest_index, "");
        lua_rawset(L, -3);

        lua_pushstring(L, "__metatable");
        lua_pushboolean(L, false);
        lua_rawset(L, -3);

        if (callbackIdx != 0) {
            lua_pushstring(L, LUA_HTTP_REQUEST_CALLBACK);
            if (callbackIdx < 0) {
                // key string: -1
                // metatable: -2
                // userdata: -3
                lua_pushvalue(L, callbackIdx - 3);
            } else {
                lua_pushvalue(L, callbackIdx);
            }
            lua_rawset(L, -3);
        }

        lua_pushstring(L, LUA_HTTP_REQUEST_STATUS);
        lua_pushinteger(L, vx::HttpRequest::Status::PROCESSING);
        lua_rawset(L, -3);

        lua_pushstring(L, "__type");
        lua_pushstring(L, "HTTPRequest");
        lua_rawset(L, -3);

        lua_pushstring(L, "__typen");
        lua_pushinteger(L, ITEM_TYPE_HTTP_REQUEST);
        lua_rawset(L, -3);
    }
    lua_setmetatable(L, -2);

    // store the HTTP Request in the pending requests table
    int type = LUA_GET_GLOBAL_AND_RETURN_TYPE(L, LUA_G_HTTP);
    vx_assert(type == LUA_TUSERDATA);

    type = LUA_GET_METAFIELD_AND_RETURN_TYPE(L, -1, LUA_G_HTTP_PRIVATE_FIELD_PENDING_REQUESTS);
    vx_assert(type == LUA_TTABLE);

    lua_pushlightuserdata(L, pointer->get());
    lua_pushvalue(L, -4);
    lua_rawset(L, -3);

    lua_pop(L, 2);

    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
}

static int _httprequest_index(lua_State * const L) {
    vx_assert(L != nullptr);
    LUAUTILS_STACK_SIZE(L)

    vx::Game *cppGame = nullptr;
    if (lua_utils_getGameCppWrapperPtr(L, &cppGame) == false || cppGame == nullptr) {
        LUAUTILS_STACK_SIZE_ASSERT(L, 0);
        LUAUTILS_INTERNAL_ERROR(L);
        // return 0;
    }

    const char *key = lua_tostring(L, 2);
    if (strcmp(key, LUA_HTTP_REQUEST_CANCEL) == 0) {
        lua_pushcfunction(L, _httprequest_cancel, "");
    } else if (strcmp(key, LUA_HTTP_REQUEST_STATUS) == 0) {
        LUA_GET_METAFIELD_AND_RETURN_TYPE_PUSHING_NIL_IF_NOT_FOUND(L, 1, LUA_HTTP_REQUEST_STATUS);
    } else {
        lua_pushnil(L);
    }

    LUAUTILS_STACK_SIZE_ASSERT(L, 1);
    return 1;
}

static void _httprequest_gc(void *_ud) {
    httpRequest_userdata *ud = static_cast<httpRequest_userdata*>(_ud);
    vx::HttpRequest_SharedPtr *requestToDelete = ud->req;
    vx_assert(requestToDelete != nullptr);
    delete requestToDelete;
}

static void _httpresponse_pushNewTable(lua_State *L,
                                       const int statusCode,
                                       const std::string &body,
                                       const bool endOfStream,
                                       const std::unordered_map<std::string, std::string> &headers) {

    vx_assert(L != nullptr);
    LUAUTILS_STACK_SIZE(L)

    lua_newuserdata(L, 1); // Lua Http Response
    lua_newtable(L);
    {
        lua_pushstring(L, "__index");
        lua_newtable(L); // table for read-only keys
        {
            lua_pushstring(L, "StatusCode");
            lua_pushinteger(L, statusCode);
            lua_rawset(L, -3);

            lua_pushstring(L, "Body");
            lua_data_pushNewTable(L, static_cast<const void *>(body.c_str()), body.length());
            lua_rawset(L, -3);

            lua_pushstring(L, "EndOfStream");
            lua_pushboolean(L, endOfStream);
            lua_rawset(L, -3);

            lua_pushstring(L, "Headers");
            lua_newtable(L);
            lua_newtable(L);
            {
                lua_pushstring(L, "values");
                lua_newtable(L);
                for (auto i = headers.begin(); i != headers.end(); i++) {
                    lua_pushstring(L, i->first.c_str());
                    lua_pushstring(L, i->second.c_str());
                    lua_rawset(L, -3);
                }
                lua_rawset(L, -3);

                // if the key is not lowercase-only then __index is used
                lua_pushstring(L, "__index");
                lua_pushcfunction(L, _httpresponse_headers_index, "");
                lua_rawset(L, -3);

                lua_pushstring(L, "__newindex");
                lua_pushcfunction(L, _httpresponse_newindex, "");
                lua_rawset(L, -3);

                lua_pushstring(L, "__metatable");
                lua_pushboolean(L, false);
                lua_rawset(L, -3);
            }

            lua_setmetatable(L, -2);

            // -1: metatable
            // -2: "Headers"
            // -3: __index table

            lua_rawset(L, -3);
        }
        lua_rawset(L, -3);

        lua_pushstring(L, "__metatable");
        lua_pushboolean(L, false);
        lua_rawset(L, -3);

        lua_pushstring(L, "__newindex");
        lua_pushcfunction(L, _httpresponse_newindex, "");
        lua_rawset(L, -3);

        lua_pushstring(L, "__type");
        lua_pushstring(L, "HTTPResponse");
        lua_rawset(L, -3);

        lua_pushstring(L, "__typen");
        lua_pushinteger(L, ITEM_TYPE_HTTP_RESPONSE);
        lua_rawset(L, -3);
    }
    lua_setmetatable(L, -2);

    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
}

/// converts any string key into a lowercase-only one
static int _httpresponse_headers_index(lua_State* L) {
    LUAUTILS_STACK_SIZE(L)

    if (lua_isstring(L, 2) == false) {
        LUAUTILS_ERROR(L, "HTTP - response header keys can only be strings");
    }

    std::string key(lua_tostring(L, 2));

    // transform every character to its lowercase equivalent
    for (size_t i = 0; i < key.length(); i += 1) {
        key.replace(i, 1, 1, std::tolower(key.at(i)));
    }

    if (LUA_GET_METAFIELD_AND_RETURN_TYPE(L, 1, "values") != LUA_TTABLE) {
        LUAUTILS_INTERNAL_ERROR(L)
    }
    lua_pushstring(L, key.c_str());

    // -1: key in correct format
    // -2: "values" table
    lua_rawget(L, -2);

    lua_remove(L, -2); // remove "values" table

    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
    return 1;
}

static int _httpresponse_newindex(lua_State *L) {
    LUAUTILS_STACK_SIZE(L)
    LUAUTILS_ERROR(L, "HTTPResponse is read-only"); // returns
    LUAUTILS_STACK_SIZE_ASSERT(L, 0)
    return 0;
}

static bool _urlRequiresPrivilegedCubzhAPIAccess(const vx::URL &url) {
    const bool targetsCubzhAPI = url.host() == "api.cu.bzh";
    const bool hasPathPrefix = vx::str::hasPrefix(url.path(), "/privileged");
    return targetsCubzhAPI && hasPathPrefix;
}

int lua_http_httprequest_callback(lua_State * const L,
                                  vx::HttpRequest_SharedPtr const &req,
                                  vx::HttpResponse &resp) {
    LUAUTILS_STACK_SIZE(L)

    const bool responseDownloadComplete = resp.getDownloadComplete();

    const int statusCode = resp.getStatusCode();

    std::string rawBody;
    if (resp.getSuccess()) {
        if (req->getOpts().getStreamResponse()) {
            resp.readBytes(rawBody);
        } else {
            const bool ok = resp.readAllBytes(rawBody);
            if (ok == false) {
                // TODO: handle error
                vxlog_error("[lua_http_httprequest_callback] failed to read response body");
            }
        }
    }

    lua_getglobal(L, LUA_G_HTTP);
    vx_assert(lua_utils_getObjectType(L, -1) == ITEM_TYPE_G_HTTP);

    int type = LUA_GET_METAFIELD_AND_RETURN_TYPE(L, -1, LUA_G_HTTP_PRIVATE_FIELD_PENDING_REQUESTS);
    vx_assert(type == LUA_TTABLE);

    LUAUTILS_STACK_SIZE_ASSERT(L, 2)

    // get the HTTP Request stored in pending HTTP Requests with its address
    lua_pushlightuserdata(L, req.get());
    lua_rawget(L, -2);

    LUAUTILS_STACK_SIZE_ASSERT(L, 3)

    // Make sure the HTTPRequest was found
    if (lua_isnil(L, -1)) {
        vxlog_debug("Lua HTTPRequest not found");
        lua_pop(L, 3);
        LUAUTILS_STACK_SIZE_ASSERT(L, 0)
        return 0;
    }

    // lua HTTPRequest is on top of the stack
    lua_getmetatable(L, -1);

    LUAUTILS_STACK_SIZE_ASSERT(L, 4)

    // -1 metatable
    // -2 lua HTTPRequest
    lua_pushstring(L, LUA_HTTP_REQUEST_STATUS);
    lua_pushinteger(L, vx::HttpRequest::Status::DONE);
    lua_rawset(L, -3);

    // -1 metatable
    // -2 lua HTTPRequest
    lua_pop(L, 1);

    // call the callback function
    type = LUA_GET_METAFIELD_AND_RETURN_TYPE(L, -1, LUA_HTTP_REQUEST_CALLBACK);
    if (type == LUA_TNIL) {
        lua_pop(L, 3);
        LUAUTILS_STACK_SIZE_ASSERT(L, 0)
        return 0;
    }
    vx_assert(lua_isfunction(L, -1));

    _httpresponse_pushNewTable(L, statusCode, rawBody, responseDownloadComplete, resp.getHeaders());

    // 1 parameter, 0 return value
    if (lua_pcall(L, 1, 0, 0) != LUA_OK) {
        if (lua_utils_isStringStrict(L, -1)) {
            lua_log_error_CStr(L, lua_tostring(L, -1));
        } else {
            lua_log_error_CStr(L, "Http - error on callback");
        }
        lua_pop(L, 1);
    }

    // Stack status:
    // -1: Http Request instance
    // -2: HTTP Requests array
    // -3: global HTTP

    lua_pop(L, 2);
    LUAUTILS_STACK_SIZE_ASSERT(L, 1)

    if (responseDownloadComplete) {
        // remove HTTP Request from pending requests
        type = LUA_GET_METAFIELD_AND_RETURN_TYPE(L, -1, LUA_G_HTTP_PRIVATE_FIELD_PENDING_REQUESTS);
        vx_assert(type == LUA_TTABLE);

        lua_pushlightuserdata(L, req.get());
        lua_pushnil(L);
        lua_rawset(L, -3);

        lua_pop(L, 2);
    } else {
        lua_pop(L, 1); // pop global HTTP
    }

    LUAUTILS_STACK_SIZE_ASSERT(L, 0)
    return 0;
}

int lua_http_kvs_get_callback(lua_State * const L, vx::HttpRequest_SharedPtr const &req) {
    LUAUTILS_STACK_SIZE(L)

    vx::HttpResponse &resp = req->getResponse();

    const int statusCode = resp.getStatusCode();

    std::string rawBody;
    if (resp.getSuccess()) {
        const bool ok = resp.readAllBytes(rawBody);
        if (ok == false) {
            // TODO: handle error
        }
    }

    lua_getglobal(L, LUA_G_HTTP);
    vx_assert(lua_utils_getObjectType(L, -1) == ITEM_TYPE_G_HTTP);

    int type = LUA_GET_METAFIELD_AND_RETURN_TYPE(L, -1, LUA_G_HTTP_PRIVATE_FIELD_PENDING_REQUESTS);
    vx_assert(type == LUA_TTABLE);

    // get the HTTP Request stored in pending HTTP Requests with its address
    lua_pushlightuserdata(L, req.get());
    lua_rawget(L, -2);

    // lua HTTPRequest is on top of the stack
    lua_getmetatable(L, -1);

    // -1 metatable
    // -2 lua HTTPRequest
    lua_pushstring(L, LUA_HTTP_REQUEST_STATUS);
    lua_pushinteger(L, vx::HttpRequest::Status::DONE);
    lua_rawset(L, -3);

    // -1 metatable
    // -2 lua HTTPRequest
    lua_pop(L, 1);

    // call the callback function
    type = LUA_GET_METAFIELD_AND_RETURN_TYPE(L, -1, LUA_HTTP_REQUEST_CALLBACK);
    if (type == LUA_TNIL) {
        lua_pop(L, 3);
        LUAUTILS_STACK_SIZE_ASSERT(L, 0)
        return 0;
    }

    // argument 1
    const bool ok = statusCode == 200;
    lua_pushboolean(L, ok);

    // argument 2
    if (ok == false) {
        lua_pushnil(L);
    } else {
        cJSON *json = cJSON_Parse(rawBody.c_str());
        if (json == nullptr) {
            lua_pushnil(L);
        } else {
            std::unordered_map<std::string, std::string> result;
            const bool ok = vx::json::readMapStringString(json, result);
            if (ok == false) {
                lua_pushnil(L);
            } else {
                const bool ok = lua_http_decode_kv_map(L, result);
                if (ok == false) {
                    lua_pushnil(L);
                }
            }
        }
    }

    // 1 parameter, 0 return value
    if (lua_pcall(L, 2, 0, 0) != LUA_OK) {
        if (lua_utils_isStringStrict(L, -1)) {
            lua_log_error_CStr(L, lua_tostring(L, -1));
        } else {
            lua_log_error_CStr(L, "Http - error on callback");
        }
        lua_pop(L, 1);
    }

    // Stack status:
    // -1: Http Request instance
    // -2: HTTP Requests array
    // -3: global HTTP

    lua_pop(L, 2);

    // remove HTTP Request from pending requests
    type = LUA_GET_METAFIELD_AND_RETURN_TYPE(L, -1, LUA_G_HTTP_PRIVATE_FIELD_PENDING_REQUESTS);
    vx_assert(type == LUA_TTABLE);

    lua_pushlightuserdata(L, req.get());
    lua_pushnil(L);
    lua_rawset(L, -3);

    lua_pop(L, 2);

    LUAUTILS_STACK_SIZE_ASSERT(L, 0)
    return 0;
}

int lua_http_kvs_set_callback(lua_State * const L, vx::HttpRequest_SharedPtr const &req) {
    LUAUTILS_STACK_SIZE(L)

    vx::HttpResponse &resp = req->getResponse();

    const int statusCode = resp.getStatusCode();

    std::string rawBody;
    if (resp.getSuccess()) {
        const bool ok = resp.readAllBytes(rawBody);
        if (ok == false) {
            // TODO: handle error
        }
    }

    lua_getglobal(L, LUA_G_HTTP);
    vx_assert(lua_utils_getObjectType(L, -1) == ITEM_TYPE_G_HTTP);
    
    int type = LUA_GET_METAFIELD_AND_RETURN_TYPE(L, -1, LUA_G_HTTP_PRIVATE_FIELD_PENDING_REQUESTS);
    vx_assert(type == LUA_TTABLE);

    // get the HTTP Request stored in pending HTTP Requests with its address
    lua_pushlightuserdata(L, req.get());
    lua_rawget(L, -2);

    // lua HTTPRequest is on top of the stack
    lua_getmetatable(L, -1);

    // -1 metatable
    // -2 lua HTTPRequest
    lua_pushstring(L, LUA_HTTP_REQUEST_STATUS);
    lua_pushinteger(L, vx::HttpRequest::Status::DONE);
    lua_rawset(L, -3);

    // -1 metatable
    // -2 lua HTTPRequest
    lua_pop(L, 1);

    // call the callback function
    type = LUA_GET_METAFIELD_AND_RETURN_TYPE(L, -1, LUA_HTTP_REQUEST_CALLBACK);
    if (type == LUA_TNIL) {
        lua_pop(L, 3);
        LUAUTILS_STACK_SIZE_ASSERT(L, 0)
        return 0;
    }

    // argument 1
    const bool ok = statusCode == 200;
    lua_pushboolean(L, ok);

    // 1 parameter, 0 return value
    if (lua_pcall(L, 1, 0, 0) != LUA_OK) {
        if (lua_utils_isStringStrict(L, -1)) {
            lua_log_error_CStr(L, lua_tostring(L, -1));
        } else {
            lua_log_error_CStr(L, "Http - error on callback");
        }
        lua_pop(L, 1);
    }

    // Stack status:
    // -1: Http Request instance
    // -2: HTTP Requests array
    // -3: global HTTP

    lua_pop(L, 2);

    // remove HTTP Request from pending requests
    type = LUA_GET_METAFIELD_AND_RETURN_TYPE(L, -1, LUA_G_HTTP_PRIVATE_FIELD_PENDING_REQUESTS);
    vx_assert(type == LUA_TTABLE);

    lua_pushlightuserdata(L, req.get());
    lua_pushnil(L);
    lua_rawset(L, -3);

    lua_pop(L, 2);

    LUAUTILS_STACK_SIZE_ASSERT(L, 0)
    return 0;
}

void lua_http_parse_headers(std::unordered_map<std::string, std::string> &headers,
                            lua_State *L,
                            int tableIndex) {
    LUAUTILS_STACK_SIZE(L)

    if (lua_istable(L, tableIndex) == false) {
        LUAUTILS_ERROR(L, "HTTP:Get - third argument should be a table");
    }

    lua_pushvalue(L, tableIndex); // place table on top
    lua_pushnil(L);               // first key

    // nil -1
    // table -2

    while (lua_next(L, -2) != 0) { // lua_next pops key at -1 (nil at first iteration)
        // Item key now at -2, value at -1

        // value -1
        // key -2
        // table -3
        if (lua_utils_isStringStrict(L, -2) && lua_utils_isStringStrict(L, -1)) {
            headers.insert({lua_tostring(L, -2), lua_tostring(L, -1)});
        } else {
            LUAUTILS_ERROR(L, "HTTP:Get - wrong type in headers (string only)");
        }
        // pop value, keep key for next iteration
        lua_pop(L, 1);
    }
    // pop table
    lua_pop(L, 1);
    LUAUTILS_STACK_SIZE_ASSERT(L, 0)
}

// Function to retrieve string values from a Lua array and store them in a set
// Returns true on success, false otherwise.
bool lua_http_parse_string_set(lua_State * const L,
                               const int arrayIdx,
                               std::unordered_set<std::string> &result) {
    LUAUTILS_STACK_SIZE(L)

    // Ensure the given index is a table
    if (lua_istable(L, arrayIdx) == false) {
        LUAUTILS_STACK_SIZE_ASSERT(L, 0)
        return false;
    }

    std::unordered_set<std::string> set;

    // Get the length of the array
    const int arrayLen = lua_objlen(L, arrayIdx);

    // Iterate through the array and store string values in the set
    for (int i = 1; i <= arrayLen; ++i) {
        lua_pushinteger(L, i);
        lua_gettable(L, arrayIdx);

        if (lua_isstring(L, -1)) {
            const char* cstr = lua_tostring(L, -1);
            std::string value = cstr != nullptr ? cstr : "" ;
            set.insert(value);
        } else {
            LUAUTILS_STACK_SIZE_ASSERT(L, 0)
            return false;
        }

        lua_pop(L, 1); // Pop the value from the stack
    }

    result = std::move(set);

    LUAUTILS_STACK_SIZE_ASSERT(L, 0)
    return true;
}

// converts [Lua map] to [C++ map[string]base64]
bool lua_http_encode_kv_map(lua_State * const L,
                            const int mapIdx,
                            std::unordered_map<std::string, std::string> &result) {
    LUAUTILS_STACK_SIZE(L)

    // Ensure the given index is a table
    if (lua_istable(L, mapIdx) == false) {
        LUAUTILS_STACK_SIZE_ASSERT(L, 0)
        return false;
    }

    // Create an unordered_map to store key-value pairs
    std::unordered_map<std::string, std::string> keyValueMap;

    // Iterate over the table
    lua_pushnil(L);  // Push nil to start the iteration
    while (lua_next(L, mapIdx) != 0) {
        const int keyType = lua_type(L, -2);
        // const int valueType = lua_type(L, -1);

        // only string-typed keys are considered
        if (keyType == LUA_TSTRING) {
            // Get the key and value as strings
            const char *key = lua_tostring(L, -2);

            // if (valueType == LUA_TNIL) {
            //     keyValueMap[key] = "";
            // }

            uint8_t *bytes = nullptr;
            size_t size = 0;
            std::string error;
            const bool ok = lua_bin_serializer_encode_value(L, -1, &bytes, &size, error);
            if (ok == false) {
                continue;
            }

            // Encode the value in base64
            const std::string base64Str = vx::encoding::base64::encode(std::string(reinterpret_cast<char*>(bytes), size));

            // Store the key-value pair in the unordered_map
            keyValueMap[key] = base64Str;
        }

        // Pop the value, leaving the key for the next iteration
        lua_pop(L, 1);
    }

    result = std::move(keyValueMap);

    LUAUTILS_STACK_SIZE_ASSERT(L, 0)
    return true;
}

bool lua_http_decode_kv_map(lua_State * const L,
                            const std::unordered_map<std::string, std::string> &keyValueMap) {
    LUAUTILS_STACK_SIZE(L)
    std::string error;
    bool ok;

    lua_newtable(L);

    for (std::pair<std::string, std::string> pair : keyValueMap) {
        const std::string& key = pair.first;
        std::string bytes;
        error = vx::encoding::base64::decode(pair.second, bytes);
        if (error.empty() == false) {
            break;
        }

        lua_pushstring(L, key.c_str());
        ok = lua_bin_serializer_decode_and_push_value(L, reinterpret_cast<const uint8_t*>(bytes.c_str()), bytes.size(), error);
        if (ok == false) {
            LUAUTILS_STACK_SIZE_ASSERT(L, 0)
            lua_pop(L, 2);
            return false;
        }
        lua_rawset(L, -3);
    }

    if (error.empty() == false) {
        lua_pop(L, 1);
        LUAUTILS_STACK_SIZE_ASSERT(L, 0)
        return false;
    }

    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
    return true;
}

// Signatures
// HTTP:Get(<url>, <callback>)
// HTTP:Get(<url>, <headers>, <callback>)
// HTTP:Get(<url>, <headers>, <config>, <callback>) (TODO?)
static int _g_http_get(lua_State * const L) {
    LUAUTILS_STACK_SIZE(L)

    const int argsCount = lua_gettop(L);
    const bool hasHeaders = argsCount == 4;
    const int callbackIndex = hasHeaders ? 4 : 3;

    if (argsCount < 1 || lua_utils_getObjectType(L, 1) != ITEM_TYPE_G_HTTP) {
        LUAUTILS_ERROR(L, "HTTP:Get - function should be called with `:`");
    }
    if (argsCount < 3 || argsCount > 4) {
        LUAUTILS_ERROR(L, "HTTP:Get - incorrect argument count");
    }

    if (lua_isstring(L, 2) == false) {
        LUAUTILS_ERROR(L, "HTTP:Get - argument 1 should be a string");
    }

    // Parse headers provided from Lua
    std::unordered_map<std::string, std::string> headers;
    if (hasHeaders) {
        lua_http_parse_headers(headers, L, 3);
    }

    if (lua_isfunction(L, callbackIndex) == false) {
        LUAUTILS_ERROR_F(L,
                         "HTTP:Get - %s argument should be a function",
                         hasHeaders ? "third" : "second");
    }

    const char *urlCStr = lua_tostring(L, 2);
    const std::string urlStr(urlCStr);
    const vx::URL url = vx::URL::make(urlStr);

    if (url.isValid() == false) {
        LUAUTILS_ERROR(L, "HTTP:Get - URL is not valid");
    }

    if (_urlRequiresPrivilegedCubzhAPIAccess(url)) {
        LUAUTILS_ERROR(L, "HTTP:Get - privileged API access is required");
    }

    // Should apply to server as well, let's just make sure there's no
    // issue communicating with the API with https before removing the condition
#if !defined(DEBUG)
#if defined(CLIENT_ENV)
    if (url.scheme() != "https" && url.host() != "localhost") {
        LUAUTILS_ERROR(L, "HTTP:Get - only secure HTTPS requests are allowed.");
    }
#endif
#endif

    // Insert credentials when request is sent to api.cu.bzh
    {

        if (url.host() == CUBZH_API_SERVER_URL || url.host() == CUBZH_API_SERVER_DEV_ADDR) {
#if defined(CLIENT_ENV)
            // URL is the one of the Cubzh API
            const vx::HubClient::UserCredentials creds = vx::HubClient::getInstance().getCredentials();
            headers.emplace(HTTP_HEADER_CUBZH_CLIENT_VERSION_V2, vx::device::appVersionCached());
            headers.emplace(HTTP_HEADER_CUBZH_CLIENT_BUILDNUMBER_V2, vx::device::appBuildNumberCached());
            headers.emplace(HTTP_HEADER_CUBZH_USER_ID, creds.ID);
            headers.emplace(HTTP_HEADER_CUBZH_USER_TOKEN, creds.token);
#elif defined(ONLINE_GAMESERVER)
            headers.emplace(HTTP_HEADER_CUBZH_GAME_SERVER_TOKEN, HTTP_HEADER_CUBZH_GAME_SERVER_TOKEN_VALUE);
#endif
        }
    }

    vx::Game *cppGame = nullptr;
    if (lua_utils_getGameCppWrapperPtr(L, &cppGame) == false) {
        LUAUTILS_ERROR(L, "HTTP:Get - failed to retrieve Game from lua state");
    }
    vx::Game_WeakPtr weakPtr = cppGame->getWeakPtr();

    vx::HttpRequestOpts opts;
    opts.setSendNow(false);

    vx::HttpRequest_SharedPtr req = vx::HttpClient::shared().GET(url,
                                                                 headers,
                                                                 opts,
                                                                 [weakPtr](vx::HttpRequest_SharedPtr req) {
        if (req->getStatus() == vx::HttpRequest::Status::CANCELLED) {
            return;
        }

        vx::Game_SharedPtr cppGame = weakPtr.lock();
        if (cppGame == nullptr) {
            return;
        }

        vx::OperationQueue *mainQueue = cppGame->isInClientMode()
        ? vx::OperationQueue::getMain()
        : vx::OperationQueue::getServerMain();

        mainQueue->dispatch([weakPtr, req]() {
            if (req->getStatus() == vx::HttpRequest::Status::CANCELLED) {
                return;
            }

            // Get L from weak game ptr
            vx::Game_SharedPtr cppGame = weakPtr.lock();
            if (cppGame == nullptr) {
                return;
            }
            lua_State *L = cppGame->getLuaState();
            vx_assert(L != nullptr);

            lua_http_httprequest_callback(L, req, req->getResponse());
        });
    });
    lua_http_request_create_and_push(L, callbackIndex, req);
    req->sendAsync();

    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
    return 1;
}

// HTTP:GetStream(url, func)
// <func> can be called multiple times
static int _g_http_get_stream(lua_State * const L) {
    vxlog_debug("[🐞][C++][_g_http_get_stream]");

    LUAUTILS_STACK_SIZE(L)

    const int argsCount = lua_gettop(L);
    const bool hasHeaders = argsCount == 4;
    const int callbackIndex = hasHeaders ? 4 : 3;

    if (argsCount < 1 || lua_utils_getObjectType(L, 1) != ITEM_TYPE_G_HTTP) {
        LUAUTILS_ERROR(L, "HTTP:GetStream - function should be called with `:`");
    }
    if (argsCount < 3 || argsCount > 4) {
        LUAUTILS_ERROR(L, "HTTP:GetStream - incorrect argument count");
    }

    if (lua_isstring(L, 2) == false) {
        LUAUTILS_ERROR(L, "HTTP:GetStream - argument 1 should be a string");
    }

    // Parse headers provided from Lua
    std::unordered_map<std::string, std::string> headers;
    if (hasHeaders) {
        lua_http_parse_headers(headers, L, 3);
    }

    if (lua_isfunction(L, callbackIndex) == false) {
        LUAUTILS_ERROR_F(L,
                         "HTTP:GetStream - %s argument should be a function",
                         hasHeaders ? "third" : "second");
    }

    const char *urlCStr = lua_tostring(L, 2);
    const std::string urlStr(urlCStr);
    const vx::URL url = vx::URL::make(urlStr);

    if (url.isValid() == false) {
        LUAUTILS_ERROR(L, "HTTP:GetStream - URL is not valid");
    }

    if (_urlRequiresPrivilegedCubzhAPIAccess(url)) {
        LUAUTILS_ERROR(L, "HTTP:GetStream - privileged API access is required");
    }

    // Should apply to server as well, let's just make sure there's no
    // issue communicating with the API with https before removing the condition
#if defined(CLIENT_ENV)
    if (url.scheme() != "https" && url.host() != "localhost") {
        LUAUTILS_ERROR(L, "HTTP:Get - only secure HTTPS requests are allowed.");
    }
#endif

    // Insert credentials when request is sent to api.cu.bzh
    {

        if (url.host() == CUBZH_API_SERVER_URL || url.host() == CUBZH_API_SERVER_DEV_ADDR) {
#if defined(CLIENT_ENV)
            // URL is the one of the Cubzh API
            const vx::HubClient::UserCredentials creds = vx::HubClient::getInstance().getCredentials();
            headers.emplace(HTTP_HEADER_CUBZH_CLIENT_VERSION_V2, vx::device::appVersionCached());
            headers.emplace(HTTP_HEADER_CUBZH_CLIENT_BUILDNUMBER_V2, vx::device::appBuildNumberCached());
            headers.emplace(HTTP_HEADER_CUBZH_USER_ID, creds.ID);
            headers.emplace(HTTP_HEADER_CUBZH_USER_TOKEN, creds.token);
#elif defined(ONLINE_GAMESERVER)
            headers.emplace(HTTP_HEADER_CUBZH_GAME_SERVER_TOKEN, HTTP_HEADER_CUBZH_GAME_SERVER_TOKEN_VALUE);
#endif
        }
    }

    vx::Game *cppGame = nullptr;
    if (lua_utils_getGameCppWrapperPtr(L, &cppGame) == false) {
        LUAUTILS_ERROR(L, "HTTP:GetStream - failed to retrieve Game from lua state");
    }
    vx::Game_WeakPtr weakPtr = cppGame->getWeakPtr();

    vx::HttpRequest_SharedPtr req = vx::HttpRequest::make("GET",
                                                          url.host(),
                                                          url.port(),
                                                          url.path(),
                                                          url.queryParams(),
                                                          url.scheme() == VX_HTTPS_SCHEME); // secure
    if (req == nullptr) {
        LUAUTILS_ERROR(L, "HTTP request is nil");
    }

    vx::HttpRequestOpts opts;
    opts.setStreamResponse(true);
    opts.setForceCacheRevalidate(true);

    req->setOpts(opts);
    req->setHeaders(headers);
    req->setCallback([weakPtr](vx::HttpRequest_SharedPtr req) {
        if (req->getStatus() == vx::HttpRequest::Status::CANCELLED) {
            return;
        }

        vx::Game_SharedPtr cppGame = weakPtr.lock();
        if (cppGame == nullptr) {
            return;
        }

        vx::OperationQueue *mainQueue = cppGame->isInClientMode()
        ? vx::OperationQueue::getMain()
        : vx::OperationQueue::getServerMain();

        mainQueue->dispatch([weakPtr, req]() {
            if (req->getStatus() == vx::HttpRequest::Status::CANCELLED) {
                return;
            }

            // Get L from weak game ptr
            vx::Game_SharedPtr cppGame = weakPtr.lock();
            if (cppGame == nullptr) {
                return;
            }
            lua_State *L = cppGame->getLuaState();
            vx_assert(L != nullptr);

            lua_http_httprequest_callback(L, req, req->getResponse());
        });
    });

    lua_http_request_create_and_push(L, callbackIndex, req);
    req->sendAsync();

    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
    return 1;
}


static int _g_http_post(lua_State * const L) {
    LUAUTILS_STACK_SIZE(L)

    const int argsCount = lua_gettop(L);

    if (argsCount < 1 || lua_utils_getObjectType(L, 1) != ITEM_TYPE_G_HTTP) {
        LUAUTILS_ERROR(L, "HTTP:Post(config) - function should be called with `:`");
    }

    std::string urlStr;
    std::unordered_map<std::string, std::string> headers;
    std::string body;
    vx::HttpRequestOpts opts;
    opts.setSendNow(false);
    opts.setStreamResponse(false);
    opts.setForceCacheRevalidate(false);

    // new API (0.1.19+) -> HTTP:Post(config)
    if (argsCount >= 2 && lua_istable(L, 2)) {
        std::string err;
        if (_parseRequestConfig(L, 2, urlStr, body, headers, opts, err) == false) {
            LUAUTILS_ERROR_F(L, "HTTP:Post(config) - %s", err.c_str());
        }
    } else {
        // LEGACY PARAMETERS
        if (argsCount < 4 || argsCount > 5) {
            // error voluntarily encouraging use of new API
            // when wrong number of parameters
            LUAUTILS_ERROR(L, "HTTP:Post(config) - config should be a table");
        }

        const bool hasHeaders = argsCount == 5;
        const int bodyIndex = hasHeaders ? 4 : 3;
        const int callbackIndex = hasHeaders ? 5 : 4;

        if (lua_isstring(L, 2) == false) {
            LUAUTILS_ERROR(L, "HTTP:Post(url, [headers], body, callback) - url should be a string");
        }

        if (hasHeaders) {
            lua_http_parse_headers(headers, L, 3);
        }

        if (lua_utils_getObjectType(L, bodyIndex) == ITEM_TYPE_DATA) {
            size_t len = 0;
            const void *bytes = lua_data_getBuffer(L, bodyIndex, &len);
            body = std::string(static_cast<const char *>(bytes), len);

        } else if (lua_istable(L, bodyIndex)) {
            std::ostringstream streamEncoded;
            lua_table_to_json_string(streamEncoded, L, bodyIndex);
            body = streamEncoded.str();

        } else if (lua_isstring(L, bodyIndex)) {
            body = lua_tostring(L, bodyIndex);

        } else {
            LUAUTILS_ERROR(L, "HTTP:Post(url, [headers], body, callback) - body should be a table, string or Data");
        }

        if (lua_isfunction(L, callbackIndex) == false) {
            LUAUTILS_ERROR(L, "HTTP:Post(url, [headers], body, callback) - callback should be a function");
        }

        lua_pushvalue(L, callbackIndex); // callback now at -1

        urlStr = std::string(lua_tostring(L, 2));
    }

    const vx::URL url = vx::URL::make(urlStr);

    if (url.isValid() == false) {
        LUAUTILS_ERROR(L, "HTTP:Post - url is not valid");
    }

    if (_urlRequiresPrivilegedCubzhAPIAccess(url)) {
        LUAUTILS_ERROR(L, "HTTP:Post - privileged API access is required");
    }

    // Should apply to server as well, let's just make sure there's no
    // issue communicating with the API with https before removing the condition
#if !defined(DEBUG)
#if defined(CLIENT_ENV)
    if (url.scheme() != "https" && url.host() != "localhost") {
        LUAUTILS_ERROR(L, "HTTP:Post - only secure HTTPS requests are allowed.");
    }
#endif
#endif

    // Insert credentials when request is sent to api.cu.bzh
    {
        if (url.host() == CUBZH_API_SERVER_URL || url.host() == CUBZH_API_SERVER_DEV_ADDR) {
#if defined(CLIENT_ENV)
            // URL is the one of the Cubzh API
            const bool isAIEndpoint = url.path().find("/ai/") != std::string::npos;
            if (isAIEndpoint) {
                const vx::HubClient::UserCredentials creds = vx::HubClient::getInstance().getCredentials();
                headers.emplace(HTTP_HEADER_CUBZH_CLIENT_VERSION_V2, vx::device::appVersionCached());
                headers.emplace(HTTP_HEADER_CUBZH_CLIENT_BUILDNUMBER_V2, vx::device::appBuildNumberCached());
                headers.emplace(HTTP_HEADER_CUBZH_USER_ID, creds.ID);
                headers.emplace(HTTP_HEADER_CUBZH_USER_TOKEN, creds.token);
            } else {
                // Post to api.cu.bzh only available on server, except for /ai/ paths
                LUAUTILS_ERROR(L, "HTTP:Post to api.cu.bzh - only available on the server");
            }
#elif defined(ONLINE_GAMESERVER)
            headers.emplace(HTTP_HEADER_CUBZH_GAME_SERVER_TOKEN, HTTP_HEADER_CUBZH_GAME_SERVER_TOKEN_VALUE);
#endif
        } else if (url.host() == CUBZH_CODEGEN_SERVER_DOMAIN) {
            // codegen.cu.bzh
            // add user credentials
            const vx::HubClient::UserCredentials creds = vx::HubClient::getInstance().getCredentials();
            headers.emplace(HTTP_HEADER_CUBZH_CLIENT_VERSION_V2, vx::device::appVersionCached());
            headers.emplace(HTTP_HEADER_CUBZH_CLIENT_BUILDNUMBER_V2, vx::device::appBuildNumberCached());
            headers.emplace(HTTP_HEADER_CUBZH_USER_ID, creds.ID);
            headers.emplace(HTTP_HEADER_CUBZH_USER_TOKEN, creds.token);
        }
    }

    vx::Game *cppGame = nullptr;
    if (lua_utils_getGameCppWrapperPtr(L, &cppGame) == false) {
        LUAUTILS_ERROR(L, "HTTP:Post - failed to retrieve Game from lua state");
    }
    vx::Game_WeakPtr weakPtr = cppGame->getWeakPtr();

    vx::HttpRequest_SharedPtr req = vx::HttpClient::shared().POST(urlStr,
                                                                  headers,
                                                                  &opts,
                                                                  body,
                                                                  false /*don't send*/,
                                                                  [weakPtr](vx::HttpRequest_SharedPtr req) {
                                                                      if (req->getStatus() == vx::HttpRequest::Status::CANCELLED) { return; }

                                                                      vx::Game_SharedPtr cppGame = weakPtr.lock();
                                                                      if (cppGame == nullptr)
                                                                          return;

                                                                      vx::OperationQueue *mainQueue = cppGame->isInClientMode()
                                                                      ? vx::OperationQueue::getMain()
                                                                      : vx::OperationQueue::getServerMain();

                                                                      mainQueue->dispatch([weakPtr, req]() {
                                                                          if (req->getStatus() == vx::HttpRequest::Status::CANCELLED) { return; }

                                                                          // Get L from weak game ptr
                                                                          vx::Game_SharedPtr cppGame = weakPtr.lock();
                                                                          if (cppGame == nullptr)
                                                                              return;

                                                                          lua_State *L = cppGame->getLuaState();
                                                                          vx_assert(L != nullptr);

                                                                          vxlog_debug("POST: %s %s %d",
                                                                                      req->getHost().c_str(),
                                                                                      req->getPath().c_str(),
                                                                                      req->getResponse().getStatusCode());
                                                                          lua_http_httprequest_callback(L, req, req->getResponse());
                                                                      });
                                                                  });

    vx_assert(lua_isfunction(L, -1));
    lua_http_request_create_and_push(L, -1, req); // callback: -1
    req->sendAsync();

    lua_remove(L, -2); // remove callback function
    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
    return 1;
}

static int _g_http_patch(lua_State * const L) {
    LUAUTILS_STACK_SIZE(L)

    const int argsCount = lua_gettop(L);
    const bool hasHeaders = argsCount == 5;
    const int bodyIndex = hasHeaders ? 4 : 3;
    const int callbackIndex = hasHeaders ? 5 : 4;

    if (argsCount < 1 || lua_utils_getObjectType(L, 1) != ITEM_TYPE_G_HTTP) {
        LUAUTILS_ERROR(L, "HTTP:Patch - function should be called with `:`");
    }

    if (argsCount < 4 || argsCount > 5) {
        LUAUTILS_ERROR(L, "HTTP:Patch - incorrect argument count");
    }

    if (lua_isstring(L, 2) == false) {
        LUAUTILS_ERROR(L, "HTTP:Patch - argument 1 should be a string");
    }

    std::unordered_map<std::string, std::string> headers;
    if (hasHeaders) {
        lua_http_parse_headers(headers, L, 3);
    }

    std::string body;
    if (lua_utils_getObjectType(L, bodyIndex) == ITEM_TYPE_DATA) {
        size_t len = 0;
        const void *bytes = lua_data_getBuffer(L, bodyIndex, &len);
        body = std::string(static_cast<const char *>(bytes), len);

    } else if (lua_istable(L, bodyIndex)) {
        std::ostringstream streamEncoded;
        lua_table_to_json_string(streamEncoded, L, bodyIndex);
        body = streamEncoded.str();

    } else if (lua_isstring(L, bodyIndex)) {
        body = lua_tostring(L, bodyIndex);

    } else {
        LUAUTILS_ERROR(L, "HTTP:Patch - body parameter should be a table, string or data");
    }

    if (lua_isfunction(L, callbackIndex) == false) {
        LUAUTILS_ERROR_F(L,
                         "HTTP:Patch - %s argument should be a function",
                         hasHeaders ? "fourth" : "third");
    }

    const char *urlCStr = lua_tostring(L, 2);
    const std::string urlStr(urlCStr);
    const vx::URL url = vx::URL::make(urlStr);

    if (url.isValid() == false) {
        LUAUTILS_ERROR(L, "HTTP:Patch - URL is not valid");
    }

    if (_urlRequiresPrivilegedCubzhAPIAccess(url)) {
        LUAUTILS_ERROR(L, "HTTP:Patch - privileged API access is required");
    }

    // Should apply to server as well, let's just make sure there's no
    // issue communicating with the API with https before removing the condition
#if !defined(DEBUG)
#if defined(CLIENT_ENV)
    if (url.scheme() != "https" && url.host() != "localhost") {
        LUAUTILS_ERROR(L, "HTTP:Patch - only secure HTTPS requests are allowed.");
    }
#endif
#endif

#if defined(ONLINE_GAMESERVER)
    // Insert credentials when request is sent to api.cu.bzh
    if (url.host() == CUBZH_API_SERVER_URL || url.host() == CUBZH_API_SERVER_DEV_ADDR) {
        headers.emplace(HTTP_HEADER_CUBZH_GAME_SERVER_TOKEN, HTTP_HEADER_CUBZH_GAME_SERVER_TOKEN_VALUE);
    }
#endif

    vx::Game *cppGame = nullptr;
    if (lua_utils_getGameCppWrapperPtr(L, &cppGame) == false) {
        LUAUTILS_ERROR(L, "HTTP:Patch - failed to retrieve Game from lua state");
    }
    vx::Game_WeakPtr weakPtr = cppGame->getWeakPtr();

    vx::HttpRequest_SharedPtr req = vx::HttpClient::shared().PATCH(urlStr,
                                                                   headers,
                                                                   nullptr,
                                                                   body,
                                                                   false /*don't send*/,
                                                                   [weakPtr](vx::HttpRequest_SharedPtr req) {
                                                                       if (req->getStatus() == vx::HttpRequest::Status::CANCELLED) { return; }

                                                                       vx::Game_SharedPtr cppGame = weakPtr.lock();
                                                                       if (cppGame == nullptr)
                                                                           return;

                                                                       vx::OperationQueue *mainQueue = cppGame->isInClientMode()
                                                                       ? vx::OperationQueue::getMain()
                                                                       : vx::OperationQueue::getServerMain();

                                                                       mainQueue->dispatch([weakPtr, req]() {
                                                                           if (req->getStatus() == vx::HttpRequest::Status::CANCELLED) { return; }

                                                                           // Get L from weak game ptr
                                                                           vx::Game_SharedPtr cppGame = weakPtr.lock();
                                                                           if (cppGame == nullptr)
                                                                               return;

                                                                           lua_State *L = cppGame->getLuaState();
                                                                           vx_assert(L != nullptr);

                                                                           lua_http_httprequest_callback(L, req, req->getResponse());
                                                                       });
                                                                   });
    lua_http_request_create_and_push(L, callbackIndex, req);
    req->sendAsync();

    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
    return 1;
}

static int _g_http_delete(lua_State * const L) {
    LUAUTILS_STACK_SIZE(L)

    const int argsCount = lua_gettop(L);
    const bool hasHeaders = argsCount == 5;
    const int bodyIndex = hasHeaders ? 4 : 3;
    const int callbackIndex = hasHeaders ? 5 : 4;

    if (argsCount < 1 || lua_utils_getObjectType(L, 1) != ITEM_TYPE_G_HTTP) {
        LUAUTILS_ERROR(L, "HTTP:Delete - function should be called with `:`");
    }

    if (argsCount < 4 || argsCount > 5) {
        LUAUTILS_ERROR(L, "HTTP:Delete - incorrect argument count");
    }

    if (lua_isstring(L, 2) == false) {
        LUAUTILS_ERROR(L, "HTTP:Delete - argument 1 should be a string");
    }

    std::unordered_map<std::string, std::string> headers;
    if (hasHeaders) {
        lua_http_parse_headers(headers, L, 3);
    }

    std::string body;
    if (lua_utils_getObjectType(L, bodyIndex) == ITEM_TYPE_DATA) {
        size_t len = 0;
        const void *bytes = lua_data_getBuffer(L, bodyIndex, &len);
        body = std::string(static_cast<const char *>(bytes), len);

    } else if (lua_istable(L, bodyIndex)) {
        std::ostringstream streamEncoded;
        lua_table_to_json_string(streamEncoded, L, bodyIndex);
        body = streamEncoded.str();

    } else if (lua_isstring(L, bodyIndex)) {
        body = lua_tostring(L, bodyIndex);

    } else {
        LUAUTILS_ERROR(L, "HTTP:Delete - body parameter should be a table, string or data");
    }

    if (lua_isfunction(L, callbackIndex) == false) {
        LUAUTILS_ERROR_F(L,
                         "HTTP:Delete - %s argument should be a function",
                         hasHeaders ? "fourth" : "third");
    }

    const char *urlCStr = lua_tostring(L, 2);
    const std::string urlStr(urlCStr);
    const vx::URL url = vx::URL::make(urlStr);

    if (url.isValid() == false) {
        LUAUTILS_ERROR(L, "HTTP:Delete - URL is not valid");
    }

    if (_urlRequiresPrivilegedCubzhAPIAccess(url)) {
        LUAUTILS_ERROR(L, "HTTP:Delete - privileged API access is required");
    }

    // Should apply to server as well, let's just make sure there's no
    // issue communicating with the API with https before removing the condition
#if !defined(DEBUG)
#if defined(CLIENT_ENV)
    if (url.scheme() != "https" && url.host() != "localhost") {
        LUAUTILS_ERROR(L, "HTTP:Delete - only secure HTTPS requests are allowed.");
    }
#endif
#endif


#if defined(ONLINE_GAMESERVER)
    // Insert credentials when request is sent to api.cu.bzh
    if (url.host() == CUBZH_API_SERVER_URL || url.host() == CUBZH_API_SERVER_DEV_ADDR) {
        headers.emplace(HTTP_HEADER_CUBZH_GAME_SERVER_TOKEN, HTTP_HEADER_CUBZH_GAME_SERVER_TOKEN_VALUE);
    }
#endif

    vx::Game *cppGame = nullptr;
    if (lua_utils_getGameCppWrapperPtr(L, &cppGame) == false) {
        LUAUTILS_ERROR(L, "HTTP:Delete - failed to retrieve Game from lua state");
    }
    vx::Game_WeakPtr weakPtr = cppGame->getWeakPtr();

    vx::HttpRequest_SharedPtr req = vx::HttpClient::shared().Delete(
                                                                    urlStr,
                                                                    headers,
                                                                    nullptr,
                                                                    body,
                                                                    false /*don't send*/,
                                                                    [weakPtr](vx::HttpRequest_SharedPtr req) {
                                                                        if (req->getStatus() == vx::HttpRequest::Status::CANCELLED) { return; }

                                                                        vx::Game_SharedPtr cppGame = weakPtr.lock();
                                                                        if (cppGame == nullptr)
                                                                            return;

                                                                        vx::OperationQueue *mainQueue = cppGame->isInClientMode()
                                                                        ? vx::OperationQueue::getMain()
                                                                        : vx::OperationQueue::getServerMain();

                                                                        mainQueue->dispatch([weakPtr, req]() {
                                                                            if (req->getStatus() == vx::HttpRequest::Status::CANCELLED) { return; }

                                                                            // Get L from weak game ptr
                                                                            vx::Game_SharedPtr cppGame = weakPtr.lock();
                                                                            if (cppGame == nullptr)
                                                                                return;

                                                                            lua_State *L = cppGame->getLuaState();
                                                                            vx_assert(L != nullptr);

                                                                            lua_http_httprequest_callback(L, req, req->getResponse());
                                                                        });
                                                                    });
    lua_http_request_create_and_push(L, callbackIndex, req);
    req->sendAsync();

    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
    return 1;
}

// parses config table and pushes callback at the top of the stack
static bool _parseRequestConfig(lua_State *L,
                                const int idx,
                                std::string &url,
                                std::string &body,
                                std::unordered_map<std::string, std::string> &headers,
                                vx::HttpRequestOpts &opts,
                                std::string &err) {
    LUAUTILS_STACK_SIZE(L)

    if (lua_istable(L, idx) == false) {
        err.assign("config should be a table");
        return false;
    }

    int type = lua_getfield(L, idx, "url");
    if (type == LUA_TNIL) {
        lua_pop(L, 1);
    } else if (type == LUA_TSTRING) {
        url = lua_tostring(L, -1);
        lua_pop(L, 1);
    } else {
        err = "config.url should be a string";
        lua_pop(L, 1);
        return false;
    }

    type = lua_getfield(L, idx, "headers");
    if (type == LUA_TNIL) {
        lua_pop(L, 1);
    } else if (type == LUA_TTABLE) {
        lua_http_parse_headers(headers, L, -1);
        lua_pop(L, 1);
    } else {
        err = "config.headers should be a table";
        lua_pop(L, 1);
        return false;
    }

    type = lua_getfield(L, idx, "body");
    if (type == LUA_TNIL) {
        lua_pop(L, 1);
    } else if (lua_utils_getObjectType(L, -1) == ITEM_TYPE_DATA) {
        size_t len = 0;
        const void *bytes = lua_data_getBuffer(L, -1, &len);
        body = std::string(static_cast<const char *>(bytes), len);
        lua_pop(L, 1);
    } else if (type == LUA_TTABLE) {
        std::ostringstream streamEncoded;
        lua_table_to_json_string(streamEncoded, L, -1);
        body = streamEncoded.str();
        lua_pop(L, 1);
    } else if (type == LUA_TSTRING) {
        body = lua_tostring(L, -1);
        lua_pop(L, 1);
    } else {
        err = "config.body should be a table, string or Data";
        lua_pop(L, 1);
        return false;
    }

    type = lua_getfield(L, idx, "streamed");
    if (type == LUA_TNIL) {
        lua_pop(L, 1);
    } else if (type == LUA_TBOOLEAN) {
        opts.setStreamResponse(lua_toboolean(L, -1));
        lua_pop(L, 1);
    } else {
        err = "config.streamed should be a boolean";
        lua_pop(L, 1);
        return false;
    }

    type = lua_getfield(L, idx, "callback");
    if (type == LUA_TNIL) {
        err = "config.callback (function) is mandatory.";
        lua_pop(L, 1);
        return false;
    } else if (type == LUA_TFUNCTION) {
        // leaves callback at -1
    } else {
        err = "config.callback should be a function";
        lua_pop(L, 1);
        return false;
    }

    LUAUTILS_STACK_SIZE_ASSERT(L, 1) // callback at -1
    return true;
}

int lua_g_http_snprintf(lua_State * const L, char *result, const size_t maxLen, const bool spacePrefix) {
    LUAUTILS_STACK_SIZE(L)
    LUAUTILS_STACK_SIZE_ASSERT(L, 0)
    return snprintf(result, maxLen, "%s[HTTP]", spacePrefix ? " " : "");
}
