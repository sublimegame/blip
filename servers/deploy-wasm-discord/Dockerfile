# --------------------------------------------------
# WASM build environment
# --------------------------------------------------

# Note:
# emsdk don't work on arm64 platform (as of 3.1.38),
# so voxowl/emsdk exists only as a linux/amd64 image

FROM voxowl/emsdk:3.1.73 AS wasm_build_env

COPY ./common/assets /repo/common/assets
COPY ./common/bgfx /repo/common/bgfx
COPY ./common/Lua /repo/common/Lua
COPY ./common/VXFramework /repo/common/VXFramework
COPY ./common/VXGameServer /repo/common/VXGameServer
COPY ./common/VXLuaSandbox /repo/common/VXLuaSandbox
COPY ./common/VXNetworking /repo/common/VXNetworking

COPY ./core /repo/core
COPY ./deps /repo/deps
COPY ./lua/modules /repo/lua/modules
COPY ./i18n /repo/i18n
COPY ./deps/xptools /repo/deps/xptools
COPY ./bundle /repo/bundle

COPY ./deps /repo/deps
COPY ./wasm/Particubes /repo/wasm/Particubes

WORKDIR /repo/wasm/Particubes


# --------------------------------------------------
# Build WASM app
# --------------------------------------------------

FROM wasm_build_env AS wasm_builder

# Accept build argument
ARG CUBZH_TARGET

# Set as an environment variable (used by ./build.sh)
ENV CUBZH_TARGET=${CUBZH_TARGET}

# compile wasm application
RUN /bin/bash -c "source /emsdk/emsdk_env.sh && ./build.sh"

CMD ["bash"]


# --------------------------------------------------
# Build HTTP server (Go)
# --------------------------------------------------

FROM golang:1.23.4-alpine3.21 AS http_server_builder

COPY ./go/cu.bzh/cors /go/cu.bzh/cors
COPY ./go/cu.bzh/wasmserver /go/cu.bzh/wasmserver

WORKDIR /go/cu.bzh/wasmserver

RUN go build main.go


# --------------------------------------------------
# http_server_runner
# --------------------------------------------------
FROM alpine:3.21 AS http_server_runner

# get the http server executable
COPY --from=http_server_builder /go/cu.bzh/wasmserver/main /server

# wasm app we just compiled
COPY --from=wasm_builder /repo/wasm/Particubes/build/output /editions/latest

# COPY ./wasm/editions /editions

# tells the server to not use HTTPS but HTTP
# ENV CUBZH_NO_HTTPS="1"
# EXPOSE 80

EXPOSE 443

CMD ["/server"]
