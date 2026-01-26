
CREATE KEYSPACE system_replicated_keys WITH replication = {'class': 'org.apache.cassandra.locator.EverywhereStrategy'} AND durable_writes = true;

CREATE TABLE system_replicated_keys.encrypted_keys (
    key_file text,
    cipher text,
    strength int,
    key_id timeuuid,
    key text,
    PRIMARY KEY ((key_file, cipher), strength, key_id)
) WITH ID = f705e6dd-8cc5-378b-a7e9-aec38e033747
AND CLUSTERING ORDER BY (strength ASC, key_id ASC)
    AND bloom_filter_fp_chance = 0.01
    AND caching = {'keys': 'ALL','rows_per_partition': 'ALL'}
    AND comment = ''
    AND compaction = {'class': 'IncrementalCompactionStrategy'}
    AND compression = {'sstable_compression': 'org.apache.cassandra.io.compress.LZ4Compressor'}
    AND crc_check_chance = 1
    AND dclocal_read_repair_chance = 0
    AND default_time_to_live = 0
    AND gc_grace_seconds = 864000
    AND max_index_interval = 2048
    AND memtable_flush_period_in_ms = 0
    AND min_index_interval = 128
    AND read_repair_chance = 0
    AND speculative_retry = '99.0PERCENTILE';


CREATE KEYSPACE kvs WITH replication = {'class': 'org.apache.cassandra.locator.NetworkTopologyStrategy', 'AWS_US_EAST_2': '3'} AND durable_writes = true;

CREATE TABLE kvs.leaderboard (
    world_id uuid,
    leaderboard_name text,
    user_id uuid,
    score bigint,
    updated timestamp,
    value blob,
    PRIMARY KEY ((world_id, leaderboard_name), user_id)
) WITH ID = 64a57e90-87e6-11ef-9d6d-589f2d51c150
AND CLUSTERING ORDER BY (user_id ASC)
    AND bloom_filter_fp_chance = 0.01
    AND caching = {'keys': 'ALL','rows_per_partition': 'ALL'}
    AND comment = ''
    AND compaction = {'class': 'IncrementalCompactionStrategy'}
    AND compression = {'sstable_compression': 'org.apache.cassandra.io.compress.LZ4Compressor'}
    AND crc_check_chance = 1
    AND dclocal_read_repair_chance = 0
    AND default_time_to_live = 0
    AND gc_grace_seconds = 864000
    AND max_index_interval = 2048
    AND memtable_flush_period_in_ms = 0
    AND min_index_interval = 128
    AND read_repair_chance = 0
    AND speculative_retry = '99.0PERCENTILE';


CREATE MATERIALIZED VIEW kvs.leaderboard_by_score AS
    SELECT world_id, leaderboard_name, score, user_id, updated, value
    FROM kvs.leaderboard
    WHERE world_id IS NOT null AND leaderboard_name IS NOT null AND user_id IS NOT null AND score IS NOT null
    PRIMARY KEY ((world_id, leaderboard_name), score, user_id)
    WITH ID = 6e6008b0-87e6-11ef-9d6d-589f2d51c150
AND CLUSTERING ORDER BY (score DESC, user_id ASC)
    AND bloom_filter_fp_chance = 0.01
    AND caching = {'keys': 'ALL','rows_per_partition': 'ALL'}
    AND comment = ''
    AND compaction = {'class': 'IncrementalCompactionStrategy'}
    AND compression = {'sstable_compression': 'org.apache.cassandra.io.compress.LZ4Compressor'}
    AND crc_check_chance = 1
    AND dclocal_read_repair_chance = 0
    AND default_time_to_live = 0
    AND gc_grace_seconds = 864000
    AND max_index_interval = 2048
    AND memtable_flush_period_in_ms = 0
    AND min_index_interval = 128
    AND read_repair_chance = 0
    AND speculative_retry = '99.0PERCENTILE';


