package universe

import (
	"time"

	"github.com/scylladb/gocqlx/v3/table"
)

// ----------------------------------------------
// Type - Badge
// ----------------------------------------------

type BadgeRecord struct {
	BadgeID        string
	WorldID        string
	Tag            string
	Name           string
	Description    string
	HiddenIfLocked bool
	Rarity         float32
	CreatedAt      time.Time
	UpdatedAt      time.Time
}

func (prr BadgeRecord) Unpack() []interface{} {
	return []interface{}{
		&prr.BadgeID,
		&prr.WorldID,
		&prr.Tag,
		&prr.Name,
		&prr.Description,
		&prr.HiddenIfLocked,
		&prr.Rarity,
		&prr.CreatedAt,
		&prr.UpdatedAt,
	}
}

// ----------------------------------------------
// Table - universe.badge
// ----------------------------------------------

const (
	TABLE_UNIVERSE_BADGE                = "badge"
	FIELD_UNIVERSE_BADGE_BadgeID        = "badge_id"
	FIELD_UNIVERSE_BADGE_WorldID        = "world_id"
	FIELD_UNIVERSE_BADGE_Tag            = "tag"
	FIELD_UNIVERSE_BADGE_Name           = "name"
	FIELD_UNIVERSE_BADGE_Description    = "description"
	FIELD_UNIVERSE_BADGE_HiddenIfLocked = "hidden_if_locked"
	FIELD_UNIVERSE_BADGE_Rarity         = "rarity"
	FIELD_UNIVERSE_BADGE_CreatedAt      = "created_at"
	FIELD_UNIVERSE_BADGE_UpdatedAt      = "updated_at"
)

// metadata specifies table name and columns it must be in sync with schema.
var badgeTableMetadata = table.Metadata{
	Name: TABLE_UNIVERSE_BADGE,
	Columns: []string{
		FIELD_UNIVERSE_BADGE_BadgeID,
		FIELD_UNIVERSE_BADGE_WorldID,
		FIELD_UNIVERSE_BADGE_Tag,
		FIELD_UNIVERSE_BADGE_Name,
		FIELD_UNIVERSE_BADGE_Description,
		FIELD_UNIVERSE_BADGE_HiddenIfLocked,
		FIELD_UNIVERSE_BADGE_Rarity,
		FIELD_UNIVERSE_BADGE_CreatedAt,
		FIELD_UNIVERSE_BADGE_UpdatedAt,
	},
	PartKey: []string{FIELD_UNIVERSE_BADGE_WorldID},
	SortKey: []string{FIELD_UNIVERSE_BADGE_Tag},
}

var BadgeTable = table.New(badgeTableMetadata)

// ----------------------------------------------
// Table - universe.badge_by_id
// ----------------------------------------------

const (
	TABLE_UNIVERSE_BADGE_BY_ID = "badge_by_id"
)

var badgeByIDTableMetadata = table.Metadata{
	Name: TABLE_UNIVERSE_BADGE_BY_ID,
	Columns: []string{
		FIELD_UNIVERSE_BADGE_BadgeID,
		FIELD_UNIVERSE_BADGE_WorldID,
		FIELD_UNIVERSE_BADGE_Tag,
		FIELD_UNIVERSE_BADGE_Name,
		FIELD_UNIVERSE_BADGE_Description,
		FIELD_UNIVERSE_BADGE_HiddenIfLocked,
		FIELD_UNIVERSE_BADGE_Rarity,
		FIELD_UNIVERSE_BADGE_CreatedAt,
		FIELD_UNIVERSE_BADGE_UpdatedAt,
	},
	PartKey: []string{FIELD_UNIVERSE_BADGE_BadgeID},
	SortKey: []string{FIELD_UNIVERSE_BADGE_WorldID, FIELD_UNIVERSE_BADGE_Tag},
}

var BadgeByIDTable = table.New(badgeByIDTableMetadata)
