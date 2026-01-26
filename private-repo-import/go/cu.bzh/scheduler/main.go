package main

import (
	"bytes"
	"context"
	"encoding/base64"
	"encoding/json"
	"errors"
	"fmt"
	"io"
	"log"
	"net/http"
	"os"
	"strconv"
	"strings"
	"time"

	schedulerTypes "cu.bzh/scheduler/types"
	"github.com/docker/docker/api/types/container"
	"github.com/docker/docker/api/types/events"
	"github.com/docker/docker/api/types/image"
	"github.com/docker/docker/api/types/network"
	"github.com/docker/docker/api/types/registry"
	"github.com/docker/docker/client"
	"github.com/docker/go-connections/nat"
)

const (
	DOCKER_API_VERSION          = "1.45"
	SCHEDULERS_JSON_FILE        = "/schedulers.json"
	DOCKER_OVERLAY_NETWORK_NAME = "cubzh"
)

var (
	// Public address for the scheduler.
	// Sent to Hub when starting a new game server (with server port)
	// Resolved based on public IP when starting the scheduler.
	schedulerInfo *SchedulerInfo
)

type SchedulerInfo struct {
	// Public host, to reach the node with that scheduler,
	// and all game servers scheduled by it.
	Host string `json:"host,omitempty"`

	// A prefix for created game servers.
	// Each scheduler uses its own unique prefix to avoid collisions.
	GameServerIDPrefix string `json:"gameserver-id-prefix,omitempty"`
}

func (i *SchedulerInfo) getServerIDFromContainerID(containerID string) string {
	return i.GameServerIDPrefix + containerID[:12]
}

func main() {

	// Load scheduler info
	b, err := os.ReadFile(SCHEDULERS_JSON_FILE)
	if err != nil {
		log.Fatal(err)
	}

	var ipToSchedulerInfo map[string]*SchedulerInfo
	err = json.Unmarshal(b, &ipToSchedulerInfo)
	if err != nil {
		log.Fatal(err)
	}

	url := "https://api.ipify.org?format=text"
	resp, err := http.Get(url)
	if err != nil {
		log.Fatal(err)
	}

	ipBytes, err := io.ReadAll(resp.Body)
	if err != nil {
		resp.Body.Close()
		log.Fatal(err)
	}

	ip := string(ipBytes)

	resp.Body.Close()

	fmt.Println("Public IP:", ip)

	schedulerInfo = &SchedulerInfo{
		Host:               "localhost",
		GameServerIDPrefix: "local-",
	}

	info, ok := ipToSchedulerInfo[ip]
	if ok {
		schedulerInfo = info
	}
	// else {
	// 	switch ip {
	// 	case "185.146.221.228": // aduermael's house
	// 		fmt.Println("🤓 debug from aduermael's house!")
	// 		schedulerInfo.Host = "192.168.10.103"
	// 	}
	// }

	fmt.Println("Host:", schedulerInfo.Host)
	fmt.Println("GameServerIDPrefix:", schedulerInfo.GameServerIDPrefix)

	// Start goroutine to process Docker events

	go processEvents()

	// Expose web server for Hub requests
	http.HandleFunc("/ping", ping)
	http.HandleFunc("/create", createGameServer)

	fmt.Println("✨ listening on :80")
	if err := http.ListenAndServe(":80", nil); err != nil {
		log.Fatal(err)
	}
}

func askHubToRemoveServer(serverID string) error {
	postBody, err := json.Marshal(map[string]string{})
	if err != nil {
		return err
	}

	resp, err := http.Post("http://hub-http/private/server/"+serverID+"/exit", "application/json", bytes.NewBuffer(postBody))
	if err != nil {
		return err
	}

	if resp.StatusCode != http.StatusOK {
		return errors.New("status code: " + strconv.Itoa(resp.StatusCode))
	}

	return nil
}

