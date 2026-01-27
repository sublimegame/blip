#!/bin/bash

#
# hub_save_backup.sh
#
# Creates a backup of the Hub server
#

# exit on error
set -e

# 
# Name of the backup being created
#
VX_BACKUP_NAME="hub_$(date +%Y-%m-%d_%H-%M-%S-%3N)"

# create the backup destination directory if it doesn't exist
HOME_PATH="/home/ubuntu"
VX_BACKUP_COLLECTION_DIR="${HOME_PATH}/backups"
VX_BACKUP_DIR_PATH="${VX_BACKUP_COLLECTION_DIR}/${VX_BACKUP_NAME}"

# create the backup destination directory if it doesn't exist
mkdir -p ${VX_BACKUP_DIR_PATH}

# Flush logs file if it exists and create it if it doesn't
echo "" > "${VX_BACKUP_DIR_PATH}/backup.log"

# get ID of user db container:
VX_USER_DB_CONTAINER=$(docker ps | grep pcubes_user-db | awk 'END {print $1}')

echo -n "Dumping the mongodb database... "
# Execute a script in the container to dump the mongodb database and make an archive of it
docker exec -ti "${VX_USER_DB_CONTAINER}" bash -c "rm -rf /data/db/dump; mongodump --out /data/db/dump" >> "$VX_BACKUP_DIR_PATH/backup.log" 2>&1
# Copy the dump to the backup directory
tar -C ${HOME_PATH}/db -zcf ${VX_BACKUP_DIR_PATH}/dump.tar.gz dump
echo "✅"

# Archive the games
echo -n "Archiving games... "
tar -C ${HOME_PATH} -zcf ${VX_BACKUP_DIR_PATH}/games.tar.gz games >> "$VX_BACKUP_DIR_PATH/backup.log" 2>&1
echo "✅"

# Archive the items
echo -n "Archiving items... "
tar -C ${HOME_PATH} -zcf ${VX_BACKUP_DIR_PATH}/items.tar.gz items >> "$VX_BACKUP_DIR_PATH/backup.log" 2>&1
echo "✅"

# Make an archive of the backup directory
echo -n "Archiving backup... "
tar -C ${VX_BACKUP_DIR_PATH}/.. -zcf ${VX_BACKUP_COLLECTION_DIR}/${VX_BACKUP_NAME}.tar.gz ${VX_BACKUP_NAME}
rm -r ${VX_BACKUP_DIR_PATH}
echo "✅"

echo "✨ Backup completed successfully"
