//
//  lua_constants.h
//  Cubzh
//
//  Created by Gaetan de Villele on 29/07/2019.
//  Copyright © 2019 voxowl. All rights reserved.
//

#pragma once

// MARK: - Lua globals names

#define LUA_G_REQUIRE               "require"

#define P3S_LUA_G_IS_CLIENT         "IsClient"         // bool (true on Client)
#define P3S_LUA_G_IS_SERVER         "IsServer"         // bool (true on Server)
#define P3S_LUA_G_CLIENT            "Client"           // name of global lua table
#define P3S_LUA_G_SERVER            "Server"           // name of global lua table
#define P3S_LUA_G_SHARED            "Shared"           // name of global lua table
#define P3S_LUA_G_EVENT             "Event"            // name of global lua table
#define P3S_LUA_G_WORLD             "World"            // name of global lua table
#define P3S_LUA_G_MAP               "Map"              // name of global lua table
#define P3S_LUA_G_BLOCK             "Block"            // name of global lua table
#define P3S_LUA_G_FACE              "Face"             // name of global lua table
#define P3S_LUA_G_TYPE              "Type"             // name of global lua table
#define P3S_LUA_G_PLAYERS           "Players"          // name of global lua table
#define P3S_LUA_G_OTHER_PLAYERS     "OtherPlayers"     // name of global lua table
#define P3S_LUA_G_COLOR             "Color"            // name of global lua table
#define P3S_LUA_G_INPUTS		    "Inputs"		   // name of global lua table
#define P3S_LUA_G_TIME              "Time"             // name of global lua table
#define LUA_G_TIMECYCLE             "TimeCycle"        // name of global lua table - DEPRECATED
#define LUA_G_TIME_CYCLE_MARK       "TimeCycleMark"    // name of global lua table - DEPRECATED
#define P3S_LUA_G_NUMBER3           "Number3"          // name of global lua table
#define P3S_LUA_G_ORIENTATION       "Orientation"      // name of global lua table
#define P3S_LUA_G_SHAPE             "Shape"            // name of global lua table
#define P3S_LUA_G_MUTABLESHAPE      "MutableShape"     // name of global lua table
#define P3S_LUA_G_POINT             "Point"            // name of global lua table
#define P3S_LUA_G_OBJECT            "Object"           // name of global lua table
#define P3S_LUA_G_BOX               "Box"              // name of global lua table
#define P3S_LUA_G_DEV               "Dev"              // name of global lua table
#define P3S_LUA_G_PRIVATE           "Private"          // name of global lua table
#define P3S_LUA_G_RAY               "Ray"              // name of global lua table
#define P3S_LUA_G_SCREEN            "Screen"           // name of global lua table
#define P3S_LUA_G_COLLISIONGROUPS   "CollisionGroups"  // name of global lua table
#define P3S_LUA_G_KEYVALUESTORE     "KeyValueStore"    // name of global lua table
#define P3S_LUA_G_TIMER             "Timer"            // name of global lua table
#define P3S_LUA_G_LIGHT             "Light"            // name of global lua table
#define P3S_LUA_G_LIGHTTYPE         "LightType"        // name of global lua table
#define P3S_LUA_G_LEADERBOARD       "Leaderboard"      // name of global lua table
#define P3S_LUA_G_DEFAULT_COLORS    "DefaultColors"    // name of global lua table
#define LUA_G_HTTP                  "HTTP"             // name of global lua table
#define LUA_G_JSON                  "JSON"             // name of global lua table
#define LUA_G_URL                   "URL"              // name of global lua table
#define LUA_G_DATA                  "Data"             // name of global lua table
#define LUA_G_ENVIRONMENT           "Environment"      // name of global lua table
#define LUA_G_FILE                  "File"             // name of global lua table
#define LUA_G_CAMERA                "Camera"           // name of global lua table
#define LUA_G_PROJECTIONMODE        "ProjectionMode"   // name of global lua table
#define LUA_G_AUDIOSOURCE           "AudioSource"      // name of global lua table
#define LUA_G_AUDIOLISTENER         "AudioListener"    // name of global lua table
#define LUA_G_TEXT                  "Text"
#define LUA_G_TEXTTYPE              "TextType"
#define LUA_G_FONT                  "Font"
#define LUA_G_NUMBER2               "Number2"
#define LUA_G_ASSETS                "Assets"
#define LUA_G_PALETTE               "Palette"
#define LUA_G_ASSETTYPE             "AssetType"
#define LUA_G_PHYSICSMODE           "PhysicsMode"
#define LUA_G_LOCALEVENT            "LocalEvent"
#define LUA_G_MENU                  "Menu"
#define LUA_G_QUAD                  "Quad"
#define LUA_G_AI                    "AI"
#define LUA_G_POINTER_EVENT         "PointerEvent"
#define LUA_G_ANIMATION             "Animation"
#define P3S_LUA_G_ROTATION          "Rotation"
#define P3S_LUA_G_SYSTEM            "System"           // name of global lua table
#define LUA_G_MODULES               "Modules"
#define P3S_LUA_G_DEPRECATED        "Deprecated"
#define LUA_G_MESH                  "Mesh"

