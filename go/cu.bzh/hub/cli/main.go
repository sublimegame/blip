package main

import (
	"bufio"
	"errors"
	"fmt"
	"io"
	"io/fs"
	"log"
	"os"
	"path/filepath"
	"strconv"
	"strings"
	"time"

	"cu.bzh/hub/dbclient"
	"cu.bzh/hub/game"
	"cu.bzh/hub/item"
	"cu.bzh/hub/scyllaclient"
	"cu.bzh/hub/search"
	"cu.bzh/hub/user"
	"cu.bzh/mailer"

	"github.com/urfave/cli"
)

const (
	pcubesDir    = "/particubes_items"
	avatarSuffix = "_edit"
	// command flag names
	kFlagDryrun = "dryrun"
	DB_ADDR     = "hub-db"
)

var (
	MAILJET_API_KEY    = os.Getenv("MAILJET_API_KEY")
	MAILJET_API_SECRET = os.Getenv("MAILJET_API_SECRET")
)

var (
	CUBZH_SEARCH_SERVER  string               = ""
	CUBZH_SEARCH_APIKEY  string               = ""
	scyllaClientUniverse *scyllaclient.Client = nil
)

// TODO: update "username_1" index so it has sparse=true
// collection := client.MongoClient.Database("particubes").Collection("users")
// err = user.EnsureIndexes(collection)
// if err != nil {
// 	return nil, err
// }

func main() {
	var err error

	// Create scylla clients for different keyspaces
	{
		// Keyspace : "universe"
		{
			config := scyllaclient.ClientConfig{
				Servers: []string{
					// "scylladb:9042",
					DB_ADDR,
				},
				Username:  "",
				Password:  "",
				AwsRegion: "",
				Keyspace:  "universe",
			}
			scyllaClientUniverse, err = scyllaclient.New(config)
			if err != nil {
				log.Fatal("failed to create scylla client (universe):", err.Error())
			}
		}
	}

	app := cli.NewApp()
	app.Name = "Particubes CLI"
	app.Usage = ""
	// app.EnableBashCompletion = true
	app.Commands = []cli.Command{
		{
			Name:   "user-add",
			Usage:  "Adds user in database.",
			Action: addUser,
			/*Flags: []cli.Flag{
				cli.StringFlag{
					Name:  "file",
					Value: "FOO",
					Usage: ".csv file path",
				},
			},*/
		},
		{
			Name:   "user-update",
			Usage:  "Updates user in database.",
			Action: updateUser,
		},
		{
			Name:   "sync-users-with-mailjet",
			Usage:  "Syncs users with mailjet contacts",
			Action: syncMailjet,
		},
		{
			Name:   "user-gen-password",
			Usage:  "Generates new password for user.",
			Action: userGenPassword,
		},
		{
			Name:   "user-info",
			Usage:  "Prints user info.",
			Action: infoUser,
		},
		{
			Name:   "user-games",
			Usage:  "Lists user games.",
			Action: userGames,
		},
		{
			Name:   "info-user",
			Usage:  "Prints user info.",
			Action: infoUser,
		},
		{
			Name:   "add-game",
			Usage:  "Adds game in database.",
			Action: addGame,
		},
		{
			Name:   "one-game-per-user",
			Usage:  "Makes sure each user is the author of one game at least. Creates games to enforce this if necessary.",
			Action: oneGamePerUser,
		},
		{
			Name:   "assign-user-numbers",
			Usage:  "Assigns user numbers (using account creation date)",
			Action: assignUserNumbers,
		},
		{
			Name:   "assign-user-steamkeys",
			Usage:  "Assigns user steam keys",
			Action: assignUserSteamKeys,
		},
		{
			Name:   "one-item-per-pcubes",
			Usage:  "Makes sure each .pcubes has a corresponding entry in DB.",
			Action: oneItemPerPcubes,
		},
		// {
		// 	Name:   "insert-airtable-tester",
		// 	Usage:  "inserts legacy beta tester from airtable",
		// 	Action: insertAirtableTester,
		// },
		{
			Name:   "email-confirmation-reminders",
			Usage:  "Send email confirmation reminders",
			Action: emailConfirmationReminders,
		},
		{
			Name:   "sanitize",
			Usage:  "Sanitizes what needs to be sanitized.",
			Action: sanitize,
		},
		{
			Name:   "stats",
			Usage:  "Compute and display stats about users (dev/artists)",
			Action: userStats,
		},
		{
			Name:   "rename-body-parts",
			Usage:  "Rename items having body-parts' names",
			Action: renameBodyParts,
		},
		// --------------- SEARCH ENGINE ----------------
		{ // create collections (can be forced to delete & re-create collections)
			// collection names: "itemdraft", "world", "creation"
			Name:      "se-init-collection",
			Usage:     "Create collection.",
			Action:    searchInitCollections,
			UsageText: "cli se-init-collection -c itemdraft\n	cli se-init-collection -c world --force\n	cli se-init-collection -c creation --force",
			Flags: []cli.Flag{
				cli.StringFlag{
					Name:     "collection,c",
					Required: true,
				},
				cli.BoolFlag{
					Name: "force,f",
				},
			},
			ArgsUsage: "--force to force the creation of collections, even if they already exist",
		},
		{ // index all items from mongoDB
			Name:   "se-feed-itemdraft-from-mongo",
			Usage:  "Insert ItemDraft data in search engine.",
			Action: searchFeedAllItemDrafts,
		},
		{
			Name:   "search-itemdraft",
			Usage:  "perform a search among ItemDrafts.",
			Action: searchItemDraft,
		},
		{ // index all worlds from mongoDB
			Name:   "se-feed-world-from-mongo",
			Usage:  "Insert Worlds data in search engine.",
			Action: searchFeedAllWorlds,
		},
		{
			Name:   "se-search-world",
			Usage:  "perform a search among Worlds.",
			Action: searchWorld,
		},
		{
			Name:   "se-feed-creations-from-mongo",
			Usage:  "Insert Creations data in search engine.",
			Action: searchFeedAllCreations,
		},
		{
			Name:   "se-search-creation",
			Usage:  "perform a search among Creations.",
			Action: searchCreation,
		},
		//
		// --- ITEMS ---
		//
		{ // Processes all 3zh files, counts blocks in them and update the BlockCount field in mongoDB
			Name:   "items_count_blocks_in_3zh",
			Usage:  "Counts blocks in all 3zh files and updates the 'BlockCount' field in mongoDB.",
			Action: itemsCountBlocksIn3zh,
			Flags: []cli.Flag{
				cli.BoolFlag{
					Name: kFlagDryrun,
				},
			},
			ArgsUsage: "--" + kFlagDryrun + " to have the command logging the changes instead of writing them in mongoDB",
		},
		//
		// DEV
		//
		{
			Name:        "store-module",
			Description: "Stores a module in the universe keyspace",
			Action:      storeModule,
		},
		{
			Name:        "read-module",
			Description: "Stores a module in the universe keyspace",
			Action:      readModule,
		},
		{
			Name:        "dev",
			Description: "",
			Action:      dev,
		},
		{
			Name:        "get-stats-under13",
			Description: "",
			Action:      getStatsUnder13,
		},
		// --------------------------------
		// FRIENDS
		// --------------------------------
		{ // Unit tests
			Name:        "test-friends",
			Usage:       "test-friends",
			Description: "This is a test suite for the friends system.",
			Action:      friendsTests,
		},
		{
			Name:        "new-friend-req",
			Usage:       "new-friend-req <userID1> <username1> <userID2> <username2>",
			Description: "Creates a new friend request from a user to another one.",
			Action:      newFriendRequest,
		},
		{
			Name:        "get-friend-reqs-sent",
			Description: "Gets friend requests sent",
			Action:      friendRequestsSent,
		},
		{
			Name:        "get-friend-reqs-received",
			Description: "Gets friend requests received",
			Action:      friendRequestsReceived,
		},
		{
			Name:        "friend-req-cancel",
			Description: "",
			Action:      friendRequestCancel,
		},
		{
			Name:   "get-friend-relation",
			Usage:  "get-friend-relation <userID1> <userID2>",
			Action: getFriendRelation,
		},
		// {
		// 	Name:   "iter-friend-relations-fdb",
		// 	Usage:  "iter-friend-relations-fdb",
		// 	Action: iterFriendRelationsFDB,
		// },
		// ----------------
		// KVS MIGRATION
		// ----------------
		// {
		// 	Name:   "kvs-migrate",
		// 	Usage:  "Migrates data from one KVS to the ScyllaDB cluster",
		// 	Action: kvsMigrate,
		// },
		{
			Name:   "fix-bug-parent-phone",
			Usage:  "fix-bug-parent-phone",
			Action: fixBugParentPhone,
		},
		{
			Name:   "fix-balance-cache",
			Usage:  "fix-balance-cache",
			Action: fixBalanceCache,
		},
		{
			Name:   "fix-balance-cache-for-all-users",
			Usage:  "fix-balance-cache-for-all-users",
			Action: fixBalanceCacheForAllUsers,
		},
	}

	err = app.Run(os.Args)
	if err != nil {
		fmt.Println("❌", err)
		os.Exit(1)
	}
}

