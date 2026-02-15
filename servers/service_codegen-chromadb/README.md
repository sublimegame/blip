# Service: codegen-chromadb

This service runs on machine `services-us-1`.

## Create the service

```sh
docker service create \
--name codegen-chromadb \
--network cubzh \
--mode global \
--constraint node.labels.services-us-1==true \
--with-registry-auth \
registry.digitalocean.com/cubzh/codegen-chromadb-embeddings:latest
```

## Update swarm service (change image)

```sh
docker service update \
--image registry.digitalocean.com/cubzh/codegen-chromadb-embeddings:latest \
--with-registry-auth \
codegen-chromadb
```
