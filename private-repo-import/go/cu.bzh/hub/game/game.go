package game

import (
	"errors"
	"fmt"
	"os"
	"path/filepath"
	"time"

	"github.com/gomodule/redigo/redis"

	"cu.bzh/hub/dbclient"
	"cu.bzh/hub/search"
	"cu.bzh/hub/user"
	"cu.bzh/utils"
)

// Note: time.Time is encoded/decoded using RFC3339

const (
	DEFAULT_MAX_PLAYERS = 8
	MAX_PLAYERS_LIMIT   = 64
)

var (
	redisPool *redis.Pool
)

func init() {
	redisPool = newRedisPool("gameservers-db:6379")
}

// List is an enum representing different game lists
type listFilter string

const (
	listFilterNone     listFilter = "none"     // no particular list
	listFilterNew      listFilter = "new"      // recently created
	listFilterRecent   listFilter = "recent"   // recently updated
	listFilterPopular  listFilter = "popular"  // popular ones
	listFilterFriends  listFilter = "friends"  // played by friends
	listFilterDev      listFilter = "dev"      // games that can be edited
	listFilterMine     listFilter = "mine"     // games owned by user
	listFilterFeatured listFilter = "featured" // featured games
)

type ContentWarning string

const (
	contentWarningPorn      ContentWarning = "porn"      //
	contentWarningGore      ContentWarning = "gore"      //
	contentWarningPolitical ContentWarning = "political" //
)

// supported sort methods
var supportedLists = map[string]bool{"new": true, "recent": true, "mine": true, "featured": true}

func isListSupported(list string) bool {
	_, supported := supportedLists[list]
	return supported
}

func GetDefaultScriptToUse(clientBuildNumber *int) string {
	if clientBuildNumber == nil || *clientBuildNumber >= 199 {
		return defaultScript_0_1_19plus
	}
	return defaultScriptLegacy
}

// Game constains all user fields
type Game struct {
	ID string `bson:"id" json:"id,omitempty"`

	// Author is the id of the User who created the game
	Author string `bson:"author" json:"author,omitempty"`

	// Not saved in database, but set to true within some requests
	// to indicate that the request sender is the author
	IsAuthor bool `bson:"-" json:"-"`

	// Contributors represents other users allowed to edit the script
	Contributors []string `bson:"contributors" json:"contributors,omitempty"`

	// Tags represents the tags that the Game has
	Tags []string `bson:"tags" json:"tags,omitempty"`

	// Title ...
	Title string `bson:"title" json:"title,omitempty"`

	// Title ...
	Description string `bson:"description" json:"description,omitempty"`

	// Max number of players on a server
	MaxPlayers int32 `bson:"max-players" json:"max-players,omitempty"`

	// Script ...
	Script string `bson:"script" json:"script,omitempty"`

	// True when the game is created, becomes false when the
	// script is updated. Allows to serve most recent default
	// script in the meantime, dynamically.
	DefaultScript bool `bson:"default-script" json:"default-script"`

	// True if the game has been featured
	Featured bool `bson:"featured" json:"featured,omitempty"`

	// Number of likes
	Likes int32 `bson:"likes" json:"likes,omitempty"`

	// Indicates if user liked the game or not.
	// This is different for each user, not saved in the database.
	Liked bool `bson:"-" json:"-"`

	// Number of views
	Views int32 `bson:"views" json:"views,omitempty"`

	Created time.Time `bson:"created" json:"created,omitempty"`
	Updated time.Time `bson:"updated" json:"updated,omitempty"`

	// IsNew means "not saved in database"
	IsNew bool `bson:"-" json:"-"`

	// ReadOnly is used to make sure the Game object is never saved
	ReadOnly bool `bson:"-" json:"-"`

	// username of the game's author
	AuthorName string `bson:"authorName,omitempty" json:"authorName,omitempty"`

	// can be "porn", "gore", "political", ...
	ContentWarnings []ContentWarning `bson:"contentWarn,omitempty" json:"contentWarn,omitempty"`

	// ...
	MapBase64 string `bson:"mapBase64" json:"mapBase64,omitempty"`

	// Whether the game is archived
	Archived bool `bson:"archived" json:"archived,omitempty"`
}

