# README

### Docker Swarm setup

```
docker service create \
--name wasm-discord \
--mode global \
--network web \
--mount type=bind,source=/cubzh/certificates,destination=/cubzh/certificates \
registry.digitalocean.com/cubzh/wasm:latest-discord
```

```
docker service update --with-registry-auth --image registry.digitalocean.com/cubzh/wasm:latest-discord wasm-discord
```
