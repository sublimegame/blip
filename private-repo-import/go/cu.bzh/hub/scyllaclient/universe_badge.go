package scyllaclient

import (
	"errors"
	"time"

	"cu.bzh/hub/scyllaclient/model/universe"
	"github.com/gocql/gocql"
	"github.com/google/uuid"
	"github.com/scylladb/gocqlx/v3/qb"
)

// ----------------------------------------------
// Operations - Badge
// ----------------------------------------------

func (c *Client) InsertBadge(badge Badge) (Badge, bool, error) {

	err := c.ensureClientKeyspace(keyspaceUniverse)
	if err != nil {
		return badge, false, err
	}

	// validate arguments
	if err = validateUUID(badge.BadgeID); err != nil {
		return badge, false, err
	}
	if err = validateUUID(badge.WorldID); err != nil {
		return badge, false, err
	}
	if err = validateTag(badge.Tag); err != nil {
		return badge, false, err
	}
	if badge.CreatedAt.IsZero() {
		return badge, false, errors.New("createdAt is empty")
	}
	if badge.UpdatedAt.IsZero() {
		return badge, false, errors.New("updatedAt is empty")
	}

	// ensure time values are UTC value and rounded to the millisecond
	badge.CreatedAt = badge.CreatedAt.UTC().Truncate(time.Millisecond)
	badge.UpdatedAt = badge.UpdatedAt.UTC().Truncate(time.Millisecond)

	// Build the record
	record := universe.BadgeRecord{
		BadgeID:        badge.BadgeID,
		WorldID:        badge.WorldID,
		Tag:            badge.Tag,
		Name:           badge.Name,
		Description:    badge.Description,
		HiddenIfLocked: badge.HiddenIfLocked,
		Rarity:         badge.Rarity,
		CreatedAt:      badge.CreatedAt,
		UpdatedAt:      badge.UpdatedAt,
	}

	// insert record (no update)
	stmts, names := universe.BadgeTable.InsertBuilder().Unique().ToCql()
	q := c.session.Query(stmts, names)
	q = q.BindStruct(record)
	defer q.Release()

	result := map[string]interface{}{}
	if err := q.MapScan(result); err != nil {
		return badge, false, err
	}

	recordInserted := false
	{
		applied, ok := result["[applied]"].(bool)
		if !ok {
			return badge, false, errors.New("failed to check if applied")
		}
		recordInserted = applied
	}

	return badge, recordInserted, err
}

// count all badges records
// (this is used in unit tests only)
func (c *Client) CountAllBadges() (int, error) {

	err := c.ensureClientKeyspace(keyspaceUniverse)
	if err != nil {
		return 0, err
	}

	// Build the query
	stmts := "SELECT COUNT(*) FROM " + universe.BadgeTable.Name()
	names := []string{}
	q := c.session.Query(stmts, names)

	var count int
	err = q.GetRelease(&count)

	return count, err
}

func (c *Client) UpdateBadge(badgeID string, bu BadgeUpdate) error {

	err := c.ensureClientKeyspace(keyspaceUniverse)
	if err != nil {
		return err
	}

	// validate arguments
	if err = validateUUID(badgeID); err != nil {
		return err
	}

	// ensure updatedAt is being provided
	if bu.UpdatedAt.IsZero() {
		return errors.New("updatedAt is missing")
	}

	// primary key for the update query
	badgeWorldID := ""
	badgeTag := ""
	{
		badge, err := c.GetBadgeByID(badgeID)
		if err != nil {
			return err
		}
		if badge == nil {
			return errors.New("badge not found")
		}
		badgeWorldID = badge.WorldID
		badgeTag = badge.Tag
	}

	// ensure updatedAt is UTC value and rounded to the millisecond
	bu.UpdatedAt = bu.UpdatedAt.UTC().Truncate(time.Millisecond)

	// Build the update query
	builder := universe.BadgeTable.UpdateBuilder()
	bindMap := qb.M{}

	builder.Set(universe.FIELD_UNIVERSE_BADGE_UpdatedAt)
	bindMap[universe.FIELD_UNIVERSE_BADGE_UpdatedAt] = bu.UpdatedAt

	if bu.Name != nil {
		builder.Set(universe.FIELD_UNIVERSE_BADGE_Name)
		bindMap[universe.FIELD_UNIVERSE_BADGE_Name] = *bu.Name
	}
	if bu.Description != nil {
		builder.Set(universe.FIELD_UNIVERSE_BADGE_Description)
		bindMap[universe.FIELD_UNIVERSE_BADGE_Description] = *bu.Description
	}

	bindMap[universe.FIELD_UNIVERSE_BADGE_WorldID] = badgeWorldID
	bindMap[universe.FIELD_UNIVERSE_BADGE_Tag] = badgeTag

	// build the query
	stmts, names := builder.ToCql()

	// execute the query
	q := c.session.Query(stmts, names)
	q = q.BindMap(bindMap)
	err = q.ExecRelease()

	return err
}

