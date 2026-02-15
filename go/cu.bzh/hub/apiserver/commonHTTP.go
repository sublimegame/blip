package main

import (
	"bytes"
	"compress/gzip"
	"crypto/md5"
	"encoding/json"
	"errors"
	"fmt"
	"html/template"
	"io"
	"net/http"
	"os"
	"path/filepath"
	"strconv"
	"strings"

	"cu.bzh/hub/dbclient"
	"cu.bzh/hub/item"
	"cu.bzh/hub/scyllaclient"
	"cu.bzh/hub/user"
	"github.com/andybalholm/brotli"
	"github.com/go-chi/chi/v5"
)

const (
	OFFICIAL_ITEM_REPO string = "official"
	DEFAULT_USERNAME   string = "noob"
)

type serverError struct {
	Code    int    `json:"code,omitempty"`
	Message string `json:"msg,omitempty"`
}

// HTTP 200 OK
func respondOK(w http.ResponseWriter) {
	w.Header().Set("Content-Type", "application/json")
	w.WriteHeader(http.StatusOK)
	fmt.Fprint(w, "{}")
}

// HTTP 304 Not Modified
func respondNotModified(w http.ResponseWriter) {
	w.Header().Set("Content-Type", "application/json")
	w.WriteHeader(http.StatusNotModified)
	fmt.Fprint(w, "{}")
}

func respondWithHTML(code int, msg string, w http.ResponseWriter) {
	w.Header().Set("Content-Type", "text/html; charset=utf-8")
	w.WriteHeader(code)
	fmt.Fprint(w, msg)
}

func respondWithHTMLTemplate(code int, templateName string, content map[string]interface{}, w http.ResponseWriter) {
	w.Header().Set("Content-Type", "text/html; charset=utf-8")
	w.WriteHeader(code)

	// Parse the HTML template.
	tmpl, err := template.ParseFiles("templates/page_emailVerified.html")
	if err != nil {
		http.Error(w, "Internal Server Error", http.StatusInternalServerError)
		return
	}

	// Execute the template, passing the data.
	err = tmpl.Execute(w, content)
	if err != nil {
		http.Error(w, "Internal Server Error", http.StatusInternalServerError)
		return
	}
}

func respondWithError(code int, msg string, w http.ResponseWriter) {
	respondWithJSON(code, &serverError{Code: code, Message: msg}, w)
}

func respondWithJSON(code int, payload interface{}, w http.ResponseWriter) {
	jsonString, err := json.Marshal(payload)
	if err != nil {
		fmt.Println("❌ respondWithJSON - error marshalling payload:", err.Error())
		w.WriteHeader(http.StatusInternalServerError)
		w.Write([]byte("{}"))
		return
	}

	// write headers and status code
	w.Header().Set("Content-Type", "application/json")
	w.Header().Set("Content-Length", fmt.Sprintf("%d", len(jsonString)))
	w.WriteHeader(code)

	// write body
	w.Write(jsonString)
}

func respondWithJSONV2(code int, payload interface{}, r *http.Request, w http.ResponseWriter) {
	jsonBytes, err := json.Marshal(payload)
	if err != nil {
		w.WriteHeader(http.StatusInternalServerError)
		w.Write([]byte("{}"))
		return
	}

	responseBytes := jsonBytes

	// compress response if JSON size is > 40 bytes and client accepts it
	// if len(jsonBytes) > 40 {
	acceptEncodingHeader := r.Header.Get("Accept-Encoding")
	fmt.Println("[🐞] acceptEncodingHeader:", acceptEncodingHeader)
	if strings.Contains(acceptEncodingHeader, "br") {
		var b bytes.Buffer
		br := brotli.NewWriter(&b)
		_, err := br.Write(jsonBytes)
		if err == nil {
			err = br.Close()
		}
		if err == nil {
			responseBytes = b.Bytes()
			w.Header().Set("Content-Encoding", "br")
		}
	} else if strings.Contains(acceptEncodingHeader, "gzip") {
		var b bytes.Buffer
		gz := gzip.NewWriter(&b)
		_, err := gz.Write(jsonBytes)
		if err == nil {
			err = gz.Close()
		}
		if err == nil {
			responseBytes = b.Bytes()
			w.Header().Set("Content-Encoding", "gzip")
		}
	}
	// }

	// write headers and status code
	w.Header().Set("Content-Type", "application/json")
	w.Header().Set("Content-Length", fmt.Sprintf("%d", len(responseBytes)))
	w.Header().Set("Vx-Content-Length", fmt.Sprintf("%d", len(jsonBytes)))
	w.Header().Set("Content-Length-Uncompressed", fmt.Sprintf("%d", len(jsonBytes)))
	w.WriteHeader(code)

	// write body
	w.Write(responseBytes)
}

