# README

## Setup server

### Update the system

```
$ sudo apt update
$ sudo apt upgrade -y
```

### Enable byobu (optional)

```
$ byobu-enable
$ exit
```

⚠️ Note: you will need to disconnect and re-connect to get a byobu session.

### Install Docker

```
$ sudo apt update
$ sudo apt install -y docker.io
```

You can check the docker version and make sure the docker client can talk to the docker server (daemon):

```
$ docker version
Client:
 Version:           20.10.17
 API version:       1.41
 Go version:        go1.16.15
 Git commit:        100c70180f
 Built:             Thu Sep 22 06:21:41 2022
 OS/Arch:           linux/amd64
 Context:           default
 Experimental:      true

Server:
 Engine:
  Version:          20.10.17
  API version:      1.41 (minimum version 1.12)
  Go version:       go1.16.15
  Git commit:       a89b842
  Built:            Thu Sep 22 06:22:13 2022
  OS/Arch:          linux/amd64
  Experimental:     false
 containerd:
  Version:          v1.6.6
  GitCommit:        10c12954828e7c7c9b6e0ea9b0c02b01407d3ae1
 runc:
  Version:          1.1.2
  GitCommit:
 docker-init:
  Version:          0.19.0
  GitCommit:        de40ad0
```

If you see this kind of return with this error:

```
Client:
 Version:           20.10.17
 API version:       1.41
 Go version:        go1.16.15
 Git commit:        100c70180f
 Built:             Thu Sep 22 06:21:41 2022
 OS/Arch:           linux/amd64
 Context:           default
 Experimental:      true
Got permission denied while trying to connect to the Docker daemon socket at unix:///var/run/docker.sock: Get "http://%2Fvar%2Frun%2Fdocker.sock/v1.24/version": dial unix /var/run/docker.sock: connect: permission denied
```

You'll need to add your user (`ubuntu`) to the `docker` user group:

```
$ sudo usermod -aG docker ubuntu
```

⚠️ Note: you will need to disconnect and re-connect to get a new user session, and see the "permission denied" error disappear.

### Set machine hostname

We can set the name of the new machine, so we can identify it easily in the future.

Replace the content of `/etc/hostname` with the new hostname you'd like to use:

```
$ sudo nano /etc/hostname
```

Hostname should be `placeholder`.

Update the hostname used in `/etc/hosts` if the old hostname was used in it (it might not be needed):

```
$ sudo nano /etc/hosts
```

Reboot the machine:

```
$ sudo reboot
```

### Setup Docker Swarm

```
$ docker swarm init --advertise-addr 10.116.0.2
```

### Docker Swarm setup

```
docker network create --driver overlay web
```

```
docker service create \
--name nginx \
--mode global \
--network web \
--publish published=443,target=443,mode=host \
--publish published=80,target=80,mode=host \
--mount type=bind,source=/voxowl/nginx/nginx.conf,destination=/etc/nginx/nginx.conf \
--mount type=bind,source=/voxowl/cubzh/certificates,destination=/cubzh/certificates \
nginx:1.27.1-alpine
```

```
docker service create \
--name appmaker-server \
--mode global \
--network web \
--mount type=bind,source=/voxowl/appmaker/logs,destination=/appmaker/logs \
registry.digitalocean.com/cubzh/appmaker-server:latest
```

```
docker service update --with-registry-auth --image registry.digitalocean.com/cubzh/appmaker-server:latest appmaker-server
```
