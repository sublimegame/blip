package main

import (
	"crypto/md5"
	"crypto/rand"
	"crypto/sha256"
	"encoding/hex"
	"io"
	"strings"
)

func generateRandomHash() (string, error) {
	// Size of the random data. You can adjust it according to your needs.
	randomData := make([]byte, 32) // 32 bytes

	// Read random bytes; this uses a cryptographically secure random generator
	if _, err := rand.Read(randomData); err != nil {
		return "", err
	}

	// Hash the random data
	hash := sha256.Sum256(randomData)

	// Encode the hash to a hexadecimal string
	hashStr := hex.EncodeToString(hash[:])

	return hashStr, nil
}

// OUTPUT EXAMPLE:
// HASH: 857bcdb99323fe9ae0dd525ee6ea5cb5e4f6e81b54f6c9a33baac9983a4ed751
// KEY: f54798e898d6b79d780bd446150d7c88
func challengeGetHashAndKey() (string, string, error) {
	hash, err := generateRandomHash()
	if err != nil {
		return "", "", err
	}

	// hub world ID
	// ! \\ needs to be updated if changes within client
	worldID := "958fa098-7482-4a7a-b863-ec9330f0f4a1"
	part1 := worldID[15:25]
	part2 := strings.ToUpper(worldID[3:18])

	salted := hash[0:15] + part1 + hash[15:30] + part2 + hash[30:45] + "cubzh" + hash[45:len(hash)]

	h := md5.New()

	_, err = io.WriteString(h, salted)

	if err != nil {
		return "", "", err
	}

	keyHash := h.Sum(nil)
	key := hex.EncodeToString(keyHash[:])

	return hash, key, nil
}
