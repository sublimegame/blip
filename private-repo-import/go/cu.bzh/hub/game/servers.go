package game

import (
	"bytes"
	"encoding/json"
	"errors"
	"fmt"
	"io"
	"net/http"
	"strconv"
	"strings"
	"time"

	"github.com/gomodule/redigo/redis"

	"cu.bzh/hub/dbclient"
	"cu.bzh/hub/types"
	schedulerTypes "cu.bzh/scheduler/types"
	"cu.bzh/types/gameserver"
)

const (
	BACKGROUND_ROUTINE_INTERVAL = 60 * time.Second // 60 seconds
)

func init() {
	go backgroundRoutine()
}

// Server describes a game server
type Server struct {
	// Generated ID
	ID string `json:"id,omitempty"`
	// host
	Host string `json:"host,omitempty"`
	// port
	Port string `json:"port,omitempty"`
	// path
	// Path string `json:"path,omitempty"`
	// address to join the game (Addr = host:Port)
	Addr string `json:"addr,omitempty"`
	// ID of game being served
	GameID string `json:"gameid,omitempty"`
	// number of connected players
	Players int32 `json:"players,omitempty"`
	// Max Players
	MaxPlayers int32 `json:"maxPlayers,omitempty"`
	// Image tag
	ImageTag string `json:"tag,omitempty"`
	// Field used by redis to return errors
	Error string `json:"error,omitempty"`
	// Dev mode
	Mode gameserver.Mode `json:"mode,omitempty"`
	// Auth token
	AuthToken string `json:"authToken,omitempty"`
}

// Servers ...
type Servers struct {
	// Server list
	Servers []*Server `json:"servers,omitempty"`
	// Field used by redis to return errors
	Error string `json:"error,omitempty"`
}

func backgroundRoutine() {
	for {
		cleanGameServers()
		time.Sleep(BACKGROUND_ROUTINE_INTERVAL)
	}
}

// removes server entries from when they're gone
// (not getting heartbeats from them)
func cleanGameServers() {

	redisConn := redisPool.Get()
	defer redisConn.Close()

	// nbRemoved, ok := res.(int64)

	res, err := scriptCleanGameServers.Do(redisConn)
	if err != nil {
		fmt.Println("could not clean game servers:", err.Error())
		return
	}

	byteSlice, ok := res.([]byte)

	if !ok {
		fmt.Println("cleanGameServers: can't cast response")
		return
	}

	var serverIDs []string

	err = json.Unmarshal(byteSlice, &serverIDs)

	if err != nil {
		fmt.Println("json unmarshall error:", err.Error())
		return
	}

	if len(serverIDs) == 0 {
		return
	}

	fmt.Println("SERVERS REMOVED:", serverIDs)
}

// GetServers returns running server for given game
// Ideally, they should be ordered, smaller pings first
func (g *Game) GetServers(gameserverTag string, region types.Region, gameServerMode gameserver.Mode) ([]*Server, error) {

	// make sure `region` is not nil
	if region == nil {
		return nil, errors.New("region is nil")
	}

	// make sure `mode` is valid
	err := gameserver.ValidateMode(gameServerMode)
	if err != nil {
		return nil, err
	}

	redisConn := redisPool.Get()
	defer redisConn.Close()

	tag := gameserverTag
	if tag == "" {
		return nil, errors.New("gameserver tag was not provided")
	}

	servers := &Servers{}

	res, err := scriptGetGameServers.Do(
		redisConn,
		g.ID,
		tag,
		region.String(),
		gameServerMode,
	)
	if err != nil {
		fmt.Println("❌ ERROR:", err.Error())
		return nil, err
	}

	byteSlice, ok := res.([]byte)
	if !ok {
		fmt.Println("❌ can't cast response")
		return nil, errors.New("can't cast response")
	}

	err = json.Unmarshal(byteSlice, servers)
	if err != nil {
		fmt.Println("❌ json unmarshall error:", err.Error())
		return nil, err
	}

	if servers.Error != "" {
		fmt.Println("❌ error:", servers.Error)
		return nil, errors.New(servers.Error)
	}

	return servers.Servers, nil
}

