package search

import (
	"context"
	"errors"
	"fmt"
	"net/http"
	"reflect"
	"slices"
	"strconv"
	"strings"
	"time"

	"cu.bzh/utils"
	"github.com/typesense/typesense-go/typesense"
	"github.com/typesense/typesense-go/typesense/api"
)

const (
	KCreationFieldType = "type"
	// fields common to all creations types
	KCreationFieldID             = "id"
	KCreationFieldName           = "name"
	KCreationFieldAuthorID       = "authorId"
	KCreationFieldAuthorName     = "authorName"
	KCreationFieldDescription    = "description"
	KCreationFieldCategory       = "category"
	KCreationFieldCreatedAt      = "createdAt"
	KCreationFieldUpdatedAt      = "updatedAt"
	KCreationFieldViews          = "views"
	KCreationFieldLikes          = "likes"
	KCreationFieldFeatured       = "featured"
	KCreationFieldBanned         = "banned"
	KCreationFieldArchived       = "archived"
	KCreationFieldItemFullname   = "item_fullname"
	KCreationFieldItemBlockCount = "item_blockCount"
	KCreationFieldItemType       = "item_type"

	// Creation types
	CreationTypeWorld = "world"
	CreationTypeItem  = "item"
	// CreationTypeTexture = "texture"
)

var (
	creationTypesAllowed = []string{CreationTypeWorld, CreationTypeItem}
)

// --------------------------------------------------
// Types
// --------------------------------------------------

// World type, as it is stored as a document in the typesense database
type Creation struct {
	// creation type (world, item, texture, ...)
	Type string `json:"type"`

	// common fields
	ID          string `json:"id"`
	Name        string `json:"name"`       // for world: title
	AuthorID    string `json:"authorId"`   // user ID of the author
	AuthorName  string `json:"authorName"` // username of the author (for item: repo)
	Description string `json:"description"`
	Category    string `json:"category"` // item: "hat", "head"
	CreatedAt   int64  `json:"createdAt"`
	UpdatedAt   int64  `json:"updatedAt"`
	Views       int32  `json:"views"`
	Likes       int32  `json:"likes"`
	Featured    bool   `json:"featured"`
	Banned      bool   `json:"banned"`
	Archived    bool   `json:"archived"`

	// item specific fields
	Item_Fullname   string `json:"item_fullname"` // TODO: check if this is needed
	Item_BlockCount int64  `json:"item_blockCount"`
	Item_Type       string `json:"item_type"`

	// world specific fields
	// none
}

// CreationUpdate ...
type CreationUpdate struct {
	ID string `json:"id"`
	// Type        *string `json:"type"` // cannot be updated
	Name        *string `json:"name"`
	AuthorID    *string `json:"authorId"`
	AuthorName  *string `json:"authorName"`
	Description *string `json:"description"`
	Category    *string `json:"category"`
	CreatedAt   *int64  `json:"createdAt"`
	UpdatedAt   *int64  `json:"updatedAt"`
	Views       *int32  `json:"views"`
	Likes       *int32  `json:"likes"`
	Featured    *bool   `json:"featured"`
	Banned      *bool   `json:"banned"`
	Archived    *bool   `json:"archived"`
	// Item specific fields
	Item_Fullname   *string `json:"item_fullname"` // TODO: check if this is needed
	Item_BlockCount *int64  `json:"item_blockCount"`
	Item_Type       *string `json:"item_type"`
}

func NewCreationUpdateFromWorldUpdate(wu WorldUpdate) CreationUpdate {
	return CreationUpdate{
		ID:         wu.ID,
		Name:       wu.Title,
		AuthorID:   wu.AuthorID,
		AuthorName: wu.AuthorName,
		CreatedAt:  wu.CreatedAt,
		UpdatedAt:  wu.UpdatedAt,
		Likes:      wu.Likes,
		Views:      wu.Views,
		Featured:   wu.Featured,
		Banned:     wu.Banned,
		Archived:   wu.Archived,
	}
}

