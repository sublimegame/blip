# Service Ollama embed

Ollama container with model `mxbai-embed-large`.

## Build docker image and push it to the registry

```sh
./build_and_push.sh
```

## Create service

```sh
docker service create \
--name codegen-ollama-embed \
--network cubzh \
--mode global \
--constraint node.labels.services-us-1==true \
--with-registry-auth \
registry.digitalocean.com/cubzh/codegen-ollama-embed:latest
```

## Remove service

```sh
docker service rm codegen-ollama-embed
```