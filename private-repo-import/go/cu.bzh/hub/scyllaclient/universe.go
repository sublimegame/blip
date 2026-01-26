package scyllaclient

import (
	"errors"
	"time"

	"github.com/gocql/gocql"
	"github.com/scylladb/gocqlx/v3/qb"
	"github.com/scylladb/gocqlx/v3/table"
)

// ------------------------------
// Types
// ------------------------------
type FriendRequestResult struct {
	Type int
}

// ------------------------------
// Constants
// ------------------------------
var (
	FriendRequestNotCreated                  = FriendRequestResult{Type: 0}
	FriendRequestCreated                     = FriendRequestResult{Type: 1}
	FriendRequestConvertedIntoFriendRelation = FriendRequestResult{Type: 2}
)

// ------------------------------
// Table : relation
// ------------------------------

const (
	// Table universe.friend_relation
	tableUniverseRelation = "relation"
	// Fields
	FIELD_UNIVERSE_RELATION_USERID_1   = "id1"
	FIELD_UNIVERSE_RELATION_USERNAME_1 = "name1"
	FIELD_UNIVERSE_RELATION_USERID_2   = "id2"
	FIELD_UNIVERSE_RELATION_USERNAME_2 = "name2"
	FIELD_UNIVERSE_RELATION_TYPE       = "type"
	FIELD_UNIVERSE_RELATION_CREATED_AT = "created_at"
	// Relation types
	FIELD_UNIVERSE_RELATION_TYPE_FRIEND_REQ_SENT     = "frReqSent"
	FIELD_UNIVERSE_RELATION_TYPE_FRIEND_REQ_RECEIVED = "frReqRcvd"
	FIELD_UNIVERSE_RELATION_TYPE_FRIEND_RELATION     = "frRel"
)

// metadata specifies table name and columns it must be in sync with schema.
var universeRelationTableMetadata = table.Metadata{
	Name: tableUniverseRelation,
	Columns: []string{
		FIELD_UNIVERSE_RELATION_USERID_1,
		FIELD_UNIVERSE_RELATION_USERNAME_1,
		FIELD_UNIVERSE_RELATION_USERID_2,
		FIELD_UNIVERSE_RELATION_USERNAME_2,
		FIELD_UNIVERSE_RELATION_TYPE,
		FIELD_UNIVERSE_RELATION_CREATED_AT,
	},
	PartKey: []string{FIELD_UNIVERSE_RELATION_USERID_1},
	SortKey: []string{FIELD_UNIVERSE_RELATION_TYPE, FIELD_UNIVERSE_RELATION_USERID_2},
}

var universeRelationTable = table.New(universeRelationTableMetadata)

type UniverseRelation struct {
	ID1       gocql.UUID `json:"id1,omitempty" db:"id1"`
	Name1     string     `json:"name1,omitempty" db:"name1"`
	ID2       gocql.UUID `json:"id2,omitempty" db:"id2"`
	Name2     string     `json:"name2,omitempty" db:"name2"`
	Type      string     `json:"type,omitempty" db:"type"`
	CreatedAt time.Time  `json:"created_at,omitempty" db:"created_at"`
}

// ------------------------------
// RELATIONS
// ------------------------------

