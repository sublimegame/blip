package item

import (
	"time"
	"utils"
)

// SortMethod is an enum of different ways to sort games
type SortMethod string

const (
	sortMethodNew     SortMethod = "new"     // recently created
	sortMethodRecent  SortMethod = "recent"  // recently updated
	sortMethodPopular SortMethod = "popular" // popular ones
	sortMethodFriends SortMethod = "friends" // created by friends
	sortMethodMine    SortMethod = "mine"    // games owned by user
)

// supported sort methods
var supportedSortMethods = map[string]bool{"mine": true}

func isSortMethodSupported(method string) bool {
	_, supported := supportedSortMethods[method]
	return supported
}

// Item represents an game item
type Item struct {
	ID string `bson:"id" json:"id,omitempty"`

	// Author is the id of the User who created the item
	Author string `bson:"author" json:"author,omitempty"`

	// Name
	Name string `bson:"name" json:"name,omitempty"`

	// Repo
	// repo usually == author's username
	Repo string `bson:"repo" json:"repo,omitempty"`

	Created time.Time `bson:"created" json:"created,omitempty"`
	Updated time.Time `bson:"updated" json:"updated,omitempty"`

	// IsNew means "not saved in database"
	IsNew bool `bson:"-" json:"-"`

	// ReadOnly is used to make sure the Game object is never saved
	ReadOnly bool `bson:"-" json:"-"`
}

// NewReadOnly can be used to execute quick read-only actions
func NewReadOnly(ID string) *Game {
	return &Item{ID: ID, ReadOnly: true}
}

// New creates a new item, with empty ID
func New() (*Item, error) {
	now := time.Now()
	uuid, err := utils.UUID()
	if err != nil {
		return nil, err
	}
	return &Item{ID: uuid, Name: "", Repo: "", Created: now, Updated: now, IsNew: true}, nil
}