func (c *Creation) String() string {
	return fmt.Sprintf(
		"Creation{Type: %s, ID: %s, Name: %s, AuthorID: %s, AuthorName: %s, Description: %s, Category: %s, CreatedAt: %d, UpdatedAt: %d, Views: %d, Likes: %d, Featured: %t, Banned: %t, Archived: %t, Item_Fullname: %s, Item_BlockCount: %d, Item_Type: %s}",
		c.Type, c.ID, c.Name, c.AuthorID, c.AuthorName, c.Description, c.Category, c.CreatedAt, c.UpdatedAt, c.Views, c.Likes, c.Featured, c.Banned, c.Archived, c.Item_Fullname, c.Item_BlockCount, c.Item_Type,
	)
}

func (c *Creation) Print() {
	str := ""

	str += fmt.Sprintf("| Creation (type: %s)\n", c.Type)
	str += fmt.Sprintf("| ID: %s\n", c.ID)
	str += fmt.Sprintf("| Name: %s\n", c.Name)
	str += fmt.Sprintf("| AuthorID: %s\n", c.AuthorID)
	str += fmt.Sprintf("| AuthorName: %s\n", c.AuthorName)
	str += fmt.Sprintf("| Description: %s\n", c.Description)
	str += fmt.Sprintf("| Category: %s\n", c.Category)
	str += fmt.Sprintf("| CreatedAt: %d\n", c.CreatedAt)
	str += fmt.Sprintf("| UpdatedAt: %d\n", c.UpdatedAt)
	str += fmt.Sprintf("| Views: %d\n", c.Views)
	str += fmt.Sprintf("| Likes: %d\n", c.Likes)
	str += fmt.Sprintf("| Featured: %t\n", c.Featured)
	str += fmt.Sprintf("| Banned: %t\n", c.Banned)
	str += fmt.Sprintf("| Archived: %t\n", c.Archived)

	if c.Type == CreationTypeItem {
		str += fmt.Sprintf("| Item_Fullname: %s\n", c.Item_Fullname)
		str += fmt.Sprintf("| Item_BlockCount: %d\n", c.Item_BlockCount)
		str += fmt.Sprintf("| Item_Type: %s\n", c.Item_Type)
	}

	fmt.Println(str)
}

// SearchCreationsResult is the result of a search in the Creation collection
type SearchCreationsResult struct {
	Results      []Creation
	TotalResults int
	Page         int
	PerPage      int
}

// --------------------------------------------------
// Exposed functions
// --------------------------------------------------

// NewCreationFromValues creates a new Creation struct using the provided values.
func NewCreationFromValues(creationType, id, name, authorID, authorName, description, category string, createdAt, updatedAt time.Time, views, likes int32, featured, banned, archived bool, itemFullname string, itemBlockCount int64, itemType string) (Creation, error) {

	// check if the creation type is allowed
	if !slices.Contains(creationTypesAllowed, creationType) {
		return Creation{}, fmt.Errorf("creation type %s is not supported", creationType)
	}

	if id == "" {
		return Creation{}, fmt.Errorf("id is empty")
	}
	if name == "" {
		return Creation{}, fmt.Errorf("name is empty")
	}
	if authorID == "" {
		return Creation{}, fmt.Errorf("authorID is empty")
	}
	if authorName == "" {
		fmt.Println("⚠️ authorName is empty", id, name, authorID)
		// return Creation{}, fmt.Errorf("authorName is empty")
	}
	// description is optional
	// category is optional except for item
	if creationType == CreationTypeItem && category == "" {
		fmt.Println("⚠️ category is empty", id, authorName, name, authorID)
		// return Creation{}, fmt.Errorf("category is required for item")
	}
	if createdAt.IsZero() {
		return Creation{}, fmt.Errorf("createdAt is required")
	}
	if updatedAt.IsZero() {
		return Creation{}, fmt.Errorf("updatedAt is required")
	}
	if views < 0 {
		return Creation{}, fmt.Errorf("views is negative")
	}
	if likes < 0 {
		return Creation{}, fmt.Errorf("likes is negative")
	}
	// item specific fields
	if creationType == CreationTypeItem {
		if itemFullname != authorName+"."+name {
			return Creation{}, fmt.Errorf("itemFullname is not valid")
		}
		if itemBlockCount < 0 {
			return Creation{}, fmt.Errorf("itemBlockCount is negative")
		}
		if itemType == "" {
			return Creation{}, fmt.Errorf("itemType is empty")
		}
	}

	creation := Creation{
		Type:            creationType,
		ID:              id,
		Name:            name,
		AuthorID:        authorID,
		AuthorName:      authorName,
		Description:     description,
		Category:        category,
		CreatedAt:       createdAt.UnixMicro(),
		UpdatedAt:       updatedAt.UnixMicro(),
		Likes:           likes,
		Views:           views,
		Featured:        featured,
		Banned:          banned,
		Archived:        archived,
		Item_Fullname:   itemFullname,
		Item_BlockCount: itemBlockCount,
		Item_Type:       itemType,
	}

	return creation, nil
}

