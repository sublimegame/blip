package main

import (
	"bytes"
	"context"
	"encoding/json"
	"fmt"
	"io"
	"log"
	"net/http"
	"net/url"
	"os"
	"strings"
	"time"

	"github.com/go-chi/chi/v5"
	chiMiddleware "github.com/go-chi/chi/v5/middleware"

	vxMiddleware "blip.game/middleware"
	"cu.bzh/ai"
	"cu.bzh/ai/anthropic"

	ollama "github.com/ollama/ollama/api"
)

const (
	// names of env variables
	ENVAR_DOCS_REFERENCE_DIR     = "CODEGEN_DOCS_REFERENCE_DIR"  // path to the directory containing the docs reference
	ENVAR_SAMPLE_SCRIPTS_FILE    = "CODEGEN_SAMPLE_SCRIPTS_FILE" // path to the file containing the sample scripts
	ENVAR_CHROMA_DB_HOST         = "CODEGEN_CHROMADB_HOST"       // host of the chromadb server
	ENVAR_CHROMA_DB_PORT         = "CODEGEN_CHROMADB_PORT"       // port of the chromadb server
	ENVAR_ANTHROPIC_API_KEY      = "ANTHROPIC_API_KEY"           // api key for the anthropic api
	ENVAR_ANTHROPIC_API_KEY_FILE = "ANTHROPIC_API_KEY_FILE"      // file containing the anthropic api key
	ENVAR_GEMINI_API_KEY         = "GEMINI_API_KEY"              // api key for the gemini api
	ENVAR_GEMINI_API_KEY_FILE    = "GEMINI_API_KEY_FILE"         // file containing the gemini api key
	ENVAR_GROK_API_KEY           = "GROK_API_KEY"                // api key for the grok api
	ENVAR_GROK_API_KEY_FILE      = "GROK_API_KEY_FILE"           // file containing the grok api key
	ENVAR_OLLAMA_EMBED_URL       = "CODEGEN_OLLAMA_EMBED_URL"    // url of the ollama server

	// Models
	MODEL_NAME_LOCAL     = "local"
	MODEL_EMBEDDING_NAME = "mxbai-embed-large"
	MODEL_DEFAULT        = anthropic.MODEL_CLAUDE_3_7_SONNET

	// default values
	DEFAULT_DOCS_REFERENCE_DIR         = "./docs/content/reference"
	DEFAULT_SAMPLE_SCRIPTS_FILE        = "../../../../cubzh/lua/docs/content/sample-scripts.yml"
	DEFAULT_CHROMA_DB_SCHEME           = "http"
	DEFAULT_CHROMA_DB_HOST             = "codegen-chromadb"
	DEFAULT_CHROMA_DB_PORT             = "8000"
	FULL_REFERENCE_OUTPUT_FILE         = "./reference.txt"
	HTTP_LISTEN_PORT                   = 80
	DEBUG                              = true
	CHROMA_DB_TENANT                   = "buzz"
	CHROMA_DB_DATABASE                 = "buzz"
	CHAT_MARKER                        = "\n>>>>>>>CHAT\n"
	SCRIPT_MARKER                      = "\n>>>>>>>SCRIPT\n"
	REFERENCE_MARKDOWN_OUTPUT_DIR      = "./reference"
	SAMPLE_SCRIPTS_MARKDOWN_OUTPUT_DIR = "./sample_scripts"
)

var (
	docs_reference_dir  = DEFAULT_DOCS_REFERENCE_DIR
	sample_scripts_file = DEFAULT_SAMPLE_SCRIPTS_FILE
	chromadb_scheme     = DEFAULT_CHROMA_DB_SCHEME
	chromadb_host       = DEFAULT_CHROMA_DB_HOST
	chromadb_port       = DEFAULT_CHROMA_DB_PORT
	chromadb_url        = fmt.Sprintf("%s://%s:%s", chromadb_scheme, chromadb_host, chromadb_port)
	anthropic_api_key   = ""
	gemini_api_key      = ""
	grok_api_key        = ""
	ollama_embed_url    = ""
	//
	REFERENCE = ""
)

type PromptRequest struct {
	Model  string `json:"model"`
	Prompt string `json:"prompt"`
	Script string `json:"script"`
	Scene  string `json:"scene"` // JSON description of the world
}

type Document struct {
	ID       string `json:"id"`
	Markdown string `json:"markdown"`
	Summary  string `json:"summary"`
	Path     string `json:"path"`
}

