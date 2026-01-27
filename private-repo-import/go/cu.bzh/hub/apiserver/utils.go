package main

import (
	"bytes"
	"crypto/ecdsa"
	"crypto/elliptic"
	"crypto/sha256"
	"crypto/x509"
	"encoding/binary"
	"encoding/hex"
	"encoding/json"
	"encoding/pem"
	"errors"
	"fmt"
	"image"
	"image/gif"
	"image/jpeg"
	"image/png"
	"io"
	"math/big"
	"math/rand/v2"
	"net"
	"net/http"
	"os"
	"path/filepath"
	"regexp"
	"sort"
	"strings"
	"time"
	"unicode"

	"github.com/fxamacker/cbor"
	"github.com/google/uuid"
	"github.com/sergi/go-diff/diffmatchpatch"

	"golang.org/x/image/draw"

	"blip.game/git"
	"cu.bzh/ai"
	"cu.bzh/ai/anthropic"
	"cu.bzh/hub/dbclient"
	"cu.bzh/hub/game"
	"cu.bzh/hub/item"
	"cu.bzh/hub/scyllaclient/model/kvs"
	"cu.bzh/hub/search"
	"cu.bzh/hub/user"
)

// writeFile atomically writes the contents of r to the specified filepath.  If
// an error occurs, the target file is guaranteed to be either fully written, or
// not written at all.  WriteFile overwrites any file that exists at the
// location (but only if the write fully succeeds, otherwise the existing file
// is unmodified).
func writeFile(r io.Reader, filename string, mode os.FileMode) error {

	dir, file := filepath.Split(filename)
	if dir == "" {
		dir = "."
	} else {
		if _, err := os.Stat(dir); os.IsNotExist(err) {
			// create parent dirs if not found
			err = os.MkdirAll(dir, 0755)
			if err != nil {
				return err
			}
		}
	}

	f, err := os.CreateTemp(dir, file)
	if err != nil {
		return fmt.Errorf("cannot create temp file: %v", err)
	}

	name := f.Name()
	if _, err := io.Copy(f, r); err != nil {
		_ = f.Close()
		_ = os.Remove(name)
		return fmt.Errorf("cannot write data to tempfile %q: %v", name, err)
	}

	// fsync is important, otherwise os.Rename could rename a zero-length file
	if err := f.Sync(); err != nil {
		_ = f.Close()
		_ = os.Remove(name)
		return fmt.Errorf("can't flush tempfile %q: %v", name, err)
	}
	if err := f.Close(); err != nil {
		return fmt.Errorf("can't close tempfile %q: %v", name, err)
	}

	_, err = os.Stat(filename)
	if err != nil && os.IsNotExist(err) == false {
		// it's ok if the file is not present,
		// but other errors are not accepted.
		_ = os.Remove(name)
		return err
	}

	// MODE ERRORS
	/*else {
		sourceInfo, err := os.Stat(name)
		if err != nil {
			return err
		}

		if sourceInfo.Mode() != destInfo.Mode() {
			if err := os.Chmod(name, destInfo.Mode()); err != nil {
				return fmt.Errorf("can't set filemode on tempfile %q: %v", name, err)
			}
		}
	}*/

	if err := os.Chmod(name, mode); err != nil {
		return fmt.Errorf("can't set filemode on tempfile %q: %v", name, err)
	}

	if err := os.Rename(name, filename); err != nil {
		return fmt.Errorf("cannot replace %q with tempfile %q: %v", filename, name, err)
	}

	return nil
}

func under13(dob *time.Time) bool {

	now := time.Now()
	currentYear := now.Year()
	dobYear := dob.Year()

	if currentYear-dobYear > 13 {
		return false
	} else if currentYear-dobYear == 13 {
		currentMonth := now.Month()
		dobMonth := dob.Month()
		if currentMonth > dobMonth {
			return false
		} else if currentMonth == dobMonth {
			currentDay := now.Day()
			dobDay := dob.Day()
			if currentDay >= dobDay {
				return false
			}
		}
	}

	return true
}

func getHostAndPort(r *http.Request) (string, string) {
	// addr := r.Header.Get("X-FORWARDED-FOR")
	// if addr == "" {
	// 	addr = r.RemoteAddr
	// }
	addr := r.RemoteAddr
	host, port, err := net.SplitHostPort(addr)
	if err != nil {
		host = addr
		if r.TLS != nil {
			port = "443"
		} else {
			port = "80"
		}
	}

	return host, port
}

