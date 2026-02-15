//
//  lua_require.cpp
//  Cubzh
//
//  Created by Corentin Cailleaud on 10/08/2022.
//  Copyright � 2020 Voxowl Inc. All rights reserved.
//

#include "lua_require.hpp"

// Lua / Sandbox
#include "lua.hpp"
#include "lua_constants.h"
#include "lua_utils.hpp"
#include "lua_system.hpp"
#include "lua_logs.hpp"

#include "VXGame.hpp"
#include "VXConfig.hpp"

// xptools
#include "xptools.h"
#include "BZMD5.hpp"

#include <map>

#define SCRIPTS_PATH "modules/"
#define REQUIRE_LOADED_MODULES "_lm"

// HASHES START
static std::map<std::string, std::string> hashes = {
    {"ai.lua", "7a76bbbdf548abf723f627cafea1ebda"},
    {"ai_ambience.lua", "d3c1168a1a4479e5c2a7daf39f372b2b"},
    {"alert.lua", "4e492c7477e6ec583af35266c294f4a7"},
    {"ambience.lua", "8ef3e913169145dd9c165a7f3ad0ffaa"},
    {"animations.lua", "601442fb30aa829ab3a810fefb117e68"},
    {"api.lua", "7d58f4b698ded023bfd3c8ddbcf2dc65"},
    {"avatar.lua", "a6248096db10f712fb53d8190ef73a17"},
    {"badge.lua", "90479fd56b5725a42844b8100e5d6863"},
    {"badge_modal.lua", "24fbef89add5a9fc414f3c119e9c6d06"},
    {"box_gizmo.lua", "5950374f053149a7098085394857f8de"},
    {"box_outline.lua", "4851b6ef0225f26c3593aba18357351c"},
    {"bundle.lua", "b7b64fd832586404aa5b49edb1afac3b"},
    {"camera.lua", "e211afd6f1022dc41a88a9ba8e7b2744"},
    {"camera_modes.lua", "172cdad466300ebe3d16c62987c90945"},
    {"ccc.lua", "28a76b27c2bf4fb83e37f9ad91e8aabc"},
    {"chat.lua", "ef5609442663582146b13cfbe49d95cf"},
    {"cleanup.lua", "f69e5c4cd26de73e8239d19b54de629f"},
    {"coins.lua", "5f2db576fae83635ac7d3e257a3961b4"},
    {"colorpicker.lua", "72407bb5d542bbd5efee3a9137279706"},
    {"config.lua", "9f8081dd950afe4e1dd86e934623f8ee"},
    {"controls.lua", "753aaefabd07627339db75ff842ad689"},
    {"creations.lua", "0610c04498d7ab6004d9bd116abd6a0e"},
    {"crosshair.lua", "27db31cf924898fe322e78064a54897e"},
    {"cubzh.lua", "eef9ee69b9c6991a2ca2d45feac47676"},
    {"dialog.lua", "45a0ff3cc399845c423a51870596f619"},
    {"drawer.lua", "1f819441819efa48e47aa3206fd187d7"},
    {"ease.lua", "645a239a595286b04769f2cb4ba6a24d"},
    {"empty_table.lua", "758f71ae5158fe43af17275e107a81fb"},
    {"envtest.lua", "9df9eef2f9d6d20da58a5ddd0df864b0"},
    {"explode.lua", "77110317b3161e85c7d31421666575bf"},
    {"flow.lua", "384503a08ae638e5f8e8af21138c465d"},
    {"friends.lua", "3f769f4e87234733f3e57bdd2aa62bcb"},
    {"gallery.lua", "81dbf48c9065d66529eb0c17221d3cc1"},
    {"gizmo.lua", "97b8fd02b64c5fef6d1a1ca88ea05c59"},
    {"hierarchyactions.lua", "0a0671c9762d937ab3da7a282ee7c97a"},
    {"history.lua", "cd2be59852c447437e443f3c6a25f211"},
    {"horizontal_list.lua", "fd6be64d8aecc38c7288874397b51e5b"},
    {"input_modal.lua", "b42dede5075ec3fbc5dc34b396ccf2c7"},
    {"inputcodes.lua", "1eeccb7b5ecd8b028bd09058fe79df2c"},
    {"ishape.lua", "64af47f3d3bcbde53b026f989cda1a9c"},
    {"item_details.lua", "83f8ed906cdd406ccfb0c6afa7d75498"},
    {"item_editor.lua", "64e16c1a2a200a4f014808a1523c736a"},
    {"item_grid.lua", "a5f039b38fa1918187bcc24266d3a058"},
    {"jumpfly.lua", "e791f319d70cced971f448047c34658d"},
    {"kvs.lua", "55fd5128a3767b4873571b6bf86d3868"},
    {"leaderboard.lua", "5b828252fc4b5c745af37b9faf383721"},
    {"loading_modal.lua", "c8ba69f927dded517c1adbd368918569"},
    {"localevent.lua", "1b0618d16bdbf99be3e2818699f1e36d"},
    {"localize.lua", "b63b9f67759a91f5e2bb4f9e30f81c86"},
    {"logo.lua", "2e5c506d43c60a6f3d9e4c176a915750"},
    {"massloading.lua", "4bf08bb2d48a67832fedc5c1d28644ad"},
    {"menu.lua", "3d8386f49000f69e56125fb349153f1a"},
    {"modal.lua", "451cb1c747757c4ed2f6000a7041dd65"},
    {"movegizmo.lua", "6361622dcef41356cbff5a0b6ce2125c"},
    {"multi.lua", "6982e3dd41e8aec140986b108a4b4262"},
    {"notifications.lua", "468ecbe015b64e0aed93994be88fe634"},
    {"object_skills.lua", "203d0319f4e9090df1149c6ee18c7571"},
    {"orientationcube.lua", "61febe42e8c462ca7145439c931cd0f7"},
    {"pages.lua", "9c2bc9a4465baf2217c979b2c226b44a"},
    {"palette.lua", "b24947f6fb401e0ea108e6c4f0d1b46d"},
    {"particles.lua", "c98dfe4f7f4e35db22dc62a424e1e59a"},
    {"passkey.lua", "126356de12f237324fb9e0c2b2a9e524"},
    {"pf_astar.lua", "df08dead95b83b5adeea375ec1d45388"},
    {"phonenumbers.lua", "23076129b8f99eb1a9a1e1fd8adcf953"},
    {"plane.lua", "4f1d26d381eb8b1aef773cf25bfc5416"},
    {"player.lua", "d8d89a1cf90c890879e0592abcdc05ff"},
    {"polygonbuilder.lua", "e699fb44c6ba150dffe9fc1c3e4d806d"},
    {"profile.lua", "ecc767f808d89e8c9c49afe6d6857241"},
    {"radialmenu.lua", "99aa0e85332cd13ae5d1521db3153484"},
    {"report.lua", "affb9d0efdb15205d4dba74e12e6ce5e"},
    {"rotategizmo.lua", "0e709fe21fa5013e5e5dbd5b000e8185"},
    {"safe_local_store.lua", "edfff9a9fd1a1c6a5b493cd2c813c006"},
    {"scalegizmo.lua", "6ba93690b159dd33fd3680085f6f2033"},
    {"server_list.lua", "d10a065eb3b941b5fdc9ee0db038c98d"},
    {"settings.lua", "0ab246eece08f016cb9a90c1990d5b2e"},
    {"sfx.lua", "fb7b4203ea77dd2255865358206e6d1a"},
    {"signup.lua", "35d04143638dbe3d785296fde1bdc349"},
    {"social.lua", "8af6883dac5d7305f8ce07cf10fe89a8"},
    {"str.lua", "06b5faf5fa901fcf57acea45e6128c26"},
    {"system_api.lua", "20e4ce157c5d30f01ef861550d4d0bdd"},
    {"textbubbles.lua", "2d765edf378dd6262e01d99eef17eee0"},
    {"time.lua", "f55039a1bfed507fa067363b2130083f"},
    {"trail.lua", "2ced19ddd5b8ee6485507844f1ccdd4f"},
    {"transformgizmo.lua", "b595651f55a154f7a1bcb75b95f2ebf1"},
    {"ui_avatar.lua", "e88e28eb8355a1eab807e269a65f988a"},
    {"ui_avatar_editor.lua", "ebb6dd0482463f7cce415907c1c9f6fe"},
    {"ui_container.lua", "9eae7118578f44476e308695b550beb2"},
    {"ui_gizmo_rotation.lua", "de616aa12774ee78d9ce6474dcc13759"},
    {"ui_loading_animation.lua", "f8c360d65760dc372135da83d1756a09"},
    {"ui_outfit.lua", "4caa81fdbf77f7dadb84c15eb8cf310e"},
    {"ui_pointer.lua", "287e0edc380cd5c62559a61029afe1d2"},
    {"ui_toast.lua", "715f37bb2716b840978d42650dd69aab"},
    {"uikit.lua", "b5588281ee81c6df95619fd6325d5077"},
    {"uitheme.lua", "41f512eb8ae7d560019a719b658b45cb"},
    {"url.lua", "bc033016cd7c5e8328003f4b072fae7f"},
    {"user.lua", "3ab93d7e719c270f1f831a5e4ef75e80"},
    {"username_form.lua", "585c384187b1e49677fd47cd1f7218e6"},
    {"verify_account_form.lua", "87950e7828953760ca353b95ba46d5e2"},
    {"walk_sfx.lua", "7e9dab4a5f2a16f5fc1d39f176e1eb33"},
    {"wingtrail.lua", "73a43206d503e99fb24d46cf354821fd"},
    {"world.lua", "e8466779e9fa38270765ff9946d159c6"},
    {"world_details.lua", "8af184301644bcb692532a61777847a2"},
    {"world_editor.lua", "1e2a6910fbcf252d378073e75453012e"},
    {"world_v2.lua", "dd567eb3d84877ade826453ad44749fc"},
};
// HASHES END

