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