// getCredentialsFromRequest
func getCredentialsFromRequest(r *http.Request) (*UserCredentials, error) {
	if r == nil {
		return nil, errors.New("request is nil")
	}

	userID := r.Header.Get(HTTP_HEADER_CUBZH_USER_ID_V2)
	if userID == "" {
		userID = r.Header.Get(HTTP_HEADER_CUBZH_USER_ID)
	}

	userToken := r.Header.Get(HTTP_HEADER_CUBZH_USER_TOKEN_V2)
	if userToken == "" {
		userToken = r.Header.Get(HTTP_HEADER_CUBZH_USER_TOKEN)
	}

	if len(userID) <= 0 {
		return nil, errors.New("user ID header is empty or missing")
	}
	if len(userToken) <= 0 {
		return nil, errors.New("user token header is empty or missing")
	}

	credentials := NewUserCredentials(userID, userToken)

	return credentials, nil
}

// `fileExtension` argument must contain a dot '.'
func ensureItemFileIsRemoved(itm *item.Item, fileExtension string) error {
	if itm.Repo == "" || itm.Name == "" || fileExtension == "" {
		return errors.New("bad arguments")
	}

	var err error

	// get item path
	itemPath := filepath.Join(ITEMS_STORAGE_PATH, itm.Repo, itm.Name+fileExtension)

	// check if path exists
	_, err = os.Stat(itemPath)
	if os.IsNotExist(err) {
		return nil // no error
	}

	// delete item from disk
	err = os.Remove(itemPath)
	return err
}

func appendTokenIfMissing(tokens []user.Token, newToken user.Token) []user.Token {
	for i, t := range tokens {
		if t.Token == newToken.Token {
			// update timestamp & variant values
			tokens[i].Variant = newToken.Variant
			tokens[i].Timestamp = newToken.Timestamp
			return tokens
		}
	}
	return append(tokens, newToken)
}

func parseItemFullname(fullname string) (repo string, name string, err error) {
	if fullname == "" {
		return "", "", errors.New("fullname is empty")
	}
	// split fullname
	s := strings.Split(fullname, ".")
	if len(s) != 2 {
		return "", "", errors.New("fullname is invalid")
	}
	return s[0], s[1], nil
}

// ----------------------------------------
// Search Engine utils
// ----------------------------------------

// searchEngineUpsertWorld ...
func searchEngineUpsertWorld(w *game.Game) error {

	if w == nil {
		return errors.New("world is nil")
	}

	// create search engine client
	searchClient, err := search.NewClient(CUBZH_SEARCH_SERVER, CUBZH_SEARCH_APIKEY)
	if err != nil {
		return err
	}

	// upsert world in search engine (World collection)
	{
		sew, err := newSearchEngineWorldFromGame(w)
		if err != nil {
			return err
		}
		err = searchClient.UpsertWorld(sew)
		if err != nil {
			return err
		}
	}

	// upsert world in search engine (Creation collection)
	{
		sec, err := newSearchEngineCreationFromWorld(w)
		if err != nil {
			return err
		}
		err = searchClient.UpsertCreation(sec)
		if err != nil {
			return err
		}
	}

	return err
}

// searchEngineUpdateWorld ...
func searchEngineUpdateWorld(wu search.WorldUpdate) error {

	// create search engine client
	searchClient, err := search.NewClient(CUBZH_SEARCH_SERVER, CUBZH_SEARCH_APIKEY)
	if err != nil {
		return err
	}

	// update World in search engine
	{
		err = searchClient.UpdateWorld(wu)
		if err != nil {
			return err
		}
	}

	// update Creation in search engine
	{
		cu := search.NewCreationUpdateFromWorldUpdate(wu)
		err = searchClient.UpdateCreation(cu)
		if err != nil {
			return err
		}
	}

	return err
}

// searchEngineUpsertItem ...
func searchEngineUpsertItem(itm *item.Item) error {

	if itm == nil {
		return errors.New("item is nil")
	}

	// create search engine client
	searchClient, err := search.NewClient(CUBZH_SEARCH_SERVER, CUBZH_SEARCH_APIKEY)
	if err != nil {
		return err
	}

	// upsert item in search engine (Item collection)
	{
		sew, err := newSearchEngineItemFromItem(itm)
		if err != nil {
			return err
		}
		err = searchClient.UpsertItemDraft(sew)
		if err != nil {
			return err
		}
	}

	// upsert item in search engine (Creation collection)
	{
		sec, err := newSearchEngineCreationFromItem(itm)
		if err != nil {
			return err
		}
		err = searchClient.UpsertCreation(sec)
		if err != nil {
			return err
		}
	}

	return err
}