func respondWithJSONPretty(code int, payload interface{}, w http.ResponseWriter) {
	w.Header().Set("Content-Type", "application/json")
	prettyJSON, err := json.MarshalIndent(payload, "", "    ")
	if err != nil {
		w.WriteHeader(http.StatusInternalServerError)
		w.Write([]byte("{}"))
		return
	}
	w.Header().Set("Content-Length", fmt.Sprintf("%d", len(prettyJSON)))
	w.WriteHeader(code)
	w.Write(prettyJSON)
}

func respondWithRawJSON(code int, jsonBytes []byte, w http.ResponseWriter) {
	w.Header().Set("Content-Type", "application/json")
	w.Header().Set("Content-Length", fmt.Sprintf("%d", len(jsonBytes)))
	w.WriteHeader(code)
	w.Write(jsonBytes)
}

func respondWithRawData(code int, bytes []byte, w http.ResponseWriter) {
	w.Header().Set("Content-Type", "application/octet-stream")
	w.Header().Set("Content-Length", fmt.Sprintf("%d", len(bytes)))
	w.WriteHeader(code)
	w.Write(bytes)
}

func respondWithString(code int, str string, w http.ResponseWriter) {
	w.Header().Set("Content-Type", "text/plain; charset=utf-8")
	strBytes := []byte(str)
	w.Header().Set("Content-Length", fmt.Sprintf("%d", len(strBytes)))
	w.WriteHeader(code)
	w.Write(strBytes)
}

func notFound(w http.ResponseWriter, r *http.Request) {
	respondWithError(http.StatusNotFound, "not found", w)
}

func getItem(id string, w http.ResponseWriter, r *http.Request) {
	dbClient, err := dbclient.SharedUserDB()
	if err != nil {
		respondWithError(http.StatusInternalServerError, "", w)
		return
	}

	// get item info from database
	item, err := item.GetByID(dbClient, id, "")
	if err != nil {
		respondWithError(http.StatusBadRequest, "", w)
		return
	}

	res := &getItemRes{
		Item: &Item{
			ID:               item.ID,
			AuthorID:         item.Author,
			AuthorName:       "...",
			Contributors:     make([]string, 0),
			ContributorNames: make([]string, 0),
			Repo:             item.Repo,
			Name:             item.Name,
			Public:           item.Public,
			Description:      item.Description,
			Featured:         item.Featured,
			Likes:            0,
			Views:            0,
			Tags:             make([]string, 0),
		},
	}

	respondWithJSON(http.StatusOK, res, w)
}

