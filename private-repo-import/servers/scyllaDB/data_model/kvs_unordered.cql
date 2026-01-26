-- List all keyspaces
SELECT keyspace_name FROM system_schema.keyspaces;
-- List all tables in a keyspace
SELECT table_name FROM system_schema.tables WHERE keyspace_name = 'kvs';

-- Create a keyspace
CREATE KEYSPACE kvs WITH replication = {'class' : 'NetworkTopologyStrategy', 'replication_factor' : 1};
-- durable_writes is true by default
CREATE KEYSPACE kvs WITH replication = {'class': 'NetworkTopologyStrategy', 'AWS_US_EAST_2' : 3} AND durable_writes = true;

-- Use this keyspace
-- use kvs;

-- Create a table for the KVS data (unordered values)
CREATE TABLE kvs.unordered (
   world_id uuid,
   store text,
   key text,
   value blob,
   PRIMARY KEY ((world_id, store), key)
);

-- Insert data

INSERT INTO kvs.unordered(world_id, store, key, value) VALUES (00b160e8-2aa2-43b0-821b-03daa8f50c41, 'bar', 'first', 0x2A) IF NOT EXISTS;
INSERT INTO kvs.unordered(world_id, store, key, value) VALUES (00b160e8-2aa2-43b0-821b-03daa8f50c41, 'bar', 'fourth', 0x2D) IF NOT EXISTS;
INSERT INTO kvs.unordered(world_id, store, key, value) VALUES (00b160e8-2aa2-43b0-821b-03daa8f50c41, 'bar', 'second', 0x2B) IF NOT EXISTS;
INSERT INTO kvs.unordered(world_id, store, key, value) VALUES (00b160e8-2aa2-43b0-821b-03daa8f50c41, 'bar', 'third', 0x2C) IF NOT EXISTS;

INSERT INTO kvs.unordered(world_id, store, key, value) VALUES (00b160e8-2aa2-43b0-821b-03daa8f50c41, 'foo', 'first', 0x2A) IF NOT EXISTS;
INSERT INTO kvs.unordered(world_id, store, key, value) VALUES (00b160e8-2aa2-43b0-821b-03daa8f50c41, 'foo', 'second', 0x2B) IF NOT EXISTS;
INSERT INTO kvs.unordered(world_id, store, key, value) VALUES (00b160e8-2aa2-43b0-821b-03daa8f50c41, 'foo', 'fourth', 0x2D) IF NOT EXISTS;
INSERT INTO kvs.unordered(world_id, store, key, value) VALUES (00b160e8-2aa2-43b0-821b-03daa8f50c41, 'foo', 'third', 0x2C) IF NOT EXISTS;

INSERT INTO kvs.unordered(world_id, store, key, value) VALUES (00b160e8-2aa2-43b0-821b-03daa8f50c42, 'bar', 'first', 0x2A) IF NOT EXISTS;
INSERT INTO kvs.unordered(world_id, store, key, value) VALUES (00b160e8-2aa2-43b0-821b-03daa8f50c42, 'bar', 'second', 0x2B) IF NOT EXISTS;
INSERT INTO kvs.unordered(world_id, store, key, value) VALUES (00b160e8-2aa2-43b0-821b-03daa8f50c42, 'bar', 'third', 0x2C) IF NOT EXISTS;
INSERT INTO kvs.unordered(world_id, store, key, value) VALUES (00b160e8-2aa2-43b0-821b-03daa8f50c42, 'bar', 'fourth', 0x2D) IF NOT EXISTS;

INSERT INTO kvs.unordered(world_id, store, key, value) VALUES (00b160e8-2aa2-43b0-821b-03daa8f50c42, 'foo', 'first', 0x2A) IF NOT EXISTS;
INSERT INTO kvs.unordered(world_id, store, key, value) VALUES (00b160e8-2aa2-43b0-821b-03daa8f50c42, 'foo', 'second', 0x2B) IF NOT EXISTS;
INSERT INTO kvs.unordered(world_id, store, key, value) VALUES (00b160e8-2aa2-43b0-821b-03daa8f50c42, 'foo', 'third', 0x2C) IF NOT EXISTS;
INSERT INTO kvs.unordered(world_id, store, key, value) VALUES (00b160e8-2aa2-43b0-821b-03daa8f50c42, 'foo', 'fourth', 0x2D) IF NOT EXISTS;

-- Read data

