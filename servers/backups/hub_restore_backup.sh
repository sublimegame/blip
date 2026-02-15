#!/bin/bash

#
# Restore a backup of the Hub server (MongoDB, worlds and items)
#
# Requirements:
# - The backup must be in the /vx_backups/archives directory
# - The backup must be named like this: hub_2025-02-03_08-43-44-006
#
# How to run it:
# /vx_backups/hub_restore_backup.sh hub_2025-02-03_08-43-44-006
#

# exit on error
set -e

# location of the backup collection
VX_BACKUP_COLLECTION_DIR="/voxowl/vx_backups/archives"

# ⚠️
# ⚠️ Name of the backup to restore
# ⚠️
# example: hub_2025-02-02_21-58-23-009
VX_BACKUP_NAME=""

# if an argument is passed, use it as the backup name
if [ -n "$1" ]; then
    VX_BACKUP_NAME="$1"
fi

# Exit with an error if the backup name is not set
if [ -z "$VX_BACKUP_NAME" ]; then
    echo "❌ Backup name is not provided"
    exit 1
fi

# make sure the backup name is valid
if ! echo "$VX_BACKUP_NAME" | grep -qE '^hub_[0-9]{4}-[0-9]{2}-[0-9]{2}_[0-9]{2}-[0-9]{2}-[0-9]{2}-[0-9]{3}$'; then
    echo "❌ Backup name is not valid"
    exit 1
fi

# create the backup destination directory if it doesn't exist
VX_BACKUP_DIR_PATH="${VX_BACKUP_COLLECTION_DIR}/${VX_BACKUP_NAME}"
VX_BACKUP_ARCHIVE_PATH="${VX_BACKUP_DIR_PATH}.tar.gz"
VX_MONGODB_VOLUME_PATH="/voxowl/user-db/db"
VX_WORLDS_VOLUME_PATH="/voxowl/hub/worlds"
VX_ITEMS_VOLUME_PATH="/voxowl/hub/items"

# Extract the backup archive
echo -n "Extracting backup archive... ⚙️"
tar -C ${VX_BACKUP_COLLECTION_DIR} -zxf ${VX_BACKUP_ARCHIVE_PATH}
echo -e "\b\b✅"

# Copy the MongoDB dump to the docker volume on the host machine (/voxowl/user-db/db)
echo -n "Extracting MongoDB dump... ⚙️"
# remove ${VX_MONGODB_VOLUME_PATH}/dump directory if it exists
rm -rf ${VX_MONGODB_VOLUME_PATH}/dump
mkdir -p ${VX_MONGODB_VOLUME_PATH}
tar -C ${VX_MONGODB_VOLUME_PATH} -zxf ${VX_BACKUP_DIR_PATH}/dump.tar.gz
echo -e "\b\b✅"

echo "Loading MongoDB dump... ⚙️"
# Find container ID of the user-db service
VX_USER_DB_CONTAINER_ID=$(docker ps -q --filter "name=^user-db\.")
docker exec ${VX_USER_DB_CONTAINER_ID} mongorestore -u root -p root /data/db/dump
echo -e "\b\b✅"

# Extract and install worlds (games)
echo -n "Extracting and installing worlds... ⚙️"
tar -C ${VX_BACKUP_DIR_PATH} -zxf ${VX_BACKUP_DIR_PATH}/games.tar.gz
# Change owner of the games directory to the current user
chown -R $(whoami):$(whoami) ${VX_BACKUP_DIR_PATH}/games
# Update permissions (not sure this is needed)
chmod -R 740 ${VX_BACKUP_DIR_PATH}/games
# atomically replace the worlds directory with the new one
mkdir -p ${VX_WORLDS_VOLUME_PATH}
mv ${VX_WORLDS_VOLUME_PATH} ${VX_WORLDS_VOLUME_PATH}.old
mv ${VX_BACKUP_DIR_PATH}/games ${VX_WORLDS_VOLUME_PATH}
rm -rf ${VX_WORLDS_VOLUME_PATH}.old
echo -e "\b\b✅"

# Extract and install items
echo -n "Extracting and installing items... ⚙️"
tar -C ${VX_BACKUP_DIR_PATH} -zxf ${VX_BACKUP_DIR_PATH}/items.tar.gz
# Change owner of the items directory to the current user
chown -R $(whoami):$(whoami) ${VX_BACKUP_DIR_PATH}/items
# Update permissions (not sure this is needed)
chmod -R 740 ${VX_BACKUP_DIR_PATH}/items
# atomically replace the items directory with the new one
mkdir -p ${VX_ITEMS_VOLUME_PATH}
mv ${VX_ITEMS_VOLUME_PATH} ${VX_ITEMS_VOLUME_PATH}.old
mv ${VX_BACKUP_DIR_PATH}/items ${VX_ITEMS_VOLUME_PATH}
rm -rf ${VX_ITEMS_VOLUME_PATH}.old
echo -e "\b\b✅"

echo "Flushing extracted backup... ⚙️"
rm -rf ${VX_BACKUP_DIR_PATH}
echo -e "\b\b✅"

echo "✨ Backup restored successfully"