func newSearchEngineWorldFromGame(w *game.Game) (search.World, error) {
	if w == nil {
		return search.World{}, errors.New("world is nil")
	}
	return search.NewWorldFromValues(
		w.ID,
		w.Title,
		w.Author,
		w.AuthorName,
		w.Created,
		w.Updated,
		w.Likes,
		w.Views,
		w.Featured,
		w.Archived,
		len(w.ContentWarnings) > 0,
	), nil
}

func newSearchEngineItemFromItem(itm *item.Item) (search.ItemDraft, error) {
	if itm == nil {
		return search.ItemDraft{}, errors.New("item is nil")
	}
	return search.NewItemDraftFromValues(
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
		itm.Views,
	), nil
}

// newSearchEngineCreationFromWorld creates a new search engine creation from a world.
// It is used to upsert a world in the search engine. (in the "Creation" collection)
func newSearchEngineCreationFromWorld(w *game.Game) (search.Creation, error) {
	if w == nil {
		return search.Creation{}, errors.New("world is nil")
	}

	creation, err := search.NewCreationFromValues(
		search.CreationTypeWorld,   //
		w.ID,                       //
		w.Title,                    // name
		w.Author,                   //
		w.AuthorName,               //
		w.Description,              //
		"",                         // category
		w.Created,                  //
		w.Updated,                  //
		w.Views,                    //
		w.Likes,                    //
		w.Featured,                 //
		len(w.ContentWarnings) > 0, // banned
		w.Archived,                 //
		"",                         // itemFullname
		0,                          // itemBlockCount
		"",                         // itemType
	)
	if err != nil {
		return search.Creation{}, err
	}

	return creation, nil
}

// newSearchEngineCreationFromItem creates a new search engine creation from an item.
// It is used to upsert an item in the search engine. (in the "Creation" collection)
func newSearchEngineCreationFromItem(itm *item.Item) (search.Creation, error) {
	if itm == nil {
		return search.Creation{}, errors.New("item is nil")
	}
	// itemType might be empty for old items ("voxels" items)
	itemType := itm.ItemType
	if itemType == "" {
		fmt.Println("[⚠️][newSearchEngineCreationFromItem] itemType is empty for item:", itm.ID, itm.Name)
		itemType = "voxels"
	}
	creation, err := search.NewCreationFromValues(
		search.CreationTypeItem, //
		itm.ID,                  //
		itm.Name,                //
		itm.Author,              //
		itm.Repo,                // authorName
		itm.Description,         //
		itm.Category,            //
		itm.Created,             //
		itm.Updated,             //
		itm.Views,               //
		itm.Likes,               //
		itm.Featured,            //
		itm.Banned,              //
		itm.Archived,            //
		itm.Repo+"."+itm.Name,   // itemFullname
		int64(itm.BlockCount),   // itemBlockCount
		itemType,                // itemType
	)
	if err != nil {
		return search.Creation{}, err
	}

	return creation, nil
}

func SanitizePhoneNumberLocally(phone string) string {
	// remove all non-digit characters
	re := regexp.MustCompile(`\D`)
	phone = re.ReplaceAllString(phone, "")
	// add + prefix
	phone = "+" + phone
	return phone
}

func constructSMSForParentDashboard(dashboardURL string) string {
	return "You've been appointed as a parent or guardian for an account on Cubzh. 🙂\nHere's the link to your dashboard to approve it: " + dashboardURL
}

func preludeSendDashboardSMSToParent(parentPhoneNumber, dashboardURL string) error {

	type Payload struct {
		To         string            `json:"to"`
		TemplateID string            `json:"template_id"`
		Variables  map[string]string `json:"variables"`
	}

	// Define the URL
	url := "https://api.ding.live/v2/transactional"

	// Get the authorization token from environment
	token := os.Getenv("PRELUDE_SMS_TOKEN")

	// Define the JSON payload
	payload := Payload{
		To:         parentPhoneNumber,
		TemplateID: "template_01hp78bjhcef9tej1tgcg0cpq2",
		Variables: map[string]string{
			"content": constructSMSForParentDashboard(dashboardURL),
		},
	}

	jsonPayload, err := json.Marshal(payload)
	if err != nil {
		return fmt.Errorf("failed to marshal payload: %w", err)
	}

	// Create a new POST request with the JSON payload
	req, err := http.NewRequest("POST", url, bytes.NewBuffer(jsonPayload))
	if err != nil {
		return fmt.Errorf("failed to create request: %w", err)
	}

	// Set the required headers
	req.Header.Set("Authorization", "Bearer "+token)
	req.Header.Set("Content-Type", "application/json")

	// Create a client and send the request
	client := &http.Client{}
	resp, err := client.Do(req)
	if err != nil {
		return fmt.Errorf("failed to send request: %w", err)
	}
	defer resp.Body.Close()

	// Check the response status
	if resp.StatusCode != http.StatusOK {
		body, _ := io.ReadAll(resp.Body)
		return fmt.Errorf("request failed with status %d: %s", resp.StatusCode, string(body))
	}

	// Optionally, read the response body
	body, err := io.ReadAll(resp.Body)
	if err != nil {
		return fmt.Errorf("failed to read response body: %w", err)
	}

	fmt.Println("🐞 PRELUDE RESPONSE:", string(body))
	return nil
}

