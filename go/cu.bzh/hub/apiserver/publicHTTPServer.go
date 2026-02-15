package main

import (
	"bytes"
	"context"
	"crypto/ecdsa"
	"crypto/sha256"
	"crypto/x509"
	"encoding/base64"
	"encoding/json"
	"encoding/pem"
	"errors"
	"fmt"
	"io"
	"log"
	"mime"
	"net/http"
	"net/url"
	"os"
	"os/exec"
	"path/filepath"
	"regexp"
	"slices"
	"sort"
	"strconv"
	"strings"
	"time"
	"unicode"

	dinggolang "github.com/ding-live/ding-golang"
	dingcomponents "github.com/ding-live/ding-golang/models/components"
	"github.com/go-chi/chi/v5"
	"github.com/go-chi/chi/v5/middleware"
	"github.com/google/uuid"

	"blip.game/discord"
	vxMiddleware "blip.game/middleware"
	"cu.bzh/ai/moderation"
	"cu.bzh/cors"
	"cu.bzh/hub/apnsclient"
	"cu.bzh/hub/dbclient"
	"cu.bzh/hub/fcmclient"
	"cu.bzh/hub/game"
	"cu.bzh/hub/item"
	"cu.bzh/hub/scyllaclient"
	"cu.bzh/hub/scyllaclient/model/kvs"
	"cu.bzh/hub/search"
	"cu.bzh/hub/transactions"
	"cu.bzh/hub/types"
	"cu.bzh/hub/user"
	"cu.bzh/mailer"
	"cu.bzh/types/gameserver"
	"cu.bzh/utils"
	httpUtils "cu.bzh/utils/http"
	"github.com/golang-jwt/jwt/v4"
)

const (
	// Client minimum version (forces users to update)
	CLIENT_MIN_VERSION = "0.1.22"

	// Discord
	DISCORD_DASHBOARD_CHANNEL_ID = "1060223186228224050"

	// World
	WORLD_MAX_PLAYERS_LIMIT_LOW  = 1
	WORLD_MAX_PLAYERS_LIMIT_HIGH = 64

	WORLD_TITLE_REGEX = "^.{0,49}$"
	ITEM_TITLE_REGEX  = "^[a-z][a-z0-9_]{0,19}$"
	// Description can contain up to 10K chars
	// - golang allows 1000 repetition maximum, so we have to use multiple selectors
	// - `(?s)` allows `.` selector to match newline char (\n)
	DESCRIPTION_REGEX       = "(?s)^.{0,1000}.{0,1000}.{0,1000}.{0,1000}.{0,1000}.{0,1000}.{0,1000}.{0,1000}.{0,1000}.{0,1000}$"
	USER_BIO_MAX_CHARACTERS = 140
	TEST_PHONE_NUMBER       = "+17777777777"
	TEST_PHONE_VERIF_CODE   = "777777"

	// --- Request Headers ---

	// app version and build number
	HTTP_HEADER_CUBZH_APP_VERSION_V2     = "Czh-Ver" // introduced on 0.0.48 patch 2
	HTTP_HEADER_CUBZH_APP_BUILDNUMBER_V2 = "Czh-Rev" // introduced on 0.0.48 patch 2
	// User credentials
	HTTP_HEADER_CUBZH_USER_ID       = "vx_userid"  // TO REMOVE part of user credentials
	HTTP_HEADER_CUBZH_USER_TOKEN    = "vx_token"   // TO REMOVE part of user credentials
	HTTP_HEADER_CUBZH_USER_ID_V2    = "Czh-Usr-Id" // introduced on 0.0.62
	HTTP_HEADER_CUBZH_USER_TOKEN_V2 = "Czh-Tk"     // introduced on 0.0.62
	HTTP_HEADER_IAP_SANDBOX_KEY     = "Blip-Iap-Sandbox-Key"
	// Game server credentials ?
	HTTP_HEADER_CUBZH_GAME_SERVER_TOKEN = "Czh-Server-Token"

	// --- Response Headers ---
	// ...

	// --- HTTP COOKIES ---
	HTTP_COOKIE_SESSION = "session"

	// QUERY PARAMETERS
	HTTP_QUERY_PARAM_REPO     = "repo"
	HTTP_QUERY_PARAM_CATEGORY = "category"

	// AI parameters
	AI_CHAT_COMPLETION_JSON  = "json"
	AI_IMAGE_GENERATION_JSON = "json"

	// Error messages
	errRequestDecodingFailed = "request decoding failed"
	errUserCredsMissing      = "user credentials headers missing"

	MAX_AGE_24H                 = "86400"
	MAX_AGE_2MIN                = "120"
	MAX_AGE_1H                  = "3600"
	CACHE_THUMBNAILS_MAX_AGE    = MAX_AGE_1H
	CACHE_FEATUREDGAMES_MAX_AGE = MAX_AGE_1H
	CACHE_ITEMS_3ZH_MAX_AGE     = MAX_AGE_2MIN

	// Push Notifications
	PUSH_TOKEN_TYPE_APPLE    = "apple"
	PUSH_TOKEN_TYPE_FIREBASE = "firebase"

	// SALTS
	USERNAME_CHECK_SALT = "a173cdcefa99aa95457b6bbb52b5566d"

	// DING - values loaded from environment
	DING_API_CUSTOMER_UUID = "d9d4f26c-db84-4a8d-9691-22af744e2889"

	// NOTIFICATIONS

	// Categories
	NOTIFICATION_CATEGORY_GENERIC = "generic" // direct messages, follow, etc.
	NOTIFICATION_CATEGORY_MONEY   = "money"   // grants, purchases, etc.
	NOTIFICATION_CATEGORY_SOCIAL  = "social"  // friend requests etc.
	NOTIFICATION_CATEGORY_LIKE    = "like"    // items & worlds likes

	// PASSKEY
	PASSKEY_RP_ID = "blip.game"
)

var (
	errInternal               error = errors.New("internal")
	errInvalidUsernameOrEmail error = errors.New("invalid username or email")

	supportedAvatarFields []string = []string{"boots", "jacket", "pants"}
	// TODO: gaetan: stop using those variables, and use "Shared()" accessors instead
	scyllaClientKvs        *scyllaclient.Client = nil
	scyllaClientModeration *scyllaclient.Client = nil
	scyllaClientUniverse   *scyllaclient.Client = nil

	// Discord
	discordToken                  = ""
	discordClient *discord.Client = nil

	// Ding API
	DING_API_KEY = ""

	IAP_CONSUMABLE_COIN_PRODUCTS = map[string]int{
		"blip.coins.1": 160,
		"blip.coins.2": 800,
		"blip.coins.3": 1600,
		"blip.coins.4": 3200,
		"blip.coins.5": 8000,
		"blip.coins.6": 16000,
		"blip.coins.7": 32000,
	}
)

func runPublicHTTPServerUnsecure(port string) {
	runPublicHTTPServer(false, port, "", "")
}

func runPublicHTTPServerSecure(port, certFilePath, keyFilePath string) {
	runPublicHTTPServer(true, port, certFilePath, keyFilePath)
}

func runPublicHTTPServer(secure bool, port, certFilePath, keyFilePath string) {
	var err error

	// Load secrets
	{
		// Twilio auth token
		{
			content, err := os.ReadFile("/cubzh/secrets/CUBZH_TWILIO_API_AUTH_TOKEN")
			if err != nil {
				log.Fatal("Error loading Twilio Auth Token:", err.Error())
				return
			}
			SECRET_TWILIO_API_AUTH_TOKEN = strings.TrimRight(string(content), "\n")
		}
		// Discord auth token
		{
			content, err := os.ReadFile("/voxowl/secrets/DISCORD_AUTH_TOKEN")
			if err != nil {
				log.Fatal("Error loading Discord token:", err.Error())
				return
			}
			discordToken = strings.TrimRight(string(content), "\n")
		}
		// Ding API key
		{
			DING_API_KEY = os.Getenv("DING_API_KEY")
			if DING_API_KEY == "" {
				log.Fatal("Error loading Ding API key: DING_API_KEY environment variable is not set")
				return
			}
		}
	}

	// Create scylla clients for different keyspaces
	{
		// Keyspace : "kvs"
		{
			config := scyllaclient.ClientConfig{
				Servers: []string{
					"hub-db:9042",
				},
				Username:  "",
				Password:  "",
				AwsRegion: "",
				Keyspace:  "kvs",
			}
			scyllaClientKvs, err = scyllaclient.NewShared(config)
			if err != nil {
				log.Fatal("failed to create scylla client (kvs):", err.Error())
			}
		}

		// Keyspace : "moderation"
		{
			config := scyllaclient.ClientConfig{
				Servers: []string{
					"hub-db:9042",
				},
				Username:  "",
				Password:  "",
				AwsRegion: "",
				Keyspace:  "moderation",
			}
			scyllaClientModeration, err = scyllaclient.NewShared(config)
			if err != nil {
				log.Fatal("failed to create scylla client (moderation):", err.Error())
			}
		}

		// Keyspace : "universe"
		{
			config := scyllaclient.ClientConfig{
				Servers: []string{
					"hub-db:9042",
				},
				Username:  "",
				Password:  "",
				AwsRegion: "",
				Keyspace:  "universe",
			}
			scyllaClientUniverse, err = scyllaclient.NewShared(config)
			if err != nil {
				log.Fatal("failed to create scylla client (universe):", err.Error())
			}
		}
	}

	// Preload featured items
	{
		searchClient, err := search.NewClient(CUBZH_SEARCH_SERVER, CUBZH_SEARCH_APIKEY)
		if err != nil {
			fmt.Println("❌ Failed to preload featured items:", err.Error())
		} else {
			preloadFeaturedItems(searchClient)
		}
	}

	// Discord client
	{
		discordClient, err = discord.NewClient(discordToken)
		if err != nil {
			log.Fatal("Error creating Discord client:", err.Error())
			return
		}
	}

	// Check Push notification clients are correctly configured (this crashes the program on error)
	_ = apnsclient.Shared(apnsclient.ClientOpts{})
	_ = fcmclient.Shared(fcmclient.ClientOpts{})

	router := chi.NewRouter()
	// router.Use(middleware.RequestID)
	// router.Use(middleware.RealIP)
	router.Use(middleware.Logger)
	router.Use(middleware.Recoverer)
	router.Use(cors.GetCubzhCORSHandler())
	router.Use(sessionMiddleware)
	router.Use(vxMiddleware.ReadUserCredentialsMiddleware) // Read user credentials from the request headers

	// -------------------------------
	// ADMIN API
	// -------------------------------
	router.Route("/admin", func(r chi.Router) {
		r.Use(cubzhClientVersionMiddleware)
		r.Use(authenticateUserMiddleware)
		r.Use(adminAuthMiddleware)
		r.Delete("/banuseraccount/{userID}", banUserAccount)
	})

	// -------------------------------
	// PRIVATE API is being moved here
	// -------------------------------

	// TODO: gaetan: move private API routes here
	router.Route("/private", func(r chi.Router) {
		// TODO: gaetan: re-enable this once this part is not in development anymore
		// r.Use(privateAPIClientFilterMiddleware)

		// WORLDS

		// Returns world for given world ID
		r.Get("/worlds/{id}", privateGetWorld) // ?field=script&field=mapBase64
		r.Get("/world/{id}", privateGetWorld)  // ?field=script&field=mapBase64

		// ITEMS

		r.Get("/items/{id}/model", privateGetItemModel)
		r.Get("/items/{repo}/{name}/model", privateGetItemModel)

		// GAMESERVERS

		// Updates server info
		r.Post("/server/{id}/info", postServerInfo)
		// Notification sent by exiting server
		r.Post("/server/{id}/exit", postServerExit)
		// KEY-VALUE STORE
		r.Get("/kvs/world/{worldID}/store/{store}", getKVSValues)
		r.Patch("/kvs/world/{worldID}/store/{store}", patchKVSValues)
		// CHAT MESSAGES
		r.Post("/chat/message", postChatMessage)

	})

	// -------------------------------
	// USER OPTIONALLY AUTHENTICATED ROUTES
	// -------------------------------

	router.Group(func(r chi.Router) {
		r.Use(cubzhClientVersionMiddleware)
		r.Use(optionalAuthMiddleware)
		// TODO: (gdevillele) add lastSeenMiddleware

		r.Get("/worlds/{id}", worldGet) // default: ?f=title&f=description

		// Avatar
		r.Get("/users/self/avatar", getUserAvatar)
		r.Get("/users/{idOrUsername}/avatar", getUserAvatar)

		r.Get("/module", moduleGet) // ?src=github.com/aduermael/pathfinding:v0.1
	})

	// -------------------------------
	// UNAUTHENTICATED ROUTES
	// -------------------------------

	router.Group(func(r chi.Router) {
		// TODO: unauthenticated routes, we need a middleware to avoid abuse, especially for POST requests
		r.Use(cubzhClientVersionMiddleware)

		r.Get("/users/{id}/email/verify", userVerifyEmail)

		// checks if username is valid and available
		r.Post("/checks/username", checkUsernameV2)
		// checks if phone number is valid and re-format it if possible
		r.Post("/checks/phonenumber", checkPhoneNumber)
		// create a new user account
		r.Post("/users", createUser)

		// Items
		r.Get("/items", getItems)
		r.Get("/itemdrafts", getItems) // TODO: gaetan: check if we can remove this
		r.Get("/items/{repo}/{name}/model", getItemModel)

		// -------------------------------
		// UTILS
		// -------------------------------
		// r.Get("/debug", debug)
		// r.Get("/sanitisePhone/{phoneNumber}", debugSanitisePhone)
		r.Get("/ping", ping)
		r.Get("/min-version", getMinVersion)

		// Parent dashboard
		// {
		// 	"secretKey": "456",
		// 	"settings": {
		// 		"allowAccess": true,
		// 		"allowIngameChat": true
		// 		"allowPrivateMessages": true,
		// 	},
		// }
		r.Get("/dashboards/{dashboardID}/{secretKey}", getParentDashboard)
		r.Patch("/dashboards/{dashboardID}/{secretKey}", patchParentDashboard)

		// Transactions

		// Create a payment request. This is used to make a user pay for an AI generation.
		r.Get("/transactions/payment-request", getTransactionsPaymentRequest)
		r.Post("/transactions/payment-request", postTransactionsPaymentRequest)
	})

	// -------------------------------
	// USER AUTHENTICATED ROUTES
	// -------------------------------

	router.Group(func(r chi.Router) {
		r.Use(cubzhClientVersionMiddleware)
		r.Use(authenticateUserMiddleware)
		r.Use(lastSeenMiddleware)

		// Elevated rights routes v2
		r.Route("/privileged", func(r chi.Router) {
			// allows users to delete their own account
			r.Delete("/users/self", deleteUser)

			// Notifications
			// Get notifications for the current user (?category=social&read=false&returnCount=true)
			r.Get("/users/self/notifications", getUserNotifications)
			// Patch a single notification (mark as read, etc.)
			r.Patch("/users/self/notifications/{id}", patchUserNotification)
			// Patch all notifications matching the conditions (mark all as read, etc.)
			r.Patch("/users/self/notifications", patchUserNotifications) // ?category=social&read=false

			// Leaderboard
			r.Get("/leaderboards/{worldID}/{leaderboardName}", getLeaderboard) // ?limit=10&mode=best&friends=true&fetchUsernames=true
			r.Get("/leaderboards/{worldID}/{leaderboardName}/{userID}", getLeaderboardUser)
			r.Get("/leaderboards/{worldID}/{leaderboardName}/self", getLeaderboardUser)
			r.Post("/leaderboards/{worldID}/{leaderboardName}", postLeaderboard)

			// Worlds
			r.Patch("/worlds/{id}/views", patchWorldViews) // increments views count
		})

		// BADGES

		r.Post("/badges", postBadge)
		r.Patch("/badges/{badgeID}", patchBadge)
		r.Get("/badges/{badgeID}", getBadgeByID)
		r.Get("/badges/{badgeID}/thumbnail.png", getBadgeThumbnail)

		// REPORTS

		r.Post("/reports", postReports)

		// USERS

		r.Get("/users", getUsers)
		r.Get("/users/self", getUserInfo)
		r.Get("/users/{id}", getUserInfo)
		r.Get("/users/self/friends", getUserFriendRelations)
		r.Get("/users/{id}/friends", getUserFriendRelations)
		r.Get("/users/self/friends/{friendID}", getUserFriendRelation)
		r.Get("/users/{id}/friends/{friendID}", getUserFriendRelation)
		r.Get("/users/self/friend-requests", getUserFriendRequests) // ?status=sent|received
		// TODO: gaetan: cleanup passkey routes
		r.Get("/users/self/passkey-challenge", getUserPasskeyChallenge)
		r.Post("/users/self/authorize-passkey", postUserPasskey)
		// r.Get("/users/{id}/friend-requests", getUserFriendRequests)
		// TODO: gaetan: add ability to remove a friend relation (aka "unfriend")
		r.Patch("/users/self", patchUserInfo)
		r.Patch("/users/self/avatar", patchUserAvatar)

		r.Post("/users/self/block", postBlockUser)
		r.Post("/users/self/unblock", postUnblockUser)

		r.Get("/users/self/badges", getUserBadges)     // unlocked badges
		r.Get("/users/{userID}/badges", getUserBadges) // unlocked badges
		r.Post("/users/self/badges", unlockBadge)

		// User balance
		r.Get("/users/self/balance", getUserBalance)
		// User transactions
		r.Get("/users/self/transactions", getUserRecentTransactions)
		// router.Get("/users/{id}/transactions", getUserRecentTransactions)

		// WORLDS

		r.Get("/worlds", getWorlds)
		r.Get("/worlds/{worldID}/badges", getWorldBadges)
		r.Get("/worlds/{id}/history", getWorldScriptHistory)
		r.Get("/worlds/{id}/server/{tag}", getWorldRunningServer)                 // provides server address
		r.Get("/worlds/{id}/server/{tag}/{region}", getWorldRunningServer)        // provides server address
		r.Get("/worlds/{id}/server/{tag}/{region}/{mode}", getWorldRunningServer) // provides server address
		r.Get("/worlddrafts", getWorlddrafts)                                     // Returns worlds for given filter

		r.Patch("/worlds/{id}/history", patchWorldScriptHistory)
		r.Patch("/worlds/{id}", patchWorld)            // title, description, mapBase64, script
		r.Patch("/worlds/{id}/likes", patchWorldLikes) // true: add like, false: remove like

		// update world thumbnail
		r.Post("/worlds/{id}/thumbnail", postWorldThumbnail) // TODO: remove once Cubzh app is discontinued
		r.Post("/worlds/{id}/thumbnail.png", postWorldThumbnailV2)

		r.Post("/worlds/{id}/script", postWorldScript)
		r.Post("/worlddrafts", worlddraftsCreate)

		// CREATIONS

		r.Get("/creations", getCreations)

		// SERVERS

		r.Get("/servers", getServers)

		// ITEMS

		r.Get("/items/{idOrFullname}", getItemInfo)
		r.Patch("/items/{id}/likes", patchItemLikes) // true: add like, false: remove like
		// update an item's 3D model
		r.Post("/items/{repo}/{name}/model", postItemModel)
		// POST /itemdrafts | Create a new Itemdraft
		r.Post("/itemdrafts", itemdraftsCreate)
		// PATCH /itemdrafts/:id | Modifies fields of an Itemdraft
		r.Patch("/itemdrafts/{id}", itemdraftsPatch)
		// DELETE /itemdrafts/:id | Delete an Itemdraft
		r.Delete("/itemdrafts/{id}", itemdraftsDelete)

		// Posts a file that will then in most cases be attached to an item (3D model, texture, sound, etc.)
		r.Post("/items/file/{fileType}", postItemFile)
		r.Get("/items/file/{fileName}", getItemFile)

		// KEY-VALUE STORE
		// ex: /kvs/world/{worldID}/store/{store}?key=foo&key=bar
		r.Get("/kvs/world/{worldID}/store/{store}", getKVSValues)
		r.Patch("/kvs/world/{worldID}/store/{store}", patchKVSValues)

		// User push tokens
		r.Post("/users/pushToken", postUserPushToken) // used until 0.1.2 (179) // TODO: remove
		r.Post("/users/pushtoken", postUserPushToken)

		// same as /user-search but doesn't return the user having sent the request
		r.Get("/user-search-others/{searchString}", getUserSearchOthers)

		// Friends relations

		r.Post("/friend-request", createFriendRequest)
		r.Post("/friend-request-cancel", cancelFriendRequest)
		r.Post("/friend-request-reply", replyToFriendRequest)
		// TODO: gaetan: stop using this route and remove it
		// it should be replaced by "getUserFriendRelations" function
		r.Get("/friend-relations", getFriendRelations)

		// IAP

		r.Post("/purchases/verify", verifyPurchase)
	})

	// -------------------------------
	// USER & GAME SERVER AUTHENTICATED ROUTES
	// -------------------------------

	router.Group(func(r chi.Router) {
		r.Use(userAndGameServerAuthMiddleware)

		// AI
		r.Post("/ai/chatcompletions", getChatCompletions)
		r.Post("/ai/imagegenerations", getImageGenerations)
	})

	// -------------------------------
	// EXTRA
	// -------------------------------

	//
	router.Post("/secret", postSecret)

	// -------------------------------
	// ANALYTICS
	// -------------------------------
	router.Get("/tmpUsersWithTag/{tag}", tmpUsersWithTag)

	// -------------------------------
	// Login / logout
	// -------------------------------

	// Returns login options based on provided username or email
	router.Post("/get-login-options", getLoginOptions)
	// ask for magic key to be sent
	router.Post("/get-magic-key", getMagicKey)
	// login (with magic key or password)
	router.Post("/login", login)
	// check if there's a valid magic key, meaning the user recently asked for it
	router.Post("/has-valid-magic-key", hasValidMagicKey)

	// -------------------------------
	// Worlds
	// -------------------------------
	// Returns world's thumbnail (image/png)
	router.Get("/worlds/{id}/thumbnail.png", getWorldThumbnail)
	router.Get("/worlds/{id}/thumbnail-{width}.png", getWorldThumbnail) // remove once build ~215 is forced
	// Add World like
	router.Post("/world-add-like", worldAddLikeDeprecated)
	// Remove World like
	router.Post("/world-remove-like", worldRemoveLikeDeprecated)

	// -------------------------------
	// Blueprints
	// -------------------------------

	// ----------
	// list
	// - filter by category
	// - filter by repo ('caillef')
	// - filter by name (later with regex)

	// Get Blueprints
	// query parameters supported:
	// /blueprints?category=hat&repo=caillef
	// LATER: limit=100 offset=0
	// router.Get("/blueprints", blueprintsList)

	// router.HandleFunc("/blueprints", blueprintPublish)
	// router.HandleFunc("/blueprints/{id}", blueprintGetByID)
	// router.HandleFunc("/blueprints/{repo}/{name}/{version}", blueprintGetByFullname)

	router.NotFound(notFound)

	if secure {
		log.Fatal(http.ListenAndServeTLS(port, certFilePath, keyFilePath, router))
	} else {
		log.Fatal(http.ListenAndServe(port, router))
	}
}

// Returns user for Username or Email
// Answers with proper error and returns nil when user can't be found.
// If nil is returned, caller should return right away in most cases.
func getUserWithUsernameOrEmail(dbClient *dbclient.Client, usernameOrEmail string) (usr *user.User, err error) {
	if dbClient == nil {
		err = errors.New("DB client is nil")
		return
	}

	if user.IsUsernameValid(usernameOrEmail) {
		usr, err = user.GetByUsername(dbClient, usernameOrEmail)
	} else if user.IsEmailValid(usernameOrEmail) {
		usr, err = user.GetByEmail(dbClient, usernameOrEmail)
		if usr == nil && err == nil {
			// user not found, try by parent email
			usr, err = user.GetByParentEmail(dbClient, usernameOrEmail)
		}
	} else {
		err = errInvalidUsernameOrEmail
	}

	return
}

func getUserWithCredentialsAndRespondOnError(credentials *UserCredentials, w http.ResponseWriter) *user.User {

	var u *user.User
	var err error

	dbClient, err := dbclient.SharedUserDB()
	if err != nil {
		respondWithError(http.StatusInternalServerError, "", w)
		return nil
	}

	u, err = user.GetWithIDAndToken(dbClient, credentials.UserIDNew, credentials.Token)
	if err != nil {
		respondWithError(http.StatusInternalServerError, "", w)
		return nil
	}
	if u == nil {
		respondWithError(http.StatusUnauthorized, "", w)
		return nil
	}

	return u
}

func getLoginOptions(w http.ResponseWriter, r *http.Request) {

	dbClient, err := dbclient.SharedUserDB()
	if err != nil {
		respondWithError(http.StatusInternalServerError, "", w)
		return
	}

	var req getLoginOptionReq
	err = json.NewDecoder(r.Body).Decode(&req)
	if err != nil {
		respondWithError(http.StatusBadRequest, "can't decode json request", w)
		return
	}

	usernameOrEmail := user.UsernameOrEmailSanitize(req.GetUsernameOrEmail())

	usr, err := getUserWithUsernameOrEmail(dbClient, usernameOrEmail)
	if err != nil {
		respondWithError(http.StatusInternalServerError, "", w)
		return
	}
	if usr == nil {
		respondWithError(http.StatusNotFound, "", w)
		return
	}

	res, err := NewGetLoginOptionRes(usr)
	if err != nil {
		respondWithError(http.StatusInternalServerError, "", w)
		return
	}

	respondWithJSON(http.StatusOK, res, w)
}

