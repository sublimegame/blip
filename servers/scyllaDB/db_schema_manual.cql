-- ----------------------------------------------
-- LIST OF ALL ITEMS
-- ----------------------------------------------

-- TABLE kvs.leaderboard
-- MATERIALIZED VIEW kvs.leaderboard_by_score
-- TABLE kvs.unordered

-- TABLE moderation.username
-- TABLE moderation.chat_message
-- MATERIALIZED VIEW moderation.chat_message_by_user
-- MATERIALIZED VIEW moderation.chat_message_by_server

-- TABLE universe.user_notification
-- INDEX user_notification_read_idx ON universe.user_notification((user_id), read)
-- INDEX user_notification_category_idx ON universe.user_notification((user_id), category)
-- TABLE universe.parent_dashboard
-- INDEX universe_parent_dashboard_by_child ON universe.parent_dashboard(child_id)
-- INDEX universe_parent_dashboard_by_parent ON universe.parent_dashboard(parent_id)
-- TABLE universe.module
-- TABLE universe.module_ref
-- TABLE universe.module_contributor
-- TABLE universe.relation
-- TABLE universe.user_balance
-- TABLE universe.user_balance_lock
-- TABLE universe.transaction_group
-- TABLE universe.grant_coin_transaction_by_user
-- TABLE universe.dummy_coin_transaction_by_user
-- TABLE universe.earn_coin_transaction_by_user
-- TABLE universe.purchase_coin_transaction_by_user
-- TABLE universe.treasury_transaction_grant_coin
-- TABLE universe.treasury_transaction_dummy_coin
-- TABLE universe.treasury_transaction_earn_coin
-- TABLE universe.treasury_transaction_purchase_coin

-- ----------------------------------------------
-- REMOVE ALL KEYSPACES
-- ----------------------------------------------

-- DROP KEYSPACE IF EXISTS kvs;
-- DROP KEYSPACE IF EXISTS moderation;
-- DROP KEYSPACE IF EXISTS universe;

-- ----------------------------------------------
-- [kvs] keyspace
-- ----------------------------------------------

-- replication strategy is for a single node cluster
CREATE KEYSPACE kvs WITH replication = {'class' : 'SimpleStrategy', 'replication_factor' : 1};

-- [kvs.leaderboard] table

CREATE TABLE kvs.leaderboard (
    world_id uuid,
    leaderboard_name text,
    user_id uuid,
    score bigint,
    value blob,
    updated timestamp,
    PRIMARY KEY ((world_id, leaderboard_name), user_id)
);

CREATE MATERIALIZED VIEW kvs.leaderboard_by_score AS
    SELECT *
    FROM kvs.leaderboard
    WHERE world_id IS NOT null AND leaderboard_name IS NOT null AND user_id IS NOT null AND score IS NOT null
    PRIMARY KEY ((world_id, leaderboard_name), score, user_id)
    WITH CLUSTERING ORDER BY (score DESC);

-- [kvs.unordered] table

-- Create a table for the KVS data (unordered values)
CREATE TABLE kvs.unordered (
    world_id uuid,
    store text,
    key text,
    value blob,
    PRIMARY KEY ((world_id, store), key)
);

-- ----------------------------------------------
-- [moderation] keyspace
-- ----------------------------------------------

-- replication strategy is for a single node cluster
CREATE KEYSPACE moderation WITH replication = {'class' : 'SimpleStrategy', 'replication_factor' : 1};

-- [moderation.username] table

CREATE TABLE moderation.username (
    username text,
    appropriate boolean,
    created timestamp,
    PRIMARY KEY (username)
);

-- [moderation.chat_message] table

CREATE TABLE moderation.chat_message (
    id uuid,
    senderID uuid,
    createdAt timestamp,
    updatedAt timestamp,
    serverID text,
    content text,
    status tinyint,
    reason tinyint,
    PRIMARY KEY ((id), createdAt)
);

CREATE MATERIALIZED VIEW moderation.chat_message_by_user AS
    SELECT * 
    FROM moderation.chat_message
    WHERE senderID IS NOT null AND createdAt IS NOT null
    PRIMARY KEY((senderID), createdAt, id);

CREATE MATERIALIZED VIEW moderation.chat_message_by_server AS
    SELECT * 
    FROM moderation.chat_message
    WHERE serverID IS NOT null AND createdAt IS NOT null
    PRIMARY KEY((serverID), createdAt, id);

-- ----------------------------------------------
-- [universe] keyspace
-- ----------------------------------------------

-- replication strategy is for a single node cluster
CREATE KEYSPACE universe WITH replication = {'class' : 'SimpleStrategy', 'replication_factor' : 1};

-- [universe.user_notification] table

