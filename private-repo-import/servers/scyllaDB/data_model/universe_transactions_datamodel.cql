-- 
-- QUERIES WE NEED TO PERFORM
-- 

-- 1. Get amount of coins a user has (total, purchased, rewarded, granted)
--    - for displaying it in the UI
--    - to check if a user can spend coins (purchase item, ...)
--
-- 2. Check if a specific COIN GRANT has already been given to a user
--    (to avoid giving the same grant multiple times)
--
-- 3. Give coins to a user (as a COIN GRANT)
--    - for rewarding a user
--    - ??? should we keep track of the reason for the grant ???
--    - ??? should we store the total amount of coins granted on the platform ???
--
-- [LATER] 3. User purchases an item
-- [LATER] 4. User earns coins from item sale
-- [LATER] 5. User gets coins by in-app purchase

-- ?????????
-- ?. Get total amount of coins a user has, but only the ones he can spend

-- 
-- 2 types of coins ? (premium and regular)
-- separate coins from premium coins ?
-- separate coins earned on the platform and coins purchased
-- need to keep track of all transactions
--
DROP KEYSPACE IF EXISTS universe;
CREATE KEYSPACE universe WITH replication = {'class' : 'NetworkTopologyStrategy', 'replication_factor' : 1};

-- Note: We store all data for a given user in a single PARTITION.
--       This allows to perform BATCH operations on the user data.

-- CREATE TABLE universe.user (
--     user_id UUID,
--     username TEXT,
--     isPayingUser BOOLEAN, -- becomes true when the user makes an in-app purchase
--     created_at TIMESTAMP,
--     updated_at TIMESTAMP,
--     PRIMARY KEY ((user_id))
-- );

-- Constraint: we cannot mix COUNTER and NON-COUNTER columns in the same table
-- Constraint: we cannot update COUNTER columns inside a BATCH
-- coin counters are in millionths of coins
CREATE TABLE universe.user_balance (
   user_id UUID,
   total_dummy_coins COUNTER,     -- coins received as grant by users that are not confirmed yet
   total_granted_coins COUNTER,
   total_earned_coins COUNTER,
   total_purchased_coins COUNTER,
   total_coins COUNTER,
   PRIMARY KEY ((user_id))
);

-- SELECT * FROM universe.user_balance WHERE user_id = 00000000-0000-0000-0000-000000000001;

CREATE TABLE universe.user_balance_lock (
   user_id UUID,
   created_at TIMESTAMP,
   PRIMARY KEY ((user_id))
);

CREATE TABLE universe.transaction_group (
   transaction_group_id UUID,
   created_at TIMESTAMP,
   info TEXT,
   PRIMARY KEY ((transaction_group_id))
);

-- CREATE INDEX ON universe.transaction_group (created_at);

CREATE TABLE universe.grant_coin_transaction_by_user (
   user_id UUID,
   created_at TIMESTAMP,
   transaction_id UUID,
   transaction_group_id UUID,
   amount BIGINT,
   info TEXT,
   PRIMARY KEY ((user_id), created_at, transaction_id)
) WITH CLUSTERING ORDER BY (created_at DESC);

-- TODO: !!! index by grant reason !!!
-- CREATE INDEX ON universe.grant_coin_transaction_by_user (user_id, info);

CREATE TABLE universe.dummy_coin_transaction_by_user (
   user_id UUID,
   created_at TIMESTAMP,
   transaction_id UUID,
   transaction_group_id UUID,
   amount BIGINT,
   info TEXT,
   PRIMARY KEY ((user_id), created_at, transaction_id)
) WITH CLUSTERING ORDER BY (created_at DESC);

CREATE TABLE universe.earn_coin_transaction_by_user (
   user_id UUID,
   created_at TIMESTAMP,
   transaction_id UUID,
   transaction_group_id UUID,
   amount BIGINT,
   info TEXT,
   PRIMARY KEY ((user_id), created_at, transaction_id)
) WITH CLUSTERING ORDER BY (created_at DESC);