func main() {
	{
		// Retrieve env variables values
		envar_docs_reference_dir := os.Getenv(ENVAR_DOCS_REFERENCE_DIR)
		if envar_docs_reference_dir != "" {
			fmt.Printf("🤖 %s: %s\n", ENVAR_DOCS_REFERENCE_DIR, envar_docs_reference_dir)
			docs_reference_dir = envar_docs_reference_dir
		}
		envar_sample_scripts_file := os.Getenv(ENVAR_SAMPLE_SCRIPTS_FILE)
		if envar_sample_scripts_file != "" {
			fmt.Printf("🤖 %s: %s\n", ENVAR_SAMPLE_SCRIPTS_FILE, envar_sample_scripts_file)
			sample_scripts_file = envar_sample_scripts_file
		}
		envar_chromadb_host := os.Getenv(ENVAR_CHROMA_DB_HOST)
		if envar_chromadb_host != "" {
			fmt.Printf("🤖 %s: %s\n", ENVAR_CHROMA_DB_HOST, envar_chromadb_host)
			chromadb_host = envar_chromadb_host
		}
		envar_chromadb_port := os.Getenv(ENVAR_CHROMA_DB_PORT)
		if envar_chromadb_port != "" {
			fmt.Printf("🤖 %s: %s\n", ENVAR_CHROMA_DB_PORT, envar_chromadb_port)
			chromadb_port = envar_chromadb_port
		}

		// construct the chromadb connection url
		chromadb_url = fmt.Sprintf("%s://%s:%s", chromadb_scheme, chromadb_host, chromadb_port)

		// ollama
		ollama_embed_url = os.Getenv(ENVAR_OLLAMA_EMBED_URL)

		// Anthropic API key
		anthropic_api_key = os.Getenv(ENVAR_ANTHROPIC_API_KEY)
		anthropic_api_key_file := os.Getenv(ENVAR_ANTHROPIC_API_KEY_FILE)
		// if no key is set, but the file path is set, try to read the file
		if anthropic_api_key == "" && anthropic_api_key_file != "" {
			fileContent, err := os.ReadFile(anthropic_api_key_file)
			if err != nil {
				fmt.Println("❌ error reading anthropic api key file:", err.Error())
				return
			}
			// remove trailing newlines
			anthropic_api_key = strings.TrimSpace(string(fileContent))
		}

		// Gemini API key
		gemini_api_key = os.Getenv(ENVAR_GEMINI_API_KEY)
		gemini_api_key_file := os.Getenv(ENVAR_GEMINI_API_KEY_FILE)
		// if no key is set, but the file path is set, try to read the file
		if gemini_api_key == "" && gemini_api_key_file != "" {
			fileContent, err := os.ReadFile(gemini_api_key_file)
			if err != nil {
				fmt.Println("❌ error reading gemini api key file:", err.Error())
				return
			}
			// remove trailing newlines
			gemini_api_key = strings.TrimSpace(string(fileContent))
		}

		// Grok API key
		grok_api_key = os.Getenv(ENVAR_GROK_API_KEY)
		grok_api_key_file := os.Getenv(ENVAR_GROK_API_KEY_FILE)
		// if no key is set, but the file path is set, try to read the file
		if grok_api_key == "" && grok_api_key_file != "" {
			fileContent, err := os.ReadFile(grok_api_key_file)
			if err != nil {
				fmt.Println("❌ error reading grok api key file:", err.Error())
				return
			}
			// remove trailing newlines
			grok_api_key = strings.TrimSpace(string(fileContent))
		}
	}

	fmt.Println("🤖 STARTING BUZZ :")
	fmt.Println("  - docs_reference_dir:", docs_reference_dir)
	fmt.Println("  - chromadb_url:", chromadb_url)
	fmt.Println("  - ollama_embed_url:", ollama_embed_url)

	chromaClient, err := NewChromaClient(chromadb_url, CHROMA_DB_TENANT, CHROMA_DB_DATABASE)
	if err != nil {
		fmt.Println("❌ error creating chroma client:", err.Error())
		return
	}

	err = chromaClient.Check()
	if err != nil {
		fmt.Println("❌ error checking chroma client:", err.Error())
		return
	}

	client, err := getOllamaClient()
	if err != nil {
		fmt.Printf("Error creating ollama client: %v\n", err)
		return
	}

	if len(os.Args) > 1 && os.Args[1] == "--embed" {
		start := time.Now()
		chromaClient.RemoveAllCollections()
		convertReferenceIntoEmbeddingDocuments()
		convertSampleScriptsIntoEmbeddingDocuments()
		err = storeEmbeddings(client, chromaClient)
		if err != nil {
			fmt.Println("❌ error storing embeddings:", err.Error())
		}
		elapsed := time.Since(start)
		hours := int(elapsed.Hours())
		minutes := int(elapsed.Minutes()) % 60
		seconds := int(elapsed.Seconds()) % 60
		fmt.Printf("Total embedding time: %dh%dm%ds\n", hours, minutes, seconds)
		return
	}

	// Check if --serve flag is provided
	if len(os.Args) > 1 && os.Args[1] == "--serve" {
		startHTTPServer()
		return
	}

	// // Read reference file if it exists
	// if _, err := os.Stat(FULL_REFERENCE_OUTPUT_FILE); err == nil {
	// 	content, err := os.ReadFile(FULL_REFERENCE_OUTPUT_FILE)
	// 	if err != nil {
	// 		fmt.Println("❌ error reading reference file:", err.Error())
	// 		return
	// 	}
	// 	REFERENCE = string(content)
	// }

	// // fmt.Printf("Reference size: %d bytes\n", len(REFERENCE))

	// reader := bufio.NewReader(os.Stdin)
	// fmt.Print("Enter your prompt: ")
	// prompt, err := reader.ReadString('\n')
	// if err != nil {
	// 	fmt.Println("❌ error reading input:", err.Error())
	// 	return
	// }
	// prompt = strings.TrimSpace(prompt)

	// requestType, err := agentRequestTypeAnalyzer(LLM_TO_USE, prompt)
	// if err != nil {
	// 	fmt.Println("❌ error analyzing request type:", err.Error())
	// 	return
	// }
	// fmt.Printf("Request type: %s\n", requestType.Type)

	// req := PromptRequest{
	// 	Prompt: prompt,
	// 	Script: "",
	// 	Scene:  "",
	// }

	// var result string
	// if requestType.Type == "code" {
	// 	result, err = agentCodeGenerator(req, chromaClient)
	// } else if requestType.Type == "chat" {
	// 	result, err = agentChat(prompt)
	// }
	// if err != nil {
	// 	fmt.Println("❌ error generating result:", err.Error())
	// 	return
	// }

	// fmt.Println(result)
}

