package item

import (
	"bytes"
	"errors"
	"fmt"
	"os"
	"os/exec"
	"path/filepath"
	"strconv"
	"strings"
	"time"

	"cu.bzh/hub/dbclient"
	"cu.bzh/hub/search"
	"cu.bzh/hub/user"
	"cu.bzh/utils"
)

// --------------------------------------------------
// TYPES
// --------------------------------------------------

// ItemFilter is used to filter Items in certain DB requests.
type ItemFilter struct {
	Repo     string // filter is active if string is non-empty
	Category string // filter is active if string is non-empty
	// ...
}

func IsCategory(str string) bool {
	return str == string(categoryHead) ||
		str == string(categoryBody) ||
		str == string(categoryLeftArm) ||
		str == string(categoryLeftLeg) ||
		str == string(categoryRightArm) ||
		str == string(categoryRightLeg) ||
		str == string(categoryLeftFoot) ||
		str == string(categoryRightFoot) ||
		str == string(categoryLeftHand) ||
		str == string(categoryRightHand) ||
		str == string(categoryHair) ||
		str == string(categoryTop) ||
		str == string(categoryLeftSleeve) ||
		str == string(categoryRightSleeve) ||
		str == string(categoryLeftGlove) ||
		str == string(categoryRightGlove) ||
		str == string(categoryLeftPant) ||
		str == string(categoryRightPant) ||
		str == string(categoryLeftBoot) ||
		str == string(categoryRightBoot) ||
		str == string(categoryBoots) || // v3
		str == string(categoryJacket) || // v3
		str == string(categoryPants) || // v3
		str == string(categoryShirt) // v3
}

func IsItemType(str string) bool {
	return str == string(itemTypeMesh) || str == string(itemTypeVoxels) || str == ""
}

// supported list filters
var supportedLists = map[string]bool{"new": true, "recent": true, "mine": true, "featured": true}

func isListSupported(list string) bool {
	_, supported := supportedLists[list]
	return supported
}

// Item type
type Item struct {
	ID string `bson:"id" json:"id,omitempty"`

	// Author is the ID of the User who created the game
	Author string `bson:"author" json:"author,omitempty"`

	// Contributors represents other users allowed to edit the script
	Contributors []string `bson:"contributors" json:"contributors,omitempty"`

	// Tags represents the tags that the Item has
	Tags []string `bson:"tags" json:"tags,omitempty"`

	// Repo ...
	Repo string `bson:"repo" json:"repo,omitempty"`

	// Name ...
	Name string `bson:"name" json:"name,omitempty"`

	// Public ...
	Public bool `bson:"public" json:"public,omitempty"`

	// Description ...
	Description string `bson:"description" json:"description,omitempty"`

	// Items can have internal scripts, empty by default
	Script string `bson:"script" json:"script,omitempty"`

	// True if the item has been featured
	Featured bool `bson:"featured" json:"featured,omitempty"`

	// Number of likes
	Likes int32 `bson:"likes" json:"likes,omitempty"`

	// Number of views (not fully supported yet)
	Views int32 `bson:"-" json:"-"`

	// Indicates if user liked the item or not.
	// This is different for each user, not saved in the database.
	Liked bool `bson:"-" json:"-"`

	Created time.Time `bson:"created" json:"created,omitempty"`
	Updated time.Time `bson:"updated" json:"updated,omitempty"`

	// IsNew means "not saved in database"
	IsNew bool `bson:"-" json:"-"`

	// ReadOnly is used to make sure the Item object is never saved
	ReadOnly bool `bson:"-" json:"-"`

	// "hair", "head", "body", ...
	// Can be empty ("") for regular items.
	Category string `bson:"category" json:"category,omitempty"`

	// "mesh", "voxels" (empty = "voxels" for legacy items)
	ItemType string `bson:"type" json:"type,omitempty"`

	// count of blocks in the item file (.3zh)
	BlockCount uint32 `bson:"blockCount" json:"blockCount,omitempty"`

	// Whether the item is banned
	Banned bool `bson:"ban" json:"ban,omitempty"`

	// An archived Item is not shown in the "Gallery" nor the "My Items" list
	Archived bool `bson:"archived" json:"archived,omitempty"`
}

