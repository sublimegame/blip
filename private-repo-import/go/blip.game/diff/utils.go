package diff

import (
	"strings"
)

// ParseLines splits a string by newline characters and removes the last element if it's empty
// (it's due to the last line ending with a newline character)
func ParseLines(text string) []string {
	elems := strings.Split(text, "\n")
	if len(elems) > 0 && elems[len(elems)-1] == "" {
		elems = elems[:len(elems)-1]
	}
	return elems
}

// StringContainsLines returns true if the text contains all the lines
func StringContainsLines(text string, lines []string) bool {
	linesAsText := strings.Join(lines, "\n")
	return strings.Contains(text, linesAsText)
}

// FindLines returns the line index (starting from 0) of the first occurrence of the sequence
// of lines in linesToFind within linesToSearchIn. It performs a sequential search, looking for
// an exact match of the entire sequence. If the sequence is not found, it returns -1.
func FindLines(linesToFind []string, linesToSearchIn []string) int {
	// Edge case: empty search pattern always matches at position 0
	if len(linesToFind) == 0 {
		return 0
	}

	// Edge case: can't find anything in empty text
	if len(linesToSearchIn) == 0 {
		return -1
	}

	// Edge case: pattern longer than search text
	if len(linesToFind) > len(linesToSearchIn) {
		return -1
	}

	cursorInLines := 0
	for i, textLine := range linesToSearchIn {
		if textLine == linesToFind[cursorInLines] {
			cursorInLines++
			if cursorInLines == len(linesToFind) {
				// Found the complete sequence
				// The current index i points to the last line of the match
				// We need to return the index of the first line of the match
				return i - (len(linesToFind) - 1)
			}
		} else {
			// If we don't match, we need to reset our cursor
			// But first check if the current line matches the first line of our pattern
			// This handles overlapping potential matches
			if textLine == linesToFind[0] {
				cursorInLines = 1
			} else {
				cursorInLines = 0
			}
		}
	}
	return -1
}

// find line number for a given char index in a string
func FindLineNumberForCharIndex(text string, charIndex int) int {
	lines := ParseLines(text)
	charCounter := 0
	for i, line := range lines {
		charCounter += len(line)
		if charIndex < charCounter {
			return i + 1
		}
	}
	return len(lines)
}
