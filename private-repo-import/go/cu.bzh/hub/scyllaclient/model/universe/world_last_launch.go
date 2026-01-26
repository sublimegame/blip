package universe

import (
	"time"

	"github.com/scylladb/gocqlx/v3/table"
)

// ----------------------------------------------
// Type - WorldLastLaunch
// ----------------------------------------------

type WorldLastLaunchRecord struct {
	WorldID   string
	UserID    string
	UpdatedAt time.Time
}

func (prr WorldLastLaunchRecord) Unpack() []interface{} {
	return []interface{}{
		&prr.WorldID,
		&prr.UserID,
		&prr.UpdatedAt,
	}
}

// ----------------------------------------------
// Table - universe.world_last_launch
// ----------------------------------------------

const (
	TABLE_UNIVERSE_WORLD_LAST_LAUNCH           = "world_last_launch"
	FIELD_UNIVERSE_WORLD_LAST_LAUNCH_WorldID   = "world_id"
	FIELD_UNIVERSE_WORLD_LAST_LAUNCH_UserID    = "user_id"
	FIELD_UNIVERSE_WORLD_LAST_LAUNCH_UpdatedAt = "updated_at"
)

// metadata specifies table name and columns it must be in sync with schema.
var worldLastLaunchTableMetadata = table.Metadata{
	Name: TABLE_UNIVERSE_WORLD_LAST_LAUNCH,
	Columns: []string{
		FIELD_UNIVERSE_WORLD_LAST_LAUNCH_WorldID,
		FIELD_UNIVERSE_WORLD_LAST_LAUNCH_UserID,
		FIELD_UNIVERSE_WORLD_LAST_LAUNCH_UpdatedAt,
	},
	PartKey: []string{FIELD_UNIVERSE_WORLD_LAST_LAUNCH_WorldID},
	SortKey: []string{FIELD_UNIVERSE_WORLD_LAST_LAUNCH_UserID},
}

var WorldLastLaunchTable = table.New(worldLastLaunchTableMetadata)

// ----------------------------------------------
// Materialized View - universe.world_last_launch_by_user
// ----------------------------------------------

const (
	VIEW_UNIVERSE_WORLD_LAST_LAUNCH_BY_USER = "world_last_launch_by_user"
)

var worldLastLaunchByUserTableMetadata = table.Metadata{
	Name: VIEW_UNIVERSE_WORLD_LAST_LAUNCH_BY_USER,
	Columns: []string{
		FIELD_UNIVERSE_WORLD_LAST_LAUNCH_WorldID,
		FIELD_UNIVERSE_WORLD_LAST_LAUNCH_UserID,
		FIELD_UNIVERSE_WORLD_LAST_LAUNCH_UpdatedAt,
	},
	PartKey: []string{FIELD_UNIVERSE_WORLD_LAST_LAUNCH_UserID},
	SortKey: []string{FIELD_UNIVERSE_WORLD_LAST_LAUNCH_UpdatedAt, FIELD_UNIVERSE_WORLD_LAST_LAUNCH_WorldID},
}

var WorldLastLaunchByUserTable = table.New(worldLastLaunchByUserTableMetadata)

// ----------------------------------------------
// Materialized View - universe.world_last_launch_by_world
// ----------------------------------------------

const (
	VIEW_UNIVERSE_WORLD_LAST_LAUNCH_BY_WORLD = "world_last_launch_by_world"
)

var worldLastLaunchByWorldTableMetadata = table.Metadata{
	Name: VIEW_UNIVERSE_WORLD_LAST_LAUNCH_BY_WORLD,
	Columns: []string{
		FIELD_UNIVERSE_WORLD_LAST_LAUNCH_WorldID,
		FIELD_UNIVERSE_WORLD_LAST_LAUNCH_UserID,
		FIELD_UNIVERSE_WORLD_LAST_LAUNCH_UpdatedAt,
	},
	PartKey: []string{FIELD_UNIVERSE_WORLD_LAST_LAUNCH_WorldID},
	SortKey: []string{FIELD_UNIVERSE_WORLD_LAST_LAUNCH_UpdatedAt, FIELD_UNIVERSE_WORLD_LAST_LAUNCH_UserID},
}

var WorldLastLaunchByWorldTable = table.New(worldLastLaunchByWorldTableMetadata)
