# Service: fileserver

This service runs on machine `services-us-1`.

## Requirements

### Install the files on the machine

Files being served by the fileserver must be present on the machine.

**Paths**

- on the machine: `/voxowl/fileserver/files`
- in the container: `/voxowl/fileserver/files`

#### Build docker image

```sh
docker build \
  --platform linux/amd64 \
  --file dockerfiles/fileserver.Dockerfile \
  --tag registry.digitalocean.com/cubzh/fileserver:latest \
  .
```

#### Push docker image

```sh
docker push registry.digitalocean.com/cubzh/fileserver:latest
```

#### Update the service

```sh
docker service update --with-registry-auth --image registry.digitalocean.com/cubzh/fileserver:latest fileserver
```

#### Create the service

```sh
docker service create \
--name fileserver \
--mode global \
--constraint node.labels.services-us-1==true \
--mount type=bind,readonly=true,source=/voxowl/fileserver,target=/voxowl/fileserver \
--network cubzh \
--with-registry-auth \
registry.digitalocean.com/cubzh/fileserver:latest
```