func CheckPhoneNumber(phoneNumber string) (bool, string, error) {

	if len(phoneNumber) == 0 {
		return false, "", errors.New("phone number is empty")
	}

	// remove '+' prefix for now (if present)
	// we will add it back later
	phoneNumber = strings.TrimPrefix(phoneNumber, "+")

	// remove all characters that are not digits
	phoneNumber = strings.Map(func(r rune) rune {
		if unicode.IsDigit(r) {
			return r
		}
		return -1 // character is dropped
	}, phoneNumber)

	phoneNumber = "+" + phoneNumber

	// check if the phone number is valid using Twilio's Lookup API
	info, err := twilioSanitizePhoneNumberWithAPI(phoneNumber)
	if err != nil {
		return false, "", err
	}

	if info.IsValid {
		phoneNumber = info.PhoneNumber
	} else {
		phoneNumber = ""
	}

	return info.IsValid, phoneNumber, nil // success
}

func extractNScoresAround(scores []kvs.KvsLeaderboardRecord, reference kvs.KvsLeaderboardRecord, n int) []kvs.KvsLeaderboardRecord {
	// Step 1: Sort the scores
	sort.Slice(scores, func(i, j int) bool {
		return scores[i].Score < scores[j].Score
	})

	// Step 2: Find the index of the reference score
	var refIndex int
	found := false
	for i, score := range scores {
		if score.Score == reference.Score {
			refIndex = i
			found = true
			break
		}
	}
	if !found {
		fmt.Println("Reference score not found in the list.")
		return []kvs.KvsLeaderboardRecord{}
	}

	// Step 3: Extract the n scores above and below the reference score
	start := refIndex - n
	if start < 0 {
		start = 0
	}
	end := refIndex + n + 1
	if end > len(scores) {
		end = len(scores)
	}

	// Return the slice of scores around the reference
	return scores[start:end]
}

func isValidUUID(uuidStr string) bool {
	_, err := uuid.Parse(uuidStr)
	return err == nil
}

func randomString(length int) string {
	b := make([]byte, length)
	for i := range b {
		b[i] = byte(rand.IntN(256))
	}
	return hex.EncodeToString(b)
}

func resizePNGImage(input io.Reader, output io.Writer, newWidth uint) error {
	// Decode the image (from PNG to image.Image):
	src, err := png.Decode(input)
	if err != nil {
		return err
	}

	// Compute the new height based on the new width
	srcRatio := float64(src.Bounds().Max.X) / float64(src.Bounds().Max.Y)
	newHeight := uint(float64(newWidth) * srcRatio)

	// Set the expected size that you want:
	dst := image.NewRGBA(image.Rect(0, 0, int(newWidth), int(newHeight)))

	// Resize:
	draw.NearestNeighbor.Scale(dst, dst.Rect, src, src.Bounds(), draw.Over, nil)

	// Encode to output
	return png.Encode(output, dst)
}

// image can be PNG, JPEG or GIF
func convertImageToPNG(input []byte) ([]byte, error) {
	var err error

	// decode the input image to image.Image
	var src image.Image
	{
		_, err = png.Decode(bytes.NewReader(input))
		if err == nil {
			return input, nil // already a PNG, nothing to do
		}
		// try to decode as JPEG
		isDecoded := false
		if !isDecoded {
			src, err = jpeg.Decode(bytes.NewReader(input))
			if err == nil {
				isDecoded = true
			}
		}
		// try to decode as GIF
		if !isDecoded {
			src, err = gif.Decode(bytes.NewReader(input))
			if err == nil {
				isDecoded = true
			}
		}
		// if we failed to decode the image, return an error
		if !isDecoded {
			return nil, errors.New("failed to decode image")
		}
	}

	// encode the image to PNG
	var output []byte
	{
		buf := bytes.NewBuffer(nil)
		err = png.Encode(buf, src)
		if err != nil {
			return nil, err
		}
		output = buf.Bytes()
	}

	return output, nil
}

