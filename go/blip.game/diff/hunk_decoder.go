package diff

import (
	"bufio"
	"io"
	"strings"
)

// ----------------------------------------------
// Hunk decoder
// ----------------------------------------------

type HunkDecoder struct {
	reader *bufio.Reader
}

func NewHunkDecoder(input io.Reader) *HunkDecoder {
	return &HunkDecoder{
		reader: bufio.NewReader(input),
	}
}

func (d *HunkDecoder) DecodeHunk() (*Hunk, error) {
	header, err := d.readHunkHeader()
	if err != nil {
		return nil, err
	}

	lines, err := d.readHunkLines()
	if err != nil {
		return nil, err
	}

	hunkBody, err := newHunkBody(lines)
	if err != nil {
		return nil, err
	}

	newHunk := &Hunk{
		Header: header,
		Body:   hunkBody,
	}

	return newHunk, nil
}

func (d *HunkDecoder) readHunkHeader() (HunkHeader, error) {
	for {
		line, err := d.reader.ReadString('\n')
		if err != nil {
			return HunkHeader{}, err // err can be io.EOF
		}
		var header HunkHeader
		header, err = newHunkHeader(strings.TrimSpace(line))
		if err != nil {
			continue
		}
		return header, nil
	}
}

func (d *HunkDecoder) readHunkLines() ([]string, error) {
	var lines []string
	for {
		// we peek before reading a line to avoid reading the first line of the next hunk
		peek, err := d.reader.Peek(1)
		if err != nil && err != io.EOF {
			return nil, err
		}
		if string(peek) == "@" {
			break
		}

		// read the next line
		line, err := d.reader.ReadString('\n')
		if err != nil && err != io.EOF {
			return nil, err
		}

		line = strings.TrimSuffix(line, "\n")
		lines = append(lines, line)

		if err == io.EOF {
			break
		}
	}
	return lines, nil
}