// sends magic key for given username or email
// empty response with 200 status code when it works.
// other status codes can come with error messages
func getMagicKey(w http.ResponseWriter, r *http.Request) {

	var usr *user.User
	var err error

	dbClient, err := dbclient.SharedUserDB()
	if err != nil {
		respondWithError(http.StatusInternalServerError, "", w)
		return
	}

	var req getMagicKeyReq
	err = json.NewDecoder(r.Body).Decode(&req)

	if err != nil {
		respondWithError(http.StatusBadRequest, "can't decode json request", w)
		return
	}

	usernameOrEmail := user.UsernameOrEmailSanitize(req.UsernameOrEmail)

	usr, err = getUserWithUsernameOrEmail(dbClient, usernameOrEmail)
	if err != nil {
		respondWithError(http.StatusInternalServerError, "", w)
		return
	}
	if usr == nil {
		respondWithError(http.StatusNotFound, "", w)
		return
	}

	// Decide what delivery method to use for the magic key
	deliveryMethod := ""
	if usr.Email != "" {
		deliveryMethod = "email"
	} else if usr.ParentEmail != "" {
		deliveryMethod = "parentEmail"
	} else if usr.Phone != nil && *usr.Phone != "" {
		deliveryMethod = "phone"
	} else if usr.ParentPhone != nil && *usr.ParentPhone != "" {
		deliveryMethod = "parentPhone"
	}
	if deliveryMethod == "" {
		respondWithError(http.StatusInternalServerError, "no delivery method available", w)
		return
	}

	clientIP := httpUtils.GetIP(r)

	if deliveryMethod == "email" || deliveryMethod == "parentEmail" {
		emailAddressToUse := usr.GetEmailAddressToUse()
		if emailAddressToUse == "" {
			// can't send link, no email.
			respondWithError(http.StatusBadRequest, "", w)
			return
		}

		err = usr.GenerateMagicLink(dbClient)
		if err != nil {
			respondWithError(http.StatusInternalServerError, "", w)
			return
		}

		var content struct {
			Email           string
			Username        string
			Key             string
			SourceIPAddress string
		}

		content.Key = usr.MagicKey
		content.Username = usr.Username
		content.Email = emailAddressToUse
		content.SourceIPAddress = clientIP

		err = mailer.Send(
			emailAddressToUse,
			"", // email address used as name, because we don't have any name
			"./templates/email_generic.html",
			"./templates/email_magicKey.md",
			MAILJET_API_KEY,
			MAILJET_API_SECRET,
			&content,
			"particubes-email-confirm",
		)
		if err != nil {
			fmt.Println("❌ [getMagicKey] mailer.Send error:", err.Error())
			respondWithError(http.StatusInternalServerError, "", w)
			return
		}
	} else if deliveryMethod == "phone" || deliveryMethod == "parentPhone" {
		// send verification code to phone
		phoneNumber, err := usr.GetPhoneNumberToUse()
		if err != nil {
			respondWithError(http.StatusInternalServerError, "user has no phone number", w)
			return
		}

		s := dinggolang.New(dinggolang.WithSecurity(DING_API_KEY))
		isReturning := true // bypass security verifications
		// https://github.com/ding-live/ding-golang/blob/main/models/components/createauthenticationrequest.go
		ctx := context.Background()
		res, err := s.Otp.CreateAuthentication(ctx, &dingcomponents.CreateAuthenticationRequest{
			CustomerUUID:    DING_API_CUSTOMER_UUID,
			PhoneNumber:     phoneNumber,
			IP:              &clientIP,
			IsReturningUser: &isReturning,
		})

		// handle error
		if err != nil {
			fmt.Println("dinggolang error:", err.Error())
			respondWithError(http.StatusInternalServerError, err.Error(), w)
			return
		}

		// handle response
		if res.CreateAuthenticationResponse == nil {
			fmt.Println("dinggolang error: CreateAuthenticationResponse is nil")
			respondWithError(http.StatusInternalServerError, "failed to send phone verif code", w)
			return
		}

		authUUID := res.CreateAuthenticationResponse.AuthenticationUUID
		if authUUID == nil || *authUUID == "" {
			fmt.Println("dinggolang error: authUUID is empty")
			respondWithError(http.StatusInternalServerError, "failed to get auth UUID", w)
			return
		}

		usr.MagicKey = *authUUID
		err = usr.UpdateAndSaveMagicKey(dbClient, time.Minute*30)
		if err != nil {
			fmt.Println("failed to update user:", err.Error())
			respondWithError(http.StatusInternalServerError, "failed to update user", w)
			return
		}
	}

	respondOK(w)
}

func checkPhoneCode(usr *user.User, code string) error {
	fmt.Println("🐞 [checkPhoneCode] code:", code, "authID", usr.MagicKey)
	// verif code provided, update phone number
	s := dinggolang.New(dinggolang.WithSecurity(DING_API_KEY))
	var request *dingcomponents.CreateCheckRequest = &dingcomponents.CreateCheckRequest{
		CustomerUUID:       DING_API_CUSTOMER_UUID,
		AuthenticationUUID: usr.MagicKey,
		CheckCode:          code,
	}
	ctx := context.Background()
	res, err := s.Otp.Check(ctx, request)
	if err != nil {
		return err
	}
	// handle response
	if res.CreateCheckResponse == nil {
		return errors.New("CreateCheckResponse is nil")
	}
	if res.CreateCheckResponse.Status == nil || *res.CreateCheckResponse.Status != dingcomponents.CheckStatusValid {
		return errors.New("wrong phone verif code")
	}
	return nil
}

func login(w http.ResponseWriter, r *http.Request) {

	dbClient, err := dbclient.SharedUserDB()
	if err != nil {
		respondWithError(http.StatusInternalServerError, "", w)
		return
	}

	var req loginReq
	err = json.NewDecoder(r.Body).Decode(&req)
	if err != nil {
		fmt.Println("❌ [login] can't decode json request:", err.Error())
		respondWithError(http.StatusBadRequest, "can't decode json request", w)
		return
	}

	// determine if it is a passkey login
	usePasskeyLogin := req.PasskeyCredentialIDBase64 != "" &&
		req.PasskeyAuthenticatorDataBase64 != "" &&
		req.PasskeyRawClientDataJSONString != "" &&
		req.PasskeySignatureBase64 != "" &&
		req.PasskeyUserIDString != ""

	// get user trying to login
	var usr *user.User
	if usePasskeyLogin {
		// get user from database
		usr, err = user.GetByID(dbClient, req.PasskeyUserIDString)
		if err != nil {
			fmt.Println("❌ [login] getUserWithUsernameOrEmail error (1):", err.Error())
			// TODO: add granularity on error messages
			respondWithError(http.StatusInternalServerError, "", w)
			return
		}
	} else { // not using passkey login
		// get username or email from request
		usernameOrEmail := user.UsernameOrEmailSanitize(req.UsernameOrEmail)
		// get user from database
		usr, err = getUserWithUsernameOrEmail(dbClient, usernameOrEmail)
		if err != nil {
			fmt.Println("❌ [login] getUserWithUsernameOrEmail error (2):", err.Error())
			// TODO: add granularity on error messages
			respondWithError(http.StatusInternalServerError, "", w)
			return
		}
	}
	if usr == nil {
		respondWithError(http.StatusNotFound, "", w)
		return
	}

	if usePasskeyLogin {

		// decode JSON object
		var clientData PasskeyClientData
		err = json.Unmarshal([]byte(req.PasskeyRawClientDataJSONString), &clientData)
		if err != nil {
			respondWithError(http.StatusBadRequest, "", w)
			return
		}

		// retrieve challenge from the server-side session
		challengeStr, err := getSessionFieldPasskeyChallenge(r)
		if err != nil {
			fmt.Println("❌ [login] failed to get challenge from session:", err)
			respondWithError(http.StatusBadRequest, "", w)
			return
		}

		// check the client data validity
		{
			// check that the request has the correct type
			if clientData.Type != "webauthn.get" {
				fmt.Println("❌ [login] invalid client data type:", clientData.Type)
				respondWithError(http.StatusBadRequest, "", w)
				return
			}

			// check that the challenge value is correct
			expectedChallengeBase64 := base64.RawURLEncoding.EncodeToString([]byte(challengeStr))
			if clientData.Challenge != expectedChallengeBase64 {
				fmt.Println("❌ [login] invalid client data challenge")
				respondWithError(http.StatusBadRequest, "", w)
				return
			}

			// check the origin value is valid
			// iOS:
			// - origin: https://blip.game
			// Android:
			// - origin: android:apk-key-hash:gp5fbrMM50ugOjFrMmzrq6GVM2srhZhkfMFeNkG9gE4
			// - androidPackageName: com.voxowl.blip
			if !slices.Contains(allowedOrigins, clientData.Origin) {
				fmt.Println("❌ [login] invalid client data origin:", clientData.Origin)
				respondWithError(http.StatusBadRequest, "", w)
				return
			}
		}

		// check credential ID matches with user account
		if usr.PasskeyCredentialID != req.PasskeyCredentialIDBase64 {
			fmt.Println("❌ [login] credential ID mismatch")
			respondWithError(http.StatusBadRequest, "", w)
			return
		}

		passkeyAuthenticatorData, err := base64.RawURLEncoding.DecodeString(req.PasskeyAuthenticatorDataBase64)
		if err != nil {
			fmt.Println("❌ [login] failed to decode passkeyAuthenticatorData:", err)
			respondWithError(http.StatusBadRequest, "", w)
			return
		}

		passkeySignature, err := base64.RawURLEncoding.DecodeString(req.PasskeySignatureBase64)
		if err != nil {
			fmt.Println("❌ [login] failed to decode passkeySignature:", err)
			respondWithError(http.StatusBadRequest, "", w)
			return
		}

		// 4. Verify RP ID hash
		RPIDHashReceived := passkeyAuthenticatorData[:32]
		expectedRPIDHash := sha256.Sum256([]byte(PASSKEY_RP_ID))
		if !bytes.Equal(RPIDHashReceived, expectedRPIDHash[:]) {
			fmt.Println("❌ [login] RP ID hash mismatch")
			respondWithError(http.StatusBadRequest, "", w)
			return
		}

		// get public key
		pemBytes := usr.PasskeyPublicKey
		if pemBytes == "" {
			fmt.Println("❌ [login] passkeyCredentialPEM is empty")
			respondWithError(http.StatusBadRequest, "", w)
			return
		}

		// Decode PEM representation into ECDSA public key
		block, _ := pem.Decode([]byte(pemBytes))
		if block == nil {
			fmt.Println("❌ [login] failed to decode PEM block containing public key")
			respondWithError(http.StatusBadRequest, "", w)
			return
		}

		// Parse the public key
		publicKey, err := x509.ParsePKIXPublicKey(block.Bytes)
		if err != nil {
			fmt.Println("❌ [login] failed to parse public key:", err)
			respondWithError(http.StatusInternalServerError, "", w)
			return
		}

		// Ensure it's an ECDSA public key
		ecdsaPublicKey, ok := publicKey.(*ecdsa.PublicKey)
		if !ok {
			fmt.Println("❌ [login] public key is not an ECDSA public key")
			respondWithError(http.StatusInternalServerError, "", w)
			return
		}

		// 5. Build signed data = authData || SHA256(clientDataJSON)
		clientDataHash := sha256.Sum256([]byte(req.PasskeyRawClientDataJSONString))
		signedData := append(passkeyAuthenticatorData, clientDataHash[:]...)

		// 6. Verify signature
		hashedSignedData := sha256.Sum256(signedData)
		if !ecdsa.VerifyASN1(ecdsaPublicKey, hashedSignedData[:], passkeySignature) {
			fmt.Println("❌ [login] signature invalid")
			respondWithError(http.StatusBadRequest, "", w)
			return
		}

		// TODO: gaetan: check if signCount is valid:

		// 7. Verify signCount
		// newSignCount := uint32(passkeyAuthenticatorData[33])<<24 |
		// 	uint32(passkeyAuthenticatorData[34])<<16 |
		// 	uint32(passkeyAuthenticatorData[35])<<8 |
		// 	uint32(passkeyAuthenticatorData[36])

		// fmt.Println("🐞 [login] newSignCount:", newSignCount)
		// fmt.Println("🐞 [login] usr.PasskeySignCount:", usr.PasskeySignCount)
		// if newSignCount != 0 && newSignCount <= uint32(usr.PasskeySignCount) {
		// 	respondWithError(http.StatusBadRequest, "", w)
		// 	return
		// }

		// 8. Update signCount in DB and return success
		// usr.PasskeySignCount = int(newSignCount)
		// updateSignCountInDB(credID, newSignCount)
		// err = usr.Save(dbClient)
		// if err != nil {
		// 	respondWithError(http.StatusInternalServerError, "", w)
		// 	return
		// }

		// w.WriteHeader(http.StatusOK)
		// fmt.Fprintln(w, "Authentication verified")

		// success, let's continue...

	} else if req.Password != "" {
		if usr.CheckPassword(string(req.Password)) == false { // wrong password
			respondWithError(http.StatusBadRequest, "", w)
			return
		}
	} else if req.MagicKey != "" {
		magicKey := user.SanitizeMagicKey(req.MagicKey)
		if usr.GetEmailAddressToUse() != "" {
			if usr.CheckMagicKey(magicKey) == false { // wrong or expired magic key
				respondWithError(http.StatusBadRequest, "", w)
				return
			}
		} else if _, err := usr.GetPhoneNumberToUse(); err == nil {
			err = checkPhoneCode(usr, magicKey)
			if err != nil {
				fmt.Println("❌ [login] ERR:", err.Error())
				respondWithError(http.StatusBadRequest, "", w)
				return
			}
		} else {
			fmt.Println("❌ [login] unexpected state")
			respondWithError(http.StatusBadRequest, "", w)
			return
		}
		// remove magic key
		usr.MagicKey = ""
		usr.MagicKeyExpirationDate = nil
	} else {
		respondWithError(http.StatusBadRequest, "", w)
		return
	}

	// Update login tokens
	{
		// Generate new token for user, if there's none.
		if usr.Token == "" {
			tk, err := utils.UUID()
			if err != nil {
				respondWithError(http.StatusInternalServerError, "", w)
				return
			}
			usr.Token = tk
		}

		err = usr.Save(dbClient)
		if err != nil {
			respondWithError(http.StatusInternalServerError, "", w)
			return
		}
	}

	isUnder13 := false
	if usr.Dob != nil {
		isUnder13 = under13(usr.Dob)
	} else if usr.EstimatedDob != nil {
		isUnder13 = under13(usr.EstimatedDob)
	}

	res := &loginRes{
		Username:      usr.Username,
		Credentials:   NewUserCredentials(usr.ID, usr.Token),
		HasUsername:   usr.Username != "",
		HasDOB:        usr.Dob != nil || usr.EstimatedDob != nil,
		HasPassword:   usr.Password != "",
		HasEmail:      usr.GetEmailAddressToUse() != "",
		Under13:       isUnder13,
		AvatarHead:    usr.AvatarHead,
		AvatarBody:    usr.AvatarBody,
		AvatarLArm:    usr.AvatarLArm,
		AvatarRArm:    usr.AvatarRArm,
		AvatarLHand:   usr.AvatarLHand,
		AvatarRHand:   usr.AvatarRHand,
		AvatarLLeg:    usr.AvatarLLeg,
		AvatarRLeg:    usr.AvatarRLeg,
		AvatarLFoot:   usr.AvatarLFoot,
		AvatarRFoot:   usr.AvatarRFoot,
		AvatarHair:    usr.AvatarHair,
		AvatarTop:     usr.AvatarTop,
		AvatarLSleeve: usr.AvatarLSleeve,
		AvatarRSleeve: usr.AvatarRSleeve,
		AvatarLGlove:  usr.AvatarLGlove,
		AvatarRGlove:  usr.AvatarRGlove,
		AvatarLPant:   usr.AvatarLPant,
		AvatarRPant:   usr.AvatarRPant,
		AvatarLBoot:   usr.AvatarLBoot,
		AvatarRBoot:   usr.AvatarRBoot,
	}

	respondWithJSON(http.StatusOK, &res, w)
}

func userVerifyEmail(w http.ResponseWriter, r *http.Request) {

	dbClient, err := dbclient.SharedUserDB()
	if err != nil {
		respondWithError(http.StatusInternalServerError, "internal server error", w)
		return
	}

	// retrieve path parameters
	userID := chi.URLParam(r, "id")

	// retrive query parameter : confirmation code
	queryParams := r.URL.Query()
	code := queryParams.Get("code")

	// get user
	usr, err := user.GetByID(dbClient, userID)
	if err != nil || usr == nil {
		respondWithError(http.StatusBadRequest, "bad request", w)
		return
	}

	if usr.EmailVerifCode == nil && usr.ParentEmailVerifCode == nil {
		respondWithError(http.StatusBadRequest, "bad request", w)
		return
	}

	if usr.EmailVerifCode != nil && *usr.EmailVerifCode == code && usr.EmailTemporary != nil {
		usr.Email = *usr.EmailTemporary
		usr.EmailTemporary = nil
		usr.EmailVerifCode = nil

	} else if usr.ParentEmailVerifCode != nil && *usr.ParentEmailVerifCode == code && usr.ParentEmailTemporary != nil {
		usr.ParentEmail = *usr.ParentEmailTemporary
		usr.ParentEmailTemporary = nil
		usr.ParentEmailVerifCode = nil

	} else {
		respondWithHTML(http.StatusOK, "❌ Error: wrong email verification code.", w)
		return
	}

	err = usr.Save(dbClient)
	if err != nil {
		respondWithError(http.StatusInternalServerError, "internal server error", w)
		return
	}

	// respondWithHTML(http.StatusOK, "Your email has been verified! ✅", w)
	content := map[string]interface{}{
		"HTML": "Your email has been verified! ✅",
	}
	respondWithHTMLTemplate(http.StatusOK, "./templates/page_emailVerified.html", content, w)
}

func checkUsernameV2(w http.ResponseWriter, r *http.Request) {

	type checkUsernameReq struct {
		Username string `json:"username,omitempty"`
	}

	type checkUsernameRes struct {
		Format      bool   `json:"format"`
		Available   bool   `json:"available"`
		Appropriate bool   `json:"appropriate"`
		Key         string `json:"key,omitempty"`
	}

	var req checkUsernameReq
	err := json.NewDecoder(r.Body).Decode(&req)
	if err != nil {
		respondWithError(http.StatusBadRequest, "can't decode json request", w)
		return
	}

	res := &checkUsernameRes{
		Format:      false,
		Available:   false,
		Appropriate: true,
		Key:         "",
	}

	if user.IsUsernameValid(req.Username) == false {

		// trackEventForHTTPReq(&eventReq{Type: "CHECK_USERNAME"}, r)
		respondWithJSON(http.StatusOK, res, w)
		return
	}

	res.Format = true

	sanitizedUsername, err := user.SanitizeUsername(req.Username)
	if err != nil {
		respondWithError(http.StatusInternalServerError, "", w)
		return
	}

	dbClient, err := dbclient.SharedUserDB()
	if err != nil {
		respondWithError(http.StatusInternalServerError, "", w)
		return
	}

	available, err := user.IsUsernameAvailable(dbClient, sanitizedUsername)
	if err != nil {
		respondWithError(http.StatusInternalServerError, "", w)
		return
	}
	res.Available = available

	if available == true {
		// Check if username is appropriate in ScyllaDB cluster
		record, err := scyllaClientModeration.GetModerationUsername(sanitizedUsername)
		if err != nil {
			// If not in ScyllaDB cluster, moderate using AI service
			appropriate, err := moderation.ModerateUsername(sanitizedUsername, moderation.Opts{Timeout: 2 * time.Second})
			if err != nil {
				// on error, we consider the username appropriate
				res.Appropriate = true
			} else {
				res.Appropriate = appropriate
				// update moderation cache
				err := scyllaClientModeration.SetModerationUsername(sanitizedUsername, appropriate)
				if err != nil {
					fmt.Println("❌[ScyllaDB] failed to store username moderation:", err.Error())
				}
			}
		} else {
			res.Appropriate = record.Appropriate
		}
	}

	// -------------------------------------------------
	// TEMPORARY HACK : always allow usernames
	// -------------------------------------------------
	res.Appropriate = true
	// -------------------------------------------------
	// TEMPORARY HACK : always allow usernames
	// -------------------------------------------------

	if res.Format && res.Available && res.Appropriate {
		res.Key = utils.SaltedMD5ed(sanitizedUsername, USERNAME_CHECK_SALT)
	}

	respondWithJSON(http.StatusOK, res, w)
}

func checkPhoneNumber(w http.ResponseWriter, r *http.Request) {
	type checkPhoneNumberReq struct {
		PhoneNumber string `json:"phoneNumber,omitempty"`
	}

	type checkPhoneNumberRes struct {
		IsValid              bool   `json:"isValid"`
		FormattedPhoneNumber string `json:"formattedNumber,omitempty"`
	}

	// Parse request
	var req checkPhoneNumberReq
	err := json.NewDecoder(r.Body).Decode(&req)
	if err != nil {
		respondWithError(http.StatusBadRequest, "unable to decode request", w)
		return
	}
	phoneNumber := req.PhoneNumber

	var isValid bool = false
	var formattedPhoneNumber string = ""

	if phoneNumber == TEST_PHONE_NUMBER {
		isValid = true
		formattedPhoneNumber = TEST_PHONE_NUMBER
	} else {
		// Run checks on phone number received and prepare response
		isValid, formattedPhoneNumber, err = CheckPhoneNumber(phoneNumber)
	}

	if err != nil {
		respondWithError(http.StatusInternalServerError, err.Error(), w)
		return
	}

	// fmt.Println("🐞[CheckPhone]", isValid, "\""+formattedPhoneNumber+"\"", err)

	res := checkPhoneNumberRes{
		IsValid:              isValid,
		FormattedPhoneNumber: formattedPhoneNumber,
	}
	respondWithJSON(http.StatusOK, res, w)
}

// createUser creates a new user account
func createUser(w http.ResponseWriter, r *http.Request) {

	type createUserReq struct {
		DOB      string `json:"dob,omitempty"`      // optional
		Username string `json:"username,omitempty"` // optional
		Key      string `json:"key,omitempty"`      // optional
		Password string `json:"password,omitempty"` // optional
	}

	type createUserRes struct {
		Credentials *UserCredentials `json:"credentials,omitempty"`
	}

	dbClient, err := dbclient.SharedUserDB()
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

	// decode request
	var req createUserReq
	err = json.NewDecoder(r.Body).Decode(&req)
	if err != nil {
		respondWithError(http.StatusBadRequest, "can't decode json request", w)
		return
	}

	// Validate request

	// If password is provided, it must be at least 8 characters long
	if req.Password != "" && len(req.Password) < 8 {
		respondWithError(http.StatusBadRequest, "password is too short", w)
		return
	}

	sanitizedUsername := ""
	if req.Username != "" {
		sanitizedUsername, err = user.SanitizeUsername(req.Username)
		if err != nil {
			respondWithError(http.StatusBadRequest, "invalid username", w)
			return
		}

		// if username is provided, it must be accompanied by a matching key
		key := utils.SaltedMD5ed(sanitizedUsername, USERNAME_CHECK_SALT)
		if key != req.Key {
			respondWithError(http.StatusBadRequest, "wrong key", w)
			return
		}

		// check if username is available
		available, err := user.IsUsernameAvailable(dbClient, sanitizedUsername)
		if err != nil {
			respondWithError(http.StatusInternalServerError, "", w)
			return
		}
		if available == false {
			respondWithError(http.StatusBadRequest, "username not available", w)
			return
		}
	}

	// Date of birth is not required right away. (it's optional)
	// It will be set later, when the user will have completed the profile.

	// dob is in the format mm-dd-yyyy
	var dobTime time.Time

	if req.DOB != "" {
		// parse the date of birth
		dobTime, err = time.Parse("01-02-2006", req.DOB)
		if err != nil {
			respondWithError(http.StatusBadRequest, "", w)
			return
		}

		// check the date of birth is not in the future
		if dobTime.After(time.Now()) {
			respondWithError(http.StatusBadRequest, "date of birth is in the future", w)
			return
		}
	}

	// create a new user
	usr, err := user.New()
	if err != nil {
		respondWithError(http.StatusInternalServerError, "failed to create user", w)
		return
	}

	// set date of birth
	usr.Dob = &dobTime

	// set username
	usr.Username = sanitizedUsername

	// set password
	if req.Password != "" {
		usr.SetPassword(req.Password)
	}

	// set client version
	usr.CreatedWithClientVersion = clientInfo.Version

	// set client build number
	usr.CreatedWithClientBuildNumber = &clientInfo.BuildNumber

	// save the user
	err = usr.Save(dbClient)
	if err != nil {
		respondWithError(http.StatusInternalServerError, "failed to save user", w)
		return
	}

	res := &createUserRes{
		Credentials: NewUserCredentials(usr.ID, usr.Token),
	}

	respondWithJSON(http.StatusOK, res, w)
}

func hasValidMagicKey(w http.ResponseWriter, r *http.Request) {

	type hasValidMagicKeyReq struct {
		UsernameOrEmail string `json:"username-or-email,omitempty"`
	}

	type hasValidMagicKeyRes struct {
		HasValidMagicKey bool `json:"has-valid-magic-key,omitempty"`
	}

	var usr *user.User
	var err error

	var req hasValidMagicKeyReq
	err = json.NewDecoder(r.Body).Decode(&req)

	if err != nil {
		respondWithError(http.StatusBadRequest, "can't decode json request", w)
		return
	}

	usernameOrEmail := user.UsernameOrEmailSanitize(req.UsernameOrEmail)

	dbClient, err := dbclient.SharedUserDB()
	if err != nil {
		respondWithError(http.StatusInternalServerError, "", w)
		return
	}

	usr, err = getUserWithUsernameOrEmail(dbClient, usernameOrEmail)
	if err != nil {
		respondWithError(http.StatusInternalServerError, "", w)
		return
	}
	if usr == nil {
		respondWithError(http.StatusNotFound, "", w)
		return
	}

	res := &hasValidMagicKeyRes{
		HasValidMagicKey: false,
	}

	if usr.IsMagicKeyValid() {
		res.HasValidMagicKey = true
	}

	respondWithJSON(http.StatusOK, &res, w)
}

// ------------
// WORLDS
// ------------

