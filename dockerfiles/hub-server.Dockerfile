# -------------------------------------
# Compile Cubzh CLI (C++)
# -------------------------------------
# binary created at /cubzh/cli/cmake/cubzh_cli
#
# NOTE: this only builds if plaform is linux/amd64 (not supported on arm64)
#       because the linking against /cubzh/deps/libz/linux-x86_64/lib/libz.a
#       is hardcoded.
#
FROM voxowl/cpp-build-env:18.1.3 AS cmake-build-cubzh-cli

COPY ./core /cubzh/core
COPY ./cli /cubzh/cli
COPY ./deps /cubzh/deps

WORKDIR /cubzh/cli/cmake
RUN CC=clang CXX=clang++ cmake -G Ninja .
RUN cmake --build . --clean-first

# -------------------------------------
# Compile Hub CLI (Go)
# -------------------------------------
# binary created at /go/cu.bzh/hub/cli/exe
FROM golang:1.24.4-bullseye AS go-build-hub-cli

COPY ./go/cu.bzh/hub/cli /go/cu.bzh/hub/cli

WORKDIR /go/cu.bzh/hub/cli
RUN go build -o exe *.go

# -------------------------------------
# Compile Hub API Server (Go)
# -------------------------------------
# binary created at /go/cu.bzh/hub/apiserver/exe
FROM golang:1.24.4-bullseye AS go-build-hub-apiserver

COPY ./go/cu.bzh/hub/apiserver /go/cu.bzh/hub/apiserver
COPY ./go/blip.game/git /go/blip.game/git

WORKDIR /go/cu.bzh/hub/apiserver
RUN go build -o exe *.go

# -------------------------------------
# Hub server base image
# -------------------------------------
# contains everything needed but the compiled executables
FROM ubuntu:24.04 AS hub-server-base

# Needed to be able to send emails
# (TLS verification for mailjet HTTPS access)
RUN apt-get update && apt-get install -y ca-certificates

# install: email templates
COPY ./go/cu.bzh/hub/apiserver/templates /templates
COPY ./go/cu.bzh/hub/cli/email_emailAddressConfirmation.md /email_emailAddressConfirmation.md
COPY ./go/cu.bzh/hub/cli/email-template.html /email-template.html
COPY ./go/cu.bzh/hub/cli/email-user-password.md /email-user-password.md

# -------------------------------------
# Hub server image
# -------------------------------------
# It's the hub server base image + binaries compiled above
FROM hub-server-base AS hub-server
# install: Cubzh CLI
COPY --from=cmake-build-cubzh-cli /cubzh/cli/cmake/cubzh_cli /cubzh_cli
# install: Hub CLI
COPY --from=go-build-hub-cli /go/cu.bzh/hub/cli/exe /cli
# install: Hub API server
COPY --from=go-build-hub-apiserver /go/cu.bzh/hub/apiserver/exe /hub

EXPOSE 80

ENTRYPOINT ["/hub"]
