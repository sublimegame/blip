package main

import (
	"fmt"
	"os"
	"strings"

	"cu.bzh/hub/dbclient"
	"cu.bzh/hub/search"
	"cu.bzh/sessionstore"
)

const (
	WORLDS_STORAGE_PATH             = "/voxowl/hub/worlds" // /voxowl/hub/worlds/cff877c9-2a26-475e-be44-681ec630e7d8/code
	ITEMS_STORAGE_PATH              = "/voxowl/hub/items"
	BADGES_STORAGE_PATH             = "/voxowl/hub/badges" // /voxowl/hub/badges/cff877c9-2a26-475e-be44-681ec630e7d8/thumbnail.png
	sessionKey                      = "particubes-session"
	nonVerifiedSessionTTL           = 10                 // in seconds
	verifiedSessionTTL              = 60 * 30            // in seconds (30 minutes)
	loggedInSessionTTL              = 60 * 60 * 24 * 10  // in seconds (10 days)
	gameServerVerifiedSessionTTL    = 60 * 60 * 24 * 365 // in seconds (1 year) // TODO: should be shorter and renewed
	// Salt values loaded from environment at runtime
	serverChainedCertFile = "/cubzh/certificates/cu.bzh.chained.crt"
	serverKeyFile                   = "/cubzh/certificates/cu.bzh.key"
	DISCORDBOT_NOTIF_GAMESERVER_TAG = "82f674ea"
	// AI
	ENVAR_ANTHROPIC_API_KEY      = "ANTHROPIC_API_KEY"
	ENVAR_ANTHROPIC_API_KEY_FILE = "ANTHROPIC_API_KEY_FILE"
	// Search
	kSearchDefaultText    string = "*"
	kSearchDefaultPage    int    = 1
	kSearchDefaultPerPage int    = 10
	// Search - Items
	kItemSearchDefaultMinBlock int    = 5
	kItemSearchDefaultArchived bool   = false
	kItemSearchDefaultSortBy   string = search.KItemDraftFieldUpdatedAt + ":desc" // search.KItemDraftFieldLikes + ":desc" // "likes:desc"
	// Search - Worlds
	kWorldSearchDefaultCategory string = ""
	kWorldSearchDefaultPerPage  int    = 100 // TO REMOVE when the client app sends a value (current is 0.0.59)
	kWorldSearchDefaultSortBy   string = "updatedAt:desc"
)

var (
	// AI
	AnthropicAPIKey     string = ""
	AnthropicAPIKeyFile string = ""
	OPENAI_API_KEY      string = ""
	OPENAI_API_PROJECT  string = ""

	// Mailjet
	MAILJET_API_KEY    string = ""
	MAILJET_API_SECRET string = ""

	// Salts
	clientSalt     string = ""
	gameServerSalt string = ""

	// Search
	CUBZH_SEARCH_SERVER string = ""
	CUBZH_SEARCH_APIKEY string = ""

	sessionStore *sessionstore.SessionStore

	// host for game servers
	gameServersRequestChan = make(chan *gameServersRequest, 64)

	// default search filters for Items
	kSearchDefaultCategories []string = make([]string, 0)
	kSearchDefaultRepos      []string = make([]string, 0)
)