CREATE TABLE kvs.unordered (
    world_id uuid,
    store text,
    key text,
    value blob,
    PRIMARY KEY ((world_id, store), key)
) WITH ID = 6de59580-cfef-11ee-a4cc-2660c38c476a
AND CLUSTERING ORDER BY (key ASC)
    AND bloom_filter_fp_chance = 0.01
    AND caching = {'keys': 'ALL','rows_per_partition': 'ALL'}
    AND comment = ''
    AND compaction = {'class': 'IncrementalCompactionStrategy'}
    AND compression = {'sstable_compression': 'org.apache.cassandra.io.compress.LZ4Compressor'}
    AND crc_check_chance = 1
    AND dclocal_read_repair_chance = 0
    AND default_time_to_live = 0
    AND gc_grace_seconds = 864000
    AND max_index_interval = 2048
    AND memtable_flush_period_in_ms = 0
    AND min_index_interval = 128
    AND read_repair_chance = 0
    AND speculative_retry = '99.0PERCENTILE';


CREATE KEYSPACE universe WITH replication = {'class': 'org.apache.cassandra.locator.NetworkTopologyStrategy', 'AWS_US_EAST_2': '3'} AND durable_writes = true;

CREATE TABLE universe.dummy_coin_transaction_by_user (
    user_id uuid,
    created_at timestamp,
    transaction_id uuid,
    amount bigint,
    info text,
    transaction_group_id uuid,
    PRIMARY KEY (user_id, created_at, transaction_id)
) WITH ID = a119ce50-74cf-11ef-9d6d-589f2d51c150
AND CLUSTERING ORDER BY (created_at DESC, transaction_id ASC)
    AND bloom_filter_fp_chance = 0.01
    AND caching = {'keys': 'ALL','rows_per_partition': 'ALL'}
    AND comment = ''
    AND compaction = {'class': 'IncrementalCompactionStrategy'}
    AND compression = {'sstable_compression': 'org.apache.cassandra.io.compress.LZ4Compressor'}
    AND crc_check_chance = 1
    AND dclocal_read_repair_chance = 0
    AND default_time_to_live = 0
    AND gc_grace_seconds = 864000
    AND max_index_interval = 2048
    AND memtable_flush_period_in_ms = 0
    AND min_index_interval = 128
    AND read_repair_chance = 0
    AND speculative_retry = '99.0PERCENTILE';


CREATE TABLE universe.earn_coin_transaction_by_user (
    user_id uuid,
    created_at timestamp,
    transaction_id uuid,
    amount bigint,
    info text,
    transaction_group_id uuid,
    PRIMARY KEY (user_id, created_at, transaction_id)
) WITH ID = a1a4f750-74cf-11ef-9d6d-589f2d51c150
AND CLUSTERING ORDER BY (created_at DESC, transaction_id ASC)
    AND bloom_filter_fp_chance = 0.01
    AND caching = {'keys': 'ALL','rows_per_partition': 'ALL'}
    AND comment = ''
    AND compaction = {'class': 'IncrementalCompactionStrategy'}
    AND compression = {'sstable_compression': 'org.apache.cassandra.io.compress.LZ4Compressor'}
    AND crc_check_chance = 1
    AND dclocal_read_repair_chance = 0
    AND default_time_to_live = 0
    AND gc_grace_seconds = 864000
    AND max_index_interval = 2048
    AND memtable_flush_period_in_ms = 0
    AND min_index_interval = 128
    AND read_repair_chance = 0
    AND speculative_retry = '99.0PERCENTILE';


CREATE TABLE universe.grant_coin_transaction_by_user (
    user_id uuid,
    created_at timestamp,
    transaction_id uuid,
    amount bigint,
    info text,
    transaction_group_id uuid,
    PRIMARY KEY (user_id, created_at, transaction_id)
) WITH ID = a0ac6680-74cf-11ef-9d6d-589f2d51c150
AND CLUSTERING ORDER BY (created_at DESC, transaction_id ASC)
    AND bloom_filter_fp_chance = 0.01
    AND caching = {'keys': 'ALL','rows_per_partition': 'ALL'}
    AND comment = ''
    AND compaction = {'class': 'IncrementalCompactionStrategy'}
    AND compression = {'sstable_compression': 'org.apache.cassandra.io.compress.LZ4Compressor'}
    AND crc_check_chance = 1
    AND dclocal_read_repair_chance = 0
    AND default_time_to_live = 0
    AND gc_grace_seconds = 864000
    AND max_index_interval = 2048
    AND memtable_flush_period_in_ms = 0
    AND min_index_interval = 128
    AND read_repair_chance = 0
    AND speculative_retry = '99.0PERCENTILE';


