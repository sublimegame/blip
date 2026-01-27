-- List all keyspaces
SELECT keyspace_name FROM system_schema.keyspaces;
-- List all tables in a keyspace
SELECT table_name FROM system_schema.tables WHERE keyspace_name = 'moderation';

-- ----------------------------------

-- Create a keyspace
-- CREATE KEYSPACE moderation WITH REPLICATION = {'class' : 'NetworkTopologyStrategy', 'replication_factor' : 1};
CREATE KEYSPACE moderation WITH replication = {'class': 'NetworkTopologyStrategy', 'AWS_US_EAST_2' : 3} AND durable_writes = true;

-- Use this keyspace
-- use moderation;

-- Create a table for usernames moderation caching
CREATE TABLE moderation.username (
   username text,
   appropriate boolean,
   created timestamp,
   PRIMARY KEY (username)
);

-- Insert data

INSERT INTO moderation.username(username, appropriate, created) VALUES ('gaetan', true, toTimestamp(now())) IF NOT EXISTS;
INSERT INTO moderation.username(username, appropriate, created) VALUES ('offensive', false, toTimestamp(now())) IF NOT EXISTS;

--
--
--

-- Read data

-- whole table
SELECT * FROM moderation.username;

-- 
-- SELECT * FROM kvs.unordered WHERE world_id = 00b160e8-2aa2-43b0-821b-03daa8f50c41; --ALLOW FILTERING

-- Delete records
-- DELETE FROM kvs.unordered WHERE world_id = 00b160e8-2aa2-43b0-821b-03daa8f50c41 AND store = 'foo';

-- Empty table
TRUNCATE moderation.username;

-- Delete table
DROP TABLE moderation.username;

-- /////////////////////////////////////////////////////////////////////
-- CHAT MESSAGES
-- /////////////////////////////////////////////////////////////////////

-- docker run --rm --name scyllaU -d -p 9042:9042 -d scylladb/scylla:5.2.0 --overprovisioned 1 --smp 1
-- docker exec -ti scyllaU bash
-- cqlsh

-- WRITE
-- 1. Insert a message
-- 2. Update a message (status, reason)
-- 3. Delete a message (after a given time)

-- READ
-- 1. Get message by its UUID
-- 2. Get messages for time interval
-- 3. Get messages for a user
-- 4. Get messages for a user and a time interval
-- LATER
-- 5. Get messages for a server and time interval

CREATE KEYSPACE moderation WITH REPLICATION = {'class' : 'NetworkTopologyStrategy', 'replication_factor' : 1};

DROP MATERIALIZED VIEW IF EXISTS moderation.chat_message_by_user;
DROP MATERIALIZED VIEW IF EXISTS moderation.chat_message_by_server;
DROP TABLE IF EXISTS moderation.chat_message;

CREATE TABLE moderation.chat_message (
   id UUID,
   senderID UUID,
   createdAt timestamp,
   updatedAt timestamp,
   serverID TEXT, -- string containing the worldID, regionID, and instanceID
   content TEXT,
   status TINYINT, -- ok, removed, hidden
   reason TINYINT, -- removed by teammember, removed by ggwp, user banned from Cubzh, ...
   -- TODO: filteredContent TEXT, -- content with bad words replaced by ***
   -- TODO: recommendedContent TEXT,
   -- mentions TEXT, -- JSON ARRAY
   PRIMARY KEY ((id), createdAt)
);

CREATE MATERIALIZED VIEW moderation.chat_message_by_user AS
        SELECT * FROM moderation.chat_message
        WHERE senderID IS NOT NULL AND createdAt IS NOT NULL
        PRIMARY KEY((senderID), createdAt, id);

CREATE MATERIALIZED VIEW moderation.chat_message_by_server AS
        SELECT * FROM moderation.chat_message
        WHERE serverID IS NOT NULL AND createdAt IS NOT NULL
        PRIMARY KEY((serverID), createdAt, id);

INSERT INTO moderation.chat_message(id, senderID, createdAt, updatedAt, serverID, content, status, reason) VALUES (00000000-0000-0000-0000-000000000001, 00000000-0000-0000-0000-000000000111, toTimestamp(now()), toTimestamp(now()), 'worldID.regionID.instanceID', 'Hello world! 1', 11, 21) IF NOT EXISTS;
INSERT INTO moderation.chat_message(id, senderID, createdAt, updatedAt, serverID, content, status, reason) VALUES (00000000-0000-0000-0000-000000000002, 00000000-0000-0000-0000-000000000111, toTimestamp(now()), toTimestamp(now()), 'worldID.regionID.instanceID', 'Hello world! 2', 11, 21) IF NOT EXISTS;
INSERT INTO moderation.chat_message(id, senderID, createdAt, updatedAt, serverID, content, status, reason) VALUES (00000000-0000-0000-0000-000000000003, 00000000-0000-0000-0000-000000000222, toTimestamp(now()), toTimestamp(now()), 'worldID.regionID.instanceID', 'Hello world! 3', 11, 21) IF NOT EXISTS;
INSERT INTO moderation.chat_message(id, senderID, createdAt, updatedAt, serverID, content, status, reason) VALUES (00000000-0000-0000-0000-000000000004, 00000000-0000-0000-0000-000000000222, toTimestamp(now()), toTimestamp(now()), 'worldID.regionID.instanceID', 'Hello world! 4', 11, 21) IF NOT EXISTS;

TRUNCATE moderation.chat_message;
SELECT * FROM moderation.chat_message;

-- get messages by ID
SELECT * FROM moderation.chat_message;
SELECT * FROM moderation.chat_message WHERE id = 00000000-0000-0000-0000-000000000001;
SELECT * FROM moderation.chat_message WHERE createdAt < toTimestamp(now());

-- get messages by userID
SELECT * FROM moderation.chat_message_by_user WHERE senderID = 00000000-0000-0000-0000-000000000111;
SELECT * FROM moderation.chat_message_by_user WHERE senderID = 00000000-0000-0000-0000-000000000111 AND createdAt >= toTimestamp(now());

-- get messages by serverID
SELECT * FROM moderation.chat_message_by_server WHERE serverID = 'worldID.regionID.instanceID';
SELECT * FROM moderation.chat_message_by_server WHERE serverID = 'worldID.regionID.instanceID' AND createdAt <= toTimestamp(now());

-- NOTES

-- gameserver calls HUB API to store chat messages
-- HUB Server stores chat messages in ScyllaDB
-- HUB Server calls ggwp API to validate chat messages
-- HUB Server replies to gameserver with the validation result
-- gameserver dispatches the chat message to all players

-- messages sent could be displayed right away (greyed out) until validated by the moderation team and dispatched to all other (like on Discord)
-- on error : display "message not sent, and was reported to the moderation team"

-- TODO
-- 1. add route in HUB API to store chat messages
        -- store in db
        -- call ggwp API
        -- update db record if moderation needed
        -- respond to gameserver

-- HTTP POST /gameserver-chat-msg
-- REQUEST
-- {
--     "sender_id": "00000000-0000-0000-0000-000000000111",
--     "msg_content": "coucou",
--     "server_id": "worldID.regionID.instanceID"
--     "mentionned_ids": ["00000000-0000-0000-0000-000000000222", "00000000-0000-0000-0000-000000000333"] -- later
-- }
-- RESPONSE
-- {
--     "msg_id": "00000000-0000-0000-0000-000000000001",
-- ...
-- }

-- /////////////////////////////////////////////////////////////////////
-- REPORT RESOURCES
-- /////////////////////////////////////////////////////////////////////