func searchInitCollections(c *cli.Context) error {
	err := ensureSearchEngineEnvars()
	if err != nil {
		return err
	}

	// retrieve flags values
	force := c.Bool("force")
	collection := c.String("collection")

	searchClient, err := search.NewClient(CUBZH_SEARCH_SERVER, CUBZH_SEARCH_APIKEY)
	if err != nil {
		return err
	}

	if collection == "itemdraft" {

		fmt.Println("Collection ItemDrafts...")
		if !force && searchClient.CollectionExists(search.COLLECTION_ITEMDRAFTS) {
			fmt.Println("    already exists ✅")
			return nil
		}
		err = searchClient.CreateItemDraftCollection()
		if err != nil {
			fmt.Println("    creation failure ❌")
			return err
		}

	} else if collection == "world" {

		fmt.Println("Collection Worlds...")
		if !force && searchClient.CollectionExists(search.COLLECTION_WORLDS) {
			fmt.Println("    already exists ✅")
			return nil
		}
		err = searchClient.CreateWorldCollection()
		if err != nil {
			fmt.Println("    creation failure ❌")
			return err
		}

	} else if collection == "creation" {

		fmt.Println("⚙️ Creating collection \"Creations\"...")
		err = searchClient.CreateCreationCollection(force)
		if err != nil {
			fmt.Println("❌", err.Error())
			return err
		}

	} else {
		return errors.New("invalid collection name")
	}

	fmt.Println("    created successfully ✅")
	return nil
}

// searchFeedAllItemDrafts gets all the Items in the mongoDB and inserts their info in the search engine DB
func searchFeedAllItemDrafts(c *cli.Context) error {
	fmt.Println("Updating collection : ItemDrafts...")

	err := ensureSearchEngineEnvars()
	if err != nil {
		return err
	}

	// mongo client
	dbClient, err := dbclient.SharedUserDB()
	if err != nil {
		return err
	}

	// search engine client
	searchClient, err := search.NewClient(CUBZH_SEARCH_SERVER, CUBZH_SEARCH_APIKEY)
	if err != nil {
		return err
	}

	// get all items drafts from mongoDB
	items, err := item.GetAll(dbClient)
	if err != nil {
		return err
	}

	for i, itm := range items {
		fmt.Println("    ItemDraft : updating", i+1, "/", len(items), "|", itm.Repo, itm.Name)
		itemDraft := search.NewItemDraftFromValues(
			itm.ID,
			itm.Repo,
			itm.Name,
			itm.ItemType,
			itm.Description,
			itm.Category,
			itm.Created,
			itm.Updated,
			int64(itm.BlockCount),
			itm.Banned,
			itm.Archived,
			itm.Likes,
			itm.Views)
		err = searchClient.UpsertItemDraft(itemDraft)
		if err != nil {
			return err
		}
	}

	fmt.Println("    Updated collection ItemDrafts successfully ✅")
	return nil
}

func searchItemDraft(c *cli.Context) error {
	fmt.Println("Searching ItemDrafts...")

	err := ensureSearchEngineEnvars()
	if err != nil {
		return err
	}

	args := c.Args() // array of strings

	// search engine client
	searchClient, err := search.NewClient(CUBZH_SEARCH_SERVER, CUBZH_SEARCH_APIKEY)
	if err != nil {
		return err
	}

	// var count int = 0
	var page int = 1
	var perPage int = 50
	filter := search.ItemDraftFilter{
		MinBlockCount: nil,
		Banned:        nil, // utils.NewFalse()
		Archived:      nil, // utils.NewFalse()
		Type:          nil, // "voxel" or "mesh"
	}
	sortBy := search.KItemDraftFieldLikes + ":desc"
	searchText := ""
	if len(args) > 0 {
		searchText = args[0]
	}
	results, err := searchClient.SearchCollectionItemDrafts(searchText, sortBy, page, perPage, filter)
	if err != nil {
		return err
	}

	for _, itm := range results.Results {
		fmt.Println(">", itm.Fullname, time.UnixMicro(itm.UpdatedAt))
	}

	return nil
}

// searchFeedAllItemDrafts gets all the Items in the mongoDB and inserts their info in the search engine DB
func searchFeedAllWorlds(c *cli.Context) error {
	fmt.Println("Updating collection : Worlds...")

	err := ensureSearchEngineEnvars()
	if err != nil {
		return err
	}

	// mongo client
	dbClient, err := dbclient.SharedUserDB()
	if err != nil {
		return err
	}

	// search engine client
	searchClient, err := search.NewClient(CUBZH_SEARCH_SERVER, CUBZH_SEARCH_APIKEY)
	if err != nil {
		return err
	}

	// get all records drafts from mongoDB
	records, err := game.GetAll(dbClient)
	if err != nil {
		return err
	}

	for i, record := range records {
		g, err := game.GetByID(dbClient, record.ID, "", nil) // TODO: gaetan: remove this line
		if err == nil && g != nil {
			fmt.Println("    World : updating", i+1, "/", len(records), "|", g.Title, g.AuthorName)
			isBanned := len(g.ContentWarnings) > 0
			world := search.NewWorldFromValues(
				g.ID,
				g.Title,
				g.Author,
				g.AuthorName,
				g.Created,
				g.Updated,
				g.Likes,
				g.Views,
				g.Featured,
				isBanned,
				g.Archived,
			)
			err = searchClient.UpsertWorld(world)
			if err != nil {
				return err
			}
		}
	}

	fmt.Println("    Updated collection Worlds successfully ✅")
	return nil
}

func searchWorld(c *cli.Context) error {
	fmt.Println("Searching Worlds...")

	err := ensureSearchEngineEnvars()
	if err != nil {
		return err
	}

	args := c.Args() // array of strings
	if len(args) == 0 {
		return errors.New("missing search query")
	}

	// search engine client
	searchClient, err := search.NewClient(CUBZH_SEARCH_SERVER, CUBZH_SEARCH_APIKEY)
	if err != nil {
		return err
	}

	// var count int = 0
	var sortBy string = "updatedAt:desc"
	var page int = 1
	var perPage int = 50
	filter := search.WorldFilter{}
	results, err := searchClient.SearchCollectionWorlds(args[0], sortBy, page, perPage, filter)
	if err != nil {
		return err
	}

	for _, itm := range results.Results {
		fmt.Println("-----")
		itm.Print()
	}

	return nil
}

// searchFeedAllItemDrafts gets all the Items in the mongoDB and inserts their info in the search engine DB
func searchFeedAllCreations(c *cli.Context) error {
	fmt.Println("Updating collection : Creations...")

	err := ensureSearchEngineEnvars()
	if err != nil {
		return err
	}

	// mongo client
	dbClient, err := dbclient.SharedUserDB()
	if err != nil {
		return err
	}

	// search engine client
	searchClient, err := search.NewClient(CUBZH_SEARCH_SERVER, CUBZH_SEARCH_APIKEY)
	if err != nil {
		return err
	}

	// WORLDS -> CREATIONS
	{
		// get all records drafts from mongoDB
		records, err := game.GetAll(dbClient)
		if err != nil {
			return err
		}

		for i, record := range records {
			w, err := game.GetByID(dbClient, record.ID, "", nil) // TODO: gaetan: remove this line (for now it's needed to get the author name value for instance)
			if err != nil {
				fmt.Println("    Creation (world) : skipping", i+1, "/", len(records), "|", record.ID, "| ❌", err.Error())
				continue
			}
			if w == nil {
				fmt.Println("    Creation (world) : skipping", i+1, "/", len(records), "|", record.ID, "| ❌", "game not found")
				continue
			}

			fmt.Println("    Creation (world) : updating", i+1, "/", len(records), "|", w.Title, w.AuthorName)
			isBanned := len(w.ContentWarnings) > 0
			// creationType, id, name, authorID, authorName, description, category string, createdAt, updatedAt time.Time, likes, views int32, featured, banned, archived bool, itemFullname string, itemBlockCount int64
			creationWorld, err := search.NewCreationFromValues(
				search.CreationTypeWorld,
				w.ID,
				w.Title,
				w.Author,
				w.AuthorName,
				w.Description,
				"", // category
				w.Created,
				w.Updated,
				w.Views,
				w.Likes,
				w.Featured,
				isBanned,
				w.Archived,
				"", // itemFullname
				0,  // itemBlockCount
				"", // itemType
			)
			if err != nil {
				return err
			}
			err = searchClient.UpsertCreation(creationWorld)
			if err != nil {
				return err
			}
		}
	}

	// ITEMS -> CREATIONS
	{
		// get all items drafts from mongoDB
		items, err := item.GetAll(dbClient)
		if err != nil {
			return err
		}

		for i, itm := range items {
			fmt.Println("    Creation (item) : updating", i+1, "/", len(items), "|", itm.Repo, itm.Name)
			// itemType might be empty for old items ("voxels" items)
			itemType := itm.ItemType
			if itemType == "" {
				itemType = "voxels"
			}
			creationItem, err := search.NewCreationFromValues(
				search.CreationTypeItem,
				itm.ID,
				itm.Name,
				itm.Author,
				itm.Repo, // authorName
				itm.Description,
				itm.Category,
				itm.Created,
				itm.Updated,
				itm.Views,
				itm.Likes,
				itm.Featured,
				itm.Banned,
				itm.Archived,
				itm.Repo+"."+itm.Name, // itemFullname
				int64(itm.BlockCount), // itemBlockCount
				itemType,              // itemType
			)
			if err != nil {
				return err
			}
			err = searchClient.UpsertCreation(creationItem)
			if err != nil {
				return err
			}
		}
	}

	fmt.Println("    Updated collection Creations successfully ✅")
	return nil
}

