// -------------------------------------------------------------
//  Cubzh
//  lua_fog.cpp
//  Created by Arthur Cormerais on June 12, 2023.
// -------------------------------------------------------------

#include "lua_sky.hpp"

// Lua
#include "lua.hpp"
#include "lua_utils.hpp"
#include "lua_constants.h"
#include "sbs.hpp"
#include "lua_color.hpp"
#include "lua_data.hpp"

// engine
#include "game.h"
#include "texture.h"

// vx
#include "VXGameRenderer.hpp"

// MARK: - Sky field names -

#define LUA_G_SKY_FIELD_LIGHT               "LightColor"
#define LUA_G_SKY_FIELD_SKY                 "SkyColor"
#define LUA_G_SKY_FIELD_HORIZON             "HorizonColor"
#define LUA_G_SKY_FIELD_ABYSS               "AbyssColor"
#define LUA_G_SKY_FIELD_IMAGE               "Image"

// MARK: - static private prototypes -

static int _sky_g_mt_index(lua_State *L);
static int _sky_g_mt_newindex(lua_State *L);

static Texture *_sky_getTextureFromFacesTable(lua_State *L, int idx, bool filtering);

// MARK: - static private functions -

static int _sky_g_mt_index(lua_State *L) {
    vx_assert(L != nullptr);
    LUAUTILS_STACK_SIZE(L)

    // validate arguments count
    if (lua_utils_assertArgsCount(L, 2) == false) {
        vxlog_error("Wrong argument count. Returning nil.");
        lua_pushnil(L);
        LUAUTILS_STACK_SIZE_ASSERT(L, 1)
        return 1;
    }

    // validate type of 1st argument
    if (lua_istable(L, 1) == false) {
        vxlog_error("Wrong 1st argument type. Returning nil.");
        lua_pushnil(L);
        LUAUTILS_STACK_SIZE_ASSERT(L, 1)
        return 1;
    }

    // validate type of 2nd argument
    if (lua_utils_isStringStrict(L, 2) == false) {
        vxlog_error("Wrong 2nd argument type. Returning nil.");
        lua_pushnil(L);
        LUAUTILS_STACK_SIZE_ASSERT(L, 1)
        return 1;
    }

    // retrieve Game from Lua state
    vx::Game *g = nullptr;
    if (lua_utils_getGameCppWrapperPtr(L, &g) == false || g == nullptr) {
        LUAUTILS_INTERNAL_ERROR(L)
        return 0;
    }

    vx::GameRendererState_SharedPtr grs = g->getGameRendererState();

    const char *key = lua_tostring(L, 2);

    if (strcmp(key, LUA_G_SKY_FIELD_LIGHT) == 0) {
        lua_color_create_and_push_table(L, grs->skyLightColor.x, grs->skyLightColor.y, grs->skyLightColor.z, 1.0f, false, true);
    } else if (strcmp(key, LUA_G_SKY_FIELD_SKY) == 0) {
        lua_color_create_and_push_table(L, grs->skyColor.x, grs->skyColor.y, grs->skyColor.z, 1.0f, false, true);
    } else if (strcmp(key, LUA_G_SKY_FIELD_HORIZON) == 0) {
        lua_color_create_and_push_table(L, grs->horizonColor.x, grs->horizonColor.y, grs->horizonColor.z, 1.0f, false, true);
    } else if (strcmp(key, LUA_G_SKY_FIELD_ABYSS) == 0) {
        lua_color_create_and_push_table(L, grs->abyssColor.x, grs->abyssColor.y, grs->abyssColor.z, 1.0f, false, true);
    } else if (strcmp(key, LUA_G_SKY_FIELD_IMAGE) == 0) {
        lua_pushboolean(L, grs->skyboxMode >= vx::rendering::SkyboxMode_Cubemap);
    } else {
        lua_pushnil(L);
    }

    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
    return 1;
}

