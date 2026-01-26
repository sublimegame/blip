package types

import "cu.bzh/types/gameserver"

// ErrorResponse ...
type ErrorResponse struct {
	Error string `json:"error,omitempty"`
}

// RequestCreateGameServer ...
type RequestCreateGameServer struct {
	Image      string          `json:"image,omitempty"` // image with tag
	GameID     string          `json:"game-id,omitempty"`
	Mode       gameserver.Mode `json:"mode,omitempty"`
	MaxPlayers int32           `json:"max-players,omitempty"`
	ClientInfo ClientInfo      `json:"client-info,omitempty"`
	AuthToken  string          `json:"auth-token,omitempty"`
}

type ClientInfo struct {
	// ClientVersion     string `json:"clientVersion,omitempty"`
	ClientBuildNumber int `json:"clientBuildNumber,omitempty"`
}

// ResponseCreateGameServer ...
type ResponseCreateGameServer struct {
	Host     string `json:"host,omitempty"`
	Port     string `json:"port,omitempty"`
	Path     string `json:"path,omitempty"`
	ServerID string `json:"server-id,omitempty"`
}
