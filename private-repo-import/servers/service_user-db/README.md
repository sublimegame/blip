# Service: user-db

## Info

This is the `user-db` service running on machine `services-us-1`.
This service is the legacy MongoDB used to store user accounts for the `hub-http` service.

## Prepare the host machine

```sh
mkdir -p /voxowl/user-db/db
```

## Create the service

```sh
docker service create \
--name user-db \
--network cubzh \
--mode global \
--constraint node.labels.services-us-1==true \
--mount type=bind,source=/voxowl/user-db/db,target=/data/db \
--env MONGO_INITDB_ROOT_USERNAME=root \
--env MONGO_INITDB_ROOT_PASSWORD=root \
mongo:4.0.10
```
