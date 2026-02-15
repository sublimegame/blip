package main

import (
	"fmt"
	"reflect"
	"slices"
	"strings"
	"time"

	"cu.bzh/hub/search"
)

//
// Type definition
//

// Note: almost same as search.Creation (time.Time instead of int64)
type Creation struct {
	// creation type (world, item, texture, ...)
	Type string `json:"type"`

	// common fields
	ID          string     `json:"id"`
	Name        string     `json:"name"`       // for world: title
	AuthorID    string     `json:"authorId"`   // user ID of the author
	AuthorName  string     `json:"authorName"` // username of the author (for item: repo)
	Description string     `json:"description"`
	Category    string     `json:"category"` // item: "hat", "head"
	CreatedAt   *time.Time `json:"createdAt"`
	UpdatedAt   *time.Time `json:"updatedAt"`
	Views       int32      `json:"views"`
	Likes       int32      `json:"likes"`
	Featured    bool       `json:"featured"`
	Banned      bool       `json:"banned"`
	Archived    bool       `json:"archived"`

	// item specific fields
	Item_Fullname   string `json:"item_fullname"` // TODO: check if this is needed
	Item_BlockCount int64  `json:"item_blockCount"`
}

//
// Constructors
//

// TODO: gaetan: add sanitization for the parameters and return an error if the parameters are invalid
func NewCreationFromValues(creationType, id, name, authorID, authorName, description, category string, createdAt, updatedAt *time.Time, views, likes int32, featured, banned, archived bool, itemFullname string, itemBlockCount int64) Creation {
	return Creation{
		Type:        creationType,
		ID:          id,
		Name:        name,
		AuthorID:    authorID,
		AuthorName:  authorName,
		Description: description,
		Category:    category,
		CreatedAt:   createdAt,
		UpdatedAt:   updatedAt,
		Views:       views,
		Likes:       likes,
		Featured:    featured,
		Banned:      banned,
		Archived:    archived,
	}
}

func NewCreationFromSearchCreation(sec search.Creation) Creation {
	var createdPtr *time.Time
	if sec.CreatedAt != 0 {
		val := time.UnixMicro(sec.CreatedAt)
		createdPtr = &val
	}
	var updatedPtr *time.Time
	if sec.UpdatedAt != 0 {
		val := time.UnixMicro(sec.UpdatedAt)
		updatedPtr = &val
	}

	return NewCreationFromValues(
		sec.Type,
		sec.ID,
		sec.Name,
		sec.AuthorID,
		sec.AuthorName,
		sec.Description,
		"",
		createdPtr,
		updatedPtr,
		sec.Views,
		sec.Likes,
		sec.Featured,
		sec.Banned,
		sec.Archived,
		"",
		0,
	)
}

func (c *Creation) KeepFields(fieldsToKeep []string) error {

	if len(fieldsToKeep) == 0 {
		return nil // nothing to do. success.
	}

	// process all fields of Worlds and set them to nil if they are not part of fields array
	val := reflect.ValueOf(c)

	if val.Kind() == reflect.Ptr {
		val = val.Elem()
	}

	if val.Kind() != reflect.Struct {
		return fmt.Errorf("obj must be a struct or a pointer to a struct")
	}

	typ := val.Type()

	for i := 0; i < val.NumField(); i++ {
		var field reflect.StructField = typ.Field(i)
		jsonTagInStruct, tagFoundInStruct := field.Tag.Lookup("json")
		if tagFoundInStruct {
			jsonTagInStruct = strings.TrimSuffix(jsonTagInStruct, ",omitempty")
			if !slices.Contains(fieldsToKeep, jsonTagInStruct) {
				ff := val.FieldByName(field.Name)
				// set field to its "zero" value
				if ff.CanSet() {
					ff.Set(reflect.Zero(field.Type))
				}
			}
		}
	}

	return nil
}

//
// Methods
//

func (c *Creation) IsWorld() bool {
	return c.Type == search.CreationTypeWorld
}

func (c *Creation) IsItem() bool {
	return c.Type == search.CreationTypeItem
}
