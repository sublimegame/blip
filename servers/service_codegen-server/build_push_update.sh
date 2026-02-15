#!/bin/bash

# exit on error
set -e

# Find the git repository root
REPO_ROOT=$(git rev-parse --show-toplevel)
cd $REPO_ROOT
trap "cd -" EXIT

# build the image
docker build \
--platform linux/amd64 \
--file servers/service_codegen-server/Dockerfile \
--tag registry.digitalocean.com/cubzh/codegen-server:latest \
.

# push the image
docker push registry.digitalocean.com/cubzh/codegen-server:latest

# update the service
ssh ubuntu@135.148.121.106 -i ~/.ssh/id_ovh_voxowl_com -o IdentitiesOnly=yes "docker service update --image registry.digitalocean.com/cubzh/codegen-server:latest --with-registry-auth codegen-server"
