package main

import (
	"bytes"
	"crypto/md5"
	"encoding/base64"
	"encoding/hex"
	"encoding/json"
	"errors"
	"fmt"
	"io"
	"net/http"
	"os"
	"path/filepath"
	"strconv"
	"time"

	"github.com/go-chi/chi/v5"
	"github.com/google/uuid"

	"cu.bzh/hub/dbclient"
	"cu.bzh/hub/game"
	"cu.bzh/hub/scyllaclient"
	"cu.bzh/hub/user"
)

// GET /badges/{badgeID}
func getBadgeByID(w http.ResponseWriter, r *http.Request) {

	// Parse URL path
	badgeID := chi.URLParam(r, "badgeID")

	// get single badge by ID
	badge, err := scyllaClientUniverse.GetBadgeByID(badgeID)
	if err != nil {
		respondWithError(http.StatusInternalServerError, err.Error(), w)
		return
	}

	respondWithJSON(http.StatusOK, badge, w)

}

// GET /badges/{badgeID}/thumbnail.png
func getBadgeThumbnail(w http.ResponseWriter, r *http.Request) {

	const (
		URL_PARAM_BADGE_ID  = "badgeID"
		URL_PARAM_WIDTH     = "width"
		THUMBNAIL_MAX_WIDTH = 800 // max value for width query param
	)

	var err error
	var badgeID string
	var width = 256

	// retrieve world ID value from URL
	badgeID = chi.URLParam(r, URL_PARAM_BADGE_ID)

	// retrieve width value from URL
	widthStr := chi.URLParam(r, URL_PARAM_WIDTH)
	if widthStr != "" {
		width, err = strconv.Atoi(widthStr)
		if err != nil || width < 0 || width > THUMBNAIL_MAX_WIDTH {
			respondWithError(http.StatusBadRequest, "invalid width", w)
			return
		}
	} else { // check query params
		widthStr = r.URL.Query().Get(URL_PARAM_WIDTH)
		if widthStr != "" {
			width, err = strconv.Atoi(widthStr)
			if err != nil || width < 0 || width > THUMBNAIL_MAX_WIDTH {
				respondWithError(http.StatusBadRequest, "invalid width", w)
				return
			}
		}
	}

	// get thumbnail bytes
	pngBytes, err := getBadgeThumbnailBytes(badgeID, &width)
	if err != nil {
		respondWithError(http.StatusNotFound, err.Error(), w)
		return
	}

	w.Header().Set("Content-Type", "image/png")
	w.Header().Set("Content-Length", strconv.Itoa(len(pngBytes)))
	w.Header().Set("Cache-Control", "max-age="+CACHE_THUMBNAILS_MAX_AGE)
	w.WriteHeader(http.StatusOK)

	_, err = w.Write(pngBytes)
	if err != nil {
		fmt.Println("❌ error while writing badge thumbnail to response:", err.Error())
	}
}

// GET /world/{worldID}/badges
func getWorldBadges(w http.ResponseWriter, r *http.Request) {

	// Parse URL path
	worldID := chi.URLParam(r, "worldID")

	// get all badges of the world
	badges, err := scyllaClientUniverse.GetBadgesByWorldID(worldID)
	if err != nil {
		respondWithError(http.StatusInternalServerError, err.Error(), w)
		return
	}

	// compute rarity
	for i, badge := range badges {
		// count of players who have played the world
		totalPlayerCount, err := scyllaClientUniverse.CountWorldLastLaunchByWorldID(worldID)
		if err != nil {
			respondWithError(http.StatusInternalServerError, err.Error(), w)
			return
		}
		// count of players who have unlocked the badge
		playerUnlockCount, err := scyllaClientUniverse.CountUserBadgesByID(badge.BadgeID)
		if err != nil {
			respondWithError(http.StatusInternalServerError, err.Error(), w)
			return
		}
		// badges[i].Rarity = float32(count) / 100.0
		if totalPlayerCount == 0 {
			badges[i].Rarity = 0
		} else {
			badges[i].Rarity = float32(playerUnlockCount) / float32(totalPlayerCount)
		}
	}

	// get lock/unlock status for each badge (and the current user)
	usr, err := getUserFromRequestContext(r)
	if err != nil {
		respondWithError(http.StatusInternalServerError, err.Error(), w)
		return
	}

	badgesResponse := make([]*Badge, len(badges))

	for i, badge := range badges {
		userBadge, err := scyllaClientUniverse.GetUserBadge(usr.ID, badge.BadgeID)
		if err != nil {
			respondWithError(http.StatusInternalServerError, err.Error(), w)
			return
		}
		badgesResponse[i] = NewBadge(badge, userBadge)
	}

	respondWithJSON(http.StatusOK, badgesResponse, w)
}

