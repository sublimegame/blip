package main

import (
	"fmt"
	"os"

	"github.com/spf13/cobra"

	"cu.bzh/ai"
	"cu.bzh/ai/anthropic"
)

var rootCmd = &cobra.Command{
	Use:   "anthropic-cli",
	Short: "A CLI tool for interacting with Anthropic's AI models",
}

var listModelsCmd = &cobra.Command{
	Use:   "ls",
	Short: "List available Anthropic models",
	Run: func(cmd *cobra.Command, args []string) {
		// load anthropic API key
		apiKey := os.Getenv(anthropic.ENVAR_API_KEY)
		if apiKey == "" {
			fmt.Printf("Error: %s environment variable is not set\n", anthropic.ENVAR_API_KEY)
			os.Exit(1)
		}

		models, err := anthropic.GetModels(apiKey)
		if err != nil {
			fmt.Printf("Error listing models: %v\n", err)
			os.Exit(1)
		}

		fmt.Println("Available models:")
		for _, model := range models {
			fmt.Printf("- %s (%s)\n", model.DisplayName, model.ID)
		}
	},
}

var generateCmd = &cobra.Command{
	Use:   "generate",
	Short: "Generate text using an Anthropic model",
	Run: func(cmd *cobra.Command, args []string) {
		// load anthropic API key
		apiKey := os.Getenv(anthropic.ENVAR_API_KEY)
		if apiKey == "" {
			fmt.Printf("Error: %s environment variable is not set\n", anthropic.ENVAR_API_KEY)
			os.Exit(1)
		}

		modelName, _ := cmd.Flags().GetString("model")
		prompt, _ := cmd.Flags().GetString("prompt")
		systemPrompt, _ := cmd.Flags().GetString("system")

		modelID, err := convertModelNameToModelID(modelName)
		if err != nil {
			fmt.Printf("Error: unsupported model: %s\n", modelName)
			os.Exit(1)
		}

		if prompt == "" {
			fmt.Println("Error: prompt is required for text generation")
			os.Exit(1)
		}

		model := anthropic.NewModel(modelID)
		if model == nil {
			fmt.Printf("Error: unsupported model: %s\n", modelName)
			os.Exit(1)
		}

		opts := ai.GenerateOpts{
			APIKey:       apiKey,
			SystemPrompt: ai.NewSystemPromptFromText(systemPrompt),
		}

		response, err := model.GenerateSimple(prompt, opts)
		if err != nil {
			fmt.Printf("Error generating text: %v\n", err)
			os.Exit(1)
		}

		fmt.Println(response)
	},
}

func init() {
	generateCmd.Flags().StringP("model", "m", anthropic.MODEL_CLAUDE_3_5_HAIKU, "Model to use for generation")
	generateCmd.Flags().StringP("prompt", "p", "", "Prompt to send to the model")
	generateCmd.Flags().StringP("system", "s", "", "System prompt to use (optional)")
	generateCmd.MarkFlagRequired("prompt")

	// add commands to root command
	rootCmd.AddCommand(listModelsCmd)
	rootCmd.AddCommand(generateCmd)
}

func main() {
	if err := rootCmd.Execute(); err != nil {
		fmt.Println(err)
		os.Exit(1)
	}
}

// convertModelNameToModelID converts a model "short name" to a valid model ID.
func convertModelNameToModelID(modelName string) (string, error) {
	switch modelName {
	case "claude-3-5-haiku":
	case "claude-haiku":
	case "haiku":
		return anthropic.MODEL_CLAUDE_3_5_HAIKU, nil
	case "claude-3-5-sonnet":
	case "claude-3-5":
	case "claude35":
		return anthropic.MODEL_CLAUDE_3_5_SONNET, nil
	case "claude-3-7-sonnet":
	case "claude-3-7":
	case "claude37":
		return anthropic.MODEL_CLAUDE_3_7_SONNET, nil
	}
	return "", fmt.Errorf("unsupported model: %s", modelName)
}
