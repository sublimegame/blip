package main

import (
	"os"
	"strings"
)

const (
	// Secret file paths
	secretsBasePath     = "/cubzh/secrets/"
	appleKeyIDFile      = secretsBasePath + "apple-iap/KEY_ID"
	applePrivateKeyFile = secretsBasePath + "apple-iap/PRIVATE_KEY"
	appleIssuerIDFile   = secretsBasePath + "apple-iap/ISSUER_ID"
)

// loadSecretFromFile loads a secret from a file and trims whitespace
func loadSecretFromFile(path string) (string, error) {
	content, err := os.ReadFile(path)
	if err != nil {
		return "", err
	}
	return strings.TrimSpace(string(content)), nil
}