// ItemLike represents one like for a game
type ItemLike struct {
	ItemID  string    `bson:"item" json:"game,omitempty"`
	UserID  string    `bson:"user" json:"user,omitempty"`
	Created time.Time `bson:"created" json:"created,omitempty"`
}

// NewReadOnly can be used to execute quick read-only actions like obtaining .pcubes
func NewReadOnly(ID string) *Item {
	return &Item{ID: ID, ReadOnly: true}
}

// New creates a new item, with generated ID
func New(repo, name, authorID, itemType, category string, bypassReservedNames bool) (*Item, error) {
	if repo == "" {
		return nil, errors.New("repo can't be empty")
	}
	if name == "" {
		return nil, errors.New("name can't be empty")
	}
	if bypassReservedNames == false && (IsItemNameForBodyPart(name) || IsItemNameForEquipment(name)) {
		return nil, errors.New("name cannot be a reserved name")
	}
	if authorID == "" {
		return nil, errors.New("author ID can't be empty")
	}
	if category != "" && IsCategory(category) == false {
		return nil, errors.New("category is not valid")
	}
	if IsItemType(itemType) == false {
		return nil, errors.New("item type is not valid")
	}

	now := time.Now()
	uuid, err := utils.UUID()
	if err != nil {
		return nil, err
	}
	return &Item{
		ID:         uuid,
		Repo:       repo,
		Name:       name,
		Author:     authorID,
		Public:     false,
		Script:     "",
		Created:    now,
		Updated:    now,
		IsNew:      true,
		Category:   category,
		ItemType:   itemType,
		BlockCount: 0, // now, an item can have 0 blocks (if it's a mesh for instance)
		Banned:     false,
		Archived:   false,
	}, nil
}

// LockByAuthor acquires a lock on the item having the provided name.
// Also checks the provided userID matches
// func LockByAuthor(dbClient *dbclient.Client, itemRepo, itemName, authorID string) (string, error) {
// 	// TODO: implement me!
// 	return "", nil
// }

// ListAllWithFilter lists all items, considering the provided filter
func ListAllWithFilter(dbClient *dbclient.Client, itemFilter ItemFilter) ([]*Item, error) {
	var results []*Item

	filter := make(map[string]interface{})
	if len(itemFilter.Category) > 0 { // filter is active if string is non-empty
		filter["category"] = itemFilter.Category
	}
	if len(itemFilter.Repo) > 0 { // filter is active if string is non-empty
		filter["repo"] = itemFilter.Repo
	}

	findOptions := &dbclient.FindOptions{}
	findOptions.SetLimit(100) // default limit

	err := dbClient.GetRecordsMatchingFilter(dbclient.ItemCollectionName, filter, findOptions, &results)
	return results, err
}

// GetByID returns the Item having the provided ID, if it exists.
// - `requesterUserID` is optional, it can remain empty
func GetByID(dbClient *dbclient.Client, itemID string, requesterUserID string) (*Item, error) {

	result := &Item{}
	found, err := dbClient.GetSingleRecordMatchingKeyValue(dbclient.ItemCollectionName, "id", itemID, result)
	if err != nil {
		return nil, err
	}
	if found == false {
		return nil, errors.New("item not found")
	}

	if requesterUserID != "" {
		like := &ItemLike{}
		filter := make(map[string]interface{})
		filter["item"] = itemID
		filter["user"] = requesterUserID
		found, err = dbClient.GetSingleRecord(dbclient.ItemLikeCollectionName, filter, like)
		if err != nil {
			return nil, err
		}
		result.Liked = found
	}

	return result, nil
}

