-- List all keyspaces
SELECT keyspace_name FROM system_schema.keyspaces;
-- List all tables in a keyspace
SELECT table_name FROM system_schema.tables WHERE keyspace_name = 'universe';

-- /////////////////////////////////////////////////////////

CREATE KEYSPACE universe WITH replication = {'class': 'NetworkTopologyStrategy', 'AWS_US_EAST_2' : 3} AND durable_writes = true;

-- whole table
SELECT * FROM universe.relation;
-- 
SELECT * FROM universe.relation WHERE sender_id = 00000000-0000-0000-0000-000000000001; --ALLOW FILTERING
SELECT * FROM universe.relation WHERE recipient_id = 00000000-0000-0000-0000-000000000002; --ALLOW FILTERING
-- Empty table
TRUNCATE universe.relation;
-- Delete table
DROP TABLE universe.relation;

-- 
-- DEV
-- 

DROP KEYSPACE IF EXISTS universe;
CREATE KEYSPACE universe WITH REPLICATION = {'class' : 'NetworkTopologyStrategy', 'replication_factor' : 1};

DROP TABLE IF EXISTS universe.relation;

-- 2 records by relation if relation goes both ways
CREATE TABLE universe.relation (
    id1 UUID,
    name1 TEXT,
    id2 UUID,
    name2 TEXT,
    type TEXT,
    created_at TIMESTAMP,
   PRIMARY KEY ((id1), type, id2)
);

-- relation type : frReqSent / frReqRcvd / frRel / [LATER: block, mute, follower, followed, like, liked]
-- INSERT INTO universe.relation(id1, name1, id2, name2, type, created_at) VALUES (00000000-0000-0000-0000-000000000001, 'user1', 00000000-0000-0000-0000-000000000002, 'user2', 'frReqSent', toTimestamp(now())) IF NOT EXISTS;
-- SELECT * FROM universe.relation WHERE id1 = 00000000-0000-0000-0000-000000000001 AND type = 'frReqSent';

-- 
-- TEST DATA
-- 
DROP KEYSPACE IF EXISTS universe;
CREATE KEYSPACE universe WITH REPLICATION = {'class' : 'NetworkTopologyStrategy', 'replication_factor' : 1};
DROP TABLE IF EXISTS universe.relation;
CREATE TABLE universe.relation (
    id1 UUID,
    name1 TEXT,
    id2 UUID,
    name2 TEXT,
    type TEXT,
    created_at TIMESTAMP,
   PRIMARY KEY ((id1), type, id2)
);
INSERT INTO universe.relation(id1, name1, id2, name2, type, created_at) VALUES (00000000-0000-0000-0000-000000000001, 'user1', 00000000-0000-0000-0000-000000000002, 'user2', 'frReqSent', toTimestamp(now())) IF NOT EXISTS;
INSERT INTO universe.relation(id1, name1, id2, name2, type, created_at) VALUES (00000000-0000-0000-0000-000000000002, 'user2', 00000000-0000-0000-0000-000000000001, 'user1', 'frReqRcvd', toTimestamp(now())) IF NOT EXISTS;

INSERT INTO universe.relation(id1, name1, id2, name2, type, created_at) VALUES (00000000-0000-0000-0000-000000000003, 'user3', 00000000-0000-0000-0000-000000000001, 'user1', 'frReqSent', toTimestamp(now())) IF NOT EXISTS;
INSERT INTO universe.relation(id1, name1, id2, name2, type, created_at) VALUES (00000000-0000-0000-0000-000000000001, 'user1', 00000000-0000-0000-0000-000000000003, 'user3', 'frReqRcvd', toTimestamp(now())) IF NOT EXISTS;

INSERT INTO universe.relation(id1, name1, id2, name2, type, created_at) VALUES (00000000-0000-0000-0000-000000000001, 'user1', 00000000-0000-0000-0000-000000000004, 'user4', 'frRel', toTimestamp(now())) IF NOT EXISTS;
INSERT INTO universe.relation(id1, name1, id2, name2, type, created_at) VALUES (00000000-0000-0000-0000-000000000004, 'user4', 00000000-0000-0000-0000-000000000001, 'user1', 'frRel', toTimestamp(now())) IF NOT EXISTS;

INSERT INTO universe.relation(id1, name1, id2, name2, type, created_at) VALUES (00000000-0000-0000-0000-000000000003, 'user3', 00000000-0000-0000-0000-000000000004, 'user4', 'frReqSent', toTimestamp(now())) IF NOT EXISTS;
INSERT INTO universe.relation(id1, name1, id2, name2, type, created_at) VALUES (00000000-0000-0000-0000-000000000004, 'user4', 00000000-0000-0000-0000-000000000003, 'user3', 'frReqRcvd', toTimestamp(now())) IF NOT EXISTS;
