package main

import (
	"crypto/ecdsa"
	"crypto/x509"
	"encoding/pem"
	"errors"
	"fmt"
	"os"
	"sync"
	"time"

	"github.com/golang-jwt/jwt/v4"
)

type tokenInfo struct {
	token      string
	expiresAt  time.Time
	privateKey *ecdsa.PrivateKey
	issuerID   string
	keyID      string
}

var (
	tokenMutex sync.RWMutex
	tokenCache *tokenInfo
)

// GetAppStoreJWT returns a valid JWT token for App Store API authentication.
// It will reuse the existing token if it's still valid, or generate a new one if needed.
func GetAppStoreJWT() (string, error) {
	tokenMutex.RLock()
	if tokenCache != nil && time.Until(tokenCache.expiresAt) > 5*time.Minute {
		token := tokenCache.token
		tokenMutex.RUnlock()
		return token, nil
	}
	tokenMutex.RUnlock()

	// Need to generate a new token
	tokenMutex.Lock()
	defer tokenMutex.Unlock()

	// Double check after acquiring write lock
	if tokenCache != nil && time.Until(tokenCache.expiresAt) > 5*time.Minute {
		return tokenCache.token, nil
	}

	// If we don't have a token or it's expired, initialize a new one
	if tokenCache == nil {
		keyID, err := loadSecretFromFile(appleKeyIDFile)
		if err != nil {
			return "", err
		}

		issuerID, err := loadSecretFromFile(appleIssuerIDFile)
		if err != nil {
			return "", err
		}

		privateKey, err := privateKeyFromFile(applePrivateKeyFile)
		if err != nil {
			return "", err
		}

		tokenCache = &tokenInfo{
			privateKey: privateKey,
			issuerID:   issuerID,
			keyID:      keyID,
		}
	}

	// Generate new token
	token, err := generateAuthToken(tokenCache.privateKey, tokenCache.issuerID, tokenCache.keyID)
	if err != nil {
		return "", err
	}

	// Update cache
	tokenCache.token = token
	tokenCache.expiresAt = time.Now().Add(15 * time.Minute)

	return token, nil
}

// privateKeyFromFile reads and parses the private key from a .p8 file
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

// generateAuthToken creates a JWT token for App Store API authentication
func generateAuthToken(privateKey *ecdsa.PrivateKey, issuerID, keyID string) (string, error) {
	// Use current time for iat
	iat := time.Now().UTC()
	expirationTime := iat.Add(15 * time.Minute)

	fmt.Printf("💰 TOKEN GENERATION DETAILS:\n")
	fmt.Printf("  - Issuer ID: %s\n", issuerID)
	fmt.Printf("  - Key ID: %s\n", keyID)
	fmt.Printf("  - Private Key Type: %T\n", privateKey)
	fmt.Printf("  - Private Key Curve: %s\n", privateKey.Curve.Params().Name)
	fmt.Printf("  - Issued At: %s\n", iat.Format(time.RFC3339))
	fmt.Printf("  - Expires At: %s\n", expirationTime.Format(time.RFC3339))

	// Validate inputs
	if issuerID == "" {
		return "", errors.New("issuer ID cannot be empty")
	}
	if keyID == "" {
		return "", errors.New("key ID cannot be empty")
	}
	if privateKey == nil {
		return "", errors.New("private key cannot be nil")
	}

	// Create token with claims
	token := jwt.NewWithClaims(jwt.SigningMethodES256, jwt.MapClaims{
		"iss": issuerID,              // Your App Store Connect issuer ID
		"iat": iat.Unix(),            // Issued at time (current time)
		"exp": expirationTime.Unix(), // Expiration time (15 minutes from now)
		"aud": "appstoreconnect-v1",  // Audience
		"bid": "com.voxowl.blip",     // Bundle ID - verify this matches your app
	})

	// Set required headers
	token.Header["alg"] = "ES256"
	token.Header["typ"] = "JWT"
	token.Header["kid"] = keyID

	// Sign the token with the private key
	tokenString, err := token.SignedString(privateKey)
	if err != nil {
		fmt.Printf("💰 TOKEN SIGNING ERROR: %v\n", err)
		return "", fmt.Errorf("failed to sign token: %v", err)
	}

	fmt.Printf("💰 TOKEN GENERATED\n")

	// Verify the token can be parsed back
	parsedToken, err := jwt.Parse(tokenString, func(token *jwt.Token) (interface{}, error) {
		// Verify the signing method
		if _, ok := token.Method.(*jwt.SigningMethodECDSA); !ok {
			return nil, fmt.Errorf("unexpected signing method: %v", token.Header["alg"])
		}
		return &privateKey.PublicKey, nil
	})
	if err != nil {
		fmt.Printf("💰 TOKEN VERIFICATION ERROR: %v\n", err)
		return "", fmt.Errorf("failed to verify generated token: %v", err)
	}
	if !parsedToken.Valid {
		return "", errors.New("generated token is invalid")
	}

	fmt.Printf("💰 TOKEN GENERATED AND VERIFIED SUCCESSFULLY\n")
	return tokenString, nil
}
