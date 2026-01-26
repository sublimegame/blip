package main

import (
	"fmt"
	"net/http"
	"path/filepath"

	"github.com/go-chi/chi/v5"
	"github.com/go-chi/chi/v5/middleware"

	"cu.bzh/cors"
)

const (
	LISTEN_ADDR    = ":80"
	FILES_ROOT_DIR = "/voxowl/fileserver/files" // Replace with your root folder path
)

func main() {
	fmt.Println("✨ Fileserver listening on port " + LISTEN_ADDR + "...")

	r := chi.NewRouter()
	r.Use(middleware.Logger)
	r.Use(cors.GetCubzhCORSHandler())

	// Define the GET route
	r.Get("/*", func(w http.ResponseWriter, r *http.Request) {
		// Retrieve the filePath from the URL
		filePath := chi.URLParam(r, "*")

		// Create the full path by concatenating the root folder and the file path
		fullPath := filepath.Join(FILES_ROOT_DIR, filePath)

		// Serve the file
		http.ServeFile(w, r, fullPath)
	})

	// Start the server
	err := http.ListenAndServe(LISTEN_ADDR, r)
	fmt.Println("❌ Error:", err.Error())
}
