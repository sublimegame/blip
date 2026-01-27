# Service: nginx-for-services

This is the NGINX service running on machine `services-us-1`.

## Requirements

### Install the config file on the machine

NGINX configuration file is stored on the machine and is mounted inside the container (read-only). This allows to use the official NGINX docker image and avoid the need of a custom one.

**Paths**

- on the machine: `/voxowl/nginx/nginx.conf`
- in the container: `/voxowl/nginx/nginx.conf`

### Install the SSL/TLS certificates on the machine

SSL/TLS certificates are stored on the machine and are mounted inside the container (read-only).

**Paths**

- on the machine: `/voxowl/certificates/cu.bzh`
- in the container: `/voxowl/certificates/cu.bzh`

## Create the service

```sh
docker service create \
--name nginx-for-services \
--mode global \
--constraint node.labels.services-us-1==true \
--mount type=bind,readonly=true,source=/voxowl/nginx/nginx.conf,target=/etc/nginx/nginx.conf \
--mount type=bind,readonly=true,source=/voxowl/certificates/cu.bzh,target=/voxowl/certificates/cu.bzh \
--network cubzh \
--publish published=80,target=80,mode=host \
--publish published=443,target=443,mode=host \
nginx:1.27.3-alpine
```
