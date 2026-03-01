# Cubzh - Android

cmake_minimum_required(VERSION 3.31.0)

project(Cubzh)

# Project number
# Note: increment & push to have other developers' IDE perform a sync w/o the need to "touch" this file
set(DUMMY 17)

# Build options
set(HEADLESS FALSE)
set(BGFX_DEBUG FALSE)

set(VX_APP_BUILD_TARGET_ANDROID_GOOGLEPLAY "android_googleplay")

# C++ language version
set(CMAKE_CXX_STANDARD 17)

# CPU architecture detection
include(architectures.cmake)
# Android CPU arch names: arm64-v8a, armeabi-v7a, x86, x86_64
set(ACTIVE_ANDROID_ARCH ${CMAKE_ANDROID_ARCH_ABI})
# Voxowl CPU arch names: arm64, arm, x86, x86_64
convert_android_arch_to_arch(${ACTIVE_ANDROID_ARCH} ACTIVE_ARCH)
#message(WARNING "🛠️ACTIVE_ANDROID_ARCH: ${ACTIVE_ANDROID_ARCH}")
#message(WARNING "🛠️ACTIVE_ARCH: ${ACTIVE_ARCH}")

# Define macro __VX_APP_BUILD_TARGET
ADD_DEFINITIONS(-D__VX_APP_BUILD_TARGET="${VX_APP_BUILD_TARGET_ANDROID_GOOGLEPLAY}")

