package kvs

import (
	"bytes"

	"github.com/scylladb/gocqlx/v3/table"
)

// ----------------------------------------------
// Table : kvs.leaderboard
// ----------------------------------------------

const (
	TABLE_KVS_LEADERBOARD                 = "leaderboard"
	FIELD_KVS_LEADERBOARD_WorldID         = "world_id"
	FIELD_KVS_LEADERBOARD_LeaderboardName = "leaderboard_name"
	FIELD_KVS_LEADERBOARD_UserID          = "user_id"
	FIELD_KVS_LEADERBOARD_Score           = "score"
	FIELD_KVS_LEADERBOARD_Value           = "value"
	FIELD_KVS_LEADERBOARD_Updated         = "updated"
)

// metadata specifies table name and columns it must be in sync with schema.
var kvsLeaderboardTableMetadata = table.Metadata{
	Name: TABLE_KVS_LEADERBOARD,
	Columns: []string{
		FIELD_KVS_LEADERBOARD_WorldID,
		FIELD_KVS_LEADERBOARD_LeaderboardName,
		FIELD_KVS_LEADERBOARD_UserID,
		FIELD_KVS_LEADERBOARD_Score,
		FIELD_KVS_LEADERBOARD_Value,
		FIELD_KVS_LEADERBOARD_Updated,
	},
	PartKey: []string{FIELD_KVS_LEADERBOARD_WorldID, FIELD_KVS_LEADERBOARD_LeaderboardName},
	SortKey: []string{FIELD_KVS_LEADERBOARD_UserID},
}

var KvsLeaderboardTable = table.New(kvsLeaderboardTableMetadata)

type KvsLeaderboardRecord struct {
	WorldID         string
	LeaderboardName string
	UserID          string
	Score           int64
	Value           []byte
	Updated         int64
}

// Equals method to compare two KvsLeaderboardRecord structs
func (r KvsLeaderboardRecord) Equals(other KvsLeaderboardRecord) bool {
	// Compare all fields
	return r.WorldID == other.WorldID &&
		r.LeaderboardName == other.LeaderboardName &&
		r.UserID == other.UserID &&
		r.Score == other.Score &&
		bytes.Equal(r.Value, other.Value) && // Use bytes.Equal for comparing []byte slices
		r.Updated == other.Updated
}

type KvsLeaderboardRecordArray []KvsLeaderboardRecord

// Len method (needed for sort.Interface)
func (a KvsLeaderboardRecordArray) Len() int {
	return len(a)
}

// Less method (needed for sort.Interface)
func (a KvsLeaderboardRecordArray) Less(i, j int) bool {
	// Compare Score first
	if a[i].Score != a[j].Score {
		return a[i].Score > a[j].Score
	}
	// If Score is equal, compare Updated (most recent is less)
	return a[i].Updated < a[j].Updated
}

// Swap method (needed for sort.Interface)
func (a KvsLeaderboardRecordArray) Swap(i, j int) {
	a[i], a[j] = a[j], a[i]
}

func (klr KvsLeaderboardRecord) IsValid() bool {
	return klr.WorldID != "" && klr.LeaderboardName != "" && klr.UserID != ""
}

func (klr KvsLeaderboardRecord) Unpack() []interface{} {
	return []interface{}{
		&klr.WorldID,
		&klr.LeaderboardName,
		&klr.UserID,
		&klr.Score,
		&klr.Value,
		&klr.Updated,
	}
}

// ----------------------------------------------
// Table : kvs.leaderboard_by_score
// ----------------------------------------------

const (
	TABLE_KVS_LEADERBOARD_BY_SCORE = "leaderboard_by_score"
)

// metadata specifies table name and columns it must be in sync with schema.
var kvsLeaderboardByScoreTableMetadata = table.Metadata{
	Name: TABLE_KVS_LEADERBOARD_BY_SCORE,
	Columns: []string{
		FIELD_KVS_LEADERBOARD_WorldID,
		FIELD_KVS_LEADERBOARD_LeaderboardName,
		FIELD_KVS_LEADERBOARD_UserID,
		FIELD_KVS_LEADERBOARD_Score,
		FIELD_KVS_LEADERBOARD_Value,
		FIELD_KVS_LEADERBOARD_Updated,
	},
	PartKey: []string{FIELD_KVS_LEADERBOARD_WorldID, FIELD_KVS_LEADERBOARD_LeaderboardName},
	SortKey: []string{FIELD_KVS_LEADERBOARD_Score, FIELD_KVS_LEADERBOARD_UserID},
}

var KvsLeaderboardByScoreTable = table.New(kvsLeaderboardByScoreTableMetadata)
