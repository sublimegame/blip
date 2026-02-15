package main

import (
	"bytes"
	"encoding/json"
	"errors"
	"fmt"
	"image/jpeg"
	"image/png"
	"io"
	"net/http"
	"path/filepath"
	"regexp"
	"slices"
	"strconv"
	"time"

	"github.com/go-chi/chi/v5"

	"cu.bzh/hub/dbclient"
	"cu.bzh/hub/game"
	"cu.bzh/hub/search"
	"cu.bzh/hub/types"
	"cu.bzh/hub/user"
	schedulerTypes "cu.bzh/scheduler/types"
	"cu.bzh/types/gameserver"

	"cu.bzh/hub/scyllaclient"
)

//
// PRIVATE ROUTES
//

// TODO: add fields filtering (maybe do the same as "worldGet" function)
func privateGetWorld(w http.ResponseWriter, r *http.Request) {

	game, _, _, err, statusCode := getGameForRequest(r)
	if err != nil {
		respondWithError(statusCode, err.Error(), w)
		return
	}

	res := &getWorldRes{
		World: &World{
			ID:           game.ID,
			AuthorID:     game.Author,
			AuthorName:   game.AuthorName,
			Contributors: game.Contributors,
			Tags:         game.Tags,
			Title:        game.Title,
			Description:  game.Description,
			Featured:     game.Featured,
			Likes:        game.Likes,
			Liked:        game.Liked,
			Views:        game.Views,
			IsAuthor:     false,
			Script:       game.Script,
			MapBase64:    game.MapBase64,
		},
	}

	respondWithJSONV2(http.StatusOK, res, r, w)
}

//
// OPTIONALLY AUTHENTICATED ROUTES
//

func worldGet(w http.ResponseWriter, r *http.Request) {

	game, usr, fieldsToReturn, err, statusCode := getGameForRequest(r)
	if err != nil {
		respondWithError(statusCode, err.Error(), w)
		return
	}

	// struct to return
	worldv2 := newWorldV2FromGame(game, usr, fieldsToReturn)

	respondWithJSONV2(http.StatusOK, worldv2, r, w)
}

// code factorisation for worldGet and privateGetWorld
func getGameForRequest(r *http.Request) (*game.Game, *user.User, []string, error, int) {
	// Retrieve user doing the request (if any)
	// it's ok if usr is nil (using route without authentication)
	usr, _ := getUserFromRequestContext(r)

	// get client build number
	clientBuildNumber := 0
	{
		clientInfo, err := getClientInfo(r)
		if err == nil {
			clientBuildNumber = clientInfo.BuildNumber
		}
	}

	// shared Hub DB client
	dbClient, err := dbclient.SharedUserDB()
	if err != nil {
		return nil, nil, nil, errors.New(""), http.StatusInternalServerError
	}

	// Parse URL path
	worldID := chi.URLParam(r, "id")

	// validate `worldID` value
	if worldID == "" { // TODO: gaetan: ensure it looks like a UUID
		return nil, nil, nil, errors.New("world ID is missing"), http.StatusBadRequest
	}

	// Parse URL query params
	fieldsToReturn := []string{"id"} // always return id
	{
		queryParams := r.URL.Query()
		for key, values := range queryParams {
			if key == "field" || key == "f" { // TODO: remove "field" once 0.0.69 is forced
				for _, fieldName := range values {
					fieldsToReturn = append(fieldsToReturn, fieldName)
				}
			}
		}
	}

	// get game info from database
	var g *game.Game

	if usr != nil {
		g, err = game.GetByID(dbClient, worldID, usr.ID, &clientBuildNumber)
	} else {
		g, err = game.GetByID(dbClient, worldID, "", &clientBuildNumber)
	}
	if err != nil {
		return nil, nil, nil, errors.New(""), http.StatusInternalServerError
	}
	if g == nil {
		return nil, nil, nil, errors.New(""), http.StatusNotFound
	}

	// If world script has been requested, retrieve the correct
	// world script to use for the world and add it to the worldv2 struct
	if slices.Contains(fieldsToReturn, "script") {
		code, err := worldUtilsGetScript(g, &clientBuildNumber)
		if err != nil {
			return nil, nil, nil, errors.New(""), http.StatusInternalServerError
		}
		g.Script = code
	}

	return g, usr, fieldsToReturn, nil, 0
}

//
// AUTHENTICATED ROUTES
//