-- whole table
SELECT * FROM kvs.unordered;
-- for 1 world
SELECT * FROM kvs.unordered WHERE world_id = 00b160e8-2aa2-43b0-821b-03daa8f50c41; --ALLOW FILTERING
-- for 1 world AND 1 store
SELECT * FROM kvs.unordered WHERE world_id = 00b160e8-2aa2-43b0-821b-03daa8f50c41 AND store = 'bar' AND key = 'first';
-- for 1 world AND 1 store AND 1 key
SELECT key, value FROM kvs.unordered WHERE world_id = 00b160e8-2aa2-43b0-821b-03daa8f50c41 AND store = 'bar' AND key IN ('first', 'second');

-- Delete records
DELETE FROM kvs.unordered WHERE world_id = 00b160e8-2aa2-43b0-821b-03daa8f50c41 AND store = 'foo';

-- Empty table
TRUNCATE kvs.unordered;

-- Delete table
DROP TABLE kvs.unordered;

-- 
-- /////////////// DEBUG DB //////////////
-- 

CREATE KEYSPACE kvs WITH replication = {'class' : 'NetworkTopologyStrategy', 'replication_factor' : 1};

CREATE TABLE kvs.unordered (
   world_id uuid,
   store text,
   key text,
   value blob,
   PRIMARY KEY ((world_id, store), key)
);

INSERT INTO kvs.unordered(world_id, store, key, value) VALUES (00b160e8-2aa2-43b0-821b-03daa8f50c41, 'bar', 'first', 0x2A) IF NOT EXISTS;
INSERT INTO kvs.unordered(world_id, store, key, value) VALUES (00b160e8-2aa2-43b0-821b-03daa8f50c41, 'bar', 'fourth', 0x2D) IF NOT EXISTS;
INSERT INTO kvs.unordered(world_id, store, key, value) VALUES (00b160e8-2aa2-43b0-821b-03daa8f50c41, 'bar', 'second', 0x2B) IF NOT EXISTS;
INSERT INTO kvs.unordered(world_id, store, key, value) VALUES (00b160e8-2aa2-43b0-821b-03daa8f50c41, 'bar', 'third', 0x2C) IF NOT EXISTS;

INSERT INTO kvs.unordered(world_id, store, key, value) VALUES (00b160e8-2aa2-43b0-821b-03daa8f50c41, 'foo', 'first', 0x2A) IF NOT EXISTS;
INSERT INTO kvs.unordered(world_id, store, key, value) VALUES (00b160e8-2aa2-43b0-821b-03daa8f50c41, 'foo', 'second', 0x2B) IF NOT EXISTS;
INSERT INTO kvs.unordered(world_id, store, key, value) VALUES (00b160e8-2aa2-43b0-821b-03daa8f50c41, 'foo', 'fourth', 0x2D) IF NOT EXISTS;
INSERT INTO kvs.unordered(world_id, store, key, value) VALUES (00b160e8-2aa2-43b0-821b-03daa8f50c41, 'foo', 'third', 0x2C) IF NOT EXISTS;

INSERT INTO kvs.unordered(world_id, store, key, value) VALUES (00b160e8-2aa2-43b0-821b-03daa8f50c42, 'bar', 'first', 0x2A) IF NOT EXISTS;
INSERT INTO kvs.unordered(world_id, store, key, value) VALUES (00b160e8-2aa2-43b0-821b-03daa8f50c42, 'bar', 'second', 0x2B) IF NOT EXISTS;
INSERT INTO kvs.unordered(world_id, store, key, value) VALUES (00b160e8-2aa2-43b0-821b-03daa8f50c42, 'bar', 'third', 0x2C) IF NOT EXISTS;
INSERT INTO kvs.unordered(world_id, store, key, value) VALUES (00b160e8-2aa2-43b0-821b-03daa8f50c42, 'bar', 'fourth', 0x2D) IF NOT EXISTS;

INSERT INTO kvs.unordered(world_id, store, key, value) VALUES (00b160e8-2aa2-43b0-821b-03daa8f50c42, 'foo', 'first', 0x2A) IF NOT EXISTS;
INSERT INTO kvs.unordered(world_id, store, key, value) VALUES (00b160e8-2aa2-43b0-821b-03daa8f50c42, 'foo', 'second', 0x2B) IF NOT EXISTS;
INSERT INTO kvs.unordered(world_id, store, key, value) VALUES (00b160e8-2aa2-43b0-821b-03daa8f50c42, 'foo', 'third', 0x2C) IF NOT EXISTS;
INSERT INTO kvs.unordered(world_id, store, key, value) VALUES (00b160e8-2aa2-43b0-821b-03daa8f50c42, 'foo', 'fourth', 0x2D) IF NOT EXISTS;
