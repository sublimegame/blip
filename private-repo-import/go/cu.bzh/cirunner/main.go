package main

import (
	"bytes"
	"encoding/json"
	"fmt"
	"io"
	"io/ioutil"
	"log"
	"net/http"
	"os"
	"os/exec"
	"path/filepath"
	"time"
)

const (
	port             = "42080"
	workdir          = "/Users/aduermael/cubzh-ci-repos"
	STATUS_URL       = "https://api.github.com/repos/%s/statuses/%s"
	CONTEXT          = "CI / Runner"
	CLOUDFLARE_DOMAIN = "ci.cu.bzh"
)

var (
	GITHUB_ACCESS_TOKEN  = os.Getenv("GITHUB_ACCESS_TOKEN")
	CLOUDFLARE_API_TOKEN = os.Getenv("CLOUDFLARE_API_TOKEN")
	CLOUDFLARE_ZONE_ID   = os.Getenv("CLOUDFLARE_ZONE_ID")
)

var (
	taskChannel chan PullRequestEvent
)

type PullRequestEvent struct {
	Repository string `json:"repository"`
	Branch     string `json:"branch"`
	Commit     string `json:"commit_sha"`
	Number     string `json:"pull_request_number"`
}

type CloudflareResponse struct {
	Success bool `json:"success"`
	Result  []struct {
		ID      string `json:"id"`
		Name    string `json:"name"`
		Content string `json:"content"`
		Type    string `json:"type"`
	} `json:"result"`
}

func main() {
	taskChannel = make(chan PullRequestEvent)

	go ciRoutine()

	// Add IP update routine
	go func() {
		for {
			updateCloudflareIP()
			time.Sleep(5 * time.Minute)
		}
	}()

	http.HandleFunc("/webhook", handleWebhook)
	fmt.Printf("Server listening on port %s...\n", port)
	log.Fatal(http.ListenAndServe(":"+port, nil))
}

func handleWebhook(w http.ResponseWriter, r *http.Request) {
	if r.Method != "POST" {
		w.WriteHeader(http.StatusMethodNotAllowed)
		return
	}

	body, err := ioutil.ReadAll(r.Body)
	if err != nil {
		fmt.Println("💥 could not read request body")
		http.Error(w, "could not read request body", http.StatusInternalServerError)
		return
	}

	var event PullRequestEvent
	err = json.Unmarshal(body, &event)
	if err != nil {
		fmt.Println("💥 could not parse payload")
		http.Error(w, "could not parse payload", http.StatusBadRequest)
		return
	}

	reportStatus(event.Commit, event.Repository, "pending", CONTEXT, "enqueued...")

	taskChannel <- event

	w.WriteHeader(http.StatusOK)
}

func ciRoutine() {
	for {
		task := <-taskChannel
		runTask(task)
	}
}

func runTask(task PullRequestEvent) {

	var err error

	fmt.Println("✨ PR #" + task.Number + " (" + task.Repository + "-" + task.Commit + ")")
	dir := filepath.Join(workdir, task.Repository)
	repoURL := "https://github.com/" + task.Repository + ".git"
	ensureDir(dir)

	reportStatus(task.Commit, task.Repository, "pending", CONTEXT, "fetching...")

	// clone repository the first time
	if isGitRepo(dir) == false {
		fmt.Printf("⚙️ Cloning repository...")
		if err := gitClone(repoURL, dir); err != nil {
			reportStatus(task.Commit, task.Repository, "failure", CONTEXT, "could not clone")
			fmt.Println("💥 Error cloning repository:", err)
			return
		}
		fmt.Println("\r\033[K✅ Cloned repository")
	}

	fmt.Printf("⚙️ Fetching & Resetting...")

	if err := gitFetchAndReset(dir, task.Commit); err != nil {
		reportStatus(task.Commit, task.Repository, "failure", CONTEXT, "could not fetch")
		fmt.Println("💥 Error fetching and resetting repository after clone:", err)
		return
	}

	fmt.Println("\r\033[K✅ Fetched & Reset")

	reportStatus(task.Commit, task.Repository, "pending", CONTEXT, "running...")

	err = os.Chdir(dir)
	if err != nil {
		reportStatus(task.Commit, task.Repository, "failure", CONTEXT, "could not select dir")
		fmt.Println("💥 Error changing directory:", err)
		return
	}

	os.Setenv("CI", "1")
	os.Setenv("GITHUB_TOKEN", GITHUB_ACCESS_TOKEN)
	daggerCmd := exec.Command("dagger", "call", "github-ci",
		"--commit="+task.Commit,
		"--repo="+task.Repository,
		"--token=env:GITHUB_TOKEN",
		"--src=.")
	// daggerCmd.Stdout = io.Discard
	// daggerCmd.Stderr = io.Discard
	daggerCmd.Stdout = os.Stdout
	daggerCmd.Stderr = os.Stderr
	if err := daggerCmd.Run(); err != nil {
		reportStatus(task.Commit, task.Repository, "failure", CONTEXT, "dagger command error")
		fmt.Println("💥 Error running dagger command:", err)
		return
	}

	fmt.Println("✅")
	reportStatus(task.Commit, task.Repository, "success", CONTEXT, "")
}

