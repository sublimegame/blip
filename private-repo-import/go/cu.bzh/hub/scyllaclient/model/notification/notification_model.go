package notification

import (
	"github.com/scylladb/gocqlx/v3/table"
)

// ----------------------------------------------
// Table : universe.user_notification
// ----------------------------------------------

const (
	TABLE_UNIVERSE_USERNOTIFICATION          = "user_notification"
	FIELD_UNIVERSE_USERNOTIFICATION_UserID   = "user_id"
	FIELD_UNIVERSE_USERNOTIFICATION_TimeUUID = "time_uuid"
	FIELD_UNIVERSE_USERNOTIFICATION_Category = "category"
	FIELD_UNIVERSE_USERNOTIFICATION_Read     = "read"
	FIELD_UNIVERSE_USERNOTIFICATION_Content  = "content"
)

// metadata specifies table name and columns it must be in sync with schema.
var universeUserNotificationTableMetadata = table.Metadata{
	Name: TABLE_UNIVERSE_USERNOTIFICATION,
	Columns: []string{
		FIELD_UNIVERSE_USERNOTIFICATION_UserID,
		FIELD_UNIVERSE_USERNOTIFICATION_TimeUUID,
		FIELD_UNIVERSE_USERNOTIFICATION_Category,
		FIELD_UNIVERSE_USERNOTIFICATION_Read,
		FIELD_UNIVERSE_USERNOTIFICATION_Content,
	},
	PartKey: []string{FIELD_UNIVERSE_USERNOTIFICATION_UserID},
	SortKey: []string{FIELD_UNIVERSE_USERNOTIFICATION_TimeUUID},
}

var UniverseUserNotificationTable = table.New(universeUserNotificationTableMetadata)

type UniverseUserNotificationRecord struct {
	UserID   string
	TimeUUID string
	Category string
	Read     bool
	Content  string
}

func (prr UniverseUserNotificationRecord) Unpack() []interface{} {
	return []interface{}{
		&prr.UserID,
		&prr.TimeUUID,
		&prr.Category,
		&prr.Read,
		&prr.Content,
	}
}
