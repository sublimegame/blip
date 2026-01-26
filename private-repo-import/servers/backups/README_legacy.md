# Server applications

Back to local:

```
eval $(docker-machine env -u)
```

## GAETAN'S NOTES

Import Digital Ocean's droplet into docker-machine

```shell
docker-machine create --driver generic --generic-ip-address=138.197.185.53 --generic-ssh-key ~/.ssh/id_rsa_srv pcubes-gameservers-eu-3
```

```shell
ssh -i ~/.ssh/id_rsa_srv root@138.197.185.53
```

```shell
docker swarm join \
--token SWMTKN-1-5ow10ej2ybj80ynq8v2hv975c9duvbwgh9oui4d3p5dz0cdoe2-5twlj7nx12iqjfmb6my4jxr7s \
--advertise-addr 138.197.185.53 \
18.190.42.21:2377
```


## Deploy prod server

### Init Swarm Manager

```shell
eval $(docker-machine env pcubes-manager-1)
docker swarm init --advertise-addr 18.190.42.21
```

`18.190.42.21` is the public IP here.

### Add workers:

#### US-EAST

##### Worker (1)

docker swarm join --token SWMTKN-1-5ow10ej2ybj80ynq8v2hv975c9duvbwgh9oui4d3p5dz0cdoe2-5twlj7nx12iqjfmb6my4jxr7s 18.190.42.21:2377

```shell
docker swarm join \
--token SWMTKN-1-5ow10ej2ybj80ynq8v2hv975c9duvbwgh9oui4d3p5dz0cdoe2-5twlj7nx12iqjfmb6my4jxr7s \
--advertise-addr 18.190.48.38 \
18.190.42.21:2377
```

##### Worker (2)

```shell
docker swarm join \
--token SWMTKN-1-5ow10ej2ybj80ynq8v2hv975c9duvbwgh9oui4d3p5dz0cdoe2-5twlj7nx12iqjfmb6my4jxr7s \
--advertise-addr 3.129.97.153 \
18.190.42.21:2377
```

#### Europe

```shell
docker swarm join \
--token SWMTKN-1-5ow10ej2ybj80ynq8v2hv975c9duvbwgh9oui4d3p5dz0cdoe2-5twlj7nx12iqjfmb6my4jxr7s \
--advertise-addr 13.36.138.117 \
18.190.42.21:2377
```

### Set labels

#### Manager (1)

```
docker node update --label-add master=1 "$NODE_ID"
```

#### Worker (1)

```
docker node update --label-add tracking=1 "$NODE_ID"
```

#### Worker (2)

```
docker node update --label-add websites=1 "$NODE_ID"
```

#### Gameservers EU

```
docker node update --label-add gameservers-eu=1 "$NODE_ID"
```

### Run!




## Update prod files

```
docker-machine scp -r testItems pcubes-manager-1:/tmp/
docker-machine ssh pcubes-manager-1

cd /tmp/testItems
sudo rm -r /particubes_items/*
sudo mv * /particubes_items/

cd /tmp/prod_particubes_games
sudo rm -r /particubes_games/*
sudo mv * /particubes_games/
```

## Local server environment

The simplest way to launch a local server environment:

```shell
# from particubes-worlds directory
./accounts.sh
# use "test" when prompted
# "dev" should be used when debugging server code
```

Note: do not commit the update in `Config.plist`.

## Debug game server

Local server environment should be running for this to work. If it's not the case, please read "local server environment" section.

```shell
launch-debug-gameserver.sh
./build_deps.sh && go run *.go <game_id> srv:debug http://accounts
```

Note: do not commit the update in `Config.plist` (adding debug server to the list)

## Docker machines

Command to create a new machine on Google Cloud:

```
docker-machine create pcubes-manager-2 \
-d google \
--google-machine-type n1-standard-1 \
--google-tags particubes-swarm \
--google-project pcubes \
--google-zone us-east4-a
```

NOTE: We're not using Swarm currently, but it's a good thing tag machines with `particubes-swarm` label in case we want to use it in the future.

Available zones: [https://cloud.google.com/compute/docs/regions-zones/#available](https://cloud.google.com/compute/docs/regions-zones/#available)

## Mongo

```
mongodump --out <dir>
tar -zcvf db-2020-03-30.tar.gz <dir>
```

https://towardsdatascience.com/how-to-deploy-a-mongodb-replica-set-using-docker-6d0b9ac00e49

How to create key file for replicas:

```
openssl rand -base64 741 > mongo-keyfile
chmod 600 mongo-keyfile
```

```
docker exec <container> bash -c 'mkdir /data/keyfile /data/admin'
```

```
# put files in the container
admin.js -> /data/admin/
replica.js -> /data/admin/
mongo-keyfile -> /data/keyfile/
```

```
# change folder owner to the user container
$ docker exec mongoNode1 bash -c 'chown -R mongodb:mongodb /data'
```

```
# env file
MONGO_USER_ADMIN=admin
MONGO_PASS_ADMIN=5GBUJ3EJMGHW6SYJ
MONGO_REPLICA_ADMIN=replicaAdmin
MONGO_PASS_REPLICA=2XXATQC7133ZPXNU
```

## Backup

Nice article: [https://medium.com/faun/how-to-backup-docker-containered-mongo-db-with-mongodump-and-mongorestore-b4eb1c0e7308](https://medium.com/faun/how-to-backup-docker-containered-mongo-db-with-mongodump-and-mongorestore-b4eb1c0e7308)

### Dump db files

```shell
# from exe repo (/keys)
./ssh-prod-worker-3.sh

# get ID of user db container:
USER_DB_CONTAINER=$(docker ps | grep pcubes_user-db | awk 'END {print $1}')

docker exec -ti "$USER_DB_CONTAINER" bash

rm -rf /data/db/dump
mongodump --out /data/db/dump
cd /data/db
tar -zcvf dump.tar.gz dump

exit
exit
```

### Create archive of games and items

```shell
# from exe repo (/keys)
./ssh-prod-worker-3.sh

tar -zcvf games.tar.gz games
tar -zcvf items.tar.gz items
```

### Get files over SFTP and put them in the private repo

#### SFTP:

- `ubuntu@18.190.42.21`
- Remote path: `/home/ubuntu`

### Destination:

Put files in `servers/backups/<date>/`:

- `db/dump.tar.gz` -> `dump.tar.gz`
- `games.tar.gz` -> `/particubes_games`
- `items.tar.gz` -> `/particubes_items`

Create an archive: 

```shell
cd servers/backups
tar -zcvf <date>.tar.gz <date>
```

Note: it would be better to use a replica for the backup, disconnecting it from the network while doing it.

## Restore

Use SFTP to connect to the node. 

Upload files from backup:

- `particubes_games` -> `/particubes_games`
- `particubes_items` -> `/particubes_items`
- `dump.tar.gz` -> `/particubes/data/db/dump.tar.gz`

Extract MongoDB backup:

```shell
docker exec -ti sshd ash
cd /particubes/data/db
tar -zxvf "dump.tar.gz" -C "/particubes/data/db"
exit
```

Restore mongo data:

```
docker exec -ti user-db mongorestore /data/db/dump
```

# NOTE: MAKE SURE THERE ARE NO PERMISSION ISSUES