// GetByRepoAndName ...
// Can return several items if there are several versions.
func GetByRepoAndName(dbClient *dbclient.Client, repo string, name string) ([]*Item, error) {
	items := make([]*Item, 0)
	filter := make(map[string]interface{})
	filter["repo"] = repo
	filter["name"] = name
	err := dbClient.GetRecordsMatchingFilter(dbclient.ItemCollectionName, filter, nil, &items)
	return items, err
}

func GetByAuthorID(dbClient *dbclient.Client, authorID string) ([]*Item, error) {
	items := make([]*Item, 0)
	filter := make(map[string]interface{})
	filter["author"] = authorID
	err := dbClient.GetRecordsMatchingFilter(dbclient.ItemCollectionName, filter, nil, &items)
	return items, err
}

// GetByRepo
// Return items for a specific Repo
func GetByRepo(dbClient *dbclient.Client, repo string) ([]*Item, error) {
	items := make([]*Item, 0)
	filter := make(map[string]interface{})
	filter["repo"] = repo
	err := dbClient.GetRecordsMatchingFilter(dbclient.ItemCollectionName, filter, nil, &items)
	return items, err
}

// GetAll returns all users stored in the database
func GetAll(dbClient *dbclient.Client) ([]*Item, error) {
	items := make([]*Item, 0)
	err := dbClient.GetAllRecords(dbclient.ItemCollectionName, &items)
	return items, err
}

// List lists items
func List(dbClient *dbclient.Client, filterStr string, userID string) ([]*Item, error) {
	var results []*Item

	filter := make(map[string]interface{})
	findOptions := &dbclient.FindOptions{}
	// default limit
	findOptions.SetLimit(100)
	// default sort
	sort := make(map[string]interface{})
	sort["updated"] = -1
	findOptions.SetSort(sort)

	// gdevillele: I disabled the projection (field selection)
	// findOptions.SetProjection(bson.M{
	// 	"id":     1,
	// 	"author": 1,
	// 	// "contributors": 1,
	// 	// "tags": 1,
	// 	"repo":        1,
	// 	"name":        1,
	// 	"public":      1,
	// 	"description": 1,
	// 	"script":      1, // ???
	// 	"featured":    1,
	// 	"created":     1,
	// 	"updated":     1,
	// })

	list := listNone
	f := utils.NewFilter(filterStr)
	if l, exists := f.Params["list"]; exists {
		if isListSupported(l) == false {
			return nil, errors.New("list filter not supported: " + l)
		}
		list = listFilter(l)
	}

	switch list {
	case listNew:
		sort = make(map[string]interface{})
		sort["created"] = -1
		findOptions.SetSort(sort)
	case listRecent:
		sort = make(map[string]interface{})
		sort["updated"] = -1
		findOptions.SetSort(sort)
	case listMine:
		filter["author"] = userID
	case listFeatured:
		filter["featured"] = true
		sort = make(map[string]interface{})
		sort["updated"] = -1
		findOptions.SetSort(sort)
	}

	if f.String != "" {
		// regex option is better in that situation
		// "rock" search will return "rock", "rock123", "big_rock"
		// with $text index, words have to match exactly.
		// filter = append(filter, bson.E{"$text", bson.M{"$search": f.String}})
		filter["name"] = dbclient.Regex{Pattern: ".*" + f.String + ".*", Options: "i"}
	}

	err := dbClient.GetRecordsMatchingFilter(dbclient.ItemCollectionName, filter, findOptions, &results)
	return results, err
}

func ListAllByCategory(dbClient *dbclient.Client, category string) ([]*Item, error) {
	var results []*Item

	filter := make(map[string]interface{})
	filter["category"] = category

	findOptions := &dbclient.FindOptions{}
	// findOptions.SetLimit(100) // default limit

	err := dbClient.GetRecordsMatchingFilter(dbclient.ItemCollectionName, filter, findOptions, &results)
	return results, err
}

// ListAllByName is not used currently
// func ListAllByName(dbClient *dbclient.Client, itemName string) ([]*Item, error) {
// 	var results []*Item
// 	return results, errors.New("NOT IMPLEMENTED")
// }

