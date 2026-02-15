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

docker build -f servers/gameserver.Dockerfile -t gameserver:embedded .

docker save gameserver:embedded | gzip > ./servers/go/src/accounts/game/gameserver.tar.gz