// GameLike represents one like for a game
type GameLike struct {
	GameID  string    `bson:"game" json:"game,omitempty"`
	UserID  string    `bson:"user" json:"user,omitempty"`
	Created time.Time `bson:"created" json:"created,omitempty"`
}

// GameComment represents one comment for a game
type GameComment struct {
	GameID  string    `bson:"game" json:"game,omitempty"`
	UserID  string    `bson:"user" json:"user,omitempty"`
	Content []byte    `bson:"content" json:"content,omitempty"`
	Created time.Time `bson:"created" json:"created,omitempty"`
	Updated time.Time `bson:"updated" json:"updated,omitempty"`
}

// NewReadOnly can be used to execute quick read-only actions like listing game servers
func NewReadOnly(ID string) *Game {
	return &Game{ID: ID, ReadOnly: true}
}

// New creates a new user, with empty ID
func New() (*Game, error) {
	now := time.Now()
	uuid, err := utils.UUID()
	if err != nil {
		return nil, err
	}
	return &Game{
		ID:            uuid,
		Title:         "",
		MaxPlayers:    DEFAULT_MAX_PLAYERS,
		Script:        "",
		DefaultScript: true,
		Created:       now,
		Updated:       now,
		IsNew:         true}, nil
}

// GetByAuthor returns all the games of the given author
func GetByAuthor(dbClient *dbclient.Client, authorID string) ([]*Game, error) {
	games := make([]*Game, 0)
	filter := make(map[string]interface{})
	filter["author"] = authorID
	err := dbClient.GetRecordsMatchingFilter(dbclient.GameCollectionName, filter, nil, &games)
	return games, err
}

// GetAll returns all users stored in the database
func GetAll(dbClient *dbclient.Client) ([]*Game, error) {
	games := make([]*Game, 0)
	err := dbClient.GetAllRecords(dbclient.GameCollectionName, &games)
	return games, err
}

// List lists games
// TODO: gaetan: the boolean return value here is a bad design, let's check whether it is actually used
func List(dbClient *dbclient.Client, filterStr string, userID string) ([]*Game, error, bool /*is "mine" filter active*/) {
	var results []*Game
	var isMineFilterActive bool = false

	filter := make(map[string]interface{})
	findOptions := &dbclient.FindOptions{}
	// default limit
	findOptions.SetLimit(100)
	// default sort
	sort := make(map[string]interface{})
	sort["updated"] = -1
	findOptions.SetSort(sort)

	// findOptions.SetProjection(bson.M{
	// 	"id":             1,
	// 	"title":          1,
	// 	"description":    1,
	// 	"created":        1,
	// 	"updated":        1,
	// 	"default-script": 1,
	// 	"author":         1,
	// 	"likes":          1,
	// 	"views":          1,
	// })

	list := listFilterNone
	f := utils.NewFilter(filterStr)
	if l, exists := f.Params["list"]; exists {
		if isListSupported(l) == false {
			return nil, errors.New("list filter not supported: " + l), false
		}
		list = listFilter(l)
	}

	switch list {
	case listFilterNew:
		sort = make(map[string]interface{})
		sort["created"] = -1
		findOptions.SetSort(sort)
	case listFilterRecent:
		sort = make(map[string]interface{})
		sort["updated"] = -1
		findOptions.SetSort(sort)
	case listFilterMine:
		filter["author"] = userID
		isMineFilterActive = true
	case listFilterFeatured:
		filter["featured"] = true
		sort = make(map[string]interface{})
		sort["updated"] = -1
		findOptions.SetSort(sort)
	}

	if f.String != "" {
		// Note (gdevillele) : I replaced this by a regex, as the asbstraction is not available yet
		// filter = append(filter, bson.E{"$text", bson.M{"$search": f.String}})
		filter["title"] = dbclient.Regex{Pattern: ".*" + f.String + ".*", Options: "i"}
	}

	err := dbClient.GetRecordsMatchingFilter(dbclient.GameCollectionName, filter, findOptions, &results)

	// filter out archived worlds
	resultsNonArchived := make([]*Game, 0)
	for _, world := range results {
		if world.Archived == false {
			resultsNonArchived = append(resultsNonArchived, world)
		}
	}

	return resultsNonArchived, err, isMineFilterActive
}

