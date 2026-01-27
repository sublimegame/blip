#!/bin/bash

# - builds gameserver image (target == build-env)
# - updates config.json for built games to always target this game server
# - runs container with ash and gives instructions to build and run the gameserver
# - NOTE: uncomment/comment docker run statements to mount files from disk

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

IMAGE="gameserver:debug"

# build docker image for GameServer
DOCKER_BUILDKIT=1 docker build \
--target=build-env \
-t "$IMAGE" \
-f servers/gameserver.Dockerfile \
.

PORT=22222

IP=$(ifconfig | grep "inet " | grep -Fv 127.0.0.1 | awk '{print $2}')
IP=$( cut -d $' ' -f 1 <<< $IP )

# set config.json, for all games to reach this server
docker run --rm -i -v "$CONFIG_JSON":/config.json imega/jq ".DebugGameServer = \"$IP:$PORT\"" /config.json \
	> "$CONFIG_JSON-new"
	mv "$CONFIG_JSON-new" "$CONFIG_JSON"

# run the container

echo "#####################"
echo "# build:"
echo "/servers/gameserver/compileExe.sh"
echo "# run:"
echo "/common/VXGameServer/gameserver <GAME ID> debug-server-id api.particubes.com:10080"
echo "#####################"

# docker run -ti --rm \
# -v "$PWD"/common:/common \
# -v "$PWD"/deps/zlib:/deps/zlib \
# -v "$PWD"/xptools:/xptools \
# -v "$PWD"/servers/gameserver:/servers/gameserver \
# -p "$PORT":"$PORT" \
# "$IMAGE"

docker run -ti --rm \
-p "$PORT":"$PORT" \
"$IMAGE"