CREATE TABLE universe.module (
    origin text,
    commit text,
    commit_created_at timestamp,
    created_at timestamp,
    documentation text,
    script text,
    updated_at timestamp,
    PRIMARY KEY (origin, commit)
) WITH ID = 00fa97f0-dd27-11ee-a4cc-2660c38c476a
AND CLUSTERING ORDER BY (commit ASC)
    AND bloom_filter_fp_chance = 0.01
    AND caching = {'keys': 'ALL','rows_per_partition': 'ALL'}
    AND comment = ''
    AND compaction = {'class': 'IncrementalCompactionStrategy'}
    AND compression = {'sstable_compression': 'org.apache.cassandra.io.compress.LZ4Compressor'}
    AND crc_check_chance = 1
    AND dclocal_read_repair_chance = 0
    AND default_time_to_live = 0
    AND gc_grace_seconds = 864000
    AND max_index_interval = 2048
    AND memtable_flush_period_in_ms = 0
    AND min_index_interval = 128
    AND read_repair_chance = 0
    AND speculative_retry = '99.0PERCENTILE';


CREATE TABLE universe.module_contributor (
    origin text,
    commit text,
    user_id uuid,
    share float,
    user_name text,
    PRIMARY KEY (origin, commit, user_id)
) WITH ID = 03317520-dd27-11ee-a4cc-2660c38c476a
AND CLUSTERING ORDER BY (commit ASC, user_id ASC)
    AND bloom_filter_fp_chance = 0.01
    AND caching = {'keys': 'ALL','rows_per_partition': 'ALL'}
    AND comment = ''
    AND compaction = {'class': 'IncrementalCompactionStrategy'}
    AND compression = {'sstable_compression': 'org.apache.cassandra.io.compress.LZ4Compressor'}
    AND crc_check_chance = 1
    AND dclocal_read_repair_chance = 0
    AND default_time_to_live = 0
    AND gc_grace_seconds = 864000
    AND max_index_interval = 2048
    AND memtable_flush_period_in_ms = 0
    AND min_index_interval = 128
    AND read_repair_chance = 0
    AND speculative_retry = '99.0PERCENTILE';


CREATE TABLE universe.module_ref (
    origin text,
    ref text,
    commit text,
    created_at timestamp,
    hash text,
    ref_is_commit boolean,
    updated_at timestamp,
    PRIMARY KEY (origin, ref)
) WITH ID = 01780550-dd27-11ee-a4cc-2660c38c476a
AND CLUSTERING ORDER BY (ref ASC)
    AND bloom_filter_fp_chance = 0.01
    AND caching = {'keys': 'ALL','rows_per_partition': 'ALL'}
    AND comment = ''
    AND compaction = {'class': 'IncrementalCompactionStrategy'}
    AND compression = {'sstable_compression': 'org.apache.cassandra.io.compress.LZ4Compressor'}
    AND crc_check_chance = 1
    AND dclocal_read_repair_chance = 0
    AND default_time_to_live = 0
    AND gc_grace_seconds = 864000
    AND max_index_interval = 2048
    AND memtable_flush_period_in_ms = 0
    AND min_index_interval = 128
    AND read_repair_chance = 0
    AND speculative_retry = '99.0PERCENTILE';


