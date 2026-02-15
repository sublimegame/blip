package universe

import (
	"time"

	"github.com/scylladb/gocqlx/v3/table"
)

// ----------------------------------------------
// Type - Badge
// ----------------------------------------------

type UserBadgeRecord struct {
	UserID    string
	BadgeID   string
	CreatedAt time.Time
}

func (prr UserBadgeRecord) Unpack() []interface{} {
	return []interface{}{
		&prr.UserID,
		&prr.BadgeID,
		&prr.CreatedAt,
	}
}

// ----------------------------------------------
// Table - universe.badge
// ----------------------------------------------

const (
	TABLE_UNIVERSE_USER_BADGE           = "user_badge"
	FIELD_UNIVERSE_USER_BADGE_UserID    = "user_id"
	FIELD_UNIVERSE_USER_BADGE_BadgeID   = "badge_id"
	FIELD_UNIVERSE_USER_BADGE_CreatedAt = "created_at"
)

// metadata specifies table name and columns it must be in sync with schema.
var userBadgeTableMetadata = table.Metadata{
	Name: TABLE_UNIVERSE_USER_BADGE,
	Columns: []string{
		FIELD_UNIVERSE_USER_BADGE_UserID,
		FIELD_UNIVERSE_USER_BADGE_BadgeID,
		FIELD_UNIVERSE_USER_BADGE_CreatedAt,
	},
	PartKey: []string{FIELD_UNIVERSE_USER_BADGE_UserID},
	SortKey: []string{FIELD_UNIVERSE_USER_BADGE_BadgeID},
}

var UserBadgeTable = table.New(userBadgeTableMetadata)

// ----------------------------------------------
// Table - universe.badge_by_id
// ----------------------------------------------

const (
	TABLE_UNIVERSE_USER_BADGE_BY_ID = "user_badge_by_id"
)

var userBadgeByIDTableMetadata = table.Metadata{
	Name: TABLE_UNIVERSE_USER_BADGE_BY_ID,
	Columns: []string{
		FIELD_UNIVERSE_USER_BADGE_UserID,
		FIELD_UNIVERSE_USER_BADGE_BadgeID,
		FIELD_UNIVERSE_USER_BADGE_CreatedAt,
	},
	PartKey: []string{FIELD_UNIVERSE_USER_BADGE_BadgeID},
	SortKey: []string{FIELD_UNIVERSE_USER_BADGE_UserID},
}

var UserBadgeByIDTable = table.New(userBadgeByIDTableMetadata)
