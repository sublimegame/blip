package main

import (
	"bytes"
	"crypto/rand"
	"encoding/base64"
	"encoding/hex"
	"encoding/json"
	"flag"
	"fmt"
	"image"
	"image/draw"
	"image/png"
	"io"
	"net/http"
	"os"
	"path/filepath"
	"time"

	"github.com/blackironj/panorama/conv"
)

const (
	filesRootDir = "./files"
	version      = 1
)

var (
	apiKey = os.Getenv("SCENARIO_API_KEY")
	secret = os.Getenv("SCENARIO_API_SECRET")
)

type GenerateRequest struct {
	Prompt     string `json:"prompt"`
	Width      int    `json:"width"`
	Style      string `json:"style"`
	NumOutputs int    `json:"numOutputs"`
}

type WebGenerateRequest struct {
	Prompt string `json:"prompt"`
}

type WebGenerateResponse struct {
	URL string `json:"url"`
}

type Job struct {
	ID     string `json:"jobId"`
	Status    string `json:"status"`
	Metadata  JobMetadata `json:"metadata"`
	Progress float32 `json:"progress"`
}

type JobMetadata struct {
	AssetIds []string `json:"assetIds"`
}

type GenerateResponse struct {
	Job Job `json:"job"`
	CreativeUnitsCost int32 `json:"creativeUnitsCost"`
}

type AssetResponse struct {
	Asset struct {
		URL string `json:"url"`
	} `json:"asset"`
}

func generateRandomFilename() string {
	bytes := make([]byte, 16)
	rand.Read(bytes)
	return hex.EncodeToString(bytes) + ".png"
}

