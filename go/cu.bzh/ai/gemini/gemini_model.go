package gemini

import (
	"bytes"
	"encoding/json"
	"fmt"
	"io"
	"net/http"
	"slices"

	"github.com/bcicen/jstream"

	"cu.bzh/ai"
)

const (
	// Environment variable names
	ENVAR_API_KEY = "GEMINI_API_KEY"

	// Models names
	MODEL_GEMINI_2_5_PRO_PAID         = "gemini-2.5-pro-preview-03-25" // Enhanced thinking and reasoning, multimodal understanding, advanced coding, and more
	MODEL_GEMINI_2_5_PRO_EXPERIMENTAL = "gemini-2.5-pro-exp-03-25"     // Experimental version of the paid model. Data is used for training.
	MODEL_GEMINI_2_0_FLASH            = "gemini-2.0-flash"             // Next generation features, speed, thinking, realtime streaming, and multimodal generation
	MODEL_GEMINI_2_0_FLASH_LITE       = "gemini-2.0-flash-lite"        // cost efficiency and low latency

	// Default config
	DEFAULT_MAX_TOKENS = 8192

	// Roles
	ROLE_USER      ai.MessageRole = "user"
	ROLE_ASSISTANT ai.MessageRole = "model"

	// API URLs (unexposed)
	apiUrlModels                = "https://generativelanguage.googleapis.com/v1beta/models?key=%s"
	apiUrlGenerateContent       = "https://generativelanguage.googleapis.com/v1beta/models/%s:generateContent?key=%s"
	apiUrlStreamGenerateContent = "https://generativelanguage.googleapis.com/v1beta/models/%s:streamGenerateContent?key=%s"
)

var (
	supportedModels = []string{
		MODEL_GEMINI_2_5_PRO_PAID,
		MODEL_GEMINI_2_5_PRO_EXPERIMENTAL,
		MODEL_GEMINI_2_0_FLASH,
		MODEL_GEMINI_2_0_FLASH_LITE,
	}
)

func IsModelSupported(modelName string) bool {
	return slices.Contains(supportedModels, modelName)
}

// GetModels returns a list of all supported models
func GetModels(apiKey string) ([]ai.ModelInfo, error) {
	// construct URL
	url := fmt.Sprintf(apiUrlModels, apiKey)

	// create HTTP request
	req, err := createHttpRequestForGeminiApi("GET", url, nil)
	if err != nil {
		return nil, err
	}

	// send request
	client := &http.Client{}
	resp, err := client.Do(req)
	if err != nil {
		return nil, err
	}
	defer resp.Body.Close()

	// json decode response
	var response GetModelsResponse
	err = json.NewDecoder(resp.Body).Decode(&response)
	if err != nil {
		return nil, fmt.Errorf("error decoding response: %v", err)
	}

	// construct return value
	modelInfos := make([]ai.ModelInfo, len(response.Models))
	for i, model := range response.Models {
		modelInfos[i] = ai.ModelInfo{
			DisplayName: model.DisplayName,
			ID:          model.Name,
		}
	}

	return modelInfos, nil
}

type GeminiModel struct {
	ModelName string
}

func NewModel(modelName string) *GeminiModel {
	// Check if the model name is supported
	if !slices.Contains(supportedModels, modelName) {
		return nil
	}
	// Create a new GeminiModel instance
	return &GeminiModel{
		ModelName: modelName,
	}
}

func (m GeminiModel) Name() string {
	return m.ModelName
}

func (m GeminiModel) GenerateSimple(userPrompt string, opts ai.GenerateOpts) (string, error) {

	// TODO: validate input

	if opts.MaxTokens < 1 {
		opts.MaxTokens = DEFAULT_MAX_TOKENS
	}

	requestBody := GenerateRequestBody{
		Contents: []Content{
			{
				Parts: []Part{
					{
						Text: userPrompt,
					},
				},
			},
		},
	}

	// construct URL
	url := fmt.Sprintf(apiUrlGenerateContent, m.ModelName, opts.APIKey)

	// construct JSON body
	jsonBody, err := json.Marshal(requestBody)
	if err != nil {
		return "", err
	}

	// create HTTP request
	req, err := createHttpRequestForGeminiApi("POST", url, bytes.NewBuffer(jsonBody))
	if err != nil {
		return "", err
	}

	// send request
	client := &http.Client{}
	resp, err := client.Do(req)
	if err != nil {
		return "", err
	}
	defer resp.Body.Close()

	body, err := io.ReadAll(resp.Body)
	if err != nil {
		return "", err
	}

	// parse response
	var response GenerateResponse
	err = json.Unmarshal(body, &response)
	if err != nil {
		return "", err
	}

	// TODO: improve response handling (support multiple candidates & their multiple parts)
	// if response.Type == "error" {
	// 	return "", fmt.Errorf("error from Gemini: %s", response.Message)
	// }
	if len(response.Candidates) == 0 || len(response.Candidates[0].Content.Parts) == 0 {
		return "", fmt.Errorf("empty response from Gemini")
	}

	return response.Candidates[0].Content.Parts[0].Text, nil
}

