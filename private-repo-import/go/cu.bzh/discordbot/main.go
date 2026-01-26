package main

import (
	"bytes"
	"encoding/json"
	"errors"
	"fmt"
	"io"
	"log"
	"math/rand"
	"net/http"
	"net/url"
	"os"
	"os/signal"
	"regexp"
	"sort"
	"strconv"
	"strings"
	"syscall"
	"time"
	"unicode"

	"github.com/bwmarrin/discordgo"
	"github.com/go-chi/chi/v5"
	"github.com/go-chi/chi/v5/middleware"

	linkTypes "cu.bzh/web/link/types"

	"cu.bzh/hub/apnsclient"
	"cu.bzh/hub/dbclient"
	"cu.bzh/hub/fcmclient"
	"cu.bzh/hub/item"
	"cu.bzh/hub/scyllaclient"
	"cu.bzh/hub/transactions"
	"cu.bzh/hub/user"
)

const (
	// Connectivity
	HTTP_PORT = ":80"
	// Discord
	CHANNEL_BOT_TESTS           = "1066775183240740895"
	CHANNEL_LUA_SCRIPTS         = "766205280353648640"
	CHANNEL_DEV_TALKS           = "966959833154723840"
	MULTIPLAYER_MESSAGE_ID      = "1051919764974473276"
	MULTIPLAYER_CHANNEL_ID      = "1051918003878498324"
	DASHBOARD_CHANNEL_ID        = "1060223186228224050"
	DASHBOARD_FIRST_MESSAGE_ID  = "1187083931573555280"
	DASHBOARD_SECOND_MESSAGE_ID = "1070301907379302450"
	STAFF_CHANNEL_ID            = "656867160831819786"
	LETS_PLAY_CHANNEL_ID        = "1054396406061879348"
	DISCORD_USER_ADUERMAEL      = "140631519130681344"
	DISCORD_USER_GDEVILLELE     = "145954514980044801"
	DELAY_BACKUP                = 24 * time.Hour   // 24 hours
	DELAY_CHECK_SERVER_STATUS   = 15 * time.Minute // 15 minutes // for development : 15 * time.Second
	BOT_ID                      = "1051892823592534076"
	BOT_USERNAME                = "Buzzh"
	SEARCH_ENGINE_ADDR          = "http://144.126.245.237:8108"
	SEARCH_ENGINE_APIKEY        = "Hu52ddsas2AdxdE"
	DISCORD_MSG_MAX_LEN         = 2000
	// Discord Bot Commands
	CMD_GET_BALANCE          = "!getbalance"         // display balance of a user
	CMD_GET_COMPUTED_BALANCE = "!getcomputedbalance" // display balance of a user (recomputed)
	CMD_GRANT_COINS          = "!grantcoins"         //
	CMD_SEND_NOTIFICATION    = "!notif"              // send a notification to a user
	CMD_TEST                 = "!test"               // test command
	CMD_DAY1_NOTIFICATION    = "!day1notification"   //
	CMD_SEND_LEGACY_GRANTS   = "!sendlegacygrants"   //
	// ...
	APNS_AUTH_KEY_FILEPATH         = "/voxowl/secrets/apns/AuthKey_X48M5UN4U7.p8"   // Path to the APNS auth key file
	FIREBASE_ACCOUNT_JSON_FILEPATH = "/voxowl/secrets/firebase/cubzh-firebase.json" // Path to the Firebase account JSON file
)

var (
	TOKEN               = ""
	OPEN_AI_TOKEN       = os.Getenv("OPENAI_API_KEY_DISCORDBOT")
	OPEN_AI_ORG         = "org-XTefQQRpqPYx78mphyu4dkwa"
	dg                  *discordgo.Session
	worldsPreviousState map[string]*World
	TEAM_PLAYERS        = []string{DISCORD_USER_ADUERMAEL}
	TEAM_MEMBERS              = []string{DISCORD_USER_ADUERMAEL, DISCORD_USER_GDEVILLELE}
	cubzh                     = true
	docs                      = true
	api                       = true
	app                       = true
	hfErr               error = nil
)

type World struct {
	Title         string `json:"title,omitempty"`
	Description   string `json:"description,omitempty"`
	OnlinePlayers int    `json:"online-players,omitempty"`
	Link          string `json:"link,omitempty"`
}

func main() {
	var err error

	TOKEN = os.Getenv("TOKEN")

	worldsPreviousState = make(map[string]*World)

	fmt.Println("Starting Discord Bot... 🤖")

	fmt.Println("Creating ScyllaDB client...")

	{
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
			_, err = scyllaclient.NewShared(config)
			if err != nil {
				log.Fatal("failed to create scylla client (universe):", err.Error())
			}
		}
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
			_, err := scyllaclient.NewShared(config)
			if err != nil {
				log.Fatal("failed to create scylla client (moderation):", err.Error())
				return
			}
		}
	}

	// create HTTP router
	r := chi.NewRouter()
	r.Use(middleware.Logger)
	r.NotFound(notFound)
	r.Post("/servers-info", serversInfo)
	// WebSocket
	r.HandleFunc("/", userSessionHandler)
	r.HandleFunc("/ws/user-session", userSessionHandler)

	go func() {
		log.Fatal(http.ListenAndServe(HTTP_PORT, r))
	}()

	fmt.Println("Listening for user sessions...")

	// Create a new Discord session using the provided bot token.
	dg, err = discordgo.New("Bot " + TOKEN)
	if err != nil {
		fmt.Println("error creating Discord session,", err)
		return
	}

	// Register the messageCreate func as a callback for MessageCreate events.
	dg.AddHandler(messageCreate)
	dg.AddHandler(reactionAdd)
	dg.AddHandler(reactionRemove)

	// dg.Identify.Intents = discordgo.IntentsGuildMessages
	dg.Identify.Intents = discordgo.IntentsAllWithoutPrivileged | discordgo.IntentMessageContent
	// dg.Identify.Intents = discordgo.IntentsAll

	// Open a websocket connection to Discord and begin listening.
	err = dg.Open()
	if err != nil {
		fmt.Println("error opening connection:", err) // <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<
		return
	}

	// https://discord.com/channels/355905150528913409/1030027730705727539
	// https://discord.com/channels/355905150528913409/355905150528913410
	c, err := dg.Channel("355905150528913410")
	if err != nil {
		fmt.Println("error getting channel", err)
		return
	}
	fmt.Println(c)

	// start monitoring
	go func() {
		for {
			updateServerStatus()
			time.Sleep(DELAY_CHECK_SERVER_STATUS)
		}
	}()

	// start backup
	// go func() {
	// 	updateBackup()
	// }()

	// start day 1 notification ticker
	registerDay1NotificationTicker(dg)

	// Wait here until CTRL-C or other term signal is received.
	fmt.Println("Bot is now running.  Press CTRL-C to exit.")
	sc := make(chan os.Signal, 1)
	signal.Notify(sc, syscall.SIGINT, syscall.SIGTERM, os.Interrupt)
	<-sc

	// Cleanly close down the Discord session.
	dg.Close()
}