// ensureDir ensures that a directory exists, and creates it if not
func ensureDir(dir string) error {
	// fmt.Println("ensure dir:", dir)
	if _, err := os.Stat(dir); os.IsNotExist(err) {
		fmt.Println("create dir:", dir)
		return os.MkdirAll(dir, 0755)
	}
	return nil
}

// isGitRepo checks if a directory is a git repository
func isGitRepo(dir string) bool {
	gitDir := filepath.Join(dir, ".git")
	// fmt.Println("is git repo:", gitDir)
	if _, err := os.Stat(gitDir); os.IsNotExist(err) {
		fmt.Println(dir, "is not a git repo", err.Error())
		return false
	}
	return true
}

// gitClone clones a git repository to a specified directory
func gitClone(repoURL, dir string) error {
	cmd := exec.Command("git", "clone", repoURL, dir)
	cmd.Stdout = os.Stdout
	cmd.Stderr = os.Stderr
	return cmd.Run()
}

// Fetch, reset, update submodules & pull lfs
func gitFetchAndReset(dir, commitHash string) error {
	// Fetch the latest changes from the origin
	fetchCmd := exec.Command("git", "-C", dir, "fetch", "origin", commitHash)
	fetchCmd.Stdout = io.Discard
	fetchCmd.Stderr = io.Discard
	// fetchCmd.Stdout = os.Stdout
	// fetchCmd.Stderr = os.Stderr
	if err := fetchCmd.Run(); err != nil {
		return fmt.Errorf("error fetching updates: %w", err)
	}

	// Reset to the specified commit hash with a hard reset
	resetCmd := exec.Command("git", "-C", dir, "reset", "--hard", commitHash)
	resetCmd.Stdout = io.Discard
	resetCmd.Stderr = io.Discard
	// resetCmd.Stdout = os.Stdout
	// resetCmd.Stderr = os.Stderr
	if err := resetCmd.Run(); err != nil {
		return fmt.Errorf("error resetting to commit %s: %w", commitHash, err)
	}

	// Update submodules recursively
	submoduleCmd := exec.Command("git", "-C", dir, "submodule", "update", "--init", "--recursive")
	submoduleCmd.Stdout = io.Discard
	submoduleCmd.Stderr = io.Discard
	// submoduleCmd.Stdout = os.Stdout
	// submoduleCmd.Stderr = os.Stderr
	if err := submoduleCmd.Run(); err != nil {
		return fmt.Errorf("error updating submodules: %w", err)
	}

	// Pull the LFS files after reset
	lfsPullCmd := exec.Command("git", "-C", dir, "lfs", "pull")
	lfsPullCmd.Stdout = io.Discard
	lfsPullCmd.Stderr = io.Discard
	// lfsPullCmd.Stdout = os.Stdout
	// lfsPullCmd.Stderr = os.Stderr
	if err := lfsPullCmd.Run(); err != nil {
		return fmt.Errorf("error pulling LFS files: %w", err)
	}
	return nil
}