// Responds with item's pcubes data (application/octet-stream)
// A repo/name pair or an id should be provided as parameters.
// etag is optional, allows to respond with http.StatusNotModified
// if the client already has the file in cache.
func getItemPcubes(repo, name, id, etag string, w http.ResponseWriter, r *http.Request) {

	dbClient, err := dbclient.SharedUserDB()
	if err != nil {
		respondWithError(http.StatusInternalServerError, "", w)
		return
	}

	// if id != "" {
	// TODO: retrieve repo and name if id is provided
	// repo = ...
	// name = ...
	// }

	currentEtag := ""

	pcubesFilePath := filepath.Join(ITEMS_STORAGE_PATH, repo, name+".pcubes")

	_, err = os.Stat(pcubesFilePath)
	if os.IsNotExist(err) {
		respondWithError(http.StatusBadRequest, "file not found", w)
		return
	}

	pcubesEtagPath := filepath.Join(ITEMS_STORAGE_PATH, repo, name+".etag")

	_, err = os.Stat(pcubesEtagPath)

	if err != nil {
		if os.IsNotExist(err) {

			// Failed to find .etag, create it!
			file, err := os.Open(pcubesFilePath)
			if err != nil {
				fmt.Println("can't open file")
				respondWithError(http.StatusInternalServerError, "", w)
				return
			}
			defer file.Close()

			hash := md5.New()
			_, err = io.Copy(hash, file)
			if err != nil {
				fmt.Println("can't create md5")
				respondWithError(http.StatusInternalServerError, "", w)
				return
			}

			md5Str := fmt.Sprintf("%x", hash.Sum(nil))

			etagFile, err := os.Create(pcubesEtagPath)
			if err != nil {
				fmt.Println("can't create etag file")
				respondWithError(http.StatusInternalServerError, "", w)
				return
			}
			defer etagFile.Close()

			_, err = etagFile.WriteString(md5Str)
			if err != nil {
				fmt.Println("can't write etag in file")
				respondWithError(http.StatusInternalServerError, "", w)
				return
			}

			etagFile.Sync()

		} else { // other error
			respondWithError(http.StatusInternalServerError, "", w)
			return
		}
	}

	_, err = os.Stat(pcubesEtagPath)
	if err != nil {
		if os.IsNotExist(err) {
			// Failed to find .etag, internal error
			fmt.Println("pcubes not found when requesting with etag:", repo, name)
			respondWithError(http.StatusInternalServerError, "", w)
			return
		} else {
			respondWithError(http.StatusInternalServerError, "", w)
			return
		}
	}

	etagFile, err := os.Open(pcubesEtagPath)
	if err != nil {
		fmt.Println("can't open etag file")
		respondWithError(http.StatusInternalServerError, "", w)
		return
	}
	defer etagFile.Close()

	bytes, err := io.ReadAll(etagFile)
	if err != nil {
		fmt.Println("can't read etag file")
		respondWithError(http.StatusInternalServerError, "", w)
		return
	}

	currentEtag = string(bytes)

	if etag != "" && currentEtag == etag {
		// user has correct etag!
		respondNotModified(w)
		return
	}

	var itemFromDB *item.Item = nil
	var itemsFromDB []*item.Item = nil

	if id != "" {
		// get item info from database (by ID)
		itemFromDB, err = item.GetByID(dbClient, id, "")
	} else {
		// get item info from database (by Repo + Name)
		// fmt.Println("get item: ", req.Repo, req.Name)
		itemsFromDB, err = item.GetByRepoAndName(dbClient, repo, name)
		// fmt.Println("🎟 ITEMS FROM DB: ", itemsFromDB, len(itemsFromDB), err)
		if err == nil && len(itemsFromDB) > 0 {
			itemFromDB = itemsFromDB[0]
		}
	}

	if err != nil {
		fmt.Println("not found (1)")
		respondWithError(http.StatusBadRequest, "", w)
		return
	}

	if itemFromDB == nil {
		fmt.Println("not found (2)")
		respondWithError(http.StatusBadRequest, "", w)
		return
	}

	// construct pcubes abs path
	path := filepath.Join(ITEMS_STORAGE_PATH, itemFromDB.Repo, itemFromDB.Name+".pcubes")

	_, err = os.Stat(path)
	if os.IsNotExist(err) {
		// Failed to find a pcubes file for the item.
		// Returning the default pcubes file containing 1 cube.
		fmt.Println("pcubes not found, returning default pcubes file...")
		path = filepath.Join(ITEMS_STORAGE_PATH, "official", "default.pcubes")
		_, err = os.Stat(path)
		if os.IsNotExist(err) {
			fmt.Println("default pcubes not found")
			respondWithError(http.StatusBadRequest, "", w)
			return
		}
	}

	file, err := os.Open(path)
	if err != nil {
		fmt.Println("can't open file")
		respondWithError(http.StatusInternalServerError, "", w)
		return
	}
	defer file.Close()

	fi, err := file.Stat()
	if err != nil {
		respondWithError(http.StatusInternalServerError, "", w)
		return
	}

	w.Header().Set("Content-Type", "application/octet-stream")
	w.Header().Set("Content-Length", strconv.Itoa(int(fi.Size())))
	w.Header().Set("ETag", currentEtag)

	w.WriteHeader(http.StatusOK)

	// can't change the header afterwards if copy fails
	// no error handling here for now.
	_, _ = io.Copy(w, file)
}

