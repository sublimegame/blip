package grok

import (
	"encoding/json"
	"fmt"
	"io"
	"net/http"

	"cu.bzh/ai"
)

const (
	// API URLs (not exposed)
	apiUrlModels         = "https://api.x.ai/v1/models"
	apiUrlLanguageModels = "https://api.x.ai/v1/language-models"
)

// GetModels returns a list of all supported models
func GetModels(apiKey string) ([]ai.ModelInfo, error) {

	if apiKey == "" {
		return nil, fmt.Errorf("api key is not set")
	}

	req, err := http.NewRequest("GET", apiUrlModels, nil)
	if err != nil {
		return nil, fmt.Errorf("error creating request: %v", err)
	}

	req.Header.Set("Content-Type", "application/json")
	req.Header.Set("Authorization", fmt.Sprintf("Bearer %s", apiKey))

	client := &http.Client{}
	resp, err := client.Do(req)
	if err != nil {
		return nil, fmt.Errorf("error sending request: %v", err)
	}
	defer resp.Body.Close()

	body, err := io.ReadAll(resp.Body)
	if err != nil {
		return nil, fmt.Errorf("error reading response: %v", err)
	}

	fmt.Println("response: ", string(body))

	var response GetModelsResponse
	err = json.Unmarshal(body, &response)
	if err != nil {
		return nil, fmt.Errorf("error parsing response: %v", err)
	}

	// construct return value
	models := make([]ai.ModelInfo, len(response.Data))
	for i, model := range response.Data {
		models[i] = ai.ModelInfo{
			DisplayName: model.ID,
			ID:          model.ID,
			CreatedAt:   model.CreatedAt,
			Type:        model.Object,
		}
	}

	return models, nil
}
