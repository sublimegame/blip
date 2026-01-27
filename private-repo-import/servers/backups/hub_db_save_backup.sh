#!/bin/bash

# 
# hub_db_save_backup.sh
#
# Creates a backup of the Hub DB (ScyllaDB)
# 
# This must run inside the ScyllaDB container
#
# How to run it from inside the container:
# /var/lib/scylla/backups/hub_db_restore_backup.sh scylla_backup_2025-01-31_22-01-39.431
#
# How to run it from outside the container:
# docker exec -ti hub-db bash -c "/var/lib/scylla/backups/hub_db_restore_backup.sh scylla_backup_2025-01-31_22-01-39.431"
#

# exit on error
set -e

# variables
export VX_SCYLLA_BACKUP_NAME="scylla_backup_$(date +%Y-%m-%d_%H-%M-%S-%3N)"
export VX_SCYLLA_BACKUP_PATH="/var/lib/scylla/backups/${VX_SCYLLA_BACKUP_NAME}"

# create directory for new backup
mkdir -p ${VX_SCYLLA_BACKUP_PATH}

# backup the schema
echo "⚙️ Backing up database schema..."
cqlsh -e "DESC SCHEMA WITH INTERNALS" > ${VX_SCYLLA_BACKUP_PATH}/db_schema_complete.cql
echo "✅ Done"

# create a snapshot of our keyspaces
echo "⚙️ Creating database snapshot..."
nodetool snapshot --tag ${VX_SCYLLA_BACKUP_NAME} -kc kvs,moderation,universe
echo "✅ Done"

# collect all the snapshot files and copy them to the backup directory
echo "⚙️ Collecting snapshot files..."
find /var/lib/scylla/data -type d -name ${VX_SCYLLA_BACKUP_NAME} -path "*/snapshots/${VX_SCYLLA_BACKUP_NAME}" -exec sh -c '\
dir=$(echo "$1" | sed "s|/var/lib/scylla/||"); \
dir=${dir%/*}; \
mkdir -p "${VX_SCYLLA_BACKUP_PATH}/${dir}"; \
cp -r "$1" "${VX_SCYLLA_BACKUP_PATH}/${dir}"' _ {} \;
echo "✅ Done"

# Remove snapshots of Materialized Views (they will be recomputed)
echo "⚙️ Removing snapshots of Materialized Views..."
rm -r ${VX_SCYLLA_BACKUP_PATH}/data/kvs/leaderboard_by_score-*
rm -r ${VX_SCYLLA_BACKUP_PATH}/data/moderation/chat_message_by_user-*
rm -r ${VX_SCYLLA_BACKUP_PATH}/data/moderation/chat_message_by_server-*
echo "✅ Done"

# Remove snapshots of Secondary Indexes (they will be recomputed)
echo "⚙️ Removing snapshots of Secondary Indexes..."
rm -r ${VX_SCYLLA_BACKUP_PATH}/data/universe/user_notification_read_idx_index-*
rm -r ${VX_SCYLLA_BACKUP_PATH}/data/universe/user_notification_category_idx_index-*
rm -r ${VX_SCYLLA_BACKUP_PATH}/data/universe/universe_parent_dashboard_by_child_index-*
rm -r ${VX_SCYLLA_BACKUP_PATH}/data/universe/universe_parent_dashboard_by_parent_index-*
echo "✅ Done"

# archive the backup
echo "⚙️ Archiving the backup..."
tar -C ${VX_SCYLLA_BACKUP_PATH}/.. -zcf ${VX_SCYLLA_BACKUP_PATH}.tar.gz ${VX_SCYLLA_BACKUP_NAME}
rm -r ${VX_SCYLLA_BACKUP_PATH}
echo "✅ Done"

echo "✅ Backup done : ${VX_SCYLLA_BACKUP_NAME}"
echo "🔗 Backup path : ${VX_SCYLLA_BACKUP_PATH}.tar.gz"