func patchWorldViews(w http.ResponseWriter, r *http.Request) {
	type worldAddViewsReq struct {
		Value int `json:"value"` // can only be a positive value
	}

	// retrieve user sending the request
	usr, err := getUserFromRequestContext(r)
	if err != nil {
		respondWithError(http.StatusInternalServerError, "", w)
		return
	}

	// get client build number
	clientInfo, err := getClientInfo(r)
	if err != nil {
		respondWithError(http.StatusBadRequest, "", w)
		return
	}

	// Parse URL

	worldID := chi.URLParam(r, "id")
	if !isValidUUID(worldID) {
		respondWithError(http.StatusBadRequest, "invalid world ID", w)
		return
	}

	// Parse request

	var req worldAddViewsReq
	err = json.NewDecoder(r.Body).Decode(&req)
	if err != nil {
		respondWithError(http.StatusBadRequest, "can't decode json request", w)
		return
	}
	if req.Value <= 0 {
		respondWithError(http.StatusBadRequest, "value must be positive", w)
		return
	}

	dbClient, err := dbclient.SharedUserDB()
	if err != nil {
		respondWithError(http.StatusInternalServerError, "", w)
		return
	}

	err = game.IncrementViews(dbClient, worldID, req.Value)
	if err != nil {
		respondWithError(http.StatusInternalServerError, "can't increment game views", w)
		return
	}

	// update world last launch for user
	{
		userID := usr.ID
		err = scyllaClientUniverse.UpsertWorldLastLaunch(scyllaclient.WorldLastLaunch{
			WorldID:   worldID,
			UserID:    userID,
			UpdatedAt: time.Now(),
		})
		if err != nil {
			respondWithError(http.StatusInternalServerError, "can't update world last launch", w)
			return
		}
		fmt.Println("✅ PATCH world views: world last launch updated for user", userID, "and world", worldID)
	}

	// Update search engine data with data from the database
	{
		w, err := game.GetByID(dbClient, worldID, usr.Username, &clientInfo.BuildNumber)
		if w != nil && err == nil { // success
			updatedInt := w.Updated.UnixMicro()
			wu := search.WorldUpdate{
				ID:        w.ID,
				UpdatedAt: &updatedInt,
				Likes:     &w.Likes,
			}
			err = searchEngineUpdateWorld(wu)
			if err != nil {
				fmt.Println("❌ PATCH world views: search engine update failed:", err.Error())
			}
		}
	}

	respondOK(w)
}

