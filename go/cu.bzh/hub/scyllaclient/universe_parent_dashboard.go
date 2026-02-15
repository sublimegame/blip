package scyllaclient

import (
	"crypto/rand"
	"crypto/sha256"
	"fmt"
	"math/big"
	"time"

	"github.com/gocql/gocql"
	"github.com/scylladb/gocqlx/v3/qb"
	"github.com/scylladb/gocqlx/v3/table"
)

const (
	// Table universe.parent_dashboard
	TABLE_UNIVERSE_PARENTDASHBOARD               = "parent_dashboard"
	FIELD_UNIVERSE_PARENTDASHBOARD_DASHBOARDID   = "dashboard_id"
	FIELD_UNIVERSE_PARENTDASHBOARD_CHILDID       = "child_id"
	FIELD_UNIVERSE_PARENTDASHBOARD_PARENTID      = "parent_id"
	FIELD_UNIVERSE_PARENTDASHBOARD_SECRETKEYHASH = "secret_key_hash"
	FIELD_UNIVERSE_PARENTDASHBOARD_CREATEDAT     = "created_at"
	FIELD_UNIVERSE_PARENTDASHBOARD_UPDATEDAT     = "updated_at"
)

// ----------------------------------------------
// Table : PARENT_DASHBOARD
// ----------------------------------------------

// metadata specifies table name and columns it must be in sync with schema.
var universeParentDashboardTableMetadata = table.Metadata{
	Name: TABLE_UNIVERSE_PARENTDASHBOARD,
	Columns: []string{
		FIELD_UNIVERSE_PARENTDASHBOARD_DASHBOARDID,
		FIELD_UNIVERSE_PARENTDASHBOARD_CHILDID,
		FIELD_UNIVERSE_PARENTDASHBOARD_PARENTID,
		FIELD_UNIVERSE_PARENTDASHBOARD_SECRETKEYHASH,
		FIELD_UNIVERSE_PARENTDASHBOARD_CREATEDAT,
		FIELD_UNIVERSE_PARENTDASHBOARD_UPDATEDAT,
	},
	PartKey: []string{FIELD_UNIVERSE_PARENTDASHBOARD_DASHBOARDID},
	SortKey: []string{},
}

var universeParentDashboardTable = table.New(universeParentDashboardTableMetadata)

type UniverseParentDashboardRecord struct {
	DashboardID   string
	ChildID       string
	ParentID      string
	SecretKeyHash string
	CreatedAt     time.Time
	UpdatedAt     time.Time
}

func (prr UniverseParentDashboardRecord) Unpack() []interface{} {
	return []interface{}{
		&prr.DashboardID,
		&prr.ChildID,
		&prr.ParentID,
		&prr.SecretKeyHash,
		&prr.CreatedAt,
		&prr.UpdatedAt,
	}
}

const (
	charset = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789"
	length  = 10 // length of the dashboard ID
)

func generateRandomString() (string, error) {
	id := make([]byte, length)
	for i := range id {
		num, err := rand.Int(rand.Reader, big.NewInt(int64(len(charset))))
		if err != nil {
			return "", err
		}
		id[i] = charset[num.Int64()]
	}
	return string(id), nil
}

func hashSecretKey(s string) string {
	// use sha256 to hash the secret key
	sha := sha256.New()
	sha.Write([]byte("ILoveCubes@#$%^&")) // salt
	sha.Write([]byte(s))
	return fmt.Sprintf("%x", sha.Sum(nil))
}

func CheckSecretKey(secretKey, secretKeyHash string) bool {
	return secretKeyHash == hashSecretKey(secretKey)
}

// ----------------------------------------------
// Exposed functions
// ----------------------------------------------

// CreateUniverseParentDashboard creates a universe parent-dashboard record
func (c *Client) CreateUniverseParentDashboard(childID, parentID string) (*UniverseParentDashboardRecord, string, error) {
	// check client is for correct keyspace
	err := c.ensureClientKeyspace(keyspaceUniverse)
	if err != nil {
		return nil, "", err
	}

	dashboardID, err := generateRandomString()
	if err != nil {
		return nil, "", err
	}

	secretKeyClear, err := generateRandomString()
	if err != nil {
		return nil, "", err
	}
	secretKeyHash := hashSecretKey(secretKeyClear)

	prr := UniverseParentDashboardRecord{
		DashboardID:   dashboardID,
		ChildID:       childID,
		ParentID:      parentID,
		SecretKeyHash: secretKeyHash,
		CreatedAt:     time.Now(),
		UpdatedAt:     time.Now(),
	}

	// Create a new universe parent-dashboard record
	err = c.insertUniverseParentDashboard(prr)
	if err != nil {
		return nil, "", err
	}

	return &prr, secretKeyClear, nil // success
}

// GetAllUniverseParentDashboards returns all universe parent-dashboard records
func (c *Client) GetAllUniverseParentDashboards() ([]UniverseParentDashboardRecord, error) {
	// check client is for correct keyspace
	err := c.ensureClientKeyspace(keyspaceUniverse)
	if err != nil {
		return nil, err
	}

	stmts, names := qb.Select(universeParentDashboardTable.Metadata().Name).Columns().ToCql()
	q := c.session.Query(stmts, names)

	var records []UniverseParentDashboardRecord
	err = q.SelectRelease(&records)
	if err != nil {
		return nil, err
	}

	return records, nil
}

