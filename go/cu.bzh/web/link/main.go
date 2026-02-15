package main

import (
	"encoding/json"
	"fmt"
	"log"
	"net/http"
	"net/url"
	"os"
	"sync"

	"github.com/go-chi/chi/v5"
	"github.com/go-chi/chi/v5/middleware"

	"cu.bzh/amplitude"
	httpUtils "cu.bzh/utils/http"
	"cu.bzh/web/link/types"
)

// --------------------
// Globals
// --------------------

const (
	// Environment variable names
	ENV_VAR_CONFIG_FILEPATH = "CONFIG_FILEPATH"
	ENV_VAR_LISTEN_ADDR     = "LISTEN_ADDR"

	// Default values
	DEFAULT_CONFIG_FILEPATH = "/voxowl/link/config.json"
	DEFAULT_LISTEN_ADDR     = ":80"
)

var (
	// Runtime configuration
	configFilepath = DEFAULT_CONFIG_FILEPATH
	listenAddr     = DEFAULT_LISTEN_ADDR
)

// --------------------
// Globals
// --------------------

var (
	config     *types.Config = types.NewConfig()
	configLock sync.RWMutex
)

// --------------------
// Functions
// --------------------

func main() {
	// load config file path from environment variable
	if value := os.Getenv(ENV_VAR_CONFIG_FILEPATH); value != "" {
		configFilepath = value
	}

	// load listen address from environment variable
	if value := os.Getenv(ENV_VAR_LISTEN_ADDR); value != "" {
		listenAddr = value
	}

	fmt.Println("✨ link service starting...")

	// Read config file containing the redirect rules
	fmt.Println("📜 load configuration from file...")
	err := loadConfig()
	if err != nil {
		fmt.Println("❌ ERROR: failed to load config (" + err.Error() + ")")
		os.Exit(1)
	}

	// Define HTTP router
	router := chi.NewRouter()
	router.Use(middleware.Logger)
	router.Get("/", index)
	// redirect (GET)
	router.Get("/{hash}", redirectHash)
	// config (GET)
	router.Get("/internal/config", getConfig)
	// add redirect to config
	router.Post("/redirect", addRedirect)
	// TODO: Implement delete endpoint once the security requirements are defined
	// router.Delete("/redirect", removeRedirect)

	log.Fatal(http.ListenAndServe(listenAddr, router))
}

func index(w http.ResponseWriter, r *http.Request) {
	replyRawText(w, http.StatusOK, "It works.")
}

func redirectHash(w http.ResponseWriter, r *http.Request) {
	hash := chi.URLParam(r, "hash")

	// Use RLock for reads
	configLock.RLock()
	redirect, found := config.Redirects[hash]
	configLock.RUnlock()
	if !found {
		replyWithError(w, http.StatusNotFound, "Not found.")
		return
	}

	sourceURL := r.URL.String()
	configURL := redirect.Destination

	// construct destination URL
	destinationURL, err := url.Parse(redirect.Destination)
	if err != nil {
		replyWithError(w, http.StatusInternalServerError, "Internal error.")
		return
	}
	// merge query parameters
	destinationQuery := destinationURL.Query()
	for key, values := range r.URL.Query() {
		for _, value := range values {
			destinationQuery.Add(key, value)
		}
	}
	destinationURL.RawQuery = destinationQuery.Encode()

	// use fragment from request if present
	if r.URL.Fragment != "" {
		destinationURL.Fragment = r.URL.Fragment
	}
	destinationURLStr := destinationURL.String()

	// get client IP address
	clientIP := httpUtils.GetIP(r)

	// report redirect in analytics
	event := &amplitude.Event{
		UserID:    "link.cu.bzh",
		EventType: "redirect",
		IP:        clientIP,
		EventProperties: map[string]any{
			"hash":   hash,
			"config": configURL,
			"src":    sourceURL,
			"dest":   destinationURLStr,
		},
	}

	// add query parameters to the Amplitude event
	for key, values := range destinationQuery {
		csv := ""
		for i, value := range values {
			csv += value
			if i < len(values)-1 {
				csv += " "
			}
		}
		key = "query_param_" + key
		event.EventProperties[key] = csv
	}

	// add the fragment to the Amplitude event
	event.EventProperties["fragment"] = destinationURL.Fragment

	// send event to Amplitude API
	_ = amplitude.SendEvent(event, amplitude.ChannelLink)

	header := w.Header()
	header.Set("Cache-Control", "no-store")

	// reply with redirect
	http.Redirect(w, r, destinationURLStr, http.StatusMovedPermanently)
}

func getConfig(w http.ResponseWriter, r *http.Request) {

	// // open the JSON file
	// file, err := os.Open(configPath)
	// if err != nil {
	// 	w.WriteHeader(http.StatusInternalServerError)
	// 	w.Write([]byte("failed to read config"))
	// 	return
	// }
	// defer file.Close()

	// // read content of JSON file
	// content, err := io.ReadAll(file)
	// if err != nil {
	// 	w.WriteHeader(http.StatusInternalServerError)
	// 	w.Write([]byte("failed to read config"))
	// 	return
	// }

	err := json.NewEncoder(w).Encode(config)
	if err != nil {
		replyWithError(w, http.StatusInternalServerError, "Internal error.")
		return
	}
	w.WriteHeader(http.StatusOK)
}

func addRedirect(w http.ResponseWriter, r *http.Request) {

	// Parse request
	var req types.Redirect
	err := json.NewDecoder(r.Body).Decode(&req)
	if err != nil {
		replyWithError(w, http.StatusBadRequest, "Failed to decode request: "+err.Error())
		return
	}

	if err := req.Validate(); err != nil {
		replyWithError(w, http.StatusBadRequest, "Invalid request: "+err.Error())
		return
	}

	// ADD NEW REDIRECT
	err = config.CreateRedirect(req)
	if err != nil {
		replyWithError(w, http.StatusInternalServerError, "Internal error: "+err.Error())
		return
	}

	saveConfig()

	w.WriteHeader(http.StatusOK)
}

func loadConfig() error {
	configFromFile, err := readConfig()
	if err != nil {
		return err
	}
	configLock.Lock()
	config = configFromFile
	configLock.Unlock()
	return nil
}

func saveConfig() error {
	configLock.Lock()
	err := writeConfig(config)
	configLock.Unlock()
	return err
}

// readConfig reads config file and returns a Config struct
func readConfig() (*types.Config, error) {
	conf := types.NewConfig()

	// Open the JSON file
	file, err := os.Open(configFilepath)
	if err != nil {
		return conf, nil // return empty config
	}
	defer file.Close()

	// Decode JSON from the file into a Config struct
	decoder := json.NewDecoder(file)
	err = decoder.Decode(conf)

	return conf, err
}

// writeConfig overwrites the config file with the content of the provided Config struct
func writeConfig(conf *types.Config) error {
	// Create or open a file for writing
	file, err := os.Create(configFilepath)
	if err != nil {
		return err
	}
	defer file.Close()

	// Create a JSON encoder that writes to the file
	encoder := json.NewEncoder(file)

	// Encode the struct into JSON and write it to the file
	err = encoder.Encode(*conf)

	return err
}

func replyRawText(w http.ResponseWriter, status int, message string) {
	w.Header().Set("Content-Type", "text/plain")
	w.WriteHeader(status)
	w.Write([]byte(message))
}

func replyWithError(w http.ResponseWriter, status int, message string) {
	type ErrorResponse struct {
		Error string `json:"error"`
	}
	w.Header().Set("Content-Type", "application/json")
	w.WriteHeader(status)
	json.NewEncoder(w).Encode(ErrorResponse{Error: message})
}