// GetByID gets game with given game ID
// if not found, returns no error and nil game
// error != nil in case of internal server issue
// if not empty, requesterUserID will be used to set Game.Liked field.
// true if requester did like the game.
func GetByID(dbClient *dbclient.Client, gameID string, requesterUserID string, clientBuildNumber *int) (*Game, error) {
	g := &Game{}

	filter := make(map[string]interface{})
	filter["id"] = gameID
	found, err := dbClient.GetSingleRecord(dbclient.GameCollectionName, filter, g)
	if err != nil {
		return g, err
	}
	if found == false {
		return nil, nil
	}

	if g.DefaultScript == true {
		g.Script = GetDefaultScriptToUse(clientBuildNumber)
	}

	// get game's author name
	if g.Author != "" {
		author := &user.User{}
		filter := make(map[string]interface{})
		filter["id"] = g.Author
		found, err := dbClient.GetSingleRecord(dbclient.UserCollectionName, filter, author)
		if err == nil && found == true {
			// ok
			g.AuthorName = author.Username
		}
	}

	if requesterUserID != "" {
		like := &GameLike{}
		filter := make(map[string]interface{})
		filter["game"] = gameID
		filter["user"] = requesterUserID
		found, err := dbClient.GetSingleRecord(dbclient.GameLikeCollectionName, filter, like)
		if err != nil {
			return nil, err
		}
		g.Liked = found
	}

	if g.MaxPlayers == 0 {
		g.MaxPlayers = DEFAULT_MAX_PLAYERS
	}

	return g, nil
}

// AddContentWarning adds new content warning to Game, if it isn't already present.
func (g *Game) AddContentWarning(newContentWarning ContentWarning) error {
	if newContentWarning != contentWarningPorn &&
		newContentWarning != contentWarningGore &&
		newContentWarning != contentWarningPolitical {
		return errors.New("unsupported content warning")
	}

	// check if content warning is already present in Game
	for _, cw := range g.ContentWarnings {
		if cw == newContentWarning {
			return nil // content warning already there, it's fine
		}
	}
	// insert new content warning
	g.ContentWarnings = append(g.ContentWarnings, newContentWarning)

	return nil
}

// Save writes the game in database, in given games collection.
// Making sure the author and other referenced user can be
// found in the users collection.
func (g *Game) Save(dbClient *dbclient.Client, clientBuildNumber *int) error {

	if g.ReadOnly {
		return errors.New("read only game instance can't be saved")
	}

	if g.Author == "" {
		return errors.New("author can't be empty")
	}

	u, err := user.GetByID(dbClient, g.Author)
	if err != nil {
		return errors.New("author not found")
	}

	if u == nil {
		return errors.New("author not found")
	}

	defaultScriptToUse := GetDefaultScriptToUse(clientBuildNumber)

	// Toggle DefaultScript to `true` if the script has been changed
	if g.DefaultScript == true && g.Script != "" && g.Script != defaultScriptToUse {
		g.DefaultScript = false
	}
	if g.DefaultScript == true {
		// no need to keep script if we want the default one
		g.Script = ""
	}

	// UPDATE
	if g.IsNew == false {
		err := g.update(dbClient)
		if err != nil {
			fmt.Println("Game.Save update error:", err.Error())
			return err
		}
	} else { // INSERT
		err := g.insert(dbClient)
		if err != nil {
			fmt.Println("Game.Save insert error:", err.Error())
			return err
		}
		g.IsNew = false
	}

	return nil
}

