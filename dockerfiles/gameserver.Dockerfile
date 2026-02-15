#
# Cubzh gameserver
#

# --------------------------------------------------
# Assemble dependencies
# --------------------------------------------------

FROM voxowl/ubuntu:24.04 AS deps
# Note: voxowl/ubuntu has ca-certificates package installed, which is needed by deptool

# deps (sources)
COPY ./deps/bgfx /project/deps/bgfx

# deps (prebuilt)
COPY ./deps/libluau/_active_/prebuilt/linux/x86_64 /project/deps/libluau/_active_/prebuilt/linux/x86_64
COPY ./deps/libpng/_active_/prebuilt/linux/x86_64 /project/deps/libpng/_active_/prebuilt/linux/x86_64
COPY ./deps/libssl/linux/amd64 /project/deps/libssl/linux/amd64
COPY ./deps/libwebsockets/linux/amd64 /project/deps/libwebsockets/linux/amd64
COPY ./deps/libz/linux-x86_64 /project/deps/libz/linux-x86_64
COPY ./deps/xptools /project/deps/xptools

# deptoop
# TODO: fix it. It doesn't download the files at the correct path
# COPY ./deps/deptool/deptool_linux_x86_64 /project/deps/deptool/deptool_linux_x86_64
# RUN /project/deps/deptool/deptool_linux_x86_64 download libluau 0.661 linux

# --------------------------------------------------
# Compilation environment with sources
# --------------------------------------------------

FROM voxowl/cpp-build-env:18.1.3 AS private

# TODO: add this to the voxowl/cpp-build-env image
RUN apt-get update
RUN apt-get install -y crossbuild-essential-amd64

# deps
COPY --from=deps /project /project

ARG CUBZH_TARGETARCH
ENV CUBZH_TARGETARCH=$CUBZH_TARGETARCH

COPY ./core /project/core
# avoiding main function defined twice
# ideally we should review build scripts for the gameserver
# not to include unit tests.
RUN rm -rf /project/core/tests

COPY ./common/VXFramework /project/common/VXFramework
COPY ./common/VXGameServer /project/common/VXGameServer
COPY ./common/VXLuaSandbox /project/common/VXLuaSandbox
COPY ./common/VXNetworking /project/common/VXNetworking

COPY ./servers/gameserver /project/servers/gameserver

WORKDIR /project/servers/gameserver

# --------------------------------------------------
# Image with compilation env + compiled gameserver executable
# --------------------------------------------------

FROM private AS builder

# compile using 5 threads
RUN /project/servers/gameserver/compileExe.sh 5

# --------------------------------------------------
# Stripped-down image containing the gameserver executable
# --------------------------------------------------

FROM ubuntu:24.04 AS gameserver

COPY --from=builder /project/common/VXGameServer/gameserver /gameserver

COPY ./bundle /bundle
COPY ./lua/modules /bundle/modules

EXPOSE 80

ENTRYPOINT ["/gameserver"]

# --------------------------------------------------
# Debug environment
# --------------------------------------------------
# FROM private AS builder-debug
# RUN apt update && apt install -y gdb
# # RUN /project/servers/gameserver/compileExeDebug.sh 10