// InsertFriendRequest inserts a friend request in the database
func (c *Client) InsertFriendRequest(senderID, senderUsername, recipientID, recipientUsername string) (FriendRequestResult, error) {

	// check client is for correct keyspace
	err := c.ensureClientKeyspace(keyspaceUniverse)
	if err != nil {
		return FriendRequestNotCreated, err
	}

	// check if users are already friends
	rel, err := c.GetFriendRelation(senderID, recipientID)
	if err != nil {
		return FriendRequestNotCreated, err
	}
	if rel != nil {
		return FriendRequestNotCreated, errors.New("users are already friends")
	}

	// check if the friend request already exists (same-way)
	existingFriendReq, err := c.GetFriendRequest(senderID, recipientID)
	if err != nil {
		return FriendRequestNotCreated, err
	}
	if existingFriendReq != nil {
		return FriendRequestNotCreated, errors.New("request already exists")
	}

	// Prepare the friend request

	now := time.Now()

	senderUUID, err := gocql.ParseUUID(senderID)
	if err != nil {
		return FriendRequestNotCreated, err
	}

	recipientUUID, err := gocql.ParseUUID(recipientID)
	if err != nil {
		return FriendRequestNotCreated, err
	}

	// check if the opposite friend request already exists
	oppositeFriendReq, err := c.GetFriendRequest(recipientID, senderID)
	if err != nil {
		return FriendRequestNotCreated, err
	}
	if oppositeFriendReq != nil {
		// Remove opposite friend request and convert it into a friend relation
		batch := c.session.NewBatch(gocql.LoggedBatch)

		// delete rows for opposite friend request (recipient -> sender)
		stmt, _ := universeRelationTable.Delete()
		batch.Query(stmt, recipientID, FIELD_UNIVERSE_RELATION_TYPE_FRIEND_REQ_SENT, senderID)
		batch.Query(stmt, senderID, FIELD_UNIVERSE_RELATION_TYPE_FRIEND_REQ_RECEIVED, recipientID)

		// insert rows for friend relation
		stmt, _ = universeRelationTable.Insert()
		batch.Query(stmt, recipientUUID, recipientUsername, senderUUID, senderUsername, FIELD_UNIVERSE_RELATION_TYPE_FRIEND_RELATION, now)
		batch.Query(stmt, senderUUID, senderUsername, recipientUUID, recipientUsername, FIELD_UNIVERSE_RELATION_TYPE_FRIEND_RELATION, now)

		err = c.session.ExecuteBatch(batch)
		return FriendRequestConvertedIntoFriendRelation, err
	}

	// Insert new friend request in database

	relSent := UniverseRelation{
		ID1:       senderUUID,
		Name1:     senderUsername,
		ID2:       recipientUUID,
		Name2:     recipientUsername,
		Type:      FIELD_UNIVERSE_RELATION_TYPE_FRIEND_REQ_SENT,
		CreatedAt: now,
	}

	relRcvd := UniverseRelation{
		ID1:       recipientUUID,
		Name1:     recipientUsername,
		ID2:       senderUUID,
		Name2:     senderUsername,
		Type:      FIELD_UNIVERSE_RELATION_TYPE_FRIEND_REQ_RECEIVED,
		CreatedAt: now,
	}

	batch := c.session.NewBatch(gocql.LoggedBatch)
	stmt, _ := universeRelationTable.Insert()
	batch.Query(stmt, relSent.ID1, relSent.Name1, relSent.ID2, relSent.Name2, relSent.Type, relSent.CreatedAt) // sent
	batch.Query(stmt, relRcvd.ID1, relRcvd.Name1, relRcvd.ID2, relRcvd.Name2, relRcvd.Type, relRcvd.CreatedAt) // received
	err = c.session.ExecuteBatch(batch)
	if err != nil {
		return FriendRequestNotCreated, err
	}

	return FriendRequestCreated, err
}

// GetFriendRequestsSent returns userIDs of users to which the provided user has sent a friend request
func (c *Client) GetFriendRequestsSent(userID string) ([]UniverseRelation, error) {
	err := c.ensureClientKeyspace(keyspaceUniverse)
	if err != nil {
		return nil, err
	}

	builder := universeRelationTable.SelectBuilder()
	builder = builder.Where(qb.Eq(FIELD_UNIVERSE_RELATION_USERID_1), qb.Eq(FIELD_UNIVERSE_RELATION_TYPE))
	query := c.session.Query(builder.ToCql())
	query = query.BindMap(qb.M{
		FIELD_UNIVERSE_RELATION_USERID_1: userID,
		FIELD_UNIVERSE_RELATION_TYPE:     FIELD_UNIVERSE_RELATION_TYPE_FRIEND_REQ_SENT,
	})
	var results []UniverseRelation
	err = query.SelectRelease(&results)

	return results, err
}