// This function will be called (due to AddHandler above) every time a new
// message is created on any channel that the authenticated bot has access to.
func messageCreate(s *discordgo.Session, m *discordgo.MessageCreate) {

	// Ignore all messages created by the bot itself
	// This isn't required in this specific example but it's a good practice.
	if m.Author.ID == s.State.User.ID {
		return
	}

	if isTeamMember(m.Author.ID) || m.ChannelID == CHANNEL_LUA_SCRIPTS || m.ChannelID == CHANNEL_DEV_TALKS {
		for _, user := range m.Mentions {
			if user.Bot && user.Username == BOT_USERNAME { // NOTE: should we check BOT_ID instead? is it constant?
				// fmt.Println("RAW CONTENT:", m.Content)

				reg, err := regexp.Compile("<@[^>]*>")
				if err != nil {
					fmt.Println("ERROR:", err)
					return
				}

				content := reg.ReplaceAllString(m.Content, "")
				content = strings.TrimSpace(content)

				// fmt.Println("CONTENT:", content)

				msg, err := dg.ChannelMessageSendReply(m.ChannelID, "🤖⚙️...", &discordgo.MessageReference{MessageID: m.ID})
				if err != nil {
					fmt.Println("ERROR:", err)
					return
				}

				systemPrompt := "You're an assistant that's helping users of a user-generated content platform called Cubzh. Users can be players, artists or coders. When there's a question about code or scripting without indication about the language: always assume the language is Lua and surround code blocks with ```lua and ``` markers. Answers should be as short as possible, for a young audience (teenagers)."

				text, err := askReplicate(content, systemPrompt)
				if err == nil {
					// fmt.Println("TEXT:", text)
					dg.ChannelMessageEdit(m.ChannelID, msg.ID, text)
					// dg.ChannelMessageSend(m.ChannelID, text)
				} else {
					// fmt.Println("ASK AI ERROR:", err.Error())
					dg.ChannelMessageEdit(m.ChannelID, msg.ID, "🤖❌ sorry, something went wrong!")
					// dg.ChannelMessageSend(m.ChannelID, "🤖❌ sorry, something went wrong!")
				}
			}
		}
	}

	// Test if `Buzzh` bot is mentionned alone in the `bot-tests` channel.
	if m.ChannelID == CHANNEL_BOT_TESTS {
		// 	len(m.Mentions) == 1 &&
		// 	m.Mentions[0].Bot &&
		// 	m.Mentions[0].ID == BOT_ID

		// parse content
		reg, err := regexp.Compile("<@[^>]*>")
		if err != nil {
			fmt.Println("ERROR:", err.Error())
			return
		}
		content := reg.ReplaceAllString(m.Content, "")
		content = strings.TrimSpace(content)

		// Handle Buzzh (bot) commands
		// example:
		// !myCommand myArgument
		if len(content) > 0 && content[0] == '!' {
			fmt.Println("⚡️ bot command !", content)
			if strings.HasPrefix(content, CMD_TEST) {
				s.ChannelMessageSend(CHANNEL_BOT_TESTS, "Hello! Discord bot is working. 🙂")

			} else if strings.HasPrefix(content, CMD_GET_BALANCE) {

				args := parseCommandArguments(content)
				if len(args) != 1 {
					s.ChannelMessageSend(CHANNEL_BOT_TESTS, "!getbalance command expects 1 arguments. Got "+strconv.FormatInt(int64(len(args)), 10)+"\nexample: `!getbalance <username>`")
					return
				}

				username := args[0]
				loadingMsg, err := s.ChannelMessageSend(CHANNEL_BOT_TESTS, "⚙️ Getting balances for username: `"+username+"`")

				// shared Hub DB client
				dbClient, err := dbclient.SharedUserDB()
				if err != nil {
					s.ChannelMessageSend(CHANNEL_BOT_TESTS, "❌ failed to get MongoClient: "+err.Error())
					return
				}

				// get user by username
				usr, err := user.GetByUsername(dbClient, username)
				if err != nil {
					s.ChannelMessageSend(CHANNEL_BOT_TESTS, "❌ "+err.Error())
					return
				}
				if usr == nil {
					s.ChannelMessageSend(CHANNEL_BOT_TESTS, "❌ user not found")
					return
				}

				scyllaClientUniverse := scyllaclient.GetShared("universe")
				if scyllaClientUniverse == nil {
					s.ChannelMessageSend(CHANNEL_BOT_TESTS, "❌ failed to get ScyllaClient: universe")
					return
				}

				// get user balance
				balances, err := scyllaClientUniverse.GetUserBalances(usr.ID)
				if err != nil {
					s.ChannelMessageSend(CHANNEL_BOT_TESTS, "❌ "+err.Error())
					return
				}
				var text string
				if balances == nil {
					text = "💰 `" + usr.Username + "`" +
						"\n	🪙 Granted: " + strconv.FormatInt(0, 10) +
						"\n	🪙 Dummy: " + strconv.FormatInt(0, 10) +
						"\n	🪙 Earned: " + strconv.FormatInt(0, 10) +
						"\n	🪙 Purchased: " + strconv.FormatInt(0, 10) +
						"\n	🪙 Total: " + strconv.FormatInt(0, 10)
				} else {
					text = "💰 " + usr.Username +
						"\n	🪙 Granted: " + strconv.FormatInt(balances.GrantedCoins/1000000, 10) +
						"\n	🪙 Dummy: " + strconv.FormatInt(balances.DummyCoins/1000000, 10) +
						"\n	🪙 Earned: " + strconv.FormatInt(balances.EarnedCoins/1000000, 10) +
						"\n	🪙 Purchased: " + strconv.FormatInt(balances.PurchasedCoins/1000000, 10) +
						"\n	🪙 Total: " + strconv.FormatInt(balances.TotalCoins/1000000, 10)
				}

				s.ChannelMessageDelete(loadingMsg.ChannelID, loadingMsg.ID)
				_, err = s.ChannelMessageSend(CHANNEL_BOT_TESTS, text)
				if err != nil {
					fmt.Println("[🐞]ERROR:", err)
					return
				}

			} else if strings.HasPrefix(content, CMD_GET_COMPUTED_BALANCE) {

				args := parseCommandArguments(content)
				if len(args) != 1 {
					s.ChannelMessageSend(CHANNEL_BOT_TESTS, "!getcomputedbalance command expects 1 arguments. Got "+strconv.FormatInt(int64(len(args)), 10)+"\nexample: `!getcomputedbalance <username>`")
					return
				}

				username := args[0]
				loadingMsg, err := s.ChannelMessageSend(CHANNEL_BOT_TESTS, "⚙️ Computing balances for username: `"+username+"`")

				// shared Hub DB client
				dbClient, err := dbclient.SharedUserDB()
				if err != nil {
					s.ChannelMessageSend(CHANNEL_BOT_TESTS, "❌ failed to get MongoClient: "+err.Error())
					return
				}

				// get user by username
				usr, err := user.GetByUsername(dbClient, username)
				if err != nil {
					s.ChannelMessageSend(CHANNEL_BOT_TESTS, "❌ "+err.Error())
					return
				}
				if usr == nil {
					s.ChannelMessageSend(CHANNEL_BOT_TESTS, "❌ user not found")
					return
				}

				scyllaClientUniverse := scyllaclient.GetShared("universe")
				if scyllaClientUniverse == nil {
					s.ChannelMessageSend(CHANNEL_BOT_TESTS, "❌ failed to get ScyllaClient: universe")
					return
				}

				// get user balance
				balances, err := scyllaClientUniverse.GetUserBalancesComputedFromTransactions(usr.ID)
				if err != nil {
					s.ChannelMessageSend(CHANNEL_BOT_TESTS, "❌ "+err.Error())
					return
				}
				var text string
				if balances == nil {
					text = "💰 `" + usr.Username + "`" +
						"\n	🪙 Granted: " + strconv.FormatInt(0, 10) +
						"\n	🪙 Dummy: " + strconv.FormatInt(0, 10) +
						"\n	🪙 Earned: " + strconv.FormatInt(0, 10) +
						"\n	🪙 Purchased: " + strconv.FormatInt(0, 10) +
						"\n	🪙 Total: " + strconv.FormatInt(0, 10)
				} else {
					text = "💰 " + usr.Username +
						"\n	🪙 Granted: " + strconv.FormatInt(balances.GrantedCoins/1000000, 10) +
						"\n	🪙 Dummy: " + strconv.FormatInt(balances.DummyCoins/1000000, 10) +
						"\n	🪙 Earned: " + strconv.FormatInt(balances.EarnedCoins/1000000, 10) +
						"\n	🪙 Purchased: " + strconv.FormatInt(balances.PurchasedCoins/1000000, 10) +
						"\n	🪙 Total: " + strconv.FormatInt(balances.TotalCoins/1000000, 10)
				}

				s.ChannelMessageDelete(loadingMsg.ChannelID, loadingMsg.ID)
				_, err = s.ChannelMessageSend(CHANNEL_BOT_TESTS, text)
				if err != nil {
					fmt.Println("[🐞]ERROR:", err)
					return
				}

			} else if strings.HasPrefix(content, CMD_GRANT_COINS) {
				// parse command
				args := parseCommandArguments(content)
				argsCount := len(args)
				if argsCount < 4 || argsCount > 5 {
					s.ChannelMessageSend(CHANNEL_BOT_TESTS, "!grantcoins command expects 3 or 4 arguments. Got "+strconv.FormatInt(int64(len(args)), 10)+"\nexample: `!grantcoins <username> <amount> <reason> <description> <uniqueBool>`")
					return
				}
				username := args[0]
				amountStr := args[1]
				amount, err := strconv.ParseInt(amountStr, 10, 64)
				if err != nil {
					s.ChannelMessageSend(CHANNEL_BOT_TESTS, "❌ failed to parse amount: "+err.Error())
					return
				}
				amount *= 1000000
				reason := args[2]
				description := args[3]
				unique := argsCount == 5 && args[4] == "unique"

				loadingMsg, err := s.ChannelMessageSend(CHANNEL_BOT_TESTS, "Granting coins...⚙️")

				// get user by username
				dbClient, err := dbclient.SharedUserDB()
				if err != nil {
					s.ChannelMessageSend(CHANNEL_BOT_TESTS, "❌ failed to get MongoClient: "+err.Error())
					return
				}
				usr, err := user.GetByUsername(dbClient, username)
				if err != nil {
					s.ChannelMessageSend(CHANNEL_BOT_TESTS, "❌ "+err.Error())
					return
				}
				if usr == nil {
					s.ChannelMessageSend(CHANNEL_BOT_TESTS, "❌ user not found")
					return
				}

				// grant coins
				opts := transactions.NewGrantCoinsOpts(unique, description)
				err = transactions.GrantCoinsToUser(usr.ID, amount, reason, opts)
				if err != nil {
					s.ChannelMessageSend(CHANNEL_BOT_TESTS, "❌ failed to grant coins: "+err.Error())
					return
				}

				s.ChannelMessageEdit(loadingMsg.ChannelID, loadingMsg.ID, "Granting coins...✅")
				loadingMsg, err = s.ChannelMessageSend(CHANNEL_BOT_TESTS, "Sending push notification...⚙️")

				// send push notification
				title := "Look! Coins!"
				message := "You have been granted " + strconv.FormatInt(amount/1000000, 10) + " coins for " + reason
				err = sendPushNotification(usr, "money", title, message, "", 42)
				if err != nil {
					s.ChannelMessageEdit(loadingMsg.ChannelID, loadingMsg.ID, "Sending push notification...❌")
					return
				}
				s.ChannelMessageEdit(loadingMsg.ChannelID, loadingMsg.ID, "Sending push notification...✅")

			} else if strings.HasPrefix(content, CMD_SEND_LEGACY_GRANTS) {
				s.ChannelMessageSend(CHANNEL_BOT_TESTS, "Sending legacy grants...")

				dbClient, err := dbclient.SharedUserDB()
				if err != nil {
					s.ChannelMessageSend(CHANNEL_BOT_TESTS, "❌ failed to get MongoClient: "+err.Error())
					return
				}

				scyllaClientUniverse := scyllaclient.GetShared("universe")
				if scyllaClientUniverse == nil {
					s.ChannelMessageSend(CHANNEL_BOT_TESTS, "❌ failed to get ScyllaClient")
					return
				}

				grant := func(amount int64, reason string, description string, usr *user.User, scyllaClient *scyllaclient.Client) error {
					// grant coins
					unique := true
					opts := transactions.NewGrantCoinsOpts(unique, description)
					err = transactions.GrantCoinsToUser(usr.ID, amount*1000000, reason, opts)
					if err != nil {
						s.ChannelMessageSend(CHANNEL_BOT_TESTS, "❌ failed to grant coins: "+err.Error())
						return err
					}

					// create notification
					category := "money"
					title := "Cha-Ching! " + strconv.FormatInt(amount, 10) + " 🪙 for you!"
					message := description
					notificationID, badgeCount, err := scyllaClientUniverse.CreateUserNotification(usr.ID, category, title, message)
					if err != nil {
						s.ChannelMessageSend(CHANNEL_BOT_TESTS, "❌ failed to create notification: "+err.Error())
					} else {
						// send push notification
						go func() {
							sendPushNotification(usr, category, title, message, notificationID, badgeCount)
						}()
					}

					s.ChannelMessageSend(CHANNEL_BOT_TESTS, "✅ granted "+usr.Username+" "+strconv.FormatInt(amount, 10)+" coins. reason:"+reason)
					return nil
				}

				users, err := user.GetAll(dbClient)
				if err != nil {
					s.ChannelMessageSend(CHANNEL_BOT_TESTS, "❌ can't get all users: "+err.Error())
					return
				}

				for _, u := range users {
					if u.Tags["pioneer"] || u.Tags["dustzh"] || u.Tags["aypierre"] {
						fmt.Println("Handling grants for ", u.Username)
					}

					if u.Tags["pioneer"] {
						grant(200, "cubzh-pioneer-grant", "Thanks for being a pioneer!", u, scyllaClientUniverse)
					}

					if u.Tags["dustzh"] {
						grant(200, "cubzh-dustzh-grant", "Thanks for playing Dutzh!", u, scyllaClientUniverse)
					}

					if u.Tags["aypierre"] {
						grant(609, "cubzh-aypierre-grant", "Thanks for watching Aypierre's stream!", u, scyllaClientUniverse)
					}
				}

				fmt.Println("✅ Done.")

			} else if strings.HasPrefix(content, "!lasttransactions") {

				// parse command
				args := parseCommandArguments(content)
				if len(args) > 2 {
					s.ChannelMessageSend(CHANNEL_BOT_TESTS, "!lasttransactions command expects 1 or 2 arguments. Got "+strconv.FormatInt(int64(len(args)), 10)+"\nexample: `!lasttransactions <username> 15`")
					return
				}
				username := args[0]
				limit := uint(10)
				if len(args) == 2 {
					limitInt64, err := strconv.ParseInt(args[1], 10, 64)
					if err != nil {
						s.ChannelMessageSend(CHANNEL_BOT_TESTS, "❌ failed to parse limit: "+err.Error())
						return
					}
					if limitInt64 < 1 {
						s.ChannelMessageSend(CHANNEL_BOT_TESTS, "❌ limit must be >= 1")
						return
					}
					limit = uint(limitInt64)
				}

				// get user by username
				dbClient, err := dbclient.SharedUserDB()
				if err != nil {
					s.ChannelMessageSend(CHANNEL_BOT_TESTS, "❌ failed to get MongoClient: "+err.Error())
					return
				}

				usr, err := user.GetByUsername(dbClient, username)
				if err != nil {
					s.ChannelMessageSend(CHANNEL_BOT_TESTS, "❌ "+err.Error())
					return
				}
				if usr == nil {
					s.ChannelMessageSend(CHANNEL_BOT_TESTS, "❌ user not found")
					return
				}

				// get transactions
				scyllaClientUniverse := scyllaclient.GetShared("universe")
				if scyllaClientUniverse == nil {
					s.ChannelMessageSend(CHANNEL_BOT_TESTS, "❌ failed to get ScyllaClient: universe")
					return
				}
				transactions, err := scyllaClientUniverse.GetUserTransactions(usr.ID, limit)
				if err != nil {
					s.ChannelMessageSend(CHANNEL_BOT_TESTS, "❌ failed to get transactions: "+err.Error())
					return
				}

				// print transactions
				text := "📜 Last transactions for user: `" + username + "`\n"
				for _, transaction := range transactions {
					// convert timestamp to time.Time
					text += "	🪙 Amount: " + strconv.FormatInt(transaction.Amount, 10) + ", Info.Reason: " + transaction.Info.Reason + ", Info.Description: " + transaction.Info.Description + ", Time: " + transaction.CreatedAt.String() + "\n"
				}

				s.ChannelMessageSend(CHANNEL_BOT_TESTS, text)

			} else if strings.HasPrefix(content, "!mod") {

				// remove command name
				args := parseCommandArguments(content)
				if len(args) != 3 {
					s.ChannelMessageSend(CHANNEL_BOT_TESTS, "!mod command expects 3 arguments. Got "+strconv.FormatInt(int64(len(args)), 10)+"\nexample: `!mod get username <username>`")
					return
				}

				cmd := args[0]       // get/set
				dataType := args[1]  // "username" (or "itemName" later)
				dataValue := args[2] //

				if cmd == "get" && dataType == "username" {

					c := scyllaclient.GetShared("moderation")
					if c == nil {
						s.ChannelMessageSend(CHANNEL_BOT_TESTS, "❌ failed to get ScyllaClient: moderation")
						return
					}
					r, err := c.GetModerationUsername(dataValue)
					if err != nil {
						s.ChannelMessageSend(CHANNEL_BOT_TESTS, "❌ db query failed:"+err.Error())
						return
					}
					boolToString := map[bool]string{true: "true", false: "false"}
					timeToString := func(t time.Time) string {
						return t.Format("2006-01-02 15:04:05")
					}
					s.ChannelMessageSend(CHANNEL_BOT_TESTS, "Username moderation: "+r.Username+" "+boolToString[r.Appropriate]+" "+timeToString(r.Created))

				} else {
					s.ChannelMessageSend(CHANNEL_BOT_TESTS, "only this command is supported: !mod get username <username>")
					return
				}

			} else if strings.HasPrefix(content, "!getnotifs") {
				// Each filter is optional
				// !getnotifs <username> [read=true|false] [category=category] [limit=limit]

				args := parseCommandArguments(content)
				if len(args) < 1 || len(args) > 4 {
					s.ChannelMessageSend(CHANNEL_BOT_TESTS, "!getnotifs command expects [1-4] arguments. Got "+strconv.FormatInt(int64(len(args)), 10)+"\nexample: `!getnotifs gaetan read=true category=money limit=10`")
					return
				}

				username := args[0]
				args = args[1:]
				var read *bool = nil
				var category *string = nil
				limit := uint(0)

				for _, arg := range args {
					if strings.HasPrefix(arg, "read=") {
						readValue := arg[5:]
						readBool, err := strconv.ParseBool(readValue)
						if err != nil {
							s.ChannelMessageSend(CHANNEL_BOT_TESTS, "❌ failed to parse read value: "+err.Error())
							return
						}
						read = &readBool
					} else if strings.HasPrefix(arg, "category=") {
						categoryValue := arg[9:]
						category = &categoryValue
					} else if strings.HasPrefix(arg, "limit=") {
						limitInt64, err := strconv.ParseInt(arg[6:], 10, 64)
						if err != nil {
							s.ChannelMessageSend(CHANNEL_BOT_TESTS, "❌ failed to parse limit: "+err.Error())
							return
						}
						if limitInt64 < 1 {
							s.ChannelMessageSend(CHANNEL_BOT_TESTS, "❌ limit must be >= 1")
							return
						}
						limit = uint(limitInt64)
					} else {
						s.ChannelMessageSend(CHANNEL_BOT_TESTS, "❌ invalid argument: "+arg)
						return
					}
				}

				readStr := "nil"
				if read != nil {
					readStr = fmt.Sprintf("%v", *read)
				}

				categoryStr := "nil"
				if category != nil {
					categoryStr = *category
				}

				s.ChannelMessageSend(CHANNEL_BOT_TESTS, "🐞 username: "+username+", read: `"+readStr+"`, category: `"+categoryStr+"`, limit: `"+strconv.FormatUint(uint64(limit), 10)+"`")

				// shared Hub DB client
				dbClient, err := dbclient.SharedUserDB()
				if err != nil {
					s.ChannelMessageSend(CHANNEL_BOT_TESTS, "❌ "+err.Error())
					return
				}

				usr, err := user.GetByUsername(dbClient, username)
				if err != nil {
					s.ChannelMessageSend(CHANNEL_BOT_TESTS, "❌ "+err.Error())
					return
				}
				if usr == nil {
					s.ChannelMessageSend(CHANNEL_BOT_TESTS, "❌ recipient user not found")
					return
				}

				// Get notifications

				scyllaClientUniverse := scyllaclient.GetShared("universe")
				if scyllaClientUniverse == nil {
					s.ChannelMessageSend(CHANNEL_BOT_TESTS, "❌ failed to get ScyllaClient: universe")
					return
				}

				opts := scyllaclient.GetNotificationsOpts{
					Read:     read,
					Category: category,
					Limit:    limit,
				}

				notifications, err := scyllaClientUniverse.GetUserNotifications(usr.ID, opts)
				if err != nil {
					s.ChannelMessageSend(CHANNEL_BOT_TESTS, "❌ db query failed:"+err.Error())
					return
				}

				if len(notifications) == 0 {
					s.ChannelMessageSend(CHANNEL_BOT_TESTS, "No notifications found")
					return
				}

				for _, n := range notifications {
					// s.ChannelMessageSend(CHANNEL_BOT_TESTS_ID, fmt.Sprintf("Notification: %v", n))
					s.ChannelMessageSend(CHANNEL_BOT_TESTS, "🔔 `"+n.Category+"` `"+strconv.FormatBool(n.Read)+"` "+n.Title+" - "+n.Message)
				}

				s.ChannelMessageSend(CHANNEL_BOT_TESTS, "✅ Done.")

			} else if strings.HasPrefix(content, "!notifMarkRead") {
				// Each filter is optional
				// !notifMarkRead <username> [category=category]

				args := parseCommandArguments(content)
				if len(args) < 1 || len(args) > 2 {
					s.ChannelMessageSend(CHANNEL_BOT_TESTS, "!notifMarkRead command expects [1-2] arguments. Got "+strconv.FormatInt(int64(len(args)), 10)+"\nexample: `!notifMarkRead gaetan category=money`")
					return
				}

				username := args[0]
				args = args[1:]
				var category *string = nil

				for _, arg := range args {
					if strings.HasPrefix(arg, "category=") {
						categoryValue := arg[9:]
						category = &categoryValue
					} else {
						s.ChannelMessageSend(CHANNEL_BOT_TESTS, "❌ invalid argument: "+arg)
						return
					}
				}

				// shared Hub DB client
				dbClient, err := dbclient.SharedUserDB()
				if err != nil {
					s.ChannelMessageSend(CHANNEL_BOT_TESTS, "❌ "+err.Error())
					return
				}

				usr, err := user.GetByUsername(dbClient, username)
				if err != nil {
					s.ChannelMessageSend(CHANNEL_BOT_TESTS, "❌ "+err.Error())
					return
				}
				if usr == nil {
					s.ChannelMessageSend(CHANNEL_BOT_TESTS, "❌ recipient user not found")
					return
				}

				scyllaClientUniverse := scyllaclient.GetShared("universe")
				if scyllaClientUniverse == nil {
					s.ChannelMessageSend(CHANNEL_BOT_TESTS, "❌ failed to get ScyllaClient: universe")
					return
				}

				filter := scyllaclient.NotificationFilter{
					Read:     nil,
					Category: category,
				}

				err = scyllaClientUniverse.MarkNotificationsAsRead(usr.ID, filter, true)
				if err != nil {
					s.ChannelMessageSend(CHANNEL_BOT_TESTS, "❌ db query failed:"+err.Error())
					return
				}

				s.ChannelMessageSend(CHANNEL_BOT_TESTS, "✅ Done.")

			} else if strings.HasPrefix(content, CMD_SEND_NOTIFICATION) {
				// !notif <username> <title> <message> <category>

				args := parseCommandArguments(content)
				if len(args) != 4 {
					s.ChannelMessageSend(CHANNEL_BOT_TESTS, "!notif command expects 4 arguments. Got "+strconv.FormatInt(int64(len(args)), 10)+"\nexample: `!notif <username> <title> <message> <category>`")
					return
				}

				recipient := args[0] // recipient (username or $team)
				title := args[1]     // title
				message := args[2]   // message
				category := args[3]  // category

				// Command validation
				if recipient == "" {
					s.ChannelMessageSend(CHANNEL_BOT_TESTS, "❌ recipient is empty")
					return
				}

				if title == "" {
					s.ChannelMessageSend(CHANNEL_BOT_TESTS, "❌ title is empty")
					return
				}

				if message == "" {
					s.ChannelMessageSend(CHANNEL_BOT_TESTS, "❌ message is empty")
					return
				}

				if category == "" {
					s.ChannelMessageSend(CHANNEL_BOT_TESTS, "❌ category is empty")
					return
				}
				{
					const NOTIFICATION_CATEGORY_GENERIC = "generic" // direct messages, follow, etc.
					const NOTIFICATION_CATEGORY_MONEY = "money"     // grants, purchases, etc.
					const NOTIFICATION_CATEGORY_SOCIAL = "social"   // friend requests etc.
					const NOTIFICATION_CATEGORY_LIKE = "like"       // items & worlds likes
					if category != NOTIFICATION_CATEGORY_SOCIAL && category != NOTIFICATION_CATEGORY_MONEY && category != NOTIFICATION_CATEGORY_GENERIC && category != NOTIFICATION_CATEGORY_LIKE {
						s.ChannelMessageSend(CHANNEL_BOT_TESTS, "❌ category is invalid")
						return
					}
				}

				// shared Hub DB client
				dbClient, err := dbclient.SharedUserDB()
				if err != nil {
					s.ChannelMessageSend(CHANNEL_BOT_TESTS, "❌ "+err.Error())
					return
				}

				usr, err := user.GetByUsername(dbClient, recipient)
				if err != nil {
					s.ChannelMessageSend(CHANNEL_BOT_TESTS, "❌ "+err.Error())
					return
				}
				if usr == nil {
					s.ChannelMessageSend(CHANNEL_BOT_TESTS, "❌ recipient user not found")
					return
				}

				scyllaClientUniverse := scyllaclient.GetShared("universe")
				if scyllaClientUniverse == nil {
					s.ChannelMessageSend(CHANNEL_BOT_TESTS, "❌ failed to get ScyllaClient: universe")
					return
				}

				// create notification
				notificationID, badgeCount, err := scyllaClientUniverse.CreateUserNotification(usr.ID, category, title, message)
				if err != nil {
					s.ChannelMessageSend(CHANNEL_BOT_TESTS, "❌ failed to create notification: "+err.Error())
					return
				}

				err = sendPushNotification(usr, category, title, message, notificationID, badgeCount)
				if err != nil {
					s.ChannelMessageSend(CHANNEL_BOT_TESTS, "❌ failed to send push notification: "+err.Error())
					return
				}

				s.ChannelMessageSend(CHANNEL_BOT_TESTS, "completed")

			} else if strings.HasPrefix(content, CMD_DAY1_NOTIFICATION) {
				// !day1notification [dryrun]

				args := parseCommandArguments(content)
				argsCount := len(args)
				if argsCount > 1 {
					s.ChannelMessageSend(CHANNEL_BOT_TESTS, "!day1notification command expects 0 or 1 argument. Got "+strconv.FormatInt(int64(len(args)), 10)+"\nexample: `!day1notification [dryrun]`")
					return
				}

				dryrun := false
				if argsCount == 1 {
					dryrun = args[0] == "dryrun"
				}

				opts := Day1NotificationsOpts{
					Dryrun:      dryrun,
					MinimalLogs: false,
				}

				sendDay1Notification(s, opts)

			} else if strings.HasPrefix(content, "!feature") {

				args := parseCommandArguments(content)
				if len(args) != 2 {
					s.ChannelMessageSend(CHANNEL_BOT_TESTS, "!feature command expects 2 arguments. Got "+strconv.FormatInt(int64(len(args)), 10)+"\nexample: `!feature yes <worldID>`")
					return
				}

				decisionStr := args[0] // yes or no
				worldID := args[1]     // UUID v4

				if (decisionStr != "yes" && decisionStr != "no") || worldID == "" {
					s.ChannelMessageSend(CHANNEL_BOT_TESTS, "!feature command unexpected arguments\nexample: `!feature yes <worldID>`")
					return
				}

				decision := decisionStr == "yes"

				err = featureWorld(decision, worldID)
				if err != nil {
					s.ChannelMessageSend(CHANNEL_BOT_TESTS, "❌ error: "+err.Error())
					return
				}

				if decision {
					s.ChannelMessageSend(CHANNEL_BOT_TESTS, "✅ world featured successfully!")
				} else {
					s.ChannelMessageSend(CHANNEL_BOT_TESTS, "✅ world unfeatured successfully!")
				}

				return

			} else if strings.HasPrefix(content, "!link") {

				usernameAndDiscriminator := m.Author.Username + "#" + m.Author.Discriminator

				// remove command name
				args := strings.TrimPrefix(content, "!link")
				args = strings.TrimSpace(args)

				// HELP subcommand
				if strings.HasPrefix(args, "help") || args == "" {
					s.ChannelMessageSend(CHANNEL_BOT_TESTS, "Hello "+usernameAndDiscriminator+",")
					s.ChannelMessageSend(CHANNEL_BOT_TESTS, "available commands:\n- create\n- update (soon)\n- delete (soon)\n- print")
					return
				}

				// PRINT subcommand
				if strings.HasPrefix(args, "print") || args == "" {

					// get link.cu.bzh config file
					response, err := http.Get("https://link.cu.bzh/internal/config")
					if err != nil {
						s.ChannelMessageSend(CHANNEL_BOT_TESTS, "❌ "+err.Error())
						return
					}
					defer response.Body.Close()

					// Create a File message with the file content
					fileMessage := &discordgo.File{
						Name:   "config.json",
						Reader: response.Body,
					}

					// Send the message with the file
					s.ChannelMessageSendComplex(m.ChannelID, &discordgo.MessageSend{
						Content: "📜 link.cu.bzh config:",
						File:    fileMessage,
					})

					return
				}

				// CREATE subcommand
				if strings.HasPrefix(args, "create") {
					cmdArgs := strings.TrimPrefix(args, "create")
					cmdArgs = strings.TrimSpace(cmdArgs)
					argsValues := strings.Fields(cmdArgs)
					if len(argsValues) != 2 {
						s.ChannelMessageSend(CHANNEL_BOT_TESTS, "!link create command expects 2 arguments. Got "+strconv.FormatInt(int64(len(argsValues)), 10))
						return
					}
					hash := argsValues[0]
					if !isAlphanumeric(hash) {
						s.ChannelMessageSend(CHANNEL_BOT_TESTS, "!link create 1st argument (hash) must be alphanumeric.")
						return
					}
					destinationURL := argsValues[1]
					if !isValidURL(destinationURL) {
						s.ChannelMessageSend(CHANNEL_BOT_TESTS, "!link create 2nd argument (destination URL) must be valid.")
						return
					}

					s.ChannelMessageSend(CHANNEL_BOT_TESTS, "creating link: `"+cmdArgs+"`")

					// Prepare body of HTTP request
					req := linkTypes.NewRedirect(hash, destinationURL, m.Author.Username)
					jsonData, err := json.Marshal(req)
					if err != nil {
						s.ChannelMessageSend(CHANNEL_BOT_TESTS, "❌ "+err.Error())
						return
					}

					// Send the POST request
					const url string = "https://link.cu.bzh/redirect"
					resp, err := http.Post(url, "application/json", bytes.NewBuffer(jsonData))
					if err != nil {
						s.ChannelMessageSend(CHANNEL_BOT_TESTS, "❌ "+err.Error())
						return
					}
					defer resp.Body.Close()

					// TODO: read response...
					body, err := io.ReadAll(resp.Body)
					if err != nil {
						s.ChannelMessageSend(CHANNEL_BOT_TESTS, "❌ "+err.Error())
						return
					}

					if resp.StatusCode == http.StatusOK {
						s.ChannelMessageSend(
							CHANNEL_BOT_TESTS,
							"✅ link has been created successfully.\n`"+req.Hash+"` >> `"+req.Destination+"`",
							discordgo.WithRestRetries(3))
						return
					}

					s.ChannelMessageSend(
						CHANNEL_BOT_TESTS,
						"❌ ERROR: "+strconv.FormatInt(int64(resp.StatusCode), 10)+" "+string(body),
						discordgo.WithRestRetries(3))

					return
				}

				// UPDATE subcommand
				if strings.HasPrefix(args, "update") {
					cmdArgs := strings.TrimPrefix(args, "update")
					s.ChannelMessageSend(CHANNEL_BOT_TESTS, "updating link: (soon)"+cmdArgs)
					return
				}

			} else if strings.HasPrefix(content, "!pingD1") {
				// !pingD1 [dryrun]

				args := parseCommandArguments(content)
				argsCount := len(args)
				if argsCount > 1 {
					s.ChannelMessageSend(CHANNEL_BOT_TESTS, "!pingD1 command expects 0 or 1 argument. Got "+strconv.FormatInt(int64(len(args)), 10)+"\nexample: `!pingD1 [dryrun]")
					return
				}

				// dryrun := false
				// if argsCount == 1 {
				// 	dryrun = args[0] == "dryrun"
				// }

				// shared Hub DB client
				dbClient, err := dbclient.SharedUserDB()
				if err != nil {
					s.ChannelMessageSend(CHANNEL_BOT_TESTS, "❌ "+err.Error())
					return
				}

				// get time corresponding to yesterday 00:00:00 and 23:59:59
				// yesterdayBegin := time.Now().AddDate(0, 0, -1).Truncate(24 * time.Hour)
				// yesterdayEnd := yesterdayBegin.Add(24*time.Hour - time.Second)
				yesterdayBegin := time.Now().Add((-23 - 24) * time.Hour)
				yesterdayEnd := time.Now().Add(-23 * time.Hour)

				// print times in UTC
				s.ChannelMessageSend(CHANNEL_BOT_TESTS, "🔍 looking for users created between "+yesterdayBegin.UTC().Format("2006-01-02 15:04:05")+" and "+yesterdayEnd.UTC().Format("2006-01-02 15:04:05"))

				users, err := user.GetByCreationDateBetween(dbClient, yesterdayBegin, yesterdayEnd)
				if err != nil {
					s.ChannelMessageSend(CHANNEL_BOT_TESTS, "❌ "+err.Error())
					return
				}

				s.ChannelMessageSend(CHANNEL_BOT_TESTS, "✅ pingD1 done. User count: "+strconv.FormatInt(int64(len(users)), 10))

				// s.ChannelMessageSend(CHANNEL_BOT_TESTS_ID, "✅ pingD1 done. (dryrun: "+strconv.FormatBool(dryrun)+")")

				// } else if strings.HasPrefix(content, "!autofriend") {
				// 	// !autofriend <username>

				// 	args := parseCommandArguments(content)
				// 	argsCount := len(args)
				// 	if argsCount != 1 {
				// 		s.ChannelMessageSend(CHANNEL_BOT_TESTS_ID, "!autofriend command expects 1 argument. Got "+strconv.FormatInt(int64(len(args)), 10)+"\nexample: `!autofriend <username>")
				// 		return
				// 	}

				// 	username := args[0]
				// 	if username == "" {
				// 		s.ChannelMessageSend(CHANNEL_BOT_TESTS_ID, "!autofriend command expects a non-empty username.")
				// 		return
				// 	}

				// 	// shared Hub DB client
				// 	dbClient, err := dbclient.SharedUserDB()
				// 	if err != nil {
				// 		s.ChannelMessageSend(CHANNEL_BOT_TESTS_ID, "❌ "+err.Error())
				// 		return
				// 	}

				// 	usr, err := user.GetByUsername(dbClient, username)
				// 	if err != nil || usr == nil {
				// 		s.ChannelMessageSend(CHANNEL_BOT_TESTS_ID, "❌ user not found: "+err.Error())
				// 		return
				// 	}

				// 	err = sendAutoFriendRequestIfNotDoneAlready(usr)
				// 	if err != nil {
				// 		s.ChannelMessageSend(CHANNEL_BOT_TESTS_ID, "❌ failed to send auto friend request: "+err.Error())
				// 		return
				// 	}

				// 	s.ChannelMessageSend(CHANNEL_BOT_TESTS_ID, "✅ auto friend request sent to: "+username)

			} else if strings.HasPrefix(content, "!getitemids") {
				getItemIDs(s, content)

			} else if strings.HasPrefix(content, "!cleanupNotifsAll") {
				// !cleanupNotifs [dryrun]

				args := parseCommandArguments(content)
				argsCount := len(args)
				if argsCount > 1 {
					s.ChannelMessageSend(CHANNEL_BOT_TESTS, "!cleanupNotifsAll command expects 0 or 1 argument. Got "+strconv.FormatInt(int64(len(args)), 10)+"\nexample: `!cleanupNotifsAll [dryrun]")
					return
				}

				dryrun := false
				if argsCount == 1 {
					dryrun = args[0] == "dryrun"
				}

				scyllaClientUniverse := scyllaclient.GetShared("universe")
				if scyllaClientUniverse == nil {
					s.ChannelMessageSend(CHANNEL_BOT_TESTS, "❌ failed to get ScyllaClient: universe")
					return
				}

				userIDs := make([]string, 0)
				userIDs, err = scyllaClientUniverse.GetUserNotificationsUsers()
				if err != nil {
					s.ChannelMessageSend(CHANNEL_BOT_TESTS, "❌ "+err.Error())
					return
				}

				// for _, userID := range userIDs {
				// 	if dryrun {
				// 		s.ChannelMessageSend(CHANNEL_BOT_TESTS_ID, "🔍 would cleanup notifications for user: "+userID)
				// 	} else {
				// 		err = scyllaClientUniverse.CleanupUserNotifications(userID)
				// 		if err != nil {
				// 			s.ChannelMessageSend(CHANNEL_BOT_TESTS_ID, "❌ "+err.Error())
				// 			return
				// 		}
				// 		s.ChannelMessageSend(CHANNEL_BOT_TESTS_ID, "✅ cleaned up notifications for user: "+userID)
				// 	}
				// }

				s.ChannelMessageSend(CHANNEL_BOT_TESTS, "✅🔍 found "+strconv.FormatInt(int64(len(userIDs)), 10)+" user(s) with notifications"+" (dryrun: "+strconv.FormatBool(dryrun)+")")

			} else if strings.HasPrefix(content, "!cleanupNotifs") {
				// !cleanupNotifs <username> [dryrun]

				args := parseCommandArguments(content)
				argsCount := len(args)
				if argsCount < 1 || argsCount > 2 {
					s.ChannelMessageSend(CHANNEL_BOT_TESTS, "!cleanupNotifs command expects 1 or 2 argument(s). Got "+strconv.FormatInt(int64(len(args)), 10)+"\nexample: `!cleanupNotifs <username> [dryrun]")
					return
				}

				username := args[0]
				dryrun := false
				if argsCount > 1 {
					dryrun = args[1] == "dryrun"
				}

				// Command validation
				if username == "" {
					s.ChannelMessageSend(CHANNEL_BOT_TESTS, "❌ username is empty")
					return
				}
				if username == "newbie" {
					s.ChannelMessageSend(CHANNEL_BOT_TESTS, "❌ cannot cleanup notifications for username: "+username)
					return
				}

				// shared Hub DB client
				dbClient, err := dbclient.SharedUserDB()
				if err != nil {
					s.ChannelMessageSend(CHANNEL_BOT_TESTS, "❌ "+err.Error())
					return
				}

				usr, err := user.GetByUsername(dbClient, username)
				if err != nil {
					s.ChannelMessageSend(CHANNEL_BOT_TESTS, "❌ "+err.Error())
					return
				}
				if usr == nil {
					s.ChannelMessageSend(CHANNEL_BOT_TESTS, "❌ user not found")
					return
				}

				scyllaClientUniverse := scyllaclient.GetShared("universe")
				if scyllaClientUniverse == nil {
					s.ChannelMessageSend(CHANNEL_BOT_TESTS, "❌ failed to get ScyllaClient: universe")
					return
				}

				// Get all notifications for the user
				notifArr, err := scyllaClientUniverse.GetUserNotifications(usr.ID, scyllaclient.GetNotificationsOpts{})
				if err != nil {
					s.ChannelMessageSend(CHANNEL_BOT_TESTS, "❌ failed to get notifications"+err.Error())
					return
				}

				for _, n := range notifArr {
					if strings.HasPrefix(n.Message, username+" liked your world! (") ||
						strings.HasPrefix(n.Message, username+" liked your item! (") {
						if dryrun {
							s.ChannelMessageSend(CHANNEL_BOT_TESTS, "[dryrun] notification to remove: "+n.Message)
						} else {
							err = scyllaClientUniverse.DeleteUserNotification(n.UserID, n.NotificationID)
							if err != nil {
								s.ChannelMessageSend(CHANNEL_BOT_TESTS, "❌ failed to delete notification"+err.Error())
							} else {
								s.ChannelMessageSend(CHANNEL_BOT_TESTS, "✅ notification removed: `"+n.Message+"`")
							}
						}
					}
				}

				if dryrun {
					s.ChannelMessageSend(CHANNEL_BOT_TESTS, "✅ Cleanup completed. (dryrun)")
				} else {
					s.ChannelMessageSend(CHANNEL_BOT_TESTS, "✅ Cleanup completed.")
				}

			} else if strings.HasPrefix(content, "!getuserswithbio") {
				usrArr, err := getUsersWithBio()
				if err != nil {
					s.ChannelMessageSend(CHANNEL_BOT_TESTS, "❌ error: "+err.Error())
				} else {
					output := "USERS (" + strconv.FormatInt(int64(len(usrArr)), 10) + ")"
					for _, usr := range usrArr {
						outputPart := ""
						outputPart += "\n" + usr.Username
						outputPart += "\n    bio:" + usr.Bio
						if usr.Discord != "" {
							outputPart += "\n    Discord:" + usr.Discord
						}
						if usr.Tiktok != "" {
							outputPart += "\n    Tiktok:" + usr.Tiktok
						}
						if usr.Website != "" {
							outputPart += "\n    Website:" + usr.Website
						}
						if usr.X != "" {
							outputPart += "\n    X:" + usr.X
						}
						if usr.Github != "" {
							outputPart += "\n    Github:" + usr.Github
						}
						if len(output)+len(outputPart) < DISCORD_MSG_MAX_LEN {
							output += outputPart
						} else {
							s.ChannelMessageSend(CHANNEL_BOT_TESTS, output)
							output = outputPart
						}
					}
					s.ChannelMessageSend(CHANNEL_BOT_TESTS, output)

					s.ChannelMessageSend(CHANNEL_BOT_TESTS, "\n✅")
				}

			} else if strings.HasPrefix(content, "!banitem") {
				itemName := strings.TrimPrefix(content, "!banitem ")
				err = banItem(itemName)
				if err != nil {
					s.ChannelMessageSend(CHANNEL_BOT_TESTS, "`"+itemName+"` ban error: "+err.Error())
				} else {
					s.ChannelMessageSend(CHANNEL_BOT_TESTS, "`"+itemName+"` ban added ✨")
				}
			} else if strings.HasPrefix(content, "!unbanitem") {
				itemName := strings.TrimPrefix(content, "!unbanitem ")
				err = unbanItem(itemName)
				if err != nil {
					s.ChannelMessageSend(CHANNEL_BOT_TESTS, "`"+itemName+"` unban error: "+err.Error())
				} else {
					s.ChannelMessageSend(CHANNEL_BOT_TESTS, "`"+itemName+"` ban removed ✨")
				}
			} else if strings.HasPrefix(content, "!isbanneditem") {
				itemName := strings.TrimPrefix(content, "!isbanneditem ")
				isBanned, err := itemIsBanned(itemName)
				if err != nil {
					s.ChannelMessageSend(CHANNEL_BOT_TESTS, "isbanneditem `"+itemName+"` error: "+err.Error())
				} else if isBanned {
					s.ChannelMessageSend(CHANNEL_BOT_TESTS, "`"+itemName+"` is banned ⛔️")
				} else {
					s.ChannelMessageSend(CHANNEL_BOT_TESTS, "`"+itemName+"` is not banned 👌")
				}
			} else if strings.HasPrefix(content, "!gameAddContentWarn") {
				elems := strings.Split(content, " ")
				if len(elems) != 3 || elems[0] != "!gameAddContentWarn" {
					s.ChannelMessageSend(CHANNEL_BOT_TESTS, "❌ bad command")
				} else {
					gameID := elems[1]
					contentWarning := elems[2]
					err = gameAddContentWarning(gameID, contentWarning)
					if err != nil {
						s.ChannelMessageSend(CHANNEL_BOT_TESTS, "failed to add content warning to game: "+err.Error())
					} else {
						s.ChannelMessageSend(CHANNEL_BOT_TESTS, "added content warning to game ✅")
					}
				}
			} else if strings.HasPrefix(content, "!gameGetContentWarn") {
				elems := strings.Split(content, " ")
				if len(elems) != 2 || elems[0] != "!gameGetContentWarn" {
					s.ChannelMessageSend(CHANNEL_BOT_TESTS, "❌ bad command")
				} else {
					gameID := elems[1]
					contentWarnings, err := gameGetContentWarnings(gameID)
					if err != nil {
						s.ChannelMessageSend(CHANNEL_BOT_TESTS, "failed to get content warning to game: "+err.Error())
					} else {
						s.ChannelMessageSend(CHANNEL_BOT_TESTS, "Content warnings:")
						for _, cw := range contentWarnings {
							s.ChannelMessageSend(CHANNEL_BOT_TESTS, "  - "+string(cw))
						}
					}
				}
			} else {
				s.ChannelMessageSend(CHANNEL_BOT_TESTS, "🤨 unrecognized command")
			}
			// } else if strings.HasPrefix(content, "!flushtransactions") {

			// 	// parse command
			// 	args := parseCommandArguments(content)
			// 	if len(args) > 2 {
			// 		s.ChannelMessageSend(CHANNEL_BOT_TESTS_ID, "!flushtransactions command expects 1 argument. Got "+strconv.FormatInt(int64(len(args)), 10)+"\nexample: `!flushtransactions <username>`")
			// 		return
			// 	}
			// 	username := args[0]
		}
	}

	if m.ChannelID == MULTIPLAYER_CHANNEL_ID {
		fmt.Println("Interaction:", m.Interaction)

		// count += 1
		s.ChannelMessageDelete(m.ChannelID, m.ID)
	}
}

