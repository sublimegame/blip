-- Description: A simple leaderboard system for a game

-- A world can have multiple leaderboards

-- Requests

-- SET top score for [WORLD] [LEADERBOARD] [USER] -> score + value + timestamp
-- GET top scores for a set of users and a [WORLD] [LEADERBOARD] (DESC ordering will be done at the application level)
-- GET top x scores for [WORLD] [LEADERBOARD] (by score DESC)
-- GET top score for [WORLD] [LEADERBOARD] [USER]

DROP KEYSPACE IF EXISTS kvs;
CREATE KEYSPACE kvs WITH replication = {'class' : 'NetworkTopologyStrategy', 'replication_factor' : 1};

CREATE TABLE kvs.leaderboard (
   world_id UUID,
   leaderboard_name TEXT,
   user_id UUID,
   score BIGINT,
   value BLOB,
   updated TIMESTAMP,
   PRIMARY KEY ((world_id, leaderboard_name), user_id)
);

CREATE MATERIALIZED VIEW kvs.leaderboard_by_score AS
SELECT * FROM kvs.leaderboard
WHERE world_id IS NOT NULL
  AND leaderboard_name IS NOT NULL
  AND user_id IS NOT NULL
  AND score IS NOT NULL
PRIMARY KEY ((world_id, leaderboard_name), score, user_id)
WITH CLUSTERING ORDER BY (score DESC);

-- Test data
INSERT INTO kvs.leaderboard (world_id, leaderboard_name, user_id, score, value, updated) VALUES (00000000-0000-0000-0000-000000000001, 'default', 00000000-0000-0000-1111-000000000001, 10, 0x2A, toTimestamp(now()));
INSERT INTO kvs.leaderboard (world_id, leaderboard_name, user_id, score, value, updated) VALUES (00000000-0000-0000-0000-000000000001, 'default', 00000000-0000-0000-1111-000000000002, 20, 0x2A, toTimestamp(now()));
INSERT INTO kvs.leaderboard (world_id, leaderboard_name, user_id, score, value, updated) VALUES (00000000-0000-0000-0000-000000000001, 'default', 00000000-0000-0000-1111-000000000003, 30, 0x2A, toTimestamp(now()));
INSERT INTO kvs.leaderboard (world_id, leaderboard_name, user_id, score, value, updated) VALUES (00000000-0000-0000-0000-000000000001, 'default', 00000000-0000-0000-1111-000000000004, 40, 0x2A, toTimestamp(now()));
INSERT INTO kvs.leaderboard (world_id, leaderboard_name, user_id, score, value, updated) VALUES (00000000-0000-0000-0000-000000000001, 'default', 00000000-0000-0000-1111-000000000005, 50, 0x2A, toTimestamp(now()));
INSERT INTO kvs.leaderboard (world_id, leaderboard_name, user_id, score, value, updated) VALUES (00000000-0000-0000-0000-000000000001, 'default', 00000000-0000-0000-1111-000000000006, 60, 0x2A, toTimestamp(now()));
INSERT INTO kvs.leaderboard (world_id, leaderboard_name, user_id, score, value, updated) VALUES (00000000-0000-0000-0000-000000000001, 'default', 00000000-0000-0000-1111-000000000007, 70, 0x2A, toTimestamp(now()));
INSERT INTO kvs.leaderboard (world_id, leaderboard_name, user_id, score, value, updated) VALUES (00000000-0000-0000-0000-000000000001, 'default', 00000000-0000-0000-1111-000000000008, 80, 0x2A, toTimestamp(now()));
INSERT INTO kvs.leaderboard (world_id, leaderboard_name, user_id, score, value, updated) VALUES (00000000-0000-0000-0000-000000000001, 'default', 00000000-0000-0000-1111-000000000009, 90, 0x2A, toTimestamp(now()));
INSERT INTO kvs.leaderboard (world_id, leaderboard_name, user_id, score, value, updated) VALUES (00000000-0000-0000-0000-000000000001, 'default', 00000000-0000-0000-1111-000000000010, 100, 0x2A, toTimestamp(now()));

SELECT * FROM kvs.leaderboard;
SELECT * FROM kvs.leaderboard_by_score;

-- get 5 around
SELECT * FROM kvs.leaderboard_by_score WHERE world_id = 00000000-0000-0000-0000-000000000001 AND leaderboard_name = 'default' AND score <= 30 LIMIT 3;
SELECT * FROM kvs.leaderboard_by_score WHERE world_id = 00000000-0000-0000-0000-000000000001 AND leaderboard_name = 'default' AND score >= 30 ORDER BY score ASC LIMIT 3;


-- Write a score

-- Force write
--------------------------------------------------------
INSERT INTO kvs.leaderboard (world_id, leaderboard_name, user_id, score, value, updated) VALUES (00000000-0000-0000-0000-000000000001, 'default', 00000000-0000-0000-1111-000000000001, 80, 0x2A, toTimestamp(now()));

-- Conditional write 
-- if no score exists
-- if the new score is higher than the current score
--------------------------------------------------------
-- Try inserting the score (as a new one)
INSERT INTO kvs.leaderboard (world_id, leaderboard_name, user_id, score, value, updated) VALUES (00000000-0000-0000-0000-000000000001, 'default', 00000000-0000-0000-1111-000000000001, 80, 0x2A, toTimestamp(now())) IF NOT EXISTS;
-- If it fails, try updating the score (only if the new score is higher than the current score)
UPDATE kvs.leaderboard SET score = 80, value = 0x2A, updated = toTimestamp(now()) WHERE world_id = 00000000-0000-0000-0000-000000000001 AND leaderboard_name = 'default' AND user_id = 00000000-0000-0000-1111-000000000001 IF score < 80;


-- Get from MV (ordered by score DESC)
SELECT * FROM kvs.leaderboard_by_score LIMIT 10;
SELECT * FROM kvs.leaderboard WHERE world_id = 00000000-0000-0000-0000-000000000001 AND leaderboard_name = 'default' AND user_id IN (00000000-0000-0000-1111-000000000001, 00000000-0000-0000-1111-000000000002);
SELECT * FROM kvs.leaderboard WHERE world_id = 00000000-0000-0000-0000-000000000001 AND leaderboard_name = 'default' AND user_id IN (00000000-0000-0000-1111-000000000001, 00000000-0000-0000-1111-000000000002, 00000000-0000-0000-1111-000000000004);
SELECT * FROM kvs.leaderboard WHERE world_id = 00000000-0000-0000-0000-000000000001 AND leaderboard_name = 'default' AND user_id = 00000000-0000-0000-1111-000000000001;
