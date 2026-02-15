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
CONFIG_JSON="$PROJECT_ROOT/common/assets/config.json"

# The script is now executed from project root directory
cd "$PROJECT_ROOT"

# TARGET=$1


STEAM="$PROJECT_ROOT/distribution/steam"
STEAM_CMD="$STEAM/steamworks_sdk/tools/ContentBuilder/builder_osx/steamcmd.sh"
STEAM_VDF="$STEAM/steamworks_sdk/tools/ContentBuilder/scripts/app_build_1386770.vdf"

echo "⚙️  Preparing Steam build..."

# Unzip macOS App bundle
# rm -r ./distribution/builds/Blip.app
unzip ./distribution/builds/Blip.zip -d ./distribution/builds

echo "⚙️  Sending Steam build..."

$STEAM_CMD +login gaetan_voxowl haskykqisxi7gagkoS +run_app_build "$STEAM_VDF" +quit

echo "⚙️  Cleaning up..."

# Remove unzipped macOS App bundle
rm -r ./distribution/builds/Blip.app

echo "✅ Steam build sent successfully!"