func convertMessagesToOllamaMessages(aiMessages []ai.Message) []ollama.Message {
	ollamaMessages := []ollama.Message{}
	for _, message := range aiMessages {
		ollamaRole := ""
		if message.Role == ai.ROLE_USER {
			ollamaRole = "user"
		} else if message.Role == ai.ROLE_ASSISTANT {
			ollamaRole = "system"
		} else {
			log.Fatalln("unsupported message role")
		}
		msg := ollama.Message{
			Role:    ollamaRole,
			Content: message.Content,
		}
		ollamaMessages = append(ollamaMessages, msg)
	}
	return ollamaMessages
}

type askLLMOpts struct {
	ModelName    string
	MaxTokens    int
	SystemPrompt *ai.SystemPrompt
	apiKey       string
}

func askLLM(messages []ai.Message, opts askLLMOpts) (string, error) {

	// encoding, err := tiktoken.GetEncoding("cl100k_base")
	// if err != nil {
	// 	log.Fatalf("Error getting encoding: %v", err)
	// }
	// tokens := encoding.Encode(opts.SystemPrompt, nil, nil)
	// tokenCount := len(tokens)
	// fmt.Printf("system token count: %d\n", tokenCount)

	// LEGACY (needs to be removed later)
	if opts.ModelName == "" {
		opts.ModelName = MODEL_DEFAULT
	}

	// check if we should use local LLM (ollama + llama3.2:3b)
	if opts.ModelName == "local" {
		client, err := getOllamaClient()
		if err != nil {
			return "", fmt.Errorf("error creating ollama client: %v", err)
		}

		stream := false

		ollamaMessages := []ollama.Message{
			{
				Role:    "system",
				Content: opts.SystemPrompt.String(),
			},
		}

		for _, message := range messages {
			ollamaMessages = append(ollamaMessages, ollama.Message{
				Role:    string(message.Role),
				Content: message.Content,
			})
		}

		// opts := ollama.DefaultOptions()
		// opts.Temperature = 0.0

		cReq := &ollama.ChatRequest{
			// Model: "llama3",
			Model: "llama3.2:3b",
			// Model:  "qwen2.5-coder",
			// Model:    "deepseek-r1:1.5b",
			// Model: "deepseek-r1:7b",
			// Model:    "mistral:v0.3",
			Messages: ollamaMessages,
			// Template: "",
			Stream:  &stream,
			Options: map[string]interface{}{"temperature": 0.0},
		}
		var response string
		err = client.Chat(context.Background(), cReq, func(r ollama.ChatResponse) error {
			fmt.Printf("%s", r.Message.Content)
			if r.Done {
				response = r.Message.Content
			}
			return nil
		})
		if err != nil {
			return "", fmt.Errorf("error generating: %v", err)
		}
		return response, nil
	}

	// not using local LLM

	model, err := getModelForModelName(opts.ModelName)
	if err != nil {
		return "", fmt.Errorf("unsupported model: %s", opts.ModelName)
	}

	response, err := model.Generate(messages, ai.GenerateOpts{
		APIKey:       opts.apiKey,
		SystemPrompt: opts.SystemPrompt,
		MaxTokens:    opts.MaxTokens,
	})

	return response, err
}

