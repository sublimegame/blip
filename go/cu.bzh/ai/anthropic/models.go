package anthropic

import (
	"encoding/json"
	"fmt"
	"io"
	"net/http"

	"cu.bzh/ai"
)

// GetModels returns a list of all supported models
func GetModels(apiKey string) ([]ai.ModelInfo, error) {
	req, err := http.NewRequest("GET", apiUrlModels, nil)
	if err != nil {
		return nil, fmt.Errorf("error creating request: %v", err)
	}

	req.Header.Set("Content-Type", "application/json")
	req.Header.Set("x-api-key", apiKey)
	req.Header.Set("anthropic-version", apiVersion)

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

	var response GetModelsResponse
	err = json.Unmarshal(body, &response)
	if err != nil {
		return nil, fmt.Errorf("error parsing response: %v", err)
	}

	// construct return value
	models := make([]ai.ModelInfo, len(response.Data))
	for i, model := range response.Data {
		models[i] = ai.ModelInfo{
			DisplayName: model.DisplayName,
			ID:          model.ID,
			CreatedAt:   model.CreatedAt,
			Type:        model.Type,
		}
	}

	return models, nil
}
