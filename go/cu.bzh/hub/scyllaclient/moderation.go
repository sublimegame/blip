package scyllaclient

import (
	"errors"
	"time"

	"github.com/google/uuid"
	"github.com/scylladb/gocqlx/v3/qb"
	"github.com/scylladb/gocqlx/v3/table"
)

const (
	// Name of table moderation.username
	tableModerationUsername = "username"

	// Fields' names (username)
	FIELD_USERNAME    = "username"
	FIELD_APPROPRIATE = "appropriate"
	FIELD_CREATED     = "created"

	// Fields' names (chat_message)
	FIELD_CHATMSG_UUID        = "id"
	FIELD_CHATMSG_SENDER_UUID = "senderID"
	FIELD_CHATMSG_CREATED_AT  = "createdAt"
	FIELD_CHATMSG_UPDATED_AT  = "updatedAt"
	FIELD_CHATMSG_SERVER_ID   = "serverID"
	FIELD_CHATMSG_CONTENT     = "content"
	FIELD_CHATMSG_STATUS      = "status"
	FIELD_CHATMSG_REASON      = "reason"
)

// ------------------------------
// Types
// ------------------------------

// metadata specifies table name and columns it must be in sync with schema.
var moderationUsernameTableMetadata = table.Metadata{
	Name:    tableModerationUsername,
	Columns: []string{FIELD_USERNAME, FIELD_APPROPRIATE, FIELD_CREATED},
	PartKey: []string{FIELD_USERNAME},
	SortKey: []string{FIELD_USERNAME},
}

var moderationUsernameTable = table.New(moderationUsernameTableMetadata)

type ModerationUsernameRecord struct {
	Username    string
	Appropriate bool
	Created     time.Time
}

// Table chat_message

var moderationChatMessageTableMetadata = table.Metadata{
	Name: "chat_message",
	Columns: []string{
		FIELD_CHATMSG_UUID,
		FIELD_CHATMSG_SENDER_UUID,
		FIELD_CHATMSG_CREATED_AT,
		FIELD_CHATMSG_UPDATED_AT,
		FIELD_CHATMSG_SERVER_ID,
		FIELD_CHATMSG_CONTENT,
		FIELD_CHATMSG_STATUS,
		FIELD_CHATMSG_REASON},
	PartKey: []string{FIELD_CHATMSG_UUID},
	SortKey: []string{FIELD_CHATMSG_CREATED_AT},
}

var moderationChatMessageTable = table.New(moderationChatMessageTableMetadata)

type ModerationChatMessageRecord struct {
	UUID      uuid.UUID
	SenderID  uuid.UUID
	CreatedAt time.Time
	UpdatedAt time.Time
	ServerID  string
	Content   string
	Status    int
	Reason    int
}

// ------------------------------
// Functions
// ------------------------------

func (c *Client) GetModerationUsername(username string) (ModerationUsernameRecord, error) {
	err := c.ensureClientKeyspace(keyspaceModeration)
	if err != nil {
		return ModerationUsernameRecord{}, err
	}

	var results []ModerationUsernameRecord

	// no need for where clause, we are querying by primary key
	q := c.session.Query(moderationUsernameTable.SelectBuilder().ToCql())
	q = q.BindMap(qb.M{FIELD_USERNAME: username})
	err = q.SelectRelease(&results)
	if err != nil {
		return ModerationUsernameRecord{}, err
	}
	if len(results) < 1 {
		return ModerationUsernameRecord{}, errors.New("no record found")
	}

	return results[0], err
}

func (c *Client) SetModerationUsername(username string, appropriate bool) error {
	err := c.ensureClientKeyspace(keyspaceModeration)
	if err != nil {
		return err
	}

	// no need for where clause, we are querying by primary key
	q := c.session.Query(moderationUsernameTable.UpdateBuilder(FIELD_APPROPRIATE, FIELD_CREATED).ToCql())
	q = q.BindMap(qb.M{FIELD_USERNAME: username, FIELD_APPROPRIATE: appropriate, FIELD_CREATED: time.Now()})
	return q.ExecRelease()
}

func (c *Client) InsertChatMessage(msgUUID, senderUUID, content, serverID string) error {
	err := c.ensureClientKeyspace(keyspaceModeration)
	if err != nil {
		return err
	}

	// no need for where clause, we are querying by primary key
	q := c.session.Query(moderationChatMessageTable.Insert())
	q = q.BindMap(qb.M{
		FIELD_CHATMSG_UUID:        msgUUID,
		FIELD_CHATMSG_SENDER_UUID: senderUUID,
		FIELD_CHATMSG_CREATED_AT:  time.Now(),
		FIELD_CHATMSG_UPDATED_AT:  time.Now(),
		FIELD_CHATMSG_SERVER_ID:   serverID,
		FIELD_CHATMSG_CONTENT:     content,
		FIELD_CHATMSG_STATUS:      0,
		FIELD_CHATMSG_REASON:      0,
	})
	return q.ExecRelease()
}
