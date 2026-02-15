package search

import (
	"context"
	"errors"
	"fmt"
	"net/http"
	"strconv"
	"time"

	"github.com/typesense/typesense-go/typesense"
	"github.com/typesense/typesense-go/typesense/api"

	"cu.bzh/utils"
)

const (
	KItemDraftFieldID          = "id"
	KItemDraftFieldName        = "name"
	KItemDraftFieldRepo        = "repo" // author name
	KItemDraftFieldType        = "type" // "voxel" or "mesh" (more later?)
	KItemDraftFieldDescription = "description"
	KItemDraftFieldCategory    = "category" // "hat" or "head" ...
	KItemDraftFieldCreatedAt   = "createdAt"
	KItemDraftFieldUpdatedAt   = "updatedAt"
	KItemDraftFieldViews       = "views"
	KItemDraftFieldLikes       = "likes"
	KItemDraftFieldBanned      = "banned"
	KItemDraftFieldArchived    = "archived"

	// item specific fields
	KItemDraftFieldFullname   = "fullname"
	KItemDraftFieldBlockCount = "blockCount"
)

// --------------------------------------------------
// Types
// --------------------------------------------------

// ItemDraft type, as it is stored as a document in the typesense database
type ItemDraft struct {
	ID          string `json:"id"`
	Name        string `json:"name"`
	Repo        string `json:"repo"`
	Type        string `json:"type"` // "voxel" or "mesh" (more later?)
	Description string `json:"description"`
	Category    string `json:"category"`
	CreatedAt   int64  `json:"createdAt"`
	UpdatedAt   int64  `json:"updatedAt"`
	Views       int32  `json:"views"`
	Likes       int32  `json:"likes"`
	Banned      bool   `json:"banned"`
	Archived    bool   `json:"archived"`
	//
	Fullname   string `json:"fullname"`
	BlockCount int64  `json:"blockCount"`
}

// TODO: gaetan: create type ItemDraftUpdate (on the same model as search.WorldUpdate)

// SearchItemDraftsResult is the result of a search in the ItemDrafts collection
type SearchItemDraftsResult struct {
	Results      []ItemDraft
	TotalResults int
	Page         int
	PerPage      int
}

// --------------------------------------------------
// Exposed functions
// --------------------------------------------------

// NewItemDraftFromValues creates a new ItemDraft struct using the provided values.
func NewItemDraftFromValues(id, repo, name, itemType, description, category string, createdAt, updatedAt time.Time, blockCount int64, banned, archived bool, likes int32, views int32) ItemDraft {
	itemDraft := ItemDraft{
		ID:          id,
		Repo:        repo,
		Name:        name,
		Fullname:    repo + "." + name,
		Type:        itemType,
		Description: description,
		Category:    category,
		CreatedAt:   createdAt.UnixMicro(),
		UpdatedAt:   updatedAt.UnixMicro(),
		BlockCount:  blockCount,
		Banned:      banned,
		Archived:    archived,
		Likes:       likes,
		Views:       views,
	}
	if itemDraft.Category == "" {
		// allowing filtering on null category
		itemDraft.Category = "null"
	}
	return itemDraft
}

func PrintItemDraft(itm ItemDraft) {
	fmt.Println("| Item Draft")
	fmt.Println("|-------------")
	fmt.Println("|", itm.ID)
	fmt.Println("|", itm.Repo)
	fmt.Println("|", itm.Name)
	fmt.Println("|", itm.Fullname)
	fmt.Println("|", itm.Type)
	fmt.Println("|", itm.Description)
	fmt.Println("|", itm.Category)
	fmt.Println("|", itm.CreatedAt)
	fmt.Println("|", itm.UpdatedAt)
	fmt.Println("|", itm.BlockCount)
	fmt.Println("|", itm.Banned)
	fmt.Println("|", itm.Archived)
	fmt.Println("|", itm.Likes)
	fmt.Println("|", itm.Views)
	fmt.Println("|-------------")
}

