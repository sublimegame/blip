#!/bin/bash

# - builds PROD gameserver image
# - uses commit to tag the image, with "-dirty" suffix if dirty
# - pushes the image to the registry

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
CONFIG_JSON="$PROJECT_ROOT/bundle/config.json"

# The script is now executed from project root directory
cd "$PROJECT_ROOT"

# Ensure the dependencies are installed
/bin/bash -c "cd deps/deptool/cmd && go run ./... autoconfig linux"

DIRTY=$(git diff --quiet || echo '-dirty')
COMMIT=$(git rev-parse --short=8 HEAD)
TAG="$COMMIT$DIRTY"
IMAGE="registry.digitalocean.com/gameserver:$TAG"

# build docker image for GameServer
docker build \
-t "$IMAGE" \
-f dockerfiles/gameserver.Dockerfile \
--platform linux/amd64 \
--build-arg CUBZH_TARGETARCH=amd64 \
.

docker push "$IMAGE"

# set config.json, for clients to request this gameserver tag
docker run --rm -i -v "$CONFIG_JSON":/config.json imega/jq --indent 4 ".GameServerTag = \"$TAG\"" /config.json \
	> "$CONFIG_JSON-new"
	mv "$CONFIG_JSON-new" "$CONFIG_JSON"

echo "GAMESERVER IMAGE: $IMAGE (pushed)"

echo "GameServerTag set in config.json"
