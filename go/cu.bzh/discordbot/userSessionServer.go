package main

import (
	"errors"
	"fmt"
	"net/http"
	"time"

	httpUtils "cu.bzh/utils/http"
	"github.com/google/uuid"
	"github.com/gorilla/websocket"
)

const (
	UUID_LENGTH = 36
)

var (
	upgrader          = websocket.Upgrader{} // use default options
	activeConnections = make(map[string]*UserSession)
)

//
// Types
//

type UserSession struct {
	connectedAt time.Time
	userUUID    *uuid.UUID
	err         error
}

func NewUserSession() *UserSession {
	return &UserSession{
		connectedAt: time.Now(),
		userUUID:    nil,
		err:         nil,
	}
}

//
// Local functions
//

// userSessionHandler handles WebSocket requests from the user session client
// It is called once for each client connection
func userSessionHandler(w http.ResponseWriter, r *http.Request) {
	fmt.Println("[🐞] userSessionHandler called")

	c, err := upgrader.Upgrade(w, r, nil)
	if err != nil {
		fmt.Println("❌ WebSocket upgrade error:", err)
		return
	}

	// close connection when function returns
	defer c.Close()

	// CLIENT CONNECTED

	// Store the connection time
	id := httpUtils.GetIP(r)
	// TODO: check activeConnections[id] doesn't already exist ???
	activeConnections[id] = NewUserSession()
	dg.ChannelMessageSend(CHANNEL_BOT_TESTS, "🟢 connected "+id)

	// remove connection from activeConnections when function returns
	defer delete(activeConnections, id)

	// Process connection streams
	for {
		// Read next message from client
		mt, message, err := c.ReadMessage()
		if err != nil {
			if _, ok := err.(*websocket.CloseError); ok {
				// err is a CloseError, this is fine
			} else {
				// other error
				activeConnections[id].err = err
			}
			break // disconnect the client
		}
		if mt == websocket.TextMessage && len(message) == UUID_LENGTH && activeConnections[id].userUUID == nil {
			fmt.Println("✅ received UUID from client:", string(message))
			userUUID, err := uuid.Parse(string(message))
			if err != nil {
				activeConnections[id].err = errors.Join(errors.New("failed to parse UUID"), err)
				break // disconnect the client
			}
			activeConnections[id].userUUID = &userUUID
			// sending UUID back to client
			// err = c.WriteMessage(websocket.TextMessage, message)
			// if err != nil {
			// fmt.Println("❌ write:", err)
			// break
			// }
		} else {
			// received unrecognized message
			activeConnections[id].err = errors.New("received unrecognized message")
			break // disconnect the client
		}
	}

	// CLIENT BEING DISCONNECTED

	if activeConnections[id].err != nil {
		dg.ChannelMessageSend(CHANNEL_BOT_TESTS, "🔴❌ disconnected with error ["+id+"]["+activeConnections[id].err.Error()+"]")
		return
	}

	connectionDuration := time.Since(activeConnections[id].connectedAt)
	dg.ChannelMessageSend(CHANNEL_BOT_TESTS, "🔴 disconnected "+id+" duration: "+connectionDuration.String())
}