// CreateItemDraftCollection ...
func (c *Client) CreateItemDraftCollection() error {

	// Remove collection if it already exists
	if c.CollectionExists(COLLECTION_ITEMDRAFTS) {
		err := c.CollectionDelete(COLLECTION_ITEMDRAFTS)
		if err != nil {
			return err
		}
	}

	schema := &api.CollectionSchema{
		Name: COLLECTION_ITEMDRAFTS,
		Fields: []api.Field{
			{ // UUID
				Name: KItemDraftFieldID,
				Type: "string",
			},
			{
				Name: KItemDraftFieldRepo,
				Type: "string",
				// Facet: newTrue(),
			},
			{
				Name: KItemDraftFieldName,
				Type: "string",
			},
			{ // <repo>.<name>
				Name: KItemDraftFieldFullname,
				Type: "string",
			},
			{
				Name: KItemDraftFieldType,
				Type: "string",
			},
			{
				Name: KItemDraftFieldDescription,
				Type: "string",
			},
			{ // "hat" or "head" ...
				Name:  KItemDraftFieldCategory,
				Type:  "string",
				Facet: utils.NewTrue(),
			},
			{
				Name: KItemDraftFieldCreatedAt,
				Type: "int64",
			},
			{
				Name: KItemDraftFieldUpdatedAt,
				Type: "int64",
			},
			{
				Name: KItemDraftFieldBlockCount,
				Type: "int64",
			},
			{
				Name: KItemDraftFieldBanned,
				Type: "bool",
			},
			{
				Name: KItemDraftFieldArchived,
				Type: "bool",
			},
			{
				Name: KItemDraftFieldLikes,
				Type: "int32",
			},
			{
				Name: KItemDraftFieldViews,
				Type: "int32",
			},
		},
	}
	_ /* CollectionResponse */, err := c.TSClient.Collections().Create(context.Background(), schema)
	return err
}

// UpsertItemDraft updates, or inserts if it doesn't already exist, an ItemDraft record in the search database.
func (c *Client) UpsertItemDraft(itemDraft ItemDraft) error {
	// insert of update the document in the collection
	_ /* document */, err := c.TSClient.Collection(COLLECTION_ITEMDRAFTS).Documents().Upsert(context.Background(), itemDraft)
	return err
}

// UpdateItemDraft updates an ItemDraft record in the search database.
// It updates all the ItemDraft fields at once.
// For now it's not possible to perform a partial update.
func (c *Client) UpdateItemDraft(itemDraft ItemDraft) error {
	// update the document in the collection
	_ /* document */, err := c.TSClient.Collection(COLLECTION_ITEMDRAFTS).Document(itemDraft.ID).Update(context.Background(), itemDraft)
	if err != nil {
		httpErr, ok := err.(*typesense.HTTPError)
		if ok && httpErr.Status == http.StatusCreated { // HTTP 201
			err = nil
		}
	}
	return err
}

