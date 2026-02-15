# Service: hub-db

## Info

This is the `hub-db` service running on machine `services-us-1`.
This service is the ScyllaDB database for the `hub-http` service.

### ScyllaDB data storage

`/voxowl/hub-db/scylla` -> `/var/lib/scylla`

### ScyllaDB backups storage

`/voxowl/hub-db/scylla/backups` -> `/var/lib/scylla/backups`

## Create the service

```sh
docker service create \
--name hub-db \
--network cubzh \
--mode global \
--mount type=bind,source=/voxowl/hub-db/scylla,target=/var/lib/scylla \
--constraint node.labels.services-us-1==true \
scylladb/scylla:6.2.2 --smp 1 --memory 8G
```

## Create a backup

see [../backups/backup_save_hub.sh](../backups/backup_save_hub.sh)

## Restore a backup

see [../backups/backup_restore_hub.sh](../backups/backup_restore_hub.sh)

## --- DEV (ScyllaDB) ---

### Create a backup

```sh
# remove container "scylla-restore" if exists
docker rm -f scylla-restore > /dev/null 2>&1

# run local container with scylla
docker run -d \
--name scylla-restore \
-v ~/projects/voxowl/particubes-private/servers/backups/archives:/var/lib/scylla/backups \
-v ~/projects/voxowl/particubes-private/servers/backups/hub_db_restore_backup.sh:/var/lib/scylla/hub_db_restore_backup.sh \
scylladb/scylla:6.2.2 --smp 1

# get bash shell in the container
docker exec -ti scylla-restore bash
```

```sh
/var/lib/scylla/hub_db_restore_backup.sh scylla_backup_2025-02-01_11-00-34.490
```

```sh
cqlsh -e "SELECT count(*) FROM universe.user_notification;"
cqlsh -e "SELECT count(*) FROM kvs.unordered;"
```

## --- DEV (Hub) ---

```sh
docker run -ti --rm \
--name hub-restore \
-v ~/projects/voxowl/particubes-private/servers/backups:/vx_backups \
ubuntu:22.04 bash
```

```sh
./hub_restore_backup.sh hub_2025-02-02-21-58-23-009
```