// StartServer spins up a new server for the game.
func (g *Game) StartServer(gameserverTag string, region types.Region, gameserverMode gameserver.Mode, clientInfo schedulerTypes.ClientInfo, authToken string) (*Server, error) {

	// arguments validation
	if gameserverTag == "" {
		return nil, errors.New("game server tag cannot be empty")
	}
	// make sure `region` is not nil
	if region == nil {
		return nil, errors.New("region is nil")
	}
	if authToken == "" {
		return nil, errors.New("auth token cannot be empty")
	}
	// validate region
	schedulerURL := ""
	switch region {
	case types.RegionEU():
		schedulerURL = "http://gameservers-eu"
	case types.RegionUS():
		schedulerURL = "http://gameservers-us"
	case types.RegionSG():
		schedulerURL = "http://gameservers-sg"
	default:
		fmt.Println("❌ invalid region:", region)
		return nil, errors.New("invalid region")
	}
	// validate mode
	err := gameserver.ValidateMode(gameserverMode)
	if err != nil {
		return nil, err
	}
	// validate client info?
	// ...
	// validate auth token
	if authToken == "" {
		return nil, errors.New("auth token cannot be empty")
	}

	redisConn := redisPool.Get()
	defer redisConn.Close()

	// read-only game only have an ID,
	// we need to retrieve game.MaxPlayers
	if g.ReadOnly {
		dbClient, err := dbclient.SharedUserDB()
		if err != nil {
			return nil, err
		}
		completeGame, err := GetByID(dbClient, g.ID, "", &clientInfo.ClientBuildNumber)
		if err != nil {
			return nil, err
		}
		if completeGame == nil {
			return nil, errors.New("world not found")
		}
		g.MaxPlayers = completeGame.MaxPlayers
	}

	// validate max players
	if g.MaxPlayers <= 0 {
		g.MaxPlayers = DEFAULT_MAX_PLAYERS
	}
	if g.MaxPlayers > MAX_PLAYERS_LIMIT {
		g.MaxPlayers = MAX_PLAYERS_LIMIT
	}

	// NOTE: requests for the same game and region are queued,
	// they can't be processed at the same time.
	// So it's ok if the server creation takes time.

	// Prepare request for scheduler
	request := &schedulerTypes.RequestCreateGameServer{
		Image:      "registry.digitalocean.com/cubzh/gameserver:" + gameserverTag,
		GameID:     g.ID,
		Mode:       gameserverMode,
		MaxPlayers: g.MaxPlayers,
		ClientInfo: clientInfo,
		AuthToken:  authToken,
	}

	requestBody, err := json.Marshal(request)
	if err != nil {
		fmt.Println("json marshal error:", err.Error())
		return nil, err
	}

	resp, err := http.Post(schedulerURL+"/create", "application/json", bytes.NewBuffer(requestBody))
	if err != nil {
		fmt.Println("POST /create error (1):", err.Error())
		return nil, err
	}
	defer resp.Body.Close()

	responseBody, err := io.ReadAll(resp.Body)
	if err != nil {
		fmt.Println("POST /create error (2):", err.Error())
		return nil, err
	}

	if resp.StatusCode != http.StatusOK {
		fmt.Println("POST /create error (3) - status not OK:", string(responseBody))
		return nil, errors.New("request status:" + strconv.Itoa(resp.StatusCode))
	}

	// add gameserver to Redis DB

	response := &schedulerTypes.ResponseCreateGameServer{}
	err = json.Unmarshal(responseBody, response)
	if err != nil {
		fmt.Println("can't decode /create response:", err.Error())
		return nil, err
	}

	res, err := scriptAddGameServer.Do(redisConn,
		response.ServerID,
		response.Host,
		response.Port,
		response.Path,
		g.ID,
		gameserverTag,
		gameserverMode,
		region.String(),
		request.MaxPlayers,
		authToken)
	if err != nil {
		fmt.Println("Lua script error:", err.Error())
		return nil, err
	}

	jsonBytes, ok := res.([]byte)
	if !ok {
		return nil, errors.New("can't cast response")
	}

	server := &Server{}
	err = json.Unmarshal(jsonBytes, server)
	if err != nil {
		fmt.Println("json unmarshall error:", err.Error())
		return nil, err
	}

	fmt.Println("add server ("+server.ID+")",
		"- game:", server.GameID,
		"- mode:", gameserverMode,
		"- addr:", server.Addr,
		"- tag:", server.ImageTag,
		"- max players:", server.MaxPlayers)

	return server, nil
}

// UpdateServer should be called regularly by a server to maintain
// it in the database, and to update information such as the
// player count.
// If not found, err == nil in return, and found == false
func UpdateServer(serverID string, players int) (err error, found bool) {
	fmt.Println("update server:", serverID, "- players:", players)

	redisConn := redisPool.Get()
	defer redisConn.Close()

	_, err = scriptUpdateGameServer.Do(redisConn, serverID, players)
	if err != nil {
		if strings.Contains(err.Error(), "not found") {
			return nil, false
		}
		fmt.Println("scriptUpdateGameServer ERROR:", err.Error())
		return err, false
	}

	return nil, true
}