type WorldDeleteOpts struct {
	TypesenseClient  *search.Client
	WorldStoragePath string
}

func (g *Game) Delete(dbClient *dbclient.Client, opts WorldDeleteOpts) error {
	if dbClient == nil {
		return errors.New("database client is nil")
	}

	filter := make(map[string]interface{})
	filter["id"] = g.ID

	// delete world from MongoDB database
	found, err := dbClient.DeleteSingleRecordMatchingFilter(dbclient.GameCollectionName, filter)
	if err != nil {
		return err
	}
	if found == false {
		return errors.New("world not found")
	}

	// delete world from Typesense
	// (if Typesense client is provided)
	if opts.TypesenseClient != nil {
		// delete world from world collection
		found, err = opts.TypesenseClient.DeleteWorld(g.ID)
		if err != nil {
			return err
		}
		if !found {
			return errors.New("[ℹ️][World][Delete] world not found in search engine (world)")
		}
		// delete world from creation collection
		found, err = opts.TypesenseClient.DeleteCreation(g.ID)
		if err != nil {
			return err
		}
		if !found {
			return errors.New("[ℹ️][World][Delete] world not found in search engine (creation)")
		}
	}

	// delete world's files (git repo for code)
	// (if world storage path is provided)
	if opts.WorldStoragePath != "" {
		worldDirPath := filepath.Join(opts.WorldStoragePath, g.ID)
		if _, err := os.Stat(worldDirPath); os.IsNotExist(err) {
			// world directory does not exist, skip
			return nil
		}
		err = os.RemoveAll(worldDirPath)
		if err != nil {
			fmt.Println("❌[World.Delete] failed to delete world's directory:", worldDirPath, "| Error:", err.Error())
			return err
		}
	}

	return nil
}

func (g *Game) insert(dbClient *dbclient.Client) error {
	return dbClient.InsertNewRecord(dbclient.GameCollectionName, g)
}

func (g *Game) update(dbClient *dbclient.Client) (err error) {
	now := time.Now()

	updateData := make(map[string]interface{})
	updateData["author"] = g.Author
	updateData["title"] = g.Title
	updateData["description"] = g.Description
	updateData["max-players"] = g.MaxPlayers
	updateData["script"] = g.Script
	updateData["default-script"] = g.DefaultScript
	updateData["contentWarn"] = g.ContentWarnings
	updateData["mapBase64"] = g.MapBase64
	updateData["updated"] = now
	updateData["featured"] = g.Featured
	updateData["archived"] = g.Archived

	filter := map[string]interface{}{"id": g.ID}
	err = dbClient.UpdateRecord(dbclient.GameCollectionName, filter, updateData)
	if err == nil {
		g.Updated = now
	}

	return err
}

func (g *Game) UpdateUpdatedAt(dbClient *dbclient.Client) error {
	if dbClient == nil {
		return errors.New("dbClient is nil")
	}
	filter := map[string]interface{}{"id": g.ID}
	updateData := map[string]interface{}{"updated": time.Now()}
	err := dbClient.UpdateRecord(dbclient.GameCollectionName, filter, updateData)
	return err
}

type GameUpdate struct {
	Featured *bool `json:"featured,omitempty"`

	updated *time.Time `json:"updated,omitempty"`
}

func (gu *GameUpdate) convertToMap() map[string]interface{} {
	result := make(map[string]interface{})

	if gu.Featured != nil {
		result["featured"] = *gu.Featured
	}

	if gu.updated != nil {
		result["updated"] = *gu.updated
	}

	return result
}

