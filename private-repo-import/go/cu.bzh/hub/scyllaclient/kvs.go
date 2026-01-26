package scyllaclient

import (
	"log"

	"github.com/gocql/gocql"
	"github.com/scylladb/gocqlx/v3/qb"
	"github.com/scylladb/gocqlx/v3/table"
)

const (
	// Name of table kvs.unordered
	tableKvsUnordered = "unordered"
	// fields' names
	FIELD_WORLDID = "world_id"
	FIELD_STORE   = "store"
	FIELD_KEY     = "key"
	FIELD_VALUE   = "value"
)

// ------------------------------
// Types
// ------------------------------

// metadata specifies table name and columns it must be in sync with schema.
var unorderedTableMetadata = table.Metadata{
	Name:    tableKvsUnordered,
	Columns: []string{FIELD_WORLDID, FIELD_STORE, FIELD_KEY, FIELD_VALUE},
	PartKey: []string{FIELD_WORLDID, FIELD_STORE},
	SortKey: []string{FIELD_KEY},
}

var unorderedTable = table.New(unorderedTableMetadata)

type UnorderedRecord struct {
	WorldId gocql.UUID `db:"world_id"`
	Store   string     `db:"store"`
	KeyValue
}

type KeyValue struct {
	Key   string `json:"key,omitempty" db:"key"`
	Value []byte `json:"value,omitempty" db:"value"`
}

type KeyValueMap map[string][]byte

// ------------------------------
// Functions
// ------------------------------

type callback func(UnorderedRecord)

func (c *Client) GetAll(cb callback, limit uint) error {
	err := c.ensureClientKeyspace(keyspaceKvs)
	if err != nil {
		return err
	}

	query := c.session.Query("SELECT * FROM "+tableKvsUnordered, nil)
	iter := query.Iter()
	var result UnorderedRecord
	var count uint = 0
	for iter.StructScan(&result) {
		count += 1
		if limit > 0 && count > limit {
			break
		}
		cb(result)
	}
	err = iter.Close()

	return err
}

func (c *Client) GetUnorderedValuesByKeys(worldID, store string, keys []string) ([]UnorderedRecord, error) {
	err := c.ensureClientKeyspace(keyspaceKvs)
	if err != nil {
		return []UnorderedRecord{}, err
	}

	var result []UnorderedRecord = make([]UnorderedRecord, 0)

	worldIDAsUUID, err := gocql.ParseUUID(worldID)
	if err != nil {
		return result, err
	}

	q := c.session.Query(unorderedTable.SelectBuilder().Where(qb.Eq(FIELD_WORLDID), qb.Eq(FIELD_STORE), qb.In(FIELD_KEY)).ToCql())
	q = q.BindMap(qb.M{FIELD_WORLDID: worldIDAsUUID, FIELD_STORE: store, FIELD_KEY: keys})
	err = q.SelectRelease(&result)
	return result, err
}

// Single INSERT operation (no UPDATE)
func (c *Client) SetUnorderedKeyValue(record UnorderedRecord) error {
	err := c.ensureClientKeyspace(keyspaceKvs)
	if err != nil {
		return err
	}

	q := c.session.Query(unorderedTable.Insert()).BindStruct(record)
	err = q.ExecRelease()
	return err
}

// Batch SET operation
func (c *Client) SetUnorderedValuesByKeys(worldID, store string, keyValueMap KeyValueMap) error {
	err := c.ensureClientKeyspace(keyspaceKvs)
	if err != nil {
		return err
	}

	worldIDAsUUID, err := gocql.ParseUUID(worldID)
	if err != nil {
		return err
	}

	// Create a batch
	batch := c.session.NewBatch(gocql.LoggedBatch)

	// Iterate through users and add INSERT statements to the batch
	for k, v := range keyValueMap {
		if len(v) == 0 { // empty value means "remove" the record
			stmt, _ := qb.Delete(tableKvsUnordered).Where(qb.Eq(FIELD_WORLDID), qb.Eq(FIELD_STORE), qb.Eq(FIELD_KEY)).Existing().ToCql()
			batch.Query(stmt, worldIDAsUUID, store, k)
		} else {
			stmt, _ := qb.Insert(tableKvsUnordered).Columns(FIELD_WORLDID, FIELD_STORE, FIELD_KEY, FIELD_VALUE).ToCql()
			batch.Query(stmt, worldIDAsUUID, store, k, v)
		}
	}

	// Execute the batch
	if err := c.session.ExecuteBatch(batch); err != nil {
		log.Fatal("Error executing batch:", err)
	}

	return nil
}
