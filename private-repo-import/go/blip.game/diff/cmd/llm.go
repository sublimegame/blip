package main

import (
	"bufio"
	"encoding/json"
	"fmt"
	"io"
	"strings"
)

const (
	MODEL = "claude-3-7-sonnet-20250219"
)

type Message struct {
	Role    string `json:"role"`
	Content string `json:"content"`
}

type Request struct {
	Model     string    `json:"model"`
	Messages  []Message `json:"messages"`
	MaxTokens int       `json:"max_tokens"`
	Stream    bool      `json:"stream"`
	System    string    `json:"system"`
}

type ContentItem struct {
	Type string `json:"type"`
	Text string `json:"text"`
}

type Response struct {
	Content []ContentItem `json:"content"`
}

type Delta struct {
	Type string `json:"type"`
	Text string `json:"text"`
}

type StreamResponse struct {
	Type  string `json:"type"`
	Delta Delta  `json:"delta"`
	Index int    `json:"index"`
}

func askLLM(system string, messages []Message, apiKey string) (string, error) {
	// fmt.Printf("system token count: %d\n", computeTokenCount(system))

	if apiKey == "" {
		return "", fmt.Errorf("apiKey is empty")
	}

	opts := queryLlmOpts{
		model:     MODEL,
		apiKey:    apiKey,
		stream:    false,
		maxTokens: 4096,
	}

	resp, err := getLLMResponse(system, messages, opts)
	if err != nil {
		return "", err
	}

	defer resp.Body.Close()

	body, err := io.ReadAll(resp.Body)
	if err != nil {
		return "", fmt.Errorf("error reading response: %v", err)
	}

	fmt.Printf("response: %s\n", string(body))

	var response Response
	err = json.Unmarshal(body, &response)
	if err != nil {
		return "", fmt.Errorf("error parsing response: %v\nraw response: %s", err, string(body))
	}

	// Extract text from content array
	if len(response.Content) > 0 {
		return response.Content[0].Text, nil
	}

	return "", fmt.Errorf("no content in response")
}

// askLLMWithStreamedResponse returns a channel that streams the LLM response chunks as they arrive
func askLLMWithStreamedResponse(system string, messages []Message, apiKey string) (io.Reader, error) {
	// fmt.Printf("system token count: %d\n", computeTokenCount(system))

	if apiKey == "" {
		return nil, fmt.Errorf("apiKey is empty")
	}

	opts := queryLlmOpts{
		model:     MODEL,
		apiKey:    apiKey,
		stream:    true,
		maxTokens: 4096,
	}

	resp, err := getLLMResponse(system, messages, opts)
	if err != nil {
		return nil, err
	}

	// Create a pipe to convert the response body into an io.Reader that can be returned to the caller
	pipeReader, pipeWriter := io.Pipe()

	go func() {
		defer resp.Body.Close()
		defer pipeWriter.Close()

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
				pipeWriter.Write([]byte(text))
			}
		}
	}()

	return pipeReader, nil
}