// worlddraftsCreate creates a new unpublished world
func worlddraftsCreate(w http.ResponseWriter, r *http.Request) {

	dbClient, err := dbclient.SharedUserDB()
	if err != nil {
		respondWithError(http.StatusInternalServerError, "", w)
		return
	}

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

	type worlddraftCreateReq struct {
		Title      string `json:"title"`
		MaxPlayers int32  `json:"maxPlayers,omitempty"`
	}

	var req worlddraftCreateReq
	err = json.NewDecoder(r.Body).Decode(&req)
	if err != nil {
		respondWithError(http.StatusBadRequest, "can't decode json request", w)
		return
	}

	// Request validation
	if req.Title == "" {
		respondWithError(http.StatusBadRequest, "world title is invalid", w)
		return
	}
	regex, err := regexp.Compile(WORLD_TITLE_REGEX)
	if err != nil {
		respondWithError(http.StatusInternalServerError, "failed to compile regexp", w)
		return
	}
	if regex.MatchString(req.Title) == false {
		respondWithError(http.StatusBadRequest, "world title is invalid", w)
		return
	}

	if req.MaxPlayers < WORLD_MAX_PLAYERS_LIMIT_LOW {
		req.MaxPlayers = WORLD_MAX_PLAYERS_LIMIT_LOW
	}
	if req.MaxPlayers > WORLD_MAX_PLAYERS_LIMIT_HIGH {
		req.MaxPlayers = WORLD_MAX_PLAYERS_LIMIT_HIGH
	}

	g, err := game.New()
	if err != nil {
		respondWithError(http.StatusInternalServerError, "", w)
		return
	}

	g.Author = usr.ID
	g.AuthorName = usr.Username
	g.Title = req.Title
	g.Description = ""
	g.Tags = make([]string, 0)
	g.MaxPlayers = int32(req.MaxPlayers)

	err = g.Save(dbClient, &clientInfo.BuildNumber)
	if err != nil {
		respondWithError(http.StatusInternalServerError, "", w)
		return
	}

	g.IsAuthor = g.Author == usr.ID

	// Update search engine data
	// ------------------------------------------------
	{
		err = searchEngineUpsertWorld(g)
		if err != nil {
			fmt.Println("❌ PATCH worlddraft create: failed to update search engine data:", err.Error())
		}
	}

	respondWithJSON(http.StatusOK, g, w)
}

// NOTE: currently, worldList only returns worlds created by caller
func getWorlddrafts(w http.ResponseWriter, r *http.Request) {

	// retrieve user sending the request
	usr, err := getUserFromRequestContext(r)
	if err != nil {
		respondWithError(http.StatusInternalServerError, "", w)
		return
	}

	type worldListRes struct {
		Worlds       []*game.Game `json:"worlds"`
		TotalResults int          `json:"totalResults"`
		Page         int          `json:"page"`
		PerPage      int          `json:"perPage"`
	}

	// Query parameters
	var searchStr string = kSearchDefaultText
	// var page int = kSearchDefaultPage
	// var perPage int = kSearchDefaultPerPage
	// var categories []string = kSearchDefaultCategories
	{
		searchStr = r.URL.Query().Get("search")
		// {
		// 	pageStr := r.URL.Query().Get("page")
		// 	if pageStr != "" {
		// 		i, err := strconv.ParseInt(pageStr, 10, 32)
		// 		if err != nil {
		// 			respondWithError(http.StatusBadRequest, "query parameter 'page' is not valid", w)
		// 			return
		// 		}
		// 		page = int(i)
		// 	}
		// }
		// {
		// 	perPageStr := r.URL.Query().Get("perpage") // TODO: deprecate this & only use camel case
		// 	if perPageStr == "" {
		// 		perPageStr = r.URL.Query().Get("perPage") // not used yet (as of 0.0.49 #95)
		// 	}
		// 	if perPageStr != "" {
		// 		i, err := strconv.ParseInt(perPageStr, 10, 32)
		// 		if err != nil {
		// 			respondWithError(http.StatusBadRequest, "query parameter 'perpage' is not valid", w)
		// 			return
		// 		}
		// 		perPage = int(i)
		// 	}
		// }
		// {
		// 	categories = r.URL.Query()["category"]
		// }
	}

	// enforce "list:mine" filter
	searchStr = "list:mine " + searchStr

	// authenticate the request sender
	dbClient, err := dbclient.SharedUserDB()
	if err != nil {
		respondWithError(http.StatusInternalServerError, "", w)
		return
	}

	games, err, mineFilterActive := game.List(dbClient, searchStr, usr.ID)
	if err != nil {
		respondWithError(http.StatusInternalServerError, "", w)
		return
	}

	res := &worldListRes{
		Worlds:       make([]*game.Game, 0),
		TotalResults: 0,
		Page:         0,
		PerPage:      0,
	}

	for _, game := range games {
		// for now, we ignore games having content warnings
		// EXCEPT if a user is listing his own games!
		if len(game.ContentWarnings) == 0 || mineFilterActive == true {
			res.TotalResults += 1
			// NOTE: see if we want to remove some fields from game here
			game.IsAuthor = game.Author == usr.ID
			res.Worlds = append(res.Worlds, game)
		}
	}

	// no pagination yet
	res.Page = 1
	res.PerPage = res.TotalResults

	respondWithJSON(http.StatusOK, res, w)
}

func getWorldScriptHistory(w http.ResponseWriter, r *http.Request) {
	// retrieve world ID from URL
	worldID := chi.URLParam(r, "id")

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

	// get world info from database
	world, err := game.GetByID(dbClient, worldID, usr.ID, &clientInfo.BuildNumber)
	if err != nil {
		respondWithError(http.StatusInternalServerError, "", w)
		return
	}

	if world == nil {
		fmt.Println("❌ getWorldScriptHistory: world is nil")
	}

	if usr == nil {
		fmt.Println("❌ getWorldScriptHistory: user is nil")
	}

	// make sure the world is owned by the user
	if world.Author != usr.ID {
		respondWithError(http.StatusForbidden, "", w)
		return
	}

	commits, err := worldUtilsGetScriptHistory(worldID)
	if err != nil {
		respondWithError(http.StatusInternalServerError, "", w)
		return
	}

	respondWithJSON(http.StatusOK, commits, w)
}

func patchWorldScriptHistory(w http.ResponseWriter, r *http.Request) {

	type patchWorldScriptHistoryReq struct {
		CommitHash string `json:"hash"`
	}

	type patchWorldScriptHistoryResp struct {
		WorldID    string `json:"worldID"`
		CommitHash string `json:"hash"`
	}

	worldID := chi.URLParam(r, "id")

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

	// get world info from database
	world, err := game.GetByID(dbClient, worldID, usr.ID, &clientInfo.BuildNumber)
	if err != nil {
		respondWithError(http.StatusInternalServerError, "", w)
		return
	}

	// make sure the world is owned by the user
	if world.Author != usr.ID {
		respondWithError(http.StatusForbidden, "", w)
		return
	}

	// Rollback to commit provided in request body
	var req patchWorldScriptHistoryReq
	err = json.NewDecoder(r.Body).Decode(&req)
	if err != nil {
		respondWithError(http.StatusBadRequest, "can't decode json request", w)
		return
	}

	err = worldUtilsRollbackToCommit(worldID, req.CommitHash)
	if err != nil {
		respondWithError(http.StatusInternalServerError, "", w)
		return
	}

	resp := patchWorldScriptHistoryResp{
		WorldID:    worldID,
		CommitHash: req.CommitHash,
	}

	respondWithJSON(http.StatusOK, resp, w)
}

// Supported query params:
// - width: int, max value is 800 (0 means no resizing)
func getWorldThumbnail(w http.ResponseWriter, r *http.Request) {
	const (
		URL_PARAM_WORLD_ID  = "id"
		URL_PARAM_WIDTH     = "width"
		THUMBNAIL_MAX_WIDTH = 800 // max value for width query param
	)

	var err error
	var worldID string
	var width = 0

	// retrieve world ID value from URL
	worldID = chi.URLParam(r, URL_PARAM_WORLD_ID)

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

	// path to the thumbnail file
	path := filepath.Join(WORLDS_STORAGE_PATH, worldID, "thumbnail.png")

	fileStat, err := os.Stat(path)
	if os.IsNotExist(err) {
		respondWithError(http.StatusNotFound, "file not found", w)
		return
	}

	pngFile, err := os.Open(path)
	if err != nil {
		respondWithError(http.StatusInternalServerError, "", w)
		return
	}
	defer pngFile.Close()

	var pngOutputReader io.Reader = pngFile
	var contentLength int = int(fileStat.Size())

	if width > 0 {

		// create a buffer to store the resized image
		resizedBuf := bytes.Buffer{}

		err = resizePNGImage(pngFile, &resizedBuf, uint(width))
		if err != nil {
			respondWithError(http.StatusInternalServerError, "", w)
			return
		}

		pngOutputReader = &resizedBuf
		contentLength = resizedBuf.Len()
	}

	w.Header().Set("Content-Type", "image/png")
	w.Header().Set("Content-Length", strconv.Itoa(contentLength))
	w.Header().Set("Cache-Control", "max-age="+CACHE_THUMBNAILS_MAX_AGE)
	w.WriteHeader(http.StatusOK)

	// can't change the header afterwards if copy fails
	// no error handling here for now.
	_, err = io.Copy(w, pngOutputReader)
	if err != nil {
		fmt.Println("❌ error while copying world thumbnail to response:", err.Error())
	}
}

func getChatCompletions(w http.ResponseWriter, r *http.Request) {
	client := &http.Client{}
	req, err := http.NewRequest("POST", "https://api.openai.com/v1/chat/completions", r.Body)
	if err != nil {
		respondWithError(http.StatusInternalServerError, err.Error(), w)
		return
	}

	req.Header.Set("Content-Type", "application/json")
	req.Header.Set("Authorization", "Bearer "+OPENAI_API_KEY)
	req.Header.Set("OpenAI-Organization", "org-XTefQQRpqPYx78mphyu4dkwa")
	req.Header.Set("OpenAI-Project", OPENAI_API_PROJECT)

	resp, err := client.Do(req)
	if err != nil {
		respondWithError(http.StatusInternalServerError, err.Error(), w)
		return
	}
	defer resp.Body.Close()

	body, err := io.ReadAll(resp.Body)
	if err != nil {
		respondWithError(http.StatusInternalServerError, err.Error(), w)
		return
	}

	respondWithRawJSON(http.StatusOK, body, w)
}

func getImageGenerations(w http.ResponseWriter, r *http.Request) {
	type imageGenerationBody struct {
		Size     int    `json:"size"`
		Prompt   string `json:"prompt"`
		Output   string `json:"output"`
		Pixelart bool   `json:"pixelart"`
		AsURL    bool   `json:"asURL"`
	}

	client := &http.Client{}

	// Retrieve parameters
	body, err := io.ReadAll(r.Body)
	if err != nil {
		respondWithError(http.StatusInternalServerError, err.Error(), w)
		return
	}

	var inputBody imageGenerationBody
	err = json.Unmarshal(body, &inputBody)
	if err != nil {
		respondWithError(http.StatusInternalServerError, err.Error(), w)
		return
	}

	type asURLResponse struct {
		Url string `json:"url"`
	}

	if inputBody.Output == "Quad" {
		if inputBody.Pixelart {
			inputBody.Prompt = "pixel art " + inputBody.Prompt + ", inventory item, single item, white background"
		}

		type imageGenerationOpenAIBody struct {
			Size   string `json:"size"`
			Prompt string `json:"prompt"`
			N      int    `json:"n"`
		}

		if inputBody.Size != 256 && inputBody.Size != 512 && inputBody.Size != 1024 {
			respondWithError(http.StatusBadRequest, "Error: size must be 256, 512 or 1024.", w)
			return
		}

		openAIBody := imageGenerationOpenAIBody{
			Prompt: inputBody.Prompt,
			Size:   fmt.Sprintf("%dx%d", inputBody.Size, inputBody.Size),
			N:      1,
		}

		// marshall data to json (like json_encode)
		b, _ := json.Marshal(openAIBody)
		req, err := http.NewRequest("POST", "https://api.openai.com/v1/images/generations", bytes.NewReader(b))
		if err != nil {
			respondWithError(http.StatusInternalServerError, err.Error(), w)
			return
		}

		req.Header.Set("Content-Type", "application/json")
		req.Header.Set("Authorization", "Bearer "+OPENAI_API_KEY)
		req.Header.Set("OpenAI-Organization", "org-XTefQQRpqPYx78mphyu4dkwa")
		req.Header.Set("OpenAI-Project", OPENAI_API_PROJECT)

		resp, err := client.Do(req)
		if err != nil {
			respondWithError(http.StatusInternalServerError, err.Error(), w)
			return
		}
		defer resp.Body.Close()

		body, err := io.ReadAll(resp.Body)
		if err != nil {
			respondWithError(http.StatusInternalServerError, err.Error(), w)
			return
		}

		type singleURLOutput struct {
			URL string `json:"url"`
		}

		type outputURLsOpenAI struct {
			Created int               `json:"created"`
			Data    []singleURLOutput `json:"data"`
		}

		var outputURLs outputURLsOpenAI
		err = json.Unmarshal(body, &outputURLs)
		if err != nil {
			respondWithError(http.StatusInternalServerError, err.Error(), w)
			return
		}

		imageURL := outputURLs.Data[0].URL

		if inputBody.AsURL {
			res := &asURLResponse{
				Url: imageURL,
			}
			respondWithJSON(http.StatusOK, res, w)
			return
		}

		imgReq, err := http.NewRequest("GET", imageURL, nil)
		if err != nil {
			respondWithError(http.StatusInternalServerError, err.Error(), w)
			return
		}

		imgResp, err := client.Do(imgReq)
		if err != nil {
			respondWithError(http.StatusInternalServerError, err.Error(), w)
			return
		}
		defer imgResp.Body.Close()

		imgData, err := io.ReadAll(imgResp.Body)
		if err != nil {
			respondWithError(http.StatusInternalServerError, err.Error(), w)
			return
		}

		respondWithRawData(http.StatusOK, imgData, w)

	} else if inputBody.Output == "Shape" {
		if !inputBody.Pixelart {
			respondWithError(http.StatusBadRequest, "Error: output \"Shape\" can only be pixelart, try again with \"pixelart\" equals to true.", w)
			return
		}

		type imageGenerationBody struct {
			UserInput string `json:"userInput"`
		}

		aiGenReqBody := imageGenerationBody{
			UserInput: inputBody.Prompt,
		}

		// marshall data to json (like json_encode)
		b, _ := json.Marshal(aiGenReqBody)
		req, err := http.NewRequest("POST", "https://cubzhaigen.caillef.com/pixelart/vox", bytes.NewReader(b))
		if err != nil {
			respondWithError(http.StatusInternalServerError, err.Error(), w)
			return
		}

		req.Header.Set("Content-Type", "application/json")

		resp, err := client.Do(req)
		if err != nil {
			respondWithError(http.StatusInternalServerError, err.Error(), w)
			return
		}
		defer resp.Body.Close()

		body, err := io.ReadAll(resp.Body)
		if err != nil {
			respondWithError(http.StatusInternalServerError, err.Error(), w)
			return
		}

		type OutputURLs struct {
			URLs []string `json:"urls"`
		}

		var outputURLs OutputURLs
		err = json.Unmarshal(body, &outputURLs)
		if err != nil {
			respondWithError(http.StatusInternalServerError, err.Error(), w)
			return
		}

		voxURL := "https://cubzhaigen.caillef.com/" + outputURLs.URLs[0]

		if inputBody.AsURL {
			res := &asURLResponse{
				Url: voxURL,
			}
			respondWithJSON(http.StatusOK, res, w)
			return
		}

		voxReq, err := http.NewRequest("GET", voxURL, nil)
		if err != nil {
			respondWithError(http.StatusInternalServerError, err.Error(), w)
			return
		}

		voxResp, err := client.Do(voxReq)
		if err != nil {
			respondWithError(http.StatusInternalServerError, err.Error(), w)
			return
		}
		defer voxResp.Body.Close()

		voxData, err := io.ReadAll(voxResp.Body)
		if err != nil {
			respondWithError(http.StatusInternalServerError, err.Error(), w)
			return
		}

		respondWithRawData(http.StatusOK, voxData, w)
	} else {
		respondWithError(http.StatusBadRequest, "Error: output must be \"Quad\" or \"Shape\".", w)
		return
	}
}

func patchWorldLikes(w http.ResponseWriter, r *http.Request) {
	type worldAddRemoveLikeReq struct {
		Value bool `json:"value"`
	}

	// retrieve user sending the request
	usr, err := getUserFromRequestContext(r)
	if err != nil {
		respondWithError(http.StatusInternalServerError, "", w)
		return
	}

	// retrieve client build number
	clientInfo, err := getClientInfo(r)
	if err != nil {
		respondWithError(http.StatusBadRequest, "", w)
		return
	}

	worldID := chi.URLParam(r, "id")

	dbClient, err := dbclient.SharedUserDB()
	if err != nil {
		respondWithError(http.StatusInternalServerError, "", w)
		return
	}

	var req worldAddRemoveLikeReq
	err = json.NewDecoder(r.Body).Decode(&req)
	if err != nil {
		respondWithError(http.StatusBadRequest, "can't decode json request", w)
		return
	}

	if req.Value {
		err = game.AddLike(dbClient, worldID, usr.ID)

		// notification
		go func() {
			w, err := game.GetByID(dbClient, worldID, "", &clientInfo.BuildNumber)
			if err != nil || w == nil {
				return
			}

			author, err := user.GetByID(dbClient, w.Author)
			if err != nil || author == nil {
				return
			}

			// create notification
			category := NOTIFICATION_CATEGORY_LIKE
			title := "❤️ WORLD LIKED!"
			username := usr.Username
			if len(username) == 0 {
				username = user.DefaultUsername
			}
			message := username + " liked your world! (" + w.Title + ")"
			notificationID, badgeCount, err := scyllaClientUniverse.CreateUserNotification(author.ID, category, title, message)
			if err == nil {
				// send push notification
				sendPushNotification(author, category, title, message, notificationID, badgeCount)
			}
		}()

	} else {
		err = game.RemoveLike(dbClient, worldID, usr.ID)
	}

	if err != nil {
		respondWithError(http.StatusBadRequest, "", w)
		return
	}

	// Update search engine data
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
				fmt.Println("❌ PATCH world likes: search engine update failed:", err.Error())
			}
		}
	}

	respondOK(w)
}

func worldAddLikeDeprecated(w http.ResponseWriter, r *http.Request) {

	type worldAddLikeReq struct {
		Credentials *UserCredentials `json:"credentials,omitempty"`
		ID          string           `json:"id,omitempty"`
	}

	var u *user.User
	var err error

	dbClient, err := dbclient.SharedUserDB()
	if err != nil {
		respondWithError(http.StatusInternalServerError, "", w)
		return
	}

	var req worldAddLikeReq
	err = json.NewDecoder(r.Body).Decode(&req)
	if err != nil {
		respondWithError(http.StatusBadRequest, "can't decode json request", w)
		return
	}

	u = getUserWithCredentialsAndRespondOnError(req.Credentials, w)
	if u == nil {
		return
	}

	err = game.AddLike(dbClient, req.ID, u.ID)
	if err != nil {
		respondWithError(http.StatusBadRequest, "", w)
		return
	}

	respondOK(w)
}

func worldRemoveLikeDeprecated(w http.ResponseWriter, r *http.Request) {

	type worldRemoveLikeReq struct {
		Credentials *UserCredentials `json:"credentials,omitempty"`
		ID          string           `json:"id,omitempty"`
	}

	var u *user.User
	var err error

	dbClient, err := dbclient.SharedUserDB()
	if err != nil {
		respondWithError(http.StatusInternalServerError, "", w)
		return
	}

	var req worldRemoveLikeReq
	err = json.NewDecoder(r.Body).Decode(&req)
	if err != nil {
		respondWithError(http.StatusBadRequest, "can't decode json request", w)
		return
	}

	u = getUserWithCredentialsAndRespondOnError(req.Credentials, w)
	if u == nil {
		return
	}

	err = game.RemoveLike(dbClient, req.ID, u.ID)
	if err != nil {
		respondWithError(http.StatusBadRequest, "", w)
		return
	}

	respondOK(w)
}

func getServers(w http.ResponseWriter, r *http.Request) {

	type getServersRes struct {
		Servers []*WorldServer `json:"servers,omitempty"`
	}

	queryParams := r.URL.Query()

	worldIDParams := queryParams["worldID"]
	if len(worldIDParams) == 0 {
		fmt.Println("🐞 getServers - no worldID")
		respondWithError(http.StatusInternalServerError, "no worldID", w)
		return
	}
	worldID := worldIDParams[0]

	tagParams := queryParams["tag"]
	if len(tagParams) == 0 {
		fmt.Println("🐞 getServers - no tag")
		respondWithError(http.StatusInternalServerError, "no tag", w)
		return
	}
	tag := tagParams[0]

	gameServerReq := &gameServersRequest{
		Mode:               "both",
		GameID:             worldID,
		GameServerTag:      tag,
		Region:             types.RegionZero(),
		MaxResponseCount:   100,
		ResponseChan:       make(chan []*game.Server),
		IncludeFullServers: true,
		CreateIfNone:       false,
	}

	// TODO: check for buffer overflow
	gameServersRequestChan <- gameServerReq

	servers := <-gameServerReq.ResponseChan

	if servers == nil {
		fmt.Println("🐞 getServers - can't get game server")
		respondWithError(http.StatusInternalServerError, "", w)
		return
	}

	res := &getServersRes{
		Servers: make([]*WorldServer, 0),
	}

	for _, srv := range servers {
		// whether the server is in `dev` mode
		isDevMode := srv.Mode == gameserver.ModeDev
		// append server to the collection
		res.Servers = append(res.Servers, &WorldServer{
			ID:         srv.ID,
			Address:    srv.Addr,
			Players:    int32(srv.Players),
			MaxPlayers: srv.MaxPlayers,
			DevMode:    isDevMode,
		})
	}

	respondWithJSON(http.StatusOK, res, w)
}

// sends notification if push tokens are found for recipient
// done in a go routine, not blocking, not raising nor returning errors
// (just console prints)
func sendPushNotification(recipient *user.User, category, title, message, notificationID string, badgeCount uint) {
	go func() {
		var err error

		// Apple tokens
		if len(recipient.ApplePushTokens) > 0 {
			c := apnsclient.Shared(apnsclient.ClientOpts{})
			if c != nil {
				sound := "notif.caf"
				if category == "money" {
					sound = "notif_money.caf"
				}

				// collection of tokens having generated errors
				badTokens := make([]user.Token, 0)

				// loop over all tokens of the recipient
				for _, token := range recipient.ApplePushTokens {
					// send notification
					err = c.SendNotification(token.Token, token.Variant, category, title, message, notificationID, sound, badgeCount)
					if err != nil {
						fmt.Println("❌ failed to trigger Apple Push Notif:", err.Error())
						badTokens = append(badTokens, token)
					}
					// TODO: gaetan: remove invalid tokens (when error reason is "BadDeviceToken")
				}

				// remove all bad tokens from recipient.ApplePushTokens
				cleanTokensList := make([]user.Token, 0)
				for _, token := range recipient.ApplePushTokens {
					if !slices.Contains(badTokens, token) {
						cleanTokensList = append(cleanTokensList, token)
					}
				}
				dbClient, err := dbclient.SharedUserDB()
				if err != nil {
					fmt.Println("❌ failed to remove invalid Apple Push Tokens:", err.Error())
					return
				}
				// save the recipient
				recipient.ApplePushTokens = cleanTokensList
				err = recipient.Save(dbClient)
				if err != nil {
					fmt.Println("❌ failed to remove invalid Apple Push Tokens:", err.Error())
				}
			}
		}

		// Firebase tokens
		if len(recipient.FirebasePushTokens) > 0 {
			c := fcmclient.Shared(fcmclient.ClientOpts{})
			if c != nil {
				sound := category
				for _, token := range recipient.FirebasePushTokens {
					// Currently we use only 1 Firebase project (for production), so no need to specify variant (dev/prod)
					err = c.SendNotification(token.Token, category, title, message, notificationID, sound, int(badgeCount))
					if err != nil {
						fmt.Println("❌ failed to trigger Firebase Push Notif:", err.Error())
					}
					// TODO: gaetan: remove invalid tokens (if they can be identified)
				}
			}
		}
	}()
}

func getParentDashboard(w http.ResponseWriter, r *http.Request) {

	// retrieve path parameters
	dashboardID := chi.URLParam(r, "dashboardID")
	if dashboardID == "" {
		respondWithError(http.StatusBadRequest, "missing dashboard id", w)
		return
	}
	secretKey := chi.URLParam(r, "secretKey")
	if secretKey == "" {
		respondWithError(http.StatusBadRequest, "missing secret key", w)
		return
	}

	// validate secret key and update dashboard in ScyllaDB
	dashboard, err := checkParentDashboardIDAndSecretKey(dashboardID, secretKey)
	fmt.Println("GET checkParentDashboardIDAndSecretKey", dashboard, err)
	if err != nil {
		respondWithError(http.StatusInternalServerError, "internal server error", w)
		return
	}
	if dashboard == nil {
		respondWithError(http.StatusNotFound, "dashboard not found", w)
		return
	}

	dbClient, err := dbclient.SharedUserDB()
	if err != nil {
		respondWithError(http.StatusInternalServerError, "internal server error", w)
		return
	}

	parent, err := user.GetByID(dbClient, dashboard.ParentID)
	if err != nil || parent == nil {
		respondWithError(http.StatusInternalServerError, "internal server error", w)
		return
	}

	child, err := user.GetByID(dbClient, dashboard.ChildID)
	if err != nil || child == nil {
		respondWithError(http.StatusInternalServerError, "internal server error", w)
		return
	}

	var emptyString string = ""

	// Confirms the parent's phone number if it's not already done
	// (in both the parent AND the child User records)
	if parent.Phone == nil && parent.PhoneNew != nil && *parent.PhoneNew != "" {
		parent.Phone = parent.PhoneNew
		parent.PhoneNew = &emptyString
		parent.PhoneVerifCode = &emptyString
		err = parent.Save(dbClient)
		if err != nil {
			respondWithError(http.StatusInternalServerError, "internal server error", w)
			return
		}
	}
	if child.ParentPhone == nil && child.ParentPhoneNew != nil && *child.ParentPhoneNew != "" {
		child.ParentPhone = child.ParentPhoneNew
		child.ParentPhoneNew = &emptyString
		child.ParentPhoneVerifCode = &emptyString
		err = child.Save(dbClient)
		if err != nil {
			respondWithError(http.StatusInternalServerError, "internal server error", w)
			return
		}
	}

	// Response
	childAge, err := child.ComputeExactOrEstimatedAge()
	if err != nil {
		respondWithError(http.StatusInternalServerError, "internal server error", w)
		return
	}
	parentalControlApproved := false // false by default
	if child.ParentalControlApproved != nil {
		parentalControlApproved = *child.ParentalControlApproved
	}
	parentalControlChatAllowed := true // true by default
	if child.ParentalControlChatAllowed != nil {
		parentalControlChatAllowed = *child.ParentalControlChatAllowed
	}
	parentalControlMessagesAllowed := true // true by default
	if child.ParentalControlMessagesAllowed != nil {
		parentalControlMessagesAllowed = *child.ParentalControlMessagesAllowed
	}

	dashboardInfo := &DashboardInfo{
		CreatedAt:                      &dashboard.CreatedAt,
		ChildAge:                       &childAge,
		ParentalControlApproved:        &parentalControlApproved,
		ParentalControlChatAllowed:     &parentalControlChatAllowed,
		ParentalControlMessagesAllowed: &parentalControlMessagesAllowed,
		// add more fields here...
	}

	respondWithJSON(http.StatusOK, dashboardInfo, w)
}