// POST /badges
func postBadge(w http.ResponseWriter, r *http.Request) {

	type postBadgeReq struct {
		WorldID     string `json:"worldID"`               // mandatory
		ImageBase64 string `json:"imageBase64"`           // mandatory
		Tag         string `json:"tag"`                   // mandatory
		Name        string `json:"name,omitempty"`        // optional
		Description string `json:"description,omitempty"` // optional
	}

	// Parse request
	var req postBadgeReq
	if err := json.NewDecoder(r.Body).Decode(&req); err != nil {
		respondWithError(http.StatusBadRequest, "", w)
		return
	}

	// retrieve user sending the request
	senderUsr, err := getUserFromRequestContext(r)
	if err != nil {
		respondWithError(http.StatusInternalServerError, "", w)
		return
	}

	// make sure the user is the author of the world
	dbClient, err := dbclient.SharedUserDB()
	if err != nil {
		respondWithError(http.StatusInternalServerError, "", w)
		return
	}
	worldObj, err := game.GetByID(dbClient, req.WorldID, senderUsr.ID, nil)
	if err != nil {
		respondWithError(http.StatusInternalServerError, "", w)
		return
	}
	if worldObj == nil {
		respondWithError(http.StatusNotFound, "world not found", w)
		return
	}
	if worldObj.Author != senderUsr.ID {
		respondWithError(http.StatusForbidden, "user is not the author of the world", w)
		return
	}

	var pngBytes []byte

	// validate request

	// WorldID
	if _, err := uuid.Parse(req.WorldID); err != nil {
		respondWithError(http.StatusBadRequest, "worldID is not a valid UUID", w)
		return
	}

	// ImageBase64
	{
		if req.ImageBase64 == "" {
			respondWithError(http.StatusBadRequest, "imageBase64 is required", w)
			return
		}
		// decode image bytes from base64
		imageBytes, err := base64.StdEncoding.DecodeString(req.ImageBase64)
		if err != nil {
			respondWithError(http.StatusBadRequest, "invalid imageBase64", w)
			return
		}
		// convert image to PNG (supports PNG, JPEG, GIF, etc.)
		pngBytes, err = convertImageToPNG(imageBytes)
		if err != nil {
			respondWithError(http.StatusInternalServerError, "failed to convert image to PNG", w)
			return
		}
		if pngBytes == nil {
			fmt.Println("❌ postBadge - failed to convert image to PNG: nil bytes")
		}
		if len(pngBytes) == 0 {
			fmt.Println("❌ postBadge - failed to convert image to PNG: empty bytes")
		}
	}

	// Tag
	if req.Tag == "" {
		fmt.Println("❌ postBadge - tag is required")
		respondWithError(http.StatusBadRequest, "tag is required", w)
		return
	}

	// create badge
	now := time.Now()
	badge := scyllaclient.Badge{
		BadgeID:     uuid.New().String(),
		WorldID:     req.WorldID,
		Tag:         req.Tag,
		Name:        req.Name,
		Description: req.Description,
		CreatedAt:   now,
		UpdatedAt:   now,
	}
	badge, recordInserted, err := scyllaClientUniverse.InsertBadge(badge)
	if err != nil {
		fmt.Println("❌ postBadge - error inserting badge:", err.Error())
		respondWithError(http.StatusInternalServerError, err.Error(), w)
		return
	}
	if !recordInserted {
		fmt.Println("❌ postBadge - badge already exists")
		respondWithError(http.StatusConflict, "already exist", w)
		return
	}

	// write the PNG thumbnail to the filesystem
	newFilePath := filepath.Join(BADGES_STORAGE_PATH, badge.BadgeID, "thumbnail.png")
	err = writeFile(bytes.NewReader(pngBytes), newFilePath, 0644)
	if err != nil {
		fmt.Println("❌ postBadge - error writing thumbnail to filesystem:", err.Error())
		respondWithError(http.StatusInternalServerError, "failed to write thumbnail to filesystem", w)
		return
	}

	respondWithJSON(http.StatusOK, badge, w)
}