// ----------------------------------------
// Filesystem utility functions
// ----------------------------------------

type pathInfo struct {
	Exists   bool
	IsDir    bool
	EmptyDir bool
}

func getPathInfo(path string) (pathInfo, error) {

	result := pathInfo{
		Exists:   false,
		IsDir:    false,
		EmptyDir: false,
	}

	fi, err := os.Stat(path)
	if errors.Is(err, os.ErrNotExist) {
		return result, nil
	}
	if err != nil {
		return result, err
	}

	result.Exists = true
	result.IsDir = fi.IsDir()

	// Only check if empty when it's actually a directory
	if result.IsDir {
		entries, err := os.ReadDir(path)
		if err != nil {
			return result, fmt.Errorf("failed to read directory contents: %w", err)
		}
		result.EmptyDir = len(entries) == 0
	}

	return result, nil
}

// ----------------------------------------
// World utility functions
// ----------------------------------------

const codeFileGitRepoRelativePath = "main.lua"

// WorldGetScript returns the correct script for the given World.
// If world.DefaultScript is true, it returns the default script.
// Otherwise, it has a script either in the database or in the Git repository.
// The `clientBuildNumber` is used to determine the correct default script to use. (this argument will be removed later)
func worldUtilsGetScript(g *game.Game, clientBuildNumber *int) (string, error) {
	if g == nil {
		return "", errors.New("world is nil")
	}

	// check if script is in the Git repository
	_, script, err := getWorldScriptFromGit(g.ID)
	if err == nil {
		// script found in Git repository
		fmt.Println("[🐞][⭐️][WorldGetScript] ✅ from Git repo")
		return script, nil
	}

	if g.DefaultScript == true {
		// return the default script
		script := game.GetDefaultScriptToUse(clientBuildNumber)
		fmt.Println("[🐞][⭐️][WorldGetScript] ✅ default script")
		return script, nil
	}

	// script not found in Git repository, and .DefaultScript is false
	// Return the script from the database
	fmt.Println("[🐞][⭐️][WorldGetScript] ✅ from database")
	return g.Script, nil
}

type UpdateWorldCodeOpts struct {
	CommitMessage string
}

// High level function to update the code of a world.
func updateWorldCode(dbClient *dbclient.Client, worldID, code string, opts UpdateWorldCodeOpts) error {
	const DEFAULT_COMMIT_MESSAGE = "update code"
	var err error

	// Validate arguments
	if dbClient == nil {
		return errors.New("dbClient is nil")
	}
	if worldID == "" {
		return errors.New("worldID is empty")
	}

	// First, check if new code is different from the one in the git repository.
	// If it has NOT changed, we don't need to do anything and we return success.
	// TODO: gdevillele: Check this. Apparently, it's not working. (the IF block is never reached)
	{
		// check if script is in the Git repository
		_, scriptInGit, err := getWorldScriptFromGit(worldID)
		if err == nil && scriptInGit == code {
			// script found in Git repository and is the same as the code
			return nil
		}
	}

	var world *game.Game

	// Ensure the git repository exists
	{
		repoJustCreated, err := worldUtilsEnsureCodeGitRepo(worldID)
		if err != nil {
			return err
		}
		// If the repository was just created:
		// - we insert the initial commit with the default script
		// - we add the script from the database to the git repository (if any)
		if repoJustCreated {
			defaultScript := game.GetDefaultScriptToUse(nil)
			_, err = worldUtilsSimpleCodeCommit(worldID, defaultScript, "initial commit: default script")
			if err != nil {
				return err
			}
			// Check if there is a script in the database and there is, we commit it in the git repo
			world, err = game.GetByID(dbClient, worldID, "", nil)
			if err != nil {
				return err
			}
			if world.DefaultScript == true {
				// we togggle DefaultScript to false
				world.DefaultScript = false
				err = world.Save(dbClient, nil)
				if err != nil {
					return err
				}
			} else if world.Script != "" {
				// script found in database, we add it to the git repo
				_, err = worldUtilsSimpleCodeCommit(worldID, world.Script, "migrate code from database to git")
				if err != nil {
					return err
				}
			}
		}
	}

	// commit message
	commitMessage := DEFAULT_COMMIT_MESSAGE
	if opts.CommitMessage != "" {
		commitMessage = opts.CommitMessage
	}

	_, err = worldUtilsSimpleCodeCommit(worldID, code, commitMessage)
	if err != nil && err != git.ErrEmptyCommit {
		return err
	}

	// no error or ErrEmptyCommit

	// update "updatedAt" field in the database
	if world == nil {
		world, err = game.GetByID(dbClient, worldID, "", nil)
		if err != nil {
			return err
		}
	}

	err = world.UpdateUpdatedAt(dbClient)

	return err
}

