package grok

import (
	"errors"
	"fmt"
	"io"
	"log"
	"net/http"

	"cu.bzh/ai"
)

type requestOptions struct {
	apiKey         string
	jsonBodyReader io.Reader
}

// createRequestForGrokApi ...
func createRequestForGrokApi(httpMethod, apiUrl string, opts requestOptions) (*http.Request, error) {

	if httpMethod == "" {
		return nil, errors.New("http method is required")
	}

	// create HTTP request
	req, err := http.NewRequest(httpMethod, apiUrl, opts.jsonBodyReader)
	if err != nil {
		return nil, err
	}

	// set Content-Type header
	req.Header.Set("Content-Type", "application/json")
	req.Header.Set("Authorization", fmt.Sprintf("Bearer %s", opts.apiKey))
	return req, nil
}

// validateInputSimple ...
func validateInputSimple(apiKey, userPrompt string) error {

	if apiKey == "" {
		return errors.New("API key is missing")
	}

	if userPrompt == "" {
		return errors.New("user prompt is missing")
	}

	return nil
}

// validateInput ...
func validateInput(apiKey string, messages []ai.Message) error {

	if apiKey == "" {
		return errors.New("API key is missing")
	}

	if len(messages) == 0 {
		return errors.New("messages are missing")
	}

	for _, message := range messages {
		if message.Role == "" {
			return errors.New("message role is missing")
		}
	}

	return nil
}

func convertAIMessageRoleToGrokRole(role ai.MessageRole) ai.MessageRole {
	if role == ai.ROLE_USER {
		return ROLE_USER
	} else if role == ai.ROLE_ASSISTANT {
		return ROLE_ASSISTANT
	}
	log.Fatalf("[convertAIMessageRoleToGrokRole] Unsupported message role: %s", role)
	return ""
}

// convertAIMessagesToGrokMessages ...
func convertAIMessagesToGrokMessages(msgs []ai.Message) []Message {
	grokMessages := make([]Message, len(msgs))
	for i, msg := range msgs {
		grokMessages[i] = Message{
			Role:    convertAIMessageRoleToGrokRole(msg.Role),
			Content: msg.Content,
		}
	}
	return grokMessages
}