// Save writes the game in database, in given games collection.
// Making sure the author and other referenced user can be
// found in the users collection.
func (i *Item) Save(dbClient *dbclient.Client) error {

	if i.ReadOnly {
		return errors.New("read only item instance can't be saved")
	}

	if i.Author == "" {
		return errors.New("author can't be empty")
	}

	u, err := user.GetByID(dbClient, i.Author)
	if err != nil {
		return errors.New("author not found")
	}

	if u == nil {
		return errors.New("author not found")
	}

	// UPDATE
	if i.IsNew == false {
		err := i.update(dbClient, true /* refreshUpdatedAt */)
		if err != nil {
			fmt.Println("Item.Save update error:", err.Error())
			return err
		}
	} else { // INSERT
		err := i.insert(dbClient)
		if err != nil {
			fmt.Println("Item.Save insert error:", err.Error())
			return err
		}
		i.IsNew = false
	}

	return nil
}

type ItemDeleteOpts struct {
	TypesenseClient *search.Client
	ItemStoragePath string
}

func (i *Item) Delete(dbClient *dbclient.Client, opts ItemDeleteOpts) error {
	if dbClient == nil {
		return errors.New("database client is nil")
	}

	filter := make(map[string]interface{})
	filter["id"] = i.ID

	// delete item from MongoDB database
	found, err := dbClient.DeleteSingleRecordMatchingFilter(dbclient.ItemCollectionName, filter)
	if err != nil {
		return err
	}
	if found == false {
		return errors.New("item not found")
	}

	// delete item from Typesense
	// (if Typesense client is provided)
	if opts.TypesenseClient != nil {
		// delete item from itemdraft collection
		found, err = opts.TypesenseClient.DeleteItemDraft(i.ID)
		if err != nil {
			return err
		}
		if !found {
			return errors.New("[ℹ️][Item][Delete] item not found in search engine (itemdraft)")
		}
		// delete item from creation collection
		found, err = opts.TypesenseClient.DeleteCreation(i.ID)
		if err != nil {
			return err
		}
		if !found {
			return errors.New("[ℹ️][Item][Delete] item not found in search engine (creation)")
		}
	}

	// delete item's files
	// (if item storage path is provided)
	if opts.ItemStoragePath != "" {
		extensions := []string{".3zh", ".3zh.etag", ".pcubes", ".etag", ".glb", ".model.etag"}
		for _, extension := range extensions {
			filePath := filepath.Join(opts.ItemStoragePath, i.Repo, i.Name+extension)
			if _, err := os.Stat(filePath); os.IsNotExist(err) {
				// file does not exist, skip
				continue
			}
			fmt.Println("🐞[Item.Delete] deleting item's file:", filePath)
			err = os.Remove(filePath)
			if err != nil {
				fmt.Println("❌[Item.Delete] failed to delete item's file:", filePath, "| Error:", err.Error())
				return err
			}
		}
	}

	return nil
}

// SaveInternal does the same as Save
func (i *Item) SaveInternal(dbClient *dbclient.Client) error {

	if i.ReadOnly {
		return errors.New("read only item instance can't be saved")
	}

	if i.Author == "" {
		return errors.New("author can't be empty")
	}

	u, err := user.GetByID(dbClient, i.Author)
	if err != nil {
		return errors.New("author not found")
	}

	if u == nil {
		return errors.New("author not found")
	}

	// SaveInternal doesn't allow to create new items,
	// but only to update existing ones
	if i.IsNew {
		err = errors.New("creating new item is not allowed")
		fmt.Println("Item.SaveInternal error:", err.Error())
		return err
	}

	// UPDATE
	err = i.update(dbClient, false /* refreshUpdatedAt */)
	if err != nil {
		fmt.Println("Item.SaveInternal update error:", err.Error())
		return err
	}

	return nil
}

func (i *Item) insert(dbClient *dbclient.Client) error {
	return dbClient.InsertNewRecord(dbclient.ItemCollectionName, i)
}