func getItemIDs(s *discordgo.Session, content string) {
	dbClient, err := dbclient.SharedUserDB()
	if err != nil {
		s.ChannelMessageSend(CHANNEL_BOT_TESTS, "failed to get db client: "+err.Error())
		return
	}

	fields := strings.Fields(content)
	// remove first element
	fields = fields[1:]
	for _, itemFullName := range fields {
		// split repo and name on "."
		repo, name, err := item.ParseRepoDotName(itemFullName)
		if err != nil {
			s.ChannelMessageSend(CHANNEL_BOT_TESTS, "failed to parse item full name: "+err.Error())
			return
		}
		items, err := item.GetByRepoAndName(dbClient, repo, name)
		if err != nil {
			s.ChannelMessageSend(CHANNEL_BOT_TESTS, "failed to get item by repo and name: "+err.Error())
			return
		}
		if len(items) != 1 {
			s.ChannelMessageSend(CHANNEL_BOT_TESTS, "found "+strconv.Itoa(len(items))+" items with repo and name: "+repo+"."+name)
			return
		}
		s.ChannelMessageSend(CHANNEL_BOT_TESTS, items[0].ID)
	}
}

func reactionAdd(s *discordgo.Session, m *discordgo.MessageReactionAdd) {
	if m.MessageID == MULTIPLAYER_MESSAGE_ID {
		fmt.Println("reaction added")
	}
}

