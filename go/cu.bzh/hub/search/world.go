package search

import (
	"context"
	"errors"
	"fmt"
	"net/http"
	"reflect"
	"strconv"
	"strings"
	"time"

	"github.com/typesense/typesense-go/typesense"
	"github.com/typesense/typesense-go/typesense/api"
)

const (
	kWorldFieldID         = "id"
	kWorldFieldTitle      = "title"
	kWorldFieldAuthorID   = "authorId"
	kWorldFieldAuthorName = "authorName"
	kWorldFieldCreatedAt  = "createdAt"
	kWorldFieldUpdatedAt  = "updatedAt"
	kWorldFieldViews      = "views"
	kWorldFieldLikes      = "likes"
	kWorldFieldFeatured   = "featured"
	kWorldFieldBanned     = "banned"
	kWorldFieldArchived   = "archived"
)

// --------------------------------------------------
// Types
// --------------------------------------------------

// World type, as it is stored as a document in the typesense database
type World struct {
	ID         string `json:"id"`
	Title      string `json:"title"`
	AuthorID   string `json:"authorId"`
	AuthorName string `json:"authorName"`
	CreatedAt  int64  `json:"createdAt"`
	UpdatedAt  int64  `json:"updatedAt"`
	Views      int32  `json:"views"`
	Likes      int32  `json:"likes"`
	Featured   bool   `json:"featured"`
	Banned     bool   `json:"banned"`
	Archived   bool   `json:"archived"`
}

// WorldUpdate ...
type WorldUpdate struct {
	ID         string  `json:"id"`
	Title      *string `json:"title"`
	AuthorID   *string `json:"authorId"`
	AuthorName *string `json:"authorName"`
	CreatedAt  *int64  `json:"createdAt"`
	UpdatedAt  *int64  `json:"updatedAt"`
	Likes      *int32  `json:"likes"`
	Views      *int32  `json:"views"`
	Featured   *bool   `json:"featured"`
	Banned     *bool   `json:"banned"`
	Archived   *bool   `json:"archived"`
}

func (w *World) String() string {
	return fmt.Sprintf(
		"World{ID: %s, Title: %s, AuthorID: %s, AuthorName: %s, CreatedAt: %d, UpdatedAt: %d, Likes: %d, Views: %d, Featured: %t, Banned: %t, Archived: %t}",
		w.ID, w.Title, w.AuthorID, w.AuthorName, w.CreatedAt, w.UpdatedAt, w.Likes, w.Views, w.Featured, w.Banned, w.Archived,
	)
}

func (w *World) Print() {
	str := fmt.Sprintf(
		"| World\n| %s\n| %s\n| %s\n| %s\n| %d\n| %d\n| %d\n| %d\n| %t\n| %t\n| %t\n|-------------",
		w.ID,
		w.Title,
		w.AuthorID,
		w.AuthorName,
		w.CreatedAt,
		w.UpdatedAt,
		w.Likes,
		w.Views,
		w.Featured,
		w.Banned,
		w.Archived,
	)
	fmt.Println(str)
}

// SearchWorldsResult is the result of a search in the World collection
type SearchWorldsResult struct {
	Results      []World
	TotalResults int
	Page         int
	PerPage      int
}

// --------------------------------------------------
// Exposed functions
// --------------------------------------------------

// NewWorldFromValues creates a new World struct using the provided values.
func NewWorldFromValues(id, title, authorID, authorName string, createdAt, updatedAt time.Time, likes, views int32, featured, banned, archived bool) World {
	world := World{
		ID:         id,
		Title:      title,
		AuthorID:   authorID,
		AuthorName: authorName,
		CreatedAt:  createdAt.UnixMicro(),
		UpdatedAt:  updatedAt.UnixMicro(),
		Likes:      likes,
		Views:      views,
		Featured:   featured,
		Banned:     banned,
		Archived:   archived,
	}
	return world
}