CREATE TABLE universe.user_notification (
    user_id uuid,
    time_uuid timeuuid,   -- this guarantees uniqueness and preserves time ordering
    category text,
    read boolean,
    content text,
    PRIMARY KEY ((user_id), time_uuid)
) WITH CLUSTERING ORDER BY (time_uuid DESC);

CREATE INDEX ON universe.user_notification((user_id), read);
CREATE INDEX ON universe.user_notification((user_id), category);

-- [universe.parent_dashboard] table

CREATE TABLE universe.parent_dashboard (
    dashboard_id text,
    child_id uuid,
    parent_id uuid,
    secret_key_hash text,
    created_at timestamp,
    updated_at timestamp,
    PRIMARY KEY ((dashboard_id))
);

CREATE INDEX universe_parent_dashboard_by_child ON universe.parent_dashboard(child_id);
CREATE INDEX universe_parent_dashboard_by_parent ON universe.parent_dashboard(parent_id);

CREATE TABLE universe.module (
    origin text,
    commit text,
    script text,
    documentation text,
    commit_created_at timestamp,
    created_at timestamp,
    updated_at timestamp,
    PRIMARY KEY ((origin), commit)
);

CREATE TABLE universe.module_ref (
    origin text,
    ref text,
    commit text,
    ref_is_commit boolean,
    hash text,
    created_at timestamp,
    updated_at timestamp,
    PRIMARY KEY ((origin), ref)
);

CREATE TABLE universe.module_contributor (
    origin text,
    commit text,
    user_id uuid,
    user_name text,
    share float,
    PRIMARY KEY ((origin), commit, user_id)
);

CREATE TABLE universe.relation (
    id1 uuid,
    name1 text,
    id2 uuid,
    name2 text,
    type text,
    created_at timestamp,
   PRIMARY KEY ((id1), type, id2)
);

CREATE TABLE universe.user_balance (
    user_id uuid,
    total_dummy_coins counter,
    total_granted_coins counter,
    total_earned_coins counter,
    total_purchased_coins counter,
    total_coins counter,
    PRIMARY KEY ((user_id))
);

CREATE TABLE universe.user_balance_lock (
    user_id uuid,
    created_at timestamp,
    PRIMARY KEY ((user_id))
);

CREATE TABLE universe.transaction_group (
    transaction_group_id uuid,
    created_at timestamp,
    info text,
    PRIMARY KEY ((transaction_group_id))
);

CREATE TABLE universe.grant_coin_transaction_by_user (
    user_id uuid,
    created_at timestamp,
    transaction_id uuid,
    transaction_group_id uuid,
    amount bigint,
    info text,
    PRIMARY KEY ((user_id), created_at, transaction_id)
) WITH CLUSTERING ORDER BY (created_at DESC);

CREATE TABLE universe.dummy_coin_transaction_by_user (
    user_id uuid,
    created_at timestamp,
    transaction_id uuid,
    transaction_group_id uuid,
    amount bigint,
    info text,
    PRIMARY KEY ((user_id), created_at, transaction_id)
) WITH CLUSTERING ORDER BY (created_at DESC);

CREATE TABLE universe.earn_coin_transaction_by_user (
    user_id uuid,
    created_at timestamp,
    transaction_id uuid,
    transaction_group_id uuid,
    amount bigint,
    info text,
    PRIMARY KEY ((user_id), created_at, transaction_id)
) WITH CLUSTERING ORDER BY (created_at DESC);

CREATE TABLE universe.purchase_coin_transaction_by_user (
    user_id uuid,
    created_at timestamp,
    transaction_id uuid,
    transaction_group_id uuid,
    amount bigint,
    info text,
    PRIMARY KEY ((user_id), created_at, transaction_id)
) WITH CLUSTERING ORDER BY (created_at DESC);

CREATE TABLE universe.treasury_transaction_grant_coin (
    transaction_group_id uuid,
    transaction_id uuid,
    amount bigint,
    PRIMARY KEY ((transaction_group_id), transaction_id)
);

CREATE TABLE universe.treasury_transaction_dummy_coin (
    transaction_group_id uuid,
    transaction_id uuid,
    amount bigint,
    PRIMARY KEY ((transaction_group_id), transaction_id)
);

CREATE TABLE universe.treasury_transaction_earn_coin (
    transaction_group_id uuid,
    transaction_id uuid,
    amount bigint,
    PRIMARY KEY ((transaction_group_id), transaction_id)
);

CREATE TABLE universe.treasury_transaction_purchase_coin (
    transaction_group_id uuid,
    transaction_id uuid,
    amount bigint,
    PRIMARY KEY ((transaction_group_id), transaction_id)
);
