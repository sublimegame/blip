# Service: lua-docs

This is the `lua-docs` service running on machine `services-us-1`.

## Build docker image

```sh
```

## Push docker image

```sh
```

## Update service

```sh
docker service update \
--image registry.digitalocean.com/cubzh/lua-docs:latest \
--with-registry-auth \
lua-docs
```

## Create service

```sh
docker service create \
--name lua-docs \
--mode global \
--network web \
--constraint node.labels.services-us-1==true \
--with-registry-auth \
registry.digitalocean.com/cubzh/lua-docs:latest
```
