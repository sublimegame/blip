package scyllaclient

import (
	"time"

	"github.com/gocql/gocql"
	"github.com/scylladb/gocqlx/v3/qb"
	"github.com/scylladb/gocqlx/v3/table"
)

const (
	// Table universe.module
	TABLE_UNIVERSE_MODULE                 = "module"
	FIELD_UNIVERSE_MODULE_ORIGIN          = "origin"
	FIELD_UNIVERSE_MODULE_COMMIT          = "commit"
	FIELD_UNIVERSE_MODULE_SCRIPT          = "script"
	FIELD_UNIVERSE_MODULE_DOCUMENTATION   = "documentation"
	FIELD_UNIVERSE_MODULE_COMMITCREATEDAT = "commit_created_at"
	FIELD_UNIVERSE_MODULE_CREATEDAT       = "created_at"
	FIELD_UNIVERSE_MODULE_UPDATEDAT       = "updated_at"

	// Table universe.module_ref
	TABLE_UNIVERSE_MODULEREF           = "module_ref"
	FIELD_UNIVERSE_MODULEREF_ORIGIN    = "origin"
	FIELD_UNIVERSE_MODULEREF_REF       = "ref"
	FIELD_UNIVERSE_MODULEREF_COMMIT    = "commit"
	FIELD_UNIVERSE_MODULEREF_REFISC    = "ref_is_commit"
	FIELD_UNIVERSE_MODULEREF_HASH      = "hash"
	FIELD_UNIVERSE_MODULEREF_CREATEDAT = "created_at"
	FIELD_UNIVERSE_MODULEREF_UPDATEDAT = "updated_at"

	// Table universe.module_contributor
	TABLE_UNIVERSE_MODULECONTRIBUTOR          = "module_contributor"
	FIELD_UNIVERSE_MODULECONTRIBUTOR_ORIGIN   = "origin"
	FIELD_UNIVERSE_MODULECONTRIBUTOR_COMMIT   = "commit"
	FIELD_UNIVERSE_MODULECONTRIBUTOR_USERID   = "user_id"
	FIELD_UNIVERSE_MODULECONTRIBUTOR_USERNAME = "user_name"
	FIELD_UNIVERSE_MODULECONTRIBUTOR_SHARE    = "share"
)

// ----------------------------------------------
// Table : MODULE
// ----------------------------------------------

// metadata specifies table name and columns it must be in sync with schema.
var universeModuleTableMetadata = table.Metadata{
	Name: TABLE_UNIVERSE_MODULE,
	Columns: []string{
		FIELD_UNIVERSE_MODULE_ORIGIN,
		FIELD_UNIVERSE_MODULE_COMMIT,
		FIELD_UNIVERSE_MODULE_SCRIPT,
		FIELD_UNIVERSE_MODULE_DOCUMENTATION,
		FIELD_UNIVERSE_MODULE_COMMITCREATEDAT,
		FIELD_UNIVERSE_MODULE_CREATEDAT,
		FIELD_UNIVERSE_MODULE_UPDATEDAT,
	},
	PartKey: []string{FIELD_UNIVERSE_MODULE_ORIGIN},
	SortKey: []string{FIELD_UNIVERSE_MODULE_COMMIT},
}

var universeModuleTable = table.New(universeModuleTableMetadata)

type UniverseModuleRecord struct {
	Origin          string
	Commit          string
	Script          string
	Documentation   string
	CommitCreatedAt time.Time
	CreatedAt       time.Time
	UpdatedAt       time.Time
}

func (mr UniverseModuleRecord) Unpack() []interface{} {
	return []interface{}{
		&mr.Origin,
		&mr.Commit,
		&mr.Script,
		&mr.Documentation,
		&mr.CommitCreatedAt,
		&mr.CreatedAt,
		&mr.UpdatedAt,
	}
}

// ----------------------------------------------
// Table : MODULE_REF
// ----------------------------------------------