// Query parameters
// - search (string)
// - category (string)
// - authorid (string) NEW
// - authorname (string) NEW
//
// Supported categories
// - featured
// - fun_with_friends
// - new
// - solo
// - ...
func getWorlds(w http.ResponseWriter, r *http.Request) {

	// retrieve user sending the request
	usr, err := getUserFromRequestContext(r)
	if err != nil {
		respondWithError(http.StatusInternalServerError, "", w)
		return
	}

	// get client build number
	clientInfo, err := getClientInfo(r)
	if err != nil {
		respondWithError(http.StatusBadRequest, "", w)
		return
	}

	// search engine client
	searchClient, err := search.NewClient(CUBZH_SEARCH_SERVER, CUBZH_SEARCH_APIKEY)
	if err != nil {
		respondWithError(http.StatusInternalServerError, "", w)
		return
	}

	// Query parameters' names
	const queryParamSearch string = "search"
	const queryParamCategory string = "category"
	const queryParamAuthorID string = "authorid"
	const queryParamAuthorName string = "authorname"
	const queryParamSortBy string = "sortBy"

	var searchStr string = kSearchDefaultText
	var category string = kWorldSearchDefaultCategory
	var authorID string = ""
	var authorName string = ""
	var page int = kSearchDefaultPage
	var perPage int = kWorldSearchDefaultPerPage
	var sortBy string = kWorldSearchDefaultSortBy
	fieldsToReturn := []string{"id"}

	{
		if r.URL.Query().Has(queryParamSearch) {
			searchStr = r.URL.Query().Get(queryParamSearch)
		}
		if r.URL.Query().Has(queryParamCategory) {
			category = r.URL.Query().Get(queryParamCategory)
		}
		if r.URL.Query().Has(queryParamAuthorID) {
			authorID = r.URL.Query().Get(queryParamAuthorID)
		}
		if r.URL.Query().Has(queryParamAuthorName) {
			authorName = r.URL.Query().Get(queryParamAuthorName)
		}
		{
			pageStr := r.URL.Query().Get("page")
			if pageStr != "" {
				i, err := strconv.ParseInt(pageStr, 10, 32)
				if err != nil {
					respondWithError(http.StatusBadRequest, "query parameter 'page' is not valid", w)
					return
				}
				page = int(i)
			}
		}
		if r.URL.Query().Has("perPage") || r.URL.Query().Has("perpage") { // TODO: deprecate this once 0.0.63 is forced and only use "perPage"
			perPageStr := r.URL.Query().Get("perPage")
			if perPageStr == "" && r.URL.Query().Has("perpage") {
				perPageStr = r.URL.Query().Get("perpage") // TODO: deprecate this once 0.0.63 is forced and only use "perPage"
			}
			if perPageStr != "" {
				i, err := strconv.ParseInt(perPageStr, 10, 32)
				if err != nil {
					respondWithError(http.StatusBadRequest, "query parameter 'perPage' is not valid", w)
					return
				}
				perPage = int(i)
			}
		}
		{
			if r.URL.Query().Has(queryParamSortBy) {
				sortBy = r.URL.Query().Get(queryParamSortBy)
			}
		}
		{
			fieldsToReturn = append(fieldsToReturn, r.URL.Query()["f"]...)
		}
	}

	trueValue := true
	falseValue := false

	filter := search.WorldFilter{
		Banned: &falseValue,
	}
	if authorID != "" {
		filter.AuthorID = &authorID
	}
	if authorName != "" {
		filter.AuthorName = &authorName
	}
	if category == "featured" {
		filter.Featured = &trueValue
	}

	type worldsSearchRespLegacy struct {
		Results []game.Game `json:"results"`
		Count   int         `json:"count"`
		Page    int         `json:"page"`
		PerPage int         `json:"perPage"`
	}

	type worldsSearchResp struct {
		Results []WorldV2 `json:"results"`
		Count   int       `json:"count"`
		Page    int       `json:"page"`
		PerPage int       `json:"perPage"`
	}

	// shared Hub DB client
	dbClient, err := dbclient.SharedUserDB()
	if err != nil {
		respondWithError(http.StatusInternalServerError, "", w)
		return
	}

	// If a category is provided, we use the hardcoded list of worlds (no use of search engine)
	if category != "" {
		worldIDs, found := BLIP_FEATURED_WORLD_IDS_BY_CATEGORY[category]
		if found {
			resp := worldsSearchResp{}
			resp.Results = make([]WorldV2, 0)
			resp.Count = len(worldIDs)
			resp.Page = 1
			resp.PerPage = len(worldIDs)

			for _, worldID := range worldIDs {
				w, err := game.GetByID(dbClient, worldID, "", &clientInfo.BuildNumber)
				if err != nil {
					fmt.Println("❌ ERROR:", err.Error())
					continue
				}
				if w == nil {
					fmt.Println("❌ WORLD NOT FOUND:", worldID)
					continue
				}
				resp.Results = append(resp.Results, *newWorldV2FromGame(w, usr, fieldsToReturn))
			}

			respondWithJSONV2(http.StatusOK, resp, r, w)
			return
		}
	}

	searchResp, err := searchClient.SearchCollectionWorlds(searchStr, sortBy, page, perPage, filter)
	if err != nil {
		respondWithError(http.StatusInternalServerError, "search failed", w)
		return
	}

	resp := worldsSearchResp{}
	resp.Results = make([]WorldV2, len(searchResp.Results))
	resp.Count = searchResp.TotalResults
	resp.Page = searchResp.Page
	resp.PerPage = searchResp.PerPage
	for i, searchResult := range searchResp.Results {
		contentWarnings := []game.ContentWarning{}
		if searchResult.Banned {
			contentWarnings = append(contentWarnings, "political") // hardcoded
		}
		resp.Results[i] = *newWorldV2FromSearchResult(&searchResult, usr, fieldsToReturn)
	}

	{
		// client caching should be allowed for this URL
		// https://api.cu.bzh/worlds?category=featured
		allowClientCaching := category == "featured"
		if allowClientCaching {
			w.Header().Set("Cache-Control", "max-age="+CACHE_FEATUREDGAMES_MAX_AGE)
		}
	}

	respondWithJSONV2(http.StatusOK, resp, r, w)
}