// PATCH /badges/{badgeID}
func patchBadge(w http.ResponseWriter, r *http.Request) {

	type patchBadgeReq struct {
		ImageBase64 string `json:"imageBase64,omitempty"` // optional
		Name        string `json:"name,omitempty"`        // optional
		Description string `json:"description,omitempty"` // optional
	}

	// Parse URL path
	badgeID := chi.URLParam(r, "badgeID")
	if _, err := uuid.Parse(badgeID); err != nil {
		respondWithError(http.StatusBadRequest, "", w)
		return
	}

	// Parse request
	var req patchBadgeReq
	if err := json.NewDecoder(r.Body).Decode(&req); err != nil {
		respondWithError(http.StatusBadRequest, "", w)
		return
	}

	// update badge
	badge, err := scyllaClientUniverse.GetBadgeByID(badgeID)
	if err != nil { // failed to retrieve badge
		respondWithError(http.StatusInternalServerError, "", w)
		return
	}
	if badge == nil { // badge not found
		respondWithError(http.StatusInternalServerError, "", w)
		return
	}

	// make sure the user is the author of the badge
	{
		usr, err := getUserFromRequestContext(r)
		if err != nil {
			respondWithError(http.StatusInternalServerError, err.Error(), w)
			return
		}
		dbClient, err := dbclient.SharedUserDB()
		if err != nil {
			respondWithError(http.StatusInternalServerError, "", w)
			return
		}
		worldObj, err := game.GetByID(dbClient, badge.WorldID, usr.ID, nil)
		if err != nil {
			respondWithError(http.StatusInternalServerError, err.Error(), w)
			return
		}
		if worldObj == nil {
			respondWithError(http.StatusInternalServerError, "", w)
			return
		}
		if worldObj.Author != usr.ID {
			fmt.Println("❌ patchBadge - user is not the author of the badge")
			respondWithError(http.StatusForbidden, "user is not the author of the badge", w)
			return
		}
	}

	if req.Name != "" || req.Description != "" {
		badgeUpdate := scyllaclient.BadgeUpdate{
			Name:        nil,
			Description: nil,
			UpdatedAt:   time.Now(),
		}
		if req.Name != "" {
			badgeUpdate.Name = &req.Name
		}
		if req.Description != "" {
			badgeUpdate.Description = &req.Description
		}

		err = scyllaClientUniverse.UpdateBadge(badgeID, badgeUpdate)
		if err != nil {
			respondWithError(http.StatusInternalServerError, "", w)
			return
		}
	}

	if req.ImageBase64 != "" {
		// decode image bytes from base64
		imageBytes, err := base64.StdEncoding.DecodeString(req.ImageBase64)
		if err != nil {
			respondWithError(http.StatusBadRequest, "invalid imageBase64", w)
			return
		}
		// convert image to PNG (supports PNG, JPEG, GIF, etc.)
		var pngBytes []byte
		pngBytes, err = convertImageToPNG(imageBytes)
		if err != nil {
			respondWithError(http.StatusInternalServerError, "failed to convert image to PNG", w)
			return
		}
		if pngBytes == nil {
			fmt.Println("❌ postBadge - failed to convert image to PNG: nil bytes")
		}
		if len(pngBytes) == 0 {
			fmt.Println("❌ postBadge - failed to convert image to PNG: empty bytes")
		}

		// write the PNG thumbnail to the filesystem
		thumbnailFilePath := filepath.Join(BADGES_STORAGE_PATH, badge.BadgeID, "thumbnail.png")
		err = writeFile(bytes.NewReader(pngBytes), thumbnailFilePath, 0644)
		if err != nil {
			respondWithError(http.StatusInternalServerError, "failed to write thumbnail to filesystem", w)
			return
		}
	}

	respondOK(w)
}

func checkBadgeSignature(worldID, badgeTag, signature string) bool {

	// recompute signature
	md5Hash := md5.New()
	md5Hash.Write([]byte("item:" + worldID + ":" + badgeTag + "2019"))
	recomputedSignature := hex.EncodeToString(md5Hash.Sum(nil))

	return recomputedSignature == signature
}

// GET /users/self/badges
// GET /users/{userID}/badges
func getUserBadges(w http.ResponseWriter, r *http.Request) {

	// get path param
	userID := chi.URLParam(r, "userID")

	var usr *user.User
	var err error

	// get user from path param
	if userID != "" {
		dbClient, err := dbclient.SharedUserDB()
		if err != nil {
			respondWithError(http.StatusInternalServerError, err.Error(), w)
			return
		}
		usr, err = user.GetByID(dbClient, userID)
		if err != nil {
			respondWithError(http.StatusInternalServerError, err.Error(), w)
			return
		}
	} else {
		// get user from request context (sender)
		usr, err = getUserFromRequestContext(r)
		if err != nil {
			respondWithError(http.StatusInternalServerError, err.Error(), w)
			return
		}
	}

	// get badges unlocked by the user
	userBadges, err := scyllaClientUniverse.GetUserBadgesByUserID(usr.ID)
	if err != nil {
		respondWithError(http.StatusInternalServerError, err.Error(), w)
		return
	}

	badges := make([]*Badge, len(userBadges))
	for i, userBadge := range userBadges {
		badge, err := scyllaClientUniverse.GetBadgeByID(userBadge.BadgeID)
		if err != nil {
			respondWithError(http.StatusInternalServerError, err.Error(), w)
			return
		}
		badges[i] = NewBadge(*badge, &userBadge)
	}

	// world ID should be provided for each badge
	// compute rarity
	for i, badge := range badges {
		// count of players who have played the world
		totalPlayerCount, err := scyllaClientUniverse.CountWorldLastLaunchByWorldID(badge.WorldID)
		if err != nil {
			respondWithError(http.StatusInternalServerError, err.Error(), w)
			return
		}
		// count of players who have unlocked the badge
		playerUnlockCount, err := scyllaClientUniverse.CountUserBadgesByID(badge.BadgeID)
		if err != nil {
			respondWithError(http.StatusInternalServerError, err.Error(), w)
			return
		}
		// badges[i].Rarity = float32(count) / 100.0
		if totalPlayerCount == 0 {
			badges[i].Rarity = 0
		} else {
			badges[i].Rarity = float32(playerUnlockCount) / float32(totalPlayerCount)
		}
	}

	respondWithJSON(http.StatusOK, badges, w)
}

