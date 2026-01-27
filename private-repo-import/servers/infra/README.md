# Server Infrastructure

## Machines

## OVH overview

- **manager-1** 🇺🇸 (docker swarm manager)
- **manager-2** 🇺🇸 (docker swarm manager)
- **services-us-1** 🇺🇸 (API server & other services)
- **gameservers-us-1** 🇺🇸 (gameservers)
- **gameservers-eu-1** 🇪🇺 (gameservers)
- **gameservers-sg-1** 🇸🇬 (gameservers)

## manager-1

- **Region** : US-EAST-VA
- **Machine** : OVH VPS Starter (1vCore/2GB/20GB/100Mbps)
- **OS** : Ubuntu 24.04
- **IPv4** : 135.148.121.106
- **IPv6** : 2604:2dc0:101:200::36ff
- **username** : ubuntu
- **password** : <in Passwords app>

## manager-2

- **Region** : US-WEST-OR
- **Machine** : OVH VPS Starter (1vCore/2GB/20GB/100Mbps)
- **OS** : Ubuntu 24.04
- **IPv4** : 15.204.58.162
- **IPv6** : 2604:2dc0:202:300::1e13
- **username** : ubuntu
- **password** : <in Passwords app>

## services-us-1

- **Region** : CA-EAST-BHS
- **Machine** : OVH RISE-1
- **OS** : Ubuntu 24.04
- **IPv4** : 15.235.116.53
- **IPv6** : 2607:5300:203:b135::/64
- **username** : ubuntu
- **password** : <in Passwords app>

## gameservers-eu-1

- **Region** : EU-WEST-SBG
- **Machine** : OVH RISE-GAME-1
- **OS** : Ubuntu 24.04
- **IPv4** : 141.94.97.66
- **IPv6** : 2001:41d0:403:4842::/64
- **username** : ubuntu
- **password** : <in Passwords app>

## gameservers-us-1

- **Region** : CA-EAST-BHS
- **Machine** : OVH RISE-GAME-1
- **OS** : Ubuntu 24.04
- **IPv4** : 51.222.244.45
- **IPv6** : 2607:5300:203:8d2d::/64
- **username** : ubuntu
- **password** : <in Passwords app>

## gameservers-sg-1

- **Region** : AP-SOUTHEAST-SGP
- **Machine** : OVH RISE-GAME-1
- **OS** : Ubuntu 24.04
- **IPv4** : 15.235.181.168
- **IPv6** : 2402:1f00:8001:1aa8::/64
- **username** : ubuntu
- **password** : <in Passwords app>

## Install operating system

- open the machine page in OVH control panel
- find "Operating system (OS)" section
- click on the "..." button in this section
- select "Install from an OVHcloud template"
- for "Type of OS", select "Basic"
- select OS "Ubuntu 24.04 LTS"
- click on "Next" button
- fill hostname field: "gameservers-eu-1"
- fill SSH public key field
- click on "Continue" button
- wait for the installation to complete

## Setup machines

### SSH

#### Generate keypair

```sh
ssh-keygen -t ed25519 -C "ovh@voxowl.com"
```

#### Copy SSH public key to a remote machine

```sh
# manager-1
ssh-copy-id -i ~/.ssh/id_ovh_voxowl_com.pub -o PubkeyAuthentication=no ubuntu@135.148.121.106

# manager-2
ssh-copy-id -i ~/.ssh/id_ovh_voxowl_com.pub -o PubkeyAuthentication=no ubuntu@15.204.58.162
```

You will be prompted for the SSH password.

#### SSH connect using key

```sh
# manager-1
ssh ubuntu@135.148.121.106 -i ~/.ssh/id_ovh_voxowl_com -o IdentitiesOnly=yes

# manager-2
ssh ubuntu@15.204.58.162 -i ~/.ssh/id_ovh_voxowl_com -o IdentitiesOnly=yes
```

#### Define the machine's hostname

Replace the content of `/etc/hostname` with the new hostname you'd like to use:

```sh
sudo sh -c "echo "manager-1" > /etc/hostname"
```

#### Setup Docker Swarm

Enable Swarm (first manager)

```sh
docker swarm init --advertise-addr <address reachable by other nodes>

# manager-1
docker swarm init --advertise-addr 135.148.121.106
```

Join Swarm as a manager

```sh
# manager-2
docker swarm join --token SWMTKN-1-3in6fq9bq7dowwgumtstgjxql1n6og2jjfzc63ihv2oozlrdll-3x60hrj9t01zd5scuaz8mcc8g 135.148.121.106:2377
```

Join Swarm as a worker

```sh
# services-us-1
docker swarm join --token SWMTKN-1-3in6fq9bq7dowwgumtstgjxql1n6og2jjfzc63ihv2oozlrdll-3mqxawqm38qartlpz1fkzoyox 135.148.121.106:2377
```

## Networks

Create our own overlay network.

```sh
# network has to be attachable for gameserver instances to be connected to it
docker network create --attachable --driver overlay cubzh
```

## Services

### discordbot

TODO

### fileserver

-> `servers/service_fileserver`

### hub

TODO

### link

-> `servers/service_link`

### lua-docs

-> `servers/service_lua-docs`

### nginx-for-services

-> `servers/service_nginx-for-services`

### parent-dashboard

-> `servers/service_parent-dashboard`

### tracking

-> `servers/service_tracking`

### wasm

-> `servers/service_wasm`