func (c *Client) GetBadgeByID(badgeID string) (*Badge, error) {

	err := c.ensureClientKeyspace(keyspaceUniverse)
	if err != nil {
		return nil, err
	}

	// validate arguments
	if err = validateUUID(badgeID); err != nil {
		return nil, err
	}

	// Build the query
	stmts, names := universe.BadgeByIDTable.Select()
	q := c.session.Query(stmts, names)
	q = q.BindMap(qb.M{
		universe.FIELD_UNIVERSE_BADGE_BadgeID: badgeID,
	})

	badge := Badge{}
	err = q.GetRelease(&badge)
	if errors.Is(err, gocql.ErrNotFound) {
		return nil, nil // not found (return nil object & no error)
	}

	return &badge, err
}

func (c *Client) GetBadgeByWorldIDAndTag(worldID, tag string) (*Badge, error) {

	err := c.ensureClientKeyspace(keyspaceUniverse)
	if err != nil {
		return nil, err
	}

	// validate arguments
	if err = validateUUID(worldID); err != nil {
		return nil, err
	}
	if err = validateTag(tag); err != nil {
		return nil, err
	}

	// Build the query
	builder := universe.BadgeTable.SelectBuilder()
	builder.Where(qb.Eq(universe.FIELD_UNIVERSE_BADGE_WorldID), qb.Eq(universe.FIELD_UNIVERSE_BADGE_Tag))
	stmts, names := builder.ToCql()
	q := c.session.Query(stmts, names)
	q = q.BindMap(qb.M{
		universe.FIELD_UNIVERSE_BADGE_WorldID: worldID,
		universe.FIELD_UNIVERSE_BADGE_Tag:     tag,
	})

	badge := Badge{}
	err = q.GetRelease(&badge)
	if errors.Is(err, gocql.ErrNotFound) {
		return nil, nil // not found (return nil object & no error)
	}

	return &badge, err
}

func (c *Client) GetBadgesByWorldID(worldID string) ([]Badge, error) {

	err := c.ensureClientKeyspace(keyspaceUniverse)
	if err != nil {
		return []Badge{}, err
	}

	// validate arguments
	if err = validateUUID(worldID); err != nil {
		return []Badge{}, err
	}

	// Build the query
	builder := universe.BadgeTable.SelectBuilder()
	builder.Where(qb.Eq(universe.FIELD_UNIVERSE_BADGE_WorldID))
	stmts, names := builder.ToCql()
	q := c.session.Query(stmts, names)
	q = q.BindMap(qb.M{
		universe.FIELD_UNIVERSE_BADGE_WorldID: worldID,
	})

	badges := []Badge{}
	err = q.SelectRelease(&badges)

	return badges, err
}

func (c *Client) DeleteBadge(worldID, tag string) error {

	err := c.ensureClientKeyspace(keyspaceUniverse)
	if err != nil {
		return err
	}

	// validate arguments
	if err = validateUUID(worldID); err != nil {
		return err
	}
	if err = validateTag(tag); err != nil {
		return err
	}

	// Build the query
	builder := universe.BadgeTable.DeleteBuilder()
	stmts, names := builder.ToCql()
	q := c.session.Query(stmts, names)
	q = q.BindMap(qb.M{
		universe.FIELD_UNIVERSE_BADGE_WorldID: worldID,
		universe.FIELD_UNIVERSE_BADGE_Tag:     tag,
	})
	err = q.ExecRelease()

	return err
}

