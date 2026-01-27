# Service: discordbot

This is the `discordbot` service running on machine `services-us-1`.

## Build docker image

```sh
docker build \
--platform linux/amd64 \
--file dockerfiles/discordbot.Dockerfile \
--tag registry.digitalocean.com/cubzh/discordbot:latest \
.
```

## Push docker image

```sh
docker push registry.digitalocean.com/cubzh/discordbot:latest
```

## Update service

```sh
docker service update \
--image registry.digitalocean.com/cubzh/discordbot:latest \
--with-registry-auth \
discordbot
```

## Create service

```sh
docker service create \
--name discordbot \
--mode global \
--network cubzh \
--env TOKEN=${DISCORD_BOT_TOKEN} \
--mount type=bind,source=/voxowl/secrets/apns,target=/voxowl/secrets/apns \
--mount type=bind,source=/voxowl/secrets/firebase,target=/voxowl/secrets/firebase \
--constraint node.labels.services-us-1==true \
--with-registry-auth \
registry.digitalocean.com/cubzh/discordbot:latest
```

> **Note:** Set the `DISCORD_BOT_TOKEN` environment variable before running this command, or get the value from the `.env` file.

## Add file mount

```sh
docker service update \
--mount-add type=bind,source=/voxowl/secrets/apns,target=/voxowl/secrets/apns \
--mount-add type=bind,source=/voxowl/secrets/firebase,target=/voxowl/secrets/firebase \
discordbot
```
