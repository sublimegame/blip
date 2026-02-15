package diff

type HunkParser struct {
	lines []string
}

func NewHunkParser() HunkParser {
	return HunkParser{
		lines: []string{},
	}
}

func (p *HunkParser) IsEmpty() bool {
	return len(p.lines) == 0
}

func (p *HunkParser) Reset() {
	p.lines = []string{}
}

func (p *HunkParser) Write(line string) {
	p.lines = append(p.lines, line)
}

// ParseAndReturnHunk parses the lines that have been written to the HunkParser and returns a Hunk
func (p *HunkParser) ParseHunk(parentDiff *Diff) (Hunk, error) {
	return NewHunkFromLines(p.lines, parentDiff)
}