// DeleteAllBadgesOfWorlds deletes all badges of the given worlds
// It is used to delete all badges created by a user when banning them.
func (c *Client) DeleteAllBadgesOfWorlds(worldIDs []string) (count int, err error) {

	err = c.ensureClientKeyspace(keyspaceUniverse)
	if err != nil {
		return 0, err
	}

	count = 0

	// for each world, get all badges and delete them
	for _, worldID := range worldIDs {
		// get all badges of the world
		badges, err := c.GetBadgesByWorldID(worldID)
		if err != nil {
			return 0, err
		}

		// delete each badge
		for _, badge := range badges {
			err = c.DeleteBadge(badge.WorldID, badge.Tag)
			if err != nil {
				return 0, err
			}
			count++
		}
	}

	return count, nil
}

// ----------------------------------------------
// Operations - UserBadge
// ----------------------------------------------

func (c *Client) CountAllUserBadges() (int, error) {

	err := c.ensureClientKeyspace(keyspaceUniverse)
	if err != nil {
		return 0, err
	}

	// Build the query
	stmts := "SELECT COUNT(*) FROM " + universe.UserBadgeTable.Name()
	names := []string{}
	q := c.session.Query(stmts, names)

	var count int
	err = q.GetRelease(&count)

	return count, err // ok
}

func (c *Client) InsertUserBadge(userID, badgeID string) error {

	err := c.ensureClientKeyspace(keyspaceUniverse)
	if err != nil {
		return err
	}

	// validate arguments
	if err = validateUUID(userID); err != nil {
		return err
	}
	if err = validateUUID(badgeID); err != nil {
		return err
	}

	// TODO: gaetan: check if userID and badgeID do exist in the database

	// Build the record
	record := universe.UserBadgeRecord{
		UserID:    userID,
		BadgeID:   badgeID,
		CreatedAt: time.Now(),
	}

	// insert record (no update)
	stmts, names := universe.UserBadgeTable.InsertBuilder().Unique().ToCql()
	q := c.session.Query(stmts, names)
	q = q.BindStruct(record)
	err = q.ExecRelease()

	return err
}

func (c *Client) GetUserBadge(userID, badgeID string) (*UserBadge, error) {

	err := c.ensureClientKeyspace(keyspaceUniverse)
	if err != nil {
		return nil, err
	}

	// validate arguments
	if err = validateUUID(userID); err != nil {
		return nil, err
	}
	if err = validateUUID(badgeID); err != nil {
		return nil, err
	}

	// Build the query
	builder := universe.UserBadgeTable.SelectBuilder()
	builder.Where(qb.Eq(universe.FIELD_UNIVERSE_USER_BADGE_UserID), qb.Eq(universe.FIELD_UNIVERSE_USER_BADGE_BadgeID))
	stmts, names := builder.ToCql()
	q := c.session.Query(stmts, names)
	q = q.BindMap(qb.M{
		universe.FIELD_UNIVERSE_USER_BADGE_UserID:  userID,
		universe.FIELD_UNIVERSE_USER_BADGE_BadgeID: badgeID,
	})

	userBadge := UserBadge{}
	err = q.GetRelease(&userBadge)
	if errors.Is(err, gocql.ErrNotFound) {
		return nil, nil // not found (return nil object & no error)
	}

	return &userBadge, err
}

func (c *Client) GetUserBadgesByUserID(userID string) ([]UserBadge, error) {

	err := c.ensureClientKeyspace(keyspaceUniverse)
	if err != nil {
		return []UserBadge{}, err
	}

	// validate arguments
	if err = validateUUID(userID); err != nil {
		return []UserBadge{}, err
	}

	// Build the query
	stmts, names := universe.UserBadgeTable.Select()
	q := c.session.Query(stmts, names)
	q = q.BindMap(qb.M{
		universe.FIELD_UNIVERSE_USER_BADGE_UserID: userID,
	})

	userBadges := []UserBadge{}
	err = q.SelectRelease(&userBadges)

	return userBadges, err
}