static int _sky_g_mt_newindex(lua_State *L) {
    vx_assert(L != nullptr);
    LUAUTILS_STACK_SIZE(L)

    // validate arguments count
    if (lua_utils_assertArgsCount(L, 3) == false) {
        vxlog_error("Wrong argument count. Returning nil.");
        lua_pushnil(L);
        LUAUTILS_STACK_SIZE_ASSERT(L, 1)
        return 1;
    }

    // validate type of 1st argument
    if (lua_istable(L, 1) == false) {
        vxlog_error("Wrong 1st argument type. Returning nil.");
        lua_pushnil(L);
        LUAUTILS_STACK_SIZE_ASSERT(L, 1)
        return 1;
    }
    if (lua_utils_getObjectType(L, 1) != ITEM_TYPE_G_SKY) {
        LUAUTILS_STACK_SIZE_ASSERT(L, 0)
        return false;
    }

    // validate type of 2nd argument
    if (lua_utils_isStringStrict(L, 2) == false) {
        vxlog_error("Wrong 2nd argument type. Returning nil.");
        lua_pushnil(L);
        LUAUTILS_STACK_SIZE_ASSERT(L, 1)
        return 1;
    }

    // retrieve Game from Lua state
    vx::Game *g = nullptr;
    if (lua_utils_getGameCppWrapperPtr(L, &g) == false || g == nullptr) {
        LUAUTILS_INTERNAL_ERROR(L)
        return 0;
    }

    vx::GameRendererState_SharedPtr grs = g->getGameRendererState();

    const char *key = lua_tostring(L, 2);

    if (strcmp(key, LUA_G_SKY_FIELD_LIGHT) == 0) {
        if (lua_utils_getObjectType(L, 3) == ITEM_TYPE_COLOR) {
            RGBAColor color;
            lua_color_getRGBAL(L, 3, &color.r, &color.g, &color.b, nullptr, nullptr);
            float3_set(&grs->skyLightColor, color.r / 255.0f, color.g / 255.0f, color.b / 255.0f);
        } else {
            LUAUTILS_ERROR(L, "Sky.LightColor must be a Color");
        }
    } else if (strcmp(key, LUA_G_SKY_FIELD_SKY) == 0) {
        if (lua_utils_getObjectType(L, 3) == ITEM_TYPE_COLOR) {
            RGBAColor color;
            lua_color_getRGBAL(L, 3, &color.r, &color.g, &color.b, nullptr, nullptr);
            grs->setSkyColor({ color.r / 255.0f, color.g / 255.0f, color.b / 255.0f });
        } else {
            LUAUTILS_ERROR(L, "Sky.SkyColor must be a Color");
        }
    } else if (strcmp(key, LUA_G_SKY_FIELD_HORIZON) == 0) {
        if (lua_utils_getObjectType(L, 3) == ITEM_TYPE_COLOR) {
            RGBAColor color;
            lua_color_getRGBAL(L, 3, &color.r, &color.g, &color.b, nullptr, nullptr);
            grs->setHorizonColor({ color.r / 255.0f, color.g / 255.0f, color.b / 255.0f });
        } else {
            LUAUTILS_ERROR(L, "Sky.HorizonColor must be a Color");
        }
    } else if (strcmp(key, LUA_G_SKY_FIELD_ABYSS) == 0) {
        if (lua_utils_getObjectType(L, 3) == ITEM_TYPE_COLOR) {
            RGBAColor color;
            lua_color_getRGBAL(L, 3, &color.r, &color.g, &color.b, nullptr, nullptr);
            grs->setAbyssColor({ color.r / 255.0f, color.g / 255.0f, color.b / 255.0f });
        } else {
            LUAUTILS_ERROR(L, "Sky.AbyssColor must be a Color");
        }
    } else if (strcmp(key, LUA_G_SKY_FIELD_IMAGE) == 0) {
        if (lua_utils_getObjectType(L, 3) == ITEM_TYPE_DATA) {
            size_t size;
            const void *data = lua_data_getBuffer(L, 3, &size);
            if (data != nullptr) {
                Texture *tex[2] = { texture_new_raw(data, size, TextureType_Cubemap), nullptr };
                texture_set_filtering(tex[0], true);

                grs->setSkyboxTextures(tex);
            }
        } else if (lua_istable(L, 3)) {
            Texture *tex[SKYBOX_MAX_CUBEMAPS] = { nullptr, nullptr };
            bool filtering = true;
            int texIdx = 0;

            lua_getfield(L, 3, "filtering");
            if (lua_isboolean(L, -1)) {
                filtering = lua_toboolean(L, -1);
            } else if (lua_isnil(L, -1) == false) {
                LUAUTILS_ERROR(L, "Sky.Image table key 'filtering' must be a Data");
            }
            lua_pop(L, 1);

            // check if cubemap faces provided at the root level
            tex[texIdx] = _sky_getTextureFromFacesTable(L, 3, filtering);
            if (tex[texIdx] != nullptr) {
                ++texIdx;
            }

            // fill remaining slots with provided Data objects or tables of cubemap faces
            int luaIdx = 0;
            while(texIdx < SKYBOX_MAX_CUBEMAPS) {
                lua_rawgeti(L, 3, ++luaIdx);
                if (lua_isnil(L, -1)) {
                    lua_pop(L, 1);
                    break;
                } else if (lua_utils_getObjectType(L, -1) == ITEM_TYPE_DATA) {
                    size_t size;
                    const void *data = lua_data_getBuffer(L, -1, &size);
                    if (data != nullptr) {
                        tex[texIdx] = texture_new_raw(data, size, TextureType_Cubemap);
                        texture_set_filtering(tex[texIdx], filtering);
                        ++texIdx;
                    }
                } else if (lua_istable(L,-1)) {
                    tex[texIdx] = _sky_getTextureFromFacesTable(L, -1, filtering);
                    if (tex[texIdx] != nullptr) {
                        ++texIdx;
                    }
                }
                lua_pop(L, 1);
            }

            if (texIdx > 0) {
                grs->setSkyboxTextures(tex);
            }

            lua_getfield(L, 3, "t");
            if (lua_isnumber(L, -1)) {
                grs->setSkyboxLerp(static_cast<float>(lua_tonumber(L, -1)));
            } else if (lua_isnil(L, -1) == false) {
                LUAUTILS_ERROR_F(L, "Sky.%s table key 't' must be a number", key);
            }
            lua_pop(L, 1);
        } else if (lua_isnil(L, 3) || (lua_isboolean(L, 3) && lua_toboolean(L, 3) == false)) {
            grs->setSkyboxTextures(nullptr);
        } else {
            LUAUTILS_ERROR_F(L, "Sky.%s must either be Data, array of Data, table { from, to, t }, or nil/false to reset", key);
        }
    }

    LUAUTILS_STACK_SIZE_ASSERT(L, 0)
    return 0;
}