CREATE TABLE universe.parent_dashboard (
    dashboard_id text,
    child_id uuid,
    created_at timestamp,
    parent_id uuid,
    secret_key_hash text,
    updated_at timestamp,
    PRIMARY KEY (dashboard_id)
) WITH ID = 7beb61e0-5aec-11ef-9d6d-589f2d51c150
AND bloom_filter_fp_chance = 0.01
    AND caching = {'keys': 'ALL','rows_per_partition': 'ALL'}
    AND comment = ''
    AND compaction = {'class': 'IncrementalCompactionStrategy'}
    AND compression = {'sstable_compression': 'org.apache.cassandra.io.compress.LZ4Compressor'}
    AND crc_check_chance = 1
    AND dclocal_read_repair_chance = 0
    AND default_time_to_live = 0
    AND gc_grace_seconds = 864000
    AND max_index_interval = 2048
    AND memtable_flush_period_in_ms = 0
    AND min_index_interval = 128
    AND read_repair_chance = 0
    AND speculative_retry = '99.0PERCENTILE';


CREATE INDEX universe_parent_dashboard_by_child ON universe.parent_dashboard(child_id);


CREATE INDEX universe_parent_dashboard_by_parent ON universe.parent_dashboard(parent_id);


CREATE TABLE universe.purchase_coin_transaction_by_user (
    user_id uuid,
    created_at timestamp,
    transaction_id uuid,
    amount bigint,
    info text,
    transaction_group_id uuid,
    PRIMARY KEY (user_id, created_at, transaction_id)
) WITH ID = a20e8e90-74cf-11ef-9d6d-589f2d51c150
AND CLUSTERING ORDER BY (created_at DESC, transaction_id ASC)
    AND bloom_filter_fp_chance = 0.01
    AND caching = {'keys': 'ALL','rows_per_partition': 'ALL'}
    AND comment = ''
    AND compaction = {'class': 'IncrementalCompactionStrategy'}
    AND compression = {'sstable_compression': 'org.apache.cassandra.io.compress.LZ4Compressor'}
    AND crc_check_chance = 1
    AND dclocal_read_repair_chance = 0
    AND default_time_to_live = 0
    AND gc_grace_seconds = 864000
    AND max_index_interval = 2048
    AND memtable_flush_period_in_ms = 0
    AND min_index_interval = 128
    AND read_repair_chance = 0
    AND speculative_retry = '99.0PERCENTILE';


CREATE TABLE universe.relation (
    id1 uuid,
    type text,
    id2 uuid,
    created_at timestamp,
    name1 text,
    name2 text,
    PRIMARY KEY (id1, type, id2)
) WITH ID = bf500000-d499-11ee-a4cc-2660c38c476a
AND CLUSTERING ORDER BY (type ASC, id2 ASC)
    AND bloom_filter_fp_chance = 0.01
    AND caching = {'keys': 'ALL','rows_per_partition': 'ALL'}
    AND comment = ''
    AND compaction = {'class': 'IncrementalCompactionStrategy'}
    AND compression = {'sstable_compression': 'org.apache.cassandra.io.compress.LZ4Compressor'}
    AND crc_check_chance = 1
    AND dclocal_read_repair_chance = 0
    AND default_time_to_live = 0
    AND gc_grace_seconds = 864000
    AND max_index_interval = 2048
    AND memtable_flush_period_in_ms = 0
    AND min_index_interval = 128
    AND read_repair_chance = 0
    AND speculative_retry = '99.0PERCENTILE';


CREATE TABLE universe.transaction_group (
    transaction_group_id uuid,
    created_at timestamp,
    info text,
    PRIMARY KEY (transaction_group_id)
) WITH ID = 9363d3a0-74cf-11ef-9d6d-589f2d51c150
AND bloom_filter_fp_chance = 0.01
    AND caching = {'keys': 'ALL','rows_per_partition': 'ALL'}
    AND comment = ''
    AND compaction = {'class': 'IncrementalCompactionStrategy'}
    AND compression = {'sstable_compression': 'org.apache.cassandra.io.compress.LZ4Compressor'}
    AND crc_check_chance = 1
    AND dclocal_read_repair_chance = 0
    AND default_time_to_live = 0
    AND gc_grace_seconds = 864000
    AND max_index_interval = 2048
    AND memtable_flush_period_in_ms = 0
    AND min_index_interval = 128
    AND read_repair_chance = 0
    AND speculative_retry = '99.0PERCENTILE';


