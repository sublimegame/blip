# Cubzh - Wasm

cmake_minimum_required(VERSION 3.28.3)

project(Cubzh)

# Build options
set(HEADLESS FALSE)
set(DEBUG FALSE)
set(DEBUG_FULL FALSE)
set(BGFX_DEBUG FALSE)

# Name of build targets supported by this CMake script
set(VX_APP_BUILD_TARGET_WASM "wasm")
set(VX_APP_BUILD_TARGET_WASM_DISCORD "wasm_discord")

# C++ language version
set(CMAKE_CXX_STANDARD 17)

# How to enable/disable those options: emcmake cmake ./build -DSINGLE_THREAD=ON
OPTION(SINGLE_THREAD "Whether Cubzh runs on a single thread" ON) # Enabled by default
OPTION(ENABLE_URL_REWRITE_FOR_DISCORD "HTTP URL rewrite for use in Discord Activity" OFF) # Disabled by default

# Define some preprocessor macros

if (VX_ENABLE_URL_REWRITE_FOR_DISCORD)
	# We are building the Discord version of the wasm app
    ADD_DEFINITIONS(-D__VX_APP_BUILD_TARGET="${VX_APP_BUILD_TARGET_WASM_DISCORD}")
    ADD_DEFINITIONS(-D__VX_REWRITE_URLS_FOR_DISCORD)
    ADD_DEFINITIONS(-D__VX_CUBZH_DISCORD_PROXY="1219341016524525670.discordsays.com")
else()
	# We are building the regular version of the wasm app (app.cu.bzh)
    ADD_DEFINITIONS(-D__VX_APP_BUILD_TARGET="${VX_APP_BUILD_TARGET_WASM}")
endif()

set(REPO_ROOT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../..")

set(COMMON_DIR "${REPO_ROOT_DIR}/common")
set(SBS_DIR "${COMMON_DIR}/VXLuaSandbox")
set(CUTILS_DIR "${COMMON_DIR}/CUtils")
set(LUAUTILS_DIR "${COMMON_DIR}/LuaUtils")
set(VX_DIR "${COMMON_DIR}/VXFramework")
set(VX_SERVER_DIR "${COMMON_DIR}/VXGameServer")
set(VXNETWORKING_DIR "${COMMON_DIR}/VXNetworking")
set(BX_DIR "${REPO_ROOT_DIR}/deps/bgfx/bx")
set(BGFX_DIR "${REPO_ROOT_DIR}/deps/bgfx/bgfx")
set(BIMG_DIR "${REPO_ROOT_DIR}/deps/bgfx/bimg")
set(BGFX_COMMON_DIR "${REPO_ROOT_DIR}/deps/bgfx/bgfx/examples/common")
set(DEPS_MINIAUDIO_DIR "${REPO_ROOT_DIR}/deps/miniaudio")
# Cubzh Core
set(CUBZH_CORE_DIR "${REPO_ROOT_DIR}/core")

# libluau
set(LIBLUAU_VERSION "_active_")
set(LIBLUAU_DIR "${REPO_ROOT_DIR}/deps/libluau/${LIBLUAU_VERSION}/prebuilt/source")
set(LIBLUAU_INC_DIR "${LIBLUAU_DIR}/include")
set(LIBLUAU_SRC_DIR "${LIBLUAU_DIR}/src")
# set(LIBLUAU_LIB_DIR "${LIBLUAU_DIR}/lib-${CMAKE_BUILD_TYPE}")

# Emscripten compiler flags,
# -s USE_ZLIB=1
#       include emscripten port for ZLIB
# -s USE_PTHREADS=1
#       enable support for POSIX threads used by std::thread, note: not supported by all browsers (check https://webassembly.org/roadmap/)
set(EMCC_COMPILER_FLAGS  "-s USE_ZLIB=1\
                          -Wbad-function-cast\
                          -Wcast-function-type\
                          ")
if (NOT SINGLE_THREAD)
        set(EMCC_COMPILER_FLAGS  "${EMCC_COMPILER_FLAGS}\
                                  -pthread\
                                  ")
endif()
if (DEBUG)
        set(EMCC_COMPILER_FLAGS  "${EMCC_COMPILER_FLAGS}\
                                  -g\
                                  -gsource-map\
                                  ")
        set(CMAKE_BUILD_TYPE "Debug")
        # disable tracking in DEBUG builds
        # add_compile_options(-DP3S_NO_METRICS)
else()
        set(CMAKE_BUILD_TYPE "Release")
endif()

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${EMCC_COMPILER_FLAGS}")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${EMCC_COMPILER_FLAGS}")