// ensureSecrets loads secrets into global variables
func ensureSecrets() error {

	if len(CUBZH_SEARCH_SERVER) == 0 {
		b, err := os.ReadFile("/cubzh/secrets/CUBZH_SEARCH_SERVER")
		if err != nil {
			fmt.Println("❌ ERROR [ensureSecrets]", err.Error())
			CUBZH_SEARCH_SERVER = "http://144.126.245.237:8108" // fall back to stagging server
		} else {
			CUBZH_SEARCH_SERVER = string(b)
		}
	}

	if len(CUBZH_SEARCH_APIKEY) == 0 {
		b, err := os.ReadFile("/cubzh/secrets/CUBZH_SEARCH_APIKEY")
		if err != nil {
			fmt.Println("❌ ERROR [ensureSecrets]", err.Error())
			CUBZH_SEARCH_APIKEY = "Hu52ddsas2AdxdE" // fall back to stagging server
		} else {
			CUBZH_SEARCH_APIKEY = string(b)
		}
	}

	if len(OPENAI_API_KEY) == 0 {
		b, err := os.ReadFile("/cubzh/secrets/OPENAI_API_KEY")
		if err != nil {
			fmt.Println("❌ ERROR [ensureSecrets]", err.Error())
			OPENAI_API_KEY = ""
		} else {
			OPENAI_API_KEY = string(b)
		}
	}

	if len(OPENAI_API_PROJECT) == 0 {
		b, err := os.ReadFile("/cubzh/secrets/OPENAI_API_PROJECT")
		if err != nil {
			fmt.Println("❌ ERROR [ensureSecrets]", err.Error())
			OPENAI_API_PROJECT = ""
		} else {
			OPENAI_API_PROJECT = string(b)
		}
	}

	// Mailjet
	if len(MAILJET_API_KEY) == 0 {
		MAILJET_API_KEY = os.Getenv("MAILJET_API_KEY")
	}
	if len(MAILJET_API_SECRET) == 0 {
		MAILJET_API_SECRET = os.Getenv("MAILJET_API_SECRET")
	}

	// Twilio
	if len(TWILIO_ACCOUNT_SID) == 0 {
		TWILIO_ACCOUNT_SID = os.Getenv("TWILIO_ACCOUNT_SID")
	}
	if len(TWILIO_CUBZH_PHONE_NUMBER) == 0 {
		TWILIO_CUBZH_PHONE_NUMBER = os.Getenv("TWILIO_PHONE_NUMBER")
	}

	// Salts
	if len(clientSalt) == 0 {
		clientSalt = os.Getenv("CLIENT_SALT")
	}
	if len(gameServerSalt) == 0 {
		gameServerSalt = os.Getenv("GAME_SERVER_SALT")
	}

	return nil
}

func main() {
	var err error

	err = ensureSecrets()
	if err != nil {
		fmt.Println(err.Error())
		os.Exit(1)
	}

	// --------------------------------------------------
	// retrieve environment variables' values
	// --------------------------------------------------
	// secure transport
	var envSecureTransport bool = os.Getenv("PCUBES_SECURE_TRANSPORT") == "1"
	fmt.Println("[Hub] secure transport:", envSecureTransport)
	// AI
	AnthropicAPIKey = os.Getenv(ENVAR_ANTHROPIC_API_KEY)
	AnthropicAPIKeyFile = os.Getenv(ENVAR_ANTHROPIC_API_KEY_FILE)
	if AnthropicAPIKey == "" && AnthropicAPIKeyFile == "" {
		fmt.Printf("❌ ERROR: at least one of the 2 Anthropic API keys is missing (%s or %s)\n", ENVAR_ANTHROPIC_API_KEY, ENVAR_ANTHROPIC_API_KEY_FILE)
		os.Exit(1)
	}
	if AnthropicAPIKey == "" && AnthropicAPIKeyFile != "" {
		b, err := os.ReadFile(AnthropicAPIKeyFile)
		if err != nil {
			fmt.Println("❌ ERROR: failed to read Anthropic API key file:", err.Error())
			os.Exit(1)
		}
		AnthropicAPIKey = strings.TrimSpace(string(b))
	}

	dbClient, err := dbclient.SharedUserDB()
	if err != nil {
		fmt.Println("🔥 can't get database shared client", err.Error())
		return
	}

	sessionStore, err = sessionstore.NewSessionStore(10, "tcp", "session-db:6379", "")
	if err != nil {
		fmt.Println("🔥 can't get session store", err.Error())
		return
	}
	sessionStore.SetMaxAge(10 * 24 * 3600) // 10 days
	defer sessionStore.Close()

	err = dbClient.EnsureUserIndexes()
	if err != nil {
		fmt.Println("🔥 can't ensure user indexes", err.Error())
		return
	}

	err = dbClient.EnsureGameIndexes()
	if err != nil {
		fmt.Println("🔥 can't ensure game indexes", err.Error())
		return
	}

	err = dbClient.EnsureItemIndexes()
	if err != nil {
		fmt.Println("🔥 can't ensure item indexes", err.Error())
		return
	}

	go getGameServerRoutine(gameServersRequestChan)

	fmt.Println("✨ Running server! ✨")

	// select the port on which to listen
	listenPort := ":80"
	if envSecureTransport {
		listenPort = ":443"
	}

	fmt.Println("- public HTTP API listening on", listenPort)
	if envSecureTransport {
		runPublicHTTPServerSecure(listenPort, serverChainedCertFile, serverKeyFile)
	} else {
		runPublicHTTPServerUnsecure(listenPort)
	}
}
