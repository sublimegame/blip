package main

import (
	"bytes"
	"encoding/json"
	"errors"
	"fmt"
	"net/http"
	"os"
	"strings"
	"time"
)

var REPLICATE_TOKEN = os.Getenv("REPLICATE_TOKEN")

const (
	REPLICATE_API_URL = "https://api.replicate.com/v1/predictions"
	API_MODELS        = "https://api.replicate.com/v1/models/"
	// MODEL                 = "mistralai/mistral-7b-instruct-v0.2" // "mistralai/mixtral-8x7b-instruct-v0.1"
	MODEL                 = "meta/codellama-13b-instruct"
	MODEL_VERSION         = "a5e2d67630195a09b96932f5fa541fe64069c97d40cd0b69cdd91919987d0e7f" // meta/codellama-13b-instruct
	RETRIES               = 100
	SLEEP_DURATION        = 500 // ms
	REPLICATE_TEMPERATURE = 0.1
	MAX_NEW_TOKENS        = 256 // mistral
	REPLICATE_MAX_TOKENS  = 500 // llama
)

type ReplicatePredictionStatus string

// Define constants for each enum value
const (
	Starting   ReplicatePredictionStatus = "starting"
	Processing ReplicatePredictionStatus = "processing"
	Succeeded  ReplicatePredictionStatus = "succeeded"
	Failed     ReplicatePredictionStatus = "failed"
	Canceled   ReplicatePredictionStatus = "canceled"
)

type ReplicatePredictionRequest struct {
	Input   *ReplicatePredictionInput `json:"input,omitempty"`
	Version string                    `json:"version,omitempty"`
}

type ReplicatePredictionInput struct {
	Prompt                string `json:"prompt,omitempty"`
	SystemPrompt          string `json:"system_prompt,omitempty"` // llama
	REPLICATE_TEMPERATURE string `json:"temperature,omitempty"`
	PromptTemplate        string `json:"prompt_template,omitempty"`
	MaxNewTokens          int    `json:"max_new_tokens,omitempty"` // mistral
	MaxTokens             int    `json:"max_tokens,omitempty"`     // llama
}

type ReplicateResponseURLs struct {
	Cancel string `json:"cancel,omitempty"`
	Get    string `json:"get,omitempty"`
}

type ReplicateReplicatePredictionObjectMetrics struct {
	PredictTime float32 `json:"predict_time,omitempty"`
}

type ReplicatePredictionObject struct {
	ID      string                                     `json:"id,omitempty"`
	Model   string                                     `json:"model,omitempty"`
	Version string                                     `json:"version,omitempty"`
	Input   *ReplicatePredictionInput                  `json:"input,omitempty"`
	Logs    string                                     `json:"logs,omitempty"`
	Error   string                                     `json:"error,omitempty"`
	Status  ReplicatePredictionStatus                  `json:"status,omitempty"`
	URLs    *ReplicateResponseURLs                     `json:"urls,omitempty"`
	Output  []string                                   `json:"output,omitempty"`
	Metrics *ReplicateReplicatePredictionObjectMetrics `json:"metrics,omitempty"`
	// CreatedAt
}

// func main() {

// 	systemPrompt := "You're an assistant that's helping users of a user-generated content platform called Cubzh. Users can be players, artists or coders. When there's a question about code or scripting without indication about the language: always assume the language is Lua and surround code blocks with ```lua and ``` markers. Answers should be as short as possible, for a young audience (teenagers)."

// 	output, err := askReplicate("How to declare a local variable?", systemPrompt)
// 	if err != nil {
// 		fmt.Println("ERROR:", err.Error())
// 	}

// 	fmt.Println("Output:", output)
// }

func askReplicate(prompt string, systemPrompt string) (string, error) {

	ReplicatePredictionRequest := &ReplicatePredictionRequest{
		Version: MODEL_VERSION,
		Input: &ReplicatePredictionInput{
			Prompt:       prompt,
			SystemPrompt: systemPrompt,
			// PromptTemplate: "<s>[INST] {prompt} [/INST]</s>",
			// MaxNewTokens: MAX_NEW_TOKENS,
			MaxTokens: REPLICATE_MAX_TOKENS,
		},
	}

	payload, err := json.Marshal(ReplicatePredictionRequest)
	if err != nil {
		return "", err
	}

	// url := API_MODELS + MODEL + "/predictions"
	url := REPLICATE_API_URL

	req, err := http.NewRequest("POST", url, bytes.NewBuffer(payload))
	req.Header.Set("Content-Type", "application/json")
	req.Header.Set("Authorization", "Token "+REPLICATE_TOKEN)
	if err != nil {
		return "", err
	}

	client := &http.Client{}

	resp, err := client.Do(req)
	if err != nil {
		return "", err
	}
	defer resp.Body.Close()

	if resp.StatusCode != http.StatusCreated {
		return "", errors.New("status code: " + resp.Status)
	}

	var o ReplicatePredictionObject

	err = json.NewDecoder(resp.Body).Decode(&o)

	if err != nil {
		return "", err
	}

	retries := RETRIES

	for o.Status == Starting || o.Status == Processing {
		time.Sleep(SLEEP_DURATION * time.Millisecond)

		req, err := http.NewRequest("GET", o.URLs.Get, nil)
		req.Header.Set("Content-Type", "application/json")
		req.Header.Set("Authorization", "Token "+REPLICATE_TOKEN)
		if err != nil {
			return "", err
		}

		resp, err = client.Do(req)
		if err != nil {
			return "", err
		}
		defer resp.Body.Close()

		if resp.StatusCode != http.StatusOK {
			return "", errors.New("status code (2): " + resp.Status)
		}

		fmt.Println(o.Status)

		err = json.NewDecoder(resp.Body).Decode(&o)
		if err != nil {
			return "", err
		}

		retries -= 1
		if retries == 0 {
			return "", errors.New("tried to many times")
		}
	}

	// fmt.Println("predict time:", o.Metrics.PredictTime)

	return strings.Join(o.Output, ""), nil
}
