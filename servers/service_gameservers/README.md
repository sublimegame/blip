# Service gameservers

There are 3 services:

- `gameservers-eu`
- `gameservers-sg`
- `gameservers-us`

## Build docker images

```sh
docker build \
--platform linux/amd64 \
--file dockerfiles/scheduler.Dockerfile \
--tag registry.digitalocean.com/cubzh/scheduler:latest \
.
```

## Push docker images to the registry

```sh
docker push registry.digitalocean.com/cubzh/scheduler:latest
```

## Update service

```sh
# EU
docker service update \
--image registry.digitalocean.com/cubzh/scheduler:latest \
--with-registry-auth \
gameservers-eu

# SG
docker service update \
--image registry.digitalocean.com/cubzh/scheduler:latest \
--with-registry-auth \
gameservers-sg

# US
docker service update \
--image registry.digitalocean.com/cubzh/scheduler:latest \
--with-registry-auth \
gameservers-us
```

## Create service

```sh
# EU
docker service create \
--name gameservers-eu \
--mode global \
--network cubzh \
--mount type=bind,source=/var/run/docker.sock,target=/var/run/docker.sock \
--constraint node.labels.gameservers==true \
--constraint node.labels.region==eu \
--with-registry-auth \
registry.digitalocean.com/cubzh/scheduler:latest

# SG
docker service create \
--name gameservers-sg \
--mode global \
--network cubzh \
--mount type=bind,source=/var/run/docker.sock,target=/var/run/docker.sock \
--constraint node.labels.gameservers==true \
--constraint node.labels.region==sg \
--with-registry-auth \
registry.digitalocean.com/cubzh/scheduler:latest

# US
docker service create \
--name gameservers-us \
--mode global \
--network cubzh \
--mount type=bind,source=/var/run/docker.sock,target=/var/run/docker.sock \
--constraint node.labels.gameservers==true \
--constraint node.labels.region==us \
--with-registry-auth \
registry.digitalocean.com/cubzh/scheduler:latest
```

<!-- ================================ -->
<!-- ================================ -->
<!-- ================================ -->

```sh
docker service rm gameservers-eu
docker service rm gameservers-sg
docker service rm gameservers-us
```

## Add labels to the nodes

```sh
docker node update --label-add region=eu gameservers-eu-1
docker node update --label-add region=sg gameservers-sg-1
docker node update --label-add region=us gameservers-us-1
```


<!-- TODO: LATER: use a single service for all regions -->

```sh
docker service update \
--image registry.digitalocean.com/cubzh/scheduler:latest \
--with-registry-auth \
gameservers
```

```sh
docker service create \
--name gameservers \
--mode global \
--network cubzh \
--mount type=bind,source=/var/run/docker.sock,target=/var/run/docker.sock \
--constraint node.labels.gameservers==true \
--with-registry-auth \
registry.digitalocean.com/cubzh/scheduler:latest
```

⚠️ FOR THIS WE NEED TO REPLACE "gameservers-eu" (sg/us) by "gameservers" in the scheduler code and/or the hub-http code.
