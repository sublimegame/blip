#!/bin/bash

#
# hub_db_restore_backup.sh
#
# Restores a backup of the Hub DB (ScyllaDB)
#
# This must be run inside the ScyllaDB container
#
# How to run it from inside the container:
# /var/lib/scylla/backups/hub_db_restore_backup.sh scylla_backup_2025-01-31_22-01-39.431
#
# How to run it from outside the container:
# docker exec -ti hub-db bash -c "/var/lib/scylla/backups/hub_db_restore_backup.sh scylla_backup_2025-01-31_22-01-39.431"
#

# exit on error
set -e

# ⚠️
# ⚠️ Name of the backup to restore
# ⚠️
# example: scylla_backup_2025-02-02_19-06-15-859
export VX_SCYLLA_BACKUP_NAME=""

# if an argument is passed, use it as the backup name
if [ -n "$1" ]; then
    VX_SCYLLA_BACKUP_NAME="$1"
fi

# Exit with an error if the backup name is not set
if [ -z "$VX_SCYLLA_BACKUP_NAME" ]; then
    echo "❌ Backup name is not set"
    exit 1
fi

# make sure the backup name is valid
if ! echo "$VX_SCYLLA_BACKUP_NAME" | grep -qE '^scylla_backup_[0-9]{4}-[0-9]{2}-[0-9]{2}_[0-9]{2}-[0-9]{2}-[0-9]{2}-[0-9]{3}$'; then
    echo "❌ Backup name is not valid"
    exit 1
fi

# fullpath of a backup archive
export VX_SCYLLA_BACKUP_DIR_PATH=/var/lib/scylla/backups
export VX_SCYLLA_BACKUP_ARCHIVE_PATH=${VX_SCYLLA_BACKUP_DIR_PATH}/${VX_SCYLLA_BACKUP_NAME}.tar.gz
export VX_SCYLLA_BACKUP_EXTRACTED_DIR_PATH=${VX_SCYLLA_BACKUP_DIR_PATH}/${VX_SCYLLA_BACKUP_NAME}

# extract the backup
echo "⭐️ Extracting the backup..."
tar -C ${VX_SCYLLA_BACKUP_DIR_PATH} -zxf ${VX_SCYLLA_BACKUP_ARCHIVE_PATH}
# Now the backup is in ${VX_SCYLLA_BACKUP_EXTRACTED_DIR_PATH}

# Make sure ScyllaDB is running
echo "⭐️ Making sure ScyllaDB is running..."
supervisorctl start scylla
sleep 5

# recreate the DB schema
echo "⭐️ Recreating the DB schema..."
# remove all keyspaces
cqlsh -e "DROP KEYSPACE IF EXISTS kvs;"
cqlsh -e "DROP KEYSPACE IF EXISTS moderation;"
cqlsh -e "DROP KEYSPACE IF EXISTS universe;"
# recreate the DB schema
cqlsh -f ${VX_SCYLLA_BACKUP_EXTRACTED_DIR_PATH}/db_schema_complete.cql

# remove all Materialized Views
# cqlsh -e "SELECT view_name FROM system_schema.views;"
echo "⭐️ Removing all Materialized Views..."

cqlsh -e "DROP MATERIALIZED VIEW kvs.leaderboard_by_score;"
rm -rf /var/lib/scylla/data/kvs/leaderboard_by_score-*

cqlsh -e "DROP MATERIALIZED VIEW moderation.chat_message_by_user;"
rm -rf /var/lib/scylla/data/moderation/chat_message_by_user-*

cqlsh -e "DROP MATERIALIZED VIEW moderation.chat_message_by_server;"
rm -rf /var/lib/scylla/data/moderation/chat_message_by_server-*

# remove all Secondary Indexes
# cqlsh -e "SELECT index_name FROM system_schema.indexes;"
echo "⭐️ Removing all Secondary Indexes..."

cqlsh -e "DROP INDEX universe.user_notification_read_idx;"
rm -rf /var/lib/scylla/data/universe/user_notification_read_idx_index-*

cqlsh -e "DROP INDEX universe.user_notification_category_idx;"
rm -rf /var/lib/scylla/data/universe/user_notification_category_idx_index-*