func searchCreation(c *cli.Context) error {
	fmt.Println("Searching Creations...")

	err := ensureSearchEngineEnvars()
	if err != nil {
		return err
	}

	args := c.Args() // array of strings
	if len(args) == 0 {
		return errors.New("missing search query")
	}

	// search engine client
	searchClient, err := search.NewClient(CUBZH_SEARCH_SERVER, CUBZH_SEARCH_APIKEY)
	if err != nil {
		return err
	}

	// var count int = 0
	var sortBy string = "updatedAt:desc"
	var page int = 1
	var perPage int = 50
	filter := search.CreationFilter{}
	results, err := searchClient.SearchCollectionCreations(args[0], sortBy, page, perPage, filter)
	if err != nil {
		return err
	}

	for _, creation := range results.Results {
		fmt.Println("-----")
		creation.Print()
	}

	return nil
}

// regularFileExists indicates whether a regular file exists at the given path.
// Returns false if file doesn't exists, or if it exists but is a directory.
// Otherwise, returns true.
// TODO: move this to cu.bzh/utils so it can be used in other Go modules
func regularFileExists(path string) (bool, error) {
	fileinfo, err := os.Stat(path)
	if err == nil {
		// No error, means the file exists.
		// Let's check it's a regular file and not a directory.
		return !fileinfo.IsDir(), nil
	}
	if errors.Is(err, fs.ErrNotExist) {
		// file doesn't exist
		return false, nil
	}
	return false, err
}

// itemsCountBlocksIn3zh counts blocks in all the .3zh files we have and update the BlockCount value in the search engine DB.
func itemsCountBlocksIn3zh(c *cli.Context) error {
	// retrieve flags values
	flagDryrun := c.Bool(kFlagDryrun)

	fmt.Printf("Indexing number of cubes... (dryrun: %t)\n", flagDryrun)

	dbClient, err := dbclient.SharedUserDB()
	if err != nil {
		return err
	}

	// iterate over all files, like in oneItemPerPcubes
	// get number of cubes + update DB entry

	fileList := make([]string, 0)
	err = filepath.Walk(pcubesDir, func(path string, f os.FileInfo, err error) error {
		fileList = append(fileList, path)
		return err
	})

	if err != nil {
		return err
	}

	for _, filePath := range fileList {

		arr := strings.Split(filePath, "/")
		l := len(arr)
		if l < 4 { // "", "pcubesDir", <repo>, <name>
			continue
		}

		repo := arr[l-2]
		name := strings.Split(arr[l-1], ".")[0]
		ext := filepath.Ext(filePath)

		if ext == ".3zh" {
			// all good, use that file!
		} else if ext == ".pcubes" {
			cubzhFilePath := filepath.Join("/particubes_items", repo, name+".3zh")
			cubzhVersionExists, err := regularFileExists(cubzhFilePath)
			if err != nil {
				fmt.Println("❌", "error trying to find .3zh version of .pcubes ("+filePath+") :", err.Error())
				continue
			}
			if cubzhVersionExists {
				// .3zh found, skip the .pcubes
				continue
			}
			// .3zh not found, ok to use .pcubes
		} else {
			// other extensions shouln't be considered (like etags)
			continue
		}

		if repo == "" || name == "" {
			if flagDryrun {
				fmt.Println("⚠️", "repo or name is empty. Skipping it.", filePath)
			}
			continue
		}

		fmt.Println(" ⚙️ ", "processing...", filePath)

		items, err := item.GetByRepoAndName(dbClient, repo, name)
		if err != nil {
			fmt.Println("  ❌", "failed to get item from DB:", err.Error())
			return err
		}

		if len(items) == 0 {
			if flagDryrun {
				fmt.Println("    ⚠️", "item not found in DB:", repo, "/", name)
			}
		} else {
			blockCount, err := item.ItemCountBlocks(filePath)
			if err != nil {
				fmt.Println("  ❌", "failed to count blocks in .3zh/.pcubes:", err.Error())
				continue
			}

			var itm *item.Item = items[0]
			itm.BlockCount = blockCount
			// saves the item without updating the `UpdatedAt` value (only if 'dryrun' is false)
			if !flagDryrun {
				fmt.Println("  [update DB] ➡️ ", "count:", blockCount)
				err = itm.SaveInternal(dbClient)
				if err != nil {
					fmt.Println("  ❌", "failed to save new block count in DB:", err.Error())
				}
			} else {
				fmt.Println("  [dryrun] ➡️ ", "count:", blockCount)
			}
		}
	}

	return nil
}

// func kvsMigrate(c *cli.Context) error {

// 	var legacyClient *scyllaclient.Client = nil
// 	var newClient *scyllaclient.Client = nil
// 	var err error

// 	// Create legacy Scylla client
// 	{
// 		config := scyllaclient.ClientConfig{
// 			Servers:   []string{"kvs-worlds"},
// 			Username:  "",
// 			Password:  "",
// 			AwsRegion: "",
// 			Keyspace:  "kvs",
// 		}
// 		legacyClient, err = scyllaclient.New(config)
// 		if err != nil {
// 			return err
// 		}
// 	}

// 	// Create new Scylla client
// 	{
// 		// read user/password from file
// 		user := ""
// 		pass := ""
// 		{
// 			content, err := os.ReadFile("/cubzh/secrets/CUBZH_SCYLLADB_USER")
// 			if err != nil {
// 				return err
// 			}
// 			user = string(content)
// 		}
// 		{
// 			content, err := os.ReadFile("/cubzh/secrets/CUBZH_SCYLLADB_PASS")
// 			if err != nil {
// 				return err
// 			}
// 			pass = string(content)
// 		}

// 		config := scyllaclient.ClientConfig{
// 			Servers: []string{
// 				"node-0.aws-us-east-2.cc51e1ddbf0b1a73b8aa.clusters.scylla.cloud",
// 				"node-1.aws-us-east-2.cc51e1ddbf0b1a73b8aa.clusters.scylla.cloud",
// 				"node-2.aws-us-east-2.cc51e1ddbf0b1a73b8aa.clusters.scylla.cloud",
// 			},
// 			Username:  user,
// 			Password:  pass,
// 			AwsRegion: "",
// 			Keyspace:  "kvs",
// 		}
// 		newClient, err = scyllaclient.New(config)
// 		if err != nil {
// 			return err
// 		}
// 	}

// 	// Get all keys from legacy KVS
// 	err = legacyClient.GetAll(func(rec scyllaclient.UnorderedRecord) {
// 		list, err := newClient.GetUnorderedValuesByKeys(rec.WorldId.String(), rec.Store, []string{rec.Key})
// 		if err != nil {
// 			fmt.Println("❌❌❌", err.Error())
// 		} else {
// 			if len(list) == 0 {
// 				fmt.Println("❌", rec.WorldId, rec.Store, rec.Key)
// 				//newClient.SetUnorderedValuesByKeys(rec.WorldId.String(), rec.Store, scyllaclient.KeyValueMap{rec.Key: rec.Value})
// 				err = newClient.SetUnorderedKeyValue(rec)
// 				if err != nil {
// 					fmt.Println("❌ INSERT ERROR ❌", err.Error())
// 				}
// 			} else if len(list) == 1 {
// 				fmt.Println("✅", rec.WorldId, rec.Store, rec.Key)
// 			} else {
// 				fmt.Println("🔥🔥🔥", "should not happen!!! <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<")
// 			}
// 		}
// 	}, 0)
// 	if err != nil {
// 		return err
// 	}

// 	return nil
// }

func storeModule(c *cli.Context) error {

	// var scyllaClientUniverse *scyllaclient.Client = nil
	// var err error

	// // Create scylla client for "moderation" keyspace
	// {
	// 	// read user/password from file
	// 	user := ""
	// 	pass := ""
	// 	{
	// 		content, err := os.ReadFile("/cubzh/secrets/CUBZH_SCYLLADB_USER")
	// 		if err != nil {
	// 			return err
	// 		}
	// 		user = string(content)
	// 	}
	// 	{
	// 		content, err := os.ReadFile("/cubzh/secrets/CUBZH_SCYLLADB_PASS")
	// 		if err != nil {
	// 			return err
	// 		}
	// 		pass = string(content)
	// 	}

	// 	config := scyllaclient.ClientConfig{
	// 		Servers: []string{
	// 			"node-0.aws-us-east-2.cc51e1ddbf0b1a73b8aa.clusters.scylla.cloud",
	// 			"node-1.aws-us-east-2.cc51e1ddbf0b1a73b8aa.clusters.scylla.cloud",
	// 			"node-2.aws-us-east-2.cc51e1ddbf0b1a73b8aa.clusters.scylla.cloud",
	// 		},
	// 		Username:  user,
	// 		Password:  pass,
	// 		AwsRegion: "",
	// 		Keyspace:  "universe",
	// 	}
	// 	scyllaClientUniverse, err = scyllaclient.NewShared(config)
	// 	if err != nil {
	// 		return err
	// 	}
	// }

	// rec := scyllaclient.UniverseModuleRecord{
	// 	Origin:  "test_origin",
	// 	Ref:     "test_ref",
	// 	Commit:  "test_commit",
	// 	Script:  "test_script",
	// 	Hash:    "test_hash",
	// 	Updated: time.Now(),
	// }

	// err = scyllaClientUniverse.UpsertUniverseModule(rec)
	// return err

	return errors.New("not implemented")
}

func friendRequestsSent(c *cli.Context) error {

	scyllaClient, err := getLocalhostScyllaClient("universe")
	if err != nil {
		return err
	}

	userID := c.Args().Get(0)

	arr, err := scyllaClient.GetFriendRequestsSent(userID)
	if err != nil {
		return err
	}

	for _, rec := range arr {
		fmt.Println("Record:", rec.ID1, rec.ID2, rec.Type, rec.CreatedAt)
	}
	fmt.Println("Records count:", len(arr))

	return nil
}

