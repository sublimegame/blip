# Service: gameservers-db

## Info

This is the `gameservers-db` service running on machine `services-us-1`.
This service is used to store the list of active gameservers for the `hub-http` service.

## Create the service

```sh
docker service create \
--name gameservers-db \
--mode global \
--constraint node.labels.services-us-1==true \
--network cubzh \
redis:7.4.2-alpine
```
