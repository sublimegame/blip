#!/bin/bash

# exit on error
set -e

# Find git repo root directory
GIT_REPO_ROOT=$(git rev-parse --show-toplevel)
echo "🔍 Git repo root directory: $GIT_REPO_ROOT"
# Move to git repo root directory
cd $GIT_REPO_ROOT

# -----------------------------------------------------
# Ollama
# -----------------------------------------------------

# Check that ollama is installed

if ! command -v ollama &> /dev/null; then
    echo "🚨 ollama could not be found"
    exit 1
fi

# Check that the 2 models are downloaded, and download them if needed.

if ! ollama list | grep -q "llama3.2:3b"; then
    ollama pull llama3.2:3b
fi
echo "✅ llama3.2:3b downloaded"

if ! ollama list | grep -q "mxbai-embed-large"; then
    ollama pull mxbai-embed-large
fi
echo "✅ mxbai-embed-large downloaded"

# -----------------------------------------------------
# ChromaDB
# -----------------------------------------------------

# look for a container named codegen_chromadb
CHROMADB_PLATFORM="linux/amd64"
CHROMADB_IMAGE="chromadb/chroma:0.5.20"
CHROMADB_NAME="codegen-chromadb"
CHROMADB_PORT=8000
CHROMADB_HOST="localhost"

# check if the container is running and if it is, stop it
if docker ps -a | grep -q $CHROMADB_NAME; then
    echo "⚙️ stopping container $CHROMADB_NAME"
    docker rm -f $CHROMADB_NAME
fi

# run a new chromadb container
echo "⚙️ starting container $CHROMADB_NAME"
docker run --platform $CHROMADB_PLATFORM -d -p $CHROMADB_PORT:8000 --name $CHROMADB_NAME $CHROMADB_IMAGE

# wait until the chromadb server is ready and responds to requests
while ! curl -s http://$CHROMADB_HOST:$CHROMADB_PORT/api/v1/collections > /dev/null; do
    echo "🔄 waiting for chromadb server to be ready..."
    sleep 1
done

# -----------------------------------------------------
# Codegen - Embed
# -----------------------------------------------------

# Run codegen container in "embed" mode

cd $GIT_REPO_ROOT/go/cu.bzh/ai/codegen/

CODEGEN_DOCS_REFERENCE_DIR=$GIT_REPO_ROOT/lua/docs/content/reference \
CODEGEN_SAMPLE_SCRIPTS_FILE=$GIT_REPO_ROOT/lua/docs/content/sample-scripts.yml \
CODEGEN_CHROMADB_HOST=$CHROMADB_HOST \
CODEGEN_CHROMADB_PORT=$CHROMADB_PORT \
go run ./... --embed

# -----------------------------------------------------
# Save ChromaDB container state
# -----------------------------------------------------

CHROMADB_REGISTRY="registry.digitalocean.com/cubzh"
CHROMADB_IMAGE="codegen-chromadb-embeddings"
CHROMADB_TAG="latest"

CHROMADB_FULL_IMAGE="$CHROMADB_REGISTRY/$CHROMADB_IMAGE:$CHROMADB_TAG"

# save the chromadb container state to an image and push it to the registry
docker commit $CHROMADB_NAME $CHROMADB_FULL_IMAGE
docker push $CHROMADB_FULL_IMAGE

# -----------------------------------------------------
# Update docker service on the server
# -----------------------------------------------------

ssh ubuntu@135.148.121.106 -i ~/.ssh/id_ovh_voxowl_com -o IdentitiesOnly=yes "docker service update --image $CHROMADB_FULL_IMAGE --with-registry-auth codegen-chromadb"