static int _g_require_metatable_index(lua_State *L);
static int _g_require_metatable_newindex(lua_State *L);
static int _g_require_metatable_call(lua_State *L);

void lua_g_require_create_and_push(lua_State *const L) {
    vx_assert(L != nullptr);
    LUAUTILS_STACK_SIZE(L)

    lua_newtable(L); // global "require" table
    lua_newtable(L); // metatable
    {
        lua_pushstring(L, "__index");
        lua_pushcfunction(L, _g_require_metatable_index, "");
        lua_rawset(L, -3);

        lua_pushstring(L, "__newindex");
        lua_pushcfunction(L, _g_require_metatable_newindex, "");
        lua_rawset(L, -3);

        lua_pushstring(L, "__call");
        lua_pushcfunction(L, _g_require_metatable_call, "");
        lua_rawset(L, -3);

        // hide the metatable from the Lua sandbox user
        lua_pushstring(L, "__metatable");
        lua_pushboolean(L, false);
        lua_rawset(L, -3);
        
        // registry to make sure modules
        // are only loaded once
        lua_pushstring(L, REQUIRE_LOADED_MODULES);
        lua_newtable(L);
        lua_rawset(L, -3);

        // used to log tables
        lua_pushstring(L, P3S_LUA_OBJ_METAFIELD_TYPE);
        lua_pushinteger(L, ITEM_TYPE_G_REQUIRE);
        lua_rawset(L, -3);
    }
    lua_setmetatable(L, -2);

    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
}