// worldUtilsEnsureCodeGitRepo ensures that the code git repository exists for a given world ID
// If the repository does not exist, it is created and (true, nil) is returned.
// If the repository exists, it is not modified and (false, nil) is returned.
func worldUtilsEnsureCodeGitRepo(worldID string) (bool, error) {
	var err error

	// absolute path to the code repository
	worldCodeRepoAbsPath := getWorldCodeRepoPath(worldID)

	// Try to open the repository
	_, err = git.OpenRepo(worldCodeRepoAbsPath)
	if err == nil {
		// repository exists
		return false, nil // success
	}
	// there was an error opening the repository
	if err != git.ErrRepositoryNotExists {
		return false, err
	}

	// Repository does not exist, we need to create it.

	// check if there is already a file or directory at the repository path
	repoPathInfo, err := getPathInfo(worldCodeRepoAbsPath)
	if err != nil {
		return false, err
	}

	// If the path exists but is not a directory, return an error
	if repoPathInfo.Exists && repoPathInfo.IsDir == false {
		// THIS SHOULD NEVER HAPPEN
		fmt.Println("[❌][worldUtilsEnsureCodeGitRepo] SHOULD NEVER HAPPEN: code repository is not a directory:", worldCodeRepoAbsPath)
		return false, errors.New("repository path exists but is not a directory")
	}

	// Either the path does not exist, or it exists and is a directory.
	// In both cases, we need to create the repository.

	// if the path does not exist, create a directory
	if repoPathInfo.Exists == false {
		// create the directory
		err = os.MkdirAll(worldCodeRepoAbsPath, 0755)
		if err != nil {
			return false, err
		}
		repoPathInfo.Exists = true
		repoPathInfo.IsDir = true
		repoPathInfo.EmptyDir = true
	}

	// If the directory is not empty, return an error
	if repoPathInfo.EmptyDir == false {
		fmt.Println("[❌][worldUtilsEnsureCodeGitRepo] SHOULD NEVER HAPPEN: code repository is not empty:", worldCodeRepoAbsPath)
		return false, errors.New("git repository doesn't exist and its directory is not empty")
	}

	// Directory is empty

	// Let's initialize the git repository
	repo, err := git.InitRepo(worldCodeRepoAbsPath)
	if err != nil {
		return false, err
	}
	if repo == nil {
		return false, errors.New("failed to initialize code repository (nil repo)")
	}

	return true, nil
}

var (
	ErrNoChangesInCode = errors.New("no changes in code")
)

type WorldUtilsGenerateCommitMessageOpts struct {
	AnthropicAPIKey string
	CodeBefore      string
	CodeAfter       string
}

