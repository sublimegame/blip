package diff

import (
	"errors"
	"fmt"
	"strconv"
	"strings"
)

// ----------------------------------------------------------------------------
// Hunk
// ----------------------------------------------------------------------------

type Hunk struct {
	Header          HunkHeader
	Body            HunkBody
	Index           int // index of the hunk in the parent diff
	CumulativeDelta int // cumulated lines removed and added considering all previous hunks
	ParentDiff      *Diff
}

type HunkApplicabilityError error

var (
	HunkApplicabilityErrorLineCountMismatch     = errors.New("line count mismatch")
	HunkApplicabilityErrorLineNumberOutOfBounds = errors.New("line number out of bounds")
	HunkApplicabilityErrorLineAndCodeMismatch   = errors.New("line and code mismatch")
)

// NewHunk parses a single hunk (header + lines) from a unified diff
func NewHunk(hunkText string, parentDiff *Diff) (Hunk, error) {
	if len(hunkText) == 0 {
		return Hunk{}, errors.New("hunk text is empty")
	}

	// split into lines
	lines := ParseLines(hunkText)

	return NewHunkFromLines(lines, parentDiff)
}

func NewHunkFromLines(lines []string, parentDiff *Diff) (Hunk, error) {
	if len(lines) == 0 {
		return Hunk{}, errors.New("no lines provided")
	}

	// Make sure the first line is a hunk header
	if !strings.HasPrefix(lines[0], "@@") {
		return Hunk{}, fmt.Errorf("invalid hunk format: first line must be a hunk header that starts with '@@'")
	}

	// Make sure there is only one hunk header in `hunkText`
	{
		// count lines starting with @@
		lineCounter := 0
		for _, line := range lines {
			if strings.HasPrefix(line, "@@") {
				lineCounter++
			}
		}
		if lineCounter != 1 {
			return Hunk{}, fmt.Errorf("invalid hunk format: expected exactly one hunk header")
		}
	}

	// parse header
	header, err := newHunkHeader(lines[0])
	if err != nil {
		return Hunk{}, fmt.Errorf("error parsing hunk header: %v", err)
	}

	// parse body
	body, err := newHunkBody(lines[1:])
	if err != nil {
		return Hunk{}, fmt.Errorf("error parsing hunk body: %v", err)
	}

	return Hunk{
		Header:     header,
		Body:       body,
		ParentDiff: parentDiff,
	}, nil
}

func (h *Hunk) ToString() string {
	return h.Header.ToString() + "\n" + h.Body.ToString()
}

func (h *Hunk) ComputeOriginalRangeCount() uint {
	unchanged := h.Body.UnchangedLinesCount()
	removed := h.Body.RemovedLinesCount()
	return uint(unchanged) + uint(removed)
}

func (h *Hunk) ComputeNewRangeCount() uint {
	unchanged := h.Body.UnchangedLinesCount()
	added := h.Body.AddedLinesCount()
	return uint(unchanged) + uint(added)
}

func (h *Hunk) IsValid() bool {
	// check that the hunk header data matches the hunk body lines
	if h.Header.OriginalRange.Count != h.ComputeOriginalRangeCount() {
		return false // original range count does not match the lines
	}
	if h.Header.NewRange.Count != h.ComputeNewRangeCount() {
		return false // new range count does not match the lines
	}
	return true
}

func (h *Hunk) Fix() error {
	if h.ParentDiff == nil {
		return errors.New("no parent diff")
	}

	if h.ParentDiff.SourceCode == nil {
		return errors.New("no source code in parent diff")
	}

	sourceCodeLines := ParseLines(*h.ParentDiff.SourceCode)

	// Fix cumulative delta
	cumulativeDeltaAfterPreviousHunk := 0
	if h.Index > 0 {
		// get cumulative delta from previous hunk
		previousHunk := h.ParentDiff.Hunks[h.Index-1]
		cumulativeDeltaAfterPreviousHunk = previousHunk.CumulativeDelta
	}

	// get the original code version from the hunk
	hunkOriginalCodeLines := h.Body.GetOriginalVersionLines()

	// check if the original code is part of the text
	lineIndex := FindLines(hunkOriginalCodeLines, sourceCodeLines)
	if lineIndex == -1 {
		return errors.New("hunk original code is not part of the source code")
	}

	// line index -> line number
	// index starts at 0, line number starts at 1
	lineNumber := lineIndex + 1

	// update the hunk header
	h.Header.OriginalRange.Start = uint(lineNumber)
	h.Header.OriginalRange.Count = uint(len(hunkOriginalCodeLines))

	h.Header.NewRange.Start = uint(lineNumber) + uint(cumulativeDeltaAfterPreviousHunk)
	h.Header.NewRange.Count = uint(h.Body.AddedLinesCount()) + uint(h.Body.UnchangedLinesCount())

	h.CumulativeDelta = cumulativeDeltaAfterPreviousHunk + h.Body.AddedLinesCount() - h.Body.RemovedLinesCount()

	// Make sure the hunk is applicable to the source code
	return h.IsApplicableToCode(sourceCodeLines)
}

