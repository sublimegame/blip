package utils

import (
	"math/rand"
)

// RandomAlphaNumericString generates a random alphanumeric string of a given length.
// Returned string can contain uppercase and lowercase letters and digits.
func RandomAlphaNumericString(length int) string {
	chars := "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ"
	b := make([]byte, length)
	for i := range b {
		b[i] = chars[rand.Intn(len(chars))]
	}
	return string(b)
}
