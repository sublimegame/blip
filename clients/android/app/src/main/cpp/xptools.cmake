# For more information about using CMake with Android Studio, read the
# documentation: https://d.android.com/studio/projects/add-native-code.html

cmake_minimum_required(VERSION 3.20)

# Defines the project name
# project(xptools)

#
set(REPO_ROOT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../../../../..")
set(VXTOOLS_DIR "${REPO_ROOT_DIR}/deps/xptools")
set(VXTOOLS_INCLUDE_DIR "${VXTOOLS_DIR}/include")
set(VXTOOLS_COMMON_DIR "${VXTOOLS_DIR}/common")
set(VXTOOLS_ANDROID_DIR "${VXTOOLS_DIR}/android")
set(VXTOOLS_DEPS_DIR "${VXTOOLS_DIR}/deps")
set(DEPS_MINIAUDIO_DIR "${REPO_ROOT_DIR}/deps/miniaudio")

file(GLOB VXTOOLS_SOURCES
        ${VXTOOLS_COMMON_DIR}/*.cpp ${VXTOOLS_COMMON_DIR}/*.c
        ${VXTOOLS_ANDROID_DIR}/*.cpp ${VXTOOLS_ANDROID_DIR}/*.c
        ${VXTOOLS_DEPS_DIR}/*.cpp ${VXTOOLS_DEPS_DIR}/*.c)

#
add_library(xptools STATIC
        ${VXTOOLS_SOURCES})

include_directories(xptools
        ${DEPS_MINIAUDIO_DIR}
        ${VXTOOLS_DEPS_DIR})

target_link_libraries(xptools
        android log)

