# Service: user-db-admin

## Info

This is the `user-db-admin` service running on machine `services-us-1`.
This service is the admin UI (MongoExpress) for the `user-db` (MongoDB) service.

## Create the service

```sh
docker service create \
--name user-db-admin \
--mode global \
--constraint node.labels.services-us-1==true \
--network cubzh \
--env ME_CONFIG_MONGODB_SERVER=user-db \
--env ME_CONFIG_MONGODB_ADMINUSERNAME=root \
--env ME_CONFIG_MONGODB_ADMINPASSWORD=root \
--env ME_CONFIG_BASICAUTH_USERNAME=voxowl \
--env ME_CONFIG_BASICAUTH_PASSWORD=$ME_CONFIG_BASICAUTH_PASSWORD \
mongo-express:0.54
```

## Update the service

```sh
# docker service update \
# --image mongo-express:0.54 \
# --env ME_CONFIG_MONGODB_SERVER=user-db \
# --env ME_CONFIG_MONGODB_ADMINUSERNAME=root \
# --env ME_CONFIG_MONGODB_ADMINPASSWORD=root \
# --env ME_CONFIG_BASICAUTH_USERNAME=voxowl \
# --env ME_CONFIG_BASICAUTH_PASSWORD=$ME_CONFIG_BASICAUTH_PASSWORD \
# user-db-admin
```