CREATE TABLE universe.purchase_coin_transaction_by_user (
   user_id UUID,
   created_at TIMESTAMP,
   transaction_id UUID,
   transaction_group_id UUID,
   amount BIGINT,
   info TEXT,
   PRIMARY KEY ((user_id), created_at, transaction_id)
) WITH CLUSTERING ORDER BY (created_at DESC);

-- Treasury transactions

CREATE TABLE universe.treasury_transaction_grant_coin (
   transaction_group_id UUID,
   transaction_id UUID,
   amount BIGINT,
   PRIMARY KEY ((transaction_group_id), transaction_id)
);

CREATE TABLE universe.treasury_transaction_dummy_coin (
   transaction_group_id UUID,
   transaction_id UUID,
   amount BIGINT,
   PRIMARY KEY ((transaction_group_id), transaction_id)
);

CREATE TABLE universe.treasury_transaction_earn_coin (
   transaction_group_id UUID,
   transaction_id UUID,
   amount BIGINT,
   PRIMARY KEY ((transaction_group_id), transaction_id)
);

CREATE TABLE universe.treasury_transaction_purchase_coin (
   transaction_group_id UUID,
   transaction_id UUID,
   amount BIGINT,
   PRIMARY KEY ((transaction_group_id), transaction_id)
);

-- -----------------------------------------------------------------
-- TEST DATA
-- -----------------------------------------------------------------

-- CREATE USERS
INSERT INTO universe.user (user_id, username, isPayingUser, created_at, updated_at) 
VALUES (00000000-0000-0000-0000-000000000001, 'adrien', false, toTimestamp(now()), toTimestamp(now())) IF NOT EXISTS;

INSERT INTO universe.user (user_id, username, isPayingUser, created_at, updated_at) 
VALUES (00000000-0000-0000-0000-000000000002, 'gaetan', false, toTimestamp(now()), toTimestamp(now())) IF NOT EXISTS;

INSERT INTO universe.user (user_id, username, isPayingUser, created_at, updated_at) 
VALUES (00000000-0000-0000-0000-000000000003, 'arthur', false, toTimestamp(now()), toTimestamp(now())) IF NOT EXISTS;

-- -----------------------------------------------------------------
-- OPERATIONS
-- -----------------------------------------------------------------

-- 1. Get amount of coins a user has (total, purchased, rewarded, granted)

SELECT * FROM universe.user_balance WHERE user_id = 00000000-0000-0000-0000-000000000001;

-- 2. Check if a specific COIN GRANT has already been given to a user

-- TODO: test and fix this
SELECT * FROM universe.grant_coin_transaction_by_user 
WHERE user_id = 00000000-0000-0000-0000-000000000001 
AND created_at <= toTimestamp(now()) AND transaction_type = 'grant' AND info = 'phone_number_provided';

-- 3. Give coins to a user (as a COIN GRANT)

-- 1. get lock on user_balance
-- 2. create transaction
-- 3. update user_balance
-- 4. release lock

-- Note: since this is not a BATCH, we need to check the success of each operation (especially the locking)

-- acquire lock (for 15 days max)
INSERT INTO universe.user_balance_lock (user_id, created_at) VALUES (00000000-0000-0000-0000-000000000001, toTimestamp(now())) IF NOT EXISTS USING TTL 1296000;

-- create transaction(s) (this part should be a BATCH later, when transactions get more complex)
BEGIN BATCH

-- create transaction group
INSERT INTO universe.transaction_group (transaction_group_id, info, created_at)
   VALUES (00000000-0000-0000-9999-000000000001, '{"type":"phone_number_provided"}', toTimestamp(now()))

-- create user transaction
INSERT INTO universe.grant_coin_transaction_by_user (user_id, transaction_id, transaction_group_id, amount)
   VALUES (00000000-0000-0000-0000-000000000001, uuid(), 00000000-0000-0000-9999-000000000001, 15000000)

-- create treasury transaction
INSERT INTO universe.treasury_transaction_grant_coin (transaction_group_id, transaction_id, amount)
   VALUES (00000000-0000-0000-9999-000000000001, uuid(), -15000000)

