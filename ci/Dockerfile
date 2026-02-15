FROM ubuntu:20.04

ARG DEBIAN_FRONTEND=noninteractive

RUN apt-get update 
RUN apt-get install -y libreadline-dev build-essential gdb

# /!\ CONTEXT MUST BE PROJECT ROOT DIR
# TO BUILD WITHOUT ISSUES.

COPY ./ci/C /ci
COPY ./ci/storage /storage
COPY ./ci/bundle /bundle
COPY ./ci/test.lua /ci/test.lua
COPY ./ci/ci.sh /ci/ci.sh

COPY ./cubzh/core /cubzh/core
COPY ./cubzh/deps /cubzh/deps
COPY ./common/VXFramework /common/VXFramework
COPY ./common/VXGameServer /common/VXGameServer
COPY ./common/VXLuaSandbox /common/VXLuaSandbox
COPY ./common/VXNetworking /common/VXNetworking

COPY ./deps /deps

WORKDIR /ci

CMD ./ci.sh