func friendRequestsReceived(c *cli.Context) error {

	scyllaClient, err := getLocalhostScyllaClient("universe")
	if err != nil {
		return err
	}

	userID := c.Args().Get(0)

	arr, err := scyllaClient.GetFriendRequestsReceived(userID)
	if err != nil {
		return err
	}

	for _, rec := range arr {
		fmt.Println("Record:", rec.ID1, rec.ID2, rec.Type, rec.CreatedAt)
	}
	fmt.Println("Records count:", len(arr))

	return nil
}

func friendRequestCancel(c *cli.Context) error {

	scyllaClient, err := getLocalhostScyllaClient("universe")
	if err != nil {
		return err
	}

	userID1 := c.Args().Get(0)
	userID2 := c.Args().Get(1)

	err = scyllaClient.DeleteFriendRequest(userID1, userID2)
	return err
}

func getFriendRelation(c *cli.Context) error {

	scyllaClient, err := getLocalhostScyllaClient("universe")
	if err != nil {
		return err
	}

	userID1 := c.Args().Get(0)
	userID2 := c.Args().Get(1)

	relation, err := scyllaClient.GetFriendRelation(userID1, userID2)
	if err != nil {
		return err
	}

	if relation == nil {
		fmt.Println("No relation found.")
	} else {
		fmt.Println("Relation:", relation)
	}

	return nil
}

func fixBugParentPhone(c *cli.Context) error {
	var parentIDs []string

	// list IDs of all parents from parent_dashboard table
	{
		records, err := scyllaClientUniverse.GetAllUniverseParentDashboards()
		if err != nil {
			return err
		}
		parentIDs = make([]string, len(records))
		for i, record := range records {
			parentIDs[i] = record.ParentID
		}
	}

	dbClient, err := dbclient.SharedUserDB()
	if err != nil {
		return err
	}

	// For each parent, get user from user table
	// and move their phone number from ParentPhone(New) to Phone(New)
	for _, parentID := range parentIDs {
		fmt.Println(">>> Fixing parent:", parentID)
		usr, err := user.GetByID(dbClient, parentID)
		if err != nil {
			fmt.Println("  ❌", err.Error())
			return err
		}
		// if usr.ParentPhone != nil && usr.Phone == nil {
		// 	fmt.Println("  🛠️ [DRY RUN] Moving phone number from ParentPhone to Phone")
		// }
		if usr.ParentPhoneNew != nil && usr.PhoneNew == nil {
			fmt.Println("  🛠️ [DRY RUN] Moving phone number from ParentPhoneNew to PhoneNew")
			usr.PhoneNew = usr.ParentPhoneNew
			usr.ParentPhoneNew = nil
			err = usr.Save(dbClient)
			if err != nil {
				fmt.Println("  ❌ DB SAVE:", err.Error())
				return err
			}
		}
	}

	fmt.Println("🎉 Done!")
	return nil // success
}

// func iterFriendRelationsFDB(c *cli.Context) error {

// 	dbClient, err := dbclient.SharedUserDB()
// 	if err != nil {
// 		return err
// 	}

// 	kvsClient, err := kvs.Shared()
// 	if err != nil {
// 		return err
// 	}

// 	// Create new Scylla client
// 	var newClient *scyllaclient.Client = nil
// 	{
// 		// read user/password from file
// 		user := ""
// 		pass := ""
// 		{
// 			content, err := os.ReadFile("/cubzh/secrets/CUBZH_SCYLLADB_USER")
// 			if err != nil {
// 				return err
// 			}
// 			user = string(content)
// 		}
// 		{
// 			content, err := os.ReadFile("/cubzh/secrets/CUBZH_SCYLLADB_PASS")
// 			if err != nil {
// 				return err
// 			}
// 			pass = string(content)
// 		}

// 		config := scyllaclient.ClientConfig{
// 			Servers: []string{
// 				"node-0.aws-us-east-2.cc51e1ddbf0b1a73b8aa.clusters.scylla.cloud",
// 				"node-1.aws-us-east-2.cc51e1ddbf0b1a73b8aa.clusters.scylla.cloud",
// 				"node-2.aws-us-east-2.cc51e1ddbf0b1a73b8aa.clusters.scylla.cloud",
// 			},
// 			Username:  user,
// 			Password:  pass,
// 			AwsRegion: "",
// 			Keyspace:  "universe",
// 		}
// 		newClient, err = scyllaclient.New(config)
// 		if err != nil {
// 			return err
// 		}
// 	}

// 	// // Friend requests SENT
// 	// {
// 	// 	arr, err := kvsClient.GetAllFriendRequestsSent()
// 	// 	if err != nil {
// 	// 		return err
// 	// 	}

// 	// 	for _, s := range arr {

// 	// 		senderID := s.SenderID
// 	// 		senderUsr, err := user.GetByID(dbClient, senderID)
// 	// 		if err != nil {
// 	// 			fmt.Println("❌", "error getting user by ID:", senderID, err.Error())
// 	// 			continue
// 	// 		}
// 	// 		if senderUsr == nil {
// 	// 			fmt.Println("❌", "user not found by ID:", senderID)
// 	// 			continue
// 	// 		}
// 	// 		senderUsername := senderUsr.Username

// 	// 		recipientID := s.RecipientID
// 	// 		recipientUsr, err := user.GetByID(dbClient, recipientID)
// 	// 		if err != nil {
// 	// 			fmt.Println("❌", "error getting user by ID:", recipientID, err.Error())
// 	// 			continue
// 	// 		}
// 	// 		if recipientUsr == nil {
// 	// 			fmt.Println("❌", "user not found by ID:", recipientID)
// 	// 			continue
// 	// 		}
// 	// 		recipientUsername := recipientUsr.Username

// 	// 		relation, err := newClient.GetFriendRequest(senderID, recipientID)
// 	// 		if err != nil {
// 	// 			fmt.Println("❌", "error getting friend relation (request):", err.Error())
// 	// 			continue
// 	// 		}
// 	// 		if relation != nil {
// 	// 			fmt.Println("✅", "already exists:", senderUsername, recipientUsername)
// 	// 			continue
// 	// 		}

// 	// 		// _ = senderUsername
// 	// 		// _ = recipientUsername
// 	// 		// fmt.Println("🔥 TO MIGRATE -> SENT", senderID, recipientID, senderUsername, recipientUsername)

// 	// 		res, err := newClient.InsertFriendRequest(senderID, senderUsername, recipientID, recipientUsername)
// 	// 		if err != nil {
// 	// 			fmt.Println("❌", "failed to migrate:", senderUsername, recipientUsername, err.Error())
// 	// 			continue
// 	// 		}
// 	// 		if res != scyllaclient.FriendRequestCreated {
// 	// 			fmt.Println("❌", "failed to migrate:", senderUsername, recipientUsername, res)
// 	// 			continue
// 	// 		}
// 	// 	}
// 	// }

// 	// migrate friend relations
// 	{
// 		arr, err := kvsClient.GetAllFriendRelations()
// 		if err != nil {
// 			return err
// 		}

// 		for _, s := range arr {

// 			user1ID := s.ID1
// 			user1, err := user.GetByID(dbClient, user1ID)
// 			if err != nil {
// 				fmt.Println("❌", "error getting user by ID:", user1ID, err.Error())
// 				continue
// 			}
// 			if user1 == nil {
// 				fmt.Println("❌", "user not found by ID:", user1ID)
// 				continue
// 			}
// 			user1Username := user1.Username
// 			if len(user1Username) == 0 {
// 				continue
// 			}

// 			user2ID := s.ID2
// 			user2, err := user.GetByID(dbClient, user2ID)
// 			if err != nil {
// 				fmt.Println("❌", "error getting user by ID:", user2ID, err.Error())
// 				continue
// 			}
// 			if user2 == nil {
// 				fmt.Println("❌", "user not found by ID:", user2ID)
// 				continue
// 			}
// 			user2Username := user2.Username
// 			if len(user2Username) == 0 {
// 				continue
// 			}

// 			// check if friend relation already exists
// 			{
// 				relation, err := newClient.GetFriendRelation(user1ID, user2ID)
// 				if err != nil {
// 					fmt.Println("❌", "error getting friend relation (request):", err.Error())
// 					continue
// 				}
// 				if relation != nil {
// 					fmt.Printf(".")
// 					continue
// 				}
// 			}

// 			// fmt.Println("🔥 migrating...", user1ID, user2ID, user1Username, user2Username)

// 			// err = newClient.InsertFriendRelation(user1ID, user1Username, user2ID, user2Username)
// 			// if err != nil {
// 			// 	fmt.Println("❌", "failed to migrate:", user1Username, user2Username, err.Error())
// 			// 	continue
// 			// }
// 		}
// 	}

// 	return nil
// }

func newFriendRequest(c *cli.Context) error {

	scyllaClient, err := getLocalhostScyllaClient("universe")
	if err != nil {
		return err
	}

	userID1 := c.Args().Get(0)
	username1 := c.Args().Get(1)
	userID2 := c.Args().Get(2)
	username2 := c.Args().Get(3)

	fmt.Println("userID1:", userID1, "username1:", username1, "userID2:", userID2, "username2:", username2)

	// var result scyllaclient.FriendRequestResult
	_ /*result*/, err = scyllaClient.InsertFriendRequest(userID1, username1, userID2, username2)
	return err
}