func patchParentDashboard(w http.ResponseWriter, r *http.Request) {

	// retrieve path parameters
	dashboardID := chi.URLParam(r, "dashboardID")
	if dashboardID == "" {
		respondWithError(http.StatusBadRequest, "missing dashboard id", w)
		return
	}
	secretKey := chi.URLParam(r, "secretKey")
	if secretKey == "" {
		respondWithError(http.StatusBadRequest, "missing secret key", w)
		return
	}

	// parse request body
	var req DashboardInfo
	err := json.NewDecoder(r.Body).Decode(&req)
	if err != nil {
		respondWithError(http.StatusBadRequest, "can't decode json request", w)
		return
	}

	// validate secret key and update dashboard in ScyllaDB
	dashboard, err := checkParentDashboardIDAndSecretKey(dashboardID, secretKey)
	if err != nil {
		fmt.Println("🐞 patchParentDashboard 2")
		respondWithError(http.StatusInternalServerError, "internal server error", w)
		return
	}
	if dashboard == nil {
		respondWithError(http.StatusNotFound, "dashboard not found", w)
		return
	}

	dbClient, err := dbclient.SharedUserDB()
	if err != nil {
		respondWithError(http.StatusInternalServerError, "", w)
		return
	}

	// Get child user
	childUser, err := user.GetByID(dbClient, dashboard.ChildID)
	if err != nil {
		respondWithError(http.StatusInternalServerError, "", w)
	}

	if req.ParentalControlApproved != nil {
		childUser.ParentalControlApproved = req.ParentalControlApproved
	}
	if req.ParentalControlChatAllowed != nil {
		childUser.ParentalControlChatAllowed = req.ParentalControlChatAllowed
	}
	if req.ParentalControlMessagesAllowed != nil {
		childUser.ParentalControlMessagesAllowed = req.ParentalControlMessagesAllowed
	}

	err = childUser.Save(dbClient)
	if err != nil {
		respondWithError(http.StatusInternalServerError, "", w)
		return
	}

	// update the UpdatedAt field of the dashboard
	err = scyllaClientUniverse.UpdateUniverseParentDashboard(dashboardID)
	if err != nil {
		respondWithError(http.StatusInternalServerError, "internal server error", w)
		return
	}

	respondOK(w)
}

func getTransactionsPaymentRequest(w http.ResponseWriter, r *http.Request) {

	// only allow requests that requested this domain
	const REQUESTED_DOMAIN_ALLOWED = "hub-http"

	fmt.Println("[🐞][💰] getTransactionsPaymentRequest ☀️☀️☀️☀️☀️")

	// Test what domain was requested
	requestedDomain := r.Host
	fmt.Println("[🐞][💰] requestedDomain", requestedDomain)
	if requestedDomain != REQUESTED_DOMAIN_ALLOWED {
		respondWithError(http.StatusForbidden, "domain not allowed", w)
		return
	}

	respondOK(w)
}

// Min returns the smaller of x or y.
func Min(x, y int64) int64 {
	if x < y {
		return x
	}
	return y
}

// This is called by the "codegen-server" service.
func postTransactionsPaymentRequest(w http.ResponseWriter, r *http.Request) {

	fmt.Println("[🐞][💰][postTransactionsPaymentRequest] ☀️☀️☀️☀️☀️")

	// Make sure the request is coming from inside the Docker Swarm network (and not from the public internet)
	{
		// Only allow requests that requested this domain:
		const REQUESTED_DOMAIN_ALLOWED = "hub-http"
		// Test the requested domain
		requestedDomain := r.Host
		fmt.Println("[🐞][💰][postTransactionsPaymentRequest] requestedDomain:", requestedDomain)
		if requestedDomain != REQUESTED_DOMAIN_ALLOWED {
			respondWithError(http.StatusForbidden, "domain not allowed", w)
			return
		}
	}

	type postTransactionsPaymentRequestReq struct {
		UserID          string `json:"userID"`
		AmountMillionth int64  `json:"amountMillionth"`
	}

	// decode request body
	var req postTransactionsPaymentRequestReq
	err := json.NewDecoder(r.Body).Decode(&req)
	if err != nil {
		respondWithError(http.StatusBadRequest, "can't decode json request", w)
		return
	}

	dbClient, err := dbclient.SharedUserDB()
	if err != nil {
		respondWithError(http.StatusInternalServerError, "", w)
		return
	}

	// retrieve user sending the request
	usr, err := user.GetByID(dbClient, req.UserID)
	if err != nil {
		respondWithError(http.StatusUnauthorized, "", w)
		return
	}

	// get user balances (in millionths of blux)
	balances, err := scyllaClientUniverse.GetUserBalances(usr.ID)
	if err != nil {
		respondWithError(http.StatusInternalServerError, "", w)
		return
	}

	fmt.Println("[🐞][💰] postTransactionsPaymentRequest", balances)
	fmt.Println("[🐞][💰] - DummyCoins:", balances.DummyCoins)
	fmt.Println("[🐞][💰] - GrantedCoins:", balances.GrantedCoins)
	fmt.Println("[🐞][💰] - PurchasedCoins:", balances.PurchasedCoins)
	fmt.Println("[🐞][💰] - EarnedCoins:", balances.EarnedCoins)
	fmt.Println("[🐞][💰] - TotalCoins:", balances.TotalCoins)

	// Check if user has enough balance.
	// Check types of coins in order:
	// - GrantedCoins
	// - PurchasedCoins
	// - EarnedCoins

	const kAmountToWisdraw int64 = 1000000 // 1 blux

	// If user hasn't enough money, return an error.
	if balances.TotalCoins < kAmountToWisdraw {
		respondWithError(http.StatusPaymentRequired, "insufficient balance", w)
		return
	}

	// User has enough money.
	// Let's create a transaction.

	var grantedCoinsToWithdraw int64 = 0
	var purchasedCoinsToWithdraw int64 = 0
	var earnedCoinsToWithdraw int64 = 0

	var remainingAmountToWithdraw int64 = kAmountToWisdraw

	// Withdraw granted coins
	if remainingAmountToWithdraw > 0 && balances.GrantedCoins > 0 {
		amount := Min(remainingAmountToWithdraw, balances.GrantedCoins)
		grantedCoinsToWithdraw = amount
		remainingAmountToWithdraw -= amount
	}

	// Withdraw purchased coins
	if remainingAmountToWithdraw > 0 && balances.PurchasedCoins > 0 {
		amount := Min(remainingAmountToWithdraw, balances.PurchasedCoins)
		purchasedCoinsToWithdraw = amount
		remainingAmountToWithdraw -= amount
	}

	// Withdraw earned coins
	if remainingAmountToWithdraw > 0 && balances.EarnedCoins > 0 {
		amount := Min(remainingAmountToWithdraw, balances.EarnedCoins)
		earnedCoinsToWithdraw = amount
		remainingAmountToWithdraw -= amount
	}

	if remainingAmountToWithdraw > 0 {
		fmt.Println("[🐞][💰] postTransactionsPaymentRequest - THIS SHOULD NOT HAPPEN - failed to withdraw coins (1) |", remainingAmountToWithdraw)
		respondWithError(http.StatusPaymentRequired, "insufficient balance", w)
		return
	}
	if grantedCoinsToWithdraw+purchasedCoinsToWithdraw+earnedCoinsToWithdraw != kAmountToWisdraw {
		fmt.Println("[🐞][💰] postTransactionsPaymentRequest - THIS SHOULD NOT HAPPEN - failed to withdraw coins (2) |", grantedCoinsToWithdraw+purchasedCoinsToWithdraw+earnedCoinsToWithdraw)
		respondWithError(http.StatusPaymentRequired, "insufficient balance", w)
		return
	}

	// Create a transaction for the user

	opts := transactions.DebitCoinsFromUserOpts{
		GrantedCoinAmountMillionth:   grantedCoinsToWithdraw,
		PurchasedCoinAmountMillionth: purchasedCoinsToWithdraw,
		EarnedCoinAmountMillionth:    earnedCoinsToWithdraw,
		Reason:                       "AI request",
		Description:                  "",
	}

	err = transactions.DebitCoinsFromUser(usr.ID, opts)
	if err != nil {
		fmt.Println("[🐞][💰] postTransactionsPaymentRequest ERROR:", err.Error())
		respondWithError(http.StatusInternalServerError, "internal server error", w)
		return
	}

	fmt.Println("[🐞][💰] postTransactionsPaymentRequest OK ✅")

	respondOK(w)
}

func postWorldScript(w http.ResponseWriter, r *http.Request) {

	dbClient, err := dbclient.SharedUserDB()
	if err != nil {
		respondWithError(http.StatusInternalServerError, "", w)
		return
	}

	// retrieve client build number
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

	game, err := game.GetByID(dbClient, worldID, "", &clientInfo.BuildNumber)
	if err != nil {
		respondWithError(http.StatusBadRequest, "", w)
		return
	}

	// make sure user is the author of the game
	if game.Author != usr.ID {
		fmt.Println("⚠️ HACK? - PUBLIC API - trying to update script while not being author", usr.ID)
		respondWithError(http.StatusUnauthorized, "", w)
		return
	}

	// read new script from request body
	newScriptBytes, err := io.ReadAll(r.Body)
	if err != nil {
		respondWithError(http.StatusInternalServerError, "", w)
		return
	}
	newScript := string(newScriptBytes)

	// obtain existing script from git repository (or other source)
	existingScript, err := worldUtilsGetScript(game, &clientInfo.BuildNumber)
	if err != nil {
		respondWithError(http.StatusInternalServerError, "", w)
		return
	}

	// generate commit message using AI
	commitMessage := ""
	{
		commitMessage, err = worldUtilsGenerateCommitMessage(WorldUtilsGenerateCommitMessageOpts{
			AnthropicAPIKey: AnthropicAPIKey,
			CodeBefore:      existingScript,
			CodeAfter:       newScript,
		})
		if err != nil {
			fmt.Println("[❌][🐞] postWorldScript error generating commit message:", err.Error())
		}
		if err != nil && err != ErrNoChangesInCode {
			respondWithError(http.StatusInternalServerError, "", w)
			return
		}
	}

	err = updateWorldCode(dbClient, worldID, newScript, UpdateWorldCodeOpts{
		CommitMessage: commitMessage, // if empty, a default message will be generated
	})
	if err != nil {
		respondWithError(http.StatusInternalServerError, "", w)
		return
	}

	respondOK(w)
}

type purchaseReceipt struct {
	ReceiptData   string `json:"receipt-data,omitempty"`
	TransactionID string `json:"transaction-id,omitempty"`
	ProductID     string `json:"product-id,omitempty"`
	PurchaseType  string `json:"purchase-type,omitempty"` // "apple-iap", "google-iap"
}

type verifyPurchaseResponse struct {
	Success  bool   `json:"success"`
	Sandbox  bool   `json:"sandbox"`
	Error    string `json:"error,omitempty"`
	Credited int64  `json:"credited,omitempty"`
}

func verifyPurchase(w http.ResponseWriter, r *http.Request) {
	var credited int64 = 0

	_, err := dbclient.SharedUserDB()
	if err != nil {
		respondWithJSON(http.StatusInternalServerError, &verifyPurchaseResponse{Success: false, Sandbox: false, Error: "internal server error (1)", Credited: credited}, w)
		return
	}

	usr, err := getUserFromRequestContext(r)
	if err != nil {
		respondWithJSON(http.StatusInternalServerError, &verifyPurchaseResponse{Success: false, Sandbox: false, Error: "internal server error (2)", Credited: credited}, w)
		return
	}

	var req purchaseReceipt
	err = json.NewDecoder(r.Body).Decode(&req)
	if err != nil {
		respondWithJSON(http.StatusBadRequest, &verifyPurchaseResponse{Success: false, Sandbox: false, Error: "decode error", Credited: credited}, w)
		return
	}

	// Handle Apple IAP verification
	if req.PurchaseType == "apple-iap" {
		// First try production environment
		verifyURL := "https://api.storekit.itunes.apple.com/inApps/v1/transactions/" + req.TransactionID
		sandbox := false

		// Create HTTP client
		client := &http.Client{}

		// Create request
		request, err := http.NewRequest("GET", verifyURL, nil)
		if err != nil {
			respondWithJSON(http.StatusInternalServerError, &verifyPurchaseResponse{Success: false, Sandbox: sandbox, Error: "failed to create request", Credited: credited}, w)
			return
		}

		// Get valid JWT token
		token, err := GetAppStoreJWT()
		if err != nil {
			respondWithJSON(http.StatusInternalServerError, &verifyPurchaseResponse{Success: false, Sandbox: sandbox, Error: "failed to get JWT token", Credited: credited}, w)
			return
		}

		// Add required headers
		request.Header.Add("Authorization", "Bearer "+token)
		request.Header.Add("Accept", "application/json")

		// Make request to Apple API
		resp, err := client.Do(request)
		if err != nil {
			respondWithJSON(http.StatusInternalServerError, &verifyPurchaseResponse{Success: false, Sandbox: sandbox, Error: "failed to verify with Apple", Credited: credited}, w)
			return
		}
		defer resp.Body.Close()

		// IS IT POSSIBLE FOR THE CLIENT TO KNOW THAT AND SEND THE INFORMATION?

		// if UNAUTHORIZED, try sandbox environment:
		if resp.StatusCode == 401 {
			sandbox = true
			verifyURL = "https://api.storekit-sandbox.itunes.apple.com/inApps/v1/transactions/" + req.TransactionID

			request, err = http.NewRequest("GET", verifyURL, nil)
			if err != nil {
				respondWithJSON(http.StatusInternalServerError, &verifyPurchaseResponse{Success: false, Sandbox: sandbox, Error: "failed to create sandbox request", Credited: credited}, w)
				return
			}
			request.Header.Add("Authorization", "Bearer "+token)
			request.Header.Add("Accept", "application/json")

			resp, err = client.Do(request)
			if err != nil {
				respondWithJSON(http.StatusInternalServerError, &verifyPurchaseResponse{Success: false, Sandbox: sandbox, Error: "failed to verify with Apple sandbox", Credited: credited}, w)
				return
			}
			defer resp.Body.Close()
		}

		// Check status code
		if resp.StatusCode != http.StatusOK {
			respondWithJSON(resp.StatusCode, &verifyPurchaseResponse{Success: false, Sandbox: sandbox, Error: "wrong status code from Apple", Credited: credited}, w)
			return
		}

		var result map[string]interface{}
		if err := json.NewDecoder(resp.Body).Decode(&result); err != nil {
			respondWithJSON(http.StatusInternalServerError, &verifyPurchaseResponse{Success: false, Sandbox: sandbox, Error: "failed to parse Apple response", Credited: credited}, w)
			return
		}

		// Verify the transaction status
		signedTransactionInfo, ok := result["signedTransactionInfo"].(string)
		if !ok || signedTransactionInfo == "" {
			respondWithJSON(http.StatusBadRequest, &verifyPurchaseResponse{Success: false, Sandbox: sandbox, Error: "invalid transaction info", Credited: credited}, w)
			return
		}

		// Check if we have signedTransactionInfo
		if signedInfo, ok := result["signedTransactionInfo"].(string); ok {
			// Parse the JWT without verification
			parser := jwt.Parser{
				SkipClaimsValidation: true,
			}
			token, _, err := parser.ParseUnverified(signedInfo, jwt.MapClaims{})
			if err != nil {
				respondWithJSON(http.StatusInternalServerError, &verifyPurchaseResponse{Success: false, Sandbox: sandbox, Error: "couldn't parse signedTransactionInfo", Credited: credited}, w)
				return
			} else if claims, ok := token.Claims.(jwt.MapClaims); ok {
				fmt.Printf("💰 TRANSACTION DETAILS:\n")
				fmt.Printf("  - Transaction ID: %v\n", claims["transactionId"])
				// fmt.Printf("  - Original Transaction ID: %v\n", claims["originalTransactionId"])
				// fmt.Printf("  - Bundle ID: %v\n", claims["bundleId"])
				fmt.Printf("  - Product ID: %v\n", claims["productId"])
				// fmt.Printf("  - Purchase Date: %v\n", claims["purchaseDate"])
				// fmt.Printf("  - Original Purchase Date: %v\n", claims["originalPurchaseDate"])
				// fmt.Printf("  - Quantity: %v\n", claims["quantity"])
				fmt.Printf("  - Type: %v\n", claims["type"])
				fmt.Printf("  - In-App Ownership Type: %v\n", claims["inAppOwnershipType"])
				// fmt.Printf("  - Signed Date: %v\n", claims["signedDate"])
				fmt.Printf("  - Environment: %v\n", claims["environment"])
				// fmt.Printf("  - Transaction Reason: %v\n", claims["transactionReason"])
				// fmt.Printf("  - Storefront: %v\n", claims["storefront"])
				// fmt.Printf("  - Storefront ID: %v\n", claims["storefrontId"])
				fmt.Printf("  - Price: %v\n", claims["price"])
				fmt.Printf("  - Currency: %v\n", claims["currency"])
				// fmt.Printf("  - App Transaction ID: %v\n", claims["appTransactionId"])

				// backdoor to let Apple test the app with any account in sandbox mode
				// and verify that the account gets credited.
				if r.Header.Get(HTTP_HEADER_IAP_SANDBOX_KEY) == "a262649552c3d4d717af2d522fd4e2c2" {
					fmt.Println("💰 BACKDOOR: sandbox mode disabled with secret header")
					sandbox = false
				}

				if pID, ok := claims["productId"].(string); ok {
					if nbCoins, exists := IAP_CONSUMABLE_COIN_PRODUCTS[pID]; exists {
						fmt.Printf("💰 NB COINS: %d\n", nbCoins)

						if sandbox && nbCoins > 1000 { // not crediting
							// not crediting users high value products in sandbox mode
							respondWithJSON(http.StatusBadRequest, &verifyPurchaseResponse{
								Success:  false,
								Sandbox:  sandbox,
								Error:    "couldn't finalize transaction",
								Credited: 0,
							}, w)
							return
						}

						// Create unique coin transction for this Transaction ID
						amount := int64(nbCoins)
						credited += amount
						amountInMillionths := int64(amount * 1000000)
						REASON := "apple-iap_transaction_" + req.TransactionID
						DESCRIPTION := "Purchase of " + strconv.FormatInt(amount, 10) + " blux"

						if sandbox { // not crediting
							// one unique transaction ID for each product ID
							// allowing testers to get first products (cheap ones), for free.
							REASON = "test_" + pID
						}

						opts := transactions.NewCreditPurchasedCoinsOpts(DESCRIPTION)
						err = transactions.CreditPurchasedCoinsToUser(usr.ID, amountInMillionths, REASON, opts)
						if err != nil {
							respondWithJSON(http.StatusInternalServerError, &verifyPurchaseResponse{
								Success:  false,
								Sandbox:  sandbox,
								Error:    "couldn't finalize transaction",
								Credited: 0,
							}, w)
							return
						} else {
							sandbox = false // when transaction is successful, pretend it's not sandbox
						}
					}
				}
			}
		}

		// If we get here, the transaction is valid
		respondWithJSON(http.StatusOK, &verifyPurchaseResponse{
			Success:  true,
			Sandbox:  sandbox,
			Credited: credited,
		}, w)
	}

	// TODO: credit user's account (if it hasn't been done yet)

	respondOK(w)
}

// ------------
// MODULES
// ------------

func moduleGet(w http.ResponseWriter, r *http.Request) {

	// fetchModule
	src := r.URL.Query().Get("src")
	if src == "" {
		respondWithError(http.StatusBadRequest, "", w)
		return
	}

	// process & validate <src> value
	src = strings.ToLower(src)

	// read ETag value from request
	etag := r.Header.Get("If-None-Match")

	module, err := fetchModule(src, etag)
	if err != nil {
		respondWithError(http.StatusBadRequest, "", w)
		return
	}
	if module == nil {
		respondWithError(http.StatusNotFound, "", w)
	}

	// TODO: gaetan: store it in ScyllaDB (cache)

	// w.Header().Set("Cache-Control", "max-age="+MAX_AGE_24H)
	w.Header().Set("Cache-Control", "max-age="+MAX_AGE_2MIN)
	w.Header().Set("ETag", module.Commit)
	respondWithString(http.StatusOK, module.Script, w)
}

// --------------------------------------------------
// ITEM DRAFTS
// --------------------------------------------------

func NewItemFromItemDraft(itemDraft search.ItemDraft) item.Item {
	return item.Item{
		ID:          itemDraft.ID,
		Repo:        itemDraft.Repo,
		Name:        itemDraft.Name,
		Description: itemDraft.Description,
		Category:    itemDraft.Category,
		Likes:       itemDraft.Likes,
		Views:       itemDraft.Views,
		Created:     time.UnixMicro(itemDraft.CreatedAt),
		Updated:     time.UnixMicro(itemDraft.UpdatedAt),
	}
}

var FEATURED_ITEMS []item.Item = []item.Item{}

func preloadFeaturedItems(searchClient *search.Client) {
	if len(FEATURED_ITEMS) > 0 {
		// Do nothing. Items look to be are already loaded.
		return
	}
	// loop over FEATURED_ITEMS_IDS
	// for each ID, fetch the item from the search engine
	// and append it to FEATURED_ITEMS
	// search engine client
	for _, id := range FEATURED_ITEMS_IDS {
		// fetch item from search engine
		// append item to FEATURED_ITEMS
		itemDraft, err := searchClient.GetItemDraftByID(id)
		if err != nil {
			fmt.Println("❌ preloadFeaturedItems: failed to fetch item", id, ":", err.Error())
			continue
		}
		FEATURED_ITEMS = append(FEATURED_ITEMS, NewItemFromItemDraft(itemDraft))
	}
}

