#!/bin/bash

set -e

START_LOCATION="$PWD"
SCRIPT_LOCATION=$(cd -P -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)

# Go back to start location when script exits
trap "cd $START_LOCATION" EXIT

# Go to script location before running git command
# to make sure it runs within project tree
cd "$SCRIPT_LOCATION"

# Use git command to get root project directory.
PROJECT_ROOT=$(git rev-parse --show-toplevel)

# The script is now executed from project root directory
cd "$PROJECT_ROOT"

COMMIT=$(git rev-parse --short=8 HEAD)
IMAGE="ci:$COMMIT"

DOCKER_BUILDKIT=1 docker build -t "$IMAGE" -f ci/Dockerfile .

# cleanup
# docker rmi "$IMAGE"

# debug
docker run -ti --rm -w /ci \
-v "$PROJECT_ROOT"/common/engine:/common/engine \
-v "$PROJECT_ROOT"/common/Lua:/common/Lua \
-v "$PROJECT_ROOT"/common/VXFramework:/common/VXFramework \
-v "$PROJECT_ROOT"/common/VXGameServer:/common/VXGameServer \
-v "$PROJECT_ROOT"/common/VXLuaSandbox:/common/VXLuaSandbox \
-v "$PROJECT_ROOT"/common/VXNetworking:/common/VXNetworking \
-v "$PROJECT_ROOT"/xptools:/xptools \
-v "$PROJECT_ROOT"/ci/C/Makefile:/ci/Makefile \
-v "$PROJECT_ROOT"/ci/C/main.cpp:/ci/main.cpp \
-v "$PROJECT_ROOT"/ci/C/main.h:/ci/main.h \
-v "$PROJECT_ROOT"/ci/test.lua:/ci/test.lua \
-v "$PROJECT_ROOT"/ci/ci.sh:/ci/ci.sh \
-v "$PROJECT_ROOT"/ci/storage:/storage \
-v "$PROJECT_ROOT"/ci/bundle:/bundle \
"$IMAGE" bash
