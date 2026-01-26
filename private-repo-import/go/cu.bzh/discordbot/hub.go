package main

import (
	"errors"
	"fmt"

	"cu.bzh/hub/dbclient"
	"cu.bzh/hub/game"
	"cu.bzh/hub/item"
	"cu.bzh/hub/search"
	"cu.bzh/hub/user"
)

func indexItem(itm *item.Item) error {
	searchClient, err := search.NewClient(SEARCH_ENGINE_ADDR, SEARCH_ENGINE_APIKEY)
	if err != nil {
		return err
	}
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
	fmt.Println("indexing item ...")
	search.PrintItemDraft(itemDraft)
	err = searchClient.UpdateItemDraft(itemDraft)
	fmt.Println("err:", err)
	return err
}

// banItem bans an item
// param `itemRepoAndName` is of the form `repo.name`
func banItem(itemRepoAndName string) error {
	// parse repo and name
	repo, name, err := item.ParseRepoDotName(itemRepoAndName)
	if err != nil {
		return err
	}

	dbClient, err := dbclient.SharedUserDB()
	if err != nil {
		return err
	}

	items, err := item.GetByRepoAndName(dbClient, repo, name)
	if err != nil {
		return err
	}
	if len(items) < 1 {
		return errors.New("item not found")
	}
	if len(items) > 1 {
		return errors.New("more than 1 item found for repo and name")
	}

	itm := items[0]
	itm.Banned = true
	err = itm.SaveInternal(dbClient)
	if err != nil {
		return err
	}

	// re-index item data in search engine
	err = indexItem(itm)

	return err
}

// unbanItem removes a ban on an item
// param `itemRepoAndName` is of the form `repo.name`
func unbanItem(itemRepoAndName string) error {
	// parse repo and name
	repo, name, err := item.ParseRepoDotName(itemRepoAndName)
	if err != nil {
		return err
	}

	dbClient, err := dbclient.SharedUserDB()
	if err != nil {
		return err
	}

	items, err := item.GetByRepoAndName(dbClient, repo, name)
	if err != nil {
		return err
	}
	if len(items) < 1 {
		return errors.New("item not found")
	}
	if len(items) > 1 {
		return errors.New("more than 1 item found for repo and name")
	}

	itm := items[0]
	itm.Banned = false
	err = itm.SaveInternal(dbClient)
	if err != nil {
		return err
	}

	// re-index item data in search engine
	err = indexItem(itm)

	return err
}

// itemIsBanned returns whether an item is banned.
// param `itemRepoAndName` is of the form `repo.name`
func itemIsBanned(itemRepoAndName string) (bool, error) {
	// parse repo and name
	repo, name, err := item.ParseRepoDotName(itemRepoAndName)
	if err != nil {
		return false, err
	}

	dbClient, err := dbclient.SharedUserDB()
	if err != nil {
		return false, err
	}

	items, err := item.GetByRepoAndName(dbClient, repo, name)
	if err != nil {
		return false, err
	}
	if len(items) < 1 {
		return false, errors.New("item not found")
	}
	if len(items) > 1 {
		return false, errors.New("more than 1 item found for repo and name")
	}

	itm := items[0]
	return itm.Banned, nil
}

func gameAddContentWarning(gameID, contentWarning string) error {
	if gameID == "" || contentWarning == "" {
		return errors.New("bad arguments")
	}

	dbClient, err := dbclient.SharedUserDB()
	if err != nil {
		return err
	}

	gameObj, err := game.GetByID(dbClient, gameID, "", nil)
	if err != nil {
		return err
	}

	err = gameObj.AddContentWarning(game.ContentWarning(contentWarning))
	if err != nil {
		return err
	}

	err = gameObj.Save(dbClient, nil)
	return err
}

func gameGetContentWarnings(gameID string) ([]game.ContentWarning, error) {
	res := make([]game.ContentWarning, 0)

	if gameID == "" {
		return res, errors.New("bad arguments")
	}

	dbClient, err := dbclient.SharedUserDB()
	if err != nil {
		return res, err
	}

	gameObj, err := game.GetByID(dbClient, gameID, "", nil)

	return gameObj.ContentWarnings, err
}

func getUsersWithBio() ([]*user.User, error) {
	dbClient, err := dbclient.SharedUserDB()
	if err != nil {
		return nil, err
	}

	var results []*user.User
	filter := make(map[string]interface{})
	findOptions := &dbclient.FindOptions{}

	// the `i` option means it is case-sensitive
	filter["bio"] = dbclient.Regex{Pattern: `^(?!\s*$).+`, Options: "i"}
	err = dbClient.GetRecordsMatchingFilter(dbclient.UserCollectionName, filter, findOptions, &results)

	return results, err
}

func featureWorld(featureYesNo bool, worldID string) error {
	dbClient, err := dbclient.SharedUserDB()
	if err != nil {
		return err
	}
	if dbClient == nil {
		return errors.New("dbClient is nil")
	}

	// update world in database

	worldObj, err := game.GetByID(dbClient, worldID, "", nil)
	if err != nil {
		return err
	}

	var update game.GameUpdate
	update.Featured = &featureYesNo

	err = worldObj.UpdateAndSave(dbClient, update)
	if err != nil {
		return err
	}

	// update world in search engine

	sew := search.NewWorldFromValues(
		worldObj.ID,
		worldObj.Title,
		worldObj.Author,
		worldObj.AuthorName,
		worldObj.Created,
		worldObj.Updated,
		worldObj.Likes,
		worldObj.Views,
		worldObj.Featured,
		len(worldObj.ContentWarnings) > 0,
	)

	// get search engine client
	searchClient, err := search.NewClient(SEARCH_ENGINE_ADDR, SEARCH_ENGINE_APIKEY)
	if err != nil {
		return err
	}

	// insert or update ItemDraft in search engine
	err = searchClient.UpsertWorld(sew)
	return err
}