static int _g_require_metatable_index(lua_State *L) {
    LUAUTILS_STACK_SIZE(L)
    LUAUTILS_ERROR(L, "require has no property, call require(\"libraryName\")"); // returns
    LUAUTILS_STACK_SIZE_ASSERT(L, 0)
    return 0;
}

static int _g_require_metatable_newindex(lua_State *L) {
    LUAUTILS_STACK_SIZE(L)
    LUAUTILS_ERROR(L, "require is read-only"); // returns
    LUAUTILS_STACK_SIZE_ASSERT(L, 0)
    return 0;
}

bool lua_require(lua_State *const L, const char* moduleName) {
    LUAUTILS_STACK_SIZE(L)
    
    lua_getglobal(L, "require");
    const int type = LUA_GET_METAFIELD_AND_RETURN_TYPE(L, -1, "__call");
    if (type == LUA_TNIL) {
        lua_pop(L, 1);
        LUAUTILS_STACK_SIZE_ASSERT(L, 0)
        return false;
    }
    // -2 require
    // -1 call
    LUAUTILS_STACK_SIZE_ASSERT(L, 2)

    lua_pushvalue(L, -2);
    lua_pushstring(L, moduleName);
    // -4 require
    // -3 call
    // -2 require
    // -1 moduleName
    LUAUTILS_STACK_SIZE_ASSERT(L, 4)

    if (lua_pcall(L, 2, 1, 0) != LUA_OK) {
        if (lua_utils_isStringStrict(L, -1)) {
            lua_log_error_CStr(L, lua_tostring(L, -1));
        } else {
            lua_log_error_CStr(L, "can't require");
        }
        lua_pop(L, 1); // pop error
        lua_pop(L, 1); // pop require global table
        LUAUTILS_STACK_SIZE_ASSERT(L, 0);
        return false;
    }
    LUAUTILS_STACK_SIZE_ASSERT(L, 2)

    // -2 require
    // -1 returned value from pcall
    lua_remove(L, -2); // remove require global table, keep module
    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
    return true;
}