func generateImage(prompt string) (string, error) {
	fmt.Printf("Received prompt: %s\n", prompt)

	// Create output directory if it doesn't exist
	outDir := filepath.Join(filesRootDir, fmt.Sprintf("v%d", version))
	os.MkdirAll(outDir, 0755)

	fmt.Println("Created output directory")

	// Create auth header
	auth := base64.StdEncoding.EncodeToString([]byte(apiKey + ":" + secret))

	// Generate image
	reqBody := GenerateRequest{
		Prompt:     prompt,
		Width:      1024,
		Style:      "standard",
		NumOutputs: 1,
	}

	fmt.Println("Sending generation request to API...")

	jsonBody, err := json.Marshal(reqBody)
	if err != nil {
		return "", err
	}

	req, err := http.NewRequest("POST", "https://api.cloud.scenario.com/v1/generate/skybox-base-360", bytes.NewBuffer(jsonBody))
	if err != nil {
		return "", err
	}

	req.Header.Set("Authorization", "Basic "+auth)
	req.Header.Set("Content-Type", "application/json")

	client := &http.Client{}
	resp, err := client.Do(req)
	if err != nil {
		return "", err
	}
	defer resp.Body.Close()

	if resp.StatusCode != http.StatusOK {
		return "", fmt.Errorf("API error: %d", resp.StatusCode)
	}

	var genResp GenerateResponse
	if err := json.NewDecoder(resp.Body).Decode(&genResp); err != nil {
		return "", err
	}

	jobID := genResp.Job.ID
	fmt.Printf("Job created with ID: %s\n", jobID)

	// Poll for completion
	fmt.Println("Polling for job completion...")
	for {
		req, _ := http.NewRequest("GET", "https://api.cloud.scenario.com/v1/jobs/"+jobID, nil)
		req.Header.Set("Authorization", "Basic "+auth)

		resp, err := client.Do(req)
		if err != nil {
			return "", err
		}

		var statusResp GenerateResponse
		if err := json.NewDecoder(resp.Body).Decode(&statusResp); err != nil {
			return "", err
		}
		resp.Body.Close()

		fmt.Printf("Job status: %s (Progress: %.1f%%)\n", statusResp.Job.Status, statusResp.Job.Progress*100)

		if statusResp.Job.Status == "success" {
			fmt.Println("Generation completed successfully!")

			// Get asset info
			assetID := statusResp.Job.Metadata.AssetIds[0]
			fmt.Printf("Retrieving asset info for ID: %s\n", assetID)

			req, _ := http.NewRequest("GET", "https://api.cloud.scenario.com/v1/assets/"+assetID, nil)
			req.Header.Set("Authorization", "Basic "+auth)

			resp, err := client.Do(req)
			if err != nil {
				return "", err
			}

			var assetResp AssetResponse
			if err := json.NewDecoder(resp.Body).Decode(&assetResp); err != nil {
				return "", err
			}
			resp.Body.Close()

			// Download image
			fmt.Println("Downloading generated image...")
			resp, err = http.Get(assetResp.Asset.URL)
			if err != nil {
				return "", err
			}
			defer resp.Body.Close()

			out, err := os.CreateTemp("", "*.png")
			if err != nil {
				return "", err
			}
			defer out.Close()
			tempFile := out.Name()

			_, err = io.Copy(out, resp.Body)
			if err != nil {
				return "", err
			}

			fmt.Println("Converting equirectangular image to cubemap...")

			inImage, ext, err := conv.ReadImage(tempFile)
			if err != nil {
				return "", err
			}

			edgeLen := 512
			sides := []string{"front", "back", "left", "right", "top", "bottom"}
			canvases := conv.ConvertEquirectangularToCubeMap(edgeLen, inImage, sides)

			if err := conv.WriteImage(canvases, ".", ext, sides); err != nil {
				return "", err
			}

			fmt.Println("Combining cubemap faces into single image...")

			// Combine images into a single image
			images := map[string]struct{ x, y int }{
				"top.png":    {1, 0},
				"left.png":   {0, 1},
				"back.png":   {3, 1},
				"right.png":  {2, 1},
				"front.png":  {1, 1},
				"bottom.png": {1, 2},
			}

			// Create a new RGBA image that's 4x3 tiles of edgeLen
			combined := image.NewRGBA(image.Rect(0, 0, edgeLen*4, edgeLen*3))

			// Load and copy each image into position
			for filename, pos := range images {
				file, err := os.Open(filename)
				if err != nil {
					return "", err
				}

				img, _, err := image.Decode(file)
				if err != nil {
					file.Close()
					return "", err
				}

				x := pos.x * edgeLen
				y := pos.y * edgeLen

				draw.Draw(combined, image.Rect(x, y, x+edgeLen, y+edgeLen), img, image.Point{0, 0}, draw.Over)

				file.Close()
				os.Remove(filename)
			}

			// Generate random filename and save
			outFilename := generateRandomFilename()
			outPath := filepath.Join(outDir, outFilename)

			fmt.Printf("Saving final image to: %s\n", outPath)

			outFile, err := os.Create(outPath)
			if err != nil {
				return "", err
			}

			if err := png.Encode(outFile, combined); err != nil {
				outFile.Close()
				return "", err
			}
			outFile.Close()

			os.Remove(tempFile)
			fmt.Println("Image processing completed successfully!")
			return outFilename, nil
		}

		time.Sleep(1 * time.Second)
	}
}

func main() {
	promptFlag := flag.String("prompt", "", "Generate single image from prompt")
	flag.Parse()

	if *promptFlag != "" {
		filename, err := generateImage(*promptFlag)
		if err != nil {
			fmt.Printf("Error generating image: %v\n", err)
			os.Exit(1)
		}
		fmt.Printf("Generated image: %s\n", filename)
		return
	}

	http.HandleFunc("/v1/", func(w http.ResponseWriter, r *http.Request) {
		http.FileServer(http.Dir(filesRootDir)).ServeHTTP(w, r)
	})

	http.HandleFunc("/", func(w http.ResponseWriter, r *http.Request) {
		if r.Method != "POST" {
			http.Error(w, "Method not allowed", http.StatusMethodNotAllowed)
			return
		}

		var req WebGenerateRequest
		if err := json.NewDecoder(r.Body).Decode(&req); err != nil {
			http.Error(w, err.Error(), http.StatusBadRequest)
			return
		}

		filename, err := generateImage(req.Prompt)
		if err != nil {
			http.Error(w, err.Error(), http.StatusInternalServerError)
			return
		}

		resp := WebGenerateResponse{
			URL: fmt.Sprintf("/v%d/%s", version, filename),
		}

		w.Header().Set("Content-Type", "application/json")
		json.NewEncoder(w).Encode(resp)
	})

	fmt.Println("SERVER starting on :80...")
	http.ListenAndServe(":80", nil)
}