// Query parameters
// - search     (string) : the search string. Default value is "".
// - page       (int)    : the requested page of results. Default value is 1.
// - perPage    (int)    : the number of results per page Default value is 10.
// - minBlock   (int)    :
// - category   (string) : value "hat" for example
func getItems(w http.ResponseWriter, r *http.Request) {

	// search engine client
	searchClient, err := search.NewClient(CUBZH_SEARCH_SERVER, CUBZH_SEARCH_APIKEY)
	if err != nil {
		respondWithError(http.StatusInternalServerError, "", w)
		return
	}

	// Query parameters
	const queryParamSearch string = "search"
	const queryParamSortBy string = "sortBy"
	const queryParamPage string = "page"
	const queryParamPerPage string = "perPage"
	const queryParamMinBlock string = "minBlock"
	const queryParamCategory string = "category"
	const queryParamRepo string = "repo"
	const queryParamArchived string = "archived"

	type itemdraftsSearchResp struct {
		Results      []item.Item `json:"results"`
		TotalResults int         `json:"totalResults"`
		Page         int         `json:"page"`
		PerPage      int         `json:"perPage"`
	}

	// Query parameters
	var searchStr string = kSearchDefaultText
	var page int = kSearchDefaultPage
	var perPage int = kSearchDefaultPerPage
	var minBlock int = kItemSearchDefaultMinBlock
	var categories []string = kSearchDefaultCategories
	var repos []string = kSearchDefaultRepos
	var archived bool = kItemSearchDefaultArchived
	var sortBy string = kItemSearchDefaultSortBy
	{
		// search
		if r.URL.Query().Has(queryParamSearch) {
			searchStr = r.URL.Query().Get(queryParamSearch)
		}
		// sortBy
		{
			if r.URL.Query().Has(queryParamSortBy) {
				sortBy = r.URL.Query().Get(queryParamSortBy)
			}
		}
		// page
		if r.URL.Query().Has(queryParamPage) {
			pageStr := r.URL.Query().Get(queryParamPage)
			if pageStr != "" {
				i, err := strconv.ParseInt(pageStr, 10, 32)
				if err != nil {
					respondWithError(http.StatusBadRequest, "query parameter 'page' is not valid", w)
					return
				}
				page = int(i)
			}
		}
		// perPage
		if r.URL.Query().Has(queryParamPerPage) || r.URL.Query().Has("perpage") {
			perPageStr := r.URL.Query().Get(queryParamPerPage)

			// TODO: remove this & only use camel case (when 0.0.63 is forced)
			if perPageStr == "" && r.URL.Query().Has("perpage") {
				perPageStr = r.URL.Query().Get("perpage")
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
		// minBlock
		if r.URL.Query().Has(queryParamMinBlock) {
			minBlockStr := r.URL.Query().Get(queryParamMinBlock)
			if minBlockStr != "" {
				i, err := strconv.ParseInt(minBlockStr, 10, 32)
				if err != nil {
					respondWithError(http.StatusBadRequest, "query parameter 'minblock' is not valid", w)
					return
				}
				minBlock = int(i)
			}
		}
		// category
		if r.URL.Query().Has(queryParamCategory) {
			categories = r.URL.Query()[queryParamCategory]
		}
		// repo
		if r.URL.Query().Has(queryParamRepo) {
			repos = r.URL.Query()[queryParamRepo]
		}
		// archived
		if r.URL.Query().Has(queryParamArchived) {
			archivedStr := r.URL.Query().Get(queryParamArchived)
			archived = (archivedStr == "true")
		}
	}

	var falseBool bool = false
	filter := search.ItemDraftFilter{
		MinBlockCount: &minBlock,
		Banned:        &falseBool,
		Archived:      &archived,
		Categories:    nil,
		Repos:         nil,
	}
	if len(categories) > 0 {
		filter.Categories = categories
	}
	if len(repos) > 0 {
		filter.Repos = repos
	}

	resp := itemdraftsSearchResp{}

	// if featured items are requested, we don't use the search engine and return the hardcoded list
	// TODO: ADD OTHER CATEGORIES (ONLY HAIR FOR NOW)
	if slices.Contains(categories, "featured") && (slices.Contains(categories, "hair") || slices.Contains(categories, "jacket") || slices.Contains(categories, "boots") || slices.Contains(categories, "pants")) {
		// loop over FEATURED_ITEMS and append them to resp.Results
		resp.Results = make([]item.Item, 0)
		for _, itm := range FEATURED_ITEMS {
			if slices.Contains(categories, itm.Category) {
				resp.Results = append(resp.Results, itm)
			}
		}
		resp.TotalResults = len(resp.Results)
		resp.Page = 1
		resp.PerPage = len(resp.Results) // TODO: respect the perpage requested

	} else {
		searchResp, err := searchClient.SearchCollectionItemDrafts(searchStr, sortBy, page, perPage, filter)
		if err != nil {
			respondWithError(http.StatusInternalServerError, "search failed", w)
			return
		}

		resp.Results = make([]item.Item, len(searchResp.Results))
		resp.TotalResults = searchResp.TotalResults
		resp.Page = searchResp.Page
		resp.PerPage = searchResp.PerPage
		for i, itemDraft := range searchResp.Results {
			itm := item.Item{
				ID:          itemDraft.ID,
				Repo:        itemDraft.Repo,
				Name:        itemDraft.Name,
				Description: itemDraft.Description,
				Category:    itemDraft.Category,
				Likes:       itemDraft.Likes,
				Views:       itemDraft.Views,
				Created:     time.UnixMicro(itemDraft.CreatedAt),
				Updated:     time.UnixMicro(itemDraft.UpdatedAt),
			}
			resp.Results[i] = itm
		}
	}

	respondWithJSON(http.StatusOK, resp, w)
}

// POST /itemdrafts
func itemdraftsCreate(w http.ResponseWriter, r *http.Request) {

	type itemdraftCreateReq struct {
		Name           string    `json:"name"`
		Category       string    `json:"category,omitempty"`
		Original       string    `json:"original,omitempty"`       // provided when duplicating an item
		Type           string    `json:"type,omitempty"`           // "voxels" or "mesh"
		Data           string    `json:"data,omitempty"`           // base64 encoded data
		OffsetRotation []float64 `json:"offsetRotation,omitempty"` // 3 numbers (radians)
		Scale          float64   `json:"scale,omitempty"`          // scale
	}

	// shared Hub DB client
	dbClient, err := dbclient.SharedUserDB()
	if err != nil {
		respondWithError(http.StatusInternalServerError, "", w)
		return
	}

	usr, err := getUserFromRequestContext(r)
	if err != nil {
		respondWithError(http.StatusInternalServerError, "", w)
		return
	}

	var req itemdraftCreateReq
	err = json.NewDecoder(r.Body).Decode(&req)
	if err != nil {
		respondWithError(http.StatusBadRequest, "can't decode json request", w)
		return
	}

	// Info needed to create the item.
	itemRepo := usr.Username
	itemName := req.Name
	original := req.Original // repo/name (TODO: support name only)
	authorID := usr.ID
	category := req.Category
	itemType := req.Type
	if itemType == "" {
		itemType = "voxels"
	}

	// parse `original`
	// it can be ofr the form `repo.name`, `repo/name` or just `name`
	// when repo isn't provided, it defaults to caller's username
	if original != "" {
		repo := ""
		name := original
		parts := strings.Split(original, ".")
		nbParts := len(parts)

		if nbParts > 2 {
			respondWithError(http.StatusBadRequest, "incorrect name for item to duplicate", w)
			return
		}

		if nbParts == 2 {
			repo = parts[0]
			name = parts[1]
		} else {
			parts = strings.Split(original, "/")
			nbParts = len(parts)
			if nbParts > 2 {
				respondWithError(http.StatusBadRequest, "incorrect name for item to duplicate", w)
				return
			}
			if nbParts == 2 {
				repo = parts[0]
				name = parts[1]
			}
		}

		if repo == "" {
			repo = usr.Username // use user repo by default
		}

		if repo != usr.Username {
			respondWithError(http.StatusBadRequest, "can't duplicate item from other user", w)
			return
		}

		original = repo + "/" + name

		// Get original item's category
		{
			items, err := item.GetByRepoAndName(dbClient, repo, name)
			if err != nil {
				respondWithError(http.StatusInternalServerError, "failed to request original item", w)
				return
			}
			if len(items) != 1 {
				respondWithError(http.StatusNotFound, "original item not found", w)
				return
			}
			category = items[0].Category
		}
	}

	if itemRepo == "" {
		respondWithError(http.StatusBadRequest, "repo can't be empty", w)
		return
	}

	if itemName == "" {
		respondWithError(http.StatusBadRequest, "name can't be empty", w)
		return
	}

	if authorID == "" {
		respondWithError(http.StatusBadRequest, "author can't be empty", w)
		return
	}

	if category != "" && item.IsCategory(category) == false {
		respondWithError(http.StatusBadRequest, "category is not valid", w)
		return
	}

	if item.IsItemType(itemType) == false {
		respondWithError(http.StatusBadRequest, "type is not valid", w)
		return
	}

	// Make sure the item name is valid
	// chars allowed : a-z and 0-9 and _
	// has to start with a [a-z]
	// max char count 20
	regex, err := regexp.Compile(ITEM_TITLE_REGEX)
	if err != nil {
		respondWithError(http.StatusInternalServerError, "", w)
		return
	}
	if regex.MatchString(itemName) == false {
		respondWithError(http.StatusBadRequest, "invalid item name", w)
		return
	}

	// Check for duplicate.
	// Each tuple <itemRepo/itemName> has to be unique.
	// Note : tags are not supported yet.
	var items []*item.Item
	items, err = item.GetByRepoAndName(dbClient, itemRepo, itemName)
	if err != nil {
		fmt.Println("❌ POST /itemdrafts - item.GetByRepoAndName:", err.Error())
		respondWithError(http.StatusInternalServerError, "", w)
		return
	}
	if len(items) > 0 {
		respondWithError(http.StatusConflict, "repo and name already exist", w) // 409
		return
	}

	// Write default item file (3zh) for the newly created item
	var fileExtension string = ".3zh"

	var modelBytes []byte = nil

	if itemType == "mesh" {
		if req.Original != "" {
			respondWithError(http.StatusBadRequest, "can't duplicate mesh item", w)
			return
		}

		fileExtension = ".glb"

		// decode base64 encoded data
		fmt.Println("👑 RECEIVED MESH ITEM!")
		fmt.Println("👑 DATA LEN:", len(req.Data))
		fmt.Println("👑 OFFSET ROTATION:", req.OffsetRotation)
		fmt.Println("👑 SCALE:", req.Scale)

		// decode req.Data from base64 to bytes
		data, err := base64.StdEncoding.DecodeString(req.Data)
		if err != nil {
			respondWithError(http.StatusBadRequest, "can't decode base64 encoded data", w)
			return
		}

		fmt.Println("👑 DATA BASE64 DECODE ✅")

		// decode gltf data
		doc, err := decodeGltf(data)
		if err != nil {
			respondWithError(http.StatusBadRequest, "can't decode gltf data", w)
			return
		}

		fmt.Println("👑 GLTF DECODE ✅")

		err = gltfParentRootNodesIfNeeded(doc)
		if err != nil {
			respondWithError(http.StatusBadRequest, "can't parent root nodes", w)
			return
		}

		fmt.Println("👑 GLTF PARENT ROOT NODES IF NEEDED ✅")

		err = gltfApplyRotation(doc, req.OffsetRotation)
		if err != nil {
			respondWithError(http.StatusBadRequest, "can't apply rotation", w)
			return
		}

		fmt.Println("👑 GLTF APPLY ROTATION ✅")

		err = gltfApplyScale(doc, req.Scale)
		if err != nil {
			respondWithError(http.StatusBadRequest, "can't apply scale", w)
			return
		}

		fmt.Println("👑 GLTF APPLY SCALE ✅")

		err = gltfParentRootNodeIfNeeded(doc)
		if err != nil {
			respondWithError(http.StatusBadRequest, "can't parent root node", w)
			return
		}

		fmt.Println("👑 GLTF PARENT ROOT NODE IF NEEDED ✅")

		modelBytes, err = encodeGltf(doc)
		if err != nil {
			respondWithError(http.StatusBadRequest, "can't encode gltf data", w)
			return
		}

		fmt.Println("👑 GLTF ENCODE ✅")
		fmt.Println("👑 ENCODED DATA LEN:", len(modelBytes))

		// respondWithError(http.StatusInternalServerError, "not yet supported", w)
		// return
	}

	// create new item
	newItem, err := item.New(itemRepo, itemName, authorID, itemType, category, false) // false because we do not bypass reserved item names
	if err != nil {
		fmt.Println("❌ POST /itemdrafts - item.New:", err.Error())
		respondWithError(http.StatusInternalServerError, err.Error(), w)
		return
	}

	// Relative path of the .3zh or .glb file used for the new item being created.
	// It is relative to ITEMS_STORAGE_PATH.

	var fileBytes []byte = nil

	if modelBytes != nil {
		fileBytes = modelBytes
	} else {
		var srcItemFile string
		if original != "" {
			// the new item is a copy of an existing item
			srcItemFile = original + fileExtension

		} else if itemType == "voxels" {
			// new item, no duplication
			// use default item or category-specific template if category is set
			var newItemFilename string
			if category != "" {
				newItemFilename = "default_" + category // simplified by Xavier
			} else {
				newItemFilename = "newitem_v2"
			}
			newItemFilename += fileExtension

			srcItemFile = filepath.Join("official", newItemFilename) // official/default<category>.3zh
		}

		if srcItemFile == "" {
			fmt.Println("❌ POST /itemdrafts - srcItemFile is empty")
			respondWithError(http.StatusInternalServerError, "", w)
			return
		}

		srcFileAbsPath := filepath.Join(ITEMS_STORAGE_PATH, srcItemFile)
		// TODO: what if the original item has a .pcubes file and not a .3zh!? (currently, it fails)
		fileBytes, err = os.ReadFile(srcFileAbsPath) // has a .3zh extension
		if err != nil {
			fmt.Println("❌ POST /itemdrafts - os.ReadFile:", err.Error())
			respondWithError(http.StatusInternalServerError, "", w)
			return
		}
	}

	// construct pcubes abs path
	dstFilePath := filepath.Join(ITEMS_STORAGE_PATH, newItem.Repo, newItem.Name+fileExtension)

	err = os.MkdirAll(filepath.Dir(dstFilePath), os.ModePerm)
	if err != nil {
		fmt.Println("❌ POST /itemdrafts - os.MkdirAll:", err.Error())
		respondWithError(http.StatusInternalServerError, "", w)
		return
	}

	// write .3zh file for new item
	err = os.WriteFile(dstFilePath, fileBytes, 0666)
	if err != nil {
		fmt.Println("❌ POST /itemdrafts - os.WriteFile:", err.Error())
		respondWithError(http.StatusInternalServerError, "", w)
		return
	}

	if itemType == "voxels" {
		// count blocks inside the new item's .3zh file
		blockCount, err := item.ItemCountBlocks(dstFilePath)
		if err != nil {
			fmt.Println("❌ POST /itemdrafts - cli.ItemCountBlocks:", err.Error())
			respondWithError(http.StatusInternalServerError, "", w)
			return
		}
		// update the item being created with its block count
		newItem.BlockCount = blockCount
	}

	// save the item in the database
	err = newItem.Save(dbClient)
	if err != nil {
		fmt.Println("❌ POST /itemdrafts - newItem.Save:", err.Error())
		// remove created file
		_ = os.Remove(dstFilePath)
		respondWithError(http.StatusInternalServerError, "", w)
		return
	}

	// --------------------------------------------------
	// Refresh data in Search Engine
	// --------------------------------------------------
	{
		err := searchEngineUpsertItem(newItem)
		if err != nil {
			fmt.Println("🔍❌ POST /itemdrafts : failed to update search engine")
		} else {
			fmt.Println("🔍✅ POST /itemdrafts : updated search engine")
		}
	}
	// --------------------------------------------------

	respondWithJSON(http.StatusOK, newItem, w)
}

// For now, it always return models in the 3zh format.
func getItemModel(w http.ResponseWriter, r *http.Request) {
	// get URL path element values
	itemRepo := chi.URLParam(r, "repo")
	itemName := chi.URLParam(r, "name")
	itemID := chi.URLParam(r, "id")

	// get header values
	etag := r.Header.Get("If-None-Match")

	getItemModelFile(itemRepo, itemName, itemID, etag, CACHE_ITEMS_3ZH_MAX_AGE, w, r)
}

func postItemModel(w http.ResponseWriter, r *http.Request) {
	funcName := "postItemModel"

	// retrieve user sending the request
	usr, err := getUserFromRequestContext(r)
	if err != nil {
		respondWithError(http.StatusInternalServerError, "", w)
		return
	}

	dbClient, err := dbclient.SharedUserDB()
	if err != nil {
		respondWithError(http.StatusInternalServerError, "", w)
		return
	}

	itemRepo := chi.URLParam(r, "repo")
	itemName := chi.URLParam(r, "name")
	itemID := chi.URLParam(r, "id")

	// ------------------------------------
	// Request validation
	// ------------------------------------

	// we expect either an [ID] or [Repo + Name]
	// Name must not be a name reserved for a body part ("head", "body", ...)
	if itemID == "" && (itemRepo == "" || itemName == "" || item.IsItemNameForBodyPart(itemName) || item.IsItemNameForEquipment(itemName)) {
		respondWithError(http.StatusBadRequest, "", w)
		return
	}

	// TODO: maybe check the byte buffer looks like a 3zh file

	// ------------------------------------
	// Check the item in the request already exists
	// ------------------------------------

	var itemFromDB *item.Item = nil

	if itemID != "" {
		// If request provides an item ID, we use it and ignore the [repo] and [name] fields.
		// Get item info from database, by ID.
		itemFromDB, err = item.GetByID(dbClient, itemID, "")
	} else {
		// If the request doesn't provide an item ID, we use the [repo] and [name] fields.
		// Get item info from database, by Repo + Name.
		var itemsFromDB []*item.Item = nil
		itemsFromDB, err = item.GetByRepoAndName(dbClient, itemRepo, itemName)
		// /!\ we only keep the first item returned
		if err == nil && len(itemsFromDB) > 0 {
			itemFromDB = itemsFromDB[0]
		}
	}

	if err != nil {
		fmt.Println("[" + funcName + "] item not found (1)")
		respondWithError(http.StatusBadRequest, "", w)
		return
	}

	if itemFromDB == nil {
		fmt.Println("[" + funcName + "] item not found (2)")
		respondWithError(http.StatusBadRequest, "", w)
		return
	}

	// Item has been found.
	// Now check the item belongs to the user

	if itemFromDB.Author != usr.ID {
		fmt.Println("[" + funcName + "] item doesn't belong to user")
		respondWithError(http.StatusBadRequest, "", w)
		return
	}

	// construct absolute path of .3zh file
	cubzhFilePath := filepath.Join(ITEMS_STORAGE_PATH, itemFromDB.Repo, itemFromDB.Name+".3zh")
	etagPath := filepath.Join(ITEMS_STORAGE_PATH, itemFromDB.Repo, itemFromDB.Name+".3zh.etag")

	// write file
	{
		err = writeFile(r.Body, cubzhFilePath, 0666)
		if err != nil {
			fmt.Println("ERR:", err.Error())
			fmt.Println("[" + funcName + "] failed to write 3zh file")
			respondWithError(http.StatusInternalServerError, "", w)
			return
		}
		_ = os.Remove(etagPath) // remove etag
	}

	// count blocks in .3zh file and update value in DB
	{
		// /cubzh_cli blocks -i /<ITEMS_STORAGE_PATH>/gaetan/foo10.3zh
		cmd := exec.Command("/cubzh_cli", "blocks", "-i", cubzhFilePath)
		var outBuf bytes.Buffer
		// var errBuf bytes.Buffer
		cmd.Stdout = &outBuf
		// cmd.Stderr = &errBuf
		err := cmd.Run() // runs the command
		if err != nil {
			fmt.Println("❌ failed to count blocks in .3zh (1):", err.Error())
		} else {
			blockCount, err := strconv.ParseUint(outBuf.String(), 10, 32)
			if err != nil {
				fmt.Println("❌ failed to count blocks in .3zh (2):", err.Error())
			} else {
				itemFromDB.BlockCount = uint32(blockCount)
				err = itemFromDB.Save(dbClient)
				if err != nil {
					fmt.Println("❌ failed to count blocks in .3zh (3):", err.Error())
				}
			}
		}
	}

	// --------------------------------------------------
	// Refresh data in Search Engine
	// --------------------------------------------------
	{
		err := searchEngineUpsertItem(itemFromDB)
		if err != nil {
			fmt.Println("🔍❌ /item/{repo}/{name}/model : failed to update search engine")
		} else {
			fmt.Println("🔍✅ /item/{repo}/{name}/model : updated search engine")
		}
	}
	// --------------------------------------------------

	respondOK(w)
}

// POST /items/file/{fileType}
// Posts a file that will then in most cases be attached to an item (3D model, texture, sound, etc.)
func postItemFile(w http.ResponseWriter, r *http.Request) {
	fmt.Println("🤓 POST /items/file/{fileType}")

	_, err := getUserFromRequestContext(r)
	if err != nil {
		respondWithError(http.StatusInternalServerError, "", w)
		return
	}

	// Extract fileType from URL parameter
	fileType := chi.URLParam(r, "fileType")

	fmt.Println("🤓 POST /items/file/{fileType} - fileType:", fileType)

	supportedFileTypes := []string{
		"model", // textured 3D model
		// "texture", // texture
		// "sound",   // sound
	}

	if slices.Contains(supportedFileTypes, fileType) == false {
		respondWithError(http.StatusBadRequest, "invalid file type", w)
		return
	}

	if fileType == "model" {
		maxSizeInMB := 10
		maxSizeInBytes := maxSizeInMB * 1024 * 1024

		prefix := "item_file_"

		// Create a temporary file
		tmpFile, err := os.CreateTemp("", prefix+"*")
		if err != nil {
			respondWithError(http.StatusInternalServerError, "failed to create temporary file", w)
			return
		}

		// Get the generated filename without the prefix
		generatedName := filepath.Base(tmpFile.Name())
		generatedName = strings.TrimPrefix(generatedName, prefix)
		fmt.Printf("Generated filename: %s\n", generatedName)

		// Read from request body and write to temporary file with size checking
		buffer := make([]byte, 1024) // 1KB buffer for reading
		totalBytesRead := 0

		for {
			n, err := r.Body.Read(buffer)
			if n > 0 {
				totalBytesRead += n
				if totalBytesRead > maxSizeInBytes {
					respondWithError(http.StatusBadRequest, "file size is too large", w)
					tmpFile.Close()
					os.Remove(tmpFile.Name())
					return
				}

				// Write the chunk to the temporary file
				_, writeErr := tmpFile.Write(buffer[:n])
				if writeErr != nil {
					respondWithError(http.StatusInternalServerError, "failed to write to temporary file", w)
					tmpFile.Close()
					os.Remove(tmpFile.Name())
					return
				}
			}
			if err == io.EOF {
				break
			}
			if err != nil {
				respondWithError(http.StatusInternalServerError, "failed to read request body", w)
				tmpFile.Close()
				os.Remove(tmpFile.Name())
				return
			}
		}

		// Ensure all data is written to disk
		if err := tmpFile.Sync(); err != nil {
			respondWithError(http.StatusInternalServerError, "failed to sync temporary file", w)
			tmpFile.Close()
			os.Remove(tmpFile.Name())
			return
		}
		tmpFile.Close()

		fmt.Printf("✅ File uploaded successfully: %s, size: %d bytes\n", tmpFile.Name(), totalBytesRead)

		// TODO: check file content (if type is "model", should be a .glb, etc.)

		type PostItemFileResp struct {
			Filename string `json:"filename,omitempty"`
		}

		resp := &PostItemFileResp{
			Filename: generatedName,
		}

		respondWithJSON(http.StatusOK, resp, w)
	}

	respondOK(w)
}

func getItemFile(w http.ResponseWriter, r *http.Request) {
	fileName := chi.URLParam(r, "fileName")
	filePath := filepath.Join(os.TempDir(), fileName)

	// Check if file exists
	if _, err := os.Stat(filePath); os.IsNotExist(err) {
		respondWithError(http.StatusNotFound, "file not found", w)
		return
	}

	// Open the file
	file, err := os.Open(filePath)
	if err != nil {
		respondWithError(http.StatusInternalServerError, "failed to open file", w)
		return
	}
	defer file.Close()

	// Set Content-Type based on file extension
	contentType := mime.TypeByExtension(filepath.Ext(filePath))
	if contentType == "" {
		contentType = "application/octet-stream"
	}
	w.Header().Set("Content-Type", contentType)

	w.WriteHeader(http.StatusOK)
	_, _ = io.Copy(w, file)
}

// PATCH /itemdrafts/:id
func itemdraftsPatch(w http.ResponseWriter, r *http.Request) {

	type itemdraftPatchReq struct {
		Description *string `json:"description,omitempty"`
		Archived    *bool   `json:"archived,omitempty"`
	}

	// Parse URL path
	itemID := chi.URLParam(r, "id")

	// retrieve user sending the request
	usr, err := getUserFromRequestContext(r)
	if err != nil {
		respondWithError(http.StatusInternalServerError, "", w)
		return
	}

	// shared Hub DB client
	dbClient, err := dbclient.SharedUserDB()
	if err != nil {
		respondWithError(http.StatusInternalServerError, "", w)
		return
	}

	if itemID == "" {
		respondWithError(http.StatusBadRequest, "item ID is missing", w)
		return
	}

	// get item info from database
	itm, err := item.GetByID(dbClient, itemID, "")
	if err != nil {
		respondWithError(http.StatusBadRequest, "item not found", w)
		return
	}

	// check if item is owned by the user
	if itm.Author != usr.ID {
		respondWithError(http.StatusBadRequest, "item not owned by user", w)
		return
	}

	// now, we can start applying changes

	var req itemdraftPatchReq
	err = json.NewDecoder(r.Body).Decode(&req)
	if err != nil {
		respondWithError(http.StatusBadRequest, "can't decode json request", w)
		return
	}

	// Field : description
	if req.Description != nil {
		newDescription := *req.Description
		// if req.Description != "" && req.ClearDescription == false {
		regex, err := regexp.Compile(DESCRIPTION_REGEX)
		if err != nil {
			respondWithError(http.StatusInternalServerError, "failed to compile regexp", w)
			return
		}
		if regex.MatchString(newDescription) == false {
			respondWithError(http.StatusBadRequest, "item description is invalid", w)
			return
		}
		itm.Description = newDescription
	}

	// Field : archived
	if req.Archived != nil {
		itm.Archived = *req.Archived
	}

	err = itm.Save(dbClient)
	if err != nil {
		respondWithError(http.StatusInternalServerError, "", w)
		return
	}

	// --------------------------------------------------
	// Refresh data in Search Engine
	// --------------------------------------------------
	{
		err := searchEngineUpsertItem(itm)
		if err != nil {
			fmt.Println("🔍❌ /item-save : failed to update search engine")
		} else {
			fmt.Println("🔍✅ /item-save : updated search engine")
		}
	}
	// --------------------------------------------------

	respondWithJSON(http.StatusOK, itm, w)
}

// DELETE /itemdrafts/:id
func itemdraftsDelete(w http.ResponseWriter, r *http.Request) {

	// Parse URL path
	itemID := chi.URLParam(r, "id")

	// fmt.Println("🤓  DELETE /itemdrafts/:id")

	// shared Hub DB client
	dbClient, err := dbclient.SharedUserDB()
	if err != nil {
		respondWithError(http.StatusInternalServerError, "", w)
		return
	}

	usr, err := getUserFromRequestContext(r)
	if err != nil {
		respondWithError(http.StatusInternalServerError, "", w)
		return
	}

	// TODO: gaetan: validate `itemID` (must look like a UUID)
	if itemID == "" {
		fmt.Println("❌ DELETE /itemdrafts/{id} item ID is missing")
		respondWithError(http.StatusBadRequest, "item ID is missing", w)
		return
	}

	// get item info from database
	itm, err := item.GetByID(dbClient, itemID, "")
	if err != nil {
		fmt.Println("❌ DELETE /itemdrafts/{id} item not found")
		respondWithError(http.StatusBadRequest, "item not found", w)
		return
	}

	// check if item is owned by the user
	if itm.Author != usr.ID {
		fmt.Println("❌ DELETE /itemdrafts/{id} item is not owned by user")
		respondWithError(http.StatusBadRequest, "item not owned by user", w)
		return
	}

	// now, we can start the deletion

	// search engine client
	searchClient, err := search.NewClient(CUBZH_SEARCH_SERVER, CUBZH_SEARCH_APIKEY)
	if err != nil {
		fmt.Println("❌ DELETE /itemdrafts/{id} failed to create search engine client:", err.Error())
		respondWithError(http.StatusInternalServerError, "", w)
		return
	}

	// delete item from database and filesystem
	{
		err = itm.Delete(dbClient, item.ItemDeleteOpts{
			TypesenseClient: searchClient,
			ItemStoragePath: ITEMS_STORAGE_PATH,
		})
		if err != nil {
			fmt.Println("❌ DELETE /itemdrafts/{id} failed to delete item from database")
			respondWithError(http.StatusInternalServerError, "", w)
			return
		}
	}

	respondOK(w)
}

func getItemInfo(w http.ResponseWriter, r *http.Request) {

	// retrieve user sending the request
	usr, err := getUserFromRequestContext(r)
	if err != nil {
		respondWithError(http.StatusInternalServerError, "", w)
		return
	}

	// Parse URL path
	itemIDOrFullname := chi.URLParam(r, "idOrFullname")
	if itemIDOrFullname == "" {
		respondWithError(http.StatusBadRequest, "identifier is missing", w)
		return
	}

	isFullname := false
	isItemID := false
	itemRepo := ""
	itemName := ""

	{
		// determine whether it is a UUID v4 or an item fullname <repo>.<name>
		_, err := uuid.Parse(itemIDOrFullname)
		if err == nil {
			isItemID = true
		} else {
			itemRepo, itemName, err = parseItemFullname(itemIDOrFullname)
			if err == nil {
				isFullname = true
			}
		}
	}

	if isItemID == false && isFullname == false {
		respondWithError(http.StatusBadRequest, "failed to parse identifier", w)
		return
	}

	// shared Hub DB client
	dbClient, err := dbclient.SharedUserDB()
	if err != nil {
		respondWithError(http.StatusInternalServerError, "", w)
		return
	}

	var itm *item.Item = nil

	// get game info from database
	if isItemID {
		itm, err = item.GetByID(dbClient, itemIDOrFullname, usr.ID)
		if err != nil {
			respondWithError(http.StatusInternalServerError, "", w)
			return
		}
	} else if isFullname {
		itms, err := item.GetByRepoAndName(dbClient, itemRepo, itemName)
		if err != nil {
			respondWithError(http.StatusInternalServerError, "failed to find item", w)
			return
		}
		if len(itms) != 1 {
			respondWithError(http.StatusInternalServerError, "unexpected count of items", w)
			return
		}
		itm = itms[0]
	}

	if itm == nil {
		respondWithError(http.StatusNotFound, "", w)
		return
	}

	// Parse URL query params (fields to return)
	// and filter the fields to return.

	fieldsToReturn := r.URL.Query()["f"]

	// force field filtering
	itemV2 := newItemV2FromDBItem(itm, fieldsToReturn)

	if slices.Contains(fieldsToReturn, "authorName") {
		// get the author name from the DB
		author, err := user.GetByID(dbClient, itm.Author)
		if err == nil && author != nil {
			itemV2.AuthorName = author.Username
		}
	}

	respondWithJSON(http.StatusOK, itemV2, w)
}

func patchItemLikes(w http.ResponseWriter, r *http.Request) {
	type itemAddRemoveLikeReq struct {
		Value bool `json:"value"`
	}

	// retrieve user sending the request
	senderUsr, err := getUserFromRequestContext(r)
	if err != nil {
		respondWithError(http.StatusInternalServerError, "", w)
		return
	}

	itemID := chi.URLParam(r, "id")

	dbClient, err := dbclient.SharedUserDB()
	if err != nil {
		respondWithError(http.StatusInternalServerError, "", w)
		return
	}

	var req itemAddRemoveLikeReq
	err = json.NewDecoder(r.Body).Decode(&req)
	if err != nil {
		respondWithError(http.StatusBadRequest, "can't decode json request", w)
		return
	}

	if req.Value {
		err = item.AddLike(dbClient, itemID, senderUsr.ID)

		// notification
		go func() {
			itm, err := item.GetByID(dbClient, itemID, "")
			if err != nil || itm == nil {
				return
			}

			author, err := user.GetByID(dbClient, itm.Author)
			if err != nil || author == nil {
				return
			}

			// create notification
			category := NOTIFICATION_CATEGORY_LIKE
			title := "❤️ ITEM LIKED!"
			username := senderUsr.Username
			if len(username) == 0 {
				username = user.DefaultUsername
			}
			message := username + " liked your item! (" + itm.Name + ")"
			notificationID, badgeCount, err := scyllaClientUniverse.CreateUserNotification(author.ID, category, title, message)
			if err == nil {
				// send push notification
				sendPushNotification(author, category, title, message, notificationID, badgeCount)
			}
		}()

	} else {
		err = item.RemoveLike(dbClient, itemID, senderUsr.ID)
	}
	if err != nil {
		respondWithError(http.StatusBadRequest, "", w)
		return
	}

	// --------------------------------------------------
	// Refresh data in Search Engine
	// --------------------------------------------------
	{
		// TODO: gaetan: we should not need to get the item from the database here,
		// and instead, we should perform a partial update of the item in the search engine.
		// (it is not supported by the search engine yet)
		itm, err := item.GetByID(dbClient, itemID, senderUsr.ID)
		if err != nil {
			fmt.Println("🔍❌ PATCH /items/{id}/likes : failed to update search engine (1):", err.Error())
			return
		}
		err = searchEngineUpsertItem(itm)
		if err != nil {
			fmt.Println("🔍❌ PATCH /items/{id}/likes : failed to update search engine (2):", err.Error())
		}
	}
	// --------------------------------------------------

	respondOK(w)
}

// ------------
// UTILS
// ------------

// func debug(w http.ResponseWriter, r *http.Request) {
// 	// msgUUID, senderUUID, content, serverID
// 	msgUUID := "d1b1b1b1-1b1b-1b1b-1b1b-1b1b1b1b1b1b"
// 	senderUUID := "d1b1b1b1-1b1b-1b1b-1b1b-1b1b1b1b1b1b"
// 	content := "Hello, World!"
// 	serverID := "sg.1.myGame"
// 	err := scyllaClientModeration.InsertChatMessage(msgUUID, senderUUID, content, serverID)
// 	if err != nil {
// 		respondWithString(http.StatusOK, err.Error(), w)
// 		return
// 	}
// 	respondWithString(http.StatusOK, "OK", w)
// }

func ping(w http.ResponseWriter, r *http.Request) {
	type pingRes struct {
		Message string `json:"msg,omitempty"`
	}
	var res pingRes
	res.Message = "pong"
	respondWithJSON(http.StatusOK, &res, w)
}

func getMinVersion(w http.ResponseWriter, r *http.Request) {

	type getMinVersionResp struct {
		Version string `json:"version,omitempty"`
	}

	resp := &getMinVersionResp{
		Version: CLIENT_MIN_VERSION,
	}

	respondWithJSON(http.StatusOK, resp, w)
}

func postSecret(w http.ResponseWriter, r *http.Request) {

	type secretRequest struct {
		Secret string `json:"secret"`
	}

	type secretResponse struct {
		Message string `json:"message"`
	}

	// shared Hub DB client
	dbClient, err := dbclient.SharedUserDB()
	if err != nil {
		respondWithError(http.StatusInternalServerError, "", w)
		return
	}

	usr, err := getUserFromRequestContext(r)
	if err != nil {
		respondWithError(http.StatusInternalServerError, "", w)
		return
	}

	var req secretRequest
	err = json.NewDecoder(r.Body).Decode(&req)
	if err != nil {
		respondWithError(http.StatusBadRequest, "can't decode json request", w)
		return
	}

	if len(req.Secret) == 0 {
		respondWithError(http.StatusBadRequest, "secret is empty", w)
		return
	}

	secret := req.Secret

	if secret == "aypierre" || secret == "pioneer" || secret == "dustzh" {

		if usr.Tags == nil {
			usr.Tags = make(map[string]bool)
		}
		if usr.Tags[secret] == true {
			resp := &secretResponse{
				Message: "Nice try! 😬",
			}
			respondWithJSON(http.StatusOK, resp, w)
			return
		}
		usr.Tags[secret] = true
		err = usr.Save(dbClient)
		if err != nil {
			respondWithError(http.StatusInternalServerError, "", w)
			return
		}
	} else {
		respondWithError(http.StatusBadRequest, "", w)
		return
	}

	msg := ""
	if secret == "aypierre" {
		msg = "609 💰 for watching Aypierre! 🙂"
	} else if secret == "pioneer" {
		msg = "200 💰 for following the newsletter"
	} else if secret == "dustzh" {
		msg = "200 💰 for you. See you on Discord."
	}

	resp := &secretResponse{
		Message: msg,
	}
	respondWithJSON(http.StatusOK, resp, w)
}

// router.Get("/tmpUsersWithTag", tmpUsersWithTag)
func tmpUsersWithTag(w http.ResponseWriter, r *http.Request) {

	// retrieve user ID provided in the URL path
	tag := chi.URLParam(r, "tag")

	dbClient, err := dbclient.SharedUserDB()
	if err != nil {
		respondWithError(http.StatusInternalServerError, "", w)
		return
	}

	allUsers, err := user.GetAll(dbClient)
	if err != nil {
		respondWithError(http.StatusInternalServerError, "failed to get users from DB", w)
		return
	}

	matchingUsersCount := 0

	for _, usr := range allUsers {
		if usr.Tags[tag] == true {
			matchingUsersCount += 1
		}
	}

	type Response struct {
		Count uint `json:"count"`
	}

	resp := Response{
		Count: uint(matchingUsersCount),
	}

	respondWithJSON(http.StatusOK, resp, w)
}

// --------------------------------------------------
// Friend Requests
// --------------------------------------------------

// createFriendRequest creates a friend request
func createFriendRequest(w http.ResponseWriter, r *http.Request) {

	// retrieve user sending the request
	usr, err := getUserFromRequestContext(r)
	if err != nil {
		respondWithError(http.StatusInternalServerError, "", w)
		return
	}

	// parse request's body
	var req FriendRequest
	err = json.NewDecoder(r.Body).Decode(&req)
	if err != nil {
		respondWithError(http.StatusBadRequest, errRequestDecodingFailed, w)
		return
	}

	// check that the userID of the request sender matches the "SenderID" field of the request body
	if usr.ID != req.SenderID {
		respondWithError(http.StatusBadRequest, "sender ID doesn't match user ID", w)
		return
	}

	// check that the friend request "sender" and "recipient" fields have different values
	if req.SenderID == req.RecipientID {
		respondWithError(http.StatusBadRequest, "sender ID & recipient ID cannot be identical", w)
		return
	}

	dbClient, err := dbclient.SharedUserDB()
	if err != nil {
		return
	}

	// retrieve from DB, the user who is the recipient of the request
	recipientUsr, err := user.GetByID(dbClient, req.RecipientID)
	if err != nil || recipientUsr == nil {
		respondWithError(http.StatusInternalServerError, err.Error(), w)
		return
	}

	// ScyllaDB
	scyllaResult, err := scyllaClientUniverse.InsertFriendRequest(usr.ID, usr.Username, recipientUsr.ID, recipientUsr.Username)
	if err != nil {
		respondWithError(http.StatusInternalServerError, err.Error(), w)
		return
	}

	if scyllaResult == scyllaclient.FriendRequestCreated {
		// send Push notification to recipient user
		go func() {
			// create notification
			category := NOTIFICATION_CATEGORY_SOCIAL
			title := "💛 FRIEND REQUEST!"
			username := usr.Username
			if len(username) == 0 {
				username = user.DefaultUsername
			}
			message := username + " just sent a friend request your way!"
			notificationID, badgeCount, err := scyllaClientUniverse.CreateUserNotification(recipientUsr.ID, category, title, message)
			if err == nil {
				// send push notification
				sendPushNotification(recipientUsr, category, title, message, notificationID, badgeCount)
			}
		}()
	} else if scyllaResult == scyllaclient.FriendRequestConvertedIntoFriendRelation {
		// send Push notification to recipient user, telling him
		// that his friend request has been accepted by <name>
		go func() {
			// create notification
			category := NOTIFICATION_CATEGORY_SOCIAL
			title := "💛 NEW FRIEND!"
			username := usr.Username
			if len(username) == 0 {
				username = user.DefaultUsername
			}
			message := username + " accepted your friend request!"
			notificationID, badgeCount, err := scyllaClientUniverse.CreateUserNotification(recipientUsr.ID, category, title, message)
			if err == nil {
				// send push notification
				sendPushNotification(recipientUsr, category, title, message, notificationID, badgeCount)
			}
		}()
	}

	// TODO: gaetan: respond differently to the client, based on scyllaResult's value
	respondOK(w)
}

// cancelFriendRequest cancels a friend request sent by the user and that is still pending
// (ie. not yet accepted or declined)
func cancelFriendRequest(w http.ResponseWriter, r *http.Request) {

	// retrieve user sending the request
	usr, err := getUserFromRequestContext(r)
	if err != nil {
		respondWithError(http.StatusInternalServerError, "", w)
		return
	}

	// parse request's body
	var req FriendRequest
	err = json.NewDecoder(r.Body).Decode(&req)
	if err != nil {
		respondWithError(http.StatusBadRequest, errRequestDecodingFailed, w)
		return
	}

	// check that the userID of the request sender matches the "SenderID" field of the request body
	if usr.ID != req.SenderID {
		respondWithError(http.StatusBadRequest, "sender ID doesn't match user ID", w)
		return
	}

	// check that the friend request "sender" and "recipient" fields have different values
	if req.SenderID == req.RecipientID {
		respondWithError(http.StatusBadRequest, "sender ID & recipient ID cannot be identical", w)
		return
	}

	// ScyllaDB
	err = scyllaClientUniverse.DeleteFriendRequest(req.SenderID, req.RecipientID)
	if err != nil {
		respondWithError(http.StatusInternalServerError, "", w)
		return
	}

	respondOK(w)
}

// replyToFriendRequest handles the user's acceptance or rejection of a received friend request.
func replyToFriendRequest(w http.ResponseWriter, r *http.Request) {

	type friendRequestReplyReq struct {
		SenderID string `json:"senderID,omitempty"` // user having sent the friend request
		Accept   bool   `json:"accept,omitempty"`   // whether the friend request is accepted
	}

	// retrieve user sending the request
	usr, err := getUserFromRequestContext(r)
	if err != nil {
		respondWithError(http.StatusInternalServerError, "", w)
		return
	}

	// parse request's body
	var req friendRequestReplyReq
	err = json.NewDecoder(r.Body).Decode(&req)
	if err != nil {
		respondWithError(http.StatusBadRequest, errRequestDecodingFailed, w)
		return
	}

	// check that the friend request "sender" and "recipient" fields have different values
	if req.SenderID == usr.ID {
		respondWithError(http.StatusBadRequest, "user cannot reply to friend request sent by himself", w)
		return
	}

	if req.Accept {

		// ScyllaDB
		err := scyllaClientUniverse.AcceptFriendRequest(req.SenderID, usr.ID)
		if err != nil {
			respondWithError(http.StatusInternalServerError, "", w)
			return
		}

		go func() {
			dbClient, err := dbclient.SharedUserDB()
			if err != nil {
				return
			}

			friendReqSender, err := user.GetByID(dbClient, req.SenderID)
			if err != nil || friendReqSender == nil {
				return
			}

			// create notification
			category := NOTIFICATION_CATEGORY_SOCIAL
			title := "💛 NEW FRIEND!"
			username := usr.Username
			if len(username) == 0 {
				username = user.DefaultUsername
			}
			message := username + " accepted your friend request!"
			notificationID, badgeCount, err := scyllaClientUniverse.CreateUserNotification(friendReqSender.ID, category, title, message)
			if err == nil {
				// send push notification
				sendPushNotification(friendReqSender, category, title, message, notificationID, badgeCount)
			}
		}()

	} else {

		// ScyllaDB
		err := scyllaClientUniverse.DeleteFriendRequest(req.SenderID, usr.ID)
		if err != nil {
			respondWithError(http.StatusInternalServerError, "", w)
			return
		}
	}

	respondOK(w) // success
}

// add friends' usernames info
type Friend struct {
	UserID   string
	Username string
}

type FriendSlice []Friend

func (fs FriendSlice) Len() int {
	return len(fs)
}

func (fs FriendSlice) Less(d int, e int) bool {
	t := strings.Map(unicode.ToUpper, fs[d].Username)
	u := strings.Map(unicode.ToUpper, fs[e].Username)
	return t < u
}

func (fs FriendSlice) Swap(i, j int) {
	var tmp Friend = fs[i]
	fs[i] = fs[j]
	fs[j] = tmp
}

// TODO: gaetan: stop using this route and remove it
// it should be replaced by "getUserFriendRelations" function
func getFriendRelations(w http.ResponseWriter, r *http.Request) {

	// retrieve user sending the request
	usr, err := getUserFromRequestContext(r)
	if err != nil {
		respondWithError(http.StatusInternalServerError, "", w)
		return
	}

	// ScyllaDB
	relations, err := scyllaClientUniverse.GetFriendRelations(usr.ID)
	if err != nil {
		respondWithError(http.StatusInternalServerError, "failed to get friend relations", w)
		return
	}
	friendRelationsUserIDs := make([]string, len(relations))
	for i, rel := range relations {
		friendRelationsUserIDs[i] = rel.ID2.String()
	}

	respondWithJSON(http.StatusOK, friendRelationsUserIDs, w)
}

// ------------------------------
// Users
// ------------------------------

// GET /users
// Query parameters:
// - `username` string
func getUsers(w http.ResponseWriter, r *http.Request) {

	dbClient, err := dbclient.SharedUserDB()
	if err != nil {
		respondWithError(http.StatusInternalServerError, "", w)
		return
	}

	// query parameter names
	const QUERY_PARAM_USERNAME string = "username"
	const QUERY_PARAM_RECENTLY_PUBLISHED string = "recentlyPublished"

	// Parse query parameters

	var queryParams url.Values = r.URL.Query()

	// "username" query param is provided
	// -> return the user info for the given username or HTTP 404 if not found

	if usernameValues, found := queryParams[QUERY_PARAM_USERNAME]; found {
		if len(usernameValues) != 1 {
			respondWithError(http.StatusBadRequest, "expected 1 value for query param 'username'", w)
			return
		}
		usernameValue := usernameValues[0]

		usr, err := user.GetByUsername(dbClient, usernameValue)
		if err != nil {
			respondWithError(http.StatusInternalServerError, "failed to look for user", w)
			return
		}
		if usr == nil {
			respondWithError(http.StatusNotFound, "user not found", w)
			return
		}

		reply := &userInfoMinimal{
			ID:       usr.ID,
			Username: usr.Username,
		}

		respondWithJSON(http.StatusOK, reply, w)
	}

	// "recentlyPublished" query param is provided
	// -> return the users who published the recently published worlds and items

	if recentlyPublishedValues, found := queryParams[QUERY_PARAM_RECENTLY_PUBLISHED]; found {
		if len(recentlyPublishedValues) != 1 {
			respondWithError(http.StatusBadRequest, "expected 1 value for query param 'recentlyPublished'", w)
			return
		}
		recentlyPublishedValue := recentlyPublishedValues[0] == "true"
		if !recentlyPublishedValue {
			respondWithError(http.StatusBadRequest, "query param 'recentlyPublished' must be true or not provided", w)
			return
		}

		// get recently published worlds
		worlds, err, _ := game.List(dbClient, "list:recent", "")
		if err != nil {
			fmt.Println("failed to get recently published worlds", err)
			respondWithError(http.StatusInternalServerError, "failed to get recently published worlds", w)
			return
		}

		// get recently published items
		items, err := item.List(dbClient, "list:recent", "")
		if err != nil {
			fmt.Println("failed to get recently published items", err)
			respondWithError(http.StatusInternalServerError, "failed to get recently published items", w)
			return
		}

		// get users who published the worlds and items

		// Track users and their most recent creation update time
		userCreationTimes := make(map[string]time.Time)

		for _, world := range worlds {
			if world.Author == "" {
				continue
			}
			if existingTime, exists := userCreationTimes[world.Author]; !exists || world.Updated.After(existingTime) {
				userCreationTimes[world.Author] = world.Updated
			}
		}
		for _, item := range items {
			if item.Author == "" {
				continue
			}
			if existingTime, exists := userCreationTimes[item.Author]; !exists || item.Updated.After(existingTime) {
				userCreationTimes[item.Author] = item.Updated
			}
		}

		// Convert to slice for sorting
		type userWithTime struct {
			userID      string
			lastUpdated time.Time
		}

		userList := make([]userWithTime, 0, len(userCreationTimes)) // length is 0 but capacity has the number of users
		for userID, lastUpdated := range userCreationTimes {
			userList = append(userList, userWithTime{
				userID:      userID,
				lastUpdated: lastUpdated,
			})
		}

		// Sort by most recent creation update (newest first)
		slices.SortFunc(userList, func(a, b userWithTime) int {
			return a.lastUpdated.Compare(b.lastUpdated) * -1 // reverse order
		})

		// get users in sorted order
		users := make([]userInfo, len(userList))
		for i, userWithTime := range userList {
			usr, err := user.GetByID(dbClient, userWithTime.userID)
			if err == nil {
				users[i].populate(*usr, nil, false, nil)
			}
		}

		respondWithJSON(http.StatusOK, users, w)
	}

	respondWithError(http.StatusBadRequest, "missing query parameters", w)
}

func postReports(w http.ResponseWriter, r *http.Request) {

	type reportReq struct {
		ItemID  string `json:"itemID,omitempty"`
		WorldID string `json:"worldID,omitempty"`
		Message string `json:"message,omitempty"`
	}

	// retrieve user sending the request
	sender, err := getUserFromRequestContext(r)
	if err != nil {
		respondWithError(http.StatusInternalServerError, "", w)
		return
	}

	var req reportReq
	err = json.NewDecoder(r.Body).Decode(&req)
	if err != nil {
		respondWithError(http.StatusBadRequest, errRequestDecodingFailed, w)
		return
	}

	dbClient, err := dbclient.SharedUserDB()
	if err != nil {
		respondWithError(http.StatusInternalServerError, "", w)
		return
	}

	var objectType string
	var objectName string
	var objectID string
	if req.ItemID != "" {
		objectType = "ITEM"
		item, err := item.GetByID(dbClient, req.ItemID, "")
		if err != nil {
			respondWithError(http.StatusInternalServerError, "", w)
			return
		}
		objectName = item.Repo + "." + item.Name
		objectID = req.ItemID
	} else if req.WorldID != "" {
		objectType = "WORLD"
		world, err := game.GetByID(dbClient, req.WorldID, "", nil)
		if err != nil {
			respondWithError(http.StatusInternalServerError, "", w)
			return
		}
		objectName = world.Title
		objectID = req.WorldID
	} else {
		discordClient.SendMessage(DISCORD_DASHBOARD_CHANNEL_ID, "🚨 Report from unknown object type")
		respondOK(w)
		return
	}

	// construct message string
	msg := fmt.Sprintf("🚨 %s: %s (%s)\nreported by: %s\nmessage: %s",
		objectType,
		objectName,
		objectID,
		sender.Username,
		req.Message)

	err = discordClient.SendMessage(DISCORD_DASHBOARD_CHANNEL_ID, msg)
	if err != nil {
		respondWithError(http.StatusInternalServerError, err.Error(), w)
		return
	}

	respondOK(w)
}

func getUserInfo(w http.ResponseWriter, r *http.Request) {

	// retrieve user sending the request
	usr, err := getUserFromRequestContext(r)
	if err != nil {
		respondWithError(http.StatusInternalServerError, "", w)
		return
	}

	// user for which we want to retrieve info
	var targetUsr *user.User = usr

	// retrieve user ID provided in the URL path
	userID := chi.URLParam(r, "id")
	if userID != "" && userID != usr.ID {
		dbClient, err := dbclient.SharedUserDB()
		if err != nil {
			respondWithError(http.StatusInternalServerError, "", w)
			return
		}

		usr, err := user.GetByID(dbClient, userID)
		if err != nil || usr == nil {
			respondWithError(http.StatusInternalServerError, "failed to get user from DB", w)
			return
		}

		targetUsr = usr
	}

	fields := r.URL.Query()["f"]
	elevatedRights := false

	res := userInfo{}
	res.populate(*targetUsr, fields, elevatedRights, scyllaClientUniverse)

	// if targetUsr == usr {
	// one of the server side events we're using to track MAUs
	// without relying on client side events. (not allowed by Apple)
	// When launching the app, /users/self to obtain user information
	// and check if credentials are valid.
	// This is a necessary checkpoint.
	// It only allows to track users with an account though.
	// trackEventForHTTPReq(&eventReq{UserID: usr.ID, Type: "GET_OWN_USER_INFO"}, r)
	// }

	respondWithJSON(http.StatusOK, res, w)
}

// getUserFriendRelations returns the list of friends of a user
func getUserFriendRelations(w http.ResponseWriter, r *http.Request) {

	// retrieve user sending the request
	usr, err := getUserFromRequestContext(r)
	if err != nil {
		respondWithError(http.StatusInternalServerError, "", w)
		return
	}

	dbClient, err := dbclient.SharedUserDB()
	if err != nil {
		respondWithError(http.StatusInternalServerError, "", w)
		return
	}

	// user for which we want to retrieve friend relations
	var targetUserID string = usr.ID

	// retrieve user ID provided in the URL path (if any)
	if userID := chi.URLParam(r, "id"); userID != "" && userID != targetUserID {
		targetUserID = userID
	}

	// get friend relations from ScyllaDB
	relations, err := scyllaClientUniverse.GetFriendRelations(targetUserID)
	if err != nil {
		respondWithError(http.StatusInternalServerError, "", w)
		return
	}
	friendRelationsUserIDs := make([]string, len(relations))
	for i, rel := range relations {
		friendRelationsUserIDs[i] = rel.ID2.String()
	}

	userInfoArr := make([]userInfo, len(friendRelationsUserIDs))
	fields, _ := r.URL.Query()["f"]
	elevatedRights := false

	for i, id := range friendRelationsUserIDs {
		usr, err := user.GetByID(dbClient, id)
		if err == nil {
			userInfoArr[i].populate(*usr, fields, elevatedRights, scyllaClientUniverse)
		}
	}

	respondWithJSON(http.StatusOK, userInfoArr, w)
}

// getUserFriendRelation ...
// GET /users/{id}/fiends/{friendID}
func getUserFriendRelation(w http.ResponseWriter, r *http.Request) {

	// retrieve user sending the request
	sender, err := getUserFromRequestContext(r)
	if err != nil {
		respondWithError(http.StatusInternalServerError, "", w)
		return
	}

	dbClient, err := dbclient.SharedUserDB()
	if err != nil {
		respondWithError(http.StatusInternalServerError, "", w)
		return
	}

	targetUsr := sender

	// retrieve user ID provided in the URL path
	userID := chi.URLParam(r, "id")
	if userID != "" && userID != sender.ID {
		// TODO: gaetan: no need to retrieve the full user info here, we only need the ID
		usr, err := user.GetByID(dbClient, userID)
		if err != nil || usr == nil {
			respondWithError(http.StatusInternalServerError, "failed to get user from DB", w)
			return
		}
		targetUsr = usr
	}

	// retrieve friend ID provided in the URL path
	friendID := chi.URLParam(r, "friendID")
	if friendID == "" {
		respondWithError(http.StatusBadRequest, "missing friend ID", w)
		return
	}

	// ScyllaDB
	relationPtr, err := scyllaClientUniverse.GetFriendRelation(targetUsr.ID, friendID)
	if err != nil {
		respondWithError(http.StatusInternalServerError, "", w)
		return
	}
	// DB request succeeded but no relation was found
	if relationPtr == nil {
		respondWithError(http.StatusNotFound, "", w)
		return
	}

	friendUser, err := user.GetByID(dbClient, friendID)
	if err != nil {
		respondWithError(http.StatusInternalServerError, "failed to get friend from DB", w)
		return
	}

	fields, _ := r.URL.Query()["f"]
	elevatedRights := false

	res := userInfo{}
	res.populate(*friendUser, fields, elevatedRights, scyllaClientUniverse)

	respondWithJSON(http.StatusOK, res, w)
}

func getUserFriendRequests(w http.ResponseWriter, r *http.Request) {

	const (
		QUERY_STATUS_KEY          string = "status"
		QUERY_STATUS_VAL_SENT     string = "sent"
		QUERY_STATUS_VAL_RECEIVED string = "received"
	)

	// retrieve user sending the request
	sender, err := getUserFromRequestContext(r)
	if err != nil {
		respondWithError(http.StatusInternalServerError, "", w)
		return
	}

	dbClient, err := dbclient.SharedUserDB()
	if err != nil {
		respondWithError(http.StatusInternalServerError, "", w)
		return
	}

	// by default, we retrieve the friend requests of the sender
	targetUsr := sender

	// if a userID is provided, then use it
	// userID := chi.URLParam(r, "id")
	// if userID != "" && userID != sender.ID {
	// 	usr, err := user.GetByID(dbClient, userID)
	// 	if err != nil || usr == nil {
	// 		respondWithError(http.StatusInternalServerError, "failed to get user from DB", w)
	// 		return
	// 	}
	// 	targetUsr = usr
	// }

	// Parse query parameters
	var queryParams url.Values = r.URL.Query() // map[string][]string

	//
	if queryParams.Has(QUERY_STATUS_KEY) == false {
		respondWithError(http.StatusBadRequest, "missing query parameters", w)
		return
	}
	status := queryParams.Get(QUERY_STATUS_KEY) // get 1st value

	requestUserIds := make([]string, 0)

	if status == QUERY_STATUS_VAL_SENT {

		// ScyllaDB
		relations, err := scyllaClientUniverse.GetFriendRequestsSent(targetUsr.ID)
		if err != nil {
			respondWithError(http.StatusInternalServerError, "", w)
			return
		}
		requestUserIds = make([]string, len(relations))
		for i, rel := range relations {
			requestUserIds[i] = rel.ID2.String()
		}

	} else if status == QUERY_STATUS_VAL_RECEIVED {

		// ScyllaDB
		relations, err := scyllaClientUniverse.GetFriendRequestsReceived(targetUsr.ID)
		if err != nil {
			respondWithError(http.StatusInternalServerError, "", w)
			return
		}
		requestUserIds = make([]string, len(relations))
		for i, rel := range relations {
			requestUserIds[i] = rel.ID2.String()
		}

	} else {
		respondWithError(http.StatusInternalServerError, "invalid status", w)
		return
	}

	users := make([]user.User, 0)
	for _, usrID := range requestUserIds {
		usr, err := user.GetByID(dbClient, usrID)
		if err == nil && usr != nil {
			users = append(users, *usr)
		}
	}

	// construct final array of userInfo
	userInfoArr := make([]userInfo, len(users))
	fields, _ := queryParams["f"]
	elevatedRights := false
	for i, usr := range users {
		userInfoArr[i].populate(usr, fields, elevatedRights, scyllaClientUniverse)
	}

	respondWithJSON(http.StatusOK, userInfoArr, w)
}

func deleteUser(w http.ResponseWriter, r *http.Request) {

	// retrieve user sending the request
	usr, err := getUserFromRequestContext(r)
	if err != nil {
		respondWithError(http.StatusInternalServerError, "", w)
		return
	}
	fmt.Println("👋 DELETING USER...", usr.Username)

	// Check if user is already flagged as deleted
	if usr.Deleted {
		fmt.Println("👋 USER ALREADY DELETED")
		respondWithError(http.StatusBadRequest, "user already deleted", w)
		return
	}

	// database client
	dbClient, err := dbclient.SharedUserDB()
	if err != nil {
		respondWithError(http.StatusInternalServerError, "", w)
		return
	}

	// Check if user has any item/world/like
	items, err := item.GetByRepo(dbClient, usr.Username)
	if err != nil {
		respondWithError(http.StatusInternalServerError, "", w)
		return
	}
	itemsCount := len(items)

	worlds, err := game.GetByAuthor(dbClient, usr.ID)
	if err != nil {
		respondWithError(http.StatusInternalServerError, "", w)
		return
	}
	worldsCount := len(worlds)

	itemLikes, err := item.GetAllLikesFromUser(dbClient, usr.ID)
	if err != nil {
		respondWithError(http.StatusInternalServerError, "", w)
		return
	}
	worldLikes, err := game.GetAllLikesFromUser(dbClient, usr.ID)
	if err != nil {
		respondWithError(http.StatusInternalServerError, "", w)
		return
	}
	likesCount := len(itemLikes) + len(worldLikes)

	if itemsCount == 0 && worldsCount == 0 && likesCount == 0 {
		// totally delete user
		fmt.Println("👋 TOTALLY DELETE USER")
		err = usr.Remove(dbClient)
		if err != nil {
			respondWithError(http.StatusInternalServerError, "", w)
			return
		}
	} else {
		// flag user as deleted and remove all personal info
		fmt.Println("👋 FLAG USER AS DELETED")
		usr.Deleted = true
		usr.DeleteReason = int(user.DeleteReasonUserDeletedOwnAccount)
		err = usr.Save(dbClient)
		if err != nil {
			respondWithError(http.StatusInternalServerError, "", w)
			return
		}
	}

	// remove all friend requests and relations mentioning this user
	err = scyllaClientUniverse.DeleteAllRelationsForUser(usr.ID)
	if err != nil {
		respondWithError(http.StatusInternalServerError, "", w)
		return
	}

	fmt.Println("👋 USER DELETION SUCCEEDED")
	respondOK(w)
}

func patchUserInfo(w http.ResponseWriter, r *http.Request) {

	// non-nil if email has been updated
	var emailVerifCode *string = nil
	// non-nil if parentEmail has been updated
	var parentEmailVerifCode *string = nil

	// retrieve user sending the request
	usr, err := getUserFromRequestContext(r)
	if err != nil {
		respondWithError(http.StatusInternalServerError, "", w)
		return
	}

	// database client
	dbClient, err := dbclient.SharedUserDB()
	if err != nil {
		respondWithError(http.StatusInternalServerError, "", w)
		return
	}

	// parse request
	var req userPatch
	jsonDecoder := json.NewDecoder(r.Body)
	jsonDecoder.DisallowUnknownFields()
	err = jsonDecoder.Decode(&req)
	if err != nil {
		respondWithError(http.StatusBadRequest, "can't decode json request", w)
		return
	}

	// Username (only allowed once, if anonymous)
	if req.Username != nil && req.UsernameKey != nil {

		// If user account already has a username, then return an error.
		// (we don't allow to change username once it is set)
		if len(usr.Username) > 0 {
			respondWithError(http.StatusBadRequest, "username already set", w)
			return
		}

		username := *req.Username
		usernameKey := *req.UsernameKey

		sanitizedUsername, err := user.SanitizeUsername(username)
		if err != nil {
			respondWithError(http.StatusBadRequest, "invalid username", w)
			return
		}

		key := utils.SaltedMD5ed(sanitizedUsername, USERNAME_CHECK_SALT)
		if key != usernameKey {
			respondWithError(http.StatusBadRequest, "wrong username key", w)
			return
		}

		available, err := user.IsUsernameAvailable(dbClient, sanitizedUsername)
		if err != nil {
			respondWithError(http.StatusInternalServerError, "", w)
			return
		}

		if available == false {
			respondWithError(http.StatusBadRequest, "username not available", w)
			return
		}

		usr.Username = username
	}

	// DateOfBirth
	if req.DateOfBirth != nil {
		// convert DoB from string (mm-dd-yyyy) to time.Time
		var dob string = *req.DateOfBirth
		if dob != "" {
			if usr.Dob == nil {
				dobTime, err := time.Parse("01-02-2006", dob)
				if err != nil {
					respondWithError(http.StatusBadRequest, "", w)
					return
				}

				// check the date of birth is not in the future
				if dobTime.After(time.Now()) {
					respondWithError(http.StatusBadRequest, "date of birth is in the future", w)
					return
				}

				usr.Dob = &dobTime
			} else {
				// user already has a date of birth
				// TODO: reply with an error?
			}
		}
	}

	// Bio
	if req.Bio != nil {
		newValue := *req.Bio
		if len(newValue) <= USER_BIO_MAX_CHARACTERS {
			usr.Bio = newValue
		}
	}

	// Discord
	if req.Discord != nil {
		usr.Discord = *req.Discord
	}

	// X
	if req.X != nil {
		usr.X = *req.X
	}

	// TikTok
	if req.Tiktok != nil {
		usr.Tiktok = *req.Tiktok
	}

	// Website
	if req.Website != nil {
		usr.Website = *req.Website
	}

	// Github
	if req.Github != nil {
		usr.Github = *req.Github
	}

	// Email
	if req.Email != nil {
		emailAddr := *req.Email

		if user.IsEmailValid(emailAddr) == false {
			respondWithError(http.StatusBadRequest, "invalid email", w)
			return
		}

		// Make sure the submitted email is not already used by another user
		{
			otherUsr, err := user.GetByEmail(dbClient, emailAddr)
			if err != nil {
				respondWithError(http.StatusInternalServerError, "", w)
				return
			}
			if otherUsr != nil {
				respondWithError(http.StatusBadRequest, "email already used", w)
				return
			}
		}

		// We don't need to check for parent email.
		// Here, we are updating the "email" field of the user. (not "parent email")

		// generate verification code
		verifCode, _ := user.GenerateNumericalCodeWithExpirationDate(12, 0)
		emailVerifCode = &verifCode

		usr.EmailTemporary = req.Email
		usr.EmailVerifCode = &verifCode
	}

	// ParentEmail
	if req.ParentEmail != nil {
		emailAddr := *req.ParentEmail

		if user.IsEmailValid(emailAddr) == false {
			respondWithError(http.StatusBadRequest, "invalid email", w)
			return
		}

		// Here we are updating the "parent email" field of the user.
		// The email value can already be used by other users (as main email or parent email).
		// (multiple <13yo accounts can share the same parent email)

		// generate verification code
		verifCode, _ := user.GenerateNumericalCodeWithExpirationDate(12, 0)
		parentEmailVerifCode = &verifCode

		usr.ParentEmailTemporary = req.ParentEmail
		usr.ParentEmailVerifCode = parentEmailVerifCode
	}

	// EstimatedDob
	if req.Age != nil {
		var age int32 = *req.Age
		// `age` allows to compute estimated date of birth,
		// considering `age` is provided on birthdate day.
		// Users currently can't provide an age greater than 41,
		// but it's ok, we're only storing estimated DOB to serve
		// appropriate content to young ones (9+, 13+, etc.)

		// estimatedDobTime = current date minus `age` years
		estimatedDobTime := time.Now().AddDate(-int(age), 0, 0)

		usr.EstimatedDob = &estimatedDobTime
	}

	// Phone number (own or parent's)
	if req.Phone != nil || req.ParentPhone != nil {
		isParentPhone := req.Phone == nil && req.ParentPhone != nil

		var phoneNumber string
		if isParentPhone {
			// setting parent's phone number
			// If user already has his own phone number set, then we refuse to update the parent phone number.
			if usr.Phone != nil && *usr.Phone != "" {
				respondWithError(http.StatusBadRequest, "cannot update parent phone number if user already has his phone number", w)
				return
			}
			phoneNumber = *req.ParentPhone
		} else {
			// setting own phone number
			phoneNumber = *req.Phone
		}

		fmt.Println("🐞 [PATCH USER] phone number:", phoneNumber)

		// Special number for testing
		if phoneNumber == TEST_PHONE_NUMBER {
			usr.IsTestAccount = true
			usr.IsVerifiedTestAccount = false
		} else {
			usr.IsTestAccount = false
			usr.IsVerifiedTestAccount = false

			// Sanitize phone number
			phoneNumber = SanitizePhoneNumberLocally(phoneNumber)
			countryCode := ""

			// Sanitize phone number with Twilio
			if phoneInfo, err := twilioSanitizePhoneNumberWithAPI(phoneNumber); err == nil {
				phoneNumber = phoneInfo.PhoneNumber
				countryCode = phoneInfo.CountryCode
			} else {
				fmt.Println("❌ failed to sanitize phone number with Twilio:", err.Error())
			}

			// Check if the phone number is already used by another user.
			// Multiple users can provide the same *parent* phone number,
			// but a *regular* phone number can only be used by one user.
			{
				phoneAssociatedUser, err := user.GetByPhone(dbClient, phoneNumber)
				if err != nil {
					fmt.Println("[PATCH USER INFO / PHONE] 1", err.Error())
					respondWithError(http.StatusInternalServerError, "failed to get user by phone", w)
					return
				}

				if isParentPhone {
					// User is providing his parent phone number

					if phoneAssociatedUser == nil {
						// If no user exists for the parent, then create one.

						parentUsr, err := user.New()
						if err != nil {
							fmt.Println("[PATCH USER INFO / PHONE] 2", err.Error())
							respondWithError(http.StatusInternalServerError, "failed to create parent user", w)
							return
						}
						parentUsr.PhoneNew = &phoneNumber
						err = parentUsr.Save(dbClient)
						if err != nil {
							fmt.Println("[PATCH USER INFO / PHONE] 3", err.Error())
							respondWithError(http.StatusInternalServerError, "failed to save parent user", w)
							return
						}
						phoneAssociatedUser = parentUsr

					} else if phoneAssociatedUser.ID == usr.ID {
						fmt.Println("[PATCH USER INFO / PHONE] 4", err.Error())
						respondWithError(http.StatusBadRequest, "parent phone cannot be the same as own phone", w)
						return
					}

					// Now that we made sure a user exists for the parent
					// we can associate the current user with this parent account.

					// associate current user with existing parent account
					parentID := phoneAssociatedUser.ID
					childID := usr.ID

					dashboard, secretKeyClear, err := scyllaClientUniverse.CreateUniverseParentDashboard(childID, parentID)
					if err != nil {
						fmt.Println("[PATCH USER INFO / PHONE] 7", err.Error())
						respondWithError(http.StatusInternalServerError, "failed to create parent dashboard", w)
						return
					}

					// Send text message to parent with dashboard link
					dashboardURL := fmt.Sprintf("https://dashboard.cu.bzh/%s/%s", dashboard.DashboardID, secretKeyClear)

					fmt.Println("🐞[PATCH USER INFO / PHONE][COUNTRY CODE]", countryCode)
					if countryCode == "US" {
						// use Twilio
						err = twilioSendDashboardSMSToParent(phoneNumber, dashboardURL)
					} else {
						// use Prelude
						err = preludeSendDashboardSMSToParent(phoneNumber, dashboardURL)
					}
					if err != nil {
						fmt.Println("🐞[PATCH USER INFO / PHONE][send parent SMS]", countryCode, err.Error())
						respondWithError(http.StatusInternalServerError, "failed to send dashboard SMS to parent", w)
						return
					}

					// Add parent's phone number to user record
					usr.ParentPhoneNew = &phoneNumber

				} else { // isParentPhone == false

					// User is providing his own phone number
					// https://github.com/ding-live/ding-golang/blob/main/models/components/createauthenticationrequest.go
					s := dinggolang.New(dinggolang.WithSecurity(DING_API_KEY))
					ctx := context.Background()
					clientIP := httpUtils.GetIP(r)
					isReturning := false
					res, err := s.Otp.CreateAuthentication(ctx, &dingcomponents.CreateAuthenticationRequest{
						CustomerUUID:    DING_API_CUSTOMER_UUID,
						PhoneNumber:     phoneNumber,
						IP:              &clientIP,
						IsReturningUser: &isReturning,
						// DeviceType
						// DeviceModel
						// OsVersion
						// DeviceID
						// AppVersion
						// AppRealm // The Android SMS Retriever API hash code that identifies your app. This allows you to automatically retrieve and fill the OTP code on Android devices.

					})
					if err != nil {
						fmt.Println("dinggolang error:", err.Error())
						respondWithError(http.StatusInternalServerError, err.Error(), w)
						return
					}

					// handle response
					if res.CreateAuthenticationResponse == nil {
						fmt.Println("dinggolang error: CreateAuthenticationResponse is nil")
						respondWithError(http.StatusInternalServerError, "failed to send phone verif code", w)
						return
					}
					authUUID := res.CreateAuthenticationResponse.AuthenticationUUID
					if authUUID == nil || *authUUID == "" {
						fmt.Println("dinggolang error: authUUID is empty")
						respondWithError(http.StatusInternalServerError, "failed to get auth UUID", w)
						return
					}

					// Store new phone number and the verification code.
					// (to allow for verification in a later API call)
					usr.PhoneNew = &phoneNumber
					usr.PhoneVerifCode = authUUID
				}
			}
		}
	}

	// Verif code sent to phone number
	if req.PhoneVerifCode != nil || req.ParentPhoneVerifCode != nil {
		isParentPhone := req.PhoneVerifCode == nil && req.ParentPhoneVerifCode != nil

		var phoneVerifCode string
		var userVerifCode string
		if isParentPhone {
			if req.ParentPhoneVerifCode != nil {
				phoneVerifCode = *req.ParentPhoneVerifCode
			}
			if usr.ParentPhoneVerifCode != nil {
				userVerifCode = *usr.ParentPhoneVerifCode
			}
		} else {
			if req.PhoneVerifCode != nil {
				phoneVerifCode = *req.PhoneVerifCode
			}
			if usr.PhoneVerifCode != nil {
				userVerifCode = *usr.PhoneVerifCode
			}
		}

		// Handle test accounts
		// If the phone number entered was <TEST_PHONE_NUMBER>, then the user is a test account.
		// In this case, the confirmation code is always <TEST_PHONE_VERIF_CODE>.
		if usr.IsTestAccount {

			if phoneVerifCode != TEST_PHONE_VERIF_CODE {
				respondWithError(http.StatusBadRequest, "wrong phone verif code", w)
				return
			}

			usr.IsVerifiedTestAccount = true

		} else { // user account is a regular account

			if isParentPhone {
				// usr.ParentPhoneNew must not be empty
				if usr.ParentPhoneNew == nil || *usr.ParentPhoneNew == "" {
					fmt.Println("new parent phone number not set")
					respondWithError(http.StatusBadRequest, "new parent phone number not set", w)
					return
				}
				// usr.ParentPhoneVerifCode must not be empty
				if usr.ParentPhoneVerifCode == nil || *usr.ParentPhoneVerifCode == "" {
					fmt.Println("parent phone verif code field empty")
					respondWithError(http.StatusBadRequest, "parent phone verif code field empty", w)
					return
				}
			} else {
				// usr.PhoneNew must not be empty
				if usr.PhoneNew == nil || *usr.PhoneNew == "" {
					fmt.Println("new phone number not set")
					respondWithError(http.StatusBadRequest, "new phone number not set", w)
					return
				}
				// usr.PhoneVerifCode must not be empty
				if usr.PhoneVerifCode == nil || *usr.PhoneVerifCode == "" {
					fmt.Println("phone verif code field empty")
					respondWithError(http.StatusBadRequest, "phone verif code field empty", w)
					return
				}
			}

			// verif code provided, update phone number
			s := dinggolang.New(dinggolang.WithSecurity(DING_API_KEY))
			var request *dingcomponents.CreateCheckRequest = &dingcomponents.CreateCheckRequest{
				CustomerUUID:       DING_API_CUSTOMER_UUID,
				AuthenticationUUID: userVerifCode,
				CheckCode:          phoneVerifCode,
			}
			ctx := context.Background()
			res, err := s.Otp.Check(ctx, request)
			if err != nil {
				fmt.Println("dinggolang error:", err.Error())
				respondWithError(http.StatusInternalServerError, "", w)
				return
			}
			// handle response
			if res.CreateCheckResponse == nil {
				fmt.Println("dinggolang error: CreateCheckResponse is nil")
				respondWithError(http.StatusInternalServerError, "", w)
				return
			}
			// dingcomponents.CheckStatusValid
			if res.CreateCheckResponse == nil || res.CreateCheckResponse.Status == nil || *res.CreateCheckResponse.Status != dingcomponents.CheckStatusValid {
				fmt.Println("☎️ phone number verification failed (", *res.CreateCheckResponse.Status, ")")
				respondWithError(http.StatusBadRequest, "wrong phone verif code", w)
				return
			}

			// Update user record in DB
			if isParentPhone {
				usr.ParentPhone = usr.ParentPhoneNew
				usr.ParentPhoneNew = nil
				usr.ParentPhoneVerifCode = nil
			} else {
				usr.Phone = usr.PhoneNew
				usr.PhoneNew = nil
				usr.PhoneVerifCode = nil
			}
		}
	}

	err = usr.Save(dbClient)
	if err != nil {
		fmt.Println("❌ failed to update user:", err.Error())
		respondWithError(http.StatusInternalServerError, "failed to update user", w)
		return
	}

	// Send emails if necessary

	if emailVerifCode != nil {

		var content struct {
			Email    string
			Username string
			Link     string
		}

		content.Username = usr.Username
		content.Email = *usr.EmailTemporary
		content.Link = "https://api.cu.bzh/users/" + usr.ID + "/email/verify?code=" + *emailVerifCode

		err = mailer.Send(
			*usr.EmailTemporary,
			"", // email address used as name, because we don't have it
			"./templates/email_generic.html",
			"./templates/email_newEmailAddressVerifCode.md",
			MAILJET_API_KEY,
			MAILJET_API_SECRET,
			&content,
			"cubzhEmailConfirmCode",
		)

		if err != nil {
			fmt.Println("mailer.Send error:", err.Error())
			respondWithError(http.StatusInternalServerError, "", w)
			return
		}
	}

	if parentEmailVerifCode != nil {

		var content struct {
			Email    string
			Username string
			Link     string
		}

		content.Username = usr.Username
		content.Email = *usr.ParentEmailTemporary
		content.Link = "https://api.cu.bzh/users/" + usr.ID + "/email/verify?code=" + *parentEmailVerifCode

		err = mailer.Send(
			*usr.ParentEmailTemporary,
			"", // email address used as name, because we don't have it
			"./templates/email_generic.html",
			"./templates/email_newParentEmailAddressVerifCode.md",
			MAILJET_API_KEY,
			MAILJET_API_SECRET,
			&content,
			"cubzhEmailConfirmCode",
		)

		if err != nil {
			fmt.Println("mailer.Send error:", err.Error())
			respondWithError(http.StatusInternalServerError, "", w)
			return
		}
	}

	respondOK(w)
}

// GET /users/self/balance
func getUserBalance(w http.ResponseWriter, r *http.Request) {

	type UserBalances struct {
		UserID         string `json:"userID"`
		DummyCoins     int64  `json:"dummyCoins"`
		GrantedCoins   int64  `json:"grantedCoins"`
		PurchasedCoins int64  `json:"purchasedCoins"`
		EarnedCoins    int64  `json:"earnedCoins"`
		TotalCoins     int64  `json:"totalCoins"`
	}

	// retrieve user sending the request
	senderUsr, err := getUserFromRequestContext(r)
	if err != nil {
		respondWithError(http.StatusInternalServerError, "", w)
		return
	}

	// user for which we want to retrieve friend relations
	var targetUserID string = senderUsr.ID

	dbClient, err := dbclient.SharedUserDB()
	if err != nil {
		respondWithError(http.StatusInternalServerError, "", w)
		return
	}

	usr, err := user.GetByID(dbClient, targetUserID)
	if err != nil {
		respondWithError(http.StatusNotFound, "", w)
		return
	}

	// get client build number

	// get user balance
	balances, err := scyllaClientUniverse.GetUserBalances(usr.ID)
	if err != nil {
		respondWithError(http.StatusInternalServerError, "", w)
		return
	}

	result := UserBalances{
		UserID:         usr.ID,
		DummyCoins:     0,
		GrantedCoins:   0,
		EarnedCoins:    0,
		PurchasedCoins: 0,
		TotalCoins:     0,
	}

	if balances != nil {
		result.DummyCoins = balances.DummyCoins / 1000000
		result.GrantedCoins = balances.GrantedCoins / 1000000
		result.PurchasedCoins = balances.PurchasedCoins / 1000000
		result.EarnedCoins = balances.EarnedCoins / 1000000
		result.TotalCoins = balances.TotalCoins / 1000000
	}

	respondWithJSON(http.StatusOK, result, w)
}

// GET /users/self/transactions
// Response body
// --------------------
// [
//
//	{ id="09c5cd9e-9c3a-4dc5-8083-06b77e1095e3", from={id="209809842",name="caillef"}, to={id="20980242",name="gdevillele"}, amount=283, action="buy", item={ id="09b5cd9f-9c3a-4dc5-8083-06b77e1099e5", slug="caillef.shop" }, copy=273, date="2020-07-12 17:01:00.000" },
//	{ id="09c5cd9e-9c3a-4dc5-8083-06b77e1095d3", from={id="0",name="treasure"}, to={id="209809842",name="caillef"}, amount=100, action="mint", item={ id="09b5cd9f-9c3a-4dc5-8083-06b77e1099e5", slug="caillef.shop" }, copy=683, date="2020-07-10 16:00:00.000" },
//	{ id="09c5cd9e-9c3a-4dc5-8083-06b77e1095f3", from={id="20980242",name="gdevillele"}, to={id="209809842",name="caillef"}, amount=20, action="buy", item={ id="09b5cd9f-9c3a-4dc5-8083-06b77e1099e5", slug="caillef.shop" }, copy=273, date="2020-06-10 15:00:00.000" }
//
// ]
func getUserRecentTransactions(w http.ResponseWriter, r *http.Request) {

	// retrieve user sending the request
	senderUsr, err := getUserFromRequestContext(r)
	if err != nil {
		respondWithError(http.StatusInternalServerError, "", w)
		return
	}

	// user for which we want to retrieve friend relations
	var targetUserID string = senderUsr.ID

	// retrieve user ID provided in the URL path (if any)
	if userID := chi.URLParam(r, "id"); userID != "" && userID != targetUserID {
		targetUserID = userID
	}

	// retrieve query params
	var limit uint = 10
	if limitStr := r.URL.Query().Get("limit"); limitStr != "" {
		customLimit, err := strconv.Atoi(limitStr)
		if err != nil || customLimit < 1 {
			respondWithError(http.StatusBadRequest, "limit query param has bad value", w)
			return
		}
		limit = uint(customLimit)
	}

	transactions, err := scyllaClientUniverse.GetUserTransactions(targetUserID, limit)
	if err != nil {
		respondWithError(http.StatusInternalServerError, "", w)
		return
	}

	respondWithJSON(http.StatusOK, transactions, w)
}

// GET /users/self/notifications
//
// Query parameters
// --------------------
// limit: number of notifications to retrieve (default 0: no limit)
// read: filter on notification read status (default: no filtering)
// category: filter on notification category (default: no filtering)
// returnCount: if true, return the count of notifications instead of the notifications themselves (limit is ignored in this case)
func getUserNotifications(w http.ResponseWriter, r *http.Request) {

	// retrieve user sending the request
	senderUsr, err := getUserFromRequestContext(r)
	if err != nil {
		respondWithError(http.StatusInternalServerError, "", w)
		return
	}

	// Limit
	var limit uint = 0
	if limitStr := r.URL.Query().Get("limit"); limitStr != "" {
		customLimit, err := strconv.Atoi(limitStr)
		if err == nil && customLimit > 0 {
			limit = uint(customLimit)
		}
	}

	// Return count
	var returnCount bool = false
	if returnCountStr := r.URL.Query().Get("returnCount"); returnCountStr == "true" {
		returnCount = true
		limit = 0 // 0 means no limit
	}

	// Read filter
	var readFilter *bool = nil
	if readStr := r.URL.Query().Get("read"); readStr != "" {
		read, err := strconv.ParseBool(readStr)
		if err == nil {
			readFilter = &read
		}
	}

	// Category filter
	var categoryFilter *string = nil
	if category := r.URL.Query().Get("category"); category != "" {
		categoryFilter = &category
	}

	// TODO: gdevillele: optimize this by using a ScyllaDB COUNT query when <returnCount> is true
	opts := scyllaclient.GetNotificationsOpts{
		Read:     readFilter,
		Category: categoryFilter,
		Limit:    limit,
	}

	notifications, err := scyllaClientUniverse.GetUserNotifications(senderUsr.ID, opts)
	if err != nil {
		respondWithError(http.StatusInternalServerError, "", w)
		return
	}

	if returnCount == true {
		type CountResponse struct {
			Count int `json:"count"`
		}
		respondWithJSON(http.StatusOK, CountResponse{Count: len(notifications)}, w)
		return
	}

	respondWithJSON(http.StatusOK, notifications, w)
}

type patchNotificationBody struct {
	Read *bool `json:"read"`
}

// PATCH /users/self/notifications/{id}
// Mark a notification as read
//
// URL parameters
// --------------------
// id: the ID of the notification to mark as read
//
// Request body
// --------------------
// read: boolean indicating if the notification should be marked as read
func patchUserNotification(w http.ResponseWriter, r *http.Request) {

	// retrieve user sending the request
	senderUsr, err := getUserFromRequestContext(r)
	if err != nil {
		respondWithError(http.StatusInternalServerError, "", w)
		return
	}

	// Retrieve the notification ID from the URL path
	notificationID := chi.URLParam(r, "id")
	if notificationID == "" {
		respondWithError(http.StatusBadRequest, "missing notification ID", w)
		return
	}

	// Parse the request body
	var req patchNotificationBody
	err = json.NewDecoder(r.Body).Decode(&req)
	if err != nil {
		respondWithError(http.StatusBadRequest, "invalid request body", w)
		return
	}

	// Process the request
	if req.Read != nil {
		err = scyllaClientUniverse.MarkNotificationAsRead(senderUsr.ID, notificationID, *req.Read)
		if err != nil {
			respondWithError(http.StatusBadRequest, "failed to mark as read", w)
			return
		}
	}

	respondOK(w)
}

func patchUserNotifications(w http.ResponseWriter, r *http.Request) {

	// retrieve user sending the request
	senderUsr, err := getUserFromRequestContext(r)
	if err != nil {
		respondWithError(http.StatusInternalServerError, "", w)
		return
	}

	// Retrieve filters from the query parameters
	var readFilter *bool = nil
	if readStr := r.URL.Query().Get("read"); readStr != "" {
		read, err := strconv.ParseBool(readStr)
		if err == nil {
			readFilter = &read
		}
	}

	var categoryFilter *string = nil
	if category := r.URL.Query().Get("category"); category != "" {
		categoryFilter = &category
	}

	// Parse the request body
	var req patchNotificationBody
	err = json.NewDecoder(r.Body).Decode(&req)
	if err != nil {
		respondWithError(http.StatusBadRequest, "invalid request body", w)
		return
	}

	// Process the request
	if req.Read == nil {
		respondWithError(http.StatusBadRequest, "no new read value", w)
		return
	}

	filter := scyllaclient.NotificationFilter{
		Read:     readFilter,
		Category: categoryFilter,
	}
	err = scyllaClientUniverse.MarkNotificationsAsRead(senderUsr.ID, filter, *req.Read)
	if err != nil {
		respondWithError(http.StatusBadRequest, "failed to mark as read", w)
		return
	}

	respondOK(w)
}

func getLeaderboard(w http.ResponseWriter, r *http.Request) {

	// retrieve user sending the request
	senderUsr, err := getUserFromRequestContext(r)
	if err != nil {
		respondWithError(http.StatusInternalServerError, "", w)
		return
	}

	// Process URL path parameters

	worldID := chi.URLParam(r, "worldID")
	leaderboardName := chi.URLParam(r, "leaderboardName")

	if worldID == "" || leaderboardName == "" {
		respondWithError(http.StatusBadRequest, "missing worldID or leaderboardName", w)
		return
	}

	// Process query parameters

	mode := "best"
	friends := false
	fetchUsernames := false
	opts := scyllaclient.GetLeaderboardScoreOpts{}

	// Query parameter: mode
	if modeStr := r.URL.Query().Get("mode"); modeStr == "best" || modeStr == "neighbors" {
		mode = modeStr
	}

	// Query parameter: friends
	if friendsStr := r.URL.Query().Get("friends"); friendsStr != "" {
		friends, err = strconv.ParseBool(friendsStr)
		if err != nil {
			respondWithError(http.StatusBadRequest, "invalid friends value", w)
			return
		}
	}

	// Query parameter: fetchUsernames
	if fetchUsernamesStr := r.URL.Query().Get("fetchUsernames"); fetchUsernamesStr != "" {
		fetchUsernames, err = strconv.ParseBool(fetchUsernamesStr)
		if err != nil {
			respondWithError(http.StatusBadRequest, "invalid fetchUsernames value", w)
			return
		}
	}

	// Query parameter: limit
	if limitStr := r.URL.Query().Get("limit"); limitStr != "" {
		limit, err := strconv.Atoi(limitStr)
		if err != nil || limit < 0 {
			respondWithError(http.StatusBadRequest, "invalid limit", w)
			return
		}
		opts.Limit = uint(limit)
	}

	// Retrieve leaderboard records

	var records []kvs.KvsLeaderboardRecord
	leaderboardRecords := make([]LeaderboardRecord, 0)
	var senderScore *kvs.KvsLeaderboardRecord = nil

	if mode == "neighbors" {
		// get score for user having sent the request
		senderScore, err = scyllaClientKvs.GetLeaderboardScoreForUser(worldID, leaderboardName, senderUsr.ID)
		if err != nil {
			respondWithError(http.StatusInternalServerError, "", w)
			return
		}
		if senderScore == nil {
			respondWithError(http.StatusNotFound, "current user has no score", w)
			return
		}
	}

	if friends {

		// Get friend relations
		relations, err := scyllaClientUniverse.GetFriendRelations(senderUsr.ID)
		if err != nil {
			respondWithError(http.StatusInternalServerError, "", w)
			return
		}

		// Extract userIDs from relations and add the sender's ID
		userIDs := make([]string, 0, len(relations)+1)
		for _, relation := range relations {
			userIDs = append(userIDs, relation.ID2.String())
		}
		userIDs = append(userIDs, senderUsr.ID)

		// Get scores for those user IDs
		var friendsRecords []kvs.KvsLeaderboardRecord
		friendsRecords, err = scyllaClientKvs.GetLeaderboardScoresForUsers(worldID, leaderboardName, userIDs)
		if err != nil {
			respondWithError(http.StatusInternalServerError, "", w)
			return
		}

		if mode == "best" {
			records = friendsRecords
		} else if mode == "neighbors" {
			// senderScore as a value
			// Add senderScore to friendsRecords
			friendsRecords = append(friendsRecords, *senderScore)
			records = extractNScoresAround(friendsRecords, *senderScore, int(opts.Limit))
		}
	} else { // friends == false
		if mode == "best" {
			records, err = scyllaClientKvs.GetLeaderboardScores(worldID, leaderboardName, opts)
			if err != nil {
				respondWithError(http.StatusInternalServerError, "", w)
				return
			}
		} else if mode == "neighbors" {
			// senderScore as a value
			records, err = scyllaClientKvs.GetLeaderboardScoresAround(worldID, leaderboardName, senderScore.Score, opts.Limit)
			if err != nil {
				respondWithError(http.StatusInternalServerError, "", w)
				return
			}
		}
	}

	// Convert to nice type
	for _, record := range records {
		leaderboardRecords = append(leaderboardRecords, NewLeaderboardRecord(record))
	}

	if fetchUsernames {
		// Get usernames
		userIDs := make([]string, 0, len(leaderboardRecords))
		for _, record := range leaderboardRecords {
			userIDs = append(userIDs, record.UserID)
		}
		dbClient, err := dbclient.SharedUserDB()
		if err != nil {
			respondWithError(http.StatusInternalServerError, "", w)
			return
		}
		usernames, err := user.GetUsernameForUsers(dbClient, userIDs)
		if err != nil || len(usernames) != len(userIDs) {
			respondWithError(http.StatusInternalServerError, "", w)
			return
		}
		for i, _ := range leaderboardRecords {
			leaderboardRecords[i].Username = usernames[i]
		}
	}

	respondWithJSON(http.StatusOK, leaderboardRecords, w)
}

func getLeaderboardUser(w http.ResponseWriter, r *http.Request) {

	// Process URL path parameters

	worldID := chi.URLParam(r, "worldID")
	leaderboardName := chi.URLParam(r, "leaderboardName")
	userID := chi.URLParam(r, "userID") // optional

	if worldID == "" || leaderboardName == "" {
		respondWithError(http.StatusBadRequest, "missing worldID or leaderboardName", w)
		return
	}

	if userID == "" {
		// retrieve user sending the request
		senderUsr, err := getUserFromRequestContext(r)
		if err != nil {
			respondWithError(http.StatusInternalServerError, "", w)
			return
		}
		userID = senderUsr.ID
	}

	// Process query parameters
	// ...

	// Retrieve leaderboard records

	record, err := scyllaClientKvs.GetLeaderboardScoreForUser(worldID, leaderboardName, userID)
	if err != nil {
		respondWithError(http.StatusInternalServerError, "", w)
		return
	}
	if record == nil {
		respondWithError(http.StatusNotFound, "", w)
		return
	}

	// Convert to nice type
	var leaderboardRecord LeaderboardRecord = NewLeaderboardRecord(*record)

	respondWithJSON(http.StatusOK, leaderboardRecord, w)
}

func postLeaderboard(w http.ResponseWriter, r *http.Request) {
	type postLeaderboardBody struct {
		ScoreFloat float64 `json:"score"`
		ScoreInt   int64
		Value      []byte `json:"value"`
		Override   bool   `json:"override"`
	}

	// retrieve user sending the request
	senderUsr, err := getUserFromRequestContext(r)
	if err != nil {
		respondWithError(http.StatusInternalServerError, "", w)
		return
	}

	// Process URL path parameters

	worldID := chi.URLParam(r, "worldID")
	leaderboardName := chi.URLParam(r, "leaderboardName")

	if worldID == "" || leaderboardName == "" {
		respondWithError(http.StatusBadRequest, "missing worldID or leaderboardName", w)
		return
	}

	// Parse the request body
	var req postLeaderboardBody
	err = json.NewDecoder(r.Body).Decode(&req)
	if err != nil {
		respondWithError(http.StatusBadRequest, "invalid request body", w)
		return
	}
	req.ScoreInt = int64(req.ScoreFloat)

	// Process the request

	record := scyllaclient.NewLeaderboardRecord(worldID, leaderboardName, senderUsr.ID, req.ScoreInt, req.Value)
	opts := scyllaclient.SetLeaderboardScoreOpts{
		ForceWrite: req.Override,
	}

	err = scyllaClientKvs.InsertOrUpdateLeaderboardScore(record, opts)
	if err != nil {
		respondWithError(http.StatusInternalServerError, "", w)
		return
	}

	respondOK(w)
}

// getUserSearch searches user records by their username
func getUserSearchOthers(w http.ResponseWriter, r *http.Request) {
	var err error

	// database client
	dbClient, err := dbclient.SharedUserDB()
	if err != nil {
		respondWithError(http.StatusInternalServerError, "", w)
		return
	}

	// retrieve user sending the request
	senderUsr, err := getUserFromRequestContext(r)
	if err != nil {
		respondWithError(http.StatusInternalServerError, "", w)
		return
	}

	// retrieve search string provided in the URL path
	searchStr := chi.URLParam(r, "searchString")

	// get all users matching the search, from the database
	users, err := user.List(dbClient, searchStr)
	if err != nil {
		respondWithError(http.StatusInternalServerError, "failed to search users in DB", w)
		return
	}

	// filter out sender user & exact match if found.
	filteredUsers := make([]*user.User, 0)
	var exactMatch *user.User = nil
	for _, usr := range users {
		if usr.Username == searchStr {
			exactMatch = usr
		} else if usr.ID != senderUsr.ID {
			filteredUsers = append(filteredUsers, usr)
		}
	}

	// Sort filteredUsers by lastSeen
	sort.Slice(filteredUsers, func(first, second int) bool {
		lastSeenFirst := time.Now()
		lastSeenSecond := time.Now()
		if filteredUsers[first].LastSeen != nil {
			lastSeenFirst = *filteredUsers[first].LastSeen
		}
		if filteredUsers[second].LastSeen != nil {
			lastSeenSecond = *filteredUsers[second].LastSeen
		}
		return lastSeenFirst.After(lastSeenSecond)
	})

	// If an exact match was found, put it at the front of the list
	resultUsers := make([]*user.User, 0)
	if exactMatch != nil {
		resultUsers = append([]*user.User{exactMatch}, filteredUsers...)
	} else {
		resultUsers = filteredUsers
	}

	respondWithJSON(http.StatusOK, resultUsers, w)
}

// getUserAvatar ...
func getUserAvatar(w http.ResponseWriter, r *http.Request) {

	// Retrieve user doing the request (can be nil)
	senderUsr, _ := getUserFromRequestContext(r)

	// retrieve user ID provided in the URL path
	reqUserIDOrUsername := chi.URLParam(r, "idOrUsername")

	if senderUsr == nil && reqUserIDOrUsername == "" {
		respondWithError(http.StatusUnauthorized, "", w)
		return
	}

	dbClient, err := dbclient.SharedUserDB()
	if err != nil {
		respondWithError(http.StatusInternalServerError, "", w)
		return
	}

	// retrieve user whose avatar is requested
	var userObj *user.User

	if reqUserIDOrUsername == "" {
		reqUserIDOrUsername = senderUsr.ID
		userObj = senderUsr
	} else if user.IsUsernameValid(reqUserIDOrUsername) { // get by username
		userObj, err = user.GetByUsername(dbClient, reqUserIDOrUsername)
		if err != nil {
			respondWithError(http.StatusInternalServerError, "", w)
			return
		}
		if userObj == nil { // user not found
			respondWithError(http.StatusBadRequest, "user not found", w)
			return
		}
	} else { // get by ID
		userObj, err = user.GetByID(dbClient, reqUserIDOrUsername)
		if err != nil {
			respondWithError(http.StatusInternalServerError, "", w)
			return
		}
		if userObj == nil { // user not found
			respondWithError(http.StatusBadRequest, "user not found", w)
			return
		}
	}

	var reply avatar = NewAvatar(
		&userObj.AvatarSkinColorIndex,
		&userObj.AvatarEyesColorIndex,
		&userObj.AvatarEyesIndex,
		&userObj.AvatarNoseIndex,
		&userObj.AvatarBoots,
		&userObj.AvatarHair,
		&userObj.AvatarJacket,
		&userObj.AvatarPants,
		userObj.AvatarEyesColor,
		userObj.AvatarNoseColor,
		userObj.AvatarSkinColor,
		userObj.AvatarSkinColor2,
		userObj.AvatarMouthColor,
	)

	respondWithJSON(http.StatusOK, reply, w)
}

// patchUserAvatar ...
func patchUserAvatar(w http.ResponseWriter, r *http.Request) {

	// retrieve user sending the request
	senderUsr, err := getUserFromRequestContext(r)
	if err != nil {
		respondWithError(http.StatusInternalServerError, "", w)
		return
	}

	// parse request
	var req avatar
	err = json.NewDecoder(r.Body).Decode(&req)
	if err != nil {
		respondWithError(http.StatusBadRequest, "can't decode json request", w)
		return
	}
	// read request body
	// reqBytes, err := io.ReadAll(r.Body)
	// if err != nil {
	// 	respondWithError(http.StatusBadRequest, "can't read request body", w)
	// 	return
	// }
	// // unmarshal request body
	// err = json.Unmarshal(reqBytes, &req)
	// if err != nil {
	// 	respondWithError(http.StatusBadRequest, "can't decode json request", w)
	// 	return
	// }

	// database client
	dbClient, err := dbclient.SharedUserDB()
	if err != nil {
		respondWithError(http.StatusInternalServerError, "", w)
		return
	}

	// Idea : we could prohibit duplicated wearable categories in a single request.

	// handle avatar fields
	// ------------------------------

	if req.SkinColorIndex != nil {
		senderUsr.AvatarSkinColorIndex = *req.SkinColorIndex
	}

	if req.EyesColorIndex != nil {
		senderUsr.AvatarEyesColorIndex = *req.EyesColorIndex
	}

	if req.EyesIndex != nil {
		senderUsr.AvatarEyesIndex = *req.EyesIndex
	}

	if req.NoseIndex != nil {
		senderUsr.AvatarNoseIndex = *req.NoseIndex
	}

	// Boots
	if req.Boots != nil {
		err = ensureItemExists(dbClient, *req.Boots, nil)
		if err != nil {
			respondWithError(http.StatusBadRequest, "item not found or invalid: "+*req.Boots, w)
			return
		}
		senderUsr.AvatarBoots = *req.Boots
		fmt.Println("[PATCH /users/self/avatar] did set [AvatarBoots] to [" + *req.Boots + "]")
	}

	// Hair
	if req.Hair != nil {
		err = ensureItemExists(dbClient, *req.Hair, nil)
		if err != nil {
			respondWithError(http.StatusBadRequest, "item not found or invalid: "+*req.Hair, w)
			return
		}
		senderUsr.AvatarHair = *req.Hair
		fmt.Println("[PATCH /users/self/avatar] did set [AvatarHair] to [" + *req.Hair + "]")
	}

	// Jacket
	if req.Jacket != nil {
		err = ensureItemExists(dbClient, *req.Jacket, nil)
		if err != nil {
			respondWithError(http.StatusBadRequest, "item not found or invalid: "+*req.Jacket, w)
			return
		}
		senderUsr.AvatarJacket = *req.Jacket
		fmt.Println("[PATCH /users/self/avatar] did set [AvatarJacket] to [" + *req.Jacket + "]")
	}

	// Pants
	if req.Pants != nil {
		err = ensureItemExists(dbClient, *req.Pants, nil)
		if err != nil {
			respondWithError(http.StatusBadRequest, "item not found or invalid: "+*req.Pants, w)
			return
		}
		senderUsr.AvatarPants = *req.Pants
		fmt.Println("[PATCH /users/self/avatar] did set [AvatarPants] to [" + *req.Pants + "]")
	}

	// LEGACY FIELDS

	// EyesColor
	if req.EyesColor != nil {
		senderUsr.AvatarEyesColor = req.EyesColor
		fmt.Println("[PATCH /users/self/avatar] did set [AvatarEyesColor] to [", req.EyesColor.R, req.EyesColor.G, req.EyesColor.B, "]")
	}

	// NoseColor
	if req.NoseColor != nil {
		senderUsr.AvatarNoseColor = req.NoseColor
		fmt.Println("[PATCH /users/self/avatar] did set [AvatarNoseColor] to [", req.NoseColor.R, req.NoseColor.G, req.NoseColor.B, "]")
	}

	// SkinColor
	if req.SkinColor != nil {
		senderUsr.AvatarSkinColor = req.SkinColor
		fmt.Println("[PATCH /users/self/avatar] did set [AvatarSkinColor] to [", req.SkinColor.R, req.SkinColor.G, req.SkinColor.B, "]")
	}

	// SkinColor2
	if req.SkinColor2 != nil {
		senderUsr.AvatarSkinColor2 = req.SkinColor2
		fmt.Println("[PATCH /users/self/avatar] did set [AvatarSkinColor2] to [", req.SkinColor2.R, req.SkinColor2.G, req.SkinColor2.B, "]")
	}

	// MouthColor
	if req.MouthColor != nil {
		senderUsr.AvatarMouthColor = req.MouthColor
		fmt.Println("[PATCH /users/self/avatar] did set [AvatarMouthColor] to [", req.MouthColor.R, req.MouthColor.G, req.MouthColor.B, "]")
	}

	err = senderUsr.Save(dbClient)
	if err != nil {
		respondWithError(http.StatusInternalServerError, "failed to update user", w)
		return
	}

	respondOK(w)
}

func postUserPushToken(w http.ResponseWriter, r *http.Request) {

	var err error

	// database client
	dbClient, err := dbclient.SharedUserDB()
	if err != nil {
		respondWithError(http.StatusInternalServerError, "", w)
		return
	}

	// retrieve user sending the request
	usr, err := getUserFromRequestContext(r)
	if err != nil {
		respondWithError(http.StatusInternalServerError, "", w)
		return
	}

	type postPushTokenReq struct {
		Type    string `json:"type,omitempty"`    // type of push token
		Token   string `json:"token,omitempty"`   // token value
		Variant string `json:"variant,omitempty"` // token variant (optional) (e.g. "dev", "prod")
	}

	// parse request
	var req postPushTokenReq
	err = json.NewDecoder(r.Body).Decode(&req)
	if err != nil {
		respondWithError(http.StatusBadRequest, "can't decode request", w)
		return
	}

	fmt.Println("🐞[PushNotif] Received token:", req.Type, req.Token, req.Variant, " for user: ", usr.ID)

	// validate request
	{
		// req.Variant is optional (can be empty) for now
		// We will be able to enforce it when 0.1.2 won't be allowed anymore.
		if req.Type == "" || req.Token == "" {
			respondWithError(http.StatusBadRequest, "bad token", w)
			return
		}
	}

	// Add token to user account

	newToken := user.NewToken(req.Token, req.Variant, time.Now())

	if req.Type == PUSH_TOKEN_TYPE_APPLE {
		usr.ApplePushTokens = appendTokenIfMissing(usr.ApplePushTokens, newToken)
	} else if req.Type == PUSH_TOKEN_TYPE_FIREBASE {
		usr.FirebasePushTokens = appendTokenIfMissing(usr.FirebasePushTokens, newToken)
	} else {
		// unsupported token type
		respondWithError(http.StatusBadRequest, "bad token", w)
		return
	}

	err = usr.Save(dbClient)
	if err != nil {
		respondWithError(http.StatusInternalServerError, "failed to update user", w)
		return
	}

	// Create unique coin grant for the user for accepting push notifications
	amount := int64(100)
	amountInMillionths := int64(amount * 1000000)
	const REASON string = "uniqueRewardForPushNotifAcceptance"
	const DESCRIPTION string = "reward for notifications"

	opts := transactions.NewGrantCoinsOpts(true, DESCRIPTION)
	opts.Unique = true
	err = transactions.GrantCoinsToUser(usr.ID, amountInMillionths, REASON, opts)
	if err != nil {
		// Ignore error, we don't want to block the user for this
		// TODO: report errors in Discord?
	} else {
		// create notification
		category := NOTIFICATION_CATEGORY_MONEY
		title := "Cha-Ching! " + strconv.FormatInt(amount, 10) + " 🪙 for you!"
		message := "Notifications on? Check. Coins in your pocket? Double-check."
		notificationID, badgeCount, err := scyllaClientUniverse.CreateUserNotification(usr.ID, category, title, message)
		if err == nil {
			// send push notification
			sendPushNotification(usr, category, title, message, notificationID, badgeCount)
		}
	}

	respondOK(w)
}

// ensureItemExists makes sure an item have the provided <fullname> does exist in the DB.
// If <expectedCategory> is provided, then the category of the item is checked as well.
func ensureItemExists(dbClient *dbclient.Client, fullname string, expectedCategory *string) error {

	// make sure an item exists with the provided name
	repo, name, err := item.ParseRepoDotName(fullname)
	if err != nil {
		return err
	}

	itms, err := item.GetByRepoAndName(dbClient, repo, name)
	if err != nil {
		return err
	}
	if len(itms) == 0 {
		return errors.New("item not found")
	}

	// check category if one has been provided
	if expectedCategory != nil {
		itm := itms[0]
		if itm.Category != *expectedCategory {
			return errors.New("item found by category does not match")
		}
	}

	return nil
}