CREATE TABLE universe.treasury_transaction_dummy_coin (
    transaction_group_id uuid,
    transaction_id uuid,
    amount bigint,
    PRIMARY KEY (transaction_group_id, transaction_id)
) WITH ID = a320c1e0-74cf-11ef-9d6d-589f2d51c150
AND CLUSTERING ORDER BY (transaction_id ASC)
    AND bloom_filter_fp_chance = 0.01
    AND caching = {'keys': 'ALL','rows_per_partition': 'ALL'}
    AND comment = ''
    AND compaction = {'class': 'IncrementalCompactionStrategy'}
    AND compression = {'sstable_compression': 'org.apache.cassandra.io.compress.LZ4Compressor'}
    AND crc_check_chance = 1
    AND dclocal_read_repair_chance = 0
    AND default_time_to_live = 0
    AND gc_grace_seconds = 864000
    AND max_index_interval = 2048
    AND memtable_flush_period_in_ms = 0
    AND min_index_interval = 128
    AND read_repair_chance = 0
    AND speculative_retry = '99.0PERCENTILE';


CREATE TABLE universe.treasury_transaction_earn_coin (
    transaction_group_id uuid,
    transaction_id uuid,
    amount bigint,
    PRIMARY KEY (transaction_group_id, transaction_id)
) WITH ID = a36d0d20-74cf-11ef-9d6d-589f2d51c150
AND CLUSTERING ORDER BY (transaction_id ASC)
    AND bloom_filter_fp_chance = 0.01
    AND caching = {'keys': 'ALL','rows_per_partition': 'ALL'}
    AND comment = ''
    AND compaction = {'class': 'IncrementalCompactionStrategy'}
    AND compression = {'sstable_compression': 'org.apache.cassandra.io.compress.LZ4Compressor'}
    AND crc_check_chance = 1
    AND dclocal_read_repair_chance = 0
    AND default_time_to_live = 0
    AND gc_grace_seconds = 864000
    AND max_index_interval = 2048
    AND memtable_flush_period_in_ms = 0
    AND min_index_interval = 128
    AND read_repair_chance = 0
    AND speculative_retry = '99.0PERCENTILE';


CREATE TABLE universe.treasury_transaction_grant_coin (
    transaction_group_id uuid,
    transaction_id uuid,
    amount bigint,
    PRIMARY KEY (transaction_group_id, transaction_id)
) WITH ID = a298f440-74cf-11ef-9d6d-589f2d51c150
AND CLUSTERING ORDER BY (transaction_id ASC)
    AND bloom_filter_fp_chance = 0.01
    AND caching = {'keys': 'ALL','rows_per_partition': 'ALL'}
    AND comment = ''
    AND compaction = {'class': 'IncrementalCompactionStrategy'}
    AND compression = {'sstable_compression': 'org.apache.cassandra.io.compress.LZ4Compressor'}
    AND crc_check_chance = 1
    AND dclocal_read_repair_chance = 0
    AND default_time_to_live = 0
    AND gc_grace_seconds = 864000
    AND max_index_interval = 2048
    AND memtable_flush_period_in_ms = 0
    AND min_index_interval = 128
    AND read_repair_chance = 0
    AND speculative_retry = '99.0PERCENTILE';


CREATE TABLE universe.treasury_transaction_purchase_coin (
    transaction_group_id uuid,
    transaction_id uuid,
    amount bigint,
    PRIMARY KEY (transaction_group_id, transaction_id)
) WITH ID = a4539c90-74cf-11ef-9d6d-589f2d51c150
AND CLUSTERING ORDER BY (transaction_id ASC)
    AND bloom_filter_fp_chance = 0.01
    AND caching = {'keys': 'ALL','rows_per_partition': 'ALL'}
    AND comment = ''
    AND compaction = {'class': 'IncrementalCompactionStrategy'}
    AND compression = {'sstable_compression': 'org.apache.cassandra.io.compress.LZ4Compressor'}
    AND crc_check_chance = 1
    AND dclocal_read_repair_chance = 0
    AND default_time_to_live = 0
    AND gc_grace_seconds = 864000
    AND max_index_interval = 2048
    AND memtable_flush_period_in_ms = 0
    AND min_index_interval = 128
    AND read_repair_chance = 0
    AND speculative_retry = '99.0PERCENTILE';


