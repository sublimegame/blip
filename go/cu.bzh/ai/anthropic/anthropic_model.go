package anthropic

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
	ENVAR_API_KEY = "ANTHROPIC_API_KEY"

	// Models names
	MODEL_CLAUDE_3_5_HAIKU  = "claude-3-5-haiku-20241022"
	MODEL_CLAUDE_3_5_SONNET = "claude-3-5-sonnet-20241022"
	MODEL_CLAUDE_3_7_SONNET = "claude-3-7-sonnet-20250219"

	// Default config
	DEFAULT_MAX_TOKENS = 8192

	// API URLs (unexposed)
	apiUrlModels   = "https://api.anthropic.com/v1/models"
	apiUrlMessages = "https://api.anthropic.com/v1/messages"
	apiVersion     = "2023-06-01"

	// Message roles
	ROLE_USER      ai.MessageRole = "user"
	ROLE_ASSISTANT ai.MessageRole = "assistant"
)

var (
	supportedModels = []string{
		MODEL_CLAUDE_3_5_HAIKU,
		MODEL_CLAUDE_3_5_SONNET,
		MODEL_CLAUDE_3_7_SONNET,
	}
)

func IsModelSupported(modelName string) bool {
	return slices.Contains(supportedModels, modelName)
}

type AnthropicModel struct {
	ModelName string
}

// NewModel creates a new AnthropicModel instance
func NewModel(modelName string) *AnthropicModel {
	// Check if the model name is supported
	if !slices.Contains(supportedModels, modelName) {
		return nil
	}
	// Create a new AnthropicModel instance
	return &AnthropicModel{
		ModelName: modelName,
	}
}

func (m AnthropicModel) Name() string {
	return m.ModelName
}

func (m AnthropicModel) GenerateSimple(userPrompt string, opts ai.GenerateOpts) (string, error) {

	err := validateInputSimple(opts.APIKey, userPrompt)
	if err != nil {
		return "", err
	}

	if opts.MaxTokens < 1 {
		opts.MaxTokens = DEFAULT_MAX_TOKENS
	}

	// Prepare the messages
	messages := []Message{
		{
			Role:    ROLE_USER,
			Content: userPrompt,
		},
		// {
		// 	Role:    ROLE_ASSISTANT,
		// 	Content: "{ \"type\":\"",
		// },
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

	req, err := http.NewRequest("POST", apiUrlMessages, bytes.NewBuffer(jsonData))
	if err != nil {
		return "", fmt.Errorf("error creating request: %v", err)
	}

	req.Header.Set("Content-Type", "application/json")
	req.Header.Set("x-api-key", opts.APIKey)
	req.Header.Set("anthropic-version", apiVersion)

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

	if response.Type == "error" {
		return "", fmt.Errorf("error from Anthropic: %s", response.Message)
	}

	if len(response.Content) == 0 {
		return "", fmt.Errorf("empty response from Anthropic")
	}

	return response.Content[0].Text, nil
}

func (m AnthropicModel) Generate(messages []ai.Message, opts ai.GenerateOpts) (string, error) {

	err := validateInput(opts.APIKey, messages)
	if err != nil {
		return "", err
	}

	if opts.MaxTokens < 1 {
		opts.MaxTokens = DEFAULT_MAX_TOKENS
	}

	fmt.Println("[🐞][🤖][AnthropicModel][Generate] messages:", messages)

	anthropicMessages := convertAIMessagesToAnthropicMessages(messages)

	reqBody := GenerateRequest{
		Model:     m.ModelName,
		Messages:  anthropicMessages,
		MaxTokens: opts.MaxTokens,
		Stream:    false,
		System:    opts.SystemPrompt.String(),
	}

	jsonData, err := json.Marshal(reqBody)
	if err != nil {
		return "", fmt.Errorf("error creating JSON request: %v", err)
	}

	req, err := http.NewRequest("POST", apiUrlMessages, bytes.NewBuffer(jsonData))
	if err != nil {
		return "", fmt.Errorf("error creating request: %v", err)
	}

	req.Header.Set("Content-Type", "application/json")
	req.Header.Set("x-api-key", opts.APIKey)
	req.Header.Set("anthropic-version", apiVersion)

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

	if response.Type == "error" {
		return "", fmt.Errorf("error from Anthropic: %s", response.Message)
	}

	if len(response.Content) == 0 {
		return "", fmt.Errorf("no response from Anthropic")
	}

	return response.Content[0].Text, nil
}

func (m AnthropicModel) GenerateStream(messages []ai.Message, opts ai.GenerateOpts) (chan string, error) {

	err := validateInput(opts.APIKey, messages)
	if err != nil {
		return nil, err
	}

	if opts.MaxTokens < 1 {
		opts.MaxTokens = DEFAULT_MAX_TOKENS
	}

	fmt.Println("[🐞][🤖][AnthropicModel][GenerateStream] messages:", messages)

	anthropicMessages := convertAIMessagesToAnthropicMessages(messages)

	reqBody := GenerateRequest{
		Model:     m.ModelName,
		Messages:  anthropicMessages,
		MaxTokens: opts.MaxTokens,
		Stream:    true,
		System:    opts.SystemPrompt.String(),
	}

	jsonData, err := json.Marshal(reqBody)
	if err != nil {
		return nil, fmt.Errorf("error marshaling request: %v", err)
	}

	req, err := http.NewRequest("POST", apiUrlMessages, bytes.NewBuffer(jsonData))
	if err != nil {
		return nil, fmt.Errorf("error creating request: %v", err)
	}

	req.Header.Set("Content-Type", "application/json")
	req.Header.Set("x-api-key", opts.APIKey)
	req.Header.Set("anthropic-version", apiVersion)

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
				// fmt.Printf("error parsing stream data: %v\n", err)
				continue
			}

			if streamResp.Type == "content_block_delta" {
				text := streamResp.Delta.Text
				responseChan <- text
			}
		}
	}()

	return responseChan, nil
}
