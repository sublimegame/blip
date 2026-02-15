-- ----------------------
-- Notes:
-- ----------------------
-- - If the badge record has not been updated recently, we can recompute the rarity and update the record (CRON function or on GET)

-- ----------------------
-- i18n Strategy for Badge Names & Descriptions
-- ----------------------
-- 
-- APPROACH 1 (Current): JSON in TEXT columns
-- Pros: Simple, flexible, all data in one place
-- Cons: Requires JSON parsing in application, harder to query specific languages
-- Usage: Store {"en": "English", "fr": "Français"} in name/description columns
-- 
-- APPROACH 2: Separate translation table
-- CREATE TABLE universe.badge_translation (
--     badge_id UUID,
--     language_code TEXT,  -- 'en', 'fr', 'es', etc.
--     name TEXT,
--     description TEXT,
--     PRIMARY KEY ((badge_id), language_code)
-- );
-- Pros: Clean separation, easier to query specific languages, scalable
-- Cons: Additional table, more complex queries for multiple languages
-- 
-- APPROACH 3: Multiple columns per language
-- ALTER TABLE universe.badge ADD (name_en TEXT, name_fr TEXT, description_en TEXT, description_fr TEXT);
-- Pros: Simple queries, good performance
-- Cons: Schema changes needed for new languages, many NULL columns
-- 
-- RECOMMENDATION: Use Approach 1 for MVP, migrate to Approach 2 if i18n becomes complex

-- ----------------------
-- QUERIES
-- ----------------------

-- READ

-- - Get detail for a given badge
-- ----------------------------------------
-- ARGUMENTS: 
-- - (badge_uuid, [user_uuid])
-- - (world_uuid, badge_tag, [user_uuid])
-- OUTPUT:
-- - badge information
-- - count of users who unlocked the badge
-- - whether user unlocked the badge (if user_uuid is provided)
-- ----------------------------------------

-- - List all badges for a given world (world_uuid)                              ⚠️ WHAT SORTING?
-- - List all badges unlocked by a given user in a world (world_uuid, user_uuid) ⚠️ WHAT SORTING?

-- WRITE
-- - Create a new badge (image, [tag], [name], [description])
-- - Update a badge (badge_uuid, [image], [tag], [name], [description], [rarity])
-- - Unlock a badge for a given user (user_uuid, world_uuid, badge_tag)
-- - [LATER] Delete a badge (badge_uuid)

-- ----------------------
-- Definition of a badge
-- ----------------------

-- DEV: for local testing
-- DROP KEYSPACE IF EXISTS universe;
-- CREATE KEYSPACE universe 
-- WITH replication = {'class': 'NetworkTopologyStrategy', 'replication_factor' : 1}
-- AND durable_writes = true 
-- AND tablets = {'enabled': false};

-- DROP MATERIALIZED VIEW IF EXISTS universe.badge_by_world;
-- DROP TABLE IF EXISTS universe.badge;

CREATE TABLE universe.badge (
    badge_id UUID,           -- unique identifier for the badge (generated)
    world_id UUID,           -- world identifier (foreign key)
    tag TEXT,                -- identifier choosen by the user (unique for the world)
    name TEXT,     
    description TEXT,
    hidden_if_locked BOOLEAN,  -- whether the badge description is hidden until the user unlocks it
    rarity FLOAT,              -- percentage (computed value)
    created_at TIMESTAMP,
    updated_at TIMESTAMP,
    PRIMARY KEY ((world_id), tag)
);

-- Note: primary key cannot have more than 1 columns that's not part of the base table's primary key
CREATE MATERIALIZED VIEW universe.badge_by_id AS
    SELECT * FROM universe.badge
    WHERE badge_id IS NOT NULL AND world_id IS NOT NULL AND tag IS NOT NULL
    PRIMARY KEY ((badge_id), world_id, tag);

-- ----------------------
-- Definition of a badge unlock event
-- ----------------------

-- DROP MATERIALIZED VIEW IF EXISTS universe.user_badge_by_id;
-- DROP TABLE IF EXISTS universe.user_badge;

CREATE TABLE universe.user_badge (
    user_id UUID,
    badge_id UUID,
    created_at TIMESTAMP,
    PRIMARY KEY ((user_id), badge_id)
);

-- Single MV for badge queries
CREATE MATERIALIZED VIEW universe.user_badge_by_id AS
    SELECT * FROM universe.user_badge
    WHERE user_id IS NOT NULL AND badge_id IS NOT NULL
    PRIMARY KEY ((badge_id), user_id);

-- ----------------------
-- Queries
-- ----------------------

-- Create a new badge
-- ⚠️ tag must be unique for the world
INSERT INTO universe.badge (badge_id, world_id, tag, name, description, hidden_if_locked, created_at, updated_at) VALUES (00000000-0000-0000-0000-000000000001,00000000-0000-0000-0000-000000000101,'first_steps','My first steps','Complete your first action',false,toTimestamp(now()),toTimestamp(now()));
INSERT INTO universe.badge (badge_id, world_id, tag, name, description, hidden_if_locked, created_at, updated_at) VALUES (00000000-0000-0000-0000-000000000002,00000000-0000-0000-0000-000000000101,'second_level','Second level','Complete the 2nd level',false,toTimestamp(now()),toTimestamp(now()));

-- Get badge information
-- by badge_id
SELECT * FROM universe.badge_by_id WHERE badge_id = 00000000-0000-0000-0000-000000000001;
-- by world_id + tag
SELECT * FROM universe.badge WHERE world_id = 00000000-0000-0000-0000-000000000101 AND tag = 'first_steps';
SELECT * FROM universe.badge WHERE world_id = 00000000-0000-0000-0000-000000000101 AND tag = 'second_level';

-- List all badges of a world
SELECT * FROM universe.badge WHERE world_id = 00000000-0000-0000-0000-000000000101;

-- Unlock a badge for a given user

-- upsert
INSERT INTO universe.user_badge (user_id, badge_id, created_at) VALUES (00000000-0000-0000-0000-000000000001, 00000000-0000-0000-0000-000000000101, toTimestamp(now()));
INSERT INTO universe.user_badge (user_id, badge_id, created_at) VALUES (00000000-0000-0000-0000-000000000001, 00000000-0000-0000-0000-000000000202, toTimestamp(now()));
INSERT INTO universe.user_badge (user_id, badge_id, created_at) VALUES (00000000-0000-0000-0000-000000000002, 00000000-0000-0000-0000-000000000101, toTimestamp(now()));

-- Check if a user unlocked a badge
SELECT * FROM universe.user_badge WHERE user_id = 00000000-0000-0000-0000-000000000001 AND badge_id = 00000000-0000-0000-0000-000000000101;

-- List all badges unlocked by a user
SELECT * FROM universe.user_badge WHERE user_id = 00000000-0000-0000-0000-000000000001;

-- Count the number of times a badge has been unlocked
SELECT COUNT(*) FROM universe.user_badge_by_id WHERE badge_id = 00000000-0000-0000-0000-000000000001;