// GetFriendRequestsReceived returns userIDs of users from which the provided user has received a friend request
func (c *Client) GetFriendRequestsReceived(userID string) ([]UniverseRelation, error) {
	err := c.ensureClientKeyspace(keyspaceUniverse)
	if err != nil {
		return nil, err
	}

	builder := universeRelationTable.SelectBuilder()
	builder = builder.Where(qb.Eq(FIELD_UNIVERSE_RELATION_USERID_1), qb.Eq(FIELD_UNIVERSE_RELATION_TYPE))
	query := c.session.Query(builder.ToCql())
	query = query.BindMap(qb.M{
		FIELD_UNIVERSE_RELATION_USERID_1: userID,
		FIELD_UNIVERSE_RELATION_TYPE:     FIELD_UNIVERSE_RELATION_TYPE_FRIEND_REQ_RECEIVED,
	})
	var results []UniverseRelation
	err = query.SelectRelease(&results)

	return results, err
}

// GetFriendRequestsSentAndReceived ...
func (c *Client) GetFriendRequestsSentAndReceived(userID string) ([]UniverseRelation, error) {
	err := c.ensureClientKeyspace(keyspaceUniverse)
	if err != nil {
		return nil, err
	}

	var results []UniverseRelation

	builder := universeRelationTable.SelectBuilder()
	builder = builder.Where(qb.Eq(FIELD_UNIVERSE_RELATION_USERID_1), qb.In(FIELD_UNIVERSE_RELATION_TYPE))
	query := c.session.Query(builder.ToCql())
	query = query.BindMap(qb.M{
		FIELD_UNIVERSE_RELATION_USERID_1: userID,
		FIELD_UNIVERSE_RELATION_TYPE:     []string{FIELD_UNIVERSE_RELATION_TYPE_FRIEND_REQ_SENT, FIELD_UNIVERSE_RELATION_TYPE_FRIEND_REQ_RECEIVED},
	})
	err = query.SelectRelease(&results)

	return results, err
}

// AcceptFriendRequest ...
// senderUserID : user having sent the friend request
// recipientUserID : user having received the friend request and doing the accepting
func (c *Client) AcceptFriendRequest(frReqSenderID, frReqRecipientID string) error {

	// check client is for correct keyspace
	err := c.ensureClientKeyspace(keyspaceUniverse)
	if err != nil {
		return err
	}

	// Read the friend request info
	var req UniverseRelation
	{
		// statement selects by partition key (by default), no need to specify it
		stmt, names := universeRelationTable.SelectBuilder().Where(qb.Eq(FIELD_UNIVERSE_RELATION_TYPE), qb.Eq(FIELD_UNIVERSE_RELATION_USERID_2)).ToCql()
		query := c.session.Query(stmt, names)
		query = query.BindMap(qb.M{
			FIELD_UNIVERSE_RELATION_USERID_1: frReqSenderID,
			FIELD_UNIVERSE_RELATION_TYPE:     FIELD_UNIVERSE_RELATION_TYPE_FRIEND_REQ_SENT,
			FIELD_UNIVERSE_RELATION_USERID_2: frReqRecipientID,
		})

		err = query.GetRelease(&req)
		// if not found, gocql.ErrNotFound is returned
		if err != nil {
			return err
		}
	}

	now := time.Now()

	// Create a new friend relation between the two users.
	// This is a batch operation since we need to add 2 rows.

	batch := c.session.NewBatch(gocql.LoggedBatch)

	// delete friend request rows
	stmt, _ := universeRelationTable.Delete()
	batch.Query(stmt, req.ID1, FIELD_UNIVERSE_RELATION_TYPE_FRIEND_REQ_SENT, req.ID2)
	batch.Query(stmt, req.ID2, FIELD_UNIVERSE_RELATION_TYPE_FRIEND_REQ_RECEIVED, req.ID1)

	// insert friend relation rows
	stmt, _ = universeRelationTable.Insert()
	batch.Query(stmt, req.ID1, req.Name1, req.ID2, req.Name2, FIELD_UNIVERSE_RELATION_TYPE_FRIEND_RELATION, now)
	batch.Query(stmt, req.ID2, req.Name2, req.ID1, req.Name1, FIELD_UNIVERSE_RELATION_TYPE_FRIEND_RELATION, now)

	err = c.session.ExecuteBatch(batch)

	return err
}