// PATCH /worlds/{id}
func patchWorld(w http.ResponseWriter, r *http.Request) {

	// retrieve user sending the request
	usr, err := getUserFromRequestContext(r)
	if err != nil {
		respondWithError(http.StatusInternalServerError, "", w)
		return
	}

	// retrieve client info
	clientInfo, err := getClientInfo(r)
	if err != nil {
		respondWithError(http.StatusBadRequest, "", w)
		return
	}

	// shared Hub DB client
	dbClient, err := dbclient.SharedUserDB()
	if err != nil {
		respondWithError(http.StatusInternalServerError, "", w)
		return
	}

	type patchWorldReq struct {
		Title       *string `json:"title,omitempty"`
		Description *string `json:"description,omitempty"`
		MapBase64   *string `json:"mapBase64,omitempty"`
		MaxPlayers  *int32  `json:"maxPlayers,omitempty"`
		Archived    *bool   `json:"archived,omitempty"`
	}

	// Parse URL path
	worldID := chi.URLParam(r, "id")

	// TODO: validate worldID
	if worldID == "" {
		respondWithError(http.StatusBadRequest, "world ID is missing", w)
		return
	}

	// get item info from database
	world, err := game.GetByID(dbClient, worldID, usr.ID, &clientInfo.BuildNumber)
	if err != nil {
		respondWithError(http.StatusBadRequest, "world not found", w)
		return
	}

	// check if item is owned by the user
	if world.Author != usr.ID {
		respondWithError(http.StatusForbidden, "world not owned by user", w)
		return
	}

	// now, we can start applying changes

	var req patchWorldReq
	err = json.NewDecoder(r.Body).Decode(&req)
	if err != nil {
		respondWithError(http.StatusBadRequest, "can't decode json request", w)
		return
	}

	// update title if provided in request
	if req.Title != nil {
		newTitle := *req.Title

		regex, err := regexp.Compile(WORLD_TITLE_REGEX)
		if err != nil {
			respondWithError(http.StatusInternalServerError, "failed to compile regexp", w)
			return
		}
		if regex.MatchString(newTitle) == false {
			respondWithError(http.StatusBadRequest, "world title is invalid", w)
			return
		}
		world.Title = newTitle
	}

	// update description if provided in request
	if req.Description != nil {
		newDescription := *req.Description

		regex, err := regexp.Compile(DESCRIPTION_REGEX)
		if err != nil {
			respondWithError(http.StatusInternalServerError, "failed to compile regexp", w)
			return
		}
		if regex.MatchString(newDescription) == false {
			respondWithError(http.StatusBadRequest, "world description is invalid", w)
			return
		}
		world.Description = newDescription
	}

	// update mapBase64 if provided in request
	if req.MapBase64 != nil {
		newValue := *req.MapBase64
		// new value validation here ?
		world.MapBase64 = newValue
	}

	// NOTE: we might need to stop the dev server if running when setting maxPlayers
	// It's ok to keep it as is for now.
	if req.MaxPlayers != nil {
		world.MaxPlayers = *req.MaxPlayers
	}

	// update archived if provided in request
	if req.Archived != nil {
		newValue := *req.Archived
		// new value validation here ?
		world.Archived = newValue
	}

	err = world.Save(dbClient, &clientInfo.BuildNumber)
	if err != nil {
		respondWithError(http.StatusInternalServerError, "", w)
		return
	}

	// Update search engine data
	err = searchEngineUpsertWorld(world)
	if err != nil {
		fmt.Println("❌ PATCH /world/:id failed to update search engine data:", err.Error())
	}

	respondWithJSONV2(http.StatusOK, world, r, w)
}

