load(
    "@rules_apple//apple:apple.bzl",
    "apple_static_library",
)
load("@rules_cc//cc:defs.bzl", "cc_library")

# ----------------------------------------
# Compiler-specific compiler flags
# ----------------------------------------

CLANG_COPTS = [
    "-std=c++17",
    "-fPIC", # Position Independent Code
]

MSVC_COPTS = [
    "/std:c++17",
]

# Updated compiler selection based on OS
# TODO: filter on toolchain (if possible)
COMPILER_COPTS = select({
    "@platforms//os:windows": MSVC_COPTS,
    "//conditions:default": CLANG_COPTS,
})

# ----------------------------------------
# Platform-specific compiler flags
# ----------------------------------------

PLATFORM_COPTS = select({
    "@platforms//os:android": ["-Wno-string-conversion"],
    "@platforms//os:linux": ["-Wno-unused-variable"],
    "//conditions:default": [],
})

COPTS = COMPILER_COPTS + PLATFORM_COPTS

# ----------------------------------------
# Targets
# ----------------------------------------

# png (static library)

alias(
    name = "png",
    actual = select({
        "@platforms//os:macos": ":png-macos",
        "@platforms//os:ios": ":png-ios",
        "//conditions:default": ":png-default",
    }),
)

apple_static_library(
    name = "png-macos",
    minimum_os_version = "11.0",
    platform_type = "macos",
    deps = ["png_lib"],
)

apple_static_library(
    name = "png-ios",
    minimum_os_version = "13.0",
    platform_type = "ios",
    deps = ["png_lib"],
)

cc_static_library(
    name = "png-default",
    deps = ["png_lib"],
)

# ----------------------------------------

cc_library(
    name = "png_lib",
    srcs = [
        "src/png.c",
        "src/pngdebug.h",
        "src/pngerror.c",
        "src/pngget.c",
        "src/pnginfo.h",
        "src/pnglibconf.h",
        "src/pngmem.c",
        "src/pngpread.c",
        "src/pngpriv.h",
        "src/pngread.c",
        "src/pngrio.c",
        "src/pngrtran.c",
        "src/pngrutil.c",
        "src/pngset.c",
        "src/pngstruct.h",
        "src/pngtrans.c",
        "src/pngwio.c",
        "src/pngwrite.c",
        "src/pngwtran.c",
        "src/pngwutil.c",
    ] + select({
        "@platforms//cpu:armv7": [
            "src/arm/arm_init.c",
            "src/arm/filter_neon_intrinsics.c",
            "src/arm/filter_neon.S",
            "src/arm/palette_neon_intrinsics.c",
        ],
        "@platforms//cpu:arm64": [
            "src/arm/arm_init.c",
            "src/arm/filter_neon_intrinsics.c",
            "src/arm/filter_neon.S",
            "src/arm/palette_neon_intrinsics.c",
        ],
        "@platforms//cpu:x86_32": [
            "src/intel/filter_sse2_intrinsics.c",
            "src/intel/intel_init.c",
        ],
        "@platforms//cpu:x86_64": [
            "src/intel/filter_sse2_intrinsics.c",
            "src/intel/intel_init.c",
        ],
        "//conditions:default": [],
    }),
    hdrs = [
        "src/png.h",
        "src/pngconf.h",
    ],
    includes = ["."],
    linkopts = [
            "-lm",
            "-lz", # [gaetan] not sure this is needed
        ] + select({
        "@platforms//os:android": [],
        "//conditions:default": [],
    }),
    visibility = ["//visibility:public"],
    deps = ["@zlib//:zlib"],
)