// metadata specifies table name and columns it must be in sync with schema.
var universeModuleRefTableMetadata = table.Metadata{
	Name: TABLE_UNIVERSE_MODULEREF,
	Columns: []string{
		FIELD_UNIVERSE_MODULEREF_ORIGIN,
		FIELD_UNIVERSE_MODULEREF_REF,
		FIELD_UNIVERSE_MODULEREF_COMMIT,
		FIELD_UNIVERSE_MODULEREF_REFISC,
		FIELD_UNIVERSE_MODULEREF_HASH,
		FIELD_UNIVERSE_MODULEREF_CREATEDAT,
		FIELD_UNIVERSE_MODULEREF_UPDATEDAT,
	},
	PartKey: []string{FIELD_UNIVERSE_MODULEREF_ORIGIN},
	SortKey: []string{FIELD_UNIVERSE_MODULEREF_REF},
}

var universeModuleRefTable = table.New(universeModuleRefTableMetadata)

type UniverseModuleRefRecord struct {
	Origin      string    `db:"origin" cql:"origin" json:"origin"`
	Ref         string    `db:"ref" cql:"ref" json:"ref"`
	Commit      string    `db:"commit" cql:"commit" json:"commit"`
	RefIsCommit bool      `db:"ref_is_commit" cql:"ref_is_commit" json:"ref_is_commit"`
	Hash        string    `db:"hash" cql:"hash" json:"hash"`
	CreatedAt   time.Time `db:"created_at" cql:"created_at" json:"created_at"`
	UpdatedAt   time.Time `db:"updated_at" cql:"updated_at" json:"updated_at"`
}

func (mr UniverseModuleRefRecord) Unpack() []interface{} {
	return []interface{}{
		&mr.Origin,
		&mr.Ref,
		&mr.Commit,
		&mr.RefIsCommit,
		&mr.Hash,
		&mr.CreatedAt,
		&mr.UpdatedAt,
	}
}

// ----------------------------------------------
// Table : MODULE_CONTRIBUTOR
// ----------------------------------------------

// metadata specifies table name and columns it must be in sync with schema.
var universeModuleContributorTableMetadata = table.Metadata{
	Name: TABLE_UNIVERSE_MODULECONTRIBUTOR,
	Columns: []string{
		FIELD_UNIVERSE_MODULECONTRIBUTOR_ORIGIN,
		FIELD_UNIVERSE_MODULECONTRIBUTOR_COMMIT,
		FIELD_UNIVERSE_MODULECONTRIBUTOR_USERID,
		FIELD_UNIVERSE_MODULECONTRIBUTOR_USERNAME,
		FIELD_UNIVERSE_MODULECONTRIBUTOR_SHARE,
	},
	PartKey: []string{FIELD_UNIVERSE_MODULECONTRIBUTOR_ORIGIN},
	SortKey: []string{FIELD_UNIVERSE_MODULECONTRIBUTOR_COMMIT, FIELD_UNIVERSE_MODULECONTRIBUTOR_USERID},
}

var universeModuleContributorTable = table.New(universeModuleContributorTableMetadata)

type UniverseModuleContributorRecord struct {
	Origin   string
	Commit   string
	UserID   gocql.UUID
	Username string
	Share    float32
}

func (mr UniverseModuleContributorRecord) Unpack() []interface{} {
	return []interface{}{
		&mr.Origin,
		&mr.Commit,
		&mr.UserID,
		&mr.Username,
		&mr.Share,
	}
}

// ----------------------------------------------
// FUNCTIONS
// ----------------------------------------------

