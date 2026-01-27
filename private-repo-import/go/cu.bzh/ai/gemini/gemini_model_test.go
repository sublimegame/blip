package gemini

import (
	"testing"

	"cu.bzh/ai"
)

// TestGeminiModelImplementsModelInterface ensures that GeminiModel implements the ai.Model interface
func TestGeminiModelImplementsModelInterface(t *testing.T) {
	// This is a compile-time check to ensure GeminiModel implements ai.Model
	var _ ai.Model = (*GeminiModel)(nil)
}
