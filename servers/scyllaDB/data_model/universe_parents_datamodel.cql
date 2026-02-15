-- List all keyspaces
SELECT keyspace_name FROM system_schema.keyspaces;
-- List all tables in a keyspace
SELECT table_name FROM system_schema.tables WHERE keyspace_name = 'universe';

-- /////////////////////////////////////////////////////////

-- Create a keyspace
CREATE KEYSPACE universe WITH replication = {'class': 'NetworkTopologyStrategy', 'AWS_US_EAST_2' : 3} AND durable_writes = true;
-- DEV
-- CREATE KEYSPACE universe WITH replication = {'class' : 'NetworkTopologyStrategy', 'replication_factor' : 1} AND durable_writes = true;

-- https://url/<dashboardID>/<secretKey>
-- https://dashboard/?parent=00001&secretKey=456

DROP TABLE IF EXISTS universe.parent_dashboard;

-- Create a table for parent-child relationships
CREATE TABLE universe.parent_dashboard (
    dashboard_id TEXT,
    child_id UUID,
    parent_id UUID,
    secret_key_hash TEXT,
    created_at TIMESTAMP,
    updated_at TIMESTAMP,
    PRIMARY KEY ((dashboard_id))
);

CREATE INDEX universe_parent_dashboard_by_child ON universe.parent_dashboard (child_id);
CREATE INDEX universe_parent_dashboard_by_parent ON universe.parent_dashboard (parent_id);

-- Settings JSON could include a format version
-- {
--     "version": 1,
--     "access_approved": true,
--     "ingame_chat": true,
--     "private_messages": true
-- }

-- ----------------------------------------
-- REQUESTS WE WANT TO SUPPORT
-- ----------------------------------------

-- 1. Create a record
INSERT INTO universe.parent_dashboard(dashboard_id, child_id, parent_id, secret_key_hash, created_at, updated_at) VALUES ('123', 00000000-0000-0000-0000-000000000001, 00000000-0000-0000-0000-000000000002, '456', toTimestamp(now()), toTimestamp(now())) IF NOT EXISTS;
INSERT INTO universe.parent_dashboard(dashboard_id, child_id, parent_id, secret_key_hash, created_at, updated_at) VALUES ('124', 00000000-0000-0000-0000-000000000003, 00000000-0000-0000-0000-000000000002, '456', toTimestamp(now()), toTimestamp(now())) IF NOT EXISTS;

-- 2. Get a record by dashboard_id
SELECT * FROM universe.parent_dashboard WHERE dashboard_id = '123';

-- 3. Get all records for a given child
SELECT * FROM universe.parent_dashboard WHERE child_id = 00000000-0000-0000-0000-000000000001;

-- 4. Get all records for a given parent
SELECT * FROM universe.parent_dashboard WHERE parent_id = 00000000-0000-0000-0000-000000000002;

-- 5. Update record for a given child/parent pair
-- UPDATE universe.parent_dashboard SET updated_at = toTimestamp(now()) WHERE dashboard_id = '123';

-- 6. Delete record for a given dashboard_id
-- DELETE FROM universe.parent_dashboard WHERE dashboard_id = '123';
