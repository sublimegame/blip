package anthropic

import (
	"time"

	"cu.bzh/ai"
)

//
// Models
//

type GetModelsResponse struct {
	Data    []ModelData `json:"data"`
	FirstID string      `json:"first_id"`
	HasMore bool        `json:"has_more"`
	LastID  string      `json:"last_id"`
}

type ModelData struct {
	CreatedAt   time.Time `json:"created_at"`
	DisplayName string    `json:"display_name"`
	ID          string    `json:"id"`
	Type        string    `json:"type"`
}

//
// Generate request
//

type GenerateRequest struct {
	Model     string    `json:"model"`
	Messages  []Message `json:"messages"`
	MaxTokens int       `json:"max_tokens"`
	Stream    bool      `json:"stream"`
	System    string    `json:"system"`
}

type Message struct {
	Role    ai.MessageRole `json:"role"`
	Content string         `json:"content"`
}

//
// Generate reponse
//

type GenerateResponse struct {
	Type    string        `json:"type"`    // can be "error"
	Message string        `json:"message"` // if type is "error", this contains the error message
	Content []ContentItem `json:"content"`
}

type ContentItem struct {
	Type string `json:"type"`
	Text string `json:"text"`
}

//
// Generate - stream response
//

type StreamResponse struct {
	Type  string `json:"type"`
	Delta Delta  `json:"delta"`
	Index int    `json:"index"`
}

type Delta struct {
	Type string `json:"type"`
	Text string `json:"text"`
}
