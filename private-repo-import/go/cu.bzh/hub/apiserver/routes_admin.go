package main

import (
	"fmt"
	"io"
	"net/http"

	"github.com/go-chi/chi/v5"
	"github.com/google/uuid"

	"cu.bzh/hub/dbclient"
	"cu.bzh/hub/game"
	"cu.bzh/hub/item"
	"cu.bzh/hub/search"
	"cu.bzh/hub/user"
)

func banUserAccount(w http.ResponseWriter, r *http.Request) {
	fmt.Println("[🔥][BAN USER] starting...")

	// read the request body
	_, err := io.ReadAll(r.Body)
	if err != nil {
		fmt.Println("[❌][BAN USER] failed to read request body")
		respondWithError(http.StatusInternalServerError, "", w)
		return
	}

	// get userID from path
	userID := chi.URLParam(r, "userID")

	fmt.Println("[🔥][BAN USER] userID:", userID)

	// validate userID
	if userID == "" {
		fmt.Println("[❌][BAN USER] userID is empty")
		respondWithError(http.StatusBadRequest, "", w)
		return
	}
	if _, err := uuid.Parse(userID); err != nil {
		fmt.Println("[❌][BAN USER] userID is not a valid UUID")
		respondWithError(http.StatusBadRequest, "", w)
		return
	}

	dbClient, err := dbclient.SharedUserDB()
	if err != nil {
		fmt.Println("[❌][BAN USER] failed to create dbClient")
		respondWithError(http.StatusInternalServerError, "", w)
		return
	}

	// search engine client
	searchClient, err := search.NewClient(CUBZH_SEARCH_SERVER, CUBZH_SEARCH_APIKEY)
	if err != nil {
		fmt.Println("[❌][BAN USER] failed to create search engine client:", err.Error())
		respondWithError(http.StatusInternalServerError, "", w)
		return
	}

	// make sure user exists
	userToBan, err := user.GetByID(dbClient, userID)
	if err != nil {
		respondWithError(http.StatusInternalServerError, "", w)
		return
	}
	if userToBan == nil {
		respondWithError(http.StatusNotFound, "", w)
		return
	}

	fmt.Println("[🔥][BAN USER] user found. Ready to ban:", userToBan.Username)

	// Remove item likes
	count, err := item.RemoveAllLikes(dbClient, userID)
	if err != nil {
		fmt.Println("[❌][BAN USER] failed to remove item likes:", err.Error())
		respondWithError(http.StatusInternalServerError, "", w)
		return
	}
	fmt.Println("[🔥][BAN USER] removed item likes:", count)

	// Remove world likes
	count, err = game.RemoveAllLikes(dbClient, userID)
	if err != nil {
		fmt.Println("[❌][BAN USER] failed to remove world likes:", err.Error())
		respondWithError(http.StatusInternalServerError, "", w)
		return
	}
	fmt.Println("[🔥][BAN USER] removed world likes:", count)

	// Remove all notifications
	count, err = scyllaClientUniverse.RemoveAllNotificationsForUser(userID)
	if err != nil {
		fmt.Println("[❌][BAN USER] failed to remove notifications:", err.Error())
		respondWithError(http.StatusInternalServerError, "", w)
		return
	}
	fmt.Println("[🔥][BAN USER] removed notifications:", count)

	// Remove badge unlocks
	count, err = scyllaClientUniverse.RemoveAllUserBadgesForUser(userID)
	if err != nil {
		fmt.Println("[❌][BAN USER] failed to remove badge unlocks:", err.Error())
		respondWithError(http.StatusInternalServerError, "", w)
		return
	}
	fmt.Println("[🔥][BAN USER] removed badge unlocks:", count)

	// Remove world last launches
	count, err = scyllaClientUniverse.RemoveAllWorldLastLaunchesForUser(userID)
	if err != nil {
		fmt.Println("[❌][BAN USER] failed to remove world last launches:", err.Error())
		respondWithError(http.StatusInternalServerError, "", w)
		return
	}
	fmt.Println("[🔥][BAN USER] removed world last launches:", count)

	// Remove all items
	count, err = item.DeleteAllItemsForUser(dbClient, userID, item.ItemDeleteOpts{
		TypesenseClient: searchClient,
		ItemStoragePath: ITEMS_STORAGE_PATH,
	})
	if err != nil {
		fmt.Println("[❌][BAN USER] failed to remove items:", err.Error())
		respondWithError(http.StatusInternalServerError, "", w)
		return
	}
	fmt.Println("[🔥][BAN USER] removed items:", count)

	// Remove all friend requests & relations
	count, err = scyllaClientUniverse.DeleteAllFriendRequestsForUser(userID)
	if err != nil {
		fmt.Println("[❌][BAN USER] failed to remove friend requests:", err.Error())
		respondWithError(http.StatusInternalServerError, "", w)
		return
	}
	fmt.Println("[🔥][BAN USER] removed friend requests:", count)

	count, err = scyllaClientUniverse.DeleteAllFriendRelationsForUser(userID)
	if err != nil {
		fmt.Println("[❌][BAN USER] failed to remove friend relations:", err.Error())
		respondWithError(http.StatusInternalServerError, "", w)
		return
	}
	fmt.Println("[🔥][BAN USER] removed friend relations:", count)

	// Remove all badges (of the worlds created by the user)
	{
		// get all worlds created by the user
		worlds, err := game.GetByAuthor(dbClient, userID)
		if err != nil {
			fmt.Println("[❌][BAN USER] failed to get worlds:", err.Error())
			respondWithError(http.StatusInternalServerError, "", w)
			return
		}

		// construct the list of world IDs
		worldIDs := []string{}
		for _, world := range worlds {
			worldIDs = append(worldIDs, world.ID)
		}

		// delete all badges of the worlds
		count, err = scyllaClientUniverse.DeleteAllBadgesOfWorlds(worldIDs)
		if err != nil {
			fmt.Println("[❌][BAN USER] failed to remove badges:", err.Error())
			respondWithError(http.StatusInternalServerError, "", w)
			return
		}
		fmt.Println("[🔥][BAN USER] removed badges:", count)
	}

	// Remove all worlds for the user
	{
		count, err = game.DeleteAllWorldsForUser(dbClient, userID, game.WorldDeleteOpts{
			TypesenseClient:  searchClient,
			WorldStoragePath: WORLDS_STORAGE_PATH,
		})
		if err != nil {
			fmt.Println("[❌][BAN USER] failed to remove worlds:", err.Error())
			respondWithError(http.StatusInternalServerError, "", w)
			return
		}
		fmt.Println("[🔥][BAN USER] removed worlds:", count)
	}

	// Flag user account as banned (with the reason for the ban)
	{
		err = userToBan.Ban(dbClient, user.DeleteReasonAdminDeletedAccount)
		if err != nil {
			fmt.Println("[❌][BAN USER] failed to flag user as banned:", err.Error())
			respondWithError(http.StatusInternalServerError, "", w)
			return
		}
		fmt.Println("[🔥][BAN USER] flagged user as banned")
	}

	respondOK(w)
}
