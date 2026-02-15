#!/bin/bash
set -e

echo "⚙️ Deploying [cubzh-wasm-discord] to production..."

DOCKER_IMAGE_TAG="latest-discord" # "0.1.9-discord"
DOCKER_IMAGE_NAME="registry.digitalocean.com/cubzh/wasm:${DOCKER_IMAGE_TAG}"
DOCKER_SERVICE_NAME="wasm-discord"

# TODO: get git root path and use it
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

# Build and push the server docker image
docker build \
--platform linux/amd64 \
--build-arg CUBZH_TARGET=wasm-discord \
-t $DOCKER_IMAGE_NAME \
-f servers/deploy-wasm-discord/Dockerfile .

docker push $DOCKER_IMAGE_NAME

# Update the Docker Swarm service "wasm-discord"

# Define variables
USER="root"
HOST="67.205.183.56"
COMMAND="docker service update --with-registry-auth --image ${DOCKER_IMAGE_NAME} ${DOCKER_SERVICE_NAME}"

ssh $USER@$HOST -i ~/.ssh/id_rsa_cubzh_digitalocean -o IdentitiesOnly=yes "$COMMAND"
