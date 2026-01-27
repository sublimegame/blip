DROP KEYSPACE IF EXISTS universe;
CREATE KEYSPACE universe WITH replication = {'class' : 'NetworkTopologyStrategy', 'replication_factor' : 1};

CREATE TABLE universe.user_notification (
    user_id UUID,
    time_uuid TIMEUUID,   -- this guarantees uniqueness and preserves time ordering
    category TEXT,
    read BOOLEAN,
    content TEXT,
    PRIMARY KEY ((user_id), time_uuid)
) WITH CLUSTERING ORDER BY (time_uuid DESC);

-- index by read status
CREATE INDEX ON universe.user_notification((user_id), read);
CREATE INDEX ON universe.user_notification((user_id), category);

-- QUERIES
-- ✅ 1. Get all notifications for a user (sorted by created_at, most recent first)
-- ✅ 2. Get all unread notifications for a user (sorted by created_at, most recent first)
-- ✅ 3. Count all unread notifications for a user (live)
-- ✅ 4. Mark a notification as read
-- ✅ 5. Mark a notification as unread
-- ✅ 6. Mark all notifications as read
-- ✅ 7. Mark all notification of a given category as read
-- 8. Caching the count of unread notifications for a user

-- ----------------------------
-- TEST QUERIES
-- ----------------------------

INSERT INTO universe.user_notification (user_id, time_uuid, category, read, content) VALUES (00000000-0000-0000-0000-000000000001, now(), 'friends', false, '{"type":"test1"}');
INSERT INTO universe.user_notification (user_id, time_uuid, category, read, content) VALUES (00000000-0000-0000-0000-000000000001, now(), 'grant', true, '{"type":"test2"}');
INSERT INTO universe.user_notification (user_id, time_uuid, category, read, content) VALUES (00000000-0000-0000-0000-000000000001, now(), 'friends', false, '{"type":"test3"}');
INSERT INTO universe.user_notification (user_id, time_uuid, category, read, content) VALUES (00000000-0000-0000-0000-000000000001, now(), 'news', true, '{"type":"test4"}');
INSERT INTO universe.user_notification (user_id, time_uuid, category, read, content) VALUES (00000000-0000-0000-0000-000000000001, now(), 'grant', false, '{"type":"test5"}');
INSERT INTO universe.user_notification (user_id, time_uuid, category, read, content) VALUES (00000000-0000-0000-0000-000000000001, now(), 'friends', true, '{"type":"test6"}');

-- Get all users having notifications
SELECT user_id FROM universe.user_notification;

-- Get notifications for a user
SELECT * FROM universe.user_notification WHERE user_id = 00000000-0000-0000-0000-000000000001;
SELECT * FROM universe.user_notification WHERE user_id = 00000000-0000-0000-0000-000000000001 AND read = true;
SELECT * FROM universe.user_notification WHERE user_id = 00000000-0000-0000-0000-000000000001 AND read = false;

-- Count notifications for a user
SELECT count(*) FROM universe.user_notification WHERE user_id = 00000000-0000-0000-0000-000000000001;
SELECT count(*) FROM universe.user_notification WHERE user_id = 00000000-0000-0000-0000-000000000001 AND read = true;
SELECT count(*) FROM universe.user_notification WHERE user_id = 00000000-0000-0000-0000-000000000001 AND read = false;

-- Mark a notification as read
UPDATE universe.user_notification SET read = true WHERE user_id = 00000000-0000-0000-0000-000000000001 AND time_uuid = 1f7c6081-7f54-11ef-bb7b-c6d76e794523;

-- Mark a notification as unread
UPDATE universe.user_notification SET read = false WHERE user_id = 00000000-0000-0000-0000-000000000001 AND time_uuid = 1f7c6081-7f54-11ef-bb7b-c6d76e794523;

-- Mark all notifications as read for a user
SELECT * FROM universe.user_notification WHERE user_id = 00000000-0000-0000-0000-000000000001 AND read = false;
-- FOR EACH ROW
UPDATE universe.user_notification SET read = true WHERE user_id = <USER_ID> AND time_uuid = <TIME_UUID>

-- Mark all notification of a given category as read
SELECT * FROM universe.user_notification WHERE user_id = 00000000-0000-0000-0000-000000000001 AND category = 'friends'; -- AND read = false;
-- FOR EACH ROW
UPDATE universe.user_notification SET read = true WHERE user_id = <USER_ID> AND time_uuid = <TIME_UUID>