// CreateWorldCollection ...
func (c *Client) CreateWorldCollection() error {

	// Remove collection if it already exists
	if c.CollectionExists(COLLECTION_WORLDS) {
		err := c.CollectionDelete(COLLECTION_WORLDS)
		if err != nil {
			return err
		}
	}

	schema := &api.CollectionSchema{
		Name: COLLECTION_WORLDS,
		Fields: []api.Field{
			{ // UUID
				Name: kWorldFieldID,
				Type: "string",
			},
			{
				Name: kWorldFieldTitle,
				Type: "string",
			},
			{
				Name: kWorldFieldAuthorID,
				Type: "string",
			},
			{
				Name: kWorldFieldAuthorName,
				Type: "string",
			},
			{
				Name: kWorldFieldCreatedAt,
				Type: "int64",
			},
			{
				Name: kWorldFieldUpdatedAt,
				Type: "int64",
			},
			{
				Name: kWorldFieldLikes,
				Type: "int32",
			},
			{
				Name: kWorldFieldViews,
				Type: "int32",
			},
			{
				Name: kWorldFieldFeatured,
				Type: "bool",
			},
			{
				Name: kWorldFieldBanned,
				Type: "bool",
			},
			{
				Name: kWorldFieldArchived,
				Type: "bool",
			},
		},
	}
	_ /* CollectionResponse */, err := c.TSClient.Collections().Create(context.Background(), schema)
	return err
}

// UpsertWorld updates an entire World, or inserts it if it doesn't already exist in the database.
func (c *Client) UpsertWorld(w World) error {
	_, err := c.TSClient.Collection(COLLECTION_WORLDS).Documents().Upsert(context.Background(), w)
	return err
}

// UpdateWorld updates a World. It can be a partial update.
func (c *Client) UpdateWorld(wu WorldUpdate) error {
	if wu.ID == "" {
		return errors.New("world ID is empty")
	}

	// Loop on all the fields of the WorldUpdate struct using reflection
	updateData := make(map[string]interface{})

	// Get the type of the struct
	structType := reflect.TypeOf(wu)

	// Get the value of the struct
	structValue := reflect.ValueOf(wu)

	// Loop over the fields of the struct
	for i := 0; i < structType.NumField(); i++ {
		// Get the field type and value
		fieldType := structType.Field(i)
		fieldValue := structValue.Field(i)

		// Check if the field value is nil
		if fieldValue.Kind() == reflect.Pointer {
			if fieldValue.IsNil() {
				continue
			}
			fieldValue = fieldValue.Elem()
		}

		// Get the JSON tag value
		jsonTag := fieldType.Tag.Get("json")
		jsonTag = strings.Split(jsonTag, ",")[0]
		key := jsonTag
		if key == "" {
			key = fieldType.Name
		}
		if key == "id" || key == "ID" {
			continue
		}

		// Add the field name and value to the map
		updateData[key] = fieldValue.Interface()
	}

	_, err := c.TSClient.Collection(COLLECTION_WORLDS).Document(wu.ID).Update(context.Background(), updateData)
	// TODO: (gdevillele) remove this dirty fix when the typesense client is fixed
	//                    PR: https://github.com/typesense/typesense-go/pull/151
	if err != nil && strings.HasPrefix(err.Error(), "status: 201") {
		err = nil
	}

	return err
}

type WorldFilter struct {
	AuthorID   *string
	AuthorName *string
	Featured   *bool
	Banned     *bool
	Archived   *bool
}

