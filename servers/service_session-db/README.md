# Service: session-db

## Info

This is the `session-db` service running on machine `services-us-1`.
This service is used to store HTTP session data for the `hub-http` service.

## Create the service

```sh
docker service create \
--name session-db \
--mode global \
--constraint node.labels.services-us-1==true \
--network cubzh \
redis:7.4.2-alpine
```
