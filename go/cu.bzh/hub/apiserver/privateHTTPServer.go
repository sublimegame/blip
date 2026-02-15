package main

import (
	"bytes"
	"encoding/json"
	"fmt"
	"net/http"
	"time"

	"github.com/go-chi/chi/v5"

	"cu.bzh/ggwp"
	"cu.bzh/hub/game"
	"cu.bzh/hub/types"
	"cu.bzh/utils"
)

type TrackedWorld struct {
	Title         string `json:"title,omitempty"`
	Description   string `json:"description,omitempty"`
	OnlinePlayers int32  `json:"online-players,omitempty"`
	Link          string `json:"link,omitempty"`
}

var (
	trackedMultiplayerWorlds = map[string]*TrackedWorld{
		"3ddb4a98-2fc7-4658-995a-26616363bf2d": {
			Title:         "Quakzh",
			Description:   "Fast-paced first person shooter. Only one weapon, break others into pieces!",
			OnlinePlayers: 0,
			Link:          "https://app.cu.bzh/?worldID=3ddb4a98-2fc7-4658-995a-26616363bf2d",
		},
		"63e63b3c-05c1-493b-8715-23a0842c1175": {
			Title:         "Retro Motor Club",
			Description:   "Survive in this arena dodging walls placed by your opponents and yourself!",
			OnlinePlayers: 0,
			Link:          "https://app.cu.bzh/?worldID=63e63b3c-05c1-493b-8715-23a0842c1175",
		},
		"13693497-03fd-4492-9b36-9776bb11d958": {
			Title:         "Eat your fruits",
			Description:   "Fast-paced FPS where you punch trees to harvest fruits and throw them at others.",
			OnlinePlayers: 0,
			Link:          "https://app.cu.bzh/?worldID=13693497-03fd-4492-9b36-9776bb11d958",
		},
		"c02a3c8e-f54d-4de7-97d4-37c1cd64aca1": {
			Title:         "dustzh",
			Description:   "Fast-paced FPS inpired from a true classic of the genre.",
			OnlinePlayers: 0,
			Link:          "https://app.cu.bzh/?worldID=c02a3c8e-f54d-4de7-97d4-37c1cd64aca1",
		},
		"1b6b1cc3-67f2-4236-b22d-72bfac7e2161": {
			Title:         "hub",
			Description:   "A cool place to explore and chill.",
			OnlinePlayers: 0,
			Link:          "https://app.cu.bzh/?worldID=1b6b1cc3-67f2-4236-b22d-72bfac7e2161",
		},
		"62aa3fcc-6263-4fb6-ba43-a00a3a8dc80d": {
			Title:         "Ridiculous Chess",
			Description:   "A great chess version.",
			OnlinePlayers: 0,
			Link:          "https://app.cu.bzh/?worldID=62aa3fcc-6263-4fb6-ba43-a00a3a8dc80d",
		},
	}
)

func privateGetItemModel(w http.ResponseWriter, r *http.Request) {

	itemRepo := chi.URLParam(r, "repo")
	itemName := chi.URLParam(r, "name")
	itemID := chi.URLParam(r, "id")

	etag := ""
	maxAge := ""
	getItemModelFile(itemRepo, itemName, itemID, etag, maxAge, w, r)
}

type postServerInfoReq struct {
	ID        string `json:"id,omitempty"`      // server id
	NBPlayers int    `json:"players,omitempty"` // server id
}

func postServerInfo(w http.ResponseWriter, r *http.Request) {

	var req postServerInfoReq
	err := json.NewDecoder(r.Body).Decode(&req)
	if err != nil {
		respondWithError(http.StatusBadRequest, "can't decode json request", w)
		return
	}

	err, found := game.UpdateServer(req.ID, req.NBPlayers)
	if err != nil {
		respondWithError(http.StatusInternalServerError, "", w)
		return
	}

	if found == false {
		respondWithError(http.StatusBadRequest, "gameserver unknown", w)
		return
	}

	gameID, err := game.GetServerGameID(req.ID)
	if err != nil {
		fmt.Println("🔥 can't get game ID from server to increment views")
		respondWithError(http.StatusInternalServerError, "can't get game ID from server to increment views", w)
		return
	}

	if _, found := trackedMultiplayerWorlds[gameID]; found {
		updateDiscordBot()
	}

	respondOK(w)
}