// processEvents informs the hub when a container stops,
// to make sure the gameserver is removed from the database.
// A gameserver itself may not be able to inform the hub
// if it crashes for some reason.
// https://github.com/moby/moby/blob/master/client/events.go
func processEvents() {

	for {
		fmt.Println("start processing Docker events")

		cli, err := client.NewClientWithOpts(client.WithVersion(DOCKER_API_VERSION))
		if err != nil {
			fmt.Println("🔥 can't create client to listen Docker events")
			return
		}

		msgs, errs := cli.Events(context.Background(), events.ListOptions{})

		for {
			select {
			// "If an error is sent all processing will be stopped"
			case err := <-errs:
				fmt.Println(err)

			case msg := <-msgs:
				/*
					events.Message{Status:"die",
					ID:"b10c231bde88a3de0922fce1b00acd088e66cc65573f3b7de89b8c6779c80dcf",
					From:"registry.digitalocean.com/cubzh/gameserver:b7a26e2e",
					Type:"container",
					Action:"die",
					Actor:events.Actor{ID:"b10c231bde88a3de0922fce1b00acd088e66cc65573f3b7de89b8c6779c80dcf",
					Attributes:map[string]string{
						"exitCode":"137",
						"image":"registry.digitalocean.com/cubzh/gameserver:b7a26e2e",
						"name":"interesting_hugle"}},
						Scope:"local",
						Time:1611044164,
						TimeNano:1611044164228164300
					}
					events.Message{
						Status:"destroy",
						ID:"b10c231bde88a3de0922fce1b00acd088e66cc65573f3b7de89b8c6779c80dcf",
						From:"registry.digitalocean.com/cubzh/gameserver:b7a26e2e",
						Type:"container",
						Action:"destroy",
						Actor:events.Actor{ID:"b10c231bde88a3de0922fce1b00acd088e66cc65573f3b7de89b8c6779c80dcf",
						Attributes:map[string]string{"image":"registry.digitalocean.com/cubzh/gameserver:b7a26e2e",
						"name":"interesting_hugle"}},
						Scope:"local",
						Time:1611044164,
						TimeNano:1611044164417072600
					}
				*/
				if msg.Type == events.ContainerEventType {
					// fmt.Printf("%#v\n", msg)
					if msg.Action == "die" {

						containerID := msg.Actor.ID

						fmt.Println("CONTAINER DIED:", containerID[:12])

						serverID := schedulerInfo.getServerIDFromContainerID(containerID)

						err = askHubToRemoveServer(serverID)
						if err != nil {
							fmt.Println("🔥 can't ask hub to remove server:", serverID)
							fmt.Println("    ", err.Error())
						}
					}
				}
			}
		}
		// fmt.Println("🔥 sleeping 5 seconds before recreating a Docker Client")
		// time.Sleep(5 * time.Second)
	}
}

func ping(w http.ResponseWriter, r *http.Request) {
	w.Write([]byte("pong 🏓"))
}