// DeleteFriendRequest ...
// Can be used for cancelling a friend request, but also for rejecting a friend request.
func (c *Client) DeleteFriendRequest(senderID, recipientID string) error {
	err := c.ensureClientKeyspace(keyspaceUniverse)
	if err != nil {
		return err
	}

	batch := c.session.NewBatch(gocql.LoggedBatch)
	stmt, _ := universeRelationTable.Delete()
	batch.Query(stmt, senderID, FIELD_UNIVERSE_RELATION_TYPE_FRIEND_REQ_SENT, recipientID)
	batch.Query(stmt, recipientID, FIELD_UNIVERSE_RELATION_TYPE_FRIEND_REQ_RECEIVED, senderID)
	err = c.session.ExecuteBatch(batch)

	return err
}

// DeleteAllFriendRequestsForUser deletes all friend requests for a user
// (both ways, sent by & received from the user)
// This is used when user account is deleted (by an admin)
// TODO: this should also be used when user account is deleted by the user himself
func (c *Client) DeleteAllFriendRequestsForUser(userID string) (int, error) {
	err := c.ensureClientKeyspace(keyspaceUniverse)
	if err != nil {
		return 0, err
	}

	// get all friend requests sent by the user
	friendRequestsSent, err := c.GetFriendRequestsSent(userID)
	if err != nil {
		return 0, err
	}

	// get all friend requests received by the user
	friendRequestsReceived, err := c.GetFriendRequestsReceived(userID)
	if err != nil {
		return 0, err
	}

	count := 0

	// delete all friend requests (sent by the user)
	for _, fr := range friendRequestsSent {
		// fr.ID1 is the userID of the user who sent the friend request
		// fr.ID2 is the userID of the user who received the friend request
		senderID := fr.ID1.String()
		recipientID := fr.ID2.String()
		err = c.DeleteFriendRequest(senderID, recipientID)
		if err != nil {
			return 0, err
		}
		count++
	}

	// delete all friend requests (received by the user)
	for _, fr := range friendRequestsReceived {
		// fr.ID1 is the userID of the user who received the friend request
		// fr.ID2 is the userID of the user who sent the friend request
		recipientID := fr.ID1.String()
		senderID := fr.ID2.String()
		err = c.DeleteFriendRequest(senderID, recipientID)
		if err != nil {
			return 0, err
		}
		count++
	}

	return count, nil
}

func (c *Client) DeleteAllFriendRelationsForUser(userID string) (int, error) {
	err := c.ensureClientKeyspace(keyspaceUniverse)
	if err != nil {
		return 0, err
	}

	// get all friend relations for the user
	friendRelations, err := c.GetFriendRelations(userID)
	if err != nil {
		return 0, err
	}

	count := 0

	// delete all friend relations
	for _, fr := range friendRelations {
		err = c.DeleteFriendRelation(fr.ID1.String(), fr.ID2.String())
		if err != nil {
			return 0, err
		}
		count++
	}

	return count, nil
}

