#
# Cubzh Gameserver
#

# Sets the minimum version of CMake required to build the native library.
cmake_minimum_required(VERSION 3.20)

# Set default build type to Release if not specified
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Choose the type of build (Debug/Release)" FORCE)
endif()

# Set the cross-compilation flags before the project() command
set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR x86_64)

# Specify the cross-compiler
set(CMAKE_C_COMPILER "clang")
set(CMAKE_CXX_COMPILER "clang++")

# Add the target architecture flags
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} --target=x86_64-linux-gnu")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} --target=x86_64-linux-gnu")

project("Cubzh Gameserver")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# CPU architecture detection
include(architectures.cmake)
# CPU architecture for which the gameserver is to be compiled
# $ENV{CUBZH_TARGETARCH} is set to amd64 or arm64
# Voxowl CPU arch names: arm64, arm, x86, x86_64
convert_docker_arch_to_arch($ENV{CUBZH_TARGETARCH} CUBZH_TARGET_ARCH)

set(VX_APP_BUILD_TARGET_LINUX_GAMESERVER "linux_gameserver")

# Define macro __VX_APP_BUILD_TARGET
ADD_DEFINITIONS(-D__VX_APP_BUILD_TARGET="${VX_APP_BUILD_TARGET_LINUX_GAMESERVER}")

set(REPO_ROOT_DIR "${CMAKE_CURRENT_BINARY_DIR}/../..")

set(CUBZH_DEPS_DIR "${REPO_ROOT_DIR}/deps")

# --------------------------------------------------
# Dependencies
# --------------------------------------------------

# libz (prebuilt)
set(LIBZ_DIR "${CUBZH_DEPS_DIR}/libz/linux-x86_64")
set(LIBZ_INCLUDE_DIR "${LIBZ_DIR}/include")
set(LIBZ_LIBS_DIR "${LIBZ_DIR}/lib")
find_library(LIBZ z ${LIBZ_LIBS_DIR})

# libssl / libcrypto (prebuilt)
set(LIBSSL_DIR "${REPO_ROOT_DIR}/deps/libssl/linux/$ENV{CUBZH_TARGETARCH}")
set(LIBSSL_INCLUDE_DIR "${LIBSSL_DIR}/include")
set(LIBSSL_LIBS_DIR "${LIBSSL_DIR}/libs")
find_library(LIBCRYPTO_PATH crypto ${LIBSSL_LIBS_DIR})
message(STATUS "libcrypto path: ${LIBCRYPTO_PATH}")
add_library(crypto STATIC IMPORTED)
set_property(TARGET crypto PROPERTY IMPORTED_LOCATION ${LIBCRYPTO_PATH})
find_library(LIBSSL_PATH ssl ${LIBSSL_LIBS_DIR})
message(STATUS "libssl path: ${LIBSSL_PATH}")
add_library(ssl STATIC IMPORTED)
set_property(TARGET ssl PROPERTY IMPORTED_LOCATION ${LIBSSL_PATH})

# libwebsockets (prebuilt)
set(LIBWEBSOCKETS_DIR "${REPO_ROOT_DIR}/deps/libwebsockets/linux/$ENV{CUBZH_TARGETARCH}")
set(LIBWEBSOCKETS_INCLUDE_DIR "${LIBWEBSOCKETS_DIR}/include")
set(LIBWEBSOCKETS_LIBS_DIR "${LIBWEBSOCKETS_DIR}/libs")
find_library(LIBWEBSOCKETS_PATH websockets ${LIBWEBSOCKETS_LIBS_DIR})
message(STATUS "libwebsockets path: ${LIBWEBSOCKETS_PATH}")
add_library(websockets STATIC IMPORTED)
set_property(TARGET websockets PROPERTY IMPORTED_LOCATION ${LIBWEBSOCKETS_PATH})

# libminiaudio
set(LIBMINIAUDIO_INC "${REPO_ROOT_DIR}/deps/miniaudio")

# libluau
# ----------------------------------------
set(LIBLUAU_VERSION "_active_")
set(LIBLUAU_DIR "${REPO_ROOT_DIR}/deps/libluau/${LIBLUAU_VERSION}/prebuilt/linux/${CUBZH_TARGET_ARCH}")
add_library(luau STATIC IMPORTED)
set_property(TARGET luau PROPERTY IMPORTED_LOCATION ${LIBLUAU_DIR}/lib-${CMAKE_BUILD_TYPE}/libluau.a)

# libpng
# ----------------------------------------
set(LIBPNG_DIR "${REPO_ROOT_DIR}/deps/libpng/_active_/prebuilt/linux/${CUBZH_TARGET_ARCH}")
# Set LIBPNG_BUILD_TYPE based on CMAKE_BUILD_TYPE (Release, RelWithDebInfo, Debug, MinSizeRel, ...)
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(LIBPNG_BUILD_TYPE "Debug")
else()
    set(LIBPNG_BUILD_TYPE "Release")