static Texture *_sky_getTextureFromFacesTable(lua_State *L, int idx, bool filtering) {
    const void* faceData[6] = { nullptr };
    size_t faceDataSize[6] = { 0 };
    const char* faceKeys[6] = { "right", "left", "top", "bottom", "front", "back" };
    bool any = false;
    
    // use "sides" key to pre-set front, back, right, left data
    lua_getfield(L, idx, "sides");
    if (lua_utils_getObjectType(L, -1) == ITEM_TYPE_DATA) {
        size_t sideSize;
        const void* sideData = lua_data_getBuffer(L, -1, &sideSize);
        if (sideData != nullptr) {
            for (int i = 0; i < 6; ++i) {
                if (i != 2 && i != 3) {
                    faceData[i] = sideData;
                    faceDataSize[i] = sideSize;
                }
            }
            any = true;
        }
    } else if (lua_isnil(L, -1) == false) {
        LUAUTILS_ERROR(L, "Sky.Image table key 'sides' must be a Data");
    }
    lua_pop(L, 1);

    // set individual faces data
    for (int i = 0; i < 6; ++i) {
        lua_getfield(L, idx, faceKeys[i]);
        if (lua_utils_getObjectType(L, -1) == ITEM_TYPE_DATA) {
            faceData[i] = lua_data_getBuffer(L, -1, &faceDataSize[i]);
            any |= faceData[i] != nullptr;
        } else if (lua_isnil(L, -1) == false) {
            LUAUTILS_ERROR_F(L, "Sky.Image table key '%s' must be a Data", faceKeys[i]);
        }
        lua_pop(L, 1);
    }

    return any ? vx::GameRenderer::cubemapFacesToTexture(faceData, faceDataSize, filtering) : nullptr;
}

// MARK: - public functions -

bool lua_g_sky_pushNewTable(lua_State *L) {
    vx_assert(L != nullptr);
    
    LUAUTILS_STACK_SIZE(L)
    // "Client.Sky" is defined only in the client sandbox
#ifdef CLIENT_ENV
    lua_newtable(L);
    lua_newtable(L); // metatable
    {
        lua_pushstring(L, "__index");
        lua_pushcfunction(L, _sky_g_mt_index, "");
        lua_settable(L, -3);

        lua_pushstring(L, "__newindex");
        lua_pushcfunction(L, _sky_g_mt_newindex, "");
        lua_settable(L, -3);

        // hide the metatable from the Lua sandbox user
        lua_pushstring(L, "__metatable");
        lua_pushboolean(L, false);
        lua_settable(L, -3);

        lua_pushstring(L, P3S_LUA_OBJ_METAFIELD_TYPE);
        lua_pushinteger(L, ITEM_TYPE_G_SKY);
        lua_settable(L, -3);
    }
    lua_setmetatable(L, -2);
#else
    lua_pushnil(L);
#endif
    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
    return true;
}

int lua_g_sky_snprintf(lua_State *L, char *result, size_t maxLen, bool spacePrefix) {
    vx_assert(L != nullptr);
    return snprintf(result, maxLen, "%s[Sky]", spacePrefix ? " " : "");
}
