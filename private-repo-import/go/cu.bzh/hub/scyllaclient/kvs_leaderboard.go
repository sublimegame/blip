package scyllaclient

import (
	"errors"
	"math"
	"sort"
	"time"

	"cu.bzh/hub/scyllaclient/model/kvs"
	"github.com/gocql/gocql"
	"github.com/scylladb/gocqlx/v3/qb"
)

// ----------------------------
// Write
// ----------------------------

type LeaderboardRecord struct {
	WorldID         string
	LeaderboardName string
	UserID          string
	Score           int64
	Value           []byte
	Updated         time.Time
}

func NewLeaderboardRecord(worldID, leaderboardName, userID string, score int64, value []byte) LeaderboardRecord {
	return LeaderboardRecord{
		WorldID:         worldID,
		LeaderboardName: leaderboardName,
		UserID:          userID,
		Score:           score,
		Value:           value,
		Updated:         time.Now(),
	}
}

func (r LeaderboardRecord) IsValid() bool {
	isValid := r.WorldID != "" && r.LeaderboardName != "" && r.UserID != ""
	return isValid
}

type SetLeaderboardScoreOpts struct {
	/// If true, the score will be inserted or updated regardless of the existence or value the current score.
	ForceWrite bool
}

func (c *Client) InsertOrUpdateLeaderboardScore(record LeaderboardRecord, opts SetLeaderboardScoreOpts) (err error) {
	err = c.ensureClientKeyspace(keyspaceKvs)
	if err != nil {
		return
	}

	if !record.IsValid() {
		err = errors.New("record is not valid")
		return
	}

	// Convert time
	var updatedMs int64 = TimeToInt64Ms(record.Updated)

	// Construct data map
	dataMap := qb.M{
		kvs.FIELD_KVS_LEADERBOARD_WorldID:         record.WorldID,
		kvs.FIELD_KVS_LEADERBOARD_LeaderboardName: record.LeaderboardName,
		kvs.FIELD_KVS_LEADERBOARD_UserID:          record.UserID,
		kvs.FIELD_KVS_LEADERBOARD_Score:           record.Score,
		kvs.FIELD_KVS_LEADERBOARD_Value:           record.Value,
		kvs.FIELD_KVS_LEADERBOARD_Updated:         updatedMs,
	}

	if opts.ForceWrite {
		// Insert the record (no matter if it already exists or not)
		stmts, names := kvs.KvsLeaderboardTable.InsertBuilder().ToCql()
		q := c.session.Query(stmts, names)
		q = q.BindMap(dataMap)
		err = q.ExecRelease()
		return
	}

	// Override is false

	var applied bool

	// Try inserting the record if it does not exist
	{
		stmts, names := kvs.KvsLeaderboardTable.InsertBuilder().Unique().ToCql()
		q := c.session.Query(stmts, names)
		q = q.BindMap(dataMap)
		applied, err = q.ExecCASRelease()
	}

	// If insert failed, then try to update
	if err != nil || !applied {
		builder := kvs.KvsLeaderboardTable.UpdateBuilder(kvs.FIELD_KVS_LEADERBOARD_Score, kvs.FIELD_KVS_LEADERBOARD_Value, kvs.FIELD_KVS_LEADERBOARD_Updated)
		builder = builder.If(qb.Lt(kvs.FIELD_KVS_LEADERBOARD_Score))
		stmts, names := builder.ToCql()
		q := c.session.Query(stmts, names)
		q = q.BindMap(dataMap)
		applied, err = q.ExecCASRelease()
	}

	return
}

type GetLeaderboardScoreOpts struct {
	Limit uint
}

// Get all scores for a leaderboard
func (c *Client) GetLeaderboardScores(worldID, leaderboardName string, opts GetLeaderboardScoreOpts) (records []kvs.KvsLeaderboardRecord, err error) {
	err = c.ensureClientKeyspace(keyspaceKvs)
	if err != nil {
		return
	}

	builder := kvs.KvsLeaderboardByScoreTable.SelectBuilder()
	if opts.Limit > 0 {
		builder = builder.Limit(opts.Limit)
	}
	stmts, names := builder.ToCql()
	q := c.session.Query(stmts, names)
	q = q.BindMap(qb.M{
		kvs.FIELD_KVS_LEADERBOARD_WorldID:         worldID,
		kvs.FIELD_KVS_LEADERBOARD_LeaderboardName: leaderboardName,
	})
	iter := q.Iter()
	defer iter.Close()

	for {
		var record kvs.KvsLeaderboardRecord
		if !iter.StructScan(&record) {
			break
		}
		records = append(records, record)
	}

	return
}