// return whether a friend relation exists between two users (one-way test)
func (c *Client) GetFriendRequest(senderID, recipientID string) (*UniverseRelation, error) {
	err := c.ensureClientKeyspace(keyspaceUniverse)
	if err != nil {
		return nil, err
	}

	builder := universeRelationTable.SelectBuilder()
	builder = builder.Where(qb.Eq(FIELD_UNIVERSE_RELATION_USERID_1), qb.Eq(FIELD_UNIVERSE_RELATION_TYPE), qb.Eq(FIELD_UNIVERSE_RELATION_USERID_2))

	query := c.session.Query(builder.ToCql())
	query = query.BindMap(qb.M{
		FIELD_UNIVERSE_RELATION_USERID_1: senderID,
		FIELD_UNIVERSE_RELATION_TYPE:     FIELD_UNIVERSE_RELATION_TYPE_FRIEND_REQ_SENT,
		FIELD_UNIVERSE_RELATION_USERID_2: recipientID,
	})

	var result UniverseRelation
	err = query.GetRelease(&result)
	if err == gocql.ErrNotFound {
		return nil, nil // no error and no friend request found
	}

	return &result, err
}

// ------------------------------
// FRIEND RELATIONS
// ------------------------------

// GetFriendRelation returns a single friend relation between two users
func (c Client) GetFriendRelation(userID1, userID2 string) (*UniverseRelation, error) {
	err := c.ensureClientKeyspace(keyspaceUniverse)
	if err != nil {
		return nil, err
	}

	builder := universeRelationTable.SelectBuilder()
	builder = builder.Where(qb.Eq(FIELD_UNIVERSE_RELATION_USERID_1), qb.Eq(FIELD_UNIVERSE_RELATION_TYPE), qb.Eq(FIELD_UNIVERSE_RELATION_USERID_2))

	query := c.session.Query(builder.ToCql())
	query = query.BindMap(qb.M{
		FIELD_UNIVERSE_RELATION_USERID_1: userID1,
		FIELD_UNIVERSE_RELATION_TYPE:     FIELD_UNIVERSE_RELATION_TYPE_FRIEND_RELATION,
		FIELD_UNIVERSE_RELATION_USERID_2: userID2,
	})

	var result UniverseRelation
	err = query.GetRelease(&result)
	if err == gocql.ErrNotFound {
		return nil, nil // no error and no friend request found
	}

	return &result, err
}

func (c Client) GetFriendRelations(userID string) ([]UniverseRelation, error) {
	err := c.ensureClientKeyspace(keyspaceUniverse)
	if err != nil {
		return nil, err
	}

	builder := universeRelationTable.SelectBuilder()
	builder = builder.Where(qb.Eq(FIELD_UNIVERSE_RELATION_USERID_1), qb.Eq(FIELD_UNIVERSE_RELATION_TYPE))
	stmts, names := builder.ToCql()
	query := c.session.Query(stmts, names)
	query = query.BindMap(qb.M{
		FIELD_UNIVERSE_RELATION_USERID_1: userID,
		FIELD_UNIVERSE_RELATION_TYPE:     FIELD_UNIVERSE_RELATION_TYPE_FRIEND_RELATION,
	})

	var result []UniverseRelation
	err = query.SelectRelease(&result)

	return result, err
}

func (c Client) CountFriendRelations(userID string) (int, error) {
	err := c.ensureClientKeyspace(keyspaceUniverse)
	if err != nil {
		return 0, err
	}

	builder := universeRelationTable.SelectBuilder()
	builder = builder.CountAll().Where(qb.Eq(FIELD_UNIVERSE_RELATION_TYPE))
	stmt, names := builder.ToCql()

	query := c.session.Query(stmt, names)
	query = query.BindMap(qb.M{
		FIELD_UNIVERSE_RELATION_USERID_1: userID,
		FIELD_UNIVERSE_RELATION_TYPE:     FIELD_UNIVERSE_RELATION_TYPE_FRIEND_RELATION,
	})
	defer query.Release()

	iter := query.Iter()
	var result int = 0
	var count int = 0
	for iter.Scan(&count) {
		// print("count:", count)
		result += count
	}
	result += count
	err = iter.Close()

	// fmt.Println("result:", count, err)
	return count, err
}