func createGameServer(w http.ResponseWriter, r *http.Request) {
	defer r.Body.Close()

	// Ensure HTTP method is POST
	if r.Method != "POST" {
		renderError(w, "bad request", http.StatusBadRequest)
		return
	}

	// Parse request body
	var req schedulerTypes.RequestCreateGameServer
	err := json.NewDecoder(r.Body).Decode(&req)
	if err != nil {
		renderError(w, "bad request: "+err.Error(), http.StatusBadRequest)
		return
	}

	// Request validation
	if req.MaxPlayers == 0 {
		req.MaxPlayers = 1
	}

	fmt.Println("[🐞] Gameserver creation requested:", req)

	cli, err := client.NewClientWithOpts(client.WithVersion(DOCKER_API_VERSION))
	if err != nil {
		renderError(w, "bad request: "+err.Error(), http.StatusBadRequest)
		return
	}

	encodedCredentials, err := encodeRegistryAuthForDigitalOcean(os.Getenv("DIGITALOCEAN_PAT"))
	if err != nil {
		renderError(w, "internal server error", http.StatusInternalServerError)
		return
	}

	fmt.Println("[🐞] launching container:", req.Image, req.GameID, req.Mode, req.AuthToken)

	// NOTE: the container will compute its server ID with GameServerIDPrefix
	// and the 12 first chars of its container ID.
	containerConfig := &container.Config{
		Image: req.Image,
		ExposedPorts: nat.PortSet{
			"80/tcp": struct{}{},
		},
	}

	// New images (used in Cubzh 0.1.13 and later versions)
	// ensure auth token is provided
	authToken := req.AuthToken
	if authToken == "" {
		fmt.Println("🔥 bad request: auth token is required")
		renderError(w, "bad request: auth token is required", http.StatusBadRequest)
		return
	}

	containerConfig.Cmd = []string{
		string(req.Mode),
		req.GameID,
		schedulerInfo.GameServerIDPrefix,
		authToken,
		strconv.FormatInt(int64(req.MaxPlayers), 10),
	}

	hostConfig := &container.HostConfig{
		AutoRemove: false,
		Resources: container.Resources{
			Memory:   200000000,    // 200 megabytes
			NanoCPUs: 4 * 10000000, // 4% of total CPU ressources
		},
	}

	networkConfig := &network.NetworkingConfig{}
	// Note: if we don't define an alias explicitly, the default alias (container ID value) is not automatically created.
	//       We can probably remove this in the future, if we find a solution.
	// Add network alias to the container (random string)
	const charset = "abcdefghijklmnopqrstuvwxyz0123456789"
	b := make([]byte, 5)
	for i := range b {
		b[i] = charset[time.Now().UnixNano()%int64(len(charset))]
	}
	rnd := string(b)
	networkConfig = &network.NetworkingConfig{
		EndpointsConfig: map[string]*network.EndpointSettings{
			DOCKER_OVERLAY_NETWORK_NAME: {
				Aliases: []string{"gaetan_" + rnd},
			},
		},
	}

	gameServerContainer, err := cli.ContainerCreate(context.Background(), containerConfig, hostConfig, networkConfig, nil, "")

	if err != nil {
		if strings.Contains(err.Error(), "No such image") {
			fmt.Println("gameserver image not found, loading it...")
			fmt.Println("docker pull", req.Image)

			// TODO: should be synced somehow if several clients asking for same image at the same time

			r, err := cli.ImagePull(context.Background(), req.Image, image.PullOptions{
				RegistryAuth: encodedCredentials,
			})
			if err != nil {
				renderError(w, err.Error(), http.StatusInternalServerError)
				return
			}
			io.ReadAll(r)
			r.Close()

			// try to create container again, now that we have the image
			gameServerContainer, err = cli.ContainerCreate(context.Background(), containerConfig, hostConfig, networkConfig, nil, "")
			if err != nil {
				renderError(w, err.Error(), http.StatusInternalServerError)
				return
			}

		} else {
			renderError(w, err.Error(), http.StatusInternalServerError)
			return
		}
	}

	containerShortID := gameServerContainer.ID[:12]
	cubzhGameserverID := schedulerInfo.getServerIDFromContainerID(gameServerContainer.ID)

	// fmt.Println("[🐞] CONTAINER CREATED:", containerShortID, cubzhGameserverID)

	// Start container
	err = cli.ContainerStart(context.Background(), gameServerContainer.ID, container.StartOptions{})
	if err != nil {
		renderError(w, err.Error(), http.StatusInternalServerError)
		return
	}

	// fmt.Println("[🐞] CONTAINER DID START: ", containerShortID, cubzhGameserverID, schedulerInfo.Host+":"+portStr)

	// Write HTTP response
	renderContainerStarted(w, schedulerInfo.Host, "443", "/servers/"+containerShortID, cubzhGameserverID)
}

func renderContainerStarted(w http.ResponseWriter, host, port, path, serverID string) {

	resp := schedulerTypes.ResponseCreateGameServer{
		Host:     host,
		Port:     port,
		Path:     path,
		ServerID: serverID,
	}

	jsonBytes, err := json.Marshal(resp)
	if err != nil {
		http.Error(w, err.Error(), http.StatusInternalServerError)
		return
	}

	w.Header().Set("Content-Type", "application/json")
	w.WriteHeader(http.StatusOK)
	w.Write(jsonBytes)
}

func renderError(w http.ResponseWriter, errStr string, statusCode int) {

	resp := schedulerTypes.ErrorResponse{Error: errStr}

	jsonBytes, err := json.Marshal(resp)
	if err != nil {
		http.Error(w, err.Error(), http.StatusInternalServerError)
		return
	}

	w.Header().Set("Content-Type", "application/json")
	w.WriteHeader(statusCode)
	w.Write(jsonBytes)
}

// Encode your credentials
func encodeRegistryAuthForDigitalOcean(identicalUsernameAndPassword string) (string, error) {
	return encodeRegistryAuth(identicalUsernameAndPassword, identicalUsernameAndPassword)
}

func encodeRegistryAuth(username, password string) (string, error) {
	authConfig := registry.AuthConfig{
		Username: username,
		Password: password,
	}
	encodedJSON, err := json.Marshal(authConfig)
	if err != nil {
		return "", err
	}
	authStr := base64.URLEncoding.EncodeToString(encodedJSON)
	return authStr, nil
}
