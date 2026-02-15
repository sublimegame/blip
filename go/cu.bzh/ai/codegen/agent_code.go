package main

import (
	"context"
	"encoding/json"
	"fmt"
	"strings"

	"cu.bzh/ai"
	ollama "github.com/ollama/ollama/api"
)

func agentCodeGeneratorStream(req PromptRequest, chromaClient *ChromaClient) (chan string, error) {
	fmt.Println("🤖 AGENT: CODE GENERATOR STREAM")

	// Get API key for the model (can be an empty string)
	apiKey, err := getApiKeyForModelName(req.Model)
	if err != nil {
		return nil, fmt.Errorf("failed to get API key: %v", err)
	}

	client, err := getOllamaClient()
	if err != nil {
		return nil, err
	}
	client.Heartbeat(context.Background())

	embeddingReq := &ollama.EmbeddingRequest{
		Model:  MODEL_EMBEDDING_NAME,
		Prompt: req.Prompt,
	}

	resp, err := client.Embeddings(context.Background(), embeddingReq)
	if err != nil {
		return nil, err
	}
	embedding := resp.Embedding
	fmt.Println("Computed embeddings")

	docsEmbeddingsCollection, err := chromaClient.GetCollection("docs")
	if err != nil {
		return nil, fmt.Errorf("failed to get collection: %v", err)
	}

	docsEmbeddings, err := docsEmbeddingsCollection.Query(ChromaCollectionQuery{
		Embeddings: [][]float64{
			embedding,
		},
		NResults: 7,
	})
	if err != nil {
		return nil, err
	}

	sampleScriptEmbeddingsCollection, err := chromaClient.GetCollection("sampleScripts")
	if err != nil {
		return nil, fmt.Errorf("failed to get collection: %v", err)
	}

	sampleScriptEmbeddings, err := sampleScriptEmbeddingsCollection.Query(ChromaCollectionQuery{
		Embeddings: [][]float64{
			embedding,
		},
		NResults: 5,
	})
	if err != nil {
		return nil, err
	}

	messages := []ai.Message{
		{
			Role:    ai.ROLE_USER,
			Content: req.Prompt,
		},
	}

	systemPrompt := ai.NewSystemPrompt()
	systemPrompt.Append(ai.NewTextPart("You're a senior Luau developer, your name is Buzz."))
	systemPrompt.Append(ai.NewTextPart("You're a bit of a nerd, but you're also very friendly and easy to talk to."))
	systemPrompt.Append(ai.NewTextPart("You program for the Cubzh platform, NOT Roblox"))
	systemPrompt.Append(ai.NewTextPart("Output full modified Luau code as response, wrapped within luau code blocks."))
	systemPrompt.Append(ai.NewTextPart("Do not output anything other than the code."))

	systemPrompt.Append(ai.NewTextPart("Here is a description of the current scene (right before script execution):"))
	systemPrompt.Append(ai.NewTextPart("```json\n" + req.Scene + "\n```"))
	systemPrompt.Append(ai.NewTextPart("The `Name` field of each entity in the JSON matches the `Name` property of loaded objects."))
	systemPrompt.Append(ai.NewTextPart("It can be used to find the object in the scene using World:FindFirst(function(object) return object.Name == \"someName\" end), or World:Find(function(object) return object.Name == \"someName\" end) that returns a table if looking for multiple objects."))

	// DIFF VERSION
	// system += "\nApply changes to the script and output the diff."
	// system += "\nThe diff must be in Unified Diff format"
	// system += "\nShortest possible diffs are best."
	// system += "\nAll your outputs are raw diffs, nothing else."

	systemPrompt.Append(ai.NewTextPart("Pages from the documentation that can be useful:"))
	for _, embedding := range docsEmbeddings {
		var doc Document
		if err := json.Unmarshal([]byte(embedding.Document), &doc); err != nil {
			continue
		}
		fmt.Println("REFERENCE PAGE:", doc.Path)
		systemPrompt.Append(ai.NewTextPart("=========="))
		// systemPrompt.Append(ai.NewTextPart("page: " + doc.Path))
		// systemPrompt.Append(ai.NewTextPart("-----------------------------------"))
		systemPrompt.Append(ai.NewTextPart(doc.Markdown)) // TODO: gdevillele: maybe use FilePart instead of TextPart (it doesn't exist yet)
		systemPrompt.Append(ai.NewTextPart("=========="))
	}

	systemPrompt.Append(ai.NewTextPart("Sample scripts that can also be useful:"))
	for _, embedding := range sampleScriptEmbeddings {
		var doc Document
		if err := json.Unmarshal([]byte(embedding.Document), &doc); err != nil {
			continue
		}
		fmt.Println("SAMPLE SCRIPT:", doc.Path)
		systemPrompt.Append(ai.NewTextPart("=========="))
		systemPrompt.Append(ai.NewTextPart(doc.Markdown)) // TODO: gdevillele: maybe use FilePart instead of TextPart (it doesn't exist yet)
		systemPrompt.Append(ai.NewTextPart("=========="))
	}

	systemPrompt.Append(ai.NewTextPart("Here's the current script that needs to be modified:"))
	systemPrompt.Append(ai.NewTextPart("```luau\n" + req.Script + "\n```"))

	systemPrompt.Append(ai.NewTextPart("NOTE(1): Impact does not have a position, to compute the position of impact when casting a ray from a pointer event: event.Position + event.Direction * impact.Distance"))
	systemPrompt.Append(ai.NewTextPart("NOTE(2): to normalize a Number3: someNumber3:Normalize()"))
	systemPrompt.Append(ai.NewTextPart("NOTE(3): when copying a Shape or MutableShape it's often good to include children: `local s2 = Shape(s1, {includeChildren = true})`"))
	systemPrompt.Append(ai.NewTextPart("NOTE(4): when saying 'where I click', it usually implies 'if there's an impact'"))
	systemPrompt.Append(ai.NewTextPart("NOTE(5): a raw table in Lua doens't expose fields like X, Y, Z (while instances like Number2, Number3 or Rotation do)"))

	systemPrompt.Append(ai.NewTextPart("Only modify the minimum amount of code necessary to answer the user's request."))
	systemPrompt.Append(ai.NewTextPart("When user refers to himself, it's usually a reference to the `Player` object."))

	if req.Model == MODEL_NAME_LOCAL {
		// For local LLM, we need to implement a streaming version
		responseChan := make(chan string)
		go func() {
			defer close(responseChan)

			// Use the non-streaming version and then send the result through the channel
			response, err := askLLM(messages, askLLMOpts{
				ModelName:    req.Model,
				SystemPrompt: systemPrompt,
				apiKey:       apiKey,
			})
			if err != nil {
				fmt.Printf("[agentCodeGeneratorStream][ERROR]: %v\n", err)
				return
			}

			// Remove ```luau and ``` tags from the response
			response = strings.TrimPrefix(response, "```luau")
			response = strings.TrimPrefix(response, "```lua") // Handle potential lua tag variation
			response = strings.TrimSuffix(response, "```")
			response = strings.TrimSpace(response)

			// Send the response through the channel
			responseChan <- response
		}()
		return responseChan, nil

	} else {

		// For non-local LLM, use the existing streaming function
		responseChan, err := askLLMWithStreamedResponse(messages, askLLMOpts{
			ModelName:    req.Model,
			SystemPrompt: systemPrompt,
			apiKey:       apiKey,
		})
		if err != nil {
			return nil, fmt.Errorf("[agentCodeGeneratorStream][ERROR]: %v", err)
		}

		// Process the stream to remove code block markers
		processedChan := make(chan string)
		go func() {
			defer close(processedChan)

			var buffer string
			inCodeBlock := false
			partialBacktick := false
			waitingForNewline := false
			codeBlockStartPos := 0

			for chunk := range responseChan {
				buffer += chunk

				// Handle potential partial backtick markers at start or end
				if (strings.HasSuffix(buffer, "`") || strings.HasSuffix(buffer, "``")) && !strings.HasSuffix(buffer, "```") {
					partialBacktick = true
					continue
				}
				partialBacktick = false

				// Check for code block start markers
				if !inCodeBlock && strings.Contains(buffer, "```") {
					inCodeBlock = true
					codeBlockStartPos = strings.Index(buffer, "```") + 3
					waitingForNewline = true
				}

				if waitingForNewline {
					// Find the position after ```

					// Check for newline after potential language identifier
					newlinePos := strings.IndexByte(buffer[codeBlockStartPos:], '\n')
					if newlinePos != -1 {
						// Found newline, remove language identifier
						buffer = buffer[codeBlockStartPos+newlinePos+1:]
						waitingForNewline = false
					} else {
						// keep waiting for new line
						continue
					}
				}

				// Check for code block end markers, handling partial backticks
				if inCodeBlock {
					if strings.Contains(buffer, "```") {
						inCodeBlock = false
						buffer = strings.Replace(buffer, "```", "", 1)
					} else if strings.HasSuffix(buffer, "`") || strings.HasSuffix(buffer, "``") {
						// Potential partial end marker, wait for more
						continue
					}
				}

				// Only send if we're not waiting for a potential backtick completion or newline
				if !partialBacktick && !waitingForNewline && len(buffer) > 0 {
					processedChan <- buffer
					buffer = ""
				}
			}

			// Send any remaining content
			if len(buffer) > 0 {
				buffer = strings.Replace(buffer, "```", "", 1)
				processedChan <- buffer
			}
		}()

		return processedChan, nil
	}
}

