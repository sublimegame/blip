#!/bin/bash

# Runs CI the same way dagger does it.

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

echo "-------------------------------"
echo "RUN CI AGAIN WITHOUT BUILDING:"
echo "docker run -ti --rm -w \"/ci\" -v $PROJECT_ROOT/ci/test.lua:/ci/test.lua --platform linux/amd64 ci"
echo "-------------------------------"

# build docker image
docker build -t ci -f ci/Dockerfile_local --platform linux/amd64 .

# run docker image
docker run -ti --rm -w "/ci" --platform linux/amd64 ci

echo "-------------------------------"
echo "RUN CI AGAIN WITHOUT BUILDING:"
echo "docker run -ti --rm -w \"/ci\" -v $PROJECT_ROOT/ci/test.lua:/ci/test.lua --platform linux/amd64 ci"
echo "-------------------------------"