// Get all scores for a leaderboard
func (c *Client) GetLeaderboardScoresAround(worldID, leaderboardName string, score int64, limit uint) (records kvs.KvsLeaderboardRecordArray, err error) {
	err = c.ensureClientKeyspace(keyspaceKvs)
	if err != nil {
		return
	}

	records = make([]kvs.KvsLeaderboardRecord, 0)

	// select scores that are inferior or equal to the given score
	{
		builder := kvs.KvsLeaderboardByScoreTable.SelectBuilder()
		builder = builder.Where(qb.LtOrEq(kvs.FIELD_KVS_LEADERBOARD_Score))
		if limit > 0 {
			builder = builder.Limit(uint(math.Ceil(float64(limit) * 0.5)))
		}
		stmts, names := builder.ToCql()
		q := c.session.Query(stmts, names)
		q = q.BindMap(qb.M{
			kvs.FIELD_KVS_LEADERBOARD_WorldID:         worldID,
			kvs.FIELD_KVS_LEADERBOARD_LeaderboardName: leaderboardName,
			kvs.FIELD_KVS_LEADERBOARD_Score:           score,
		})
		iter := q.Iter()
		defer iter.Close()

		for {
			var record kvs.KvsLeaderboardRecord
			if !iter.StructScan(&record) {
				break
			}
			records = append(records, record)
		}
	}

	// select scores that are superior or equal to the given score
	{
		builder := kvs.KvsLeaderboardByScoreTable.SelectBuilder()
		builder = builder.Where(qb.GtOrEq(kvs.FIELD_KVS_LEADERBOARD_Score))
		builder = builder.OrderBy(kvs.FIELD_KVS_LEADERBOARD_Score, qb.ASC)
		if limit > 0 {
			builder = builder.Limit(uint(math.Ceil(float64(limit) * 0.5)))
		}
		stmts, names := builder.ToCql()
		q := c.session.Query(stmts, names)
		q = q.BindMap(qb.M{
			kvs.FIELD_KVS_LEADERBOARD_WorldID:         worldID,
			kvs.FIELD_KVS_LEADERBOARD_LeaderboardName: leaderboardName,
			kvs.FIELD_KVS_LEADERBOARD_Score:           score,
		})
		iter := q.Iter()
		defer iter.Close()

		for {
			var record kvs.KvsLeaderboardRecord
			if !iter.StructScan(&record) {
				break
			}
			records = append(records, record)
		}
	}

	// Remove duplicates
	records = removeDuplicatesByScore(records, score)

	// Sort records by score
	sort.Sort(records)

	return
}

// Function to remove duplicates by Score
func removeDuplicatesByScore(records []kvs.KvsLeaderboardRecord, score int64) []kvs.KvsLeaderboardRecord {
	var uniqueRecords []kvs.KvsLeaderboardRecord

	var found bool = false
	for _, record := range records {
		if record.Score == score {
			found = false
			for _, uniqueRecord := range uniqueRecords {
				if uniqueRecord.Equals(record) {
					found = true
					break
				}
			}
			if !found {
				uniqueRecords = append(uniqueRecords, record)
			}
		} else {
			uniqueRecords = append(uniqueRecords, record)
		}
	}

	return uniqueRecords
}

// Get allscores for a leaderboard and a set of users
func (c *Client) GetLeaderboardScoresForUsers(worldID, leaderboardName string, userIDs []string) ([]kvs.KvsLeaderboardRecord, error) {
	var err error
	records := make([]kvs.KvsLeaderboardRecord, 0)

	err = c.ensureClientKeyspace(keyspaceKvs)
	if err != nil {
		return records, err
	}

	// construct query
	builder := kvs.KvsLeaderboardTable.SelectBuilder()
	builder = builder.Where(qb.In(kvs.FIELD_KVS_LEADERBOARD_UserID))
	stmts, names := builder.ToCql()
	q := c.session.Query(stmts, names)

	// query scores, 100 at a time (scyllaDB doesn't allow more than 100 values in IN clause)
	const CHUNK_SIZE int = 100

	for i := 0; i < len(userIDs); i += CHUNK_SIZE {
		// Determine the end index of the chunk (handle if the last chunk has less than 100 elements)
		end := i + CHUNK_SIZE
		if end > len(userIDs) {
			end = len(userIDs)
		}

		// Slice the chunk
		chunk := userIDs[i:end]

		// Process the chunk
		q = q.BindMap(qb.M{
			kvs.FIELD_KVS_LEADERBOARD_WorldID:         worldID,
			kvs.FIELD_KVS_LEADERBOARD_LeaderboardName: leaderboardName,
			kvs.FIELD_KVS_LEADERBOARD_UserID:          chunk,
		})
		iter := q.Iter()

		for {
			var record kvs.KvsLeaderboardRecord
			if !iter.StructScan(&record) {
				break
			}
			records = append(records, record)
		}

		iter.Close()
	}

	q.Release()

	return records, nil
}

// Get allscores for a leaderboard and a user
func (c *Client) GetLeaderboardScoreForUser(worldID, leaderboardName, userID string) (record *kvs.KvsLeaderboardRecord, err error) {
	err = c.ensureClientKeyspace(keyspaceKvs)
	if err != nil {
		return
	}

	builder := kvs.KvsLeaderboardTable.SelectBuilder()
	builder = builder.Where(qb.Eq(kvs.FIELD_KVS_LEADERBOARD_UserID))
	stmts, names := builder.ToCql()
	q := c.session.Query(stmts, names)
	q = q.BindMap(qb.M{
		kvs.FIELD_KVS_LEADERBOARD_WorldID:         worldID,
		kvs.FIELD_KVS_LEADERBOARD_LeaderboardName: leaderboardName,
		kvs.FIELD_KVS_LEADERBOARD_UserID:          userID,
	})
	record = &kvs.KvsLeaderboardRecord{}
	err = q.Get(record)

	if err == gocql.ErrNotFound {
		// no record found
		err = nil
		record = nil
	}

	return
}