// MARK: - Lua metatables names

/// key used for storing the metatable for the "Time" global table, in the Lua registry
#define P3S_SBS_REG_TIMECYCLE_METATABLE "p3s_sbs_time_cycle_mt"

/// key used for storing the readonly metatable in the Lua registry

/// key used for storing the metatable for the "Input" global table in the Lua registry
#define P3S_SBS_REG_G_INPUTS_METATABLE "p3s_sbs_g_inputs_mt"

// MARK: - Lua metafields names

///
#define P3S_LUA_OBJ_METAFIELD_TYPE "__type"

/// Item types
typedef enum LUA_ITEM_TYPE {
    ITEM_TYPE_NONE = -1,
    ITEM_TYPE_MAP = 1,
    ITEM_TYPE_PLAYER,
    ITEM_TYPE_REMOVED_PLAYER,
    ITEM_TYPE_BLOCK,
    ITEM_TYPE_EVENT,
    ITEM_TYPE_G_EVENT,
    ITEM_TYPE_MENU_VIEW,
    ITEM_TYPE_MENU_BUTTON,
    ITEM_TYPE_MAGICEMPTYTABLE,
    ITEM_TYPE_SERVER,
    ITEM_TYPE_G_BLOCK,
    ITEM_TYPE_IMPACT,
    ITEM_TYPE_ITEMS,
    ITEM_TYPE_ITEM,
    ITEM_TYPE_NODE,
    ITEM_TYPE_LABEL,
    ITEM_TYPE_CAMERA, // global "Camera" table is also an instance (to main camera)
    ITEM_TYPE_COLOR,
    ITEM_TYPE_G_COLOR,
    ITEM_TYPE_G_TIME,
    ITEM_TYPE_G_INPUT,
    ITEM_TYPE_G_POINTER,
    ITEM_TYPE_G_DIRECTION,
    ITEM_TYPE_NUMBER3,
    ITEM_TYPE_G_NUMBER3,
    ITEM_TYPE_MAPBLOCKS,
    ITEM_TYPE_BLOCKPROPERTIES,
    ITEM_TYPE_G_POINTEREVENT,
    ITEM_TYPE_POINTEREVENT,
    ITEM_TYPE_G_SHAPE,
    ITEM_TYPE_SHAPE,
    ITEM_TYPE_G_CLOUDS,
    ITEM_TYPE_G_FOG,
    ITEM_TYPE_G_BUTTON,
    ITEM_TYPE_BUTTON,
    ITEM_TYPE_G_MUTABLESHAPE,
    ITEM_TYPE_MUTABLESHAPE,
    ITEM_TYPE_G_OBJECT,
    ITEM_TYPE_OBJECT,
    ITEM_TYPE_CONFIG,
    ITEM_TYPE_PALETTE,
    ITEM_TYPE_DEV,
    ITEM_TYPE_SYSTEM,
    ITEM_TYPE_PRIVATE,
    ITEM_TYPE_G_RAY,
    ITEM_TYPE_RAY,
    
    ITEM_TYPE_G_COLLISIONGROUPS,
    ITEM_TYPE_COLLISIONGROUPS,

    ITEM_TYPE_G_ANCHOR,
    ITEM_TYPE_ANCHOR,

    ITEM_TYPE_G_FACE,
    ITEM_TYPE_FACE,
    
    ITEM_TYPE_G_COLORPICKER,
    ITEM_TYPE_COLORPICKER,
    ITEM_TYPE_G_POINT,
    ITEM_TYPE_POINT,

    ITEM_TYPE_G_BOX,
    ITEM_TYPE_BOX,
    
    ITEM_TYPE_WORLD,
    
    ITEM_TYPE_G_KVSTORE,
    ITEM_TYPE_KVSTORE,

    ITEM_TYPE_G_PLAYERS,
    ITEM_TYPE_G_OTHERPLAYERS,
    
    ITEM_TYPE_G_CLIENT,
    
    ITEM_TYPE_SERIALIZABLE_TABLE,

    ITEM_TYPE_G_TIMER,
    ITEM_TYPE_TIMER,

    ITEM_TYPE_G_SCREEN,

    ITEM_TYPE_G_HTTP,
    ITEM_TYPE_HTTP_REQUEST,
    ITEM_TYPE_HTTP_RESPONSE,

    ITEM_TYPE_G_JSON,

    ITEM_TYPE_G_LIGHT,
    ITEM_TYPE_LIGHT,
    ITEM_TYPE_G_LIGHTTYPE,
    ITEM_TYPE_LIGHTTYPE,

    ITEM_TYPE_G_DATA,
    ITEM_TYPE_DATA,
    ITEM_TYPE_G_URL,
    ITEM_TYPE_G_FILE,

    ITEM_TYPE_G_PROJECTIONMODE,
    ITEM_TYPE_PROJECTIONMODE,

    ITEM_TYPE_G_AUDIOSOURCE,
    ITEM_TYPE_AUDIOSOURCE,
    ITEM_TYPE_G_AUDIOLISTENER,

    ITEM_TYPE_G_ENVIRONMENT,
    ITEM_TYPE_G_REQUIRE,

    ITEM_TYPE_DEFAULT_COLORS,
    ITEM_TYPE_G_DEFAULT_COLORS,

    ITEM_TYPE_G_TEXT,
    ITEM_TYPE_TEXT,
    ITEM_TYPE_G_TEXTTYPE,
    ITEM_TYPE_TEXTTYPE,

    ITEM_TYPE_NUMBER2,
    ITEM_TYPE_G_NUMBER2,
    
    ITEM_TYPE_G_ASSETS,
    ITEM_TYPE_ASSETS_REQUEST,

    ITEM_TYPE_G_PALETTE,

    ITEM_TYPE_G_ASSETTYPE,
    ITEM_TYPE_ASSETTYPE,

    ITEM_TYPE_G_PHYSICSMODE,
    ITEM_TYPE_PHYSICSMODE,

    ITEM_TYPE_G_QUAD,
    ITEM_TYPE_QUAD,

    ITEM_TYPE_G_SKY,

    ITEM_TYPE_G_ANIMATION,
    ITEM_TYPE_ANIMATION,

    ITEM_TYPE_G_ROTATION,
    ITEM_TYPE_ROTATION,

    ITEM_TYPE_G_MODULES,

    ITEM_TYPE_G_OSTEXTINPUT,

    ITEM_TYPE_DEPRECATED,

    ITEM_TYPE_G_FONT,
    ITEM_TYPE_FONT,

    ITEM_TYPE_OBJECT_DESTROYED,
    ITEM_TYPE_NOT_AN_OBJECT,

    ITEM_TYPE_G_MESH,
    ITEM_TYPE_MESH
} LUA_ITEM_TYPE;