func reactionRemove(s *discordgo.Session, m *discordgo.MessageReactionRemove) {
	if m.MessageID == MULTIPLAYER_MESSAGE_ID {
		fmt.Println("reaction removed")
	}
}

func boolToEmoji(b bool) string {
	if b {
		return "✅"
	}
	return "🔥"
}

func updateServerStatus() {
	cubzh, docs, api, app, hfErr = pingAllServers()

	content := "🤖 SERVICES STATUS 🤖\n"
	content += "==============================\n"
	content += boolToEmoji(cubzh) + " Website\n"
	content += boolToEmoji(docs) + " Documentation\n"
	content += boolToEmoji(api) + " API\n"
	content += boolToEmoji(app) + " Wasm App\n"
	content += boolToEmoji(hfErr == nil) + " HuggingFace endpoint\n"
	if hfErr != nil {
		content += "HF ERROR: " + hfErr.Error() + "\n"
	}
	content += "==============================\n"
	content += "last update: " + time.Now().Format("📅 2006-01-02  ⏰ 15:04:05 UTC") + "\n\n"

	if !(cubzh && docs && api && app && hfErr == nil) {
		content += "⚠️ **WARNING** ⚠️\n"
		content += "<@" + DISCORD_USER_GDEVILLELE + ">"
		content += "<@" + DISCORD_USER_ADUERMAEL + ">"
	}

	dg.ChannelMessageEdit(DASHBOARD_CHANNEL_ID, DASHBOARD_FIRST_MESSAGE_ID, content)
}