// CreateCreationCollection ...
func (c *Client) CreateCreationCollection(force bool) error {

	// Remove collection if it already exists
	if c.CollectionExists(COLLECTION_CREATIONS) {
		if !force {
			return fmt.Errorf("collection already exists")
		}
		err := c.CollectionDelete(COLLECTION_CREATIONS)
		if err != nil {
			return err
		}
	}

	schema := &api.CollectionSchema{
		Name: COLLECTION_CREATIONS,
		Fields: []api.Field{
			{ // UUID
				Name: KCreationFieldID,
				Type: "string",
			},
			{
				Name:  KCreationFieldType,
				Type:  "string",
				Facet: utils.NewTrue(),
			},
			{
				Name: KCreationFieldName,
				Type: "string",
			},
			{
				Name: KCreationFieldAuthorID,
				Type: "string",
			},
			{
				Name: KCreationFieldAuthorName,
				Type: "string",
			},
			{
				Name:     KCreationFieldDescription,
				Type:     "string",
				Optional: utils.NewTrue(),
			},
			{
				Name:     KCreationFieldCategory,
				Type:     "string",
				Optional: utils.NewTrue(),
				Facet:    utils.NewTrue(),
			},
			{
				Name: KCreationFieldCreatedAt,
				Type: "int64",
			},
			{
				Name: KCreationFieldUpdatedAt,
				Type: "int64",
			},
			{
				Name: KCreationFieldViews,
				Type: "int32",
			},
			{
				Name: KCreationFieldLikes,
				Type: "int32",
			},
			{
				Name: KCreationFieldFeatured,
				Type: "bool",
			},
			{
				Name: KCreationFieldBanned,
				Type: "bool",
			},
			{
				Name: KCreationFieldArchived,
				Type: "bool",
			},
			// Item specific fields
			{
				Name:     KCreationFieldItemFullname,
				Type:     "string",
				Optional: utils.NewTrue(),
			},
			{
				Name:     KCreationFieldItemBlockCount,
				Type:     "int64",
				Optional: utils.NewTrue(),
			},
			{
				Name:     KCreationFieldItemType,
				Type:     "string",
				Optional: utils.NewTrue(),
			},
		},
	}
	_ /* CollectionResponse */, err := c.TSClient.Collections().Create(context.Background(), schema)
	return err
}

// UpsertCreation updates an entire Creation, or inserts it if it doesn't already exist in the database.
func (c *Client) UpsertCreation(creation Creation) error {
	_, err := c.TSClient.Collection(COLLECTION_CREATIONS).Documents().Upsert(context.Background(), creation)
	return err
}

