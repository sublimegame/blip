# Service: hub-http

This service runs on machine `services-us-1`.

## Build docker image

Note: see script [./build.sh](./build.sh).

```sh
docker build \
--platform linux/amd64 \
--file dockerfiles/hub-server.Dockerfile \
--tag registry.digitalocean.com/cubzh/hub-http:latest \
.
```

## Push docker image

```sh
docker push registry.digitalocean.com/cubzh/hub-http:latest
```

## Create the service

```sh
docker service create \
--name hub-http \
--network cubzh \
--mode global \
--constraint node.labels.services-us-1==true \
--mount type=bind,source=/voxowl/hub/items,target=/voxowl/hub/items \
--mount type=bind,source=/voxowl/hub/worlds,target=/voxowl/hub/worlds \
--mount type=bind,source=/voxowl/hub/secrets,target=/cubzh/secrets,readonly=true \
--with-registry-auth \
registry.digitalocean.com/cubzh/hub-http:latest
```

## Update the service

```sh
docker service update \
--image registry.digitalocean.com/cubzh/hub-http:latest \
--with-registry-auth \
hub-http
```

```sh
docker service update \
--mount-add type=bind,source=/voxowl/secrets/ANTHROPIC_API_KEY,target=/voxowl/secrets/ANTHROPIC_API_KEY \
hub-http
```

```sh
docker service update \
--mount-add type=bind,source=/voxowl/secrets/DISCORD_AUTH_TOKEN,target=/voxowl/secrets/DISCORD_AUTH_TOKEN \
hub-http
```

```sh
docker service update \
--env-add ANTHROPIC_API_KEY_FILE=/voxowl/secrets/ANTHROPIC_API_KEY \
hub-http
```

```sh
docker service update \
--mount-add type=bind,source=/voxowl/hub/badges,target=/voxowl/hub/badges \
hub-http
```