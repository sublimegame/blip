# Service: parent-dashboard

This is the `parent-dashboard` service running on machine `services-us-1`.

## Build docker image

```sh
docker build \
  --platform linux/amd64 \
  --file dockerfiles/parent-dashboard.Dockerfile \
  --tag registry.digitalocean.com/cubzh/parent-dashboard:latest \
  .
```

## Push docker image

```sh
docker push registry.digitalocean.com/cubzh/parent-dashboard:latest
```

## Update the service

```sh
docker service update \
--image registry.digitalocean.com/cubzh/parent-dashboard:latest \
--with-registry-auth \
parent-dashboard
```

## Create the service

```sh
docker service create \
--name parent-dashboard \
--mode global \
--constraint node.labels.services-us-1==true \
--network cubzh \
--with-registry-auth \
registry.digitalocean.com/cubzh/parent-dashboard:latest
```