func (c Client) DeleteFriendRelation(userID1, userID2 string) error {
	err := c.ensureClientKeyspace(keyspaceUniverse)
	if err != nil {
		return err
	}

	batch := c.session.NewBatch(gocql.LoggedBatch)
	stmt, _ := universeRelationTable.Delete()
	batch.Query(stmt, userID1, FIELD_UNIVERSE_RELATION_TYPE_FRIEND_RELATION, userID2)
	batch.Query(stmt, userID2, FIELD_UNIVERSE_RELATION_TYPE_FRIEND_RELATION, userID1)
	err = c.session.ExecuteBatch(batch)

	return err
}

// InsertFriendRelation directly inserts a friend relation in the database
// without converting a friend request into a friend relation.
// This has been implemented solely for migration purposes.
// func (c Client) InsertFriendRelation(userID1, username1, userID2, username2 string) error {
// 	err := c.ensureClientKeyspace(keyspaceUniverse)
// 	if err != nil {
// 		return err
// 	}
// 	now := time.Now()
// 	// Create a new friend relation between the two users.
// 	// This is a batch operation since we need to add 2 rows.
// 	batch := c.session.NewBatch(gocql.LoggedBatch)
// 	// insert friend relation rows
// 	stmt, _ := universeRelationTable.Insert()
// 	batch.Query(stmt, userID1, username1, userID2, username2, FIELD_UNIVERSE_RELATION_TYPE_FRIEND_RELATION, now)
// 	batch.Query(stmt, userID2, username2, userID1, username1, FIELD_UNIVERSE_RELATION_TYPE_FRIEND_RELATION, now)
// 	err = c.session.ExecuteBatch(batch)
// 	return err
// }

// This is used when user account is deleted.
func (c Client) DeleteAllRelationsForUser(userID string) error {
	var err error
	var records []UniverseRelation

	// Get all records related to the user
	{
		stmt, names := universeRelationTable.Select()
		query := c.session.Query(stmt, names)
		query = query.Bind(userID)
		err = query.SelectRelease(&records)
		if err != nil {
			return err
		}
	}

	batch := c.session.NewBatch(gocql.LoggedBatch)

	for _, rec := range records {

		// Delete the associated record
		stmt, _ := universeRelationTable.Delete()
		if rec.Type == FIELD_UNIVERSE_RELATION_TYPE_FRIEND_REQ_SENT {
			batch.Query(stmt, rec.ID2, FIELD_UNIVERSE_RELATION_TYPE_FRIEND_REQ_RECEIVED, rec.ID1)
			// fmt.Println(">>>DELETE>>>", rec.ID2, FIELD_UNIVERSE_RELATION_TYPE_FRIEND_REQ_RECEIVED, rec.ID1)

		} else if rec.Type == FIELD_UNIVERSE_RELATION_TYPE_FRIEND_REQ_RECEIVED {
			batch.Query(stmt, rec.ID2, FIELD_UNIVERSE_RELATION_TYPE_FRIEND_REQ_SENT, rec.ID1)
			// fmt.Println(">>>DELETE>>>", rec.ID2, FIELD_UNIVERSE_RELATION_TYPE_FRIEND_REQ_SENT, rec.ID1)

		} else if rec.Type == FIELD_UNIVERSE_RELATION_TYPE_FRIEND_RELATION {
			batch.Query(stmt, rec.ID2, FIELD_UNIVERSE_RELATION_TYPE_FRIEND_RELATION, rec.ID1)
			// fmt.Println(">>>DELETE>>>", rec.ID2, FIELD_UNIVERSE_RELATION_TYPE_FRIEND_RELATION, rec.ID1)
		}

		// Delete the record
		batch.Query(stmt, rec.ID1, rec.Type, rec.ID2)
		// fmt.Println(">>>DELETE>>>", rec.ID1, rec.Type, rec.ID2)
	}

	err = c.session.ExecuteBatch(batch)

	return err
}