// Responds with item's 3zh data (application/octet-stream)
// A repo/name pair or an id should be provided as parameters.
// etag is optional, allows to respond with http.StatusNotModified
// if the client already has the file in cache.
// If no .3zh is found on the server, tries to convert .pcubes if found
func getItemModelFile(repo, name, id, etag, maxAge string, w http.ResponseWriter, r *http.Request) {
	var err error

	// avatar v3 is enabled for all users since build number 83
	useAvatarV3 := true

	// if requested item is a body part, check the user record in DB
	// to see if the user has chosen a custom item to use for this body part
	isBodyPartName := item.IsItemNameForBodyPart(name)
	customBodyPartName := false

	if isBodyPartName {
		// we expect `repo` and `name` to be provided but `id` to be empty
		if len(id) != 0 || len(repo) <= 0 || len(name) <= 0 {
			errorMsg := "When requesting a body part, only 'repo' & 'name' should be provided."
			fmt.Println("[getItemModel]", errorMsg)
			respondWithError(http.StatusBadRequest, errorMsg, w)
			return
		}
		if repo == OFFICIAL_ITEM_REPO {
			// Item "official.<bodyPart>" is requested.
			// Do nothing, and leave `customBodyPartName` to `false`.
		} else if repo == DEFAULT_USERNAME { // "noob"
			// Item "noob.<bodyPart>" is requested.
			// Do nothing, and leave `customBodyPartName` to `false`,
			// so that "official.<bodyPart>" is returned.
		} else {
			dbClient, err := dbclient.SharedUserDB()
			if err != nil {
				respondWithError(http.StatusInternalServerError, "", w)
				return
			}

			// use `repo` to request the user record from DB
			usr, err := user.GetByUsername(dbClient, repo)
			if err != nil {
				respondWithError(http.StatusInternalServerError, "", w)
				return
			}
			if usr != nil {
				// user has been found
				repoAndName, err := usr.GetChosenBodyPartName(name)
				if err != nil {
					respondWithError(http.StatusInternalServerError, "", w)
					return
				}
				if repoAndName != "" {
					elements := strings.Split(repoAndName, ".")
					if len(elements) == 2 {
						repo = elements[0]
						name = elements[1]
						customBodyPartName = true
					}
				}
			} else {
				// User has not been found.
				// This should not happend, but we can return default body part (repo "official").
				fmt.Println("[getItemModel] user not found, this should not happen", repo)
				// Do nothing, and leave `customBodyPartName` to `false`,
				// so that "official.<bodyPart>" is returned.
			}
		}
	}

	isEquipmentName := item.IsItemNameForEquipment(name)
	customEquipmentName := false

	if isEquipmentName {
		if isBodyPartName {
			errorMsg := "Requested item cannot be both a body part and an equipment."
			fmt.Println("[getItemModel]", errorMsg)
			respondWithError(http.StatusBadRequest, errorMsg, w)
			return
		}
		// we expect `repo` and `name` to be provided but `id` to be empty
		if len(id) != 0 || len(repo) <= 0 || len(name) <= 0 {
			errorMsg := "When requesting an equipment, only 'repo' & 'name' should be provided."
			fmt.Println("[getItemModel]", errorMsg)
			respondWithError(http.StatusBadRequest, errorMsg, w)
			return
		}
		if repo == OFFICIAL_ITEM_REPO {
			// Item "official.<equipment>" is requested.
			// Do nothing, and leave `customEquipmentName` to `false`.
		} else if repo == DEFAULT_USERNAME { // "noob"
			// Item "noob.<equipment>" is requested.
			// Do nothing, and leave `customEquipmentName` to `false`,
			// so that "official.<equipment>" is returned.
		} else {
			dbClient, err := dbclient.SharedUserDB()
			if err != nil {
				respondWithError(http.StatusInternalServerError, "", w)
				return
			}

			// use `repo` to request the user record from DB
			usr, err := user.GetByUsername(dbClient, repo)
			if err != nil {
				respondWithError(http.StatusInternalServerError, "", w)
				return
			}
			if usr != nil {
				// user has been found
				repoAndName, err := usr.GetChosenEquipmentName(name) // ex: "pants" => "caillef.realpants"
				if err != nil {
					respondWithError(http.StatusInternalServerError, "", w)
					return
				}
				if repoAndName != "" {
					elements := strings.Split(repoAndName, ".")
					if len(elements) == 2 {
						repo = elements[0]
						name = elements[1]
						customEquipmentName = true
					}
				}
			} else {
				// User has not been found.
				// This should not happend, but we can return default equipment (repo "official").
				fmt.Println("[getItemModel] user not found, this should not happen", repo)
				// Do nothing, and leave `customEquipmentName` to `false`,
				// so that "official.<equipment>" is returned.
			}
		}
	}

	if isBodyPartName && customBodyPartName == false {
		// If no custom body part name has been found in DB, then use the "official" repo.
		// Same thing for equipments.
		repo = "official"

		if useAvatarV3 {
			name, err = getOfficialItemNameForBodyPartName(name)
			if err != nil {
				respondWithError(http.StatusInternalServerError, err.Error(), w)
				return
			}
		}

	} else if isEquipmentName && customEquipmentName == false {
		// If no custom body part name has been found in DB, then use the "official" repo.
		// Same thing for equipments.
		repo = "official"

		if useAvatarV3 {
			name, err = getOfficialItemNameForEquipmentName(name)
			if err != nil {
				respondWithError(http.StatusInternalServerError, err.Error(), w)
				return
			}
		}
	}

	// if id != "" {
	// TODO: retrieve repo and name if id is provided
	// repo = ...
	// name = ...
	// }

	currentEtag := ""

	// LOOK FOR MESH FILE (.glb) FIRST
	modelFilePath := filepath.Join(ITEMS_STORAGE_PATH, repo, name+".glb")
	_, err = os.Stat(modelFilePath)
	if os.IsNotExist(err) {
		// THEN LOOK FOR .3zh FILE (voxels)
		modelFilePath = filepath.Join(ITEMS_STORAGE_PATH, repo, name+".3zh")
		_, err = os.Stat(modelFilePath)
		if os.IsNotExist(err) {
			// NO .glb, NO .3zh - LOOK FOR .pcubes FILE (legacy)
			pcubesFilePath := filepath.Join(ITEMS_STORAGE_PATH, repo, name+".pcubes")
			if os.IsNotExist(err) {
				// no .glb, no .3zh, no .pcubes - no possible model file found
				fmt.Println("[getItemModel] file not found for", repo, name)
				respondWithError(http.StatusBadRequest, "file not found", w)
				return
			}

			// OLD .pcubes file found, convert it to .3zh
			// Just changing the magic bytes and saving the file with new extension

			file, err := os.Open(pcubesFilePath)
			if err != nil {
				fmt.Println("can't open file")
				respondWithError(http.StatusInternalServerError, "", w)
				return
			}

			// skip `PARTICUBES!` magic bytes
			_, err = file.Seek(11, io.SeekStart)
			if err != nil {
				file.Close()
				fmt.Println("invalid file format")
				respondWithError(http.StatusInternalServerError, "", w)
				return
			}

			cubzhFile, err := os.Create(modelFilePath) // .3zh file
			if err != nil {
				file.Close()
				fmt.Println("can't create file")
				respondWithError(http.StatusInternalServerError, "", w)
				return
			}

			// write new magic bytes
			_, err = cubzhFile.WriteString("CUBZH!")
			if err != nil {
				file.Close()
				cubzhFile.Close()
				fmt.Println("can't write magic bytes file")
				respondWithError(http.StatusInternalServerError, "", w)
				return
			}

			_, err = io.Copy(cubzhFile, file)
			if err != nil {
				file.Close()
				cubzhFile.Close()
				fmt.Println("can't copy bytes")
				respondWithError(http.StatusInternalServerError, "", w)
				return
			}

			file.Close()
			cubzhFile.Close()
		}
	}

	etagPath := filepath.Join(ITEMS_STORAGE_PATH, repo, name+".model.etag")

	// CHECK ETAG FILE:
	// - CREATE IF NOT FOUND
	_, err = os.Stat(etagPath)
	if err != nil {
		if os.IsNotExist(err) {
			// Failed to find .etag, create it!
			file, err := os.Open(modelFilePath)
			if err != nil {
				fmt.Println("can't open file")
				respondWithError(http.StatusInternalServerError, "", w)
				return
			}
			defer file.Close()

			hash := md5.New()
			_, err = io.Copy(hash, file)
			if err != nil {
				fmt.Println("can't create md5")
				respondWithError(http.StatusInternalServerError, "", w)
				return
			}

			md5Str := fmt.Sprintf("%x", hash.Sum(nil))

			etagFile, err := os.Create(etagPath)
			if err != nil {
				fmt.Println("can't create etag file")
				respondWithError(http.StatusInternalServerError, "", w)
				return
			}
			defer etagFile.Close()

			_, err = etagFile.WriteString(md5Str)
			if err != nil {
				fmt.Println("can't write etag in file")
				respondWithError(http.StatusInternalServerError, "", w)
				return
			}

			etagFile.Close()

		} else { // other error
			respondWithError(http.StatusInternalServerError, "", w)
			return
		}
	}

	etagFile, err := os.Open(etagPath)
	if err != nil {
		fmt.Println("❌ can't open etag file")
		respondWithError(http.StatusInternalServerError, "", w)
		return
	}
	defer etagFile.Close()

	bytes, err := io.ReadAll(etagFile)
	if err != nil {
		fmt.Println("❌ can't read etag file")
		respondWithError(http.StatusInternalServerError, "", w)
		return
	}

	currentEtag = string(bytes)

	if etag != "" && currentEtag == etag {
		// user has correct etag!
		respondNotModified(w)
		return
	}

	file, err := os.Open(modelFilePath)
	if err != nil {
		fmt.Println("can't open file")
		respondWithError(http.StatusInternalServerError, "", w)
		return
	}
	defer file.Close()

	fi, err := file.Stat()
	if err != nil {
		respondWithError(http.StatusInternalServerError, "", w)
		return
	}

	w.Header().Set("Content-Type", "application/octet-stream")
	w.Header().Set("Content-Length", strconv.Itoa(int(fi.Size())))
	w.Header().Set("ETag", currentEtag)
	// if a non-empty maxAge value has been provided, add the response header
	if len(maxAge) > 0 {
		w.Header().Set("Cache-Control", "max-age="+maxAge+", s-maxage="+maxAge)
	}

	w.WriteHeader(http.StatusOK)

	// can't change the header afterwards if copy fails
	// no error handling here for now.
	_, _ = io.Copy(w, file)
}

