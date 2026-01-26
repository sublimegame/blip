package main

import (
	"fmt"
	"os"

	"github.com/spf13/cobra"

	"cu.bzh/ai"
	"cu.bzh/ai/grok"
)

const (
	SHORT_NAME_GROK_3_BETA      = "grok3"
	SHORT_NAME_GROK_3_MINI_BETA = "grok3-mini"
)

func convertModelNameToModelID(modelName string) string {
	switch modelName {
	case SHORT_NAME_GROK_3_BETA:
		return grok.MODEL_GROK_3_BETA
	case SHORT_NAME_GROK_3_MINI_BETA:
		return grok.MODEL_GROK_3_MINI_BETA
	}
	return ""
}

var rootCmd = &cobra.Command{
	Use:   "grok-cli",
	Short: "A CLI tool for interacting with Grok's AI models",
}

var listModelsCmd = &cobra.Command{
	Use:   "ls",
	Short: "List available Grok models",
	Run: func(cmd *cobra.Command, args []string) {
		// load grok API key
		apiKey := os.Getenv(grok.ENVAR_API_KEY)
		if apiKey == "" {
			fmt.Printf("Error: %s environment variable is not set\n", grok.ENVAR_API_KEY)
			os.Exit(1)
		}

		models, err := grok.GetModels(apiKey)
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
		// load grok API key
		apiKey := os.Getenv(grok.ENVAR_API_KEY)
		if apiKey == "" {
			fmt.Printf("Error: %s environment variable is not set\n", grok.ENVAR_API_KEY)
			os.Exit(1)
		}

		modelName, _ := cmd.Flags().GetString("model")
		prompt, _ := cmd.Flags().GetString("prompt")
		systemPrompt, _ := cmd.Flags().GetString("system")
		streaming, err := cmd.Flags().GetBool("streaming")
		if err != nil {
			fmt.Printf("Error: %v\n", err)
			os.Exit(1)
		}

		modelName = convertModelNameToModelID(modelName)
		if modelName == "" {
			fmt.Printf("Error: unsupported model: %s\n", modelName)
			os.Exit(1)
		}

		if prompt == "" {
			fmt.Println("Error: prompt is required for text generation")
			os.Exit(1)
		}

		model := grok.NewModel(modelName)
		if model == nil {
			fmt.Printf("Error: unsupported model: %s\n", modelName)
			os.Exit(1)
		}

		if streaming {

			messages := []ai.Message{
				{
					Role:    grok.ROLE_USER,
					Content: prompt,
				},
			}

			responseChan, err := model.GenerateStream(messages, ai.GenerateOpts{
				APIKey:       apiKey,
				SystemPrompt: ai.NewSystemPromptFromText(systemPrompt),
			})
			if err != nil {
				fmt.Printf("Error generating text: %v\n", err)
				os.Exit(1)
			}

			// consume the channel to avoid blocking
			for text := range responseChan {
				fmt.Print(text)
			}
		} else {
			response, err := model.GenerateSimple(prompt, ai.GenerateOpts{
				APIKey:       apiKey,
				SystemPrompt: ai.NewSystemPromptFromText(systemPrompt),
			})
			if err != nil {
				fmt.Printf("Error generating text: %v\n", err)
				os.Exit(1)
			}
			fmt.Println(response)
		}
	},
}

func init() {
	generateCmd.Flags().StringP("model", "m", SHORT_NAME_GROK_3_MINI_BETA, "Model to use for generation")
	generateCmd.Flags().StringP("prompt", "p", "", "Prompt to send to the model")
	generateCmd.Flags().StringP("system", "s", "", "System prompt to use (optional)")
	generateCmd.Flags().BoolP("streaming", "", false, "Enable response streaming")
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
