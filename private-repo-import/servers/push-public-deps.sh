#!/bin/bash

# script pushes images with public content that
# we need to use as dependencies.

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
# IOS_MACOS_CONFIG_PLIST="$PROJECT_ROOT/ios-macos/ParticubesWorlds/ParticubesWorlds shared/Config.plist"
CONFIG_JSON="$PROJECT_ROOT/common/assets/config.json"

# The script is now executed from project root directory
cd "$PROJECT_ROOT"

docker build -t voxowl/pcubes-gameserver-public-deps -f servers/gameserver.Dockerfile --target public .
docker push voxowl/pcubes-gameserver-public-deps