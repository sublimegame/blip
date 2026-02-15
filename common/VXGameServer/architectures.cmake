cmake_minimum_required(VERSION 3.20)

# Define architecture mapping
# (project architecture name -> Docker architecture name)
set(ARCH_MAP_arm64 "arm64")
# set(ARCH_MAP_arm "arm")
# set(ARCH_MAP_x86 "x86")
set(ARCH_MAP_x86_64 "amd64")

# Convert from project architecture name to Docker architecture name
# Example: x86_64 -> amd64
function(convert_arch_to_docker_arch INPUT_ARCH OUTPUT_ARCH)
    if(DEFINED ARCH_MAP_${INPUT_ARCH})
        set(${OUTPUT_ARCH} "${ARCH_MAP_${INPUT_ARCH}}" PARENT_SCOPE)
    else()
        message(STATUS "Unknown architecture: ${INPUT_ARCH}")
        set(${OUTPUT_ARCH} ${INPUT_ARCH} PARENT_SCOPE)
    endif()
endfunction()


# Define architecture mapping
# (Docker architecture name -> project architecture name)
set(DOCKER_ARCH_MAP_arm64 "arm64")
# set(DOCKER_ARCH_MAP_armeabi-v7a "arm") 
# set(DOCKER_ARCH_MAPP_x86 "x86")
set(DOCKER_ARCH_MAP_amd64 "x86_64")

# Convert from Android architecture name to project architecture name
# Example: amd64 -> x86_64
function(convert_docker_arch_to_arch INPUT_ARCH OUTPUT_ARCH)
    if(DEFINED DOCKER_ARCH_MAP_${INPUT_ARCH})
        set(${OUTPUT_ARCH} "${DOCKER_ARCH_MAP_${INPUT_ARCH}}" PARENT_SCOPE)
    else()
        message(STATUS "Unknown architecture: ${INPUT_ARCH}")
        set(${OUTPUT_ARCH} ${INPUT_ARCH} PARENT_SCOPE)
    endif()
endfunction()