// IsApplicableToCode checks if the hunk is applicable to the code and returns the first error it finds.
func (h *Hunk) IsApplicableToCode(codeTextLines []string) HunkApplicabilityError {

	// check that the hunk is valid (without source code context)
	if !h.IsValid() {
		return HunkApplicabilityErrorLineCountMismatch
	}

	// check that the hunk header doesn't reference lines that are out of bounds
	if h.Header.OriginalRange.Start+h.Header.OriginalRange.Count-1 > uint(len(codeTextLines)) {
		return HunkApplicabilityErrorLineNumberOutOfBounds
	}

	// check that the hunk header line numbers match the code lines
	for bodyLineIndex, bodyLine := range h.Body.GetOriginalVersionLines() {

		lineNumber := h.Header.OriginalRange.Start + uint(bodyLineIndex)
		lineIndex := lineNumber - 1

		if lineIndex >= uint(len(codeTextLines)) {
			return HunkApplicabilityErrorLineNumberOutOfBounds
		}

		if codeTextLines[lineIndex] != bodyLine {
			return HunkApplicabilityErrorLineAndCodeMismatch
		}
	}

	return nil // no error
}

// ----------------------------------------------------------------------------
// HunkHeader
// ----------------------------------------------------------------------------

type HunkHeader struct {
	OriginalRange HunkRange
	NewRange      HunkRange
}

type HunkRange struct {
	Start uint
	Count uint
}

// NewHunkHeader parses a hunk header line string and returns a HunkHeader struct
func newHunkHeader(headerLine string) (HunkHeader, error) {
	// ensure headerLine is not empty
	if len(headerLine) == 0 {
		return HunkHeader{}, errors.New("invalid hunk header line: it is empty")
	}

	// ensure headerLine doesn't contain any '\n'
	if strings.Contains(headerLine, "\n") {
		return HunkHeader{}, fmt.Errorf("invalid hunk header line: must not contain '\n'")
	}

	// ensure headerLine contains exactly two '@@'
	if strings.Count(headerLine, "@@") != 2 {
		return HunkHeader{}, fmt.Errorf("invalid hunk header line: must contain exactly two '@@'")
	}

	// Find the first occurrence of "@@" and remove everything before it
	if idx := strings.Index(headerLine, "@@"); idx > 0 {
		headerLine = headerLine[idx:]
	}

	// Find the last occurrence of "@@" and remove everything after it
	if idx := strings.LastIndex(headerLine, "@@"); idx != -1 {
		headerLine = headerLine[:idx+2]
	}

	// split on space
	parts := strings.Split(headerLine, " ")

	if len(parts) != 4 {
		return HunkHeader{}, fmt.Errorf("invalid hunk header line: it doesn't have exactly 4 parts")
	}
	if parts[0] != "@@" {
		return HunkHeader{}, fmt.Errorf("invalid hunk header line: it doesn't start with '@@'")
	}
	if parts[3] != "@@" {
		return HunkHeader{}, fmt.Errorf("invalid hunk header line: it doesn't end with '@@'")
	}

	// 2nd part is original range
	origRange, err := parseRange(parts[1])
	if err != nil {
		return HunkHeader{}, fmt.Errorf("error parsing original range: %v", err)
	}

	// 3rd part is new range
	newRange, err := parseRange(parts[2])
	if err != nil {
		return HunkHeader{}, fmt.Errorf("error parsing new range: %v", err)
	}

	return HunkHeader{
		OriginalRange: origRange,
		NewRange:      newRange,
	}, nil
}

