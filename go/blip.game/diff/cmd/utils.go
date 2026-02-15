package main

import (
	"bytes"
	"encoding/json"
	"fmt"
	"log"
	"net/http"

	"github.com/pkoukk/tiktoken-go"
)

func constructSystemPrompt(diffMode bool, fileContent []byte) string {
	system := "You are an expert developer. You can recognize all programming languages."
	system += "\nYou are given a code snippet and an assignment from the user."
	system += "\nThe assignment is a description of the changes you need to make to the code snippet."
	if diffMode {
		system += "\nYou have to complete the assignment and return the diff of the code changes you made."
		system += "\nThe diff must be in unified diff format."
		system += "\nYou should only return the diff, nothing else."
	} else {
		system += "\nYou have to complete the assignment and return the modified code snippet."
		system += "\nYou should only return the modified code snippet, nothing else."
	}
	system += "\nThe code snippet is:\n" + string(fileContent)
	return system
}

func computeTokenCount(text string) int {
	encoding, err := tiktoken.GetEncoding("cl100k_base")
	if err != nil {
		log.Fatalf("Error getting encoding: %v", err)
	}
	return len(encoding.Encode(text, nil, nil))
}

type queryLlmOpts struct {
	model     string
	apiKey    string
	stream    bool
	maxTokens int
}

// response body must be read and closed by the caller
func getLLMResponse(system string, messages []Message, opts queryLlmOpts) (*http.Response, error) {
	reqBody := Request{
		Model:     opts.model,
		Messages:  messages,
		MaxTokens: opts.maxTokens,
		Stream:    opts.stream,
		System:    system,
	}

	jsonData, err := json.Marshal(reqBody)
	if err != nil {
		return nil, fmt.Errorf("error marshaling request: %v", err)
	}

	req, err := http.NewRequest("POST", "https://api.anthropic.com/v1/messages", bytes.NewBuffer(jsonData))
	if err != nil {
		return nil, fmt.Errorf("error creating request: %v", err)
	}

	req.Header.Set("Content-Type", "application/json")
	req.Header.Set("x-api-key", opts.apiKey)
	req.Header.Set("anthropic-version", "2023-06-01")

	client := &http.Client{}
	resp, err := client.Do(req)
	if err != nil {
		return nil, fmt.Errorf("error sending request: %v", err)
	}

	return resp, nil
}
