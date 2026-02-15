--
-- WORLD LAST LAUNCH
--

--
-- Note: the world_like table should have the same structure.
--       (it is in mongoDB for now)
--

-- ----------------------
-- Data model
-- ----------------------

-- DROP KEYSPACE IF EXISTS universe;
-- CREATE KEYSPACE universe WITH replication = {'class' : 'NetworkTopologyStrategy', 'replication_factor' : 1};

-- DROP MATERIALIZED VIEW IF EXISTS universe.world_last_launch_by_user;
-- DROP MATERIALIZED VIEW IF EXISTS universe.world_last_launch_by_world;
-- DROP TABLE IF EXISTS universe.world_last_launch;

CREATE TABLE universe.world_last_launch (
    world_id UUID,
    user_id UUID,
    updated_at TIMESTAMP,
    PRIMARY KEY ((world_id), user_id)
);

-- indexing by user_id and order by updated_at (DESC) for time-based queries
CREATE MATERIALIZED VIEW universe.world_last_launch_by_user AS
    SELECT world_id, user_id, updated_at
    FROM universe.world_last_launch
    WHERE user_id IS NOT NULL AND world_id IS NOT NULL AND updated_at IS NOT NULL
    PRIMARY KEY ((user_id), updated_at, world_id)
    WITH CLUSTERING ORDER BY (updated_at DESC, world_id ASC);

-- indexing by world_id and order by updated_at (DESC) for time-based queries
CREATE MATERIALIZED VIEW universe.world_last_launch_by_world AS
    SELECT world_id, user_id, updated_at
    FROM universe.world_last_launch
    WHERE world_id IS NOT NULL AND user_id IS NOT NULL AND updated_at IS NOT NULL
    PRIMARY KEY ((world_id), updated_at, user_id)
    WITH CLUSTERING ORDER BY (updated_at DESC, user_id ASC);

-- ----------------------
-- OPERATIONS
-- ----------------------

-- [UPSERT] record the last time a world has been launched by a user
-- [COUNT]  launches for world (world_id)
-- [LIST]   launches for user (user_id)
-- [LIST]   recent launches for user (user_id, <duration>)
-- [GET]    get whether a world has been launched by a user, ever

-- insert/update launches
-- -----------------------------------------
INSERT INTO universe.world_last_launch (world_id, user_id, updated_at) VALUES (00000000-0000-0000-0000-000000000001, 00000000-0000-0000-0000-000000100001, toTimestamp(now()));
INSERT INTO universe.world_last_launch (world_id, user_id, updated_at) VALUES (00000000-0000-0000-0000-000000000001, 00000000-0000-0000-0000-000000100002, toTimestamp(now()));
INSERT INTO universe.world_last_launch (world_id, user_id, updated_at) VALUES (00000000-0000-0000-0000-000000000002, 00000000-0000-0000-0000-000000100001, toTimestamp(now()));
INSERT INTO universe.world_last_launch (world_id, user_id, updated_at) VALUES (00000000-0000-0000-0000-000000000002, 00000000-0000-0000-0000-000000100002, toTimestamp(now()));

-- launch for world and user
-- -----------------------------------------
-- whether a user ever launched a world
SELECT COUNT(*) FROM universe.world_last_launch WHERE world_id = 00000000-0000-0000-0000-000000000001 AND user_id = 00000000-0000-0000-0000-000000100001;

-- launches by world
-- -----------------------------------------
-- all launches
SELECT COUNT(*) FROM universe.world_last_launch WHERE world_id = 00000000-0000-0000-0000-000000000001;
SELECT user_id, updated_at FROM universe.world_last_launch_by_world WHERE world_id = 00000000-0000-0000-0000-000000000002; -- ordered by default (updated_at DESC)
-- most recent launches
SELECT user_id, updated_at FROM universe.world_last_launch_by_world WHERE world_id = 00000000-0000-0000-0000-000000000002 LIMIT 10; -- ordered by default (updated_at DESC)
-- launches by time
SELECT COUNT(*) FROM universe.world_last_launch_by_world WHERE world_id = 00000000-0000-0000-0000-000000000002 AND updated_at >= '2024-01-01';
SELECT user_id, updated_at FROM universe.world_last_launch_by_world WHERE world_id = 00000000-0000-0000-0000-000000000002 AND updated_at >= '2024-01-01'; -- ordered by default (updated_at DESC)

-- launches by user
-- -----------------------------------------
-- all launches
SELECT COUNT(*) FROM universe.world_last_launch_by_user WHERE user_id = 00000000-0000-0000-0000-000000100001;
SELECT world_id, updated_at FROM universe.world_last_launch_by_user WHERE user_id = 00000000-0000-0000-0000-000000100001; -- ordered by default (updated_at DESC)
-- most recent launches
SELECT world_id, updated_at FROM universe.world_last_launch_by_user WHERE user_id = 00000000-0000-0000-0000-000000100001 LIMIT 10; -- ordered by default (updated_at DESC)
-- launches by time
SELECT COUNT(*) FROM universe.world_last_launch_by_user WHERE user_id = 00000000-0000-0000-0000-000000100001 AND updated_at >= '2024-01-01';
SELECT world_id, updated_at FROM universe.world_last_launch_by_user WHERE user_id = 00000000-0000-0000-0000-000000100001 AND updated_at >= '2024-01-01'; -- ordered by default (updated_at DESC)