func getOfficialItemNameForBodyPartName(bodyPartName string) (itemName string, err error) {
	itemName = ""
	err = nil

	switch bodyPartName {
	case "body":
		itemName = "avatar_v3_body"
	case "head":
		itemName = "avatar_v3_head"
	case "left_arm":
		itemName = "avatar_v3_larm"
	case "left_foot":
		itemName = "avatar_v3_lfoot"
	case "left_hand":
		itemName = "avatar_v3_lhand"
	case "left_leg":
		itemName = "avatar_v3_lleg"
	case "right_arm":
		itemName = "avatar_v3_rarm"
	case "right_foot":
		itemName = "avatar_v3_rfoot"
	case "right_hand":
		itemName = "avatar_v3_rhand"
	case "right_leg":
		itemName = "avatar_v3_rleg"
	default:
		err = errors.New("invalid body part name")
	}

	return
}

func getOfficialItemNameForEquipmentName(equipmentName string) (itemName string, err error) {
	itemName = ""
	err = nil

	switch equipmentName {
	case "hair":
		itemName = "avatar_v3_hair"
	case "top":
		itemName = "avatar_v3_top"
	case "lsleeve":
		itemName = "avatar_v3_lsleeve"
	case "rsleeve":
		itemName = "avatar_v3_rsleeve"
	case "lpant":
		itemName = "avatar_v3_lpant"
	case "rpant":
		itemName = "avatar_v3_rpant"
	case "lglove":
		itemName = "avatar_v3_lglove"
	case "rglove":
		itemName = "avatar_v3_rglove"
	case "lboot":
		itemName = "avatar_v3_lboot"
	case "rboot":
		itemName = "avatar_v3_rboot"
	case "boots":
		itemName = "boots"
	case "jacket":
		itemName = "jacket"
	case "pants":
		itemName = "pants"
	default:
		err = errors.New("invalid equipment name")
	}

	return
}

