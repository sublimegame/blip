package main

import (
	"fmt"

	"cu.bzh/hub/game"
	"cu.bzh/hub/types"
	schedulerTypes "cu.bzh/scheduler/types"
	"cu.bzh/types/gameserver"
)

type gameServersRequest struct {
	Mode               gameserver.Mode
	GameID             string
	GameServerTag      string
	Region             types.Region
	MaxResponseCount   int  // max number of servers that we want
	IncludeFullServers bool // whether we should include servers that reached the max amount of players
	CreateIfNone       bool // whether we should create a server if none is found with given conditions
	ResponseChan       chan []*game.Server
	ClientInfo         schedulerTypes.ClientInfo
}

func getGameServerRoutine(channel <-chan *gameServersRequest) {
	for {
		req := <-channel

		// fmt.Println("process get running server for game:", req.GameID, "server:", req.GameServerTag, "[mode:"+req.Mode+"]")

		g := game.NewReadOnly(req.GameID)

		// get all running gameservers
		servers, err := g.GetServers(req.GameServerTag, req.Region, req.Mode)
		if err != nil {
			fmt.Println("can't get game servers:", err.Error())
			req.ResponseChan <- nil
			continue
		}

		// exclude full servers
		if req.IncludeFullServers == false {
			i := 0
			for _, s := range servers {
				if s.Players < s.MaxPlayers {
					// insert and increment index
					servers[i] = s
					i++
				}
			}
			servers = servers[:i]
		}

		if req.CreateIfNone == false || len(servers) > 0 {
			// fmt.Println("return servers:", servers)
			req.ResponseChan <- servers
			continue
		}

		// no server found AND CreateIfNone == true

		fmt.Println("[🐞] no server found, starting one!")

		// Generate a random auth token
		authToken := randomString(16)

		fmt.Println("[🐞] auth token:", authToken)

		server, err := g.StartServer(req.GameServerTag, req.Region, req.Mode, req.ClientInfo, authToken)
		if err != nil {
			fmt.Println("can't start game server:", err.Error())
			req.ResponseChan <- nil
			continue
		}

		servers = []*game.Server{server}

		req.ResponseChan <- servers
	}
}