func worldUtilsGenerateCommitMessage(opts WorldUtilsGenerateCommitMessageOpts) (string, error) {
	const (
		DIFF_MAX_LENGTH               = 5000
		COMMIT_MESSAGE_MAX_LENGTH     = 150
		ERR_COMMIT_MESSAGE_GENERATION = "error: could not name commit"
	)

	if opts.CodeBefore == opts.CodeAfter {
		return "", ErrNoChangesInCode
	}

	if opts.AnthropicAPIKey == "" {
		return "", errors.New("anthropic API key is missing")
	}

	// Generate diff using go-diff package
	dmp := diffmatchpatch.New()
	diffs := dmp.DiffMain(opts.CodeBefore, opts.CodeAfter, true)

	// Convert diffs to a readable format
	var diffLines []string
	for _, diff := range diffs {
		switch diff.Type {
		case diffmatchpatch.DiffInsert:
			diffLines = append(diffLines, "+ "+diff.Text)
		case diffmatchpatch.DiffDelete:
			diffLines = append(diffLines, "- "+diff.Text)
		case diffmatchpatch.DiffEqual:
			// Skip unchanged lines to keep the diff concise
			continue
		}
	}

	// Join diff lines and limit length
	diffText := strings.Join(diffLines, "\n")
	truncated := false
	if len(diffText) > DIFF_MAX_LENGTH {
		diffText = diffText[:DIFF_MAX_LENGTH] + "… (DIFF TOO LONG, TRUNCATED)"
		truncated = true
	}

	haikuModel := anthropic.NewModel(anthropic.MODEL_CLAUDE_3_5_HAIKU)
	if haikuModel == nil {
		return "", errors.New("failed to create haiku model")
	}

	systemPrompt := ai.NewSystemPrompt()
	systemPrompt.Append(ai.NewTextPart("You are an experienced programmer."))
	systemPrompt.Append(ai.NewTextPart("You are given a diff of code changes, and you have to write the commit message for it."))
	systemPrompt.Append(ai.NewTextPart("The commit message must be short and concise, and must be written in the present tense."))
	systemPrompt.Append(ai.NewTextPart("It's ok to for the tone to be a bit playful, but not too much. Use emojis if you want to."))
	if truncated {
		systemPrompt.Append(ai.NewTextPart("The diff is too long, so it was truncated, do your best with what you have."))
	}
	systemPrompt.Append(ai.NewTextPart("You must return only the commit message, nothing else."))

	var userPrompt string
	userPrompt += "Code diff:\n" + diffText

	response, err := haikuModel.GenerateSimple(userPrompt, ai.GenerateOpts{
		APIKey:       opts.AnthropicAPIKey,
		SystemPrompt: systemPrompt,
		MaxTokens:    256,
	})
	if err != nil {
		// If AI couldn't generate a commit message, we use this hardcoded value.
		response = ERR_COMMIT_MESSAGE_GENERATION
	}

	// Post processing
	msg := response
	msg = strings.TrimSpace(msg)
	if strings.Contains(msg, "\n") {
		// replace all newlines with a space
		msg = strings.ReplaceAll(msg, "\n", " ")
	}
	// limit message length
	if len(msg) > COMMIT_MESSAGE_MAX_LENGTH {
		msg = msg[:COMMIT_MESSAGE_MAX_LENGTH]
	}

	return msg, nil
}

// getWorldCodeRepoPath returns the path to the code repository for a given world ID
// It doesn't perform any check on the path.
func getWorldCodeRepoPath(worldID string) string {
	fullpath := filepath.Join(WORLDS_STORAGE_PATH, worldID, "code")
	return fullpath
}

func getWorldScriptFromGit(worldID string) (git.Commit, string, error) {
	repoFullpath := getWorldCodeRepoPath(worldID)
	// open git repository
	repo, err := git.OpenRepo(repoFullpath)
	if err != nil {
		return git.Commit{}, "", err
	}
	// get last commit and script
	commit, script, err := repo.GetLastCommitAndCode(codeFileGitRepoRelativePath)
	if err != nil {
		return git.Commit{}, "", err
	}
	return commit, script, nil
}

type Commit struct {
	CreatedAt time.Time `json:"created"`
	Message   string    `json:"message"`
	Hash      string    `json:"hash"`
}

// getWorldScriptHistory returns the history of the script of a given world.
func worldUtilsGetScriptHistory(worldID string) ([]Commit, error) {

	// absolute path to the code repository
	repoFullpath := getWorldCodeRepoPath(worldID)

	// open git repository
	repo, err := git.OpenRepo(repoFullpath)
	if err != nil {
		return nil, err
	}

	// get the history of the world
	gitCommits, err := repo.GetCommitHistory(git.GetCommitHistoryOpts{})
	if err != nil {
		return nil, err
	}

	commits := make([]Commit, 0)

	for _, gitCommit := range gitCommits {
		commits = append(commits, Commit{
			CreatedAt: gitCommit.CreatedAt,
			Message:   gitCommit.Message,
			Hash:      gitCommit.Hash,
		})
	}

	return commits, nil
}

func worldUtilsRollbackToCommit(worldID, commitHash string) error {

	// absolute path to the code repository
	repoFullpath := getWorldCodeRepoPath(worldID)

	// open git repository
	repo, err := git.OpenRepo(repoFullpath)
	if err != nil {
		return err
	}

	// checkout the commit
	err = repo.ResetHard(commitHash)
	if err != nil {
		return err
	}

	return nil // success
}

// worldUtilsSimpleCodeCommit writes a new version of a world code and commits it.
// The Git repository must already exist.
func worldUtilsSimpleCodeCommit(worldID, code, commitMessage string) (git.Hash, error) {

	// absolute path to the git repository
	gitRepoFullpath := getWorldCodeRepoPath(worldID)

	// absolute path to the code file (main.lua)
	codeFileFullpath := filepath.Join(gitRepoFullpath, codeFileGitRepoRelativePath)

	// open git repository
	repo, err := git.OpenRepo(gitRepoFullpath)
	if err != nil {
		return git.Hash{}, err
	}

	// write the code file entire content
	err = os.WriteFile(codeFileFullpath, []byte(code), 0755)
	if err != nil {
		return git.Hash{}, err
	}

	// git add
	err = repo.Add(codeFileGitRepoRelativePath)
	if err != nil {
		return git.Hash{}, err
	}

	// git commit
	var hash git.Hash
	hash, err = repo.Commit(commitMessage, "", "", time.Now(), git.CommitOpts{})
	if err != nil {
		return git.Hash{}, err
	}

	return hash, nil // success
}