// // TODO: add "max tokens"
// func agentCodeGenerator(req PromptRequest, apiKey string, chromaClient *ChromaClient) (string, error) {
// 	fmt.Println("🤖 AGENT: CODE GENERATOR")

// 	client, err := getOllamaClient()
// 	if err != nil {
// 		return "", err
// 	}
// 	client.Heartbeat(context.Background())

// 	embeddingReq := &ollama.EmbeddingRequest{
// 		Model:  MODEL_EMBEDDING_NAME,
// 		Prompt: req.Prompt,
// 		// KeepAlive
// 		// Options
// 	}

// 	resp, err := client.Embeddings(context.Background(), embeddingReq)
// 	if err != nil {
// 		return "", err
// 	}
// 	embedding := resp.Embedding
// 	// fmt.Println(embedding)

// 	docsEmbedding, err := chromaClient.GetCollection("docs")
// 	if err != nil {
// 		return "", fmt.Errorf("failed to get collection: %v", err)
// 	}

// 	embeddings, err := docsEmbedding.Query(ChromaCollectionQuery{
// 		Embeddings: [][]float64{
// 			embedding,
// 		},
// 		NResults: 5,
// 	})

// 	if err != nil {
// 		return "", err
// 	}

// 	// fmt.Println("EMBEDDINGS:", len(embeddings))
// 	/*
// 		type ChromaCollectionEntry struct {
// 			Embedding *[]float64     `json:"embedding,omitempty"`
// 			Document  string         `json:"document,omitempty"`
// 			Metadatas map[string]any `json:"metadatas,omitempty"`
// 			ID        string         `json:"id,omitempty"`
// 			Distance  float64        `json:"distance,omitempty"`
// 		}
// 	*/