// POST /users/self/badges
func unlockBadge(w http.ResponseWriter, r *http.Request) {

	type unlockBadgeReq struct {
		WorldID   string `json:"worldId"`   // mandatory
		BadgeTag  string `json:"badgeTag"`  // mandatory
		Signature string `json:"signature"` // mandatory
	}

	// Parse request
	var req unlockBadgeReq
	if err := json.NewDecoder(r.Body).Decode(&req); err != nil {
		respondWithError(http.StatusBadRequest, "", w)
		return
	}

	// validate worldID
	if _, err := uuid.Parse(req.WorldID); err != nil {
		respondWithError(http.StatusBadRequest, "worldID is invalid", w)
		return
	}
	// check if badge exists
	badge, err := scyllaClientUniverse.GetBadgeByWorldIDAndTag(req.WorldID, req.BadgeTag)
	if err != nil || badge == nil {
		respondWithError(http.StatusInternalServerError, "failed to retrieve badge", w)
		return
	}

	// get user from request context
	usr, err := getUserFromRequestContext(r)
	if err != nil {
		respondWithError(http.StatusInternalServerError, err.Error(), w)
		return
	}

	// check signature
	if !checkBadgeSignature(req.WorldID, req.BadgeTag, req.Signature) {
		respondWithError(http.StatusInternalServerError, "", w)
		return
	}

	type unlockBadgeResp struct {
		Unlocked  bool   `json:"unlocked"`
		BadgeName string `json:"badgeName,omitempty"`
		BadgeID   string `json:"badgeId,omitempty"`
	}

	response := unlockBadgeResp{
		// indicates whether the badge is being unlocked right now (false = it was already unlocked)
		Unlocked:  false,
		BadgeName: badge.Name,
		BadgeID:   badge.BadgeID,
	}

	// check whether the user has already unlocked the badge
	// if so, return OK
	{
		userBadge, err := scyllaClientUniverse.GetUserBadge(usr.ID, badge.BadgeID)
		if err != nil {
			respondWithError(http.StatusInternalServerError, err.Error(), w)
			return
		}
		if userBadge != nil {
			// success (badge was already unlocked)
			respondWithJSON(http.StatusOK, response, w)
			return
		}
	}

	// create user badge (unlock DB record)
	err = scyllaClientUniverse.InsertUserBadge(usr.ID, badge.BadgeID)
	if err != nil {
		respondWithError(http.StatusInternalServerError, err.Error(), w)
		return
	}

	// success (badge is being unlocked right now)
	response.Unlocked = true

	respondWithJSON(http.StatusOK, response, w)
}

// ----------------------------------------------
// Utility functions
// ----------------------------------------------

// Return values:
// - []byte: the image bytes
// - error: the error
func getBadgeThumbnailBytes(badgeID string, resizeWidth *int) ([]byte, error) {

	// path to the thumbnail file
	path := filepath.Join(BADGES_STORAGE_PATH, badgeID, "thumbnail.png")

	// check if file exists
	_, err := os.Stat(path)
	if os.IsNotExist(err) {
		return nil, errors.New("file not found")
	}

	// open file
	pngFile, err := os.Open(path)
	if err != nil {
		return nil, err
	}
	defer pngFile.Close()

	thumbnailBytes := []byte{}

	if resizeWidth != nil && *resizeWidth > 0 {
		// resize the image
		// create a buffer to store the resized image
		resizedBuf := bytes.Buffer{}
		// resize image
		err = resizePNGImage(pngFile, &resizedBuf, uint(*resizeWidth))
		if err != nil {
			return nil, err
		}
		thumbnailBytes = resizedBuf.Bytes()
	} else {
		// do not resize the image
		thumbnailBytes, err = io.ReadAll(pngFile)
		if err != nil {
			return nil, err
		}
	}

	return thumbnailBytes, nil
}
