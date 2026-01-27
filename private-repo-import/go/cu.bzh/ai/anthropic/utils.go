package anthropic

import (
	"errors"
	"log"

	"cu.bzh/ai"
)

func validateInputSimple(apiKey, userPrompt string) error {

	if apiKey == "" {
		return errors.New("API key is missing")
	}

	if userPrompt == "" {
		return errors.New("user prompt is missing")
	}

	return nil
}

func validateInput(apiKey string, messages []ai.Message) error {

	if apiKey == "" {
		return errors.New("API key is missing")
	}

	if len(messages) == 0 {
		return errors.New("messages are missing")
	}

	for _, message := range messages {
		if message.Role == "" {
			return errors.New("message role is missing")
		}
	}

	return nil
}

// convertAIMessageRoleToAnthropicRole ...
func convertAIMessageRoleToAnthropicRole(role ai.MessageRole) ai.MessageRole {
	if role == ai.ROLE_USER {
		return ROLE_USER
	} else if role == ai.ROLE_ASSISTANT {
		return ROLE_ASSISTANT
	}
	log.Fatalf("[convertAIMessageRoleToAnthropicRole] Unsupported message role: %s", role)
	return ""
}

// convertAIMessagesToAnthropicMessages ...
func convertAIMessagesToAnthropicMessages(msgs []ai.Message) []Message {
	anthropicMessages := make([]Message, len(msgs))
	for i, msg := range msgs {
		anthropicMessages[i] = Message{
			Role:    convertAIMessageRoleToAnthropicRole(msg.Role),
			Content: msg.Content,
		}
	}
	return anthropicMessages
}
