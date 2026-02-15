# Service: tracking

This is the `tracking` service running on machine `services-us-1`.

## Build docker image

```sh
docker build \
  --platform linux/amd64 \
  --file servers/service_tracking/tracking.Dockerfile \
  --tag registry.digitalocean.com/cubzh/tracking:latest \
  .
```

## Push docker image

```sh
docker push registry.digitalocean.com/cubzh/tracking:latest
```

## Update service

```sh
docker service update \
--image registry.digitalocean.com/cubzh/tracking:latest \
--with-registry-auth \
tracking
```

## Create service

```sh
docker service create \
--name tracking \
--mode global \
--network cubzh \
--env CZH_TRACK_CHANNEL=prod \
--constraint node.labels.services-us-1==true \
--with-registry-auth \
registry.digitalocean.com/cubzh/tracking:latest
```
