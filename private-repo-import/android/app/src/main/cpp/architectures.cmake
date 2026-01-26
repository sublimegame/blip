cmake_minimum_required(VERSION 3.31.0)

# Define architecture mapping
# (project architecture name -> Android architecture name)
set(ARCH_MAP_arm64 "arm64-v8a")
set(ARCH_MAP_arm "armeabi-v7a")
set(ARCH_MAP_x86 "x86")
set(ARCH_MAP_x86_64 "x86_64")

# Convert from project architecture name to Android architecture name
# Example: arm64 -> arm64-v8a
function(convert_arch_to_android_arch INPUT_ARCH OUTPUT_ARCH)
    if(DEFINED ARCH_MAP_${INPUT_ARCH})
        set(${OUTPUT_ARCH} "${ARCH_MAP_${INPUT_ARCH}}" PARENT_SCOPE)
    else()
        message(STATUS "Unknown architecture: ${INPUT_ARCH}")
        set(${OUTPUT_ARCH} ${INPUT_ARCH} PARENT_SCOPE)
    endif()
endfunction()


# Define architecture mapping
# (Android architecture name -> project architecture name)
set(ANDROID_ARCH_MAP_arm64-v8a "arm64")
set(ANDROID_ARCH_MAP_armeabi-v7a "arm") 
set(ANDROID_ARCH_MAP_x86 "x86")
set(ANDROID_ARCH_MAP_x86_64 "x86_64")

# Convert from Android architecture name to project architecture name
# Example: arm64-v8a -> arm64
function(convert_android_arch_to_arch INPUT_ARCH OUTPUT_ARCH)
    if(DEFINED ANDROID_ARCH_MAP_${INPUT_ARCH})
        set(${OUTPUT_ARCH} "${ANDROID_ARCH_MAP_${INPUT_ARCH}}" PARENT_SCOPE)
    else()
        message(STATUS "Unknown architecture: ${INPUT_ARCH}")
        set(${OUTPUT_ARCH} ${INPUT_ARCH} PARENT_SCOPE)
    endif()
endfunction()
