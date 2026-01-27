package grok

import (
	"time"

	"cu.bzh/ai"
)

const (
	PART_TYPE_TEXT     = "text"
	PART_TYPE_THINKING = "thinking"
)

//
// Models
//

type GetModelsResponse struct {
	Data   []ModelData `json:"data"`
	Object string      `json:"object"` // value is always "list"
}

type ModelData struct {
	ID        string    `json:"id"`
	CreatedAt time.Time `json:"created"`
	Object    string    `json:"object"`
	OwnedBy   string    `json:"owned_by"`
}

//
// Generate request
//

// Documentation: https://docs.x.ai/docs/api-reference#chat-completions
type GenerateRequest struct {
	// Required
	Model     string    `json:"model"`
	Messages  []Message `json:"messages"`
	MaxTokens int       `json:"max_tokens"`
	// Optional
	Stream bool   `json:"stream,omitempty"` // default is false
	System string `json:"system,omitempty"`
}

type Message struct {
	Role    ai.MessageRole `json:"role"`
	Content string         `json:"content"`
}

type Part struct {
	Type string `json:"type"` // can only be "text"
	Text string `json:"text"` // cannot be empty
}

//
// Generate response
//

type GenerateResponse struct {
	ID           string        `json:"id"`
	Type         string        `json:"type"`
	Role         string        `json:"role"`
	Content      []ContentPart `json:"content"`
	Model        string        `json:"model"`
	StopReason   string        `json:"stop_reason,omitempty"`
	StopSequence string        `json:"stop_sequence,omitempty"`
	Usage        Usage         `json:"usage,omitempty"`
}

type ContentPart struct {
	Type     string `json:"type"`     // can be "text" or "thinking"
	Text     string `json:"text"`     // has a value if type is "text"
	Thinking string `json:"thinking"` // has a value if type is "thinking"
}

type Usage struct {
	InputTokens              int `json:"input_tokens"`
	OutputTokens             int `json:"output_tokens"`
	CacheCreationInputTokens int `json:"cache_creation_input_tokens"`
	CacheReadInputTokens     int `json:"cache_read_input_tokens"`
}

//
// Generate - stream response
//

type StreamResponse struct {
	Type  string `json:"type"`
	Delta Delta  `json:"delta"`
	Usage Usage  `json:"usage"`
}

type Delta struct {
	Type string `json:"type"`
	Text string `json:"text"`
}

//
// Functions
//

func (gr *GenerateResponse) GetCumulatedText() string {
	text := ""
	for _, part := range gr.Content {
		if part.Type == PART_TYPE_TEXT {
			text += part.Text
		}
	}
	return text
}

func (gr *GenerateResponse) GetCumulatedThinking() string {
	thinking := ""
	for _, part := range gr.Content {
		if part.Type == PART_TYPE_THINKING {
			thinking += part.Thinking
		}
	}
	return thinking
}