func dev(c *cli.Context) error {
	// fmt.Println("🤖 dev command")

	// scyllaClientUniverse, err := getLocalhostScyllaClient("universe")
	// if err != nil {
	// 	return err
	// }

	// err = scyllaClientUniverse.DeleteAllRelationsForUser("00000000-0000-0000-0000-000000000001")

	return nil
}

func getStatsUnder13(c *cli.Context) error {

	dbClient, err := dbclient.SharedUserDB()
	if err != nil {
		return err
	}

	// Get the current time
	currentTime := time.Now()

	// Subtract 13 years from the current time
	thirteenYearsAgo := currentTime.AddDate(-13, 0, 0)

	// Subtract 1 month from the current time
	creationLimit := currentTime.AddDate(0, -1, 0)

	{
		// Count all users under 13 years old
		countUnder13, err := user.CountUsersBornAfter(dbClient, thirteenYearsAgo)
		if err != nil {
			return err
		}

		// Count all users
		countAll, err := user.CountAll(dbClient)
		if err != nil {
			return err
		}

		fmt.Println("💡 [Overall] Under 13 years old:", countUnder13, "| total:", countAll)
	}

	{
		filter1 := map[string]interface{}{
			"dob":     map[string]interface{}{"$gt": thirteenYearsAgo},
			"created": map[string]interface{}{"$gt": creationLimit},
		}
		countUnder13, err := user.CountUsers(dbClient, filter1)
		if err != nil {
			return err
		}

		filter2 := map[string]interface{}{
			"created": map[string]interface{}{"$gt": creationLimit},
		}
		countAll, err := user.CountUsers(dbClient, filter2)
		if err != nil {
			return err
		}

		fmt.Println("💡 [Last month] Under 13 years old:", countUnder13, "| total:", countAll)
	}

	return nil
}

func readModule(c *cli.Context) error {

	// var scyllaClientUniverse *scyllaclient.Client = nil
	// var err error

	// // Create scylla client for "moderation" keyspace
	// {
	// 	// read user/password from file
	// 	user := ""
	// 	pass := ""
	// 	{
	// 		content, err := os.ReadFile("/cubzh/secrets/CUBZH_SCYLLADB_USER")
	// 		if err != nil {
	// 			return err
	// 		}
	// 		user = string(content)
	// 	}
	// 	{
	// 		content, err := os.ReadFile("/cubzh/secrets/CUBZH_SCYLLADB_PASS")
	// 		if err != nil {
	// 			return err
	// 		}
	// 		pass = string(content)
	// 	}

	// 	config := scyllaclient.ClientConfig{
	// 		Servers: []string{
	// 			"node-0.aws-us-east-2.cc51e1ddbf0b1a73b8aa.clusters.scylla.cloud",
	// 			"node-1.aws-us-east-2.cc51e1ddbf0b1a73b8aa.clusters.scylla.cloud",
	// 			"node-2.aws-us-east-2.cc51e1ddbf0b1a73b8aa.clusters.scylla.cloud",
	// 		},
	// 		Username:  user,
	// 		Password:  pass,
	// 		AwsRegion: "",
	// 		Keyspace:  "universe",
	// 	}
	// 	scyllaClientUniverse, err = scyllaclient.NewShared(config)
	// 	if err != nil {
	// 		return err
	// 	}
	// }

	// var rec *scyllaclient.UniverseModuleRecord = nil
	// rec, err = scyllaClientUniverse.GetUniverseModule("test_origin", "test_ref")
	// if err != nil {
	// 	return err
	// }

	// fmt.Println("Record:", rec.Commit, rec.Script, rec.Hash, rec.Updated)
	// return nil

	return errors.New("not implemented")
}

func friendsTests(c *cli.Context) error {
	const (
		// user1
		userID1   = "00000000-0000-0000-0000-000000000001"
		username1 = "username1"
		// user2
		userID2   = "00000000-0000-0000-0000-000000000002"
		username2 = "username2"
	)

	// Create scylla client for "universe" keyspace
	scyllaClient, err := getLocalhostScyllaClient("universe")
	if err != nil {
		return err
	}

	// Create a friend request from user1 to user2
	{
		result, err := scyllaClient.InsertFriendRequest(userID1, username1, userID2, username2)
		if err != nil {
			return err
		}
		if result != scyllaclient.FriendRequestCreated {
			return errors.New("failed to create friend request")
		}
	}
	fmt.Println("✅ Friend request created")

	// Get friend requests sent by user1
	{
		arr, err := scyllaClient.GetFriendRequestsSent(userID1)
		if err != nil {
			return err
		}
		if len(arr) != 1 {
			return errors.New("unexpected number of friend requests sent")
		}
	}
	{
		arr, err := scyllaClient.GetFriendRequestsSentAndReceived(userID1)
		if err != nil {
			return err
		}
		if len(arr) != 1 {
			return errors.New("unexpected number of friend requests sent")
		}
	}

	// Get friend requests sent by user2
	{
		arr, err := scyllaClient.GetFriendRequestsSent(userID2)
		if err != nil {
			return err
		}
		if len(arr) != 0 {
			return errors.New("unexpected number of friend requests sent")
		}
	}
	{
		arr, err := scyllaClient.GetFriendRequestsSentAndReceived(userID2)
		if err != nil {
			return err
		}
		if len(arr) != 1 {
			return errors.New("unexpected number of friend requests sent")
		}
	}
	fmt.Println("✅ Friend requests sent OK")

	// Get friend requests received by user1
	{
		arr, err := scyllaClient.GetFriendRequestsReceived(userID1)
		if err != nil {
			return err
		}
		if len(arr) != 0 {
			return errors.New("unexpected number of friend requests received")
		}
	}

	// Get friend requests received by user2
	{
		arr, err := scyllaClient.GetFriendRequestsReceived(userID2)
		if err != nil {
			return err
		}
		if len(arr) != 1 {
			return errors.New("unexpected number of friend requests received")
		}
	}
	fmt.Println("✅ Friend requests received OK")

	// Cancel the friend request
	{
		{
			err = scyllaClient.DeleteFriendRequest(userID1, userID2)
			if err != nil {
				return err
			}
		}
		{
			arr, err := scyllaClient.GetFriendRequestsSent(userID1)
			if err != nil {
				return err
			}
			if len(arr) != 0 {
				return errors.New("unexpected number of friend requests sent")
			}
		}
		{
			arr, err := scyllaClient.GetFriendRequestsReceived(userID2)
			if err != nil {
				return err
			}
			if len(arr) != 0 {
				return errors.New("unexpected number of friend requests received")
			}
		}
	}
	fmt.Println("✅ Friend request cancelled")

	// Re-create a friend request from user1 to user2 and have user2 accept it
	{
		{
			result, err := scyllaClient.InsertFriendRequest(userID1, username1, userID2, username2)
			if err != nil {
				return err
			}
			if result != scyllaclient.FriendRequestCreated {
				return errors.New("failed to create friend request")
			}
		}
		{
			err := scyllaClient.AcceptFriendRequest(userID1, userID2)
			if err != nil {
				return err
			}
		}
		{
			arr, err := scyllaClient.GetFriendRequestsSent(userID1)
			if err != nil {
				return err
			}
			if len(arr) != 0 {
				return errors.New("unexpected number of friend requests sent")
			}
		}
		{
			arr, err := scyllaClient.GetFriendRequestsReceived(userID2)
			if err != nil {
				return err
			}
			if len(arr) != 0 {
				return errors.New("unexpected number of friend requests received")
			}
		}
		{
			relation, err := scyllaClient.GetFriendRelation(userID1, userID2)
			if err != nil {
				return err
			}
			if relation == nil {
				return errors.New("no friend relation found")
			}
			if relation.ID1.String() != userID1 ||
				relation.ID2.String() != userID2 ||
				relation.Name1 != username1 ||
				relation.Name2 != username2 {
				return errors.New("unexpected friend relation")
			}
			relation, err = scyllaClient.GetFriendRelation(userID2, userID1)
			if err != nil {
				return err
			}
			if relation == nil {
				return errors.New("no friend relation found")
			}
			if relation.ID1.String() != userID2 ||
				relation.ID2.String() != userID1 ||
				relation.Name1 != username2 ||
				relation.Name2 != username1 {
				return errors.New("unexpected friend relation")
			}
		}
	}
	fmt.Println("✅ Accept friend request")

	// Remove the friend relation
	{
		err := scyllaClient.DeleteFriendRelation(userID1, userID2)
		if err != nil {
			return err
		}
		relation, err := scyllaClient.GetFriendRelation(userID1, userID2)
		if err != nil {
			return err
		}
		if relation != nil {
			return errors.New("unexpected friend relation")
		}
		relation, err = scyllaClient.GetFriendRelation(userID2, userID1)
		if err != nil {
			return err
		}
		if relation != nil {
			return errors.New("unexpected friend relation")
		}
	}
	fmt.Println("✅ Delete friend relation")

	// DB TABLE is NOW EMPTY

	// Try to convert an opposite friend request into a friend relation
	{
		// REQ: usr1 -> usr2
		result, err := scyllaClient.InsertFriendRequest(userID1, username1, userID2, username2)
		if err != nil {
			return err
		}
		if result != scyllaclient.FriendRequestCreated {
			return errors.New("failed to create friend request")
		}
		// REQ: usr2 -> usr1
		result, err = scyllaClient.InsertFriendRequest(userID2, username2, userID1, username1)
		if err != nil {
			return err
		}
		if result != scyllaclient.FriendRequestConvertedIntoFriendRelation {
			return errors.New("failed to convert opposite friend request into friend relation")
		}
		// cleanup
		err = scyllaClient.DeleteFriendRelation(userID1, userID2)
		if err != nil {
			return err
		}
	}
	fmt.Println("✅ Friend request converted into friend relation")

	return nil
}

