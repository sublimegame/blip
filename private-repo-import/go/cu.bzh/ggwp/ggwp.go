package ggwp

import (
	"bytes"
	"context"
	"encoding/json"
	"errors"
	"net/http"
	"os"
	"time"
)

type ChatMessageRequest struct {
	SessionID string `json:"session_id"`
	Message   string `json:"message"`
	UserID    string `json:"user_id"`
}

type MessageDetails struct {
	ID                 string `json:"message_id"`
	Flag               bool   `json:"flag"`
	Severity           string `json:"severity"` // low, medium, high
	FilteredMessage    string `json:"filtered_message"`
	RecommendedMessage string `json:"recommended_message"`
	Language           string `json:"language"`
	Violence           bool   `json:"violence"`
	VerbalAbuse        bool   `json:"verbal_abuse"`
	Profanity          bool   `json:"profanity"`
	SexualContent      bool   `json:"sexual_content"`
	IdentityHate       bool   `json:"identity_hate"`
	Drugs              bool   `json:"drugs"`
	SelfHarm           bool   `json:"self_harm"`
	Custom             bool   `json:"custom"`
	Spam               bool   `json:"spam"`
	Link               bool   `json:"link"`
	Pii                bool   `json:"pii"`
}

type ChatMessageResponse struct {
	MessageDetails *MessageDetails `json:"message_details"`
}

func (r *ChatMessageResponse) IsMessageOk() bool {
	return r.MessageDetails.Flag == false || r.MessageDetails.Severity == "low"
}

type Opts struct {
	Timeout time.Duration
}

// ModerateUsername returns whether the username is appropriate.
func PostChatMessage(message, userID, sessionID string, opts Opts) (*ChatMessageResponse, error) {

	url := "https://api.ggwp.com/chat/v2/message"

	reqData := &ChatMessageRequest{
		Message:   message,
		UserID:    userID,
		SessionID: sessionID,
	}
	jsonStr, err := json.Marshal(reqData)
	if err != nil {
		return nil, err
	}

	ctx, cancel := context.WithTimeout(context.Background(), opts.Timeout)
	defer cancel()
	req, err := http.NewRequestWithContext(ctx, http.MethodPost, url, bytes.NewBuffer(jsonStr))
	if err != nil {
		return nil, err
	}
	req.Header.Set("Content-Type", "application/json")
	req.Header.Set("x-api-key", os.Getenv("GGWP_API_KEY"))

	client := &http.Client{}
	resp, err := client.Do(req)
	if err != nil {
		return nil, err
	}
	defer resp.Body.Close()

	if resp.StatusCode != 200 {
		return nil, errors.New("response status is not 200")
	}

	var response ChatMessageResponse
	err = json.NewDecoder(resp.Body).Decode(&response)
	if err != nil {
		return nil, err
	}

	return &response, nil
}
