#!/bin/bash

# exit on error
set -e

# Change working directory to the root of the git repository
# Get git root directory and cd to it
cd $(git rev-parse --show-toplevel)
# Change back to the original directory on exit
trap "cd -" EXIT

# Name of the docker image for `wasm` service
readonly IMAGE_NAME="registry.digitalocean.com/cubzh/wasm:latest"
readonly SERVICE_NAME="wasm"

# Build docker image
docker build \
--platform linux/amd64 \
--file servers/service_wasm/Dockerfile \
--tag $IMAGE_NAME \
--build-arg CUBZH_TARGET=$SERVICE_NAME \
.

# Push docker image
docker push $IMAGE_NAME

# Update Docker Swarm service with new image
ssh ubuntu@135.148.121.106 -i ~/.ssh/id_ovh_voxowl_com -o IdentitiesOnly=yes "docker service update --image $IMAGE_NAME --with-registry-auth $SERVICE_NAME"