func emailConfirmationReminders(c *cli.Context) error {

	// dbClient, err := dbclient.SharedUserDB()
	// if err != nil {
	// 	return err
	// }

	// users, err := user.GetAll(dbClient)
	// if err != nil {
	// 	return err
	// }

	// for _, u := range users {

	// 	fmt.Println("Username:", u.Email, "Hash:", u.EmailVerifCode, "confirmed:", u.EmailVerified)

	// 	if u.EmailVerified == false {
	// 		// mark old accounts (the ones without confirm hashes as confirmed)
	// 		if u.EmailVerifCode == "" {

	// 			uuid, err := utils.UUID()
	// 			if err != nil {
	// 				fmt.Println("❌", err.Error())
	// 				continue
	// 			}

	// 			u.EmailVerifCode = uuid
	// 			u.EmailVerified = true

	// 			err = u.Save(dbClient)
	// 			if err != nil {
	// 				fmt.Println("❌", err.Error())
	// 				continue
	// 			}

	// 			fmt.Println("=> SET HASH:", u.EmailVerifCode)

	// 		} else {

	// 			fmt.Println("=> WAITING FOR CONFIRMATION")

	// 			/*
	// 				// send confirmation email
	// 				fmt.Println("=> SEND EMAIL")

	// 				var content struct {
	// 					Link string
	// 				}

	// 				content.Link = "https://particubes.com/confirm/" + u.EmailConfirmHash

	// 				err := mailer.Send(
	// 					u.Email,
	// 					"", // email address used as name, because we don't have it
	// 					"./email-template.html",
	// 					"./email_emailAddressConfirmation.md",
	// 					MAILJET_API_KEY,
	// 					MAILJET_API_SECRET,
	// 					&content,
	// 					"particubes-email-confirm",
	// 				)

	// 				if err != nil {
	// 					fmt.Println("❌", err.Error())
	// 					continue
	// 				}
	// 			*/
	// 		}
	// 	}

	// 	fmt.Println("----------")
	// }

	// return nil
	return errors.New("not implemented")
}

// "kenydbzgt@gmail.com Kenygia 9/7/1993, jeandegouttes@gmail.com Nodacrux 4/15/1995"
// func insertAirtableTester(c *cli.Context) error {
// 	if c.NArg() < 1 {
// 		return errors.New("missing user data to insert in DB")
// 	}

// 	dataFilePath := c.Args().Get(0)
// 	dataBytes, err := ioutil.ReadFile(dataFilePath)
// 	if err != nil {
// 		fmt.Println("🔥 error: ", err.Error())
// 		return err
// 	}
// 	data := string(dataBytes)

// 	client, err := dbclient.SharedUserDB()
// 	if err != nil {
// 		fmt.Println("  ⚠️ failed to connect to mongo DB:", err)
// 		return err
// 	}
// 	collection := client.MongoClient.Database("particubes").Collection("users")

// 	err = user.EnsureIndexes(collection)
// 	if err != nil {
// 		fmt.Println("  ⚠️ failed to ensure indexes:", err)
// 		return err
// 	}

// 	// a-z and 0-9 and _
// 	regex, err := regexp.Compile("^[a-z0-9_]*$")
// 	if err != nil {
// 		fmt.Println("🔥 error: ", err.Error())
// 		return err
// 	}

// 	dataElements := strings.Split(data, ", ")
// 	fmt.Println("Found", len(dataElements), "users to insert in DB")

// 	for _, e := range dataElements {
// 		fields := strings.Split(e, " ")
// 		if len(fields) != 3 {
// 			fmt.Println("  ⚠️ failed to import:", e, "|", len(fields))
// 			continue
// 		}
// 		email := fields[0]
// 		username := fields[1]
// 		dob := fields[2]
// 		username = strings.ToLower(username)

// 		if regex.MatchString(username) == false {
// 			fmt.Println("  ⚠️ failed to import:", e)
// 			continue
// 		}

// 		fmt.Printf("  ⚙️ importing... <%s> <%s> <%s>\n", email, username, dob)

// 		newUser, err := user.New()
// 		if err != nil {
// 			fmt.Println("  ⚠️ failed to create new user:", err)
// 			continue
// 		}
// 		newUser.Username = username
// 		if newUser.Username == "_" {
// 			newUser.Username = ""
// 		}
// 		newUser.Email = email

// 		// dob is month/day/year
// 		dobElems := strings.Split(dob, "/")
// 		if len(dobElems) != 3 {
// 			fmt.Println("  ⚠️ failed to parse date of birth:", dobElems)
// 			continue
// 		}
// 		yearInt, err := strconv.ParseInt(dobElems[2], 10, 32)
// 		if err != nil {
// 			fmt.Println("  ⚠️ failed to parse dob year:", err)
// 			continue
// 		}
// 		monthInt, err := strconv.ParseInt(dobElems[0], 10, 32)
// 		if err != nil {
// 			fmt.Println("  ⚠️ failed to parse dob month:", err)
// 			continue
// 		}
// 		dayInt, err := strconv.ParseInt(dobElems[1], 10, 32)
// 		if err != nil {
// 			fmt.Println("  ⚠️ failed to parse dob day:", err)
// 			continue
// 		}
// 		dobTime := time.Date(int(yearInt), time.Month(int(monthInt)), int(dayInt), 0, 0, 0, 0, time.UTC)
// 		newUser.Dob = &dobTime
// 		err = newUser.Save(collection)
// 		if err != nil {
// 			fmt.Println("  ⚠️ failed to save new user in DB:", err)
// 			continue
// 		}
// 		fmt.Println("✅ user saved. ", e)
// 	}

// 	fmt.Println("✨data import end")
// 	return nil
// }

// --------------------
// USER
// --------------------

func addUser(c *cli.Context) error {

	u, err := user.New()
	if err != nil {
		return err
	}
	u.Username = user.UsernameOrEmailSanitize(readString("username: ", ""))
	u.Email = user.UsernameOrEmailSanitize(readString("email: ", ""))
	phoneValue := readString("phone: ", "")
	if phoneValue != "" {
		u.Phone = &phoneValue
		u.PhoneNew = nil
		u.PhoneVerifCode = nil
	}

	// pass := readString("password: ", "")
	// u.SetPassword(pass)
	pass := user.GeneratePassword(10)
	// fmt.Println("password:", pass)
	u.SetPassword(pass)

	u.FirstName = readString("first name: ", "")
	u.LastName = readString("last name: ", "")

	err = errors.New("need date of birth")

	var parsedDob time.Time

	for err != nil {
		dob := readString("date of birth (mm/dd/yyyy): ", "")
		parsedDob, err = time.Parse("01/02/2006", dob)

		if err == nil {
			u.Dob = &parsedDob
		} else {
			fmt.Println(err.Error())
		}
	}

	// add default game?
	err = errors.New("expecting y or n")
	createDefaultGame := false

	for err != nil {
		answer := readString("create default game? (y/n): ", "")
		if answer == "y" {
			createDefaultGame = true
			err = nil
		} else if answer == "n" {
			err = nil
		}
	}

	dbClient, err := dbclient.SharedUserDB()
	if err != nil {
		return err
	}

	err = u.Save(dbClient)
	if err != nil {
		return err
	}

	// u.ID should be defined now

	if createDefaultGame {
		newGame, err := game.New()
		if err != nil {
			return err
		}
		newGame.Title = u.Username
		newGame.Author = u.ID
		newGame.Save(dbClient, nil)
		fmt.Println("new game with title:", newGame.Title, "and author ID:", newGame.ID)
	}

	// SEND EMAIL

	var content struct {
		Username string
		Password string
	}

	content.Username = u.Username
	content.Password = pass

	err = mailer.Send(u.Email, u.Username, "./email-template.html", "./email-user-password.md", MAILJET_API_KEY, MAILJET_API_SECRET, &content, "particubes-cli-add-user")
	if err != nil {
		return err
	}

	return nil
}

func userGenPassword(c *cli.Context) error {

	if c.NArg() < 1 {
		return errors.New("missing username")
	}

	dbClient, err := dbclient.SharedUserDB()
	if err != nil {
		return err
	}

	username := c.Args().Get(0)
	u, err := user.GetByUsername(dbClient, username)
	if err != nil {
		return err
	}

	pass := readString("password (empty to generate): ", "")
	if pass == "" {
		pass = user.GeneratePassword(10)
	}
	u.SetPassword(pass)

	err = u.Save(dbClient)
	if err != nil {
		return err
	}

	// SEND EMAIL

	var content struct {
		Username string
		Password string
	}

	content.Username = u.Username
	content.Password = pass

	err = mailer.Send(u.Email, u.Username, "./email-template.html", "./email-user-password.md", MAILJET_API_KEY, MAILJET_API_SECRET, &content, "particubes-cli-gen-password")
	if err != nil {
		return err
	}

	return nil
}

func updateUser(c *cli.Context) error {

	if c.NArg() < 1 {
		return errors.New("missing username")
	}

	dbClient, err := dbclient.SharedUserDB()
	if err != nil {
		return err
	}

	username := c.Args().Get(0)
	u, err := user.GetByUsername(dbClient, username)
	if err != nil {
		return err
	}

	u.Username = readString("username: ", u.Username)
	u.Email = readString("email: ", u.Email)

	// pass := readString("password: ")
	// u.SetPassword(pass)

	u.FirstName = readString("first name: ", u.FirstName)
	u.LastName = readString("last name: ", u.LastName)

	// err = errors.New("need date of birth")
	// for err != nil {
	// 	dob := readString("date of birth (mm/dd/yyyy): ")
	// 	u.Dob, err = time.Parse("01/02/2006", dob)
	// }

	err = u.Save(dbClient)
	if err != nil {
		return err
	}

	return nil
}

