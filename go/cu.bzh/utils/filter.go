package utils

import (
	"regexp"
	"strings"
)

// Filter ...
type Filter struct {
	String string
	Params map[string]string
}

// NewFilter ...
func NewFilter(str string) *Filter {

	f := &Filter{String: "", Params: make(map[string]string)}

	re := regexp.MustCompile(`[a-z]+:[a-z0-9_]+`)

	indexes := re.FindAllIndex([]byte(str), -1)

	remaining := ""

	cursor := 0

	for _, index := range indexes {
		remaining += str[cursor:index[0]]
		arr := strings.Split(str[index[0]:index[1]], ":")
		f.Params[arr[0]] = arr[1]
		cursor = index[1]
	}

	remaining += str[cursor:]

	remaining = strings.TrimSpace(remaining)

	f.String = remaining

	return f
}
