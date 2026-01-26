package main

import (
	"bufio"
	"strings"
)

func parseCommandArguments(content string) []string {
	var args []string
	var currentArg strings.Builder
	inQuotes := false

	scanner := bufio.NewScanner(strings.NewReader(content))
	scanner.Split(bufio.ScanRunes)

	for scanner.Scan() {
		char := scanner.Text()

		switch char {
		case "\"":
			if inQuotes {
				args = append(args, currentArg.String())
				currentArg.Reset()
				inQuotes = false
			} else {
				inQuotes = true
			}
		case " ":
			if inQuotes {
				currentArg.WriteString(char)
			} else if currentArg.Len() > 0 {
				args = append(args, currentArg.String())
				currentArg.Reset()
			}
		default:
			currentArg.WriteString(char)
		}
	}

	if currentArg.Len() > 0 {
		args = append(args, currentArg.String())
	}

	if len(args) > 0 {
		return args[1:]
	}

	return []string{}
}