func hasBetaAccess(u *user.User) bool {

	if u.Number > 0 && u.Number <= 2200 {
		return true
	}

	earlyAccess := []int{
		1925, // jacobvan3d@gmail.com / DNFF6-DKFI9-0L7W6 (active builder, JacobVan on Discord)
		1172, // maxime@madboxgames.io / 6FKRR-49KY3-IHZIW
		2167, // tavrox@tavroxgames.com / ZGVWX-YE3ZZ-JETNV
		2168, // chloematz@tavroxgames.com / E8LVI-AJ8NT-55IY3
		2352, // julie.mascaro@gmail.com / CIK9Y-V3C9E-8QQLI
		1971, // y.phoenix@nemesis.ovh / AZAPT-KYW2I-ICX4N
		2354, // q2007r@gmail.com / XHEXQ-2WYQT-I09IQ (active scripter, creeperQ7 on Discord)
		2698, // govind.cacciatore@miniclip.com (Miniclip) / 9AF2G-6H8YY-34BNE
		2714, // juliette@isai.fr (ISAI, investor) / XG8MD-H4Z3D-XIABN
		3141, // jean@2lr.com (Jean de La Rochebrochard, investor @ Kima & New Wave) / 977I7-IZWA2-L7IGX
		3106, // lars@eqtventures.com (Lars Jörnow, investor @ EQT Ventures) / 53MTB-FWDF6-4HDZH
		3134, // jason@konvoy.vc (Jacon Chapmain, investor @ Konvoy) / DWQHF-LCDKW-FRHP5
		3240, // esellin@gmail.com (kid of Cedric Sellin) /
		3235, // gabe.sellin@gmail.com (kid of Cedric Sellin) /
		2667, // bendanmetzer07@icloud.com (very active on Discord)
		3495, // antoine@alumni.ie.edu (EQT ventures) / FDFYR-2T7EM-XM30H
		3531, // olivier@serena.vc (Serena ventures) / JIA7V-B694Y-NYJHP
	}

	for _, number := range earlyAccess {
		if u.Number == number {
			return true
		}
	}

	return false
}

func syncMailjet(c *cli.Context) error {

	if c.NArg() < 1 {
		return errors.New("missing list ID")
	}

	listID, err := strconv.Atoi(c.Args().Get(0))
	if err != nil {
		return err
	}

	listIDInt64 := int64(listID)

	dbClient, err := dbclient.SharedUserDB()
	if err != nil {
		return err
	}

	users, err := user.GetAll(dbClient)
	if err != nil {
		return err
	}

	emailAndNames := make([]*mailer.Contact, 0)

	var greetings string

	for _, u := range users {

		emailToUse := u.Email
		if emailToUse == "" {
			emailToUse = u.ParentEmail
		}

		if emailToUse != "" {
			if u.Username != "" {
				greetings = "Hi " + u.Username + "!"
			} else {
				greetings = "Hi!"
			}

			contact := &mailer.Contact{
				Email: emailToUse,
				Name:  u.Username,
				Properties: &mailer.ContactProperties{
					UserID:        u.ID,
					Username:      u.Username,
					Greetings:     greetings,
					Usernumber:    u.Number,
					HasBetaAccess: hasBetaAccess(u),
					SteamKey:      u.SteamKey,
					Email:         emailToUse,
				},
			}
			emailAndNames = append(emailAndNames, contact)
		}
	}

	err = mailer.AddContacts(MAILJET_API_KEY, MAILJET_API_SECRET, emailAndNames, listIDInt64)

	return err
}

func infoUser(c *cli.Context) error {
	if c.NArg() < 1 {
		return errors.New("missing username")
	}

	dbClient, err := dbclient.SharedUserDB()
	if err != nil {
		return err
	}

	username := c.Args().Get(0)
	u, err := user.GetByUsername(dbClient, username)
	if err != nil {
		return err
	}

	fmt.Println("----------")
	fmt.Println("ID:", u.ID)
	fmt.Println("Username:", u.Username)
	fmt.Println("Email:", u.Email)
	fmt.Println("Phone:", u.Phone)
	fmt.Println("----------")

	return nil
}

func sanitize(c *cli.Context) error {
	dbClient, err := dbclient.SharedUserDB()
	if err != nil {
		return err
	}

	users, err := user.GetAll(dbClient)
	if err != nil {
		return err
	}

	var saveUser bool

	for _, u := range users {

		saveUser = false

		sanitizedEmail := user.SanitizeEmail(u.Email)
		if sanitizedEmail != u.Email {
			fmt.Println(u.Email + " -> " + sanitizedEmail)
			saveUser = true
		}

		if saveUser {
			err := u.Save(dbClient)
			if err != nil {
				fmt.Println("could not save user:", err.Error())
			} else {
				fmt.Println("saved!")
			}
		}
	}

	return nil
}

// Display user stats
func userStats(c *cli.Context) error {

	dbClient, err := dbclient.SharedUserDB()
	if err != nil {
		return err
	}

	users, err := user.GetAll(dbClient)
	if err != nil {
		return err
	}

	developerCount := 0
	artistCount := 0
	userWithEmailCount := 0

	for _, u := range users {
		// fmt.Println("> Processing user", u.Username, u.ID)

		// check if user has at least one game that doesn't have the default script
		games, err := game.GetByAuthor(dbClient, u.ID)
		if err != nil {
			return err
		}

		for _, g := range games {
			if g.DefaultScript == false {
				u.IsDeveloper = true
				developerCount += 1
				break
			}
		}

		// check if user has at least one item
		items, err := item.List(dbClient, "list:mine", u.ID)
		if err != nil {
			return err
		}
		if len(items) > 0 {
			u.IsArtist = true
			artistCount += 1
		}

		// check if user has an email address on record
		if u.Email != "" {
			userWithEmailCount += 1
		}
	}

	var userCount = len(users)
	fmt.Println("Total users:", userCount)
	fmt.Println("Total developers:", developerCount, "(", 100.0*float32(developerCount)/float32(userCount), "% )")
	fmt.Println("Total artists:", artistCount, "(", 100.0*float32(artistCount)/float32(userCount), "% )")
	fmt.Println("Total users with email:", userWithEmailCount, "(", 100.0*float32(userWithEmailCount)/float32(userCount), "% )")

	return nil
}

func oneGamePerUser(c *cli.Context) error {
	dbClient, err := dbclient.SharedUserDB()
	if err != nil {
		return err
	}

	users, err := user.GetAll(dbClient)
	if err != nil {
		return err
	}

	for _, user := range users {
		fmt.Println("-", user.ID, "("+user.Username+")")

		games, err := game.GetByAuthor(dbClient, user.ID)
		if err != nil {
			return err
		}

		nbGames := len(games)
		foundGameWithUsernameTitle := false

		for _, g := range games {
			fmt.Println("    -", g.ID, "("+g.Title+")")
			if g.Title == user.Username {
				foundGameWithUsernameTitle = true
			}
		}

		if foundGameWithUsernameTitle == false {
			if nbGames > 0 {
				fmt.Println("game title:", games[0].Title, "->", user.Username)
				games[0].Title = user.Username
				games[0].Save(dbClient, nil)
			} else {
				newGame, err := game.New()
				if err != nil {
					return err
				}
				newGame.Title = user.Username
				newGame.Author = user.ID
				newGame.Save(dbClient, nil)
				fmt.Println("new game with title:", user.Username)
			}
		}
	}

	return nil
}

func assignUserNumbers(c *cli.Context) error {
	dbClient, err := dbclient.SharedUserDB()
	if err != nil {
		return err
	}

	currentNumber := 0

	users, err := user.GetAll(dbClient)
	if err != nil {
		return err
	}

	for _, u := range users {
		if u.Number > currentNumber {
			currentNumber = u.Number
		}
	}

	currentNumber += 1

	user.SortByCreationDate(users)

	for _, u := range users {
		if u.Number == 0 {
			u.Number = currentNumber
			currentNumber += 1
			err = u.Save(dbClient)
			if err != nil {
				return err
			}
		}
	}

	return nil
}

func assignUserSteamKeys(c *cli.Context) error {
	dbClient, err := dbclient.SharedUserDB()
	if err != nil {
		return err
	}

	nbKeys := c.NArg()

	if nbKeys < 1 {
		return errors.New("no steam keys to assign")
	}

	currentSteamKey := 0

	users, err := user.GetAll(dbClient)
	if err != nil {
		return err
	}

	user.SortByCreationDate(users)

	for _, u := range users {
		if u.SteamKey == "" {
			steamKey := c.Args().Get(currentSteamKey)
			u.SteamKey = steamKey
			currentSteamKey += 1
			fmt.Println("assigned Steam key:", steamKey, "to:", u.ID)
			err = u.Save(dbClient)
			if err != nil {
				return err
			}
			if currentSteamKey >= nbKeys {
				break
			}
		}
	}

	return nil
}