func (g *Game) UpdateAndSave(dbClient *dbclient.Client, update GameUpdate) error {

	if g.ReadOnly {
		return errors.New("read only game instance can't be saved")
	}

	now := time.Now()

	{
		update.updated = &now
		if update.Featured != nil {
			g.Featured = *update.Featured
		}
	}

	updateDataMap := update.convertToMap()

	filter := map[string]interface{}{"id": g.ID}
	err := dbClient.UpdateRecord(dbclient.GameCollectionName, filter, updateDataMap)
	if err == nil {
		g.Updated = now
	}

	return err
}

func IncrementViews(dbClient *dbclient.Client, gameID string, incr int) error {
	filter := make(map[string]interface{})
	filter["id"] = gameID

	err := dbClient.IncrementFieldInRecord(dbclient.GameCollectionName, filter, "views", incr)
	fmt.Println("game views incremented. Error:", err)
	return err
}

// AddLike ...
func AddLike(dbClient *dbclient.Client, gameID string, userID string) error {

	gameLike := &GameLike{
		GameID:  gameID,
		UserID:  userID,
		Created: time.Now(),
	}

	err := dbClient.InsertNewRecord(dbclient.GameLikeCollectionName, gameLike)
	if err != nil {
		return err
	}

	// if insert doesn't fail, we can increment the like count
	// NOTE: aduermael: I would prefer these 2 operations to be a transaction,
	// but can't find how to do it with this library.

	filter := make(map[string]interface{})
	filter["id"] = gameID

	err = dbClient.IncrementFieldInRecord(dbclient.GameCollectionName, filter, "likes", 1)
	return err
}

func RemoveLike(dbClient *dbclient.Client, gameID string, userID string) error {
	filter := make(map[string]interface{})
	filter["game"] = gameID
	filter["user"] = userID

	found, err := dbClient.DeleteSingleRecordMatchingFilter(dbclient.GameLikeCollectionName, filter)
	if err != nil {
		return err
	}
	if found == false {
		return errors.New("no like removed")
	}

	// if insert doesn't fail, we can decrement the like count
	// NOTE: aduermael: I would prefer these 2 operations to be a transaction,
	// but can't find how to do it with this library.
	// NOTE: gdevillele: if a transaction is not possible, we can re-count the likes
	// to prevent any discrepancy between the likes count in the database and the counter in the world record.

	filter = make(map[string]interface{})
	filter["id"] = gameID

	err = dbClient.IncrementFieldInRecord(dbclient.GameCollectionName, filter, "likes", -1)
	return err
}

// RemoveAllLikes removes all world likes for a given user
// Returns the number of likes removed
func RemoveAllLikes(dbClient *dbclient.Client, userID string) (int, error) {

	// get all world likes for the provided user
	worldLikes, err := GetAllLikesFromUser(dbClient, userID)
	if err != nil {
		return 0, err
	}

	count := 0
	for _, like := range worldLikes {
		err = RemoveLike(dbClient, like.GameID, userID)
		if err != nil {
			return 0, err
		}
		count++
	}

	// TODO: decrement or simply recompute the likes count for the concerned worlds
	//       (ie. update the worlds' records accordingly)

	return count, nil
}

// get all WorldLikes for a given user
func GetAllLikesFromUser(dbClient *dbclient.Client, userID string) ([]*GameLike, error) {
	var gameLikes []*GameLike
	filter := map[string]interface{}{
		"user": userID,
	}
	err := dbClient.GetRecordsMatchingFilter(dbclient.GameLikeCollectionName, filter, nil, &gameLikes)
	return gameLikes, err
}

// DeleteAllWorldsForUser deletes all worlds for a given user
// It is used to delete all worlds created by a user when banning them.
func DeleteAllWorldsForUser(dbClient *dbclient.Client, userID string, opts WorldDeleteOpts) (int, error) {

	// get all worlds of the user
	worlds, err := GetByAuthor(dbClient, userID)
	if err != nil {
		return 0, err
	}

	// delete each world
	for _, world := range worlds {
		err = world.Delete(dbClient, opts)
		if err != nil {
			return 0, err
		}
	}

	return len(worlds), nil
}