func (h *HunkHeader) ToString() string {
	return fmt.Sprintf("@@ -%d,%d +%d,%d @@",
		h.OriginalRange.Start,
		h.OriginalRange.Count,
		h.NewRange.Start,
		h.NewRange.Count,
	)
}

// ----------------------------------------------------------------------------
// HunkBody
// ----------------------------------------------------------------------------

// HunkBody is the body of a hunk (lines after the hunk header)
type HunkBody struct {
	RawLines    []string
	ParsedLines []HunkLine
}

func newHunkBody(rawLines []string) (HunkBody, error) {
	processedRawLines := []string{}
	parsedLines := []HunkLine{}
	for _, rawLine := range rawLines {
		parsedLine, err := NewHunkLine(rawLine)
		if err != nil {
			continue
		}
		processedRawLines = append(processedRawLines, rawLine)
		parsedLines = append(parsedLines, parsedLine)
	}

	// Post-processing parsed lines to remove empty trailing lines
	for i := len(parsedLines) - 1; i >= 0; i-- {
		if parsedLines[i].LineType == HunkLineTypeUnchanged && parsedLines[i].Line == "" {
			processedRawLines = processedRawLines[:i]
			parsedLines = parsedLines[:i]
		} else {
			break
		}
	}

	return HunkBody{
		RawLines:    processedRawLines,
		ParsedLines: parsedLines,
	}, nil
}

func (h *HunkBody) ToString() string {
	return strings.Join(h.RawLines, "\n")
}

// number of lines removed (lines with '-' prefix)
func (h *HunkBody) RemovedLinesCount() int {
	count := 0
	for _, line := range h.ParsedLines {
		if line.LineType == HunkLineTypeRemoved {
			count++
		}
	}
	return count
}

// number of lines added (lines with '+' prefix)
func (h *HunkBody) AddedLinesCount() int {
	count := 0
	for _, line := range h.ParsedLines {
		if line.LineType == HunkLineTypeAdded {
			count++
		}
	}
	return count
}

// number of lines unchanged (lines without -/+ prefix)
func (h *HunkBody) UnchangedLinesCount() int {
	count := 0
	for _, line := range h.ParsedLines {
		if line.LineType == HunkLineTypeUnchanged {
			count++
		}
	}
	return count
}

func (h *HunkBody) GetOriginalVersion() string {
	lines := []string{}
	for _, line := range h.ParsedLines {
		if line.LineType == HunkLineTypeRemoved || line.LineType == HunkLineTypeUnchanged {
			lines = append(lines, line.Line)
		}
	}
	return strings.Join(lines, "\n")
}

func (h *HunkBody) GetNewVersion() string {
	lines := []string{}
	for _, line := range h.ParsedLines {
		if line.LineType == HunkLineTypeAdded || line.LineType == HunkLineTypeUnchanged {
			lines = append(lines, line.Line)
		}
	}
	return strings.Join(lines, "\n")
}

func (h *HunkBody) GetOriginalVersionLines() []string {
	originalVersionLines := []string{}
	for _, line := range h.ParsedLines {
		if line.LineType == HunkLineTypeRemoved || line.LineType == HunkLineTypeUnchanged {
			originalVersionLines = append(originalVersionLines, line.Line)
		}
	}
	return originalVersionLines
}

func (h *HunkBody) GetNewVersionLines() []string {
	newVersionLines := []string{}
	for _, line := range h.ParsedLines {
		if line.LineType == HunkLineTypeAdded || line.LineType == HunkLineTypeUnchanged {
			newVersionLines = append(newVersionLines, line.Line)
		}
	}
	return newVersionLines
}

// ----------------------------------------------------------------------------
// HunkLine
// ----------------------------------------------------------------------------

type HunkLine struct {
	// Type of line (original, new, unchanged) determined by the prefix
	LineType HunkLineType
	// Line content, without the prefix
	Line string
}

type HunkLineType int

const (
	HunkLineTypeRemoved   HunkLineType = iota
	HunkLineTypeAdded     HunkLineType = iota
	HunkLineTypeUnchanged HunkLineType = iota
)