# Define .env variables as preprocessor defines (set by build.gradle, prefixed w/ ENVVAR_)
get_cmake_property(vars CACHE_VARIABLES)
foreach (v ${vars})
    message(STATUS "  ${v} = ${${v}}")
    if(v MATCHES "^ENVVAR_")
        set(value "${${v}}")
        string(REGEX REPLACE "^ENVVAR_" "" key "${v}")

        # Quote if not numeric
        if(value MATCHES "^[0-9]+$")
            add_definitions(-D${key}=${value})
        else()
            add_definitions(-D${key}=\"${value}\")
        endif()

        #message(STATUS "Define ${key} = ${value}")
    endif()
endforeach()

set(REPO_ROOT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../../../../..")

set(CUBZH_CORE_DIR "${REPO_ROOT_DIR}/core")
set(COMMON_DIR "${REPO_ROOT_DIR}/common")
set(SBS_DIR "${COMMON_DIR}/VXLuaSandbox")
set(LUAUTILS_DIR "${COMMON_DIR}/LuaUtils")
set(VX_DIR "${COMMON_DIR}/VXFramework")
set(VX_SERVER_DIR "${COMMON_DIR}/VXGameServer")
set(VXNETWORKING_DIR "${COMMON_DIR}/VXNetworking")

set(BX_DIR "${REPO_ROOT_DIR}/deps/bgfx/bx")
set(BGFX_DIR "${REPO_ROOT_DIR}/deps/bgfx/bgfx")
set(BIMG_DIR "${REPO_ROOT_DIR}/deps/bgfx/bimg")
set(BGFX_COMMON_DIR "${REPO_ROOT_DIR}/deps/bgfx/bgfx/examples/common")
set(FT_DIR "${REPO_ROOT_DIR}/deps/freetype")
set(HB_DIR "${REPO_ROOT_DIR}/deps/harfbuzz")

# libluau
# ----------------------------------------
set(LUAU_VERSION "_active_")
set(LUAU_DIR "${REPO_ROOT_DIR}/deps/libluau/${LUAU_VERSION}/prebuilt/android/${ACTIVE_ARCH}")
# Set LUAU_BUILD_TYPE based on CMAKE_BUILD_TYPE (Release, RelWithDebInfo, Debug, MinSizeRel, ...)
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(LUAU_BUILD_TYPE "Debug")
else()
    set(LUAU_BUILD_TYPE "Release")
    add_compile_options(
            -O3
    )
endif()
add_library(luau STATIC IMPORTED)
set_property(TARGET luau PROPERTY
        IMPORTED_LOCATION ${LUAU_DIR}/lib-${LUAU_BUILD_TYPE}/libluau.a)

# libpng
# ----------------------------------------
set(LIBPNG_DIR "${REPO_ROOT_DIR}/deps/libpng/_active_/prebuilt/android/${ACTIVE_ARCH}")
# Set LIBPNG_BUILD_TYPE based on CMAKE_BUILD_TYPE (Release, RelWithDebInfo, Debug, MinSizeRel, ...)
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(LIBPNG_BUILD_TYPE "Debug")
else()
    set(LIBPNG_BUILD_TYPE "Release")
endif()
add_library(png STATIC IMPORTED)
set_property(TARGET png PROPERTY
        IMPORTED_LOCATION ${LIBPNG_DIR}/lib-${LIBPNG_BUILD_TYPE}/libpng.a)

# libssl
set(LIBSSL_DIR "${REPO_ROOT_DIR}/deps/libssl/android/${ACTIVE_ANDROID_ARCH}")

# libwebsockets
set(LIBWEBSOCKETS_DIR "${REPO_ROOT_DIR}/deps/libwebsockets/android/${ACTIVE_ANDROID_ARCH}")

# Compile options
add_compile_options(
        -D_ANDROID
        -D__VX_PLATFORM_ANDROID

        # Cubzh Core/vx compilation
        -DCLIENT_ENV
        -D__VX_USE_LIBWEBSOCKETS
        
        # bgfx compilation
        # note: to run in an AVD with GLES2.0
        # - comment BGFX_CONFIG_RENDERER_OPENGLES=31
        # - set BGFX_CONFIG_RENDERER_USE_EXTENSIONS=0
        # - set setEGLContextClientVersion(2) in VXGLSurfaceView::init
        # - comment the BGFX_CAPS_INDEX32 assert in bgfx.cpp:createIndexBuffer(const Memory* _mem, uint16_t _flags)
        -DBGFX_CONFIG_RENDERER_OPENGLES=31
        -DBGFX_CONFIG_RENDERER_USE_EXTENSIONS=1
        -DBGFX_CONFIG_MULTITHREADED=1
        -DBGFX_GL_CONFIG_TEXTURE_READ_BACK_EMULATION=1
        -DBGFX_GL_CONFIG_BLIT_EMULATION=1

        # ImGui compilation
        -DIMGUI_USE_WCHAR32
)

if(HEADLESS)
    message(STATUS "/!\ Building in headless mode")
    add_compile_options(
            -DP3S_CLIENT_HEADLESS
    )
endif()

if (BGFX_DEBUG)
    add_compile_options(
            -DBGFX_CONFIG_DEBUG
    )
endif()

# Creates and names a library, sets it as either STATIC
# or SHARED, and provides the relative paths to its source code.
# You can define multiple libraries, and CMake builds them for you.
# Gradle automatically packages shared libraries with your APK.

# sbs
file(GLOB_RECURSE SBS_SOURCES
        CONFIGURE_DEPENDS
        ${SBS_DIR}/*.c ${SBS_DIR}/*.h ${SBS_DIR}/*.cpp ${SBS_DIR}/*.hpp)
add_library(sbs STATIC
        ${SBS_SOURCES})

# Cubzh Core, no recurse to exclude tests/
file(GLOB CUBZH_CORE_SOURCES
        CONFIGURE_DEPENDS
        LIST_DIRECTORIES false
        ${CUBZH_CORE_DIR}/*.c ${CUBZH_CORE_DIR}/*.h
        ${LUAUTILS_DIR}/*.c ${LUAUTILS_DIR}/*.h)
add_library(cubzhcore STATIC
        ${CUBZH_CORE_SOURCES})

# vx wrapper
add_library(vx-wrapper SHARED
        vx-wrapper.cpp)

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

# xptools
include(xptools.cmake)

# ssl/crypto
add_library(crypto STATIC IMPORTED)
set_property(TARGET crypto PROPERTY
        IMPORTED_LOCATION ${LIBSSL_DIR}/lib/libcrypto.a)
add_library(ssl STATIC IMPORTED)
set_property(TARGET ssl PROPERTY
        IMPORTED_LOCATION ${LIBSSL_DIR}/lib/libssl.a)
target_link_libraries(ssl INTERFACE
        crypto)

# freetype + harfbuzz
add_subdirectory(${FT_DIR} ${CMAKE_BINARY_DIR}/freetype)
set(HB_HAVE_FREETYPE ON CACHE BOOL "Enable FreeType support in HarfBuzz")
add_subdirectory(${HB_DIR} ${CMAKE_BINARY_DIR}/harfbuzz)

# lws
add_library(lws STATIC IMPORTED)
set_property(TARGET lws PROPERTY
        IMPORTED_LOCATION ${LIBWEBSOCKETS_DIR}/lib/libwebsockets.a)

# includes

include_directories(
        ${LUAU_DIR}/include
        ${LIBPNG_DIR}/include
        ${CUBZH_CORE_DIR}
        ${SBS_DIR}
        ${VX_DIRS}
        ${BGFX_DIR}/3rdparty
        ${BGFX_DIR}/3rdparty/dear-imgui
        ${BGFX_COMMON_DIR}
        ${VXTOOLS_INCLUDE_DIR}
        ${LIBSSL_DIR}/include
        ${LIBWEBSOCKETS_DIR}/include
        ${REPO_ROOT_DIR}/deps/gif_load
        ${FT_DIR}/include
        ${HB_DIR}/src)

# Debug: display include directories
#[[get_property(dirs DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY INCLUDE_DIRECTORIES)
foreach(dir ${dirs})
    message(STATUS "dir='${dir}'")
endforeach()]]

# Specifies libraries CMake should link to your target library. You
# can link multiple libraries, such as libraries you define in this
# build script, prebuilt third-party libraries, or system libraries.

target_link_libraries(cubzhcore
        luau xptools log z)

target_link_libraries(sbs
        luau vx)

if(NOT HEADLESS)
    target_link_libraries(bgfx-imgui
            bx bimg bgfx dear-imgui)

    target_link_libraries(vx
            bgfx-imgui)
endif()

target_link_libraries(vx
        cubzhcore sbs
        android log GLESv3 EGL
        png lws ssl crypto
        freetype harfbuzz)

target_link_libraries(vx-wrapper
        vx)