func (c *Client) CountUserBadgesByID(badgeID string) (int, error) {

	err := c.ensureClientKeyspace(keyspaceUniverse)
	if err != nil {
		return 0, err
	}

	// validate arguments
	if err = validateUUID(badgeID); err != nil {
		return 0, err
	}

	// Build the query
	builder := universe.UserBadgeByIDTable.SelectBuilder().CountAll()
	builder.Where(qb.Eq(universe.FIELD_UNIVERSE_USER_BADGE_BadgeID))
	stmts, names := builder.ToCql()
	q := c.session.Query(stmts, names)
	q = q.BindMap(qb.M{
		universe.FIELD_UNIVERSE_USER_BADGE_BadgeID: badgeID,
	})

	var count int
	err = q.GetRelease(&count)

	return count, err
}

func (c *Client) DeleteUserBadge(userID, badgeID string) error {

	err := c.ensureClientKeyspace(keyspaceUniverse)
	if err != nil {
		return err
	}

	// validate arguments
	if err = validateUUID(userID); err != nil {
		return err
	}
	if err = validateUUID(badgeID); err != nil {
		return err
	}

	// Build the query
	builder := universe.UserBadgeTable.DeleteBuilder()
	builder.Where(qb.Eq(universe.FIELD_UNIVERSE_USER_BADGE_UserID), qb.Eq(universe.FIELD_UNIVERSE_USER_BADGE_BadgeID))
	stmts, names := builder.ToCql()
	q := c.session.Query(stmts, names)
	q = q.BindMap(qb.M{
		universe.FIELD_UNIVERSE_USER_BADGE_UserID:  userID,
		universe.FIELD_UNIVERSE_USER_BADGE_BadgeID: badgeID,
	})
	err = q.ExecRelease()

	return err
}

// RemoveAllUserBadgesForUser removes all user badges for a given user
// Returns the number of badges removed
// Note: a "user badge" is the fact that a user has unlocked a badge
func (c *Client) RemoveAllUserBadgesForUser(userID string) (count int, err error) {

	err = c.ensureClientKeyspace(keyspaceUniverse)
	if err != nil {
		return 0, err
	}

	// validate arguments
	if err = validateUUID(userID); err != nil {
		return 0, err
	}

	// list all user badges
	allUserBadges, err := c.GetUserBadgesByUserID(userID)
	if err != nil {
		return 0, err
	}

	// delete each user badge
	count = 0
	for _, userBadge := range allUserBadges {
		err = c.DeleteUserBadge(userID, userBadge.BadgeID)
		if err != nil {
			return count, err
		}
		count++
	}

	return count, nil
}

// ----------------------------------------------
// Type - Badge
// ----------------------------------------------

type Badge struct {
	BadgeID        string    `json:"badgeID"`
	WorldID        string    `json:"worldID"`
	Tag            string    `json:"tag"`
	Name           string    `json:"name"`
	Description    string    `json:"description"`
	HiddenIfLocked bool      `json:"hiddenIfLocked,omitempty"`
	Rarity         float32   `json:"rarity,omitempty"`
	CreatedAt      time.Time `json:"createdAt"`
	UpdatedAt      time.Time `json:"updatedAt"`
}

// ----------------------------------------------
// Type - UserBadge
// ----------------------------------------------

type UserBadge struct {
	UserID    string
	BadgeID   string
	CreatedAt time.Time
}

// ----------------------------------------------
// Type - BadgeUpdate
// ----------------------------------------------

type BadgeUpdate struct {
	Name        *string
	Description *string
	UpdatedAt   time.Time
}

// ----------------------------------------------
// Utility functions
// ----------------------------------------------

func validateUUID(uuidStr string) error {
	if uuidStr == "" {
		return errors.New("uuid is empty")
	}
	if _, err := uuid.Parse(uuidStr); err != nil {
		return errors.New("invalid uuid")
	}
	return nil // ok
}

func validateTag(tag string) error {
	if tag == "" {
		return errors.New("tag is empty")
	}
	return nil // ok
}
