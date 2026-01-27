# Service: nginx-for-servers

This is the NGINX service running on "gameservers" machines:
- `gameservers-eu-1`
- `gameservers-us-1`
- `gameservers-sg-1`

## Requirements

### Install the config file on the machines

NGINX configuration file is stored on the machine and is mounted inside the container (read-only). 
This allows to use the official NGINX docker image and avoid the need of a custom one.

**Paths**

- on the machine: `/voxowl/nginx/nginx.conf`
- in the container: `/voxowl/nginx/nginx.conf`

### Install the SSL/TLS certificates on the machine

SSL/TLS certificates are stored on the machine and are mounted inside the container (read-only).

**Paths**

On the host machine

- `/voxowl/certificates/cu.bzh.chained.crt`
- `/voxowl/certificates/cu.bzh.key`

In the container

- `/voxowl/certificates/cu.bzh.chained.crt`
- `/voxowl/certificates/cu.bzh.key`

### Setup "gameservers" label on the docker nodes

```sh
docker node update --label-add gameservers=true gameservers-eu-1
docker node update --label-add gameservers=true gameservers-us-1
docker node update --label-add gameservers=true gameservers-sg-1
```

## Create the service

```sh
docker service create \
--name nginx-for-servers \
--mode global \
--constraint node.labels.gameservers==true \
--mount type=bind,readonly=true,source=/voxowl/nginx/nginx.conf,target=/etc/nginx/nginx.conf \
--mount type=bind,readonly=true,source=/voxowl/certificates,target=/voxowl/certificates \
--network cubzh \
--publish published=80,target=80,mode=host \
--publish published=443,target=443,mode=host \
nginx:1.27.3-alpine
```