# Emscripten linker options common to all builds,
# -bind
#       allow C/C++ to JS bindings (see vx-wrapper.cpp)
# -s WASM=1
#       get wab assembly output (wasm) instead of javascript for older browsers
# -s NO_EXIT_RUNTIME=1
#       prevent runtime from stoping when main() exits
# -s EXPORTED_RUNTIME_METHODS=[...]
#       list of runtime functions that we want available from JS (eg. ccall to call C function from JS)
# -s EXPORTED_FUNCTIONS=[...]
#       list of compiled functions that we want available from JS (_main is necessary at minimum)
# --shell-file
#       run the build through given HTML template
# --pre-js
# --post-js
#       inject JS file before/after the emitted code
# -pthread
# -s PTHREAD_POOL_SIZE
# -s PTHREAD_POOL_SIZE_STRICT
#       enable support for POSIX threads, set predefined worker pool size
#               note (october 29th 2024), 2 threads: background OperationQueue, slow background OperationQueue
# -s PTHREAD_POOL_DELAY_LOAD=1
#       do not wait at startup for the worker pool to finish loading
# -s MALLOC=...
#       specify which malloc to use, see: https://emscripten.org/docs/tools_reference/settings_reference.html?highlight=environment#malloc
# -s MIN_WEBGL_VERSION=2
# -s MAX_WEBGL_VERSION=2
#       enable the creation of WebGL2 context (max), drop support for WebGL1 (min)
# -s ALLOW_MEMORY_GROWTH=1
#       we also could use -s INITIAL_MEMORY=X w/ a fixed amount
# -s ASYNCIFY=1
# -s ASYNCIFY_IMPORTS=[...]
#       enable the use of async JS from synchronous C/C++ on specified functions, by using Asyncify.handleAsync()
#       this also lets the Asyncify instrumentation know that nothing else needs to have the required overhead for asyncify
# -s ASYNCIFY_IGNORE_INDIRECT=1
#       disable some checks for performance, asserting no indirect calls can happen
# -s FETCH=1
#       allow using the Fetch API (XMLHttpRequest : GET, PUT, POST)
#
# For DEBUG build we additionally use,
# -s LLD_REPORT_UNDEFINED
#       get readable info on undefined symbols
# -s DEMANGLE_SUPPORT=1
#       get readable function names in stack traces
# -s ASSERTIONS=1
#       enable runtime checks, set to 2 to run additional tests
# -s SAFE_HEAP=1
#       enable memory access checks
# -s EXCEPTION_DEBUG=1
#       this apparently makes our Module.printErr be called on an exception (not used currently)
# -fexceptions
#       enable exception catching JS-side
#       note: -fwasm-exceptions is also available for native wasm exception handling, but supported only in Chrome currently (check https://webassembly.org/roadmap/)
#
# For RELEASE build,
# -O3
#       optimization level, code is minified at O2 and above
set (EMCC_LINKER_FLAGS "--bind\
                        -s WASM=1\
                        -s NO_EXIT_RUNTIME=1\
                        -s \"EXPORTED_RUNTIME_METHODS=['ccall']\"\
                        -s \"EXPORTED_FUNCTIONS=['_main', '_malloc']\"\
                        --pre-js ${CMAKE_CURRENT_SOURCE_DIR}/vx-wrapper-pre-js.js\
                        --post-js ${CMAKE_CURRENT_SOURCE_DIR}/vx-wrapper-post-js.js\
                        --preload-file bundle\
                        -lidbfs.js\
                        -lwebsocket.js\
                        -s MIN_WEBGL_VERSION=2\
                        -s MAX_WEBGL_VERSION=2\
                        -s ALLOW_MEMORY_GROWTH=1\
                        -s STACK_SIZE=131072\
                        -s ASYNCIFY=1\
                        -s \"ASYNCIFY_IMPORTS=['copy', 'paste']\"\
                        -s ASYNCIFY_IGNORE_INDIRECT=1\
                        -s FETCH=1\
                        ")
                        # -s ERROR_ON_WASM_CHANGES_AFTER_LINK\

