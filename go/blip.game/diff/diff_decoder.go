package diff

import (
	"bytes"
	"fmt"
	"io"
	"strings"
)

// ----------------------------------------------
// DiffDecoder
// ----------------------------------------------

const (
	READ_BUFFER_SIZE = 1
)

type DiffDecoder struct {
	reader            io.Reader
	readBuffer        []byte
	state             DiffDecoderState
	buffer            bytes.Buffer
	bufferUnprocessed bytes.Buffer
	currentLine       string // current line being decoded (we accumulate bytes until we have a full line)
	hunkParser        HunkParser
	decodedDiff       Diff
}

type DiffDecoderState int

const (
	stateStart DiffDecoderState = iota
	stateHeader
	stateHunks
)

func NewDiffDecoder(reader io.Reader, sourceCode *string) *DiffDecoder {
	return &DiffDecoder{
		reader:            reader,
		readBuffer:        make([]byte, READ_BUFFER_SIZE),
		state:             stateStart,
		buffer:            bytes.Buffer{},
		bufferUnprocessed: bytes.Buffer{},
		currentLine:       "",
		hunkParser:        NewHunkParser(),
		decodedDiff: Diff{
			Header:     DiffHeader{},
			Hunks:      []*Hunk{},
			SourceCode: sourceCode,
		},
	}
}

// Returns true if a full line is available in d.currentLine, false otherwise.
func (d *DiffDecoder) readLineFromReader() (bool, error) {

	// A full line is already available.
	// We don't read anything from the reader.
	if strings.HasSuffix(d.currentLine, "\n") {
		return true, nil
	}

	// We don't have a full line, we read from the reader

	// Read next bytes from reader
	readBytesCount, readErr := d.reader.Read(d.readBuffer)
	if readErr != nil && readErr != io.EOF {
		fmt.Printf("[READ_LINE] ❌ reader read error [%v]\n", readErr)
		// there is an error (other than EOF), cannot continue
		return false, readErr
	}

	// Append bytes to our bytes buffer
	if readBytesCount > 0 {
		// Write the read bytes to our bytes buffer
		_, err := d.buffer.Write(d.readBuffer[:readBytesCount])
		if err != nil {
			return false, err
		}
		// also write to the unprocessed buffer
		_, err = d.bufferUnprocessed.Write(d.readBuffer[:readBytesCount])
		if err != nil {
			return false, err
		}
	} else if readErr == io.EOF { // 0 bytes read and EOF reached
		// EOF reached. It means it was the last line.
		return true, io.EOF // transmit EOF to the caller
	}

	// Try to read until the end of a line in the buffer
	chars, err := d.buffer.ReadString('\n')
	if err != nil && err != io.EOF {
		fmt.Printf("[READ_LINE] ❌ buffer read error [%v]\n", err)
		return false, err
	}

	// Append the chars we read to currentLine
	d.currentLine += chars

	if err == io.EOF {
		// EOF reached. It means we did not read a full line.
		return false, nil
	}

	// err is nil, so we have a full line (ending with '\n' character)
	return true, nil
}

// Return values:
// more (bool) : indicates if there is more data to decode
// error (error) : error if decoding failed
func (d *DiffDecoder) Decode() (bool, error) {

	lineIsLastLine := false
	lineIsComplete, err := d.readLineFromReader()
	if err != nil && err != io.EOF {
		return false, err
	}
	if err == io.EOF {
		lineIsLastLine = true
	}

	if lineIsComplete == false {
		return true, nil
	}

	// currentLine is complete
	lineToProcess := d.currentLine
	d.currentLine = ""

	// Remove the '\n' character from the end of the line
	// Last line of the file may not have a '\n' character
	lineToProcess = strings.TrimSuffix(lineToProcess, "\n")

	// fmt.Printf("[DECODE][%d] LINE [%s]\n", d.state, lineToProcess)

	switch d.state {
	case stateStart:

		// Trim the line to remove any leading or trailing whitespace
		// (this may not be needed)
		lineToProcess = strings.TrimSpace(lineToProcess)

		// Here we are looking for the first line of the first hunk.
		// It starts with "@@ "
		if strings.HasPrefix(lineToProcess, "@@ ") {
			// go to the next state
			// (we skip the diff header parsing for now)
			d.decodedDiff.Header = DiffHeader{}
			d.state = stateHunks
			// put the line back, so it can be processed by the next state on the next call to Decode()
			d.currentLine = lineToProcess + "\n"
		} else {
			// unrecognized line, ignoring it
			// fmt.Printf("[DECODE] ⚠️ ignoring line: [%s]\n", lineToProcess)
			d.bufferUnprocessed.Reset()
		}

	case stateHeader:

		// We don't parse the diff header for now.
		// We will add this later.

	case stateHunks:

		// If the line to process looks like a hunk header, we have the hunk parser parse the previous hunk
		// then we reset it and write the line to it
		if strings.HasPrefix(lineToProcess, "@@ ") {
			found, err := d.parseFixAndAppendHunk() // does nothing if the hunk parser is empty
			if err != nil {
				return false, err
			}
			if found {
				d.bufferUnprocessed.Reset()
			}
			// reset the hunk parser
			d.hunkParser.Reset()
		}

		// Write the line to the hunk parser
		// (this will be parsed later)
		d.hunkParser.Write(lineToProcess)

		if lineIsLastLine {
			found, err := d.parseFixAndAppendHunk()
			if err != nil {
				return false, err
			}
			if found {
				d.bufferUnprocessed.Reset()
			}
			// reset the hunk parser
			d.hunkParser.Reset()
		}
	}

	thereIsMore := !lineIsLastLine
	return thereIsMore, nil
}

// Returns true if a hunk was parsed and appended to the decoded diff, false otherwise.
func (d *DiffDecoder) parseFixAndAppendHunk() (bool, error) {
	if d.hunkParser.IsEmpty() {
		return false, nil // do nothing
	}

	// parse hunk
	hunk, err := d.hunkParser.ParseHunk(&d.decodedDiff)
	if err != nil {
		return false, err
	}

	// add the hunk to the decoded diff
	err = d.decodedDiff.AppendAndFixHunk(&hunk)
	if err != nil {
		return false, err
	}

	return true, nil
}

func (d *DiffDecoder) DecodedDiff() *Diff {
	return &d.decodedDiff
}

func (d *DiffDecoder) UnprocessedLines() string {
	// return d.currentLine
	return d.bufferUnprocessed.String()
}