cqlsh -e "DROP INDEX universe.universe_parent_dashboard_by_child;"
rm -rf /var/lib/scylla/data/universe/universe_parent_dashboard_by_child_index-*

cqlsh -e "DROP INDEX universe.universe_parent_dashboard_by_parent;"
rm -rf /var/lib/scylla/data/universe/universe_parent_dashboard_by_parent_index-*

# restore the backup
echo "⭐️ Stopping scylla..."
nodetool drain
# Stop the scylla service
supervisorctl stop scylla
# Delete all the files in the commitlog. 
echo "⭐️ Deleting the commitlog and data files..."
# Deleting the commitlog will prevent the newer insert from overriding the restored data.
rm -rf /var/lib/scylla/commitlog/*
# Delete all the files in the <keyspace_name_table>. 
# Note that by default the snapshots are created under ScyllaDB data directory
# sudo rm -f /var/lib/scylla/data/<keyspace_name>/<table_name-UUID>/
# Delete all data files in our keyspaces (but not directories)
rm -f /var/lib/scylla/data/kvs/*/* 2>/dev/null || true
rm -f /var/lib/scylla/data/moderation/*/* 2>/dev/null || true
rm -f /var/lib/scylla/data/universe/*/* 2>/dev/null || true

echo "⭐️ Making sure snapshot files are owned by the scylla user and group..."
# Make sure that all files are owned by the scylla user and group
find ${VX_SCYLLA_BACKUP_EXTRACTED_DIR_PATH}/data/ -type d -name ${VX_SCYLLA_BACKUP_NAME} -path "*/snapshots/${VX_SCYLLA_BACKUP_NAME}" -exec sh -c '\
sudo chown -R scylla:scylla ${1}' _ {} \;

# Restore snapshot files
echo "⭐️ Restoring snapshot files..."
# Copy snapshots directories from the backup to the data directory
find ${VX_SCYLLA_BACKUP_EXTRACTED_DIR_PATH}/data/ -type d -name ${VX_SCYLLA_BACKUP_NAME} -path "*/snapshots/${VX_SCYLLA_BACKUP_NAME}" -exec sh -c '\
dir=$(echo "$1" | sed "s|/backups/${VX_SCYLLA_BACKUP_NAME}||"); \
dir=${dir%/*/*}; \
mkdir -p "${dir}"; \
cp -r "$1"/* "${dir}"' _ {} \;

# Remove the extracted backup
rm -rf ${VX_SCYLLA_BACKUP_EXTRACTED_DIR_PATH}

# Restart the scylla service
echo "⭐️ Restarting scylla..."
supervisorctl start scylla
sleep 5

# recreate the materialized views
echo "⭐️ Recreating the materialized views..."

cqlsh -e "\
CREATE MATERIALIZED VIEW kvs.leaderboard_by_score AS \
SELECT * \
FROM kvs.leaderboard \
WHERE world_id IS NOT null AND leaderboard_name IS NOT null AND user_id IS NOT null AND score IS NOT null \
PRIMARY KEY ((world_id, leaderboard_name), score, user_id) \
WITH CLUSTERING ORDER BY (score DESC);"

cqlsh -e "\
CREATE MATERIALIZED VIEW moderation.chat_message_by_user AS \
SELECT * \
FROM moderation.chat_message \
WHERE senderID IS NOT null AND createdAt IS NOT null \
PRIMARY KEY((senderID), createdAt, id);"

cqlsh -e "\
CREATE MATERIALIZED VIEW moderation.chat_message_by_server AS \
SELECT * \
FROM moderation.chat_message \
WHERE serverID IS NOT null AND createdAt IS NOT null \
PRIMARY KEY((serverID), createdAt, id);"

# recreate the secondary indexes
echo "⭐️ Recreating the secondary indexes..."

cqlsh -e "CREATE INDEX user_notification_read_idx ON universe.user_notification((user_id), read);"
cqlsh -e "CREATE INDEX user_notification_category_idx ON universe.user_notification((user_id), category);"
cqlsh -e "CREATE INDEX universe_parent_dashboard_by_child ON universe.parent_dashboard(child_id);"
cqlsh -e "CREATE INDEX universe_parent_dashboard_by_parent ON universe.parent_dashboard(parent_id);"

echo "✅ DB restored successfully"