// func updateBackup() {

// 	for {

// 		content := ""
// 		content += "📦 BACKUP 📦\n"
// 		content += "==============================\n"
// 		content += "Backup is currently running. Please wait a few minutes before trying to access it again.\n"
// 		content += "==============================\n\n"

// 		dg.ChannelMessageEdit(DASHBOARD_CHANNEL_ID, DASHBOARD_SECOND_MESSAGE_ID, content)

// 		backup()

// 		date := time.Now().Format("2006-01-02")

// 		content = ""
// 		content += "📦 BACKUP 📦\n"
// 		content += "==============================\n"
// 		content += "Backup for the " + date + " is now available\n"
// 		content += "==============================\n\n"

// 		content += "<@" + USER_MOUTOO + ">"

// 		dg.ChannelMessageEdit(DASHBOARD_CHANNEL_ID, DASHBOARD_SECOND_MESSAGE_ID, content)

// 		time.Sleep(DELAY_BACKUP)
// 	}
// }

func updateMultiplayerMessage(worlds map[string]*World) {
	m, err := dg.ChannelMessage(MULTIPLAYER_CHANNEL_ID, MULTIPLAYER_MESSAGE_ID)
	if err != nil {
		return
	}

	// totalReactions := 0
	// for _, r := range m.Reactions {
	// 	totalReactions += r.Count
	// }

	content := ""
	content += "🕹️ **MULTIPLAYER WORLDS** ✨\n"
	content += "==============================\n\n"
	content += "Even though we all know that Cubzh is going to take over the world, these are still the early days. As a pioneer, it may be challenging for you to connect with other players. So here's a list of hand picked multiplayer experiences with real time connected player information:\n"
	content += "\n"

	emoji := ""

	sorted := make([]*World, 0, len(worlds))
	for _, w := range worlds {
		sorted = append(sorted, w)
	}

	sort.Slice(sorted[:], func(i, j int) bool {
		return sorted[i].Title > sorted[j].Title
	})

	for _, w := range sorted {

		if w.OnlinePlayers > 5 {
			emoji = "🔥"
		} else if w.OnlinePlayers == 0 {
			emoji = "😴"
		} else {
			emoji = "🕹️"
		}
		content += emoji + " **" + w.Title + "** (**" + strconv.Itoa(w.OnlinePlayers) + "** online) - 🔗 " + w.Link + "\n"
		content += "\n"
	}

	content += "=============================="

	dg.ChannelMessageEdit(m.ChannelID, MULTIPLAYER_MESSAGE_ID, content)

	for wID, w := range worlds {
		previous, found := worldsPreviousState[wID]
		if w.OnlinePlayers > 0 && (!found || previous.OnlinePlayers == 0) {
			msg := "🤖🕹️ " + strconv.Itoa(w.OnlinePlayers) + " player(s) in " + w.Title + " - " + w.Link
			msg += " ( "
			for _, userID := range TEAM_PLAYERS {
				msg += "<@" + userID + "> "
			}
			msg += ")"

			dg.ChannelMessageSend(LETS_PLAY_CHANNEL_ID, msg)
		}
	}

	worldsPreviousState = worlds
}

