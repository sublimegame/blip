package main

import (
	"fmt"
	"strings"

	"cu.bzh/ai"
)

type RequestType struct {
	Type string `json:"type"`
}

const (
	REQUEST_TYPE_CODE = "code"
	REQUEST_TYPE_CHAT = "chat"
)

func agentRequestTypeAnalyzer(model, apiKey, prompt string) (RequestType, error) {
	fmt.Println("[🤖][agentRequestTypeAnalyzer]")

	messages := []ai.Message{
		{
			Role:    ai.ROLE_USER,
			Content: prompt,
		},
		{
			Role:    ai.ROLE_ASSISTANT,
			Content: "The user request is of type:",
		},
	}

	// Version 1
	// system := "User is in a game editor. User is most likely asking for help making changes to the game but it could also be simple chit chat."
	// system += "\nYou must answer with a JSON describing the type of request in that format: { \"type\": \"code\" | \"ambience\" | \"ambience-with-possible-skybox-generation\" | \"chat\" }."
	// system += "\n\"code\" means the user is asking for help with anything that involves game mechanics, game logic, etc."
	// system += "\n\"ambience\" means the user is asking for help with the visual ambience of the game (lights, sky colors, skybox, weather, etc.)"
	// system += "\n\"ambience-with-possible-skybox-generation\" means user may want to generate a new skybox for the ambience."
	// system += "\n\"chat\" means the user is just chatting, expecting a friendly response, not changing anything in the game."

	// Version 2
	// system := "User is in a game editor. User is most likely asking for help making changes to the game but it could also be simple chit chat."
	// system += "\nYou must answer with a JSON describing the type of request in that format: { \"type\": \"code\" | \"chat\" }."
	// system += "\n\"code\" means the user is asking for help with anything that involves game mechanics, game logic, etc."
	// system += "\n\"chat\" means the user is just chatting, expecting a friendly response, not changing anything in the game."

	// Version 3
	systemPrompt := ai.NewSystemPrompt()
	systemPrompt.Append(ai.NewTextPart("User is creating a video game. User is likely asking for help to make changes to the game but it can also be simple chit chat."))
	systemPrompt.Append(ai.NewTextPart("Your assignement is to decide whether the user is asking for help to make changes to the game or just chatting."))
	systemPrompt.Append(ai.NewTextPart(`You must reply with "code" or "chat". There is no other answer possible.`))

	response, err := askLLM(messages, askLLMOpts{
		ModelName:    model,
		SystemPrompt: systemPrompt,
		apiKey:       apiKey,
		MaxTokens:    100,
	})
	if err != nil {
		fmt.Printf("[❌][agentRequestTypeAnalyzer][ERROR]: %v\n", err)
		return RequestType{}, err
	}

	fmt.Printf("[🤖][agentRequestTypeAnalyzer][RESPONSE]: %s\n", response)

	// lowercase response
	response = strings.ToLower(response)

	containsCode := strings.Contains(response, REQUEST_TYPE_CODE)
	containsChat := strings.Contains(response, REQUEST_TYPE_CHAT)

	if containsCode && containsChat {
		return RequestType{}, fmt.Errorf("response contains both code and chat")
	}

	if !containsCode && !containsChat {
		return RequestType{}, fmt.Errorf("response does not contain code or chat")
	}

	// containsCode and containsChat don't have the same value

	var result string
	if containsCode {
		result = REQUEST_TYPE_CODE
	} else {
		result = REQUEST_TYPE_CHAT
	}

	fmt.Printf("[🤖][agentRequestTypeAnalyzer][RESULT]: %s\n", result)

	return RequestType{
		Type: result,
	}, nil
}