CREATE TABLE universe.user_balance (
    user_id uuid,
    total_coins counter,
    total_dummy_coins counter,
    total_earned_coins counter,
    total_granted_coins counter,
    total_purchased_coins counter,
    PRIMARY KEY (user_id)
) WITH ID = 8d639260-74cf-11ef-9d6d-589f2d51c150
AND bloom_filter_fp_chance = 0.01
    AND caching = {'keys': 'ALL','rows_per_partition': 'ALL'}
    AND comment = ''
    AND compaction = {'class': 'IncrementalCompactionStrategy'}
    AND compression = {'sstable_compression': 'org.apache.cassandra.io.compress.LZ4Compressor'}
    AND crc_check_chance = 1
    AND dclocal_read_repair_chance = 0
    AND default_time_to_live = 0
    AND gc_grace_seconds = 864000
    AND max_index_interval = 2048
    AND memtable_flush_period_in_ms = 0
    AND min_index_interval = 128
    AND read_repair_chance = 0
    AND speculative_retry = '99.0PERCENTILE';


CREATE TABLE universe.user_balance_lock (
    user_id uuid,
    created_at timestamp,
    PRIMARY KEY (user_id)
) WITH ID = 92a6c530-74cf-11ef-9d6d-589f2d51c150
AND bloom_filter_fp_chance = 0.01
    AND caching = {'keys': 'ALL','rows_per_partition': 'ALL'}
    AND comment = ''
    AND compaction = {'class': 'IncrementalCompactionStrategy'}
    AND compression = {'sstable_compression': 'org.apache.cassandra.io.compress.LZ4Compressor'}
    AND crc_check_chance = 1
    AND dclocal_read_repair_chance = 0
    AND default_time_to_live = 0
    AND gc_grace_seconds = 864000
    AND max_index_interval = 2048
    AND memtable_flush_period_in_ms = 0
    AND min_index_interval = 128
    AND read_repair_chance = 0
    AND speculative_retry = '99.0PERCENTILE';


CREATE TABLE universe.user_notification (
    user_id uuid,
    time_uuid timeuuid,
    category text,
    content text,
    read boolean,
    PRIMARY KEY (user_id, time_uuid)
) WITH ID = c8731320-8092-11ef-9d6d-589f2d51c150
AND CLUSTERING ORDER BY (time_uuid DESC)
    AND bloom_filter_fp_chance = 0.01
    AND caching = {'keys': 'ALL','rows_per_partition': 'ALL'}
    AND comment = ''
    AND compaction = {'class': 'IncrementalCompactionStrategy'}
    AND compression = {'sstable_compression': 'org.apache.cassandra.io.compress.LZ4Compressor'}
    AND crc_check_chance = 1
    AND dclocal_read_repair_chance = 0
    AND default_time_to_live = 0
    AND gc_grace_seconds = 864000
    AND max_index_interval = 2048
    AND memtable_flush_period_in_ms = 0
    AND min_index_interval = 128
    AND read_repair_chance = 0
    AND speculative_retry = '99.0PERCENTILE';


CREATE INDEX user_notification_category_idx ON universe.user_notification((user_id), category);


CREATE INDEX user_notification_read_idx ON universe.user_notification((user_id), read);


CREATE KEYSPACE moderation WITH replication = {'class': 'org.apache.cassandra.locator.NetworkTopologyStrategy', 'AWS_US_EAST_2': '3'} AND durable_writes = true;

