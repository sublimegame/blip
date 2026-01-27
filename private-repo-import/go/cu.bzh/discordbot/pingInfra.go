package main

import (
	"bytes"
	"errors"
	"io"
	"net/http"
	"os"
	"strings"
	"time"
)

const (
	DELAY_RETRY = 3 * time.Second
)

// Function loop checks if the servers are up or down
func pingAllServers() (cubzh, docs, api, app bool, huggingFaceError error) {
	var err error

	// Cubzh website (https://cu.bzh)
	err = pingServer("https://cu.bzh", `What is Cubzh?`)
	cubzh = err == nil

	// Cubzh documentation (https://docs.cu.bzh)
	err = pingServer("https://docs.cu.bzh", `This documentation contains everything you need to know as a developer to create your own realtime multiplayer games. 👾`)
	docs = err == nil

	// Cubzh API (https://api.cu.bzh)
	err = pingServer("https://api.cu.bzh/ping", "{\"msg\":\"pong\"}")
	api = err == nil

	// Wasm app (https://app.cu.bzh)
	err = pingServer("https://app.cu.bzh", `We recommend using <a href="https://www.google.com/intl/en_en/chrome/">Google Chrome</a>`)
	app = err == nil

	// AI model endpoint
	{
		url := "https://srk28uhgwzsrm3uv.us-east-1.aws.endpoints.huggingface.cloud"
		payload := []byte(`{"inputs":"<s>[INST]Say hello[/INST]"}`)
		var req *http.Request
		req, huggingFaceError = http.NewRequest("POST", url, bytes.NewBuffer(payload))
		if huggingFaceError == nil {
			req.Header.Set("Content-Type", "application/json")
			req.Header.Set("Authorization", "Bearer "+os.Getenv("HF_API_TOKEN"))
			huggingFaceError = pingServer2(req, `"generated_text":`)
		}
	}

	return
}

// Function pingServer checks with an url if he can find a text in the body of the response when we ping it
func pingServer(url string, textToFind string) error {
	err := testWebpage(url, textToFind)
	if err != nil {
		time.Sleep(DELAY_RETRY)
		err = testWebpage(url, textToFind)
		if err != nil {
			time.Sleep(DELAY_RETRY)
			err = testWebpage(url, textToFind)
			if err != nil {
				return err
			}
		} else {
			return err
		}
	} else {
		return err
	}
	return err
}

func testWebpage(url string, textToFind string) error {

	resp, err := http.Get(url)
	if err != nil {
		return err
	}
	defer resp.Body.Close()

	if resp.StatusCode != http.StatusOK {
		err = errors.New("status code is not 200")
		return err
	}

	body, err := io.ReadAll(resp.Body)
	if err != nil {
		return err
	}

	if strings.Contains(string(body), textToFind) {
		// If the text is found, do nothing that means the server is up
		return err
	} else {
		err = errors.New("server is down")
		return err
	}
}

// Function pingServer checks with an url if he can find a text in the body of the response when we ping it
func pingServer2(req *http.Request, textToFind string) error {
	err := testWebpage2(req, textToFind)
	if err != nil {
		time.Sleep(DELAY_RETRY)
		err = testWebpage2(req, textToFind)
		if err != nil {
			time.Sleep(DELAY_RETRY)
			err = testWebpage2(req, textToFind)
		}
	}
	return err
}

func testWebpage2(req *http.Request, textToFind string) error {
	resp, err := http.DefaultClient.Do(req)
	if err != nil {
		return err
	}
	defer resp.Body.Close()

	if resp.StatusCode != http.StatusOK {
		err = errors.New("status code is not 200")
		return err
	}

	body, err := io.ReadAll(resp.Body)
	if err != nil {
		return err
	}

	if strings.Contains(string(body), textToFind) {
		// If the text is found, do nothing that means the server is up
		return err
	} else {
		err = errors.New("server is down")
		return err
	}
}