// SearchCollectionWorlds ...
func (c *Client) SearchCollectionWorlds(searchText string, sortBy string, page, perPage int, filter WorldFilter) (result SearchWorldsResult, err error) {
	fmt.Println("🔎 SearchCollectionWorlds", filter)

	err = nil

	searchParameters := &api.SearchCollectionParams{
		Q:       searchText,
		QueryBy: kWorldFieldTitle + ", " + kWorldFieldAuthorName,
		// QueryByWeights: "2, 2, 1, 1, 1",
		SortBy:   &sortBy,
		PerPage:  &perPage,
		Page:     &page,
		FilterBy: nil,
	}

	// Filter By
	{
		var filterByStr string = ""
		var first bool = true // is true only for the first filter
		if filter.AuthorID != nil && len(*filter.AuthorID) > 0 {
			if !first {
				filterByStr += " && "
			}
			filterByStr += kWorldFieldAuthorID + ":=" + *filter.AuthorID
			first = false
		}
		if filter.AuthorName != nil && len(*filter.AuthorName) > 0 {
			if !first {
				filterByStr += " && "
			}
			filterByStr += kWorldFieldAuthorName + ":=" + *filter.AuthorName
			first = false
		}
		if filter.Featured != nil {
			if !first {
				filterByStr += " && "
			}
			filterByStr += kWorldFieldFeatured + ":=" + strconv.FormatBool(*filter.Featured)
			first = false
		}
		if filter.Banned != nil {
			if !first {
				filterByStr += " && "
			}
			filterByStr += kWorldFieldBanned + ":=" + strconv.FormatBool(*filter.Banned)
			first = false
		}
		if filter.Archived != nil {
			if !first {
				filterByStr += " && "
			}
			filterByStr += kWorldFieldArchived + ":=" + strconv.FormatBool(*filter.Archived)
			first = false
		}
		if len(filterByStr) > 0 {
			searchParameters.FilterBy = &filterByStr
		}
	}

	// fmt.Println("✨ filterBy:", searchParameters.FilterBy)

	var searchResult *api.SearchResult = nil
	searchResult, err = c.TSClient.Collection(COLLECTION_WORLDS).Documents().Search(context.Background(), searchParameters)
	if err != nil {
		return
	}
	if searchResult == nil {
		err = errors.New("search result is nil")
		return
	}

	// total count of search hits
	if searchResult.Found != nil {
		result.TotalResults = *searchResult.Found
	}

	// index of the page returned
	if searchResult.Page != nil {
		result.Page = *searchResult.Page
	}

	// number of hits per page
	result.PerPage = searchResult.RequestParams.PerPage

	var hits []api.SearchResultHit = make([]api.SearchResultHit, 0)
	if searchResult.Hits != nil {
		hits = *searchResult.Hits
	}

	var world World
	for _, hit := range hits {
		world, err = newWorldFromDocument(hit.Document)
		if err != nil {
			return
		}
		result.Results = append(result.Results, world)
	}

	return
}

// DeleteWorld delete a World record in the search database.
func (c *Client) DeleteWorld(ID string) (found bool, err error) {
	// consider it found by default
	// (if the document is not found, it will be set to false below)
	found = true

	// try deleting the document in the collection
	_, err = c.TSClient.Collection(COLLECTION_WORLDS).Document(ID).Delete(context.Background())
	if err != nil {
		httpErr, ok := err.(*typesense.HTTPError)
		if ok && httpErr.Status == http.StatusNotFound { // HTTP 404
			// "not found" is not considered an error
			found = false
			err = nil
		}
	}

	return found, err
}

// --------------------------------------------------
// Unexposed functions
// --------------------------------------------------

func newWorldFromDocument(docPtr *map[string]interface{}) (World, error) {
	var res = World{}

	// make sure docPtr is not nil
	if docPtr == nil {
		return res, ErrDocumentIsNil
	}
	doc := *docPtr

	// ID
	if v, ok := doc[kWorldFieldID].(string); ok {
		res.ID = v
	}

	// Title
	if v, ok := doc[kWorldFieldTitle].(string); ok {
		res.Title = v
	}

	// AuthorID
	if v, ok := doc[kWorldFieldAuthorID].(string); ok {
		res.AuthorID = v
	}

	// AuthorName
	if v, ok := doc[kWorldFieldAuthorName].(string); ok {
		res.AuthorName = v
	}

	// CreatedAt
	if v, ok := doc[kWorldFieldCreatedAt].(float64); ok {
		res.CreatedAt = int64(v)
	}

	// UpdatedAt
	if v, ok := doc[kWorldFieldUpdatedAt].(float64); ok {
		res.UpdatedAt = int64(v)
	}

	// Likes
	if v, ok := doc[kWorldFieldLikes].(float64); ok {
		res.Likes = int32(v)
	}

	// Views
	if v, ok := doc[kWorldFieldViews].(float64); ok {
		res.Views = int32(v)
	}

	// Featured
	if v, ok := doc[kWorldFieldFeatured].(bool); ok {
		res.Featured = v
	}

	// Banned
	if v, ok := doc[kWorldFieldBanned].(bool); ok {
		res.Banned = v
	}

	// Archived
	if v, ok := doc[kWorldFieldArchived].(bool); ok {
		res.Archived = v
	}

	return res, nil
}