// askLLMWithStreamedResponse returns a channel that streams the LLM response chunks as they arrive
func askLLMWithStreamedResponse(messages []ai.Message, opts askLLMOpts) (chan string, error) {

	// LEGACY (needs to be removed later)
	if opts.ModelName == "" {
		opts.ModelName = MODEL_DEFAULT
	}

	model, err := getModelForModelName(opts.ModelName)
	if err != nil {
		return nil, fmt.Errorf("unsupported model: %s", opts.ModelName)
	}

	responseChan, err := model.GenerateStream(messages, ai.GenerateOpts{
		APIKey:       opts.apiKey,
		SystemPrompt: opts.SystemPrompt,
		MaxTokens:    opts.MaxTokens,
	})
	if err != nil {
		return nil, fmt.Errorf("error generating stream: %v", err)
	}

	return responseChan, nil
}

func startHTTPServer() {
	router := chi.NewRouter()
	router.Use(chiMiddleware.Logger)
	router.Use(chiMiddleware.Recoverer)
	router.Use(vxMiddleware.ReadUserCredentialsMiddleware)

	// Routes
	// router.Post("/", handlePrompt)
	router.Post("/", handlePromptStream)
	router.Post("/diff", handlePromptDiff)
	// router.Get("/test", handleTest)

	// Handle 404
	router.NotFound(func(w http.ResponseWriter, r *http.Request) {
		w.Header().Set("Content-Type", "application/json")
		w.WriteHeader(http.StatusNotFound)
		w.Write([]byte(`{"error": "not found"}`))
	})

	addr := fmt.Sprintf(":%d", HTTP_LISTEN_PORT)
	fmt.Printf("✨ Starting server on %s\n", addr)
	if err := http.ListenAndServe(addr, router); err != nil {
		fmt.Printf("Server error: %v\n", err)
	}
}

// func handleTest(w http.ResponseWriter, r *http.Request) {
// 	fmt.Println("🤖 HANDLE HTTP TEST")

// 	// send request to API
// 	// request, err := http.NewRequest("GET", "https://api.cu.bzh/ping", nil)
// 	request, err := http.NewRequest("GET", "http://hub-http/transactions/payment-request", nil)
// 	if err != nil {
// 		fmt.Println("❌ error creating request:", err.Error())
// 		http.Error(w, "internal server error", http.StatusInternalServerError)
// 		return
// 	}

// 	response, err := http.DefaultClient.Do(request)
// 	if err != nil {
// 		fmt.Println("❌ error sending request:", err.Error())
// 		http.Error(w, "internal server error", http.StatusInternalServerError)
// 		return
// 	}

// 	defer response.Body.Close()

// 	body, err := io.ReadAll(response.Body)
// 	if err != nil {
// 		fmt.Println("❌ error reading response body:", err.Error())
// 		http.Error(w, "internal server error", http.StatusInternalServerError)
// 		return
// 	}

// 	w.WriteHeader(http.StatusOK)
// 	w.Write(body)
// }

