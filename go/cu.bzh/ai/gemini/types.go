package gemini

import "cu.bzh/ai"

//
// Models
//

type GetModelsResponse struct {
	Models        []ModelInfo `json:"models"`
	NextPageToken string      `json:"nextPageToken,omitempty"`
}

type ModelInfo struct {
	Name                       string   `json:"name"`
	BaseModelID                string   `json:"baseModelId"`
	Version                    string   `json:"version"`
	DisplayName                string   `json:"displayName"`
	Description                string   `json:"description"`
	InputTokenLimit            int      `json:"inputTokenLimit"`
	OutputTokenLimit           int      `json:"outputTokenLimit"`
	SupportedGenerationMethods []string `json:"supportedGenerationMethods"`
	Temperature                float64  `json:"temperature"`
	MaxTemperature             float64  `json:"maxTemperature"`
	TopP                       float64  `json:"topP"`
	TopK                       int      `json:"topK"`
}

//
// Generate request body (text input)
//

type GenerateRequestBody struct {
	Contents          []Content         `json:"contents"`
	SystemInstruction SystemInstruction `json:"system_instruction,omitempty"`
	GenerationConfig  GenerationConfig  `json:"generationConfig"`
}

type Content struct {
	// Role can be omitted for request that are not multi-turn conversation
	Role  ai.MessageRole `json:"role,omitempty"`
	Parts []Part         `json:"parts"`
}

type SystemInstruction struct {
	Parts []Part `json:"parts"`
}

type Part struct {
	Text string `json:"text"`
}

type GenerationConfig struct {
	StopSequences   []string `json:"stopSequences,omitempty"`
	Temperature     float64  `json:"temperature,omitempty"`
	MaxOutputTokens int      `json:"maxOutputTokens,omitempty"`
	TopP            float64  `json:"topP,omitempty"`
	TopK            int      `json:"topK,omitempty"`
}

//
// Generate reponse
//

type GenerateResponse struct {
	Candidates    []Candidate   `json:"candidates"`
	ModelVersion  string        `json:"modelVersion"`
	UsageMetadata UsageMetadata `json:"usageMetadata"`
	// Note: there is also a "promptFeedback" field in the response
}

type Candidate struct {
	Content      Content `json:"content"`
	FinishReason string  `json:"finishReason"`
	Index        int     `json:"index"`
}

type UsageMetadata struct {
	PromptTokenCount     int            `json:"promptTokenCount"`
	CandidatesTokenCount int            `json:"candidatesTokenCount"`
	TotalTokenCount      int            `json:"totalTokenCount"`
	PromptTokensDetails  []TokenDetails `json:"promptTokensDetails"`
	ThoughtsTokenCount   int            `json:"thoughtsTokenCount"`
}

type TokenDetails struct {
	Modality   string `json:"modality"`
	TokenCount int    `json:"tokenCount"`
}