APPLY BATCH;

-- update user balance
UPDATE universe.user_balance
SET total_granted_coins = total_granted_coins + 15000000,
    total_coins = total_coins + 15000000
WHERE user_id = 00000000-0000-0000-0000-000000000001;

-- release lock
DELETE FROM universe.user_balance_lock WHERE user_id = 00000000-0000-0000-0000-000000000001;




-- INSERT INTO universe.grant_coin_transaction_by_user (user_id, transaction_id, transaction_group_id, amount, info, created_at)
-- VALUES (00000000-0000-0000-0000-000000000001, uuid(), 00000000-0000-0000-9999-000000000001, 15000000, 'phone_number_provided', toTimestamp(now())) IF NOT EXISTS;

-- SELECT * FROM universe.grant_coin_transaction_by_user WHERE user_id = 00000000-0000-0000-0000-000000000001;

-- -- CREATE USERS
-- INSERT INTO universe.user (user_id, username, total_granted_coins, total_earned_coins, total_purchased_coins, total_coins, created_at, updated_at) 
-- VALUES (00000000-0000-0000-0000-000000000001, 'adrien', 0, 0, 0, 0, toTimestamp(now()), toTimestamp(now())) IF NOT EXISTS;

-- INSERT INTO universe.user (user_id, username, total_granted_coins, total_earned_coins, total_purchased_coins, total_coins, created_at, updated_at) 
-- VALUES (00000000-0000-0000-0000-000000000002, 'gaetan', 0, 0, 0, 0, toTimestamp(now()), toTimestamp(now())) IF NOT EXISTS;

-- INSERT INTO universe.user (user_id, username, total_granted_coins, total_earned_coins, total_purchased_coins, total_coins, created_at, updated_at) 
-- VALUES (00000000-0000-0000-0000-000000000003, 'arthur', 0, 0, 0, 0, toTimestamp(now()), toTimestamp(now())) IF NOT EXISTS;

-- -----------------------------------------------------------------
-- OPERATIONS
-- -----------------------------------------------------------------

-- User receives a grant
-- -------------------------

-- 1. get lock on user (for 5min)
-- 2. get user account
-- 3. create transaction
-- 4. update user account
-- 5. release lock

-- BEGIN BATCH
-- INSERT INTO universe.user_lock (user_id, created_at) VALUES (00000000-0000-0000-0000-000000000001, toTimestamp(now())) IF NOT EXISTS
-- DELETE FROM universe.user_lock WHERE user_id = 00000000-0000-0000-0000-000000000001
-- APPLY BATCH;

BEGIN BATCH
-- IDEA: lock on user table directly ???
INSERT INTO universe.user_lock (user_id, created_at) VALUES (00000000-0000-0000-0000-000000000001, toTimestamp(now())) IF NOT EXISTS

INSERT INTO universe.transaction_by_user (id, user_id, transaction_type, info, coin_amount, created_at) 
   VALUES (uuid(), 00000000-0000-0000-0000-000000000001, 'grant', 'phone_number_provided', 15000000, toTimestamp(now()))

UPDATE universe.user_coins
SET total_granted_coins = total_granted_coins + 15000000,  -- Update the total granted coins (5 coins)
    total_coins = total_coins + 15000000                  -- Update the total coins (5 coins)
WHERE user_id = 00000000-0000-0000-0000-000000000001

DELETE FROM universe.user_lock WHERE user_id = 00000000-0000-0000-0000-000000000001
APPLY BATCH;

-- from 11 to 22
-- BEGIN BATCH
-- INSERT INTO universe.transaction (id, account_id, other_id, amount, created_at) VALUES (00000000-0000-0000-0000-000000000101, 00000000-0000-0000-0000-000000000011, 00000000-0000-0000-0000-000000000022, -42, toTimestamp(now()));
-- INSERT INTO universe.transaction (id, account_id, other_id, amount, created_at) VALUES (00000000-0000-0000-0000-000000000102, 00000000-0000-0000-0000-000000000022, 00000000-0000-0000-0000-000000000011, 42, toTimestamp(now()));
-- APPLY BATCH;