func handlePromptStream(w http.ResponseWriter, r *http.Request) {

	fmt.Println("🤖 HANDLE HTTP PROMPT (STREAM)")

	// Get user credentials
	userCredentials := vxMiddleware.GetUserCredentialsFromRequestContext(r.Context())
	if userCredentials == nil {

		// missing user credentials
		fmt.Println("[❌][handlePromptStream] no user credentials")
		http.Error(w, "unauthorized", http.StatusUnauthorized)
		return

	} else {

		// Consume blux virtual currency before generating anything
		fmt.Println("🤖 CONSUMING BLUX VIRTUAL CURRENCY for user:", userCredentials.UserID)

		type postTransactionsPaymentRequestReq struct {
			UserID          string `json:"userID"`
			AmountMillionth int64  `json:"amountMillionth"`
		}

		paymentRequest := postTransactionsPaymentRequestReq{
			UserID:          userCredentials.UserID,
			AmountMillionth: 1000000, // 1 coin
		}

		jsonData, err := json.Marshal(paymentRequest)
		if err != nil {
			fmt.Println("❌ error marshalling payment request:", err.Error())
			http.Error(w, "internal server error", http.StatusInternalServerError)
			return
		}

		// send request to API
		request, err := http.NewRequest("POST", "http://hub-http/transactions/payment-request", bytes.NewBuffer(jsonData))
		if err != nil {
			fmt.Println("❌ error creating request:", err.Error())
			http.Error(w, "internal server error", http.StatusInternalServerError)
			return
		}

		response, err := http.DefaultClient.Do(request)
		if err != nil {
			fmt.Println("❌ error sending request:", err.Error())
			http.Error(w, "internal server error", http.StatusInternalServerError)
			return
		}
		defer response.Body.Close()

		body, err := io.ReadAll(response.Body)
		if err != nil {
			fmt.Println("❌ error reading response body:", err.Error())
			http.Error(w, "internal server error", http.StatusInternalServerError)
			return
		}

		// check if the response is ok
		if response.StatusCode != http.StatusOK {
			fmt.Println("❌ error response:", string(body))
			http.Error(w, "internal server error", http.StatusInternalServerError)
			return
		}

		// all good, continue
	}

	chromaClient, err := NewChromaClient(chromadb_url, CHROMA_DB_TENANT, CHROMA_DB_DATABASE)
	if err != nil {
		fmt.Println("[❌][handlePromptStream] creating chroma client:", err.Error())
		http.Error(w, "internal server error", http.StatusInternalServerError)
		return
	}

	var req PromptRequest
	if err := json.NewDecoder(r.Body).Decode(&req); err != nil {
		fmt.Println("[❌][handlePromptStream] decoding request body:", err.Error())
		http.Error(w, "Invalid request body", http.StatusBadRequest)
		return
	}
	defer r.Body.Close()

	// Get API key for the model
	apiKey, err := getApiKeyForModelName(req.Model)
	if err != nil {
		fmt.Println("❌ Error getting API key:", err.Error())
		http.Error(w, "Invalid request body", http.StatusBadRequest)
		return
	}

	// Model to use for request type analysis
	// ------------------------------------------
	// We always use the same model for request type analysis
	requestTypeAnalysisModel := anthropic.MODEL_CLAUDE_3_5_HAIKU
	requestTypeAnalysisApiKey, err := getApiKeyForModelName(requestTypeAnalysisModel)
	if err != nil {
		fmt.Println("❌ Error getting API key for request type analysis model:", err.Error())
		http.Error(w, "Invalid model for request type analysis", http.StatusInternalServerError)
		return
	}

	requestType, err := agentRequestTypeAnalyzer(
		requestTypeAnalysisModel,
		requestTypeAnalysisApiKey,
		req.Prompt,
	)
	if err != nil {
		fmt.Printf("Error: %v\n", err)
		return
	}
	fmt.Printf("Request type: %s\n", requestType.Type)

	// Set up SSE headers
	w.Header().Set("Content-Type", "text/event-stream")
	w.Header().Set("Cache-Control", "no-cache")
	w.Header().Set("Connection", "keep-alive")
	w.Header().Set("Access-Control-Allow-Origin", "*")
	w.WriteHeader(http.StatusOK)

	flusher, ok := w.(http.Flusher)
	if !ok {
		http.Error(w, "Streaming not supported", http.StatusInternalServerError)
		return
	}

	if requestType.Type == "code" {
		fmt.Println("🤖 RECEIVED CODE PROMPT")
		fmt.Fprintf(w, "%s", CHAT_MARKER)
		fmt.Fprintf(w, "Ok, I'll code something for you! Hold on...")
		flusher.Flush()

		fmt.Fprintf(w, "%s", SCRIPT_MARKER)

		// For code generation, use the streaming version
		responseChan, processErr := agentCodeGeneratorStream(req, chromaClient)
		if processErr != nil {
			fmt.Println("[❌][handlePromptStream] Error generating code:", processErr.Error())
			return
		}

		// Stream the response chunks as they come
		for chunk := range responseChan {
			if len(chunk) > 0 {
				fmt.Printf("%s", chunk)
				fmt.Fprintf(w, "%s", chunk)
				flusher.Flush()
			}
		}

	} else if requestType.Type == "chat" {
		fmt.Println("🤖 RECEIVED CHAT PROMPT")
		// For chat, use the streaming version
		responseChan, processErr := agentChatStream(req.Model, apiKey, req.Prompt)
		if processErr != nil {
			fmt.Println("[❌][handlePromptStream] generating chat:", processErr.Error())
			return
		}

		fmt.Println("🤖 READING responseChan...")

		fmt.Fprintf(w, "%s", CHAT_MARKER)

		// Stream the response chunks as they come
		for chunk := range responseChan {
			fmt.Println("CHUNK:", chunk)
			fmt.Fprintf(w, "%s", chunk)
			flusher.Flush()
		}

		fmt.Println("🤖 DONE READING responseChan.")
	}
}

