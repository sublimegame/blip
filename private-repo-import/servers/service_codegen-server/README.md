# Service: codegen-server

This service runs on machine `services-us-1`.

## Build docker image

```sh
docker build \
--platform linux/amd64 \
--file servers/service_codegen-server/Dockerfile \
--tag registry.digitalocean.com/cubzh/codegen-server:latest \
.
```

## Push docker image

```sh
docker push registry.digitalocean.com/cubzh/codegen-server:latest
```

## Create service

```sh
docker service create \
--name codegen-server \
--network cubzh \
--mode global \
--constraint node.labels.services-us-1==true \
--mount type=bind,source=/voxowl/secrets/ANTHROPIC_API_KEY,target=/voxowl/secrets/ANTHROPIC_API_KEY \
--env CODEGEN_CHROMADB_HOST=codegen-chromadb \
--env CODEGEN_CHROMADB_PORT=8000 \
--env CODEGEN_OLLAMA_EMBED_URL=http://codegen-ollama-embed:11434 \
--env ANTHROPIC_API_KEY_FILE=/voxowl/secrets/ANTHROPIC_API_KEY
--with-registry-auth \
registry.digitalocean.com/cubzh/codegen-server:latest
```

## Update service

```sh
docker service update \
--env-add ANTHROPIC_API_KEY_FILE=/voxowl/secrets/ANTHROPIC_API_KEY \
codegen-server
```

```sh
docker service update \
--env-add GEMINI_API_KEY_FILE=/voxowl/secrets/GEMINI_API_KEY \
codegen-server
```

```sh
docker service update \
--mount-add type=bind,source=/voxowl/secrets/GEMINI_API_KEY,target=/voxowl/secrets/GEMINI_API_KEY
codegen-server
```

```sh
docker service update \
--mount-add type=bind,source=/voxowl/secrets/GROK_API_KEY,target=/voxowl/secrets/GROK_API_KEY \
--env-add GROK_API_KEY_FILE=/voxowl/secrets/GROK_API_KEY \
codegen-server
```

## GET A SHELL INSIDE THE CONTAINER

```
docker exec -ti 4713b2980a28 ash
```