if (SINGLE_THREAD)
        set(EMCC_LINKER_FLAGS  "${EMCC_LINKER_FLAGS}\
                                -s MALLOC=dlmalloc\
                                ")
else()
        set(EMCC_LINKER_FLAGS  "${EMCC_LINKER_FLAGS}\
                                -pthread\
                                -s PTHREAD_POOL_SIZE=2\
                                -s PTHREAD_POOL_SIZE_STRICT=2\
                                -s PTHREAD_POOL_DELAY_LOAD=1\
                                -s MALLOC=mimalloc\
                                ")
endif()

if (DEBUG)
        set(EMCC_LINKER_FLAGS  "${EMCC_LINKER_FLAGS}\
                                --shell-file ${CMAKE_CURRENT_SOURCE_DIR}/shell_production.html\
                                -s ASSERTIONS=1\
                                -O0\
                                ")

        if (DEBUG_FULL)
                set(EMCC_LINKER_FLAGS  "${EMCC_LINKER_FLAGS}\
                                        -s LLD_REPORT_UNDEFINED\
                                        -s SAFE_HEAP=2\
                                        -s EXCEPTION_DEBUG=1\
                                        -fexceptions\
                                        --source-map-base ./\
                                        ")
        endif()
else()
        set(EMCC_LINKER_FLAGS  "${EMCC_LINKER_FLAGS}\
                                --shell-file ${CMAKE_CURRENT_SOURCE_DIR}/shell_production.html\
                                -O3\
                                ")
endif()

# Output type, maps to emscripten -o <target>
set(CMAKE_EXECUTABLE_SUFFIX ".html")
set(EXECUTABLE_OUTPUT_PATH "output")

# Compile options
add_compile_options(
        # define custom macros
        -D__VX_PLATFORM_WASM
        -DCLIENT_ENV # client env runs game tick

        # bgfx compilation
        #-DBGFX_CONFIG_RENDERER_OPENGLES=31
        -DBGFX_CONFIG_RENDERER_OPENGLES=30
        -DBGFX_CONFIG_RENDERER_USE_EXTENSIONS=0
        -DBGFX_CONFIG_MULTITHREADED=0
        -DBGFX_GL_CONFIG_TEXTURE_READ_BACK_EMULATION=1
        -DBGFX_GL_CONFIG_BLIT_EMULATION=1

        # ImGui compilation
        -DIMGUI_USE_WCHAR32

        -DPARTICUBES_VERSION="0.1.18"
        -DCUBZH_BUILD="198"
)

if(SINGLE_THREAD)
    message(STATUS "Building in single-thread mode")
    add_compile_options(
        -D__VX_SINGLE_THREAD
    )
endif()

if(HEADLESS)
    message(STATUS "Building in headless mode")
    add_compile_options(
            -DP3S_CLIENT_HEADLESS
    )
endif()

if (BGFX_DEBUG)
    add_compile_options(
            -DBGFX_CONFIG_DEBUG
    )
endif()

### Libraries

# xptools
include(xptools.cmake)

