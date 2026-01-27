package main

import (
	"net/http"
	"strconv"

	"cu.bzh/hub/search"
	"cu.bzh/utils"
)

type CreationType string

const (
	CreationTypeItems  CreationType = "items"
	CreationTypeWorlds CreationType = "worlds"
)

type GetCreationsResponse struct {
	Creations    []*Creation `json:"creations"`
	TotalResults int         `json:"totalResults"`
	Page         int         `json:"page"`
	PerPage      int         `json:"perPage"`
}

// GET /creations
func getCreations(w http.ResponseWriter, r *http.Request) {

	// Parse query parameters
	QUERY_PARAM_TYPE := "type"
	QUERY_PARAM_AUTHOR_ID := "authorId"
	QUERY_PARAM_CATEGORY := "category"
	QUERY_PARAM_SEARCH := "search"
	QUERY_PARAM_SORT_BY := "sortBy"
	QUERY_PARAM_PER_PAGE := "perPage"
	QUERY_PARAM_PAGE := "page"
	// QUERY_PARAM_FEATURED := "featured"
	// QUERY_PARAM_MIN_BLOCK := "minBlock"
	QUERY_PARAM_FIELDS := "f"

	query := r.URL.Query()

	// String parameters
	qpType := query.Get(QUERY_PARAM_TYPE)
	qpAuthorID := query.Get(QUERY_PARAM_AUTHOR_ID)
	qpCategory := query.Get(QUERY_PARAM_CATEGORY)
	qpSearch := query.Get(QUERY_PARAM_SEARCH)
	qpSortBy := query.Get(QUERY_PARAM_SORT_BY)

	// Integer parameters with defaults
	perPage := 20 // default value
	if perPageStr := query.Get(QUERY_PARAM_PER_PAGE); perPageStr != "" {
		if parsed, err := strconv.Atoi(perPageStr); err == nil {
			perPage = parsed
		}
	}

	page := 1 // default value
	if pageStr := query.Get(QUERY_PARAM_PAGE); pageStr != "" {
		if parsed, err := strconv.Atoi(pageStr); err == nil {
			page = parsed
		}
	}

	// qpFeatured := false
	// {
	// 	featuredValueRaw := query.Get(QUERY_PARAM_FEATURED)
	// 	if featuredValueRaw == "true" || featuredValueRaw == "1" {
	// 		qpFeatured = true
	// 	}
	// }

	// Optional minBlock parameter
	// var qpMinBlock *int64
	// if minBlockStr := query.Get(QUERY_PARAM_MIN_BLOCK); minBlockStr != "" {
	// 	if parsed, err := strconv.ParseInt(minBlockStr, 10, 64); err == nil {
	// 		qpMinBlock = &parsed
	// 	}
	// }

	// Multiple field parameters (f)
	fieldsToReturn := query[QUERY_PARAM_FIELDS]

	// Get creation objects from database or search engine

	// search engine client
	searchClient, err := search.NewClient(CUBZH_SEARCH_SERVER, CUBZH_SEARCH_APIKEY)
	if err != nil {
		respondWithError(http.StatusInternalServerError, "", w)
		return
	}

	filter := search.CreationFilter{
		Banned:   utils.NewFalse(),
		Archived: utils.NewFalse(),
	}
	if qpType != "" {
		filter.Type = &qpType
	}
	if qpAuthorID != "" {
		filter.AuthorID = &qpAuthorID
	}
	// TODO: remove "null" once the client is fixed and stops sending it
	if qpCategory != "" && qpCategory != "null" {

		// currently the client asks for "featured" worlds using the category "featured"
		// TODO: gaetan: maybe we should add a "featured" query param instead
		// if qpType == string(CreationTypeWorlds) && qpCategory == "featured" {
		// 	filter.Featured = utils.NewTrue()
		// }

		// filter.Category = &qpCategory
	}
	// filter.MinBlock = qpMinBlock
	// if qpFeatured {
	// 	filter.Featured = utils.NewTrue()
	// }

	// if qpCategory is "featured", we use the hardcoded list of featured worlds
	if qpType == string(CreationTypeWorlds) && qpCategory == "featured" {

		response := GetCreationsResponse{
			Creations:    make([]*Creation, 0),
			TotalResults: 0,
			Page:         page,
			PerPage:      perPage,
		}

		found, searchCreations := getFeaturedWorldsForCategory(qpCategory, searchClient)
		if !found {
			// repond with empty response
			respondWithJSONV2(http.StatusOK, response, r, w)
			return
		}

		for _, searchCreation := range searchCreations {
			c := NewCreationFromSearchCreation(searchCreation)
			c.KeepFields(fieldsToReturn)
			response.Creations = append(response.Creations, &c)
			response.TotalResults += 1
		}

		respondWithJSONV2(http.StatusOK, response, r, w)
		return
	}

	// fmt.Printf("[SEARCH CREATIONS] 🔎 filter: %+v\n", filter)

	results, err := searchClient.SearchCollectionCreations(qpSearch, qpSortBy, page, perPage, filter)
	if err != nil {
		respondWithError(http.StatusInternalServerError, "failed to search creations", w)
		return
	}

	creations := []*Creation{}
	for _, searchCreation := range results.Results {
		c := NewCreationFromSearchCreation(searchCreation)
		c.KeepFields(fieldsToReturn)
		creations = append(creations, &c)
	}

	response := GetCreationsResponse{
		Creations:    creations,
		TotalResults: results.TotalResults,
		Page:         page,
		PerPage:      perPage,
	}

	respondWithJSONV2(http.StatusOK, response, r, w)
}