func (i *Item) update(dbClient *dbclient.Client, refreshUpdatedAt bool) error {
	now := time.Now()

	updateData := make(map[string]interface{})
	updateData["author"] = i.Author
	updateData["repo"] = i.Repo
	updateData["name"] = i.Name
	updateData["public"] = i.Public
	updateData["description"] = i.Description
	updateData["script"] = i.Script
	updateData["blockCount"] = i.BlockCount
	updateData["ban"] = i.Banned
	updateData["archived"] = i.Archived
	updateData["updated"] = now

	filter := map[string]interface{}{"id": i.ID}
	err := dbClient.UpdateRecord(dbclient.ItemCollectionName, filter, updateData)
	if err == nil {
		i.Updated = now
	}
	return err
}

func AddLike(dbClient *dbclient.Client, itemID string, userID string) error {

	itemLike := &ItemLike{
		ItemID:  itemID,
		UserID:  userID,
		Created: time.Now(),
	}

	err := dbClient.InsertNewRecord(dbclient.ItemLikeCollectionName, itemLike)
	if err != nil {
		return err
	}

	filter := make(map[string]interface{})
	filter["id"] = itemID

	err = dbClient.IncrementFieldInRecord(dbclient.ItemCollectionName, filter, "likes", 1)
	return err
}

func RemoveLike(dbClient *dbclient.Client, itemID string, userID string) error {
	filter := make(map[string]interface{})
	filter["item"] = itemID
	filter["user"] = userID

	found, err := dbClient.DeleteSingleRecordMatchingFilter(dbclient.ItemLikeCollectionName, filter)
	if err != nil {
		return err
	}
	if found == false {
		return errors.New("no like removed")
	}

	filter = make(map[string]interface{})
	filter["id"] = itemID

	err = dbClient.IncrementFieldInRecord(dbclient.ItemCollectionName, filter, "likes", -1)
	return err
}

// RemoveAllLikes removes all item likes for a given user
// Returns the number of likes removed
func RemoveAllLikes(dbClient *dbclient.Client, userID string) (int, error) {

	// get all world likes for the provided user
	itemLikes, err := GetAllLikesFromUser(dbClient, userID)
	if err != nil {
		return 0, err
	}

	count := 0
	for _, like := range itemLikes {
		err = RemoveLike(dbClient, like.ItemID, userID)
		if err != nil {
			return 0, err
		}
		count++
	}

	// TODO: decrement or simply recompute the likes count for the concerned items
	//       (ie. update the items' records accordingly)

	return count, nil
}

func DeleteAllItemsForUser(dbClient *dbclient.Client, userID string, opts ItemDeleteOpts) (int, error) {

	// get all items of the user
	items, err := GetByAuthorID(dbClient, userID)
	if err != nil {
		return 0, err
	}

	// delete each item
	for _, item := range items {
		err = item.Delete(dbClient, opts)
		if err != nil {
			return 0, err
		}
	}

	return len(items), nil
}

func IsItemNameForBodyPart(itemName string) bool {
	var bodyParts []string = []string{
		"head",
		"body",
		"left_arm",
		"right_arm",
		"left_leg",
		"right_leg",
		"left_hand",
		"right_hand",
		"left_foot",
		"right_foot"}
	for _, name := range bodyParts {
		if itemName == name {
			return true
		}
	}
	return false
}

func IsItemNameForEquipment(itemName string) bool {
	var equipmentNames []string = []string{
		"hair",
		"top",
		"lsleeve",
		"rsleeve",
		"lpant",
		"rpant",
		"lglove",
		"rglove",
		"lboot",
		"rboot",
		"boots",  // v3
		"jacket", // v3
		"pants",  // v3
		"shirt"}  // v3
	for _, name := range equipmentNames {
		if itemName == name {
			return true
		}
	}
	return false
}