func oneItemPerPcubes(c *cli.Context) error {
	dbClient, err := dbclient.SharedUserDB()
	if err != nil {
		return err
	}

	fileList := make([]string, 0)
	err = filepath.Walk(pcubesDir, func(path string, f os.FileInfo, err error) error {
		fileList = append(fileList, path)
		return err
	})

	if err != nil {
		return err
	}

	for _, filePath := range fileList {

		arr := strings.Split(filePath, "/")

		l := len(arr)

		if l >= 4 { // "", "pcubesDir", <repo>, <name>

			repo := arr[l-2]
			name := strings.Split(arr[l-1], ".")[0]

			if repo == "" || name == "" {
				continue
			}

			items, err := item.GetByRepoAndName(dbClient, repo, name)

			if err != nil {
				return err
			}

			if len(items) == 0 {
				fmt.Println("CREATE: ", repo, "/", name)

				u, err := user.GetByUsername(dbClient, repo)
				if err != nil {
					return err
				}
				categoryValue := ""
				itemType := "voxels"                                                      // empty string means generic item
				newItem, err := item.New(repo, name, u.ID, itemType, categoryValue, true) // bypass reserved names
				if err != nil {
					return err
				}

				newItem.Public = true

				err = newItem.Save(dbClient)
				if err != nil {
					return err
				}
			}
		}
	}

	return nil
}

func userGames(c *cli.Context) error {
	if c.NArg() < 1 {
		return errors.New("missing username")
	}

	dbClient, err := dbclient.SharedUserDB()
	if err != nil {
		return err
	}

	username := c.Args().Get(0)
	u, err := user.GetByUsername(dbClient, username)
	if err != nil {
		return err
	}

	games, err := game.GetByAuthor(dbClient, u.ID)
	if err != nil {
		return err
	}

	if len(games) > 0 {
		fmt.Println("----------")
		for _, g := range games {
			fmt.Println("ID:", g.ID, "Title:", g.Title)
		}
		fmt.Println("----------")
	}

	return nil
}

// --------------------
// GAME
// --------------------

func addGame(c *cli.Context) error {

	g, err := game.New()
	if err != nil {
		return err
	}

	g.Author = readString("author id: ", "")

	dbClient, err := dbclient.SharedUserDB()
	if err != nil {
		return err
	}

	err = g.Save(dbClient, nil)
	if err != nil {
		return err
	}

	return nil
}

func readString(msg string, existing string) string {
	fmt.Printf(msg)
	if existing != "" {
		fmt.Printf("(default: " + existing + ") ")
	}
	reader := bufio.NewReader(os.Stdin)
	str, _ := reader.ReadString('\n')
	str = strings.TrimSpace(str)
	if str == "" && existing != "" {
		str = existing
	}
	return str
}

// func readInt(msg string) int {
// 	err := errors.New("")
// 	var i int

// 	for err != nil {
// 		fmt.Printf(msg)
// 		reader := bufio.NewReader(os.Stdin)
// 		str, _ := reader.ReadString('\n')
// 		str = strings.TrimSpace(str)
// 		i, err = strconv.Atoi(str)
// 		if err != nil {
// 			fmt.Println("nombre incorrect")
// 		}
// 	}
// 	return i
// }

func renamePcubesFile(itemRepo string, itemName string) error {
	pcubesCurrentPath := filepath.Join("/particubes_items", itemRepo, itemName+".pcubes")
	pcubesSourceEtag := filepath.Join("/particubes_items", itemRepo, itemName+".etag")
	pcubesNewPath := filepath.Join("/particubes_items", itemRepo, itemName+avatarSuffix+".pcubes")

	srcFileStat, err := os.Stat(pcubesCurrentPath)
	if err != nil {
		return err
	}

	// file exists
	if srcFileStat.Mode().IsRegular() {
		source, err := os.Open(pcubesCurrentPath)
		if err != nil {
			fmt.Println("failed to open copy pcubes file (1)")
			return err
		}

		destination, err := os.Create(pcubesNewPath)
		if err != nil {
			fmt.Println("failed to open copy pcubes file (2)")
			source.Close()
			return err
		}

		_, err = io.Copy(destination, source)
		if err != nil {
			fmt.Println("failed to open copy pcubes file (3)")
			source.Close()
			destination.Close()
			return err
		}
		source.Close()
		destination.Close()

		// delete source file
		err = os.Remove(pcubesCurrentPath)
		if err != nil {
			fmt.Println("failed to delete pcubes", pcubesCurrentPath)
			return err
		}

		// delete source etag
		_ = os.Remove(pcubesSourceEtag)

	} else {
		fmt.Println("pcubes file is not a regular file:", pcubesCurrentPath)
	}

	return nil
}

func processBodyPartsWithName(name string, dbClient *dbclient.Client) error {

	users, err := user.GetAll(dbClient)
	fmt.Println("GET ALL USERS:", len(users), err)

	// items, err := dbClient.GetItemsByName(name)
	// if err != nil {
	// 	fmt.Println("failed to get items:", err.Error())
	// 	return err
	// }

	// fmt.Println("items count", len(items))

	// for _, item := range items {
	// 	fmt.Println("-> found : ", item.Repo, item.Name)

	// 	// rename item pcubes file
	// 	err = renamePcubesFile(item.Repo, item.Name)
	// 	if err != nil {
	// 		fmt.Println("failed to rename item pcubes file", item.Repo, item.Name, err.Error())
	// 	}

	// 	// rename item
	// 	item.Name = item.Name + avatarSuffix
	// 	saveErr := dbClient.SaveItem(item)
	// 	if saveErr != nil {
	// 		fmt.Println("failed to rename item", item.Name, saveErr.Error())
	// 		continue
	// 	}

	// 	// add link to item in author's avatar
	// 	var usr *user.User = nil
	// 	usr, usrErr := dbClient.GetUserByID(item.Author)
	// 	if usrErr != nil {
	// 		fmt.Println("failed to get author user", err.Error())
	// 		continue
	// 	}

	// 	switch name {
	// 	case "head":

	// 	}
	// 	usr.AvatarHead = item.Repo + "." + item.Name
	// 	usrErr = dbClient.SaveUser(usr)
	// 	if usrErr != nil {
	// 		fmt.Println("failed to update author user:", err.Error())
	// 		continue
	// 	}
	// }

	return nil
}

func renameBodyParts(c *cli.Context) error {
	dbClient, err := dbclient.SharedUserDB()
	if err != nil {
		return err
	}

	// users, err := user.GetAll(dbClient)
	// fmt.Println("GET ALL USERS:", len(users), err)

	results, err := item.List(dbClient, "list:mine", "4d558bc1-5700-4a0d-8c68-f05e0b97f3fd")
	fmt.Println("GET MY ITEMS:", len(results), err)

	for i, v := range results {
		fmt.Println(i, v.Repo, v.Name)
	}

	// fmt.Println("Renaming items having body parts' names...")

	// dbClient, err := dbclient.SharedUserDB()
	// if err != nil {
	// 	fmt.Println("failed to get shared db client")
	// 	return err
	// }

	// // HEAD
	// processBodyPartsWithName("head", dbClient)
	// processBodyPartsWithName("body", dbClient)
	// processBodyPartsWithName("left_arm", dbClient)
	// processBodyPartsWithName("right_arm", dbClient)
	// processBodyPartsWithName("left_leg", dbClient)
	// processBodyPartsWithName("right_leg", dbClient)

	// return err
	return nil
}

//
// UTIL FUNCTIONS FOR SEARCH ENGINE
//

func ensureSearchEngineEnvars() error {

	if len(CUBZH_SEARCH_SERVER) == 0 {
		CUBZH_SEARCH_SERVER = os.Getenv("CUBZH_SEARCH_SERVER")
		if len(CUBZH_SEARCH_SERVER) == 0 {
			return errors.New("missing envar CUBZH_SEARCH_SERVER")
		}
	}

	if len(CUBZH_SEARCH_APIKEY) == 0 {
		CUBZH_SEARCH_APIKEY = os.Getenv("CUBZH_SEARCH_APIKEY")
		if len(CUBZH_SEARCH_APIKEY) == 0 {
			return errors.New("missing envar CUBZH_SEARCH_APIKEY")
		}
	}

	return nil
}

func getLocalhostScyllaClient(keyspace string) (*scyllaclient.Client, error) {
	config := scyllaclient.ClientConfig{
		Servers: []string{
			DB_ADDR,
		},
		Username:  "",
		Password:  "",
		AwsRegion: "",
		Keyspace:  keyspace,
	}
	return scyllaclient.NewShared(config)
}

// func getFriendRequest(c *cli.Context) error {

// 	scyllaClient, err := getLocalhostScyllaClient("universe")
// 	if err != nil {
// 		return err
// 	}

// 	userID1 := c.Args().Get(0)
// 	userID2 := c.Args().Get(1)

// 	scyllaClient.

// 		// var result scyllaclient.FriendRequestResult
// 		_, /*result*/ err = scyllaClient.InsertFriendRequest(userID1, username1, userID2, username2)
// 	return err
// }

func fixBalanceCache(c *cli.Context) error {
	userID := c.Args().Get(0)
	err := scyllaClientUniverse.RebuildUserBalanceCache(userID)
	return err
}

func fixBalanceCacheForAllUsers(c *cli.Context) error {
	dbClient, err := dbclient.SharedUserDB()
	if err != nil {
		return err
	}

	users, err := user.GetAll(dbClient)
	if err != nil {
		return err
	}

	for _, u := range users {
		err := scyllaClientUniverse.RebuildUserBalanceCache(u.ID)
		if err != nil {
			fmt.Println("❌", u.ID, ":", err.Error())
		} else {
			fmt.Println("✅", u.ID)
		}
	}

	return nil
}
