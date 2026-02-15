package user

import (
	"errors"
	"regexp"
	"strings"
)

const (
	usernameMaxLen = 16
	usernameRegex  = "^[a-zA-Z][a-zA-Z0-9]{0,15}$"
)

// UsernameOrEmailSanitize ...
func UsernameOrEmailSanitize(str string) string {
	return strings.TrimSpace(strings.ToLower(str))
}

// IsUsernameValid makesreturns true if given string is a valid username
func IsUsernameValid(str string) bool {
	rxEmail := regexp.MustCompile(usernameRegex)
	return rxEmail.MatchString(str)
}

func SanitizeUsername(str string) (string, error) {
	if IsUsernameValid(str) == false {
		return "", errors.New("invalid username")
	}
	return strings.ToLower(str), nil
}

// IsEmailValid returns true if given string is a valid email
func IsEmailValid(str string) bool {
	var rxEmail = regexp.MustCompile("^[a-zA-Z0-9.!#$%&'*+\\/=?^_`{|}~-]+@[a-zA-Z0-9](?:[a-zA-Z0-9-]{0,61}[a-zA-Z0-9])?(?:\\.[a-zA-Z0-9](?:[a-zA-Z0-9-]{0,61}[a-zA-Z0-9])?)*$")

	if len(str) > 254 || rxEmail.MatchString(str) == false {
		return false
	}

	return true
}

func SanitizeMagicKey(str string) string {
	str = strings.ToLower(str)
	str = strings.TrimSpace(str)
	return str
}
