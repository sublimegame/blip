package grok

import (
	"bufio"
	"bytes"
	"encoding/json"
	"fmt"
	"io"
	"net/http"
	"slices"
	"strings"

	"cu.bzh/ai"
)

const (
	// Environment variable names
	ENVAR_API_KEY = "GROK_API_KEY"

	// Models names
	MODEL_GROK_3_BETA = "grok-3-beta" // Our flagship model that excels at enterprise tasks like data extraction, programming, and text summarization.
	// MODEL_GROK_3_FAST_BETA      = "grok-3-fast-beta"      // Same as GROK_3_BETA but on faster infrastructure (more expensive)
	MODEL_GROK_3_MINI_BETA = "grok-3-mini-beta" // A lightweight model that thinks before responding. Excels at quantitative tasks that involve math and reasoning.
	// MODEL_GROK_3_MINI_FAST_BETA = "grok-3-mini-fast-beta" // Same as GROK_3_MINI_BETA but on faster infrastructure (more expensive)

	// Default config
	DEFAULT_MAX_TOKENS = 8192

	// Roles
	ROLE_USER      = "user"
	ROLE_ASSISTANT = "system"

	// API URLs (unexposed)
	// apiUrlChatCompletions = "https://api.x.ai/v1/chat/completions"
	apiUrlMessages = "https://api.x.ai/v1/messages" // anthropic compatible endpoint

	// Response processing
	STREAM_RESPONSE_DELTA_TYPE_TEXT     = "text_delta"
	STREAM_RESPONSE_DELTA_TYPE_THINKING = "thinking_delta"
)

var (
	supportedModels = []string{
		MODEL_GROK_3_BETA,
		MODEL_GROK_3_MINI_BETA,
	}
)

func IsModelSupported(modelName string) bool {
	return slices.Contains(supportedModels, modelName)
}

//
// GrokModel
//

type GrokModel struct {
	ModelName string
}

// NewModel creates a new GrokModel instance
func NewModel(modelName string) *GrokModel {
	// Check if the model name is supported
	if !slices.Contains(supportedModels, modelName) {
		return nil
	}
	// Create a new GrokModel instance
	return &GrokModel{
		ModelName: modelName,
	}
}

func (m GrokModel) Name() string {
	return m.ModelName
}

func (m GrokModel) GenerateSimple(userPrompt string, opts ai.GenerateOpts) (string, error) {

	err := validateInputSimple(opts.APIKey, userPrompt)
	if err != nil {
		return "", err
	}

	if opts.MaxTokens < 1 {
		opts.MaxTokens = DEFAULT_MAX_TOKENS
	}

	// prepare request body

	messages := []Message{
		{
			Role:    ROLE_USER,
			Content: userPrompt,
		},
	}

	reqBody := GenerateRequest{
		Model:     m.ModelName,
		Messages:  messages,
		MaxTokens: opts.MaxTokens,
		Stream:    false,
		System:    opts.SystemPrompt.String(),
	}

	jsonData, err := json.Marshal(reqBody)
	if err != nil {
		return "", fmt.Errorf("error marshaling request: %v", err)
	}

	// send request

	req, err := createRequestForGrokApi(http.MethodPost, apiUrlMessages, requestOptions{
		apiKey:         opts.APIKey,
		jsonBodyReader: bytes.NewBuffer(jsonData),
	})
	if err != nil {
		return "", fmt.Errorf("error creating request: %v", err)
	}

	client := &http.Client{}
	resp, err := client.Do(req)
	if err != nil {
		return "", fmt.Errorf("error sending request: %v", err)
	}
	defer resp.Body.Close()

	body, err := io.ReadAll(resp.Body)
	if err != nil {
		return "", fmt.Errorf("error reading response: %v", err)
	}

	var response GenerateResponse
	err = json.Unmarshal(body, &response)
	if err != nil {
		return "", fmt.Errorf("error parsing response: %v\nraw response: %s", err, string(body))
	}

	// TODO: add error handling
	// if response.Type == "error" {
	// 	return "", fmt.Errorf("error from Grok: %s", response.Message)
	// }

	return response.GetCumulatedText(), nil
}