func handlePromptDiff(w http.ResponseWriter, r *http.Request) {
	fmt.Println("🤖 HANDLE HTTP PROMPT DIFF: NOT IMPLEMENTED YET")
	http.Error(w, "Not implemented", http.StatusNotImplemented)
	return

	// if r.Method != http.MethodPost {
	// 	fmt.Println("❌ Method not allowed")
	// 	http.Error(w, "Method not allowed", http.StatusMethodNotAllowed)
	// 	return
	// }

	// chromaClient, err := NewChromaClient(chromadb_url, CHROMA_DB_TENANT, CHROMA_DB_DATABASE)
	// if err != nil {
	// 	fmt.Println("❌ Error creating chroma client:", err.Error())
	// 	http.Error(w, "internal server error", http.StatusInternalServerError)
	// 	return
	// }

	// var req PromptRequest
	// if err := json.NewDecoder(r.Body).Decode(&req); err != nil {
	// 	fmt.Println("❌ Error decoding request body:", err.Error())
	// 	http.Error(w, "Invalid request body", http.StatusBadRequest)
	// 	return
	// }
	// defer r.Body.Close()

	// if req.Model == "" {
	// 	req.Model = MODEL_DEFAULT
	// }

	// // Get API key for the model
	// apiKey, err := getApiKeyForModel(req.Model)
	// if err != nil {
	// 	fmt.Println("❌ Error getting API key:", err.Error())
	// 	http.Error(w, "Invalid request body", http.StatusBadRequest)
	// 	return
	// }

	// //
	// // FORCE CODE GENERATION
	// //

	// result, err := agentCodeGenerator(req, apiKey, chromaClient)
	// if err != nil {
	// 	fmt.Println("❌ Error generating code:", err.Error())
	// 	http.Error(w, "Invalid request body", http.StatusBadRequest)
	// 	return
	// }

	// fmt.Println("🤖🤖🤖 RESULT 🤖🤖🤖")
	// fmt.Println(result)

	// diffObj, err := diff.NewDiff(result, &req.Script)
	// if err != nil {
	// 	fmt.Println("❌ Failed to parse diff:", err.Error())
	// 	http.Error(w, "Invalid request body", http.StatusBadRequest)
	// 	return
	// }

	// sourceCodeLines := diff.ParseLines(req.Script)

	// err = diffObj.IsApplicableToCode(sourceCodeLines)
	// if err != nil {
	// 	fmt.Println("❌ Diff is not valid:", err.Error())
	// 	http.Error(w, "Invalid diff", http.StatusInternalServerError)
	// 	return
	// }

	// w.WriteHeader(http.StatusOK)
	// w.Write([]byte("{}"))
}

func getOllamaClient() (*ollama.Client, error) {
	if ollama_embed_url == "" {
		return ollama.ClientFromEnvironment()
	}

	URL, err := url.Parse(ollama_embed_url)
	if err != nil {
		return nil, err
	}
	client := ollama.NewClient(URL, http.DefaultClient)
	return client, nil
}