-- User earns coins (similar to a grant)
-- -------------------------

-- User spends coins
-- -------------------------

-- Should the transaction be 2-way ???

-- =================================================================

-- GET USER INFO in multiple tables
-- SELECT * FROM universe.user WHERE user_id = 00000000-0000-0000-0000-000000000001;
-- SELECT * FROM universe.user_balance WHERE user_id = 00000000-0000-0000-0000-000000000001;

-- CREATE ACCOUNTS
-- INSERT INTO universe.account (id, type, user_id, created_at, updated_at) VALUES (00000000-0000-0000-0000-000000000000, 'treasury', NULL, toTimestamp(now()), toTimestamp(now())) IF NOT EXISTS;
-- INSERT INTO universe.account (id, type, user_id, created_at, updated_at) VALUES (00000000-0000-0000-0000-000000000011, 'coins_regular', 00000000-0000-0000-0000-000000000001, toTimestamp(now()), toTimestamp(now())) IF NOT EXISTS;
-- INSERT INTO universe.account (id, type, user_id, created_at, updated_at) VALUES (00000000-0000-0000-0000-000000000022, 'coins_regular', 00000000-0000-0000-0000-000000000002, toTimestamp(now()), toTimestamp(now())) IF NOT EXISTS;
-- INSERT INTO universe.account (id, type, user_id, created_at, updated_at) VALUES (00000000-0000-0000-0000-000000000033, 'coins_regular', 00000000-0000-0000-0000-000000000003, toTimestamp(now()), toTimestamp(now())) IF NOT EXISTS;

-- -- --------------------
-- -- Account
-- -- --------------------
-- CREATE TABLE universe.account (
--    id UUID,
--    transactionLock UUID, -- id of the transaction currently locking this account
--    owners SET(UUID),
--    purchasedCoins BIGINT, -- in millionths of coins
--    rewardedCoins BIGINT, -- in millionths of coins
--    grantedCoins BIGINT, -- in millionths of coins
--    created_at TIMESTAMP,
--    updated_at TIMESTAMP,
--    -- items SET(UUID), -- IDs of items owned by this account
--    -- worlds SET(UUID), -- IDs of worlds owned by this account
--    PRIMARY KEY ((id)) -- partition key, clustering key
-- );

-- -- --------------------
-- -- Transaction
-- -- --------------------
-- CREATE TABLE universe.transaction (
--    id UUID,
--    status TEXT, -- created, ...
--    error TEXT, -- can be empty
--    json TEXT, -- JSON representation of the transaction
--    created_at TIMESTAMP,
--    PRIMARY KEY ((id))
-- );


-- -- Accounts
-- CREATE TABLE universe.account (
--    id UUID,
--    type TEXT, -- platform_regular, platform_premium, ...
--    user_id UUID, -- can be empty (ex: platform accounts)
--    -- canBeNegative BOOLEAN, -- can the account balance go negative?
--    created_at TIMESTAMP,
--    updated_at TIMESTAMP,
--    PRIMARY KEY ((id), user_id, type) -- partition key, clustering key
-- );

-- -- TRANSACTION
-- CREATE TABLE universe.transaction (
--    id UUID,
--    account_id UUID,
--    -- user_id UUID, -- can be empty (ex: platform accounts)
--    other_id UUID,
--    amount BIGINT, -- in millionths of coins
--    created_at TIMESTAMP,
--    PRIMARY KEY ((account_id), id)
-- );

-- -- LOCK (for account records)
-- -- 
-- -- Records have a TTL of 5 min (for ex.),
-- -- but will be removed explicitly when a transaction happens successfully
-- CREATE TABLE universe.account_lock (
--    account_id UUID,
--    created_at TIMESTAMP,
--    PRIMARY KEY ((account_id))
-- );

-- 
-- OPERATION : COINS GRANT FROM CUBZH TO USER
-- 

-- 1. get account1 ID (by userID and type)
-- 2. get account2 ID (by userID and type)
-- 3. lock account1
-- 4. lock account2
-- 5. create transaction from account1 to account2