func reportStatus(commit, repo, state, context, description string) error {
	url := fmt.Sprintf(STATUS_URL, repo, commit)

	fmt.Println("reportStatus url:", url)

	payload := map[string]string{
		"state":       state,
		"description": description,
		"context":     context,
	}

	jsonPayload, err := json.Marshal(payload)
	if err != nil {
		fmt.Printf("💥 failed to marshal JSON payload: %v\n", err)
		return fmt.Errorf("failed to marshal JSON payload: %v", err)
	}

	req, err := http.NewRequest("POST", url, ioutil.NopCloser(bytes.NewReader(jsonPayload)))
	if err != nil {
		fmt.Printf("💥 failed to create HTTP request: %v\n", err)
		return fmt.Errorf("failed to create HTTP request: %v", err)
	}

	req.Header.Set("Authorization", "token "+GITHUB_ACCESS_TOKEN)
	req.Header.Set("Content-Type", "application/json")

	client := &http.Client{}
	resp, err := client.Do(req)
	if err != nil {
		fmt.Printf("💥 failed to send HTTP request: %v\n", err)
		return fmt.Errorf("failed to send HTTP request: %v", err)
	}
	defer resp.Body.Close()

	if resp.StatusCode != http.StatusCreated {
		fmt.Printf("💥 unexpected status code: %d\n", resp.StatusCode)
		return fmt.Errorf("unexpected status code: %d", resp.StatusCode)
	}

	// _, err := io.ReadAll(resp.Body)
	// if err != nil {
	// 	fmt.Printf("💥 can't read body: %v\n", err)
	// 	return fmt.Errorf("can't read body:: %v", err)
	// }

	fmt.Println("✅ report status sent:", context, description)

	return nil
}

func updateCloudflareIP() {
	// Get current public IP
	resp, err := http.Get("https://api.ipify.org")
	if err != nil {
		fmt.Printf("💥 Failed to get public IP: %v\n", err)
		return
	}
	defer resp.Body.Close()

	ipBytes, err := io.ReadAll(resp.Body)
	if err != nil {
		fmt.Printf("💥 Failed to read IP response: %v\n", err)
		return
	}
	currentIP := string(ipBytes)

	// fmt.Printf("Current IP: %s\n", currentIP)

	// Get existing DNS record
	listURL := fmt.Sprintf("https://api.cloudflare.com/client/v4/zones/%s/dns_records?type=A&name=%s",
		CLOUDFLARE_ZONE_ID, CLOUDFLARE_DOMAIN)

	req, _ := http.NewRequest("GET", listURL, nil)
	req.Header.Set("Authorization", "Bearer "+CLOUDFLARE_API_TOKEN)
	req.Header.Set("Content-Type", "application/json")

	client := &http.Client{}
	resp, err = client.Do(req)
	if err != nil {
		fmt.Printf("💥 Failed to get DNS records: %v\n", err)
		return
	}
	defer resp.Body.Close()

	var result CloudflareResponse
	if err := json.NewDecoder(resp.Body).Decode(&result); err != nil {
		fmt.Printf("💥 Failed to parse DNS response: %v\n", err)
		return
	}

	if !result.Success {
		fmt.Println("💥 Failed to get DNS records from Cloudflare")
		return
	}

	var recordID string
	var needsUpdate bool = true

	if len(result.Result) > 0 {
		record := result.Result[0]
		recordID = record.ID
		if record.Content == currentIP {
			needsUpdate = false
		}
	}

	if !needsUpdate {
		fmt.Println("✅ DNS record is up to date")
		return
	}

	// Update the DNS record
	updateURL := fmt.Sprintf("https://api.cloudflare.com/client/v4/zones/%s/dns_records/%s",
		CLOUDFLARE_ZONE_ID, recordID)

	updateData := map[string]interface{}{
		"type":    "A",
		"name":    CLOUDFLARE_DOMAIN,
		"content": currentIP,
		"ttl":     120,
		"proxied": false,
	}

	jsonData, _ := json.Marshal(updateData)
	req, _ = http.NewRequest("PUT", updateURL, bytes.NewBuffer(jsonData))
	req.Header.Set("Authorization", "Bearer "+CLOUDFLARE_API_TOKEN)
	req.Header.Set("Content-Type", "application/json")

	resp, err = client.Do(req)
	if err != nil {
		fmt.Printf("💥 Failed to update DNS record: %v\n", err)
		return
	}
	defer resp.Body.Close()

	if resp.StatusCode == http.StatusOK {
		fmt.Printf("✅ Updated DNS record to IP: %s\n", currentIP)
	} else {
		fmt.Printf("💥 Failed to update DNS record. Status: %d\n", resp.StatusCode)
	}
}