func getWorldRunningServer(w http.ResponseWriter, r *http.Request) {

	worldID := chi.URLParam(r, "id")
	tag := chi.URLParam(r, "tag")
	regionParam := chi.URLParam(r, "region") // optional
	modeStr := chi.URLParam(r, "mode")       // optional

	region := types.NewRegion(regionParam)

	// get client build number
	clientInfo, err := getClientInfo(r)
	if err != nil {
		respondWithError(http.StatusBadRequest, "", w)
		return
	}

	var mode gameserver.Mode
	if modeStr == "" {
		mode = gameserver.ModePlay
	} else {
		mode = gameserver.Mode(modeStr)
		err := gameserver.ValidateMode(mode)
		if err != nil {
			mode = gameserver.ModePlay
		}
	}

	if worldID == "" {
		respondWithError(http.StatusBadRequest, "", w)
		return
	}

	dbClient, err := dbclient.SharedUserDB()
	if err != nil {
		fmt.Println("❌ getWorldRunningServer - internal server error (1)", err.Error())
		respondWithError(http.StatusInternalServerError, "", w)
		return
	}

	usr, err := getUserFromRequestContext(r)
	if err != nil {
		fmt.Println("❌ getWorldRunningServer - internal server error (2)", err.Error())
		respondWithError(http.StatusInternalServerError, "", w)
		return
	}

	// making sure servers in dev mode are only requested by authors or contributors.
	if mode == gameserver.ModeDev {
		// get game info from database
		g, err := game.GetByID(dbClient, worldID, "", &clientInfo.BuildNumber)
		if err != nil {
			respondWithError(http.StatusBadRequest, "", w)
			return
		}
		if g.Author != usr.ID {
			host, _ := getHostAndPort(r)
			fmt.Println("⚠️ ", usr.Username, "asking for server in dev mode while not being author! (world:", g.ID, "- request by:", usr.ID, "- host:", host, ")")
			respondWithError(http.StatusForbidden, "", w)
			return
		}
	}

	gameServersReq := &gameServersRequest{
		Mode:               mode,
		GameID:             worldID,
		GameServerTag:      tag,
		Region:             region,
		MaxResponseCount:   1,
		ResponseChan:       make(chan []*game.Server),
		IncludeFullServers: false,
		CreateIfNone:       true,
		ClientInfo: schedulerTypes.ClientInfo{
			ClientBuildNumber: clientInfo.BuildNumber,
		},
	}

	// Enqueue server requests to avoid creating several servers
	// when one is requested for the same game and region
	// by several users simultaneously.
	// NOTE: it will be improved later to support an important
	// amount of requests, but we're not there yet.

	// TODO: check for buffer overflow
	gameServersRequestChan <- gameServersReq

	servers := <-gameServersReq.ResponseChan

	if servers == nil {
		fmt.Println("❌ getWorldRunningServer - can't get game server (1)")
		respondWithError(http.StatusInternalServerError, "", w)
		return
	}

	if len(servers) == 0 {
		fmt.Println("❌ getWorldRunningServer - can't get game server (2)")
		respondWithError(http.StatusInternalServerError, "", w)
		return
	}

	server := servers[0]

	type worldRunningServerRes struct {
		Server *WorldServer `json:"server,omitempty"`
	}

	res := &worldRunningServerRes{
		Server: &WorldServer{
			ID:         server.ID,
			Address:    server.Addr,
			Players:    int32(server.Players),
			MaxPlayers: server.MaxPlayers,
		},
	}

	respondWithJSONV2(http.StatusOK, res, r, w)
}

func postWorldThumbnail(w http.ResponseWriter, r *http.Request) {

	dbClient, err := dbclient.SharedUserDB()
	if err != nil {
		respondWithError(http.StatusInternalServerError, "", w)
		return
	}

	// get client build number
	clientInfo, err := getClientInfo(r)
	if err != nil {
		respondWithError(http.StatusBadRequest, "", w)
		return
	}

	usr, err := getUserFromRequestContext(r)
	if err != nil {
		respondWithError(http.StatusInternalServerError, "", w)
		return
	}

	worldID := chi.URLParam(r, "id")

	// get game info from database
	g, err := game.GetByID(dbClient, worldID, "", &clientInfo.BuildNumber)
	if err != nil {
		respondWithError(http.StatusBadRequest, "", w)
		return
	}

	// make sure user is the author of the game
	if usr.ID != g.Author {
		respondWithError(http.StatusBadRequest, "", w)
		return
	}

	path := filepath.Join(WORLDS_STORAGE_PATH, worldID, "thumbnail.png")

	err = writeFile(r.Body, path, 0644)
	if err != nil {
		fmt.Println(err.Error())
		respondWithError(http.StatusInternalServerError, "", w)
		return
	}

	respondOK(w)
}