func NewHunkLine(rawLineContent string) (HunkLine, error) {
	lineType := HunkLineTypeUnchanged
	if strings.HasPrefix(rawLineContent, "-") {
		lineType = HunkLineTypeRemoved
		rawLineContent = strings.TrimPrefix(rawLineContent, "-")
	} else if strings.HasPrefix(rawLineContent, "+") {
		lineType = HunkLineTypeAdded
		rawLineContent = strings.TrimPrefix(rawLineContent, "+")
	} else if strings.HasPrefix(rawLineContent, " ") {
		lineType = HunkLineTypeUnchanged
		rawLineContent = strings.TrimPrefix(rawLineContent, " ")
	} else {
		return HunkLine{}, fmt.Errorf("invalid hunk line: [%s]", rawLineContent)
	}
	return HunkLine{LineType: lineType, Line: rawLineContent}, nil
}

// ----------------------------------------------------------------------------

// func ParseAllHunks(diffText string) ([]Hunk, error) {
// 	var parsedHunks []Hunk

// 	for len(diffText) > 0 {
// 		parsedHunk, remainingText, err := ParseNextHunk(diffText)
// 		if err != nil {
// 			return nil, err
// 		}
// 		parsedHunks = append(parsedHunks, parsedHunk)
// 		diffText = remainingText
// 	}

// 	return parsedHunks, nil
// }

// func ParseNextHunk(hunkText string) (parsedHunk Hunk, remainingText string, err error) {
// 	if len(hunkText) == 0 {
// 		return Hunk{}, "", errors.New("hunk text is empty")
// 	}

// 	// split on '@@ '
// 	const separator = "@@ "
// 	parts := strings.SplitN(hunkText, separator, 3)[1:] // skip first part
// 	// reconstruct parts
// 	for i, part := range parts {
// 		parts[i] = separator + part
// 	}

// 	// If len(parts) == 2, it means there is a remaining text.
// 	// Otherwise len(parts) can only be 1.
// 	if len(parts) != 1 && len(parts) != 2 {
// 		return Hunk{}, "", errors.New("parsing error: invalid number of parts")
// 	}

// 	parsedHunk, err = NewHunk(parts[0])
// 	if err != nil {
// 		return Hunk{}, "", errors.New("error parsing hunk: " + err.Error())
// 	}

// 	remainingText = ""
// 	if len(parts) == 2 {
// 		remainingText = parts[1]
// 	}

// 	return parsedHunk, remainingText, nil
// }

func parseRange(s string) (HunkRange, error) {
	s = strings.TrimPrefix(s, "-")
	s = strings.TrimPrefix(s, "+")
	parts := strings.Split(s, ",")

	// Note: we don't support ranges that omit the line count and the comma, like @@ -1 +1 @@
	if len(parts) != 2 {
		return HunkRange{}, fmt.Errorf("invalid range format")
	}

	start, err := strconv.ParseUint(parts[0], 10, 32)
	if err != nil {
		return HunkRange{}, fmt.Errorf("invalid start value: %v", err)
	}

	count, err := strconv.ParseUint(parts[1], 10, 32)
	if err != nil {
		return HunkRange{}, fmt.Errorf("invalid count value: %v", err)
	}

	return HunkRange{
		Start: uint(start),
		Count: uint(count),
	}, nil
}

// func ApplyHunk(codeText string, hunk Hunk) string {
// 	// // Split the code text into lines
// 	// codeLines := strings.Split(codeText, "\n")

// 	// // Check if the hunk is applicable
// 	// if err := hunk.IsApplicableToCode(codeLines); err != nil {
// 	// 	// If the hunk is not applicable, return the original code
// 	// 	return codeText
// 	// }

// 	// // Get the original and new versions of the hunk
// 	// originalLines := hunk.Body.GetOriginalVersionLines()
// 	// newLines := hunk.Body.GetNewVersionLines()

// 	// // Calculate the line numbers where we need to make changes
// 	// startLine := int(hunk.Header.OriginalRange.Start - 1) // Convert to 0-based index
// 	// endLine := startLine + len(originalLines)

// 	// // Create the result by combining:
// 	// // 1. Lines before the hunk
// 	// // 2. New lines from the hunk
// 	// // 3. Lines after the hunk
// 	// result := make([]string, 0, len(codeLines)-len(originalLines)+len(newLines))
// 	// result = append(result, codeLines[:startLine]...)
// 	// result = append(result, newLines...)
// 	// result = append(result, codeLines[endLine:]...)

// 	// return strings.Join(result, "\n")

// 	fmt.Println("🔥 [ApplyHunk] NOT IMPLEMENTED YET")
// 	return codeText
// }