func (m GeminiModel) Generate(messages []ai.Message, opts ai.GenerateOpts) (string, error) {
	// TODO: gdevillele: validate input

	if opts.MaxTokens < 1 {
		opts.MaxTokens = DEFAULT_MAX_TOKENS
	}

	requestBody := GenerateRequestBody{
		Contents: convertAIMessagesToGeminiContents(messages),
		GenerationConfig: GenerationConfig{
			MaxOutputTokens: opts.MaxTokens,
		},
	}

	// construct URL
	url := fmt.Sprintf(apiUrlStreamGenerateContent, m.ModelName, opts.APIKey)

	// construct JSON body
	jsonBody, err := json.Marshal(requestBody)
	if err != nil {
		return "", err
	}

	// create HTTP request
	req, err := createHttpRequestForGeminiApi("POST", url, bytes.NewBuffer(jsonBody))
	if err != nil {
		return "", err
	}

	// send request
	client := &http.Client{}
	resp, err := client.Do(req)
	if err != nil {
		return "", err
	}
	defer resp.Body.Close()

	var generateResponse GenerateResponse
	err = json.NewDecoder(resp.Body).Decode(&generateResponse)
	if err != nil {
		return "", fmt.Errorf("error decoding response: %v", err)
	}

	return generateResponse.Candidates[0].Content.Parts[0].Text, nil
}

func (m GeminiModel) GenerateStream(messages []ai.Message, opts ai.GenerateOpts) (chan string, error) {
	// TODO: gdevillele: validate input

	if opts.MaxTokens < 1 {
		opts.MaxTokens = DEFAULT_MAX_TOKENS
	}

	requestBody := GenerateRequestBody{
		Contents:          convertAIMessagesToGeminiContents(messages),
		SystemInstruction: convertSystemPromptToGeminiSystemInstruction(opts.SystemPrompt),
		GenerationConfig: GenerationConfig{
			MaxOutputTokens: opts.MaxTokens,
		},
	}

	// construct URL
	url := fmt.Sprintf(apiUrlStreamGenerateContent, m.ModelName, opts.APIKey)

	// construct JSON body
	jsonBody, err := json.Marshal(requestBody)
	if err != nil {
		return nil, err
	}

	// create HTTP request
	req, err := createHttpRequestForGeminiApi("POST", url, bytes.NewBuffer(jsonBody))
	if err != nil {
		return nil, err
	}

	// send request
	client := &http.Client{}
	resp, err := client.Do(req)
	if err != nil {
		return nil, err
	}

	responseChan := make(chan string)

	go func() {
		defer resp.Body.Close()
		defer close(responseChan)

		decoder := jstream.NewDecoder(resp.Body, 1)

		for mv := range decoder.Stream() {
			// GenerateResponse
			response, ok := mv.Value.(map[string]interface{})
			if !ok {
				fmt.Println("[🐞][🤖]❌❌❌ FAILED TO CAST (response)")
				continue
			}
			// candidates
			candidates, ok := response["candidates"].([]interface{})
			if !ok {
				fmt.Println("[🐞][🤖]❌❌❌ FAILED TO CAST (candidates)")
				continue
			}
			for _, candidate := range candidates {
				candidateMap, ok := candidate.(map[string]interface{})
				if !ok {
					fmt.Println("[🐞][🤖]❌❌❌ FAILED TO CAST (candidate)")
					continue
				}
				candidateContent, ok := candidateMap["content"].(map[string]interface{})
				if !ok {
					fmt.Println("[🐞][🤖]❌❌❌ FAILED TO CAST (candidateContent)")
					continue
				}
				candidateContentParts, ok := candidateContent["parts"].([]interface{})
				if !ok {
					fmt.Println("[🐞][🤖]❌❌❌ FAILED TO CAST (candidateContentParts)")
					continue
				}
				// loop over parts
				for _, part := range candidateContentParts {
					partMap, ok := part.(map[string]interface{})
					if !ok {
						fmt.Println("[🐞][🤖]❌❌❌ FAILED TO CAST (part)")
						continue
					}
					partText, ok := partMap["text"].(string)
					if !ok {
						fmt.Println("[🐞][🤖]❌❌❌ FAILED TO CAST (partText)")
						continue
					}
					responseChan <- partText
				}
			}
		}
	}()

	return responseChan, nil
}
