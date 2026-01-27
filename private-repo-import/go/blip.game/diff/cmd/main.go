package main

import (
	"bytes"
	"fmt"
	"io"
	"os"
	"strings"
	"time"

	"github.com/spf13/cobra"

	"blip.game/diff"
)

var (
	flagFile   string
	flagPrompt string
	flagDiff   bool
)

func main() {

	// root command
	var rootCmd = &cobra.Command{
		Use:   "codegen",
		Short: "A simple CLI application that codes",
		Run:   rootCommand,
	}
	rootCmd.Flags().StringVarP(&flagFile, "file", "f", "", "file to read from")
	rootCmd.Flags().StringVarP(&flagPrompt, "prompt", "p", "", "user prompt")
	rootCmd.Flags().BoolVarP(&flagDiff, "diff", "d", false, "diff mode")

	// debug command
	var debugCmd = &cobra.Command{
		Use:   "debug",
		Short: "A simple CLI application that codes",
		Run:   debugCommand,
	}
	rootCmd.AddCommand(debugCmd)

	// get the arguments
	if err := rootCmd.Execute(); err != nil {
		fmt.Println(err)
		os.Exit(1)
	}
}

// Root command

func rootCommand(cmd *cobra.Command, args []string) {
	// Add timer at the start
	startTime := time.Now()

	// Get the Anthropic API key
	apiKey := os.Getenv("ANTHROPIC_API_KEY")
	if apiKey == "" {
		fmt.Println("[❌] ANTHROPIC_API_KEY environment variable not set")
		os.Exit(1)
	}

	// Get the --file -f flag
	filePath, err := cmd.Flags().GetString("file")
	if err != nil {
		fmt.Printf("Error getting file flag: %v\n", err)
		os.Exit(1)
	}

	// Get the --prompt -p flag
	userPrompt, err := cmd.Flags().GetString("prompt")
	if err != nil {
		fmt.Printf("Error getting prompt flag: %v\n", err)
		os.Exit(1)
	}

	// Get the --diff -d flag
	diffMode, err := cmd.Flags().GetBool("diff")
	if err != nil {
		fmt.Printf("Error getting diff flag: %v\n", err)
		os.Exit(1)
	}

	// Read the file
	fileContent, err := os.ReadFile(filePath)
	if err != nil {
		fmt.Printf("Error reading file: %v\n", err)
		os.Exit(1)
	}
	fileContentStr := string(fileContent)

	// Send prompt to AI model

	messages := []Message{
		{
			Role:    "user",
			Content: userPrompt,
		},
	}

	reader, err := askLLMWithStreamedResponse(constructSystemPrompt(diffMode, fileContent), messages, apiKey)
	if err != nil {
		fmt.Printf("Error asking LLM: %v\n", err)
		os.Exit(1)
	}

	responseBuffer := bytes.Buffer{}

	if diffMode {
		diffDecoder := diff.NewDiffDecoder(reader, &fileContentStr)
		previousDiffText := ""
		for {
			more, err := diffDecoder.Decode()
			if err != nil {
				fmt.Printf("Error decoding diff: %v\n", err)
				os.Exit(1)
			}

			// Clear previous output by moving cursor up and erasing lines
			if len(previousDiffText) > 0 {
				lines := strings.Count(previousDiffText, "\n") + 1
				fmt.Printf("\033[%dA\033[J", lines) // Move up N lines and clear to end
			}
			diffText := diffDecoder.DecodedDiff().ToString()
			for _, line := range strings.Split(diffText, "\n") {
				if strings.HasPrefix(line, "+") {
					fmt.Printf("\033[32m%s\033[0m\n", line) // Green
				} else if strings.HasPrefix(line, "-") {
					fmt.Printf("\033[31m%s\033[0m\n", line) // Red
				} else if strings.HasPrefix(line, "@@") {
					fmt.Printf("\033[96m%s\033[0m\n", line) // Cyan
				} else {
					fmt.Printf("\033[90m%s\033[0m\n", line) // grey
				}
			}
			unprocessed := diffDecoder.UnprocessedLines()
			unprocessed = strings.TrimSpace(unprocessed)
			fmt.Printf("\033[90m%s\033[0m", unprocessed)
			previousDiffText = diffText + "\n" + unprocessed

			if !more {
				responseBuffer.WriteString(diffDecoder.DecodedDiff().ToString())
				break
			}

			print("\n")
		}
	} else {
		for {
			buffer := make([]byte, 1)
			n, err := reader.Read(buffer)
			if err != nil && err != io.EOF {
				fmt.Printf("Error reading LLM: %v\n", err)
				os.Exit(1)
			}
			if n > 0 {
				fmt.Print(string(buffer[:n]))
				responseBuffer.WriteString(string(buffer[:n]))
			}
			if err == io.EOF {
				break
			}
		}
		print("\n")
	}

	response := responseBuffer.String()
	// count the number of tokens in the response
	tokenCount := computeTokenCount(response)
	fmt.Printf("\033[38;5;208m\nOutput tokens: %d\033[0m\n", tokenCount)

	// Add elapsed time measurement at the end
	elapsed := time.Since(startTime)
	fmt.Printf("\033[38;5;208mExecution duration: %.2f seconds\033[0m\n", elapsed.Seconds())
}

// debug command
func debugCommand(cmd *cobra.Command, args []string) {}
