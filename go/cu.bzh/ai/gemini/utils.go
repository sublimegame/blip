package gemini

import (
	"io"
	"log"
	"net/http"

	"cu.bzh/ai"
)

func createHttpRequestForGeminiApi(method, apiUrl string, jsonBodyReader io.Reader) (*http.Request, error) {

	// create HTTP request
	req, err := http.NewRequest(method, apiUrl, jsonBodyReader)
	if err != nil {
		return nil, err
	}

	// set Content-Type header
	req.Header.Set("Content-Type", "application/json")

	return req, nil
}

func convertAIMessageRoleToGeminiRole(role ai.MessageRole) ai.MessageRole {
	if role == ai.ROLE_USER {
		return ROLE_USER
	} else if role == ai.ROLE_ASSISTANT {
		return ROLE_ASSISTANT
	}
	log.Fatalf("[convertAIMessageRoleToGeminiRole] Unsupported message role: %s", role)
	return ""
}

func convertAIMessagesToGeminiContents(msgs []ai.Message) []Content {
	geminiContents := make([]Content, len(msgs))
	for i, msg := range msgs {
		geminiContents[i] = Content{
			Role: convertAIMessageRoleToGeminiRole(msg.Role),
			Parts: []Part{
				{Text: msg.Content},
			},
		}
	}
	return geminiContents
}

func convertSystemPromptToGeminiSystemInstruction(systemPrompt *ai.SystemPrompt) SystemInstruction {
	// make sure systemPrompt is not nil
	if systemPrompt == nil {
		return SystemInstruction{}
	}

	parts := make([]Part, len(*systemPrompt))
	for i, part := range *systemPrompt {
		if part.Key() == "text" {
			parts[i] = Part{
				Text: part.Value(),
			}
		} else {
			log.Fatalf("Unsupported SystemPrompt Part type: %s", part.Key())
		}
	}

	return SystemInstruction{Parts: parts}
}