// UpsertUniverseModule inserts or updates a universe module record, by origin and ref.
func (c *Client) InsertUniverseModule(mod UniverseModuleRecord, refs []UniverseModuleRefRecord, contrib []UniverseModuleContributorRecord) error {
	// check client is for correct keyspace
	err := c.ensureClientKeyspace(keyspaceUniverse)
	if err != nil {
		return err
	}

	batch := c.session.NewBatch(gocql.LoggedBatch)

	// Insert module
	stmt, _ := universeModuleTable.Insert()
	batch.Query(stmt, mod.Unpack()...)

	// Insert refs
	for _, ref := range refs {
		stmt, _ := universeModuleRefTable.Insert()
		batch.Query(stmt, ref.Unpack()...)
	}

	// Insert contributors
	for _, c := range contrib {
		stmt, _ := universeModuleContributorTable.Insert()
		batch.Query(stmt, c.Unpack()...)
	}

	return c.session.ExecuteBatch(batch)
}

// UpsertUniverseModule inserts or updates a universe module record, by origin and ref.
func (c *Client) InsertUniverseModuleRef(ref UniverseModuleRefRecord) error {
	// check client is for correct keyspace
	err := c.ensureClientKeyspace(keyspaceUniverse)
	if err != nil {
		return err
	}

	q := c.session.Query(universeModuleRefTable.Insert())
	q = q.BindStruct(ref)
	return q.ExecRelease()
}

// GetUniverseModuleByRef ...
func (c *Client) GetUniverseModuleByRef(origin, ref string) (*UniverseModuleRecord, error) {
	err := c.ensureClientKeyspace(keyspaceUniverse)
	if err != nil {
		return nil, err
	}

	// Get ref record
	refRecord, err := c.GetUniverseModuleRef(origin, ref)
	if err != nil {
		return nil, err
	}
	if refRecord == nil {
		return nil, nil // no error, just not found
	}

	// Get module record
	q := c.session.Query(universeModuleTable.SelectBuilder().Where(qb.Eq(FIELD_UNIVERSE_MODULE_COMMIT)).ToCql())
	q = q.BindMap(qb.M{
		FIELD_UNIVERSE_MODULE_ORIGIN: refRecord.Origin,
		FIELD_UNIVERSE_MODULE_COMMIT: refRecord.Commit,
	})
	var result UniverseModuleRecord
	err = q.GetRelease(&result)
	if err == gocql.ErrNotFound {
		return nil, nil // no error, just not found
	}

	return &result, err
}

func (c *Client) GetUniverseModuleRef(origin, ref string) (*UniverseModuleRefRecord, error) {
	err := c.ensureClientKeyspace(keyspaceUniverse)
	if err != nil {
		return nil, err
	}

	// no need for where clause, we are querying by primary key
	q := c.session.Query(universeModuleRefTable.SelectBuilder().Where(qb.Eq(FIELD_UNIVERSE_MODULEREF_REF)).ToCql())
	q = q.BindMap(qb.M{
		FIELD_UNIVERSE_MODULEREF_ORIGIN: origin,
		FIELD_UNIVERSE_MODULEREF_REF:    ref,
	})
	var result UniverseModuleRefRecord
	err = q.GetRelease(&result)
	if err == gocql.ErrNotFound {
		return nil, nil // no error, just not found
	}

	return &result, err
}

// GetUniverseModule returns a universe module record by primary key (origin, commit)
// Returns nil if not found.
func (c *Client) GetUniverseModule(origin, commit string) (*UniverseModuleRecord, error) {
	err := c.ensureClientKeyspace(keyspaceUniverse)
	if err != nil {
		return nil, err
	}

	// no need for where clause, we are querying by primary key
	stmt, names := universeModuleTable.SelectBuilder().Where(qb.Eq(FIELD_UNIVERSE_MODULE_COMMIT)).ToCql()
	q := c.session.Query(stmt, names)
	q = q.BindMap(qb.M{
		FIELD_UNIVERSE_MODULE_ORIGIN: origin,
		FIELD_UNIVERSE_MODULE_COMMIT: commit,
	})
	var result UniverseModuleRecord
	err = q.GetRelease(&result)
	if err == gocql.ErrNotFound {
		return nil, nil // no error, just not found
	}

	return &result, err
}
