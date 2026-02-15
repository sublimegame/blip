package grok

import (
	"testing"

	"cu.bzh/ai"
)

// TestGeminiModelImplementsModelInterface ensures that GeminiModel implements the ai.Model interface
func TestGrokModelImplementsModelInterface(t *testing.T) {
	// This is a compile-time check to ensure GrokModel implements ai.Model
	var _ ai.Model = (*GrokModel)(nil)
}
