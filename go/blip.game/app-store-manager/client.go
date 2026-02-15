package main

import (
	"crypto/ecdsa"
	"crypto/x509"
	"encoding/json"
	"encoding/pem"
	"errors"
	"fmt"
	"io"
	"net/http"
	"net/url"
	"os"
	"time"

	"github.com/dgrijalva/jwt-go"
)

type Client struct {
	authToken string
}

func NewClient(issuerID, keyID, keyFile string) (*Client, error) {
	privateKey, err := privateKeyFromFile(keyFile)
	if err != nil {
		return nil, err
	}
	authToken, err := generateAuthToken(privateKey, issuerID, keyID)
	if err != nil {
		return nil, err
	}

	return &Client{
		authToken: authToken,
	}, nil
}

func (c *Client) sendRequest(req *http.Request) ([]byte, error) {
	httpClient := &http.Client{}
	req.Header.Set("Authorization", "Bearer "+c.authToken)
	req.Header.Set("User-Agent", "App Store Connect Client")

	resp, err := httpClient.Do(req)
	if err != nil {
		return nil, err
	}
	defer resp.Body.Close()

	if resp.StatusCode != http.StatusOK {
		return nil, fmt.Errorf("API request failed with status: %d", resp.StatusCode)
	}

	bytes, err := io.ReadAll(resp.Body)
	if err != nil {
		return nil, err
	}

	return bytes, nil
}

func (c *Client) ListApps() ([]*App, error) {
	qs := url.Values{}
	qs.Set("limit", "50")
	req, err := http.NewRequest(
		http.MethodGet,
		"https://api.appstoreconnect.apple.com/v1/apps",
		nil,
	)
	if err != nil {
		return nil, err
	}

	bytes, err := c.sendRequest(req)
	if err != nil {
		return nil, err
	}

	var res Response
	if err := json.Unmarshal(bytes, &res); err != nil {
		return nil, err
	}

	apps := make([]*App, len(res.Data))
	for i, app := range res.Data {
		apps[i] = &App{
			ID:   app.ID,
			Name: app.Attributes.Name,
		}
	}

	return apps, nil
}

func (c *Client) GetLocalizations(appID string) ([]*Localization, error) {
	req, err := http.NewRequest(
		http.MethodGet,
		fmt.Sprintf("https://api.appstoreconnect.apple.com/v1/apps/%s/appInfos?include=appInfoLocalizations", appID),
		nil,
	)
	if err != nil {
		return nil, err
	}

	bytes, err := c.sendRequest(req)
	if err != nil {
		return nil, err
	}

	var res Response
	if err := json.Unmarshal(bytes, &res); err != nil {
		return nil, err
	}

	localizations := make([]*Localization, 0)
	for _, appInfo := range res.Included {
		localization, ok := BlockToLocalization(appInfo)
		if !ok {
			continue
		}
		localizations = append(localizations, localization)
	}

	return localizations, nil
}

func privateKeyFromFile(keyFile string) (*ecdsa.PrivateKey, error) {

	bytes, err := os.ReadFile(keyFile)
	if err != nil {
		return nil, err
	}

	block, _ := pem.Decode(bytes)
	if block == nil {
		return nil, errors.New("AuthKey must be a valid .p8 PEM file")
	}
	key, err := x509.ParsePKCS8PrivateKey(block.Bytes)
	if err != nil {
		return nil, err
	}

	switch pk := key.(type) {
	case *ecdsa.PrivateKey:
		return pk, nil
	default:
		return nil, errors.New("AuthKey must be of type ecdsa.PrivateKey")
	}

}

func generateAuthToken(privateKey *ecdsa.PrivateKey, issuerID, keyID string) (string, error) {

	expirationTimestamp := time.Now().Add(15 * time.Minute)

	token := jwt.NewWithClaims(jwt.SigningMethodES256, jwt.MapClaims{
		"iss": issuerID,
		"exp": expirationTimestamp.Unix(),
		"aud": "appstoreconnect-v1",
	})

	token.Header["kid"] = keyID

	tokenString, err := token.SignedString(privateKey)
	if err != nil {
		return "", err
	}

	return tokenString, nil
}