// UpdateCreation updates a Creation. It can be a partial update.
func (c *Client) UpdateCreation(cu CreationUpdate) error {
	if cu.ID == "" {
		return errors.New("creation ID is empty")
	}

	// Loop on all the fields of the CreationUpdate struct using reflection
	updateData := make(map[string]interface{})

	// Get the type of the struct
	structType := reflect.TypeOf(cu)

	// Get the value of the struct
	structValue := reflect.ValueOf(cu)

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

	_, err := c.TSClient.Collection(COLLECTION_CREATIONS).Document(cu.ID).Update(context.Background(), updateData)
	// TODO: (gdevillele) remove this dirty fix when the typesense client is fixed
	//                    PR: https://github.com/typesense/typesense-go/pull/151
	if err != nil && strings.HasPrefix(err.Error(), "status: 201") {
		err = nil
	}

	return err
}

func (c *Client) DeleteCreation(ID string) (found bool, err error) {
	// consider it found by default
	// (if the document is not found, it will be set to false below)
	found = true

	// try deleting the document in the collection
	_, err = c.TSClient.Collection(COLLECTION_CREATIONS).Document(ID).Delete(context.Background())
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

// ID parameter is the ID of the creation (it can be a world ID or an item ID)
func (c *Client) GetCreationByID(ID string) (creation Creation, err error) {
	if ID == "" {
		return Creation{}, fmt.Errorf("ID is empty")
	}

	var doc map[string]interface{}
	doc, err = c.TSClient.Collection(COLLECTION_CREATIONS).Document(ID).Retrieve(context.Background())
	if err != nil {
		return
	}
	creation, err = newCreationFromDocument(&doc)
	if err != nil {
		return
	}

	return
}

type CreationFilter struct {
	Type       *string
	AuthorID   *string
	AuthorName *string
	Category   *string
	Featured   *bool
	Banned     *bool
	Archived   *bool
	MinBlock   *int64  // for "item" type creations only
	ItemType   *string // for "item" type creations only
}

// SearchCollectionCreations ...
func (c *Client) SearchCollectionCreations(searchText string, sortBy string, page, perPage int, filter CreationFilter) (result SearchCreationsResult, err error) {
	fmt.Println("🔎 SearchCollectionCreations", filter)

	err = nil

	searchParameters := &api.SearchCollectionParams{
		Q:       searchText,
		QueryBy: KCreationFieldName + ", " + KCreationFieldCategory + ", " + KCreationFieldAuthorName + ", " + KCreationFieldDescription,
		// QueryByWeights: "2, 2, 1, 1, 1",
		SortBy:   &sortBy,
		PerPage:  &perPage,
		Page:     &page,
		FilterBy: nil,
	}

	// construct "FilterBy" string
	{
		var filterByStr string = ""
		var first bool = true // is true only for the first filter
		if filter.Type != nil && len(*filter.Type) > 0 {
			if !first {
				filterByStr += " && "
			}
			filterByStr += KCreationFieldType + ":=" + *filter.Type
			first = false
		}
		if filter.AuthorID != nil && len(*filter.AuthorID) > 0 {
			if !first {
				filterByStr += " && "
			}
			filterByStr += KCreationFieldAuthorID + ":=" + *filter.AuthorID
			first = false
		}
		if filter.AuthorName != nil && len(*filter.AuthorName) > 0 {
			if !first {
				filterByStr += " && "
			}
			filterByStr += KCreationFieldAuthorName + ":=" + *filter.AuthorName
			first = false
		}
		if filter.Category != nil && len(*filter.Category) > 0 {
			if !first {
				filterByStr += " && "
			}
			filterByStr += KCreationFieldCategory + ":=" + *filter.Category
			first = false
		}
		if filter.Featured != nil {
			if !first {
				filterByStr += " && "
			}
			filterByStr += KCreationFieldFeatured + ":=" + strconv.FormatBool(*filter.Featured)
			first = false
		}
		if filter.Banned != nil {
			if !first {
				filterByStr += " && "
			}
			filterByStr += KCreationFieldBanned + ":=" + strconv.FormatBool(*filter.Banned)
			first = false
		}
		if filter.Archived != nil {
			if !first {
				filterByStr += " && "
			}
			filterByStr += KCreationFieldArchived + ":=" + strconv.FormatBool(*filter.Archived)
			first = false
		}
		if filter.MinBlock != nil {
			if !first {
				filterByStr += " && "
			}
			// when filtering on minBlock, we allow items having enough blocks but also items with 0 blocks (they are "mesh" items)
			filterByStr += fmt.Sprintf("%s:[0, >=%s]", KCreationFieldItemBlockCount, strconv.FormatInt(*filter.MinBlock, 10))
			first = false
		}
		if filter.ItemType != nil && len(*filter.ItemType) > 0 {
			if !first {
				filterByStr += " && "
			}
			filterByStr += KCreationFieldItemType + ":=" + *filter.ItemType
			first = false
		}
		if len(filterByStr) > 0 {
			searchParameters.FilterBy = &filterByStr
		}
	}

	// fmt.Println("✨ filterBy:", searchParameters.FilterBy)

	var searchResult *api.SearchResult = nil
	searchResult, err = c.TSClient.Collection(COLLECTION_CREATIONS).Documents().Search(context.Background(), searchParameters)
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

	var creation Creation
	for _, hit := range hits {
		creation, err = newCreationFromDocument(hit.Document)
		if err != nil {
			return
		}
		result.Results = append(result.Results, creation)
	}

	return
}

// --------------------------------------------------
// Unexposed functions
// --------------------------------------------------

func newCreationFromDocument(docPtr *map[string]interface{}) (Creation, error) {
	var res = Creation{}

	// make sure docPtr is not nil
	if docPtr == nil {
		return res, ErrDocumentIsNil
	}
	doc := *docPtr

	// Parse the document and fill the Creation struct
	// TODO: gaetan: add checks

	// Type
	if v, ok := doc[KCreationFieldType].(string); ok {
		res.Type = v
	}

	// ID
	if v, ok := doc[KCreationFieldID].(string); ok {
		res.ID = v
	}

	// Name
	if v, ok := doc[KCreationFieldName].(string); ok {
		res.Name = v
	}

	// AuthorID
	if v, ok := doc[KCreationFieldAuthorID].(string); ok {
		res.AuthorID = v
	}

	// AuthorName
	if v, ok := doc[KCreationFieldAuthorName].(string); ok {
		res.AuthorName = v
	}

	// Description
	if v, ok := doc[KCreationFieldDescription].(string); ok {
		res.Description = v
	}

	// Category
	if v, ok := doc[KCreationFieldCategory].(string); ok {
		res.Category = v
	}

	// CreatedAt
	if v, ok := doc[KCreationFieldCreatedAt].(float64); ok {
		res.CreatedAt = int64(v)
	}

	// UpdatedAt
	if v, ok := doc[KCreationFieldUpdatedAt].(float64); ok {
		res.UpdatedAt = int64(v)
	}

	// Views
	if v, ok := doc[KCreationFieldViews].(float64); ok {
		res.Views = int32(v)
	}

	// Likes
	if v, ok := doc[KCreationFieldLikes].(float64); ok {
		res.Likes = int32(v)
	}

	// Featured
	if v, ok := doc[KCreationFieldFeatured].(bool); ok {
		res.Featured = v
	}

	// Banned
	if v, ok := doc[KCreationFieldBanned].(bool); ok {
		res.Banned = v
	}

	// Archived
	if v, ok := doc[KCreationFieldArchived].(bool); ok {
		res.Archived = v
	}

	// Item specific fields
	if v, ok := doc[KCreationFieldItemFullname].(string); ok {
		res.Item_Fullname = v
	}

	if v, ok := doc[KCreationFieldItemBlockCount].(float64); ok {
		res.Item_BlockCount = int64(v)
	}

	if v, ok := doc[KCreationFieldItemType].(string); ok {
		res.Item_Type = v
	}

	return res, nil
}
