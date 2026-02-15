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
