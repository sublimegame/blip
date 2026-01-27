# Backup



## Backup the Hub server (MongoDB + games + items)

Connect to the Hub server machine using SSH.

Then run the following commands:

```sh
/home/ubuntu/hub_save_backup.sh
```

Download the backup archive from the server using SFTP.

Location: `/home/ubuntu/backups/hub_2025-02-03_17-04-35-590.tar.gz`



## Restore the Hub server (MongoDB + games + items)

Upload the backup archive to the server using SFTP.

- `/vx_backups/archives/hub_2025-02-03_17-04-35-590.tar.gz`
- `/vx_backups/hub_restore_backup.sh`

Then run the following commands:

```sh
/vx_backups/hub_restore_backup.sh <name of the backup>

# Example:
# /vx_backups/hub_restore_backup.sh hub_2025-02-03_17-04-35-590
```



## Backup the Hub DB (ScyllaDB)

```sh
# Get a shell inside the ScyllaDB container
docker exec -ti hub-db bash
```

```sh
# Inside the ScyllaDB container
/var/lib/scylla/hub_db_save_backup.sh
```

Download the backup archive from the container using SFTP.

Location: `/var/lib/scylla/backups/scylla_backup_2025-02-03_17-22-43-631.tar.gz`



## Restore the Hub DB (ScyllaDB)

Upload the backup archive and the restore script to the container using SFTP.

- `/var/lib/scylla/backups/my_backup.tar.gz`
- `/var/lib/scylla/hub_db_restore_backup.sh`

```bash
# Get a shell inside the ScyllaDB container
docker exec -ti hub-db bash

# Restore the backup
/var/lib/scylla/hub_db_restore_backup.sh my_backup
```