endif()
add_library(png STATIC IMPORTED)
set_property(TARGET png PROPERTY
        IMPORTED_LOCATION ${LIBPNG_DIR}/lib-${LIBPNG_BUILD_TYPE}/libpng.a)

# bgfx
set(BGFX_3RD_PARTY_DIR "${REPO_ROOT_DIR}/deps/bgfx/bgfx/3rdparty")

# --------------------------------------------------
# Sources
# --------------------------------------------------

set(COMMON_DIR "${REPO_ROOT_DIR}/common")
set(SBS_DIR "${COMMON_DIR}/VXLuaSandbox")
set(VX_DIR "${COMMON_DIR}/VXFramework")
set(VX_SERVER_DIR "${COMMON_DIR}/VXGameServer")
set(VXNETWORKING_DIR "${COMMON_DIR}/VXNetworking")

# CUBZH CORE
set(CUBZH_CORE_DIR "${REPO_ROOT_DIR}/core")

set(SOURCE_FILES "")

# Compile options
add_compile_options(
        -DDEBUG
        -DP3S_CLIENT_HEADLESS
        -DONLINE_GAMESERVER
        -D__VX_USE_LIBWEBSOCKETS
        -D__VX_PLATFORM_LINUX
)

# Creates and names a library, sets it as either STATIC
# or SHARED, and provides the relative paths to its source code.
# You can define multiple libraries, and CMake builds them for you.
# Gradle automatically packages shared libraries with your APK.

# xptools
include(xptools.cmake)

# sbs
file(GLOB_RECURSE SBS_SOURCES
        CONFIGURE_DEPENDS
        ${SBS_DIR}/*.c ${SBS_DIR}/*.h ${SBS_DIR}/*.cpp ${SBS_DIR}/*.hpp)
add_library(sbs STATIC
        ${SBS_SOURCES})
set(SOURCE_FILES ${SOURCE_FILES} ${SBS_SOURCES})

# Cubzh Core
file(GLOB CUBZH_CORE_SOURCES
        CONFIGURE_DEPENDS
        ${CUBZH_CORE_DIR}/*.c) # ${CUBZH_CORE_DIR}/*.h
add_library(cubzh_core STATIC
        ${CUBZH_CORE_SOURCES})
set(SOURCE_FILES ${SOURCE_FILES} ${CUBZH_CORE_SOURCES})

# vx framework
file(GLOB VX_SOURCES
        CONFIGURE_DEPENDS
        ${VX_DIR}/*.c ${VX_DIR}/*.cpp ${VX_DIR}/*.h ${VX_DIR}/*.sc ${VX_DIR}/*.hpp ${VX_DIR}/TextEditor/*.h ${VX_DIR}/TextEditor/*.cpp
        ${VX_SERVER_DIR}/*.hpp ${VX_SERVER_DIR}/*.cpp
        ${VXNETWORKING_DIR}/*.hpp ${VXNETWORKING_DIR}/*.cpp
        ${VXTOOLS_DEPS_DIR}/*.c ${VXTOOLS_DEPS_DIR}/*.cpp)
add_library(vx STATIC
        ${VX_SOURCES})
set(SOURCE_FILES ${SOURCE_FILES} ${VX_SOURCES})

# vx framework - extract unique paths for includes
set(VX_DIRS "")
foreach(VX_FILE IN ITEMS ${VX_SOURCES})
    get_filename_component(VX_PATH ${VX_FILE} PATH)
    list(APPEND VX_DIRS ${VX_PATH})
endforeach()
list(REMOVE_DUPLICATES VX_DIRS)

# Headers search paths
include_directories(
        ${LIBLUAU_DIR}/include
        ${LIBPNG_DIR}/include
        ${BGFX_3RD_PARTY_DIR}
        ${CUBZH_CORE_DIR}
        ${SBS_DIR}
        ${VX_DIRS}
        ${VXTOOLS_INCLUDE_DIR}
        ${VXTOOLS_DEPS_DIR}
        ${CHANNEL_DIR}
        ${LIBZ_INCLUDE_DIR}
        ${LIBWEBSOCKETS_INCLUDE_DIR}
        ${LIBSSL_INCLUDE_DIR}
        ${LIBMINIAUDIO_INC}
        ${USR_INCLUDE_DIR}
)

# Debug: display include directories
#[[get_property(dirs DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY INCLUDE_DIRECTORIES)
foreach(dir ${dirs})
    message(STATUS "dir='${dir}'")
endforeach()]]

# Specifies libraries CMake should link to your target library. You
# can link multiple libraries, such as libraries you define in this
# build script, prebuilt third-party libraries, or system libraries.

add_executable(gameserver ${SOURCE_FILES})
find_package(Threads)
target_link_libraries(gameserver
                        ${CMAKE_DL_LIBS}
                        ${CMAKE_THREAD_LIBS_INIT}
                        ${LIBZ}
                        luau
                        xptools
                        cubzh_core
                        vx
                        sbs
                        png
                        m
                        websockets
                        ssl
                        crypto)
