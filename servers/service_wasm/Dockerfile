# --------------------------------------------------
# Build env for WASM app
# --------------------------------------------------
# /!\ so far emsdk did not work on arm64 platform
FROM voxowl/emsdk:4.0.4 AS wasm_build_env

COPY bundle /repo/bundle
COPY core /repo/core
COPY deps /repo/deps
COPY i18n /repo/i18n
COPY lua/modules /repo/lua/modules

COPY common /repo/common
COPY deps /repo/deps
COPY wasm /repo/wasm

# Make sure CMakeCache.txt is removed (as it can be a non-linux version)
RUN rm -f /repo/wasm/Particubes/build/CMakeCache.txt

WORKDIR /repo/wasm/Particubes

# --------------------------------------------------
# Build WASM app
# --------------------------------------------------

FROM wasm_build_env AS builder

# Accept build argument
ARG CUBZH_TARGET

# Set as an environment variable (used by ./build.sh)
ENV CUBZH_TARGET=${CUBZH_TARGET}

# compile wasm application
RUN /bin/bash -c "source /emsdk/emsdk_env.sh && ./build.sh"

CMD ["/bin/bash"]

# --------------------------------------------------
# Build Go HTTP server
# --------------------------------------------------

FROM golang:1.24.1-alpine3.21 AS http_builder

# for HTTPS
# /certificates

# context is root of the git repo

COPY ./go/cu.bzh/cors /go/cu.bzh/cors
COPY ./go/cu.bzh/wasmserver /go/cu.bzh/wasmserver

WORKDIR /go/cu.bzh/wasmserver

RUN go build main.go

CMD ["ash"]

# --------------------------------------------------
# web server runner | empty : no website files
# --------------------------------------------------
FROM alpine:3.21 AS web_server_empty

# get the http server executable
COPY --from=http_builder /go/cu.bzh/wasmserver/main /server

EXPOSE 80

CMD ["/server"]

# --------------------------------------------------
# Dev environment for the Go HTTP server
# --------------------------------------------------
# Useful to dev the Go http server itself.
# Contains:
#   - SSL/TLS certificates for HTTPS
#   - Go compiler
#   - Go source of http server for wasm app
#   - a compiled copy of Cubzh wasm app
FROM golang:1.24.1-alpine3.21 AS http_server_dev_env
# SSL/TLS certificates for HTTPS
# /certificates
# context is root of the git repo
COPY ./go/cu.bzh/wasmserver /go/cu.bzh/wasmserver
# compiled Cubzh wasm app
COPY --from=builder /repo/wasm/Particubes/build/output /www

WORKDIR /go/cu.bzh/wasmserver

CMD ["ash"]

# --------------------------------------------------
# web server runner
# --------------------------------------------------
FROM web_server_empty

COPY ./wasm/editions /editions

# wasm app we just compiled
COPY --from=builder /repo/wasm/Particubes/build/output /editions/latest

# Code for huggingface hackathon: (we need to make it an edition)
# # compiled Cubzh wasm app
# COPY --from=builder /repo/wasm/Particubes/build/output /www_huggingface
# # 0.0.68 stable release
# COPY ./wasm/www /www