// 	messages := []ai.Message{
// 		{
// 			Role:    ai.ROLE_USER,
// 			Content: req.Prompt,
// 		},
// 	}

// 	systemPrompt := ai.NewSystemPrompt()
// 	systemPrompt.Append(ai.NewTextPart("You're a senior Luau developer, your name is Buzz. You're a bit of a nerd, but you're also very friendly and easy to talk to."))
// 	systemPrompt.Append(ai.NewTextPart("You program for the Cubzh platform, NOT Roblox"))
// 	systemPrompt.Append(ai.NewTextPart("Do not insert comments to explain what you did, shortest possible diffs are best."))
// 	// systemPrompt.Append(ai.NewTextPart("Provide only full modified Luau code as response, wrapped within ```luau code blocks"))
// 	systemPrompt.Append(ai.NewTextPart("Provide only the code diff. You don't need to return the entire code. Your client is a git program, it knows how to apply the diff. The diff must be in unified diff format."))

// 	systemPrompt.Append(ai.NewTextPart("Pages from the documentation that can be useful to form your answer:"))
// 	for _, embedding := range embeddings {
// 		var doc Document
// 		if err := json.Unmarshal([]byte(embedding.Document), &doc); err != nil {
// 			continue
// 		}
// 		fmt.Println("PAGE:", doc.Path)
// 		systemPrompt.Append(ai.NewTextPart("==================================="))
// 		// systemPrompt.Append(ai.NewTextPart("page: " + doc.Path))
// 		// systemPrompt.Append(ai.NewTextPart("-----------------------------------"))
// 		systemPrompt.Append(ai.NewTextPart(doc.Markdown))
// 		systemPrompt.Append(ai.NewTextPart("==================================="))
// 	}

// 	samples := appendScriptSamples("")
// 	systemPrompt.Append(ai.NewTextPart(samples))

// 	systemPrompt.Append(ai.NewTextPart("The current script (sent by the user):"))
// 	systemPrompt.Append(ai.NewTextPart("```luau\n" + req.Script + "\n```"))

// 	systemPrompt.Append(ai.NewTextPart("NOTE(1): Impact does not have a position, to compute the position of impact when casting a ray from a pointer event: event.Position + event.Direction * impact.Distance"))
// 	systemPrompt.Append(ai.NewTextPart("NOTE(2): to normalize a Number3: someNumber3:Normalize()"))
// 	systemPrompt.Append(ai.NewTextPart("NOTE(3): when copying a Shape or MutableShape it's often good to include children: `local s2 = Shape(s1, {includeChildren = true})`"))
// 	systemPrompt.Append(ai.NewTextPart("NOTE(4): when saying 'where I click', it usually implies 'if there's an impact'")) // samples could help

// 	systemPrompt.Append(ai.NewTextPart("Only modify the minimum amount of code necessary to answer the user's request."))
// 	systemPrompt.Append(ai.NewTextPart("When user refers to himself, it's usually a reference to the `Player` object."))

// 	response, err := askLLM(messages, askLLMOpts{
// 		ModelName:    req.Model,
// 		SystemPrompt: systemPrompt,
// 		apiKey:       apiKey,
// 	})
// 	if err != nil {
// 		return "", fmt.Errorf("[agentCodeGenerator][ERROR]: %v", err)
// 	}

// 	// Remove ```luau and ``` tags from the response
// 	response = strings.TrimPrefix(response, "```luau")
// 	response = strings.TrimPrefix(response, "```lua") // Handle potential lua tag variation
// 	response = strings.TrimSuffix(response, "```")
// 	response = strings.TrimSpace(response)

// 	// fmt.Println("RESPONSE: " + response)

// 	return response, nil
// }
