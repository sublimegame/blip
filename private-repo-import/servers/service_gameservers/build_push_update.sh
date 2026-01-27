#!/bin/bash

# exit on error
set -e

# Change working directory to the root of the git repository
# Get git root directory and cd to it
cd $(git rev-parse --show-toplevel)
# Change back to the original directory on exit
trap "cd -" EXIT

# Update Go modules & vendor deps
echo "⭐️ [1/4] Go mod tidy & go mod vendor..."
/bin/bash -c "cd ./go/cu.bzh/scheduler && go mod tidy && go mod vendor"

# Build docker image
echo "⭐️ [2/4] Building docker image..."
docker build \
--platform linux/amd64 \
--file dockerfiles/scheduler.Dockerfile \
--tag registry.digitalocean.com/cubzh/scheduler:latest \
.

# push docker image to the registry
echo "⭐️ [3/4] Pushing docker image to the registry..."
docker push registry.digitalocean.com/cubzh/scheduler:latest

# Update Docker Swarm service with new image (on all 3 regions)
echo -n "⭐️ [4/6] Updating service for Europe..."
ssh ubuntu@135.148.121.106 -i ~/.ssh/id_ovh_voxowl_com -o IdentitiesOnly=yes \
"docker service update --image registry.digitalocean.com/cubzh/scheduler:latest --with-registry-auth gameservers-eu"

echo -n "⭐️ [5/6] Updating service for Singapore..."
ssh ubuntu@135.148.121.106 -i ~/.ssh/id_ovh_voxowl_com -o IdentitiesOnly=yes \
"docker service update --image registry.digitalocean.com/cubzh/scheduler:latest --with-registry-auth gameservers-sg"

echo -n "⭐️ [6/6] Updating service for US..."
ssh ubuntu@135.148.121.106 -i ~/.ssh/id_ovh_voxowl_com -o IdentitiesOnly=yes \
"docker service update --image registry.digitalocean.com/cubzh/scheduler:latest --with-registry-auth gameservers-us"

echo "✅ Gameserver scheduler service updated"