func GetServerGameID(serverID string) (string, error) {
	redisConn := redisPool.Get()
	defer redisConn.Close()

	res, err := scriptGetServerGameID.Do(redisConn, serverID)
	if err != nil {
		return "", err
	}

	bytes, ok := res.([]byte)

	if !ok {
		return "", errors.New("🔥 can't cast GetServerGameID response")
	}

	return string(bytes), nil
}

// RemoveServer is called by a game server when shutting down.
// To unregister it from database.
func RemoveServer(serverID string) error {
	fmt.Println("remove server:", serverID)

	var servicesToRemove = []string{serverID}

	redisConn := redisPool.Get()
	defer redisConn.Close()

	b, err := json.Marshal(servicesToRemove)
	if err != nil {
		fmt.Println("can't serialize services to remove", err.Error())
		return err
	}

	_, err = scriptRemoveGameServers.Do(redisConn, string(b))
	if err != nil {
		fmt.Println("could not clean game servers:", err.Error())
		return err
	}

	return nil
}

var (

	// Add a gameserver to Redis DB
	//
	// Arguments:
	// - serverID: string
	// - host: string
	// - port: string
	// - path: string
	// - gameID: string
	// - tag: string (gameserver image tag)
	// - mode: string ("play" / "dev")
	// - region: string ("eu", "sg", "us", etc)
	// - maxPlayers: int
	// - authToken: string
	scriptAddGameServer = redis.NewScript(0, `

		-- create server table with arguments provided
		local server = {
			id = ARGV[1],
			host = ARGV[2],
			port = ARGV[3],
			path = ARGV[4],
			gameid = ARGV[5],
			tag = ARGV[6],
			mode = ARGV[7],
			region = ARGV[8],
			maxPlayers = tonumber(ARGV[9]),
			authToken = ARGV[10],
			players = 0
		}

		-- validate region
		if server.region == nil or server.region == "" then
			error("invalid region")
		end

		-- compute address
		server.addr = server.host .. ":" .. server.port .. server.path

		-- compute expiration date
		local current_time = redis.call('TIME')
		current_time = current_time[1] -- seconds
		local expires_at = current_time + 120 -- will expire after 120 seconds if not updated

		redis.call("hmset", server.id,
				   "gameid", server.gameid,
				   "tag", server.tag,
				   "mode", server.mode,
				   "region", server.region,
				   "host", server.host,
				   "port", server.port,
				   "addr", server.addr,
				   "players", server.players,
				   "maxPlayers", server.maxPlayers,
				   "authToken", server.authToken)

		-- register in ordered sets, all servers and servers indexed by game id
		local game_servers_id = "game:" .. server.gameid .. ":" .. server.tag .. ":" .. server.mode .. ":" .. server.region
		redis.call("zadd", game_servers_id, expires_at, server.id)
		redis.call("zadd", "servers", expires_at, server.id)
		
		-- store auth token in a separate set (for fast lookup)
		redis.call("zadd", "serversAuthTokens", expires_at, server.authToken)

		return cjson.encode(server)
	`)

	// Update gameserver in Redis DB : player count field
	scriptUpdateGameServer = redis.NewScript(0, `

		local server_id = ARGV[1]
		local players = ARGV[2]

		local server_exists = redis.call('exists',server_id)
		if server_exists == 0 then
			error("not found")
		end

		local current_time = redis.call('TIME')
		current_time = current_time[1] -- seconds
		local expires_at = current_time + 120

		-- update player count
		redis.call("hset", server_id, "players", players)

		-- update expiration dates
		local res = redis.call("hmget", server_id, "gameid", "tag", "mode", "region", "authToken")
		local authToken = res[5]

		local game_servers_id = "game:" .. res[1] .. ":" .. res[2] .. ":" .. res[3] .. ":" .. res[4]
		redis.call("zadd", game_servers_id, expires_at, server_id)
		redis.call("zadd", "servers", expires_at, server_id)

		-- update expiration date for auth token
		redis.call("zadd", "serversAuthTokens", expires_at, authToken)
	`)

	// Remove gameservers from Redis DB
	scriptCleanGameServers = redis.NewScript(0, `

		local current_time = redis.call('TIME')
		current_time = current_time[1] -- seconds
		current_time = current_time - 5 -- allow 5 sec margin

		-- get IDs of servers to remove
		local server_ids = redis.call('zrangebyscore', "servers", "-inf", current_time)

		for i, server_id in ipairs(server_ids) do

			redis.call("zrem", "servers", server_id)
			redis.call("zrem", "serversAuthTokens", server_id)

			local server_exists = redis.call('exists',server_id)
			if server_exists == 1 then

				local res = redis.call("hmget", server_id, "gameid", "tag", "mode", "region")
				local game_servers_id = "game:" .. res[1] .. ":" .. res[2] .. ":" .. res[3] .. ":" .. res[4]

				if res[1] ~= nil then
					local game_servers_id = "game:" .. res[1] .. ":" .. res[2] .. ":" .. res[3] .. ":" .. res[4]

					-- remove server id from set
					redis.call("zrem", game_servers_id, server_id)

					-- remove server
					redis.call("del", server_id)
				end
			end
		end

		if #server_ids == 0 then
			return "[]"
		end

		return cjson.encode(server_ids)
	`)

	scriptRemoveGameServers = redis.NewScript(0, `

		local server_ids = cjson.decode(ARGV[1])

		for i, server_id in ipairs(server_ids) do

			redis.call("zrem", "servers", server_id)
			redis.call("zrem", "serversAuthTokens", server_id)

			local server_exists = redis.call('exists',server_id)
			if server_exists == 1 then

				local res = redis.call("hmget", server_id, "gameid", "tag", "mode", "region")

				if res[1] ~= nil then

					local game_servers_id = "game:" .. res[1] .. ":" .. res[2] .. ":" .. res[3] .. ":" .. res[4]

					-- remove server id from set
					redis.call("zrem", game_servers_id, server_id)

					-- remove server
					redis.call("del", server_id)

				end
			end
		end
	`)

	// get game ID from server ID
	scriptGetServerGameID = redis.NewScript(0, `
		local server_id = ARGV[1]

		local server_exists = redis.call("exists", server_id)
		if server_exists == 0 then
			error("not found")
		end

		local game_id = redis.call("hget", server_id, "gameid")

		return game_id
		`)

	// get gameservers from Redis DB, filtered by world ID, gameserver tag, region, mode
	scriptGetGameServers = redis.NewScript(0, `
		local toStruct = function (bulk)
			local result = {}
			local nextkey
			for i, v in ipairs(bulk) do
				if i % 2 == 1 then
					nextkey = v
				else
					result[nextkey] = v
				end
			end
			return result
		end

		local mode = ARGV[3] -- "dev", "play" or "both"

		local game_id = ARGV[1]
		local game_server_tag = ARGV[2]
		local game_server_region = ARGV[3]
		local game_server_mode = ARGV[4]

		local current_time = redis.call('TIME')
		current_time = current_time[1] -- seconds

		local res = {}
		res.servers = {}

		local nbServers = 0

		-- IDs of game server collections to retrieve
		local game_servers_ids = {}

		local regions = {"eu", "sg", "us"} -- request all regions by default
		if game_server_region ~= nil and game_server_region ~= "" then
			regions = {game_server_region}
		end

		for _, region in ipairs(regions) do
			if game_server_mode == "both" then
				table.insert(game_servers_ids, "game:" .. game_id .. ":" .. game_server_tag .. ":" .. "dev" .. ":" .. region)
				table.insert(game_servers_ids, "game:" .. game_id .. ":" .. game_server_tag .. ":" .. "play" .. ":" .. region)
			else
				table.insert(game_servers_ids, "game:" .. game_id .. ":" .. game_server_tag .. ":" .. game_server_mode .. ":" .. region)
			end
		end

		for _, game_servers_id in ipairs(game_servers_ids) do
			local server_ids = redis.call('zrangebyscore', game_servers_id, current_time, "+inf")

			-- check if not found
			if server_ids[1] == nil then
				-- do not consider as an error, it simply means
				-- no server running for this game / mode / region
			else
				for i = 1, #server_ids do
					local serverID = server_ids[i]
					local server = redis.call('hgetall', serverID)
					if server ~= nil then
						nbServers = nbServers + 1
						server = toStruct(server)
						server.id = serverID
						server.players = tonumber(server.players)
						server.maxPlayers = tonumber(server.maxPlayers)
						server.port = server.port
						-- server.mode = server.mode
						res.servers[nbServers] = server
					end
				end
			end
		end

		local jsonResponse = cjson.encode(res)

		-- make sure empty comments table is encoded into json array
		jsonResponse = string.gsub( jsonResponse, '"servers":{}', '"servers":[]' )

		return jsonResponse
		`)
)