// ParseRepoDotName parses a <repo>.<name> expression
func ParseRepoDotName(repoDotName string) (repo, name string, err error) {
	if len(repoDotName) < 3 { // min length
		err = errors.New("<repo>.<name> string too short")
		return
	}
	elements := strings.Split(repoDotName, ".")
	if len(elements) != 2 {
		err = errors.New("<repo>.<name> invalid (wrong element count)")
	}
	repo = elements[0]
	name = elements[1]
	return
}

func ItemCountBlocks(filePath string) (uint32, error) {
	var outBuf bytes.Buffer
	var errBuf bytes.Buffer

	// count blocks in .3zh file and update value in DB
	cmd := exec.Command("/cubzh_cli", "blocks", "-i", filePath)
	outBuf.Reset()
	cmd.Stdout = &outBuf
	errBuf.Reset()
	cmd.Stderr = &errBuf
	err := cmd.Run()

	outStr := outBuf.String()
	if err != nil {
		fmt.Println("📝 cubzh_cli")
		fmt.Println(outStr)
		fmt.Println(errBuf.String())
		fmt.Println("❌ [ItemCountBlocks] cubzh_cli failed to count blocks in ", filePath, "| Error:", err.Error())
		return 0, err
	}
	var blockCount uint32 = 0 // 0 means invalid block count
	if len(outStr) > 0 {
		intValue, err := strconv.ParseUint(outStr, 10, 32)
		if err != nil {
			fmt.Println("❌ [ItemCountBlocks] failed to count blocks in .3zh/.pcubes (2):", err.Error())
			return 0, err
		}
		blockCount = uint32(intValue)
	}
	return blockCount, nil
}

// --------------------------------------------------
// UNEXPOSED TYPES
// --------------------------------------------------

// listFilter is an enum representing different game lists
type listFilter string

const (
	listNone     listFilter = "none"     // no particular list
	listNew      listFilter = "new"      // recently created
	listRecent   listFilter = "recent"   // recently updated
	listMine     listFilter = "mine"     // owned by user
	listFeatured listFilter = "featured" // featured
	// listPopular  listFilter = "popular"  // popular ones
	// listFriends  listFilter = "friends"  // created by friends
	// listDev      listFilter = "dev"      // items that can be edited
)

// itemCategory is an enum representing different item categories
type itemCategory string

const (
	categoryHead        itemCategory = "head"
	categoryBody        itemCategory = "body"
	categoryLeftArm     itemCategory = "left_arm"
	categoryLeftLeg     itemCategory = "left_leg"
	categoryRightArm    itemCategory = "right_arm"
	categoryRightLeg    itemCategory = "right_leg"
	categoryLeftFoot    itemCategory = "left_foot"
	categoryRightFoot   itemCategory = "right_foot"
	categoryLeftHand    itemCategory = "left_hand"
	categoryRightHand   itemCategory = "right_hand"
	categoryHair        itemCategory = "hair"
	categoryTop         itemCategory = "top"
	categoryLeftSleeve  itemCategory = "lsleeve"
	categoryRightSleeve itemCategory = "rsleeve"
	categoryLeftGlove   itemCategory = "lglove"
	categoryRightGlove  itemCategory = "rglove"
	categoryLeftPant    itemCategory = "lpant"
	categoryRightPant   itemCategory = "rpant"
	categoryLeftBoot    itemCategory = "lboot"
	categoryRightBoot   itemCategory = "rboot"
	categoryBoots       itemCategory = "boots"  // v3
	categoryJacket      itemCategory = "jacket" // v3
	categoryPants       itemCategory = "pants"  // v3
	categoryShirt       itemCategory = "shirt"  // v3
)

type itemType string

const (
	itemTypeMesh   itemType = "mesh"
	itemTypeVoxels itemType = "voxels"
)

// get all ItemLikes for a given user
func GetAllLikesFromUser(dbClient *dbclient.Client, userID string) ([]*ItemLike, error) {
	var itemLikes []*ItemLike
	filter := map[string]interface{}{
		"user": userID,
	}
	err := dbClient.GetRecordsMatchingFilter(dbclient.ItemLikeCollectionName, filter, nil, &itemLikes)
	return itemLikes, err
}
