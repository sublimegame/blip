# REGISTRY

## Deploy

> TODO

## Cleanup

```shell
# In each repository:
/registry/docker/registry/v2/repositories/*

# Delete old manifest:
cd /registry/docker/registry/v2/repositories/<REPO>/_manifests

# list files created more than 100 days ago and delete them
find . -maxdepth 1 -mtime +100 | sudo xargs rm -rf

# when done removing files, run registry command to cleanyp the registry:
# dry run:
sudo docker exec registry bin/registry garbage-collect --dry-run /etc/docker/registry/config.yml
# real thing:
sudo docker exec registry bin/registry garbage-collect /etc/docker/registry/config.yml
```

Complete instructions: https://betterprogramming.pub/cleanup-your-docker-registry-ef0527673e3a



## FULL SCRIPT

```bash
# dashboard
cd /registry/docker/registry/v2/repositories/dashboard/_manifests/revisions/sha256 && \
find . -maxdepth 1 -mtime +30 | sudo xargs rm -rf
sudo docker exec registry bin/registry garbage-collect /etc/docker/registry/config.yml

# discordbot
cd /registry/docker/registry/v2/repositories/discordbot/_manifests/revisions/sha256 && \
find . -maxdepth 1 -mtime +30 | sudo xargs rm -rf
sudo docker exec registry bin/registry garbage-collect /etc/docker/registry/config.yml

# fileserver
cd /registry/docker/registry/v2/repositories/fileserver/_manifests/revisions/sha256 && \
find . -maxdepth 1 -mtime +30 | sudo xargs rm -rf
sudo docker exec registry bin/registry garbage-collect /etc/docker/registry/config.yml

# gameserver
cd /registry/docker/registry/v2/repositories/gameserver/_manifests/revisions/sha256 && \
find . -maxdepth 1 -mtime +30 | sudo xargs rm -rf
sudo docker exec registry bin/registry garbage-collect /etc/docker/registry/config.yml

# hub-http
cd /registry/docker/registry/v2/repositories/hub-http/_manifests/revisions/sha256 && \
find . -maxdepth 1 -mtime +30 | sudo xargs rm -rf
sudo docker exec registry bin/registry garbage-collect /etc/docker/registry/config.yml

# nginx-web
cd /registry/docker/registry/v2/repositories/nginx-web/_manifests/revisions/sha256 && \
find . -maxdepth 1 -mtime +30 | sudo xargs rm -rf
sudo docker exec registry bin/registry garbage-collect /etc/docker/registry/config.yml

# parent-dashboard
cd /registry/docker/registry/v2/repositories/parent-dashboard/_manifests/revisions/sha256 && \
find . -maxdepth 1 -mtime +30 | sudo xargs rm -rf
sudo docker exec registry bin/registry garbage-collect /etc/docker/registry/config.yml

# ping
cd /registry/docker/registry/v2/repositories/ping/_manifests/revisions/sha256 && \
find . -maxdepth 1 -mtime +30 | sudo xargs rm -rf
sudo docker exec registry bin/registry garbage-collect /etc/docker/registry/config.yml

# scheduler
cd /registry/docker/registry/v2/repositories/scheduler/_manifests/revisions/sha256 && \
find . -maxdepth 1 -mtime +30 | sudo xargs rm -rf
sudo docker exec registry bin/registry garbage-collect /etc/docker/registry/config.yml

# tracking
cd /registry/docker/registry/v2/repositories/tracking/_manifests/revisions/sha256 && \
find . -maxdepth 1 -mtime +30 | sudo xargs rm -rf
sudo docker exec registry bin/registry garbage-collect /etc/docker/registry/config.yml

# wasm
cd /registry/docker/registry/v2/repositories/wasm/_manifests/revisions/sha256 && \
find . -maxdepth 1 -mtime +30 | sudo xargs rm -rf
sudo docker exec registry bin/registry garbage-collect /etc/docker/registry/config.yml
```