// ----------------------------------------------
// Passkey
// ----------------------------------------------

type AttestedCredentialData struct {
	AAGUID              []byte
	CredentialID        []byte
	CredentialPublicKey []byte
}

// parseAuthData parses the authData field of a Passkey attestation object.
// It returns the AttestedCredentialData and the signCount.
func parseAuthData(authData []byte, expectedRPID string) (*AttestedCredentialData, int, error) {
	if len(authData) < 37 {
		return nil, 0, errors.New("authData too short")
	}

	rpIDHash := authData[0:32]
	flags := authData[32]
	signCount := binary.BigEndian.Uint32(authData[33:37])

	// Verify RP ID hash matches SHA-256 hash of expected RP ID
	hash := sha256.Sum256([]byte(expectedRPID))
	if !bytes.Equal(rpIDHash, hash[:]) {
		return nil, 0, errors.New("RP ID hash does not match")
	}

	// Check flags
	const flagAT = 0x40
	hasAttestedCredentialData := (flags & flagAT) != 0
	if !hasAttestedCredentialData {
		return nil, 0, errors.New("attested credential data not present")
	}

	// Parse attested credential data
	cursor := 37

	// AAGUID: 16 bytes
	if len(authData) < cursor+16 {
		return nil, 0, errors.New("missing AAGUID")
	}
	aaguid := authData[cursor : cursor+16]
	cursor += 16

	// Credential ID length: 2 bytes
	if len(authData) < cursor+2 {
		return nil, 0, errors.New("missing credential ID length")
	}
	credIDLen := binary.BigEndian.Uint16(authData[cursor : cursor+2])
	cursor += 2

	// Credential ID
	if len(authData) < cursor+int(credIDLen) {
		return nil, 0, errors.New("credential ID truncated")
	}
	credID := authData[cursor : cursor+int(credIDLen)]
	cursor += int(credIDLen)

	// Credential Public Key (COSE_Key format)
	if len(authData) <= cursor {
		return nil, 0, errors.New("credential public key missing")
	}
	coseKey := authData[cursor:]

	return &AttestedCredentialData{
		AAGUID:              aaguid,
		CredentialID:        credID,
		CredentialPublicKey: coseKey,
	}, int(signCount), nil
}

type COSEEC2Key struct {
	Kty int    `cbor:"1,keyasint"`
	Alg int    `cbor:"3,keyasint"`
	Crv int    `cbor:"-1,keyasint"`
	X   []byte `cbor:"-2,keyasint"`
	Y   []byte `cbor:"-3,keyasint"`
}

func decodeCOSEKeyToECDSA(coseKeyBytes []byte) (*ecdsa.PublicKey, error) {
	var key COSEEC2Key
	if err := cbor.Unmarshal(coseKeyBytes, &key); err != nil {
		return nil, fmt.Errorf("CBOR unmarshal failed: %w", err)
	}

	if key.Kty != 2 || key.Crv != 1 {
		return nil, fmt.Errorf("unsupported key type or curve: kty=%d crv=%d", key.Kty, key.Crv)
	}

	x := new(big.Int).SetBytes(key.X)
	y := new(big.Int).SetBytes(key.Y)

	pubKey := &ecdsa.PublicKey{
		Curve: elliptic.P256(),
		X:     x,
		Y:     y,
	}
	return pubKey, nil
}

func publicKeyToPEM(pubKey *ecdsa.PublicKey) ([]byte, error) {
	derBytes, err := x509.MarshalPKIXPublicKey(pubKey)
	if err != nil {
		return nil, fmt.Errorf("failed to marshal public key: %w", err)
	}
	pemBytes := pem.EncodeToMemory(&pem.Block{
		Type:  "PUBLIC KEY",
		Bytes: derBytes,
	})
	return pemBytes, nil
}

func decodeCOSEKeyToPEM(coseKeyBytes []byte) (string, error) {
	pubKey, err := decodeCOSEKeyToECDSA(coseKeyBytes)
	if err != nil {
		return "", err
	}
	pemBytes, err := publicKeyToPEM(pubKey)
	if err != nil {
		return "", err
	}
	return string(pemBytes), nil
}
