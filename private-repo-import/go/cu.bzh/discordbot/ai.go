package main

import (
	"bytes"
	"encoding/json"
	"errors"
	"fmt"
	"io"
	"net/http"
)

const (
	OPEN_AI_MODEL = "text-davinci-003"
	MAX_TOKENS    = 100
	TEMPERATURE   = 0.1
)

type CompletionRequest struct {
	Model       string  `json:"model,omitempty"`
	Prompt      string  `json:"prompt,omitempty"`
	MaxToken    int     `json:"max_tokens,omitempty"`
	Temperature float32 `json:"temperature,omitempty"`
}

type CompletionResponse struct {
	Choices []*Choice `json:"choices,omitempty"`
}

type Choice struct {
	Text string `json:"text,omitempty"`
}

func ask(prompt string) (string, error) {

	completionRequest := &CompletionRequest{
		Model:       OPEN_AI_MODEL,
		Prompt:      prompt,
		MaxToken:    MAX_TOKENS,
		Temperature: TEMPERATURE,
	}

	payload, err := json.Marshal(completionRequest)
	if err != nil {
		return "", err
	}

	fmt.Println("JSON:", string(payload))

	req, err := http.NewRequest("POST", "https://api.openai.com/v1/completions", bytes.NewBuffer(payload))
	if err != nil {
		return "", err
	}
	req.Header.Set("Content-Type", "application/json")
	req.Header.Set("Authorization", "Bearer "+OPEN_AI_TOKEN)
	req.Header.Set("OpenAI-Organization", OPEN_AI_ORG)

	client := &http.Client{}

	resp, err := client.Do(req)
	if err != nil {
		return "", err
	}
	defer resp.Body.Close()

	if resp.StatusCode != http.StatusOK {
		return "", errors.New("status code: " + resp.Status)
	}

	body, err := io.ReadAll(resp.Body)
	if err != nil {
		return "", err
	}

	var result CompletionResponse
	if err := json.Unmarshal(body, &result); err != nil {
		return "", err
	}

	if result.Choices != nil && len(result.Choices) > 0 {
		return result.Choices[0].Text, nil
	}

	return "", errors.New("no text response")
}
