#!/bin/bash

# exit on error
set -e

# Change working directory to the root of the git repository
# Get git root directory and cd to it
cd $(git rev-parse --show-toplevel)
# Change back to the original directory on exit
trap "cd -" EXIT

# Update Go modules & vendor deps
echo "⚙️ Go mod tidy & go mod vendor"
/bin/bash -c "cd ./go/cu.bzh/hub/apiserver && go mod tidy && go mod vendor"
/bin/bash -c "cd ./go/cu.bzh/hub/cli && go mod tidy && go mod vendor"

# Build docker image
docker build \
--platform linux/amd64 \
--file dockerfiles/hub-server.Dockerfile \
--tag registry.digitalocean.com/cubzh/hub-http:latest \
.

# Push docker image
docker push registry.digitalocean.com/cubzh/hub-http:latest

# Update Docker Swarm service with new image
ssh ubuntu@135.148.121.106 -i ~/.ssh/id_ovh_voxowl_com -o IdentitiesOnly=yes "docker service update --image registry.digitalocean.com/cubzh/hub-http:latest --with-registry-auth hub-http"
