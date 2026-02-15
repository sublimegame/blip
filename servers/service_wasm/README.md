# Service: wasm

This service runs on machine `services-us-1`.

## wasm

### Build docker image

```sh
docker build \
  --platform linux/amd64 \
  --file servers/service_wasm/Dockerfile \
  --tag registry.digitalocean.com/cubzh/wasm:latest \
  --build-arg CUBZH_TARGET=wasm \
  .
```

### Push docker image

```sh
docker push registry.digitalocean.com/cubzh/wasm:latest
```

### Update the service

```sh
docker service update --with-registry-auth --image registry.digitalocean.com/cubzh/wasm:latest wasm
```

### Create the service

```sh
docker service create \
--name wasm \
--mode global \
--constraint node.labels.services-us-1==true \
--network cubzh \
--with-registry-auth \
registry.digitalocean.com/cubzh/wasm:latest
```

## wasm-discord

### Build docker image

```sh
docker build \
  --platform linux/amd64 \
  --file servers/service_wasm/Dockerfile \
  --tag registry.digitalocean.com/cubzh/wasm:latest-discord \
  --build-arg CUBZH_TARGET=wasm-discord \
  .
```

### Push docker image

```sh
docker push registry.digitalocean.com/cubzh/wasm:latest-discord
```

### Update the service

```sh
docker service update --with-registry-auth --image registry.digitalocean.com/cubzh/wasm:latest-discord wasm-discord
```

### Create the service

```sh
docker service create \
--name wasm-discord \
--mode global \
--constraint node.labels.services-us-1==true \
--network cubzh \
--with-registry-auth \
registry.digitalocean.com/cubzh/wasm:latest-discord
```
