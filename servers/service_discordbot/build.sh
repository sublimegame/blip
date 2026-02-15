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
/bin/bash -c "cd ./go/cu.bzh/discordbot && go mod tidy && go mod vendor"

# Build docker image
docker build \
--platform linux/amd64 \
--file dockerfiles/discordbot.Dockerfile \
--tag registry.digitalocean.com/cubzh/discordbot:latest \
.
