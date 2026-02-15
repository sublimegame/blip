package utils

import (
	"crypto/md5"
	"encoding/base64"
	"fmt"

	"github.com/google/uuid"
)

// UUID generates a random UUID v4 and returns it
func UUID() (string, error) {
	uuidObj, err := uuid.NewRandom()
	if err != nil {
		return "", err
	}
	return uuidObj.String(), nil
}

// Base64UUID generates a random UUID v4 and returns its base64 string representation
func Base64UUID() (string, error) {
	uuidObj, err := uuid.NewRandom()
	if err != nil {
		return "", err
	}
	uuidBytes := uuidObj[:]
	encoded := base64.StdEncoding.EncodeToString(uuidBytes)
	return encoded, nil
}

// SaltedMD5edHash takes a base64 encoded hash and returns
// this: base64(md5(base64Decode(hash) + salt))
func Base64SaltedMD5edBase64Hash(hash, salt string) (string, error) {
	b, err := base64.StdEncoding.DecodeString(hash)
	if err != nil {
		return "", err
	}
	s := string(b) + salt
	r := md5.Sum([]byte(s))
	return base64.StdEncoding.EncodeToString([]byte(r[:])), nil
}

// SaltedMD5ed ...
func SaltedMD5ed(hash, salt string) string {
	s := hash + salt
	r := md5.Sum([]byte(s))
	return fmt.Sprintf("%x", r)
}
