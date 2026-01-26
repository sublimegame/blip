package diff

import (
	"errors"
	"io"
	"strings"
)

// ----------------------------------------------
// Diff
// ----------------------------------------------

type DiffHeader struct {
	OldFile string
	NewFile string
}
type Diff struct {
	Header     DiffHeader // Header lines
	Hunks      []*Hunk    // Hunk (header + lines)
	SourceCode *string
}

type DiffValidityInfo struct {
	HeaderIsValid bool
	HunksAreValid bool
}

func NewDiff(diffText string, sourceCode *string) (Diff, error) {
	diff := Diff{
		Header:     DiffHeader{},
		Hunks:      []*Hunk{},
		SourceCode: sourceCode,
	}

	// Find the first hunk header (line starting with "@@")
	headerLine := strings.Index(diffText, "@@")
	if headerLine == -1 {
		return diff, errors.New("no hunk header found")
	}

	// Extract the header lines
	parsedHeader := ParseLines(diffText[:headerLine])
	if len(parsedHeader) == 2 &&
		strings.HasPrefix(parsedHeader[0], "---") &&
		strings.HasPrefix(parsedHeader[1], "+++") {
		// header is valid
		diff.Header.OldFile = parsedHeader[0]
		diff.Header.NewFile = parsedHeader[1]
	}

	// Create a new hunk decoder
	hunkDecoder := NewHunkDecoder(strings.NewReader(diffText))
	if hunkDecoder == nil {
		return diff, errors.New("failed to create hunk decoder")
	}

	for {
		decodedHunk, err := hunkDecoder.DecodeHunk()
		if err == io.EOF {
			break
		}
		if err != nil {
			return diff, err
		}
		// a hunk has been decoded successfully
		err = diff.AppendAndFixHunk(decodedHunk)
		if err != nil {
			return diff, err
		}
	}

	return diff, nil
}

func (d *Diff) ToString() string {
	result := ""

	// header
	if d.Header.OldFile != "" && d.Header.NewFile != "" {
		result += d.Header.OldFile
		result += "\n"
		result += d.Header.NewFile
	}

	// hunks
	for _, hunk := range d.Hunks {
		if result != "" {
			result += "\n"
		}
		result += hunk.ToString()
	}

	return result
}

// IsValid returns true if the diff is valid, false otherwise
// It also returns a DiffInvalidInfo struct with the reason why the diff is invalid
// func (d *Diff) isValid() DiffValidityInfo {
// 	result := DiffValidityInfo{
// 		HeaderIsValid: true,
// 		HunksAreValid: true,
// 	}

// 	// Check diff header
// 	if len(d.Header) != 2 ||
// 		!strings.HasPrefix(d.Header[0], "---") ||
// 		!strings.HasPrefix(d.Header[1], "+++") {
// 		result.HeaderIsValid = false
// 	}

// 	// Check hunks
// 	for _, hunk := range d.Hunks {
// 		if !hunk.IsValid() {
// 			result.HunksAreValid = false
// 		}
// 	}

// 	return result
// }

func (d *Diff) HunksAreValid() bool {
	for _, hunk := range d.Hunks {
		if !hunk.IsValid() {
			return false
		}
	}
	return true
}

func (d *Diff) FixHunks() error {
	for _, h := range d.Hunks {
		err := h.Fix()
		if err != nil {
			return err
		}
	}
	return nil // no error
}

// IsApplicableToCode returns true if the diff can be applied to the code, false otherwise
func (d *Diff) IsApplicableToCode(sourceCodeLines []string) error {
	// validate each hunk
	for _, hunk := range d.Hunks {
		err := hunk.IsApplicableToCode(sourceCodeLines)
		if err != nil {
			return err
		}
	}

	return nil // no error
}

func (d *Diff) AppendAndFixHunk(hunk *Hunk) error {
	hunk.ParentDiff = d
	hunk.Index = len(d.Hunks)
	d.Hunks = append(d.Hunks, hunk)
	return hunk.Fix()
}