func (m GrokModel) Generate(messages []ai.Message, opts ai.GenerateOpts) (string, error) {

	err := validateInput(opts.APIKey, messages)
	if err != nil {
		return "", err
	}

	if opts.MaxTokens < 1 {
		opts.MaxTokens = DEFAULT_MAX_TOKENS
	}

	// prepare request body

	grokMessages := convertAIMessagesToGrokMessages(messages)

	reqBody := GenerateRequest{
		Model:     m.ModelName,
		Messages:  grokMessages,
		MaxTokens: opts.MaxTokens,
		Stream:    false,
		System:    opts.SystemPrompt.String(),
	}

	jsonData, err := json.Marshal(reqBody)
	if err != nil {
		return "", fmt.Errorf("error creating JSON request: %v", err)
	}

	// send request

	req, err := createRequestForGrokApi(http.MethodPost, apiUrlMessages, requestOptions{
		apiKey:         opts.APIKey,
		jsonBodyReader: bytes.NewBuffer(jsonData),
	})
	if err != nil {
		return "", fmt.Errorf("error creating request: %v", err)
	}

	client := &http.Client{}
	resp, err := client.Do(req)
	if err != nil {
		return "", fmt.Errorf("error sending request: %v", err)
	}
	defer resp.Body.Close()

	body, err := io.ReadAll(resp.Body)
	if err != nil {
		return "", fmt.Errorf("error reading response: %v", err)
	}

	var response GenerateResponse
	err = json.Unmarshal(body, &response)
	if err != nil {
		return "", fmt.Errorf("error parsing response: %v\nraw response: %s", err, string(body))
	}

	// TODO: add error handling

	return response.GetCumulatedText(), nil
}

func (m GrokModel) GenerateStream(messages []ai.Message, opts ai.GenerateOpts) (chan string, error) {

	err := validateInput(opts.APIKey, messages)
	if err != nil {
		return nil, err
	}

	if opts.MaxTokens < 1 {
		opts.MaxTokens = DEFAULT_MAX_TOKENS
	}

	// prepare request body

	grokMessages := convertAIMessagesToGrokMessages(messages)

	reqBody := GenerateRequest{
		Model:     m.ModelName,
		Messages:  grokMessages,
		MaxTokens: opts.MaxTokens,
		Stream:    true,
		System:    opts.SystemPrompt.String(),
	}

	jsonData, err := json.Marshal(reqBody)
	if err != nil {
		return nil, fmt.Errorf("error creating JSON request: %v", err)
	}

	// send request

	req, err := createRequestForGrokApi(http.MethodPost, apiUrlMessages, requestOptions{
		apiKey:         opts.APIKey,
		jsonBodyReader: bytes.NewBuffer(jsonData),
	})
	if err != nil {
		return nil, fmt.Errorf("error creating request: %v", err)
	}

	client := &http.Client{}
	resp, err := client.Do(req)
	if err != nil {
		return nil, fmt.Errorf("error sending request: %v", err)
	}

	responseChan := make(chan string)

	go func() {
		defer resp.Body.Close()
		defer close(responseChan)

		reader := bufio.NewReader(resp.Body)
		for {
			line, err := reader.ReadString('\n')
			if err != nil {
				if err == io.EOF {
					break
				}
				fmt.Printf("error reading stream: %v\n", err)
				return
			}

			line = strings.TrimSpace(line)
			if line == "" {
				continue
			}

			jsonData := strings.TrimPrefix(line, "data: ")
			if jsonData == "[DONE]" {
				break
			}

			var streamResp StreamResponse
			if err := json.Unmarshal([]byte(jsonData), &streamResp); err != nil {
				continue
			}

			if streamResp.Type == "content_block_delta" {
				if streamResp.Delta.Type == STREAM_RESPONSE_DELTA_TYPE_TEXT {
					text := streamResp.Delta.Text
					responseChan <- text
				}
			}
		}
	}()

	return responseChan, nil
}
