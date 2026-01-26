package main

import (
	"fmt"

	"cu.bzh/ai"
)

var (
	agentChatSystemPrompt = ai.NewSystemPrompt()
)

func init() {
	agentChatSystemPrompt.Append(ai.NewTextPart("You are a senior Luau developer, your name is Buzz."))
	agentChatSystemPrompt.Append(ai.NewTextPart("You are a bit of a nerd, but you are also very friendly and easy to talk to."))
	agentChatSystemPrompt.Append(ai.NewTextPart("User is casually chatting, not asking for code changes. Just answer being as friendly and short as possible. (super short answers are best)"))
}

func agentChatStream(model, apiKey, userPrompt string) (chan string, error) {
	fmt.Println("🤖 AGENT: CHAT STREAM")

	messages := []ai.Message{
		{
			Role:    ai.ROLE_USER,
			Content: userPrompt,
		},
	}

	const LOCAL_LLM bool = false
	if LOCAL_LLM {
		// For local LLM, we need to implement a streaming version
		responseChan := make(chan string)
		go func() {
			defer close(responseChan)

			// Use the non-streaming version and then send the result through the channel
			response, err := askLLM(messages, askLLMOpts{
				ModelName:    model,
				SystemPrompt: agentChatSystemPrompt,
				apiKey:       apiKey,
			})
			if err != nil {
				fmt.Printf("[agentChatStream][ERROR]: %v\n", err)
				return
			}

			// Send the response through the channel
			responseChan <- response
		}()
		return responseChan, nil
	} else {
		fmt.Println("askLLMWithStreamedResponse")

		// For Claude API, use the existing streaming function
		responseChan, err := askLLMWithStreamedResponse(messages, askLLMOpts{
			ModelName:    model,
			SystemPrompt: agentChatSystemPrompt,
			apiKey:       apiKey,
		})
		if err != nil {
			fmt.Println("ERROR:", err.Error())
			return nil, fmt.Errorf("[agentChatStream][ERROR]: %v", err)
		}
		return responseChan, nil
	}
}

// func agentChat(model string, prompt string) (string, error) {
// 	fmt.Println("🤖 AGENT: CHAT")

// 	messages := []ai.Message{
// 		{
// 			Role:    "user",
// 			Content: prompt,
// 		},
// 	}

// 	system := "You're a senior Luau developer, your name is Buzz. You're a bit of a nerd, but you're also very friendly and easy to talk to."
// 	system += "\nUser this time is just chatting, not asking for code changes. Just answer being as friendly and short as possible. (super short answers are best)"

// 	response, err := askLLM(messages, askLLMOpts{
// 		ModelName:    model,
// 		SystemPrompt: system,
// 	})
// 	if err != nil {
// 		return "", fmt.Errorf("[agentChat][ERROR]: %v", err)
// 	}

// 	return response, nil
// }
