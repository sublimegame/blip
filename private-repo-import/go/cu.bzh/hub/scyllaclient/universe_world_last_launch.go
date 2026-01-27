package scyllaclient

import (
	"errors"
	"time"

	"github.com/google/uuid"
	"github.com/scylladb/gocqlx/v3/qb"

	"cu.bzh/hub/scyllaclient/model/universe"
)

func (c *Client) UpsertWorldLastLaunch(wll WorldLastLaunch) error {

	err := c.ensureClientKeyspace(keyspaceUniverse)
	if err != nil {
		return err
	}

	// validate arguments
	if err := wll.ValidateWorldID(); err != nil {
		return err
	}
	if err := wll.ValidateUserID(); err != nil {
		return err
	}
	if wll.UpdatedAt.IsZero() {
		return errors.New("updatedAt is empty")
	}

	// ensure updatedAt is UTC value and rounded to the millisecond
	wll.UpdatedAt = wll.UpdatedAt.UTC().Truncate(time.Millisecond)

	// Build the record
	record := universe.WorldLastLaunchRecord{
		WorldID:   wll.WorldID,
		UserID:    wll.UserID,
		UpdatedAt: wll.UpdatedAt,
	}

	// insert or update record
	stmts, names := universe.WorldLastLaunchTable.Insert()
	q := c.session.Query(stmts, names)
	q = q.BindStruct(record)
	err = q.ExecRelease()

	return err
}

func (c *Client) GetWorldLastLaunch(worldID, userID string) (WorldLastLaunch, error) {

	err := c.ensureClientKeyspace(keyspaceUniverse)
	if err != nil {
		return WorldLastLaunch{}, err
	}

	builder := universe.WorldLastLaunchTable.SelectBuilder()
	builder = builder.Where(qb.Eq(universe.FIELD_UNIVERSE_WORLD_LAST_LAUNCH_WorldID))
	builder = builder.Where(qb.Eq(universe.FIELD_UNIVERSE_WORLD_LAST_LAUNCH_UserID))
	stmts, names := builder.ToCql()
	q := c.session.Query(stmts, names)
	q = q.BindMap(qb.M{
		universe.FIELD_UNIVERSE_WORLD_LAST_LAUNCH_WorldID: worldID,
		universe.FIELD_UNIVERSE_WORLD_LAST_LAUNCH_UserID:  userID,
	})

	var record universe.WorldLastLaunchRecord
	err = q.GetRelease(&record)
	if err != nil {
		return WorldLastLaunch{}, err
	}

	// construct WorldLastLaunch from record
	wll := WorldLastLaunch{
		WorldID:   record.WorldID,
		UserID:    record.UserID,
		UpdatedAt: record.UpdatedAt,
	}

	return wll, nil
}

// Count world last launches by worldID
func (c *Client) CountWorldLastLaunchByWorldID(worldID string) (int, error) {

	err := c.ensureClientKeyspace(keyspaceUniverse)
	if err != nil {
		return 0, err
	}

	builder := universe.WorldLastLaunchTable.SelectBuilder()
	builder = builder.CountAll().Where(qb.Eq(universe.FIELD_UNIVERSE_WORLD_LAST_LAUNCH_WorldID))
	stmts, names := builder.ToCql()
	q := c.session.Query(stmts, names)
	q = q.BindMap(qb.M{
		universe.FIELD_UNIVERSE_WORLD_LAST_LAUNCH_WorldID: worldID,
	})

	var count int
	err = q.GetRelease(&count)
	if err != nil {
		return 0, err
	}

	return count, nil
}

func (c *Client) DeleteWorldLastLaunch(worldID, userID string) error {

	err := c.ensureClientKeyspace(keyspaceUniverse)
	if err != nil {
		return err
	}

	// validate arguments
	if _, err := uuid.Parse(worldID); err != nil {
		return err
	}
	if _, err := uuid.Parse(userID); err != nil {
		return err
	}

	builder := universe.WorldLastLaunchTable.DeleteBuilder()
	builder = builder.Where(qb.Eq(universe.FIELD_UNIVERSE_WORLD_LAST_LAUNCH_WorldID))
	builder = builder.Where(qb.Eq(universe.FIELD_UNIVERSE_WORLD_LAST_LAUNCH_UserID))
	stmts, names := builder.ToCql()
	q := c.session.Query(stmts, names)
	q = q.BindMap(qb.M{
		universe.FIELD_UNIVERSE_WORLD_LAST_LAUNCH_WorldID: worldID,
		universe.FIELD_UNIVERSE_WORLD_LAST_LAUNCH_UserID:  userID,
	})
	err = q.ExecRelease()
	if err != nil {
		return err
	}

	return nil
}

func (c *Client) RemoveAllWorldLastLaunchesForUser(userID string) (count int, err error) {

	// get all world last launches for the user
	builder := universe.WorldLastLaunchByUserTable.SelectBuilder()
	builder = builder.Where(qb.Eq(universe.FIELD_UNIVERSE_WORLD_LAST_LAUNCH_UserID))
	stmts, names := builder.ToCql()
	q := c.session.Query(stmts, names)
	q = q.BindMap(qb.M{
		universe.FIELD_UNIVERSE_WORLD_LAST_LAUNCH_UserID: userID,
	})
	var records []universe.WorldLastLaunchRecord
	err = q.SelectRelease(&records)
	if err != nil {
		return 0, err
	}

	// delete all world last launches
	for _, record := range records {
		err := c.DeleteWorldLastLaunch(record.WorldID, record.UserID)
		if err != nil {
			return count, err
		}
		count++
	}

	return count, nil
}

// ----------------------------------------------
// Type - WorldLastLaunch
// ----------------------------------------------

type WorldLastLaunch struct {
	WorldID   string    `json:"worldID"`
	UserID    string    `json:"userID"`
	UpdatedAt time.Time `json:"updatedAt"`
}

func (wll WorldLastLaunch) ValidateWorldID() error {
	if wll.WorldID == "" {
		return errors.New("worldID is empty")
	}
	// validate worldID format
	if _, err := uuid.Parse(wll.WorldID); err != nil {
		return errors.New("worldID is invalid")
	}
	return nil
}

func (wll WorldLastLaunch) ValidateUserID() error {
	if wll.UserID == "" {
		return errors.New("userID is empty")
	}
	// validate userID format
	if _, err := uuid.Parse(wll.UserID); err != nil {
		return errors.New("userID is invalid")
	}
	return nil
}