func postServerExit(w http.ResponseWriter, r *http.Request) {

	serverID := chi.URLParam(r, "id")

	if serverID == "" {
		respondWithError(http.StatusBadRequest, "", w)
		return
	}

	err := game.RemoveServer(serverID)
	if err != nil {
		respondWithError(http.StatusInternalServerError, "", w)
		return
	}

	respondOK(w)
}

func updateDiscordBot() {
	fmt.Println("🤖 updateDiscordBot")
	for wID, w := range trackedMultiplayerWorlds {

		w.OnlinePlayers = 0
		world := game.NewReadOnly(wID)
		// not filtering on region, as we want all servers
		servers, err := world.GetServers(DISCORDBOT_NOTIF_GAMESERVER_TAG, types.RegionZero(), "play")
		if err != nil {
			fmt.Println("❌ updateDiscordBot (1):", err.Error())
			return
		}

		for _, s := range servers {
			w.OnlinePlayers = w.OnlinePlayers + s.Players
		}
	}

	jsonValue, err := json.Marshal(trackedMultiplayerWorlds)
	if err != nil {
		fmt.Println("❌ updateDiscordBot (2):", err.Error())
		return
	}

	resp, err := http.Post("http://discordbot/servers-info", "application/json", bytes.NewBuffer(jsonValue))
	if err != nil {
		fmt.Println("❌ updateDiscordBot (3):", err.Error())
		return
	}

	if resp.StatusCode != http.StatusOK {
		fmt.Println("❌ updateDiscordBot (4): status code:", resp.StatusCode)
		return
	}
}

type ChatMsgStatus int

const (
	CHAT_MSG_STATUS_OK       ChatMsgStatus = 0
	CHAT_MSG_STATUS_ERROR    ChatMsgStatus = 1
	CHAT_MSG_STATUS_REPORTED ChatMsgStatus = 2
)

func postChatMessage(w http.ResponseWriter, r *http.Request) {

	type Request struct {
		SenderUUID string `json:"authorID,omitempty"`
		ServerID   string `json:"serverID,omitempty"`
		Content    string `json:"content,omitempty"`
		// Mentions       []string `json:"mentions"`
	}

	type Response struct {
		MessageUUID string        `json:"msgUUID,omitempty"`
		Status      ChatMsgStatus `json:"status"`
	}

	var err error

	response := &Response{
		MessageUUID: "00000000-0000-0000-0000-000000000000",
		Status:      CHAT_MSG_STATUS_ERROR,
	}

	// parse request
	var req Request
	err = json.NewDecoder(r.Body).Decode(&req)
	if err != nil {
		fmt.Println("❌ can't decode json request")
		respondWithJSON(http.StatusOK, response, w)
		return
	}

	// generate UUID for chat message
	msgUUID, err := utils.UUID()
	if err != nil {
		fmt.Println("❌ can't generate UUID")
		respondWithJSON(http.StatusOK, response, w)
		return
	}
	response.MessageUUID = msgUUID

	// consider message is OK from here
	response.Status = CHAT_MSG_STATUS_OK

	// verify with GGWP
	ggwpResp, err := ggwp.PostChatMessage(req.Content, req.SenderUUID, req.ServerID, ggwp.Opts{Timeout: 2 * time.Second})
	if err != nil || ggwpResp == nil {
		// if err != nil, it means we couldn't get an answer from GGWP
		// let the message go through in that case
		fmt.Println("❌ ggwp.PostChatMessage error:", err)
	} else {
		if ggwpResp.IsMessageOk() == false {
			response.Status = CHAT_MSG_STATUS_REPORTED
		}
	}

	// Store chat message in database
	{
		// TODO: status should also be stored
		err = scyllaClientModeration.InsertChatMessage(msgUUID, req.SenderUUID, req.Content, req.ServerID)
		if err != nil {
			// message couldn't be stored
			// we can't transmit it to other players safely
			// change status to CHAT_MSG_STATUS_ERROR
			fmt.Println("❌ postChatMessage:", err)
			response.Status = CHAT_MSG_STATUS_ERROR
		}
	}

	// Generate a random number between 0 and 3
	// randomNumber := rand.Intn(4)
	// if randomNumber < 2 {
	// 	response.Status = CHAT_MSG_STATUS_OK // 50% chance
	// } else if randomNumber < 3 {
	// 	response.Status = CHAT_MSG_STATUS_REPORTED // 25% chance
	// } else {
	// 	response.Status = CHAT_MSG_STATUS_ERROR // 25% chance
	// }

	respondWithJSON(http.StatusOK, response, w)
}