func postWorldThumbnailV2(w http.ResponseWriter, r *http.Request) {

	// get client build number
	clientInfo, err := getClientInfo(r)
	if err != nil {
		respondWithError(http.StatusBadRequest, "", w)
		return
	}

	// retrieve user sending the request
	usr, err := getUserFromRequestContext(r)
	if err != nil {
		respondWithError(http.StatusInternalServerError, "", w)
		return
	}

	worldID := chi.URLParam(r, "id")

	// get game info from database
	dbClient, err := dbclient.SharedUserDB()
	if err != nil {
		respondWithError(http.StatusInternalServerError, "", w)
		return
	}
	g, err := game.GetByID(dbClient, worldID, "", &clientInfo.BuildNumber)
	if err != nil {
		respondWithError(http.StatusBadRequest, "", w)
		return
	}

	// make sure user is the author of the game
	if usr.ID != g.Author {
		respondWithError(http.StatusBadRequest, "", w)
		return
	}

	// Read the first few bytes to detect the file type
	header := make([]byte, 8)
	_, err = r.Body.Read(header)
	if err != nil {
		fmt.Println("❌ postWorldThumbnail - error reading header:", err.Error())
		respondWithError(http.StatusBadRequest, "invalid image file", w)
		return
	}

	// Check if it's a PNG (magic bytes: 89 50 4E 47 0D 0A 1A 0A)
	isPNG := bytes.Equal(header[:8], []byte{0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A})

	// Check if it's a JPEG (magic bytes: FF D8)
	isJPEG := bytes.Equal(header[:2], []byte{0xFF, 0xD8})

	if !isPNG && !isJPEG {
		fmt.Println("❌ postWorldThumbnail - invalid file type:", header)
		respondWithError(http.StatusBadRequest, "file must be PNG or JPEG", w)
		return
	}

	// Refuse uploads that are too large
	const uploadMaxSize int64 = 20 * 1024 * 1024 // 20MB
	if r.ContentLength > uploadMaxSize {
		fmt.Println("❌ postWorldThumbnail - file too large (exceeds 20MB)")
		respondWithError(http.StatusBadRequest, "image file too large (max 20MB)", w)
		return
	}

	// read the rest of the body (max 20MB)
	bodyBytes, err := io.ReadAll(io.MultiReader(bytes.NewReader(header), io.LimitReader(r.Body, 20*1024*1024)))
	if err != nil {
		fmt.Println("❌ postWorldThumbnail - error reading body:", err.Error())
		respondWithError(http.StatusBadRequest, "image file too large", w)
		return
	}

	// Check if there are any remaining bytes (meaning the file is larger than 20MB)
	{
		remainingBytes, err := io.ReadAll(r.Body)
		if err != nil {
			fmt.Println("❌ postWorldThumbnail - error checking remaining bytes:", err.Error())
			respondWithError(http.StatusInternalServerError, "", w)
			return
		}
		if len(remainingBytes) > 0 {
			fmt.Println("❌ postWorldThumbnail - file too large (exceeds 20MB)")
			respondWithError(http.StatusBadRequest, "image file too large (max 20MB)", w)
			return
		}
	}

	// if it's a PNG, just check it is valid
	if isPNG {
		_, err := png.Decode(bytes.NewReader(bodyBytes))
		if err != nil {
			fmt.Println("❌ postWorldThumbnail - invalid PNG file:", err.Error())
			respondWithError(http.StatusBadRequest, "invalid PNG file", w)
			return
		}
	}

	// If it's a JPEG, convert it to PNG
	if isJPEG {
		// Decode the JPEG
		img, err := jpeg.Decode(bytes.NewReader(bodyBytes))
		if err != nil {
			fmt.Println("❌ postWorldThumbnail - error decoding JPEG file:", err.Error())
			respondWithError(http.StatusBadRequest, "invalid JPEG file", w)
			return
		}

		// Create a buffer to store the PNG
		var buf bytes.Buffer

		// Encode as PNG
		err = png.Encode(&buf, img)
		if err != nil {
			fmt.Println("❌ postWorldThumbnail - error converting JPEG to PNG:", err.Error())
			respondWithError(http.StatusInternalServerError, "failed to convert image", w)
			return
		}

		// Replace the request body with our PNG buffer
		bodyBytes = buf.Bytes()
	}

	// write the PNG thumbnail to the filesystem
	imageDataReader := bytes.NewReader(bodyBytes)
	err = writeFile(imageDataReader, filepath.Join(WORLDS_STORAGE_PATH, worldID, "thumbnail.png"), 0644)
	if err != nil {
		fmt.Println("❌ postWorldThumbnail - error writing thumbnail to filesystem:", err.Error())
		respondWithError(http.StatusInternalServerError, "", w)
		return
	}

	respondOK(w)
}
