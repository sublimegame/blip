package ai

import (
	"encoding/json"
	"log"
	"strings"
	"time"
)

const (
	ROLE_USER      MessageRole = "aiRoleUser"
	ROLE_ASSISTANT MessageRole = "aiRoleAssistant"
)

type MessageRole string

type GenerateOpts struct {
	// APIKey is the API key to use the model. Depending on the model, it can be optional.
	APIKey       string
	SystemPrompt *SystemPrompt
	MaxTokens    int
}

type Message struct {
	Role    MessageRole
	Content string
}

// ModelInfo contains the information of an AI model.
// It is used to list available models for instance.
type ModelInfo struct {
	DisplayName string
	ID          string
	CreatedAt   time.Time
	Type        string
}

// ------------------------------------
// SystemPrompt
// ------------------------------------
// SystemPrompt is a system prompt for an AI model.
// It is used to provide a system prompt to the model.

type SystemPrompt []Part

func NewSystemPrompt() *SystemPrompt {
	return &SystemPrompt{}
}

func NewSystemPromptFromText(text string) *SystemPrompt {
	// system prompt must not contain empty text parts
	if text == "" {
		return nil
	}
	return &SystemPrompt{
		NewTextPart(text),
	}
}

func (p *SystemPrompt) Append(part Part) {
	*p = append(*p, part)
}

func (p *SystemPrompt) String() string {
	return strings.Join(p.Array(), "\n")
}

func (p *SystemPrompt) Array() []string {
	parts := make([]string, len(*p))
	for i, part := range *p {
		if part.Key() == "text" {
			parts[i] = part.Value()
		} else {
			log.Fatalf("Unsupported SystemPrompt Part type: %s", part.Key())
		}
	}
	return parts
}

type Part interface {
	Key() string
	Value() string
}

type TextPart struct {
	Data string
}

func NewTextPart(data string) *TextPart {
	return &TextPart{
		Data: data,
	}
}

func (p *TextPart) Key() string {
	return "text"
}

func (p *TextPart) Value() string {
	return p.Data
}

// Cutomize JSON marshalling
func (p *TextPart) MarshalJSON() ([]byte, error) {
	return json.Marshal(map[string]string{
		p.Key(): p.Value(),
	})
}

// type BlobPart struct {
// 	Data string
// }

// func (p *BlobPart) Key() string {
// 	return "blob"
// }

// func (p *BlobPart) Value() string {
// 	return p.Data
// }

// type FilePart struct {
// 	Data string
// }

// func (p *FilePart) Key() string {
// 	return "file"
// }

// func (p *FilePart) Value() string {
// 	return p.Data
// }
