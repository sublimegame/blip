package main

import (
	"fmt"
	"os"

	"github.com/spf13/cobra"

	"cu.bzh/ai"
	"cu.bzh/ai/gemini"
)

var rootCmd = &cobra.Command{
	Use:   "gemini-cli",
	Short: "A CLI tool for interacting with Google's Gemini AI models",
}

var listModelsCmd = &cobra.Command{
	Use:   "ls",
	Short: "List available Gemini models",
	Run: func(cmd *cobra.Command, args []string) {
		// load gemini API key
		apiKey := os.Getenv(gemini.ENVAR_API_KEY)
		if apiKey == "" {
			fmt.Printf("Error: %s environment variable is not set\n", gemini.ENVAR_API_KEY)
			os.Exit(1)
		}

		models, err := gemini.GetModels(apiKey)
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
	Short: "Generate text using a Gemini model",
	Run: func(cmd *cobra.Command, args []string) {
		// load gemini API key
		apiKey := os.Getenv(gemini.ENVAR_API_KEY)
		if apiKey == "" {
			fmt.Printf("Error: %s environment variable is not set\n", gemini.ENVAR_API_KEY)
			os.Exit(1)
		}

		modelName, _ := cmd.Flags().GetString("model")
		prompt, _ := cmd.Flags().GetString("prompt")
		systemPrompt, _ := cmd.Flags().GetString("system")

		modelName = convertModelNameToModelID(modelName)
		if modelName == "" {
			fmt.Printf("Error: unsupported model: %s\n", modelName)
			os.Exit(1)
		}

		if prompt == "" {
			fmt.Println("Error: prompt is required for text generation")
			os.Exit(1)
		}

		model := gemini.NewModel(modelName)
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

var generateStreamCmd = &cobra.Command{
	Use:   "generate-stream",
	Short: "Generate text using a Gemini model",
	Run: func(cmd *cobra.Command, args []string) {
		// load gemini API key
		apiKey := os.Getenv(gemini.ENVAR_API_KEY)
		if apiKey == "" {
			fmt.Printf("Error: %s environment variable is not set\n", gemini.ENVAR_API_KEY)
			os.Exit(1)
		}

		modelName, _ := cmd.Flags().GetString("model")
		prompt, _ := cmd.Flags().GetString("prompt")
		systemPrompt, _ := cmd.Flags().GetString("system")

		modelName = convertModelNameToModelID(modelName)
		if modelName == "" {
			fmt.Printf("Error: unsupported model: %s\n", modelName)
			os.Exit(1)
		}

		if prompt == "" {
			fmt.Println("Error: prompt is required for text generation")
			os.Exit(1)
		}

		model := gemini.NewModel(modelName)
		if model == nil {
			fmt.Printf("Error: unsupported model: %s\n", modelName)
			os.Exit(1)
		}

		opts := ai.GenerateOpts{
			APIKey:       apiKey,
			SystemPrompt: ai.NewSystemPromptFromText(systemPrompt),
		}

		messages := []ai.Message{
			{
				Role:    ai.ROLE_USER,
				Content: prompt,
			},
		}

		responseChan, err := model.GenerateStream(messages, opts)
		if err != nil {
			fmt.Printf("Error generating text: %v\n", err)
			os.Exit(1)
		}

		for response := range responseChan {
			fmt.Printf("%s", response)
		}
	},
}

func init() {
	generateCmd.Flags().StringP("model", "m", gemini.MODEL_GEMINI_2_0_FLASH_LITE, "Model to use for generation")
	generateCmd.Flags().StringP("prompt", "p", "", "Prompt to send to the model")
	generateCmd.Flags().StringP("system", "s", "", "System prompt to use (optional)")
	generateCmd.MarkFlagRequired("prompt")

	generateStreamCmd.Flags().StringP("model", "m", gemini.MODEL_GEMINI_2_0_FLASH_LITE, "Model to use for generation")
	generateStreamCmd.Flags().StringP("prompt", "p", "", "Prompt to send to the model")
	generateStreamCmd.Flags().StringP("system", "s", "", "System prompt to use (optional)")
	generateStreamCmd.MarkFlagRequired("prompt")

	// add commands to root command
	rootCmd.AddCommand(listModelsCmd)
	rootCmd.AddCommand(generateCmd)
	rootCmd.AddCommand(generateStreamCmd)
}

func main() {
	if err := rootCmd.Execute(); err != nil {
		fmt.Println(err)
		os.Exit(1)
	}
}

func convertModelNameToModelID(modelName string) string {
	switch modelName {
	case "gemini25":
	case "25":
	case gemini.MODEL_GEMINI_2_5_PRO_PAID:
		return gemini.MODEL_GEMINI_2_5_PRO_PAID
	case "gemini25exp":
	case "25exp":
	case gemini.MODEL_GEMINI_2_5_PRO_EXPERIMENTAL:
		return gemini.MODEL_GEMINI_2_5_PRO_EXPERIMENTAL
	case "gemini20flash":
	case "20flash":
	case "20":
	case gemini.MODEL_GEMINI_2_0_FLASH:
		return gemini.MODEL_GEMINI_2_0_FLASH
	case "gemini20flashlite":
	case "20flashlite":
	case "20lite":
	case gemini.MODEL_GEMINI_2_0_FLASH_LITE:
		return gemini.MODEL_GEMINI_2_0_FLASH_LITE
	}
	return ""
}