func serversInfo(w http.ResponseWriter, r *http.Request) {
	var worlds map[string]*World
	err := json.NewDecoder(r.Body).Decode(&worlds)
	if err != nil {
		respondWithError(http.StatusBadRequest, "can't decode json request", w)
		return
	}

	updateMultiplayerMessage(worlds)

	respondOK(w)
}

type serverError struct {
	Code    int    `json:"code,omitempty"`
	Message string `json:"msg,omitempty"`
}

func respondOK(w http.ResponseWriter) {
	w.Header().Set("Content-Type", "application/json")
	w.WriteHeader(http.StatusOK)
	fmt.Fprint(w, "{}")
}

func respondWithError(code int, msg string, w http.ResponseWriter) {
	respondWithJSON(code, &serverError{Code: code, Message: msg}, w)
}

func respondWithJSON(code int, payload interface{}, w http.ResponseWriter) {
	w.Header().Set("Content-Type", "application/json")
	w.WriteHeader(code)
	json.NewEncoder(w).Encode(payload)
}

func notFound(w http.ResponseWriter, r *http.Request) {
	fmt.Println("[🐞] HTTP NOT FOUND")
	respondWithError(http.StatusNotFound, "not found", w)
}

func contains(s []string, str string) bool {
	for _, v := range s {
		if v == str {
			return true
		}
	}
	return false
}

