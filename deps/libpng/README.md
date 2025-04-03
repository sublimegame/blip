# libpng

## How to build

```sh
./download.sh -v 1.6.47 -o .
./build.sh -v 1.6.47 -p macos
```

## Get source code

Download source code from [libpng Github repository](https://github.com/pnggroup/libpng/tags)

## Prepare source code for building

Unzip the source code and create the `pnglibconf.h` file:

```sh
cp scripts/pnglibconf.h.prebuilt pnglibconf.h
```

## Build commands (from cubzh/cubzh repo root dir)

```shell
bazel build --platforms=//:android_arm //deps/libpng:png
bazel build --platforms=//:android_arm64 //deps/libpng:png
bazel build --platforms=//:android_x86 //deps/libpng:png
bazel build --platforms=//:android_x86_64 //deps/libpng:png
```

```sh
# linux
# From cubzh/cubzh repo root dir
docker run --rm -v $(pwd):/cubzh -w /cubzh/deps/libpng --entrypoint /bin/bash --platform linux/amd64 voxowl/bazel:8.1.1 ./build.sh -p linux -v 1.6.47
```