bool lua_require_verify_local_script(const std::string &script, const std::string &name) {
    std::map<std::string, std::string>::iterator it = hashes.find(name);
    if (it == hashes.end()) {
        return false;
    }
    std::string salted = std::string("nanskip") + script;
    std::string hash = md5(salted);
    if (it->second != hash) {
        return false;
    }
    return true;
}

static int _g_require_metatable_call(lua_State *L) {
    LUAUTILS_STACK_SIZE(L)
    LUAUTILS_ASSERT_ARG_TYPE(L, 1, ITEM_TYPE_G_REQUIRE)
    
    const int argCount = lua_gettop(L);
    if (argCount != 2 && argCount != 3) {
        LUAUTILS_ERROR(L, "require(moduleName) incorrect argument count");
    }
    if (lua_isstring(L, 2) == false) {
        LUAUTILS_ERROR(L, "require(moduleName) moduleName should be a string");
    }

    // check if System table has been provided
    const bool luaSystemAvailable = (argCount == 3 &&
                                     lua_type(L, 3) == LUA_TTABLE &&
                                     lua_utils_getObjectType(L, 3) == ITEM_TYPE_SYSTEM);

    std::string filename(lua_tostring(L, 2));

    // remove eventual http/https scheme prefix
    if (vx::str::hasPrefix(filename, "https://")) {
        filename = vx::str::trimPrefix(filename, "https://");
    }
    if (vx::str::hasPrefix(filename, "http://")) {
        filename = vx::str::trimPrefix(filename, "http://");
    }

    const bool isPublicModule = vx::str::hasPrefix(filename, "github.com");

    const bool isSystemModule = isPublicModule == false && vx::str::hasPrefix(filename, "system_");
    if (isSystemModule && luaSystemAvailable == false) {
        LUAUTILS_ERROR(L, "require() not allowed to import a system module here");
    }
    
    std::string filenameWithExt = filename;
    std::string hashPath = "";
    std::string moduleName = "";
    std::string ref = "";

    if (isPublicModule) {
        moduleName = filenameWithExt;
        ref = ".latest";

        std::vector<std::string> elements = vx::str::splitString(moduleName, ":");
        if (elements.size() >= 2) {
            ref = elements[elements.size() - 1];
            moduleName = vx::str::trimSuffix(moduleName, ":" + ref);
        }

        filenameWithExt = moduleName + "/.ref/" + ref + "/script.lua";
        hashPath = SCRIPTS_PATH + moduleName + "/.ref/" + ref + "/.bzhash";
    }

    // add extension if missing
    if (isPublicModule == false) {
        std::string ext = ".lua";
        if (filename.length() < ext.length() || filename.rfind(ext) != filename.size() - ext.size()) {
            filenameWithExt = filenameWithExt + ext;
        }
    }

    LUA_GET_METAFIELD(L, 1, REQUIRE_LOADED_MODULES);
    lua_pushstring(L, filename.c_str());
    if (lua_rawget(L, -2) == LUA_TNIL) {
        lua_pop(L, 2); // pop nil value + loaded modules table
    } else {
        // found loaded module! (or error string)
        lua_remove(L, -2); // remove loaded modules table

        if (lua_isstring(L, -1)) {
            lua_error(L);
        }
        LUAUTILS_STACK_SIZE_ASSERT(L, 1)
        return 1;
    }
    LUAUTILS_STACK_SIZE_ASSERT(L, 0)

    const std::string filepath(SCRIPTS_PATH + filenameWithExt);
    FILE *file = nullptr;
    FILE *hashFile = nullptr;

    if (isPublicModule) {
        file = vx::fs::openStorageFile(filepath.c_str());
    } else {
        file = vx::fs::openBundleFile(filepath.c_str());
    }

    if (file == nullptr) {
        LUAUTILS_ERROR_F(L, "require(): can't load %s", filename.c_str()); // returns
    }

    std::string script;
    if (vx::fs::getFileTextContentAsStringAndClose(file, script) == false) {
        LUAUTILS_ERROR_F(L, "require(): can't load %s", filename.c_str()); // returns
    }

    if (isPublicModule == false) {
        if (lua_require_verify_local_script(script, filenameWithExt) == false) {
            LUAUTILS_ERROR_F(L, "require(): can't load %s - hack detected, report sent", filename.c_str());
        }
    } else { // public module, verify hash file

        hashFile = vx::fs::openStorageFile(hashPath.c_str());
        if (hashFile == nullptr) {
            LUAUTILS_ERROR_F(L, "require(): can't load %s", filename.c_str()); // returns
        }

        std::string hash;
        if (vx::fs::getFileTextContentAsStringAndClose(hashFile, hash) == false) {
            LUAUTILS_ERROR_F(L, "require(): can't load %s", filename.c_str()); // returns
        }

        std::string computedHash = md5(std::string("nanskip") + "::" + moduleName + ":" + ref + "::" + script);

        if (hash != computedHash) {
            LUAUTILS_ERROR_F(L, "require(): can't load %s - hack detected, report sent", filename.c_str());
        }
    }

    lua_State* GL = lua_mainthread(L);
    lua_State* ML = lua_newthread(GL);
    lua_xmove(GL, L, 1); // moves ML on top of L's stack

    // lua_State* parentL;
    // lua_State* ML; // module state

    if (filenameWithExt == "avatar.lua" ||
        filenameWithExt == "badge_modal.lua" ||
        filenameWithExt == "badge.lua" ||
        filenameWithExt == "bundle.lua" ||
        filenameWithExt == "chat.lua" ||
        filenameWithExt == "coins.lua" ||
        filenameWithExt == "controls.lua" ||
        filenameWithExt == "creations.lua" ||
        filenameWithExt == "friends.lua" ||
        filenameWithExt == "item_details.lua" ||
        filenameWithExt == "item_grid.lua" ||
        filenameWithExt == "kvs.lua" ||
        filenameWithExt == "leaderboard.lua" ||
        filenameWithExt == "localevent.lua" ||
        filenameWithExt == "menu.lua" ||
        filenameWithExt == "player.lua" ||
        filenameWithExt == "profile.lua" ||
        filenameWithExt == "ui_outfit.lua" ||
        filenameWithExt == "item_grid.lua" ||
        filenameWithExt == "uikit.lua" ||
        filenameWithExt == "server_list.lua" ||
        filenameWithExt == "settings.lua" ||
        filenameWithExt == "signup.lua" ||
        filenameWithExt == "login.lua" ||
        filenameWithExt == "system_api.lua" ||
        filenameWithExt == "notifications.lua" ||
        filenameWithExt == "history.lua" ||
        filenameWithExt == "ui_avatar_editor.lua" ||
        filenameWithExt == "worlds.lua" ||
        filenameWithExt == "world_details.lua" ||
        filenameWithExt == "world.lua" ||
        filenameWithExt == "world_editor.lua" ||
        filenameWithExt == "username_form.lua" ||
        filenameWithExt == "verify_account_form.lua" ||
        filenameWithExt == "user.lua" ||
        filenameWithExt == "report.lua" ||
        filenameWithExt == "passkey.lua") {

        vxlog_debug("LOADING SYSTEM MODULE: %s", filenameWithExt.c_str());

        // SPECIAL SANDBOXING, MOUNTING SYSTEM TABLE
        lua_sandbox_system_globals(ML);
//        lua_newtable(ML);
//        {
//            lua_newtable(ML);
//            lua_utils_pushSystemGlobalsFromRegistry(ML);
//            lua_setfield(ML, -2, "__index");
//            lua_setreadonly(ML, -1, true);
//        }
//        lua_setmetatable(ML, -2);

        // we can set safeenv now although it's important to set it to false if code is loaded twice into the thread
//        lua_replace(ML, LUA_GLOBALSINDEX);
//        lua_setsafeenv(ML, LUA_GLOBALSINDEX, true);

    } else {
//        lua_State* GL = lua_mainthread(L);
//        parentL = GL;

        // luaL_sandboxthread(ML);
        lua_sandbox_globals(ML); // pretty similar to luaL_sandboxthread
    }

//    ML = lua_newthread(parentL);
//    lua_xmove(parentL, L, 1); // moves ML on top of L's stack
//
//    // new thread needs to have the globals sandboxed
//    luaL_sandboxthread(ML);

    LUAUTILS_STACK_SIZE_ASSERT(L, 1); // ML on top of L

    std::string bytecode = luau_compile(script);
    if (luau_load(ML, filename.c_str(), bytecode.data(), bytecode.size(), 0) == 0)
    {
        int status = lua_resume(ML, L, 0);
        if (status == 0)
        {
            if (lua_gettop(ML) == 0)
                lua_pushstring(ML, "module must return a value");
            else if (!lua_istable(ML, -1) && !lua_isfunction(ML, -1))
                lua_pushstring(ML, "module must return a table or function");
        }
        else if (status == LUA_YIELD)
        {
            lua_pushstring(ML, "module can not yield");
        }
        else if (!lua_isstring(ML, -1))
        {
            lua_pushstring(ML, "unknown error while running module");
        }
    }

    // there's now a return value on top of ML; L stack: _MODULES ML
    lua_xmove(ML, L, 1);
    lua_remove(L, -2); // remove ML


    LUAUTILS_STACK_SIZE_ASSERT(L, 1)

    // store module in registry for next calls
    LUA_GET_METAFIELD(L, 1, REQUIRE_LOADED_MODULES);
    lua_pushstring(L, filename.c_str());
    lua_pushvalue(L, -3);
    lua_rawset(L, -3);
    lua_pop(L, 1); // pop registry

    if (lua_isstring(L, -1)) {
        vxlog_error("%s", lua_tostring(L, -1));
        lua_error(L);
    }

    LUAUTILS_STACK_SIZE_ASSERT(L, 1)
    return 1;
}

int lua_require_snprintf(lua_State *const L, char *result, size_t maxLen, bool spacePrefix) {
    RETURN_VALUE_IF_NULL(L, 0)
    return snprintf(result, maxLen, "%s[require] ", spacePrefix ? " " : "");
}