# libluau (compile from source)
file(GLOB LIBLUAU_SOURCES
        CONFIGURE_DEPENDS
        ${LIBLUAU_SRC_DIR}/Ast/src/*.cpp ${LIBLUAU_SRC_DIR}/Ast/src/*.h
        ${LIBLUAU_SRC_DIR}/Compiler/src/*.cpp ${LIBLUAU_SRC_DIR}/Compiler/src/*.h
        ${LIBLUAU_SRC_DIR}/VM/src/*.cpp ${LIBLUAU_SRC_DIR}/VM/src/*.h)
add_library(libluau STATIC
        ${LIBLUAU_SOURCES})

# sbs
file(GLOB_RECURSE SBS_SOURCES
        CONFIGURE_DEPENDS
        ${SBS_DIR}/*.c ${SBS_DIR}/*.h ${SBS_DIR}/*.cpp ${SBS_DIR}/*.hpp)
add_library(sbs STATIC
        ${SBS_SOURCES})

# engine, no recurse to exclude tests/
file(GLOB ENGINE_SOURCES
        CONFIGURE_DEPENDS
        LIST_DIRECTORIES false
        ${CUBZH_CORE_DIR}/*.c ${CUBZH_CORE_DIR}/*.h
        ${CUTILS_DIR}/*.c ${CUTILS_DIR}/*.cpp ${CUTILS_DIR}/*.h
        ${LUAUTILS_DIR}/*.c ${LUAUTILS_DIR}/*.h)
add_library(engine STATIC
        ${ENGINE_SOURCES})

# vx framework
file(GLOB_RECURSE VX_SOURCES
        CONFIGURE_DEPENDS
        ${VX_DIR}/*.c ${VX_DIR}/*.cpp ${VX_DIR}/*.h ${VX_DIR}/*.sc ${VX_DIR}/*.hpp
        ${VX_SERVER_DIR}/*.hpp ${VX_SERVER_DIR}/*.cpp
        ${VXNETWORKING_DIR}/*.hpp ${VXNETWORKING_DIR}/*.cpp)

list(REMOVE_ITEM VX_SOURCES "${VX_SERVER_DIR}/main.cpp")
add_library(vx STATIC
        ${VX_SOURCES})

# vx framework - extract unique paths for includes
set(VX_DIRS "")
foreach(VX_FILE IN ITEMS ${VX_SOURCES})
    get_filename_component(VX_PATH ${VX_FILE} PATH)
    list(APPEND VX_DIRS ${VX_PATH})
endforeach()
list(REMOVE_DUPLICATES VX_DIRS)

if(NOT HEADLESS)
        # bgfx, bx, bimg, dear-imgui
        include(bgfx.cmake)
        include(cmake/3rdparty/dear-imgui.cmake)

        # bgfx rendering backend for dear-imgui
        set(BGFX_IMGUI_SOURCES "")
        foreach(DIR "${BGFX_COMMON_DIR}/imgui")
        file(GLOB GLOB_SOURCES ${DIR}/*.c ${DIR}/*.cpp ${DIR}/*.h ${DIR}/*.sc)
        list(APPEND BGFX_IMGUI_SOURCES ${GLOB_SOURCES})
        endforeach()
        add_library(bgfx-imgui STATIC
                ${BGFX_IMGUI_SOURCES})
endif()

# lpng
include(lpng.cmake)

### Includes

include_directories(
        ${LIBLUAU_INC_DIR}
        ${CUBZH_CORE_DIR}
        ${SBS_DIR}
        ${VX_DIRS}
        ${BGFX_DIR}/3rdparty
        ${BGFX_DIR}/3rdparty/dear-imgui
        ${BGFX_COMMON_DIR}
        ${VXTOOLS_INCLUDE_DIR}
        ${VXTOOLS_DEPS_DIR}
        ${LPNG_DIR}
        ${DEPS_MINIAUDIO_DIR}
        ${REPO_ROOT_DIR}/deps/gif_load)

# Debug: display include directories
#[[get_property(dirs DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY INCLUDE_DIRECTORIES)
# foreach(dir ${dirs})
#     message(STATUS "dir='${dir}'")
# endforeach()]]

### Linking

target_link_libraries(engine
        libluau)

target_link_libraries(sbs
        libluau vx)

if(NOT HEADLESS)
    target_link_libraries(bgfx-imgui
            bx bimg bgfx dear-imgui)

    target_link_libraries(vx
            bgfx-imgui)
endif()

target_link_libraries(vx
        engine
        sbs
        xptools
        lpng
)

### Executable

add_executable(cubzh vx-wrapper.cpp)
target_link_libraries(cubzh vx)
set_target_properties(cubzh PROPERTIES LINK_FLAGS "${EMCC_LINKER_FLAGS}")