// DeleteItemDraft delete an ItemDraft record in the search database.
func (c *Client) DeleteItemDraft(ID string) (found bool, err error) {
	// consider it found by default
	// (if the document is not found, it will be set to false below)
	found = true

	// try deleting the document in the collection
	_, err = c.TSClient.Collection(COLLECTION_ITEMDRAFTS).Document(ID).Delete(context.Background())
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

type ItemDraftFilter struct {
	MinBlockCount *int
	Banned        *bool
	Archived      *bool
	Type          *string
	Categories    []string
	Repos         []string
}

func (c *Client) GetItemDraftByID(ID string) (ItemDraft, error) {
	// get the document in the collection
	doc, err := c.TSClient.Collection(COLLECTION_ITEMDRAFTS).Document(ID).Retrieve(context.Background())
	if err != nil {
		return ItemDraft{}, err
	}
	return newItemDraftFromDocument(&doc)
}

// SearchCollectionItemDrafts ...
func (c *Client) SearchCollectionItemDrafts(searchText, sortBy string, page, perPage int, filter ItemDraftFilter) (result SearchItemDraftsResult, err error) {
	err = nil

	searchParameters := &api.SearchCollectionParams{
		Q:       searchText,
		QueryBy: "repo, name, fullname, description, category",
		// QueryByWeights: "2, 2, 1, 1, 1",
		SortBy:  &sortBy,
		PerPage: &perPage,
		Page:    &page,
	}

	// Filter By
	{
		var filterByStr string = ""
		var first bool = true                                          // is true only for the first filter
		if filter.MinBlockCount != nil && *filter.MinBlockCount >= 0 { // minBlock
			var minBlockValue int = *filter.MinBlockCount
			var minBlockStr string = strconv.FormatInt(int64(minBlockValue), 10)
			if !first {
				filterByStr += " && "
			}
			// when filtering on minBlock, we allow items having enough blocks but also items with 0 blocks (they are "mesh" items)
			filterByStr += fmt.Sprintf("%s:[0, >=%s]", KItemDraftFieldBlockCount, minBlockStr)
			first = false
		}
		if filter.Banned != nil {
			if !first {
				filterByStr += " && "
			}
			filterByStr += KItemDraftFieldBanned + ":=" + strconv.FormatBool(*filter.Banned)
			first = false
		}
		if filter.Archived != nil {
			if !first {
				filterByStr += " && "
			}
			filterByStr += KItemDraftFieldArchived + ":=" + strconv.FormatBool(*filter.Archived)
			first = false
		}
		if filter.Type != nil {
			if !first {
				filterByStr += " && "
			}
			filterByStr += KItemDraftFieldType + ":=" + *filter.Type
			first = false
		}
		if len(filter.Categories) > 0 {
			if !first {
				filterByStr += " && "
			}
			if len(filter.Categories) == 1 {
				filterByStr += KItemDraftFieldCategory + ":=" + filter.Categories[0]
			} else {
				filterByStr += KItemDraftFieldCategory + ": ["
				for i, category := range filter.Categories {
					if i > 0 {
						filterByStr += ", "
					}
					filterByStr += category
				}
				filterByStr += "]"
			}
			first = false
		}
		if len(filter.Repos) > 0 {
			if !first {
				filterByStr += " && "
			}
			if len(filter.Repos) == 1 {
				filterByStr += KItemDraftFieldRepo + ":=" + filter.Repos[0]
			} else {
				filterByStr += KItemDraftFieldRepo + ": ["
				for i, repo := range filter.Repos {
					if i > 0 {
						filterByStr += ", "
					}
					filterByStr += repo
				}
				filterByStr += "]"
			}
			first = false
		}
		if len(filterByStr) > 0 {
			searchParameters.FilterBy = &filterByStr
		}
	}

	var searchResult *api.SearchResult = nil
	searchResult, err = c.TSClient.Collection(COLLECTION_ITEMDRAFTS).Documents().Search(context.Background(), searchParameters)
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

	var itemDraft ItemDraft
	for _, hit := range hits {
		itemDraft, err = newItemDraftFromDocument(hit.Document)
		if err != nil {
			return
		}
		result.Results = append(result.Results, itemDraft)
	}

	return
}

// --------------------------------------------------
// Unexposed functions
// --------------------------------------------------

func newItemDraftFromDocument(docPtr *map[string]interface{}) (ItemDraft, error) {
	var res = ItemDraft{}
	var fieldName string

	// make sure docPtr is not nil
	if docPtr == nil {
		return res, ErrDocumentIsNil
	}
	doc := *docPtr

	// ID
	if v, ok := doc[KItemDraftFieldID].(string); ok {
		res.ID = v
	}

	// Repo
	if v, ok := doc[KItemDraftFieldRepo].(string); ok {
		res.Repo = v
	}

	// Name
	if v, ok := doc[KItemDraftFieldName].(string); ok {
		res.Name = v
	}

	// Fullname
	if v, ok := doc[KItemDraftFieldFullname].(string); ok {
		res.Fullname = v
	}

	// Type
	if v, ok := doc[KItemDraftFieldType].(string); ok {
		res.Type = v
	}

	// Description
	if v, ok := doc[KItemDraftFieldDescription].(string); ok {
		res.Description = v
	}

	// Category
	if v, ok := doc[KItemDraftFieldCategory].(string); ok {
		if v == "null" {
			v = ""
		}
		res.Category = v
	}

	// CreatedAt
	if v, ok := doc[KItemDraftFieldCreatedAt].(float64); ok {
		res.CreatedAt = int64(v)
	}

	// UpdatedAt
	if v, ok := doc[KItemDraftFieldUpdatedAt].(float64); ok {
		res.UpdatedAt = int64(v)
	}

	// BlockCount
	{
		fieldName = KItemDraftFieldBlockCount
		val, ok := doc[fieldName].(float64)
		if !ok {
			return res, newTypeAssertErr(fieldName)
		}
		res.BlockCount = int64(val)
	}

	// Banned
	{
		fieldName = KItemDraftFieldBanned
		val, ok := doc[fieldName].(bool)
		if !ok {
			return res, newTypeAssertErr(fieldName)
		}
		res.Banned = val
	}

	// Likes
	if v, ok := doc[KItemDraftFieldLikes].(float64); ok {
		res.Likes = int32(v)
	}

	// Views
	if v, ok := doc[KItemDraftFieldViews].(float64); ok {
		res.Views = int32(v)
	}

	return res, nil
}