// GetUniverseParentDashboardByDashboardID returns a universe parent-dashboard record (by dashboard_id)
func (c *Client) GetUniverseParentDashboardByDashboardID(dashboardID string) (*UniverseParentDashboardRecord, error) {

	// check client is for correct keyspace
	err := c.ensureClientKeyspace(keyspaceUniverse)
	if err != nil {
		return nil, err
	}

	// Get the parent dashboard record by primary key
	stmts, names := universeParentDashboardTable.Get()
	q := c.session.Query(stmts, names)
	q = q.BindMap(qb.M{
		FIELD_UNIVERSE_PARENTDASHBOARD_DASHBOARDID: dashboardID,
	})

	var prr UniverseParentDashboardRecord
	err = q.GetRelease(&prr)
	if err == gocql.ErrNotFound {
		return nil, nil // no error, just not found
	}

	return &prr, nil
}

// // GetUniverseParentDashboardsByChildID returns all parent dashboards for a given child_id
// func (c *Client) GetUniverseParentDashboardsByChildID(childID string) ([]UniverseParentDashboardRecord, error) {
// 	// check client is for correct keyspace
// 	err := c.ensureClientKeyspace(keyspaceUniverse)
// 	if err != nil {
// 		return nil, err
// 	}

// 	q := c.session.Query(universeParentDashboardTable.Select())
// 	q = q.BindMap(map[string]interface{}{
// 		FIELD_UNIVERSE_PARENTDASHBOARD_CHILDID: childID,
// 	})
// 	iter := q.Iter()
// 	defer iter.Close()

// 	var prrs []UniverseParentDashboardRecord
// 	for {
// 		var prr UniverseParentDashboardRecord
// 		if !iter.Scan(prr.Unpack()...) {
// 			break
// 		}
// 		prrs = append(prrs, prr)
// 	}

// 	return prrs, nil
// }

// // GetUniverseParentDashboardsByParentID returns all parent dashboards for a given parent_id
// func (c *Client) GetUniverseParentDashboardsByParentID(parentID string) ([]UniverseParentDashboardRecord, error) {
// 	// check client is for correct keyspace
// 	err := c.ensureClientKeyspace(keyspaceUniverse)
// 	if err != nil {
// 		return nil, err
// 	}

// 	q := c.session.Query(universeParentDashboardTable.Select())
// 	q = q.BindMap(map[string]interface{}{
// 		FIELD_UNIVERSE_PARENTDASHBOARD_PARENTID: parentID,
// 	})
// 	iter := q.Iter()
// 	defer iter.Close()

// 	var prrs []UniverseParentDashboardRecord
// 	for {
// 		var prr UniverseParentDashboardRecord
// 		if !iter.Scan(prr.Unpack()...) {
// 			break
// 		}
// 		prrs = append(prrs, prr)
// 	}

// 	return prrs, nil
// }

// UpdateUniverseParentDashboard updates a universe parent-dashboard record (by dashboard_id)
// It only updates the updatedAt field
func (c *Client) UpdateUniverseParentDashboard(dashboardID string) error {
	// check client is for correct keyspace
	err := c.ensureClientKeyspace(keyspaceUniverse)
	if err != nil {
		return err
	}
	q := c.session.Query(universeParentDashboardTable.UpdateBuilder(FIELD_UNIVERSE_PARENTDASHBOARD_UPDATEDAT).ToCql())
	q = q.BindMap(qb.M{
		FIELD_UNIVERSE_PARENTDASHBOARD_DASHBOARDID: dashboardID,
		FIELD_UNIVERSE_PARENTDASHBOARD_UPDATEDAT:   time.Now()})
	return q.ExecRelease()
}

// // DeleteUniverseParentDashboard deletes a universe parent-dashboard record (by child_id and parent_id)
// func (c *Client) DeleteUniverseParentDashboard(dashboardID string) error {
// 	// check client is for correct keyspace
// 	err := c.ensureClientKeyspace(keyspaceUniverse)
// 	if err != nil {
// 		return err
// 	}
// 	q := c.session.Query(universeParentDashboardTable.Delete())
// 	q = q.BindMap(map[string]interface{}{
// 		FIELD_UNIVERSE_PARENTDASHBOARD_DASHBOARDID: dashboardID,
// 	})
// 	return q.ExecRelease()
// }

// ----------------------------------------------
// Private functions
// ----------------------------------------------

// insertUniverseParentDashboard inserts a universe parent-dashboard record (by child_id and parent_id)
func (c *Client) insertUniverseParentDashboard(prr UniverseParentDashboardRecord) error {
	// check client is for correct keyspace
	err := c.ensureClientKeyspace(keyspaceUniverse)
	if err != nil {
		return err
	}
	q := c.session.Query(universeParentDashboardTable.Insert())
	q = q.BindStruct(prr)
	return q.ExecRelease()
}
