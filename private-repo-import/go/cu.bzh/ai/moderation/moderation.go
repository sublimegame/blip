package moderation

import (
	"bytes"
	"context"
	"encoding/json"
	"errors"
	"fmt"
	"net/http"
	"os"
	"regexp"
	"strings"
	"time"
)

type request struct {
	Inputs string `json:"inputs"`
}

type response struct {
	GeneratedText string `json:"generated_text"`
}

type moderationResp struct {
	Flagged bool `json:"flagged"`
}

type Opts struct {
	Timeout time.Duration
}

// ModerateUsername returns whether the username is appropriate.
func ModerateUsername(username string, opts Opts) (bool, error) {

	// AI model endpoint
	url := "https://srk28uhgwzsrm3uv.us-east-1.aws.endpoints.huggingface.cloud"

	prompt := fmt.Sprintf(`<s>[INST]Only output JSON in that format: {"flagged": boolean}, nothing else. Flagging words conveying sexual allusions, hate, harassment, violence, political statements, discrimination or racism. Would you flag the word "%s"? flagged should be true if flagged, false otherwise.[/INST]`, username)

	//
	// Build request
	//

	reqData := request{
		Inputs: prompt,
	}
	jsonStr, err := json.Marshal(reqData)
	if err != nil {
		return false, err
	}

	ctx, cancel := context.WithTimeout(context.Background(), opts.Timeout)
	defer cancel()
	req, err := http.NewRequestWithContext(ctx, http.MethodPost, url, bytes.NewBuffer(jsonStr))
	if err != nil {
		return false, err
	}
	req.Header.Set("Content-Type", "application/json")
	req.Header.Set("Authorization", "Bearer "+os.Getenv("HF_API_TOKEN"))

	//
	// Send request
	//

	client := &http.Client{}
	resp, err := client.Do(req)
	if err != nil {
		return false, err
	}
	defer resp.Body.Close()

	//
	// Parse response
	//

	if resp.StatusCode != 200 {
		return false, errors.New("response status is not 200")
	}

	respData := []response{}
	err = json.NewDecoder(resp.Body).Decode(&respData)
	if err != nil {
		return false, err
	}

	if len(respData) == 0 {
		return false, errors.New("unexpected response length")
	}

	str := ""
	for _, v := range respData {
		str += v.GeneratedText
	}

	// remove prompt
	str = strings.TrimPrefix(str, prompt)

	// Regex pattern to find JSON-like strings
	modResp := moderationResp{
		Flagged: false,
	}
	{
		pattern := `\{[^}]*\}`
		r, _ := regexp.Compile(pattern)

		// Find the JSON string
		responseJsonString := r.FindString(str)

		// Parse the JSON string
		err = json.Unmarshal([]byte(responseJsonString), &modResp)
		if err != nil {
			return false, err
		}
	}

	approved := !modResp.Flagged
	return approved, nil
}