-- ?. unlock account1

-- -----------------------------------------------------------------

-- materialized view : 
--    how much on a specific account
--    how much a specific user has available

-- write a transaction as 2 records (like in accounting)

-- chain transactions

-- -----------------------------------------------------------------

-- List all keyspaces
SELECT keyspace_name FROM system_schema.keyspaces;
-- List all tables in a keyspace
SELECT table_name FROM system_schema.tables WHERE keyspace_name = 'universe';

-- Create a keyspace
-- durable_writes is true by default
CREATE KEYSPACE universe WITH replication = {'class': 'NetworkTopologyStrategy', 'AWS_US_EAST_2' : 3} AND durable_writes = true;
-- DEV
-- CREATE KEYSPACE universe WITH REPLICATION = {'class' : 'NetworkTopologyStrategy', 'replication_factor' : 1};

-- MODULES

-- Create a table for usernames moderation caching
CREATE TABLE universe.module (
   origin TEXT,
   commit TEXT,
   script TEXT,
   documentation TEXT,
   commit_created_at TIMESTAMP,
   created_at TIMESTAMP,
   updated_at TIMESTAMP,
   PRIMARY KEY ((origin), commit)
);

-- REFS

CREATE TABLE universe.module_ref (
   origin TEXT,
   ref TEXT,
   commit TEXT,
   ref_is_commit BOOLEAN, -- True means ref is prefix of a commit hash. False means ref is a branch/tag name.
   hash TEXT, -- security checksum -- hash of (origin + ref + module.script)
   created_at TIMESTAMP,
   updated_at TIMESTAMP,
   PRIMARY KEY ((origin), ref)
);

INSERT INTO universe.module_ref (origin, ref, commit, ref_is_commit, created_at, updated_at) VALUES ('github.com/aduermael/mymodule', '12345678901234567890', '12345678901234567890', true, toTimestamp(now()), toTimestamp(now())) IF NOT EXISTS;
INSERT INTO universe.module_ref (origin, ref, commit, ref_is_commit, created_at, updated_at) VALUES ('github.com/aduermael/mymodule', '123456', '12345678901234567890', true, toTimestamp(now()), toTimestamp(now())) IF NOT EXISTS;
INSERT INTO universe.module_ref (origin, ref, commit, ref_is_commit, created_at, updated_at) VALUES ('github.com/aduermael/mymodule', 'master', '12345678901234567890', false, toTimestamp(now()), toTimestamp(now())) IF NOT EXISTS;

-- CONTRIBUTORS

CREATE TABLE universe.module_contributor (
   origin TEXT,
   commit TEXT,
   user_id UUID,
   user_name TEXT,
   share FLOAT, -- Can be anything. If Adrien has 1.5, Gaetan has 0.5, and Caillef has 0.5, Adrien will have 60% and Gaetan / Caillef 20% each.
   PRIMARY KEY ((origin), commit, user_id)
);

-- -----------------------------------------------------------------
-- 
-- TEST DATA
--
-- -----------------------------------------------------------------

DROP TABLE IF EXISTS universe.module;
DROP TABLE IF EXISTS universe.module_ref;
DROP TABLE IF EXISTS universe.module_contributor;

CREATE TABLE universe.module (
   origin TEXT,
   commit TEXT,
   script TEXT,
   documentation TEXT,
   commit_created_at TIMESTAMP,
   created_at TIMESTAMP,
   updated_at TIMESTAMP,
   PRIMARY KEY ((origin), commit)
);

CREATE TABLE universe.module_ref (
   origin TEXT,
   ref TEXT,
   commit TEXT,
   ref_is_commit BOOLEAN,
   hash TEXT,
   created_at TIMESTAMP,
   updated_at TIMESTAMP,
   PRIMARY KEY ((origin), ref)
);

CREATE TABLE universe.module_contributor (
   origin TEXT,
   commit TEXT,
   user_id UUID,
   user_name TEXT,
   share FLOAT,
   PRIMARY KEY ((origin), commit, user_id)
);