// ------------------------------
// Key-Value Store
// ------------------------------

func getKVSValues(w http.ResponseWriter, r *http.Request) {

	// retrieve values from URL path
	worldID := chi.URLParam(r, "worldID")
	store := chi.URLParam(r, "store")

	// parse query parameter `format` for output format
	// format := "json"
	// if f := r.URL.Query().Get("format"); len(f) > 0 {
	// 	format = f
	// }

	// parse query parameter `key`
	keys := make([]string, 0)
	{
		values, found := r.URL.Query()["key"]
		if found {
			keys = values
		}
	}

	if len(keys) == 0 {
		respondWithError(http.StatusBadRequest, "at least one key must be provided", w)
		return
	}

	// New scyllaDB client
	records, err := scyllaClientKvs.GetUnorderedValuesByKeys(worldID, store, keys)
	if err != nil {
		fmt.Println("❌ getKVSValues (new):", err)
		respondWithError(http.StatusInternalServerError, "db query failed", w)
		return
	}

	resp := make(scyllaclient.KeyValueMap)
	for _, r := range records {
		resp[r.Key] = r.Value
	}

	respondWithJSONPretty(http.StatusOK, resp, w)
}

func patchKVSValues(w http.ResponseWriter, r *http.Request) {
	var err error

	// retrieve values from URL path
	worldID := chi.URLParam(r, "worldID")
	store := chi.URLParam(r, "store")

	// parse headers
	_ /*contentTypeHeader*/ = r.Header.Get("Content-Type")

	// parse body
	var keyValueMap scyllaclient.KeyValueMap
	err = json.NewDecoder(r.Body).Decode(&keyValueMap)
	if err != nil {
		respondWithError(http.StatusBadRequest, "can't decode json request", w)
		return
	}

	// New ScyllaDB client
	{
		err = scyllaClientKvs.SetUnorderedValuesByKeys(worldID, store, keyValueMap)
		if err != nil {
			fmt.Println("❌ patchKVSValues (new):", err)
			respondWithError(http.StatusInternalServerError, "db query failed", w)
			return
		}
	}

	respondOK(w)
}