func isTeamMember(userID string) bool {
	return contains(TEAM_MEMBERS, userID)
}

func isAlphanumeric(s string) bool {
	for _, char := range s {
		if !unicode.IsLetter(char) && !unicode.IsNumber(char) {
			return false
		}
	}
	return true
}

func isValidURL(input string) bool {
	// Attempt to parse the URL
	u, err := url.Parse(input)

	// Check if there was an error during parsing
	if err != nil {
		return false
	}

	// Check if the URL is a valid absolute URL
	return u.IsAbs()
}

// TODO: use common implementation between discordbot and apiserver
func sendPushNotification(recipient *user.User, category, title, message, notificationID string, badgeCount uint) error {
	var err error

	// Apple tokens
	if len(recipient.ApplePushTokens) > 0 {
		c := apnsclient.Shared(apnsclient.ClientOpts{
			APNSAuthKeyFilepath: APNS_AUTH_KEY_FILEPATH,
		})
		if c != nil {
			sound := "notif.caf"
			if category == "money" {
				sound = "notif_money.caf"
			}
			for _, token := range recipient.ApplePushTokens {
				err = c.SendNotification(token.Token, token.Variant, category, title, message, notificationID, sound, badgeCount)
				if err != nil {
					fmt.Println("❌ failed to trigger Apple Push Notif:", err.Error())
				}
				// TODO: gaetan: remove invalid tokens (when error reason is "BadDeviceToken")
			}
		}
	}

	// Firebase tokens
	if len(recipient.FirebasePushTokens) > 0 {
		c := fcmclient.Shared(fcmclient.ClientOpts{
			FirebaseAccountJSONFilePath: FIREBASE_ACCOUNT_JSON_FILEPATH,
		})
		if c != nil {
			sound := "notif.caf"
			if category == "money" {
				sound = "notif_money.caf"
			}
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

	return nil
}

type Day1NotificationsOpts struct {
	Dryrun      bool
	MinimalLogs bool
}

func sendDay1Notification(s *discordgo.Session, opts Day1NotificationsOpts) {
	if !opts.MinimalLogs {
		s.ChannelMessageSend(CHANNEL_BOT_TESTS, "📣 pushing day1 notification... (dryrun: "+strconv.FormatBool(opts.Dryrun)+")")
	}

	dbClient, err := dbclient.SharedUserDB()
	if err != nil {
		s.ChannelMessageSend(CHANNEL_BOT_TESTS, "❌ ERROR: failed to get dbclient")
		return
	}

	prefix := ""
	if opts.Dryrun {
		prefix += "[DRYRUN] "
	}

	// get time corresponding to now - 23h
	yesterdayBegin := time.Now().UTC().Add((-23 - 24) * time.Hour)
	yesterdayEnd := time.Now().UTC().Add(-23 * time.Hour)

	users, err := user.GetByCreationDateBetween(dbClient, yesterdayBegin, yesterdayEnd)
	if err != nil {
		s.ChannelMessageSend(CHANNEL_BOT_TESTS, "❌ ERROR: failed to get users")
		return
	}

	if !opts.MinimalLogs {
		s.ChannelMessageSend(CHANNEL_BOT_TESTS, prefix+"⏰🔍 looking for users created between "+yesterdayBegin.Format("2006-01-02 15:04:05")+" and "+yesterdayEnd.Format("2006-01-02 15:04:05"))
		s.ChannelMessageSend(CHANNEL_BOT_TESTS, prefix+"⏰🔍 found "+strconv.Itoa(len(users))+" users")
	}

	// Count the users that have not received the day1 notification yet
	{
		count := 0
		for _, usr := range users {
			if usr.AutoFriendRequestCount < 1 {
				count += 1
			}
		}

		if !opts.MinimalLogs {
			s.ChannelMessageSend(CHANNEL_BOT_TESTS, prefix+"⏰🔍 found "+strconv.Itoa(count)+" users to send day 1 auto friend request to")
		}
	}

	var printUsername string
	if !opts.Dryrun {
		for _, usr := range users {
			if usr.Username == "" {
				printUsername = "newbie"
			} else {
				printUsername = usr.Username
			}
			sent, err := sendAutoFriendRequestIfNotDoneAlready(usr, opts.Dryrun)
			if err != nil {
				s.ChannelMessageSend(CHANNEL_BOT_TESTS, prefix+"[DAY 1]❌ failed to send auto friend request to "+usr.ID+" "+printUsername+" "+err.Error())
			} else if sent {
				s.ChannelMessageSend(CHANNEL_BOT_TESTS, prefix+"[DAY 1]✅ auto friend request sent to "+printUsername+" "+usr.ID)
			} else {
				// not sent because already done
				// s.ChannelMessageSend(CHANNEL_BOT_TESTS_ID, prefix+"⏰🔍👌 auto friend request already sent to "+usr.ID+" "+printUsername)
			}
		}
	}
}

func sendAutoFriendRequestIfNotDoneAlready(recipientUsr *user.User, dryrun bool) (sent bool, err error) {
	sent = false

	if recipientUsr == nil {
		err = errors.New("user is nil")
		return
	}

	// shared Hub DB client
	dbClient, err := dbclient.SharedUserDB()
	if err != nil {
		return
	}

	scyllaClientUniverse := scyllaclient.GetShared("universe")
	if scyllaClientUniverse == nil {
		err = errors.New("scyllaClientUniverse is nil")
		return
	}

	// send auto friend request
	teamIDs := []string{
		"4d558bc1-5700-4a0d-8c68-f05e0b97f3fd", // aduermael
		"59991c61-cb96-4a11-adc6-452f58931bde", // gaetan
	}

	if recipientUsr.AutoFriendRequestCount >= 1 {
		// sent == false
		return
	}

	// recipientUsr.AutoFriendRequestCount is < 1

	// choose one team member randomly
	randomFriendID := teamIDs[rand.Intn(len(teamIDs))]

	newFriendUsr, err := user.GetByID(dbClient, randomFriendID)
	if err != nil {
		return
	}

	if !dryrun { // Real run

		// create friend request
		err = CreateFriendRequest(*scyllaClientUniverse, newFriendUsr, recipientUsr)
		if err != nil {
			return
		}

		// increment auto-friend count
		recipientUsr.AutoFriendRequestCount += 1
		err = recipientUsr.Save(dbClient)
		if err != nil {
			return
		}
	}

	sent = true

	return
}

const NOTIFICATION_CATEGORY_SOCIAL = "social" // friend requests etc.

func CreateFriendRequest(scyllaClientUniverse scyllaclient.Client, senderUsr, recipientUsr *user.User) error {
	// ScyllaDB
	scyllaResult, err := scyllaClientUniverse.InsertFriendRequest(senderUsr.ID, senderUsr.Username, recipientUsr.ID, recipientUsr.Username)
	if err != nil {
		return err
	}

	if scyllaResult == scyllaclient.FriendRequestCreated {
		// send Push notification to recipient user
		go func() {
			// create notification
			category := NOTIFICATION_CATEGORY_SOCIAL
			title := "💛 FRIEND REQUEST!"
			username := senderUsr.Username
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
			username := senderUsr.Username
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

	return nil
}