CREATE TABLE moderation.chat_message (
    id uuid,
    createdat timestamp,
    content text,
    reason tinyint,
    senderid uuid,
    serverid text,
    status tinyint,
    updatedat timestamp,
    PRIMARY KEY (id, createdat)
) WITH ID = c0721d30-f1b6-11ee-afcd-35c007c249d9
AND CLUSTERING ORDER BY (createdat ASC)
    AND bloom_filter_fp_chance = 0.01
    AND caching = {'keys': 'ALL','rows_per_partition': 'ALL'}
    AND comment = ''
    AND compaction = {'class': 'IncrementalCompactionStrategy'}
    AND compression = {'sstable_compression': 'org.apache.cassandra.io.compress.LZ4Compressor'}
    AND crc_check_chance = 1
    AND dclocal_read_repair_chance = 0
    AND default_time_to_live = 0
    AND gc_grace_seconds = 864000
    AND max_index_interval = 2048
    AND memtable_flush_period_in_ms = 0
    AND min_index_interval = 128
    AND read_repair_chance = 0
    AND speculative_retry = '99.0PERCENTILE';


CREATE MATERIALIZED VIEW moderation.chat_message_by_server AS
    SELECT serverid, createdat, id, content, reason, senderid, status, updatedat
    FROM moderation.chat_message
    WHERE serverid IS NOT null AND createdat IS NOT null
    PRIMARY KEY (serverid, createdat, id)
    WITH ID = d13530d0-f1b6-11ee-afcd-35c007c249d9
AND CLUSTERING ORDER BY (createdat ASC, id ASC)
    AND bloom_filter_fp_chance = 0.01
    AND caching = {'keys': 'ALL','rows_per_partition': 'ALL'}
    AND comment = ''
    AND compaction = {'class': 'IncrementalCompactionStrategy'}
    AND compression = {'sstable_compression': 'org.apache.cassandra.io.compress.LZ4Compressor'}
    AND crc_check_chance = 1
    AND dclocal_read_repair_chance = 0
    AND default_time_to_live = 0
    AND gc_grace_seconds = 864000
    AND max_index_interval = 2048
    AND memtable_flush_period_in_ms = 0
    AND min_index_interval = 128
    AND read_repair_chance = 0
    AND speculative_retry = '99.0PERCENTILE';


CREATE MATERIALIZED VIEW moderation.chat_message_by_user AS
    SELECT senderid, createdat, id, content, reason, serverid, status, updatedat
    FROM moderation.chat_message
    WHERE senderid IS NOT null AND createdat IS NOT null
    PRIMARY KEY (senderid, createdat, id)
    WITH ID = cd76a230-f1b6-11ee-afcd-35c007c249d9
AND CLUSTERING ORDER BY (createdat ASC, id ASC)
    AND bloom_filter_fp_chance = 0.01
    AND caching = {'keys': 'ALL','rows_per_partition': 'ALL'}
    AND comment = ''
    AND compaction = {'class': 'IncrementalCompactionStrategy'}
    AND compression = {'sstable_compression': 'org.apache.cassandra.io.compress.LZ4Compressor'}
    AND crc_check_chance = 1
    AND dclocal_read_repair_chance = 0
    AND default_time_to_live = 0
    AND gc_grace_seconds = 864000
    AND max_index_interval = 2048
    AND memtable_flush_period_in_ms = 0
    AND min_index_interval = 128
    AND read_repair_chance = 0
    AND speculative_retry = '99.0PERCENTILE';


CREATE TABLE moderation.username (
    username text,
    appropriate boolean,
    created timestamp,
    PRIMARY KEY (username)
) WITH ID = 10ba2700-c017-11ee-a4cc-2660c38c476a
AND bloom_filter_fp_chance = 0.01
    AND caching = {'keys': 'ALL','rows_per_partition': 'ALL'}
    AND comment = ''
    AND compaction = {'class': 'IncrementalCompactionStrategy'}
    AND compression = {'sstable_compression': 'org.apache.cassandra.io.compress.LZ4Compressor'}
    AND crc_check_chance = 1
    AND dclocal_read_repair_chance = 0
    AND default_time_to_live = 0
    AND gc_grace_seconds = 864000
    AND max_index_interval = 2048
    AND memtable_flush_period_in_ms = 0
    AND min_index_interval = 128
    AND read_repair_chance = 0
    AND speculative_retry = '99.0PERCENTILE';

