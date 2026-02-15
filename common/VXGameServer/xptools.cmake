# For more information about using CMake with Android Studio, read the
# documentation: https://d.android.com/studio/projects/add-native-code.html

# Sets the minimum version of CMake required to build the native library.
cmake_minimum_required(VERSION 3.4.1)

# Defines the project name
# project(xptools)

#
set(REPO_ROOT_DIR "${CMAKE_CURRENT_BINARY_DIR}/../../")
set(VXTOOLS_DIR "${REPO_ROOT_DIR}/cubzh/deps/xptools")
set(VXTOOLS_INCLUDE_DIR "${VXTOOLS_DIR}/include")
set(VXTOOLS_DEPS_DIR "${VXTOOLS_DIR}/deps")
set(VXTOOLS_COMMON_DIR "${VXTOOLS_DIR}/common")
set(VXTOOLS_LINUX_DIR "${VXTOOLS_DIR}/linux")

#
file(GLOB VXTOOLS_SOURCES
        ${VXTOOLS_COMMON_DIR}/*.cpp ${VXTOOLS_COMMON_DIR}/*.c
        ${VXTOOLS_LINUX_DIR}/*.cpp ${VXTOOLS_LINUX_DIR}/*.c
        ${VXTOOLS_LINUX_DIR}/deps/md5/*.cpp)

#
add_library(xptools STATIC
        ${VXTOOLS